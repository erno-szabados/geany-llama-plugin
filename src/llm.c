#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <json-glib/json-glib.h> // dependency: libjson-glib-dev
#include <curl/curl.h>

#include "plugin.h"
#include "llm.h"

/// @brief strdup helper
static gchar* safe_strdup(const gchar *str) {
    return str ? g_strdup(str) : NULL;
}

/// @brief Callback function for writing response data.
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    GString *response = (GString *)userp;
    g_string_append_len(response, contents, total_size);

    return total_size;
}

/// @brief Construct server URI from base uri and path
static gchar* construct_server_uri_string(const gchar* server_base_uri, const gchar *path)
{
    if (!server_base_uri || !path) {
        return NULL;
    }
    
    return g_strjoin(NULL, server_base_uri, path, NULL);
}

/// @brief Construct the JSON request payload using json-glib for the completion endpoint
static gchar* construct_completion_json_payload(const gchar* query, const gchar *current_document, const LLMArgs* args) {
    JsonBuilder* builder = json_builder_new();

    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "model");
    json_builder_add_string_value(builder, args->model);

    GString *full_prompt = g_string_new("Analyze the following document:\n\n");
    g_string_append(full_prompt, current_document);
    g_string_append(full_prompt, "\n\nBased on the document, give a concise answer the following question:\n");
    g_string_append(full_prompt, query);

    json_builder_set_member_name(builder, "prompt");
    json_builder_add_string_value(builder, full_prompt->str);

    json_builder_set_member_name(builder, "max_tokens");
    json_builder_add_int_value(builder, args->max_tokens);

    json_builder_set_member_name(builder, "temperature");
    json_builder_add_double_value(builder, args->temperature);

    json_builder_end_object(builder);

    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);

    gchar* json_payload = json_generator_to_data(generator, NULL);

    g_object_unref(generator);
    json_node_free(root);
    g_object_unref(builder);
    g_string_free(full_prompt, TRUE);

    return json_payload;
}

/// @brief Construct the JSON request payload using json-glib for the chat completion endpoint
static gchar* construct_chat_completion_json_payload(const gchar* query, const LLMArgs* args) {
    JsonBuilder* builder = json_builder_new();

    // Begin the root object
    json_builder_begin_object(builder);

    // Add "model" field
    json_builder_set_member_name(builder, "model");
    json_builder_add_string_value(builder, args->model);

    // Add "messages" array
    json_builder_set_member_name(builder, "messages");
    json_builder_begin_array(builder);
    
    // Add "system" message
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "role");
    json_builder_add_string_value(builder, "system");
    json_builder_set_member_name(builder, "content");
    json_builder_add_string_value(builder, args->system_instruction); // A predefined instruction in LLMArgs
    json_builder_end_object(builder);

    // Add "user" message (the query)
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "role");
    json_builder_add_string_value(builder, "user");
    json_builder_set_member_name(builder, "content");
    json_builder_add_string_value(builder, query);
    json_builder_end_object(builder);

    // Add more messages from args->messages (if any)
    for (guint i = 0; i < args->messages_length; ++i) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "role");
        json_builder_add_string_value(builder, args->messages[i].role);
        json_builder_set_member_name(builder, "content");
        json_builder_add_string_value(builder, args->messages[i].content);
        json_builder_end_object(builder);
    }

    // End the "messages" array
    json_builder_end_array(builder);

    // Add "max_tokens" field
    json_builder_set_member_name(builder, "max_tokens");
    json_builder_add_int_value(builder, args->max_tokens);

    // Add "temperature" field
    json_builder_set_member_name(builder, "temperature");
    json_builder_add_double_value(builder, args->temperature);

    // End the root object
    json_builder_end_object(builder);

    // Generate the JSON data
    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);

    gchar* json_payload = json_generator_to_data(generator, NULL);

    // Clean up
    g_object_unref(generator);
    json_node_free(root);
    g_object_unref(builder);

    return json_payload;
}


/// @brief populate LLMResponse from raw JSON data
static gboolean json_to_response(LLMResponse *response, const gchar *raw_json, GError **error) {
    memset(response, 0, sizeof(LLMResponse));

    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, raw_json, -1, error)) {
        g_object_unref(parser);
        return FALSE;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!JSON_NODE_HOLDS_OBJECT(root)) {
        g_set_error(error, g_quark_from_static_string("JSON Error"), 1, "Root is not a JSON object");
        g_object_unref(parser);
        return FALSE;
    }

    JsonObject *root_object = json_node_get_object(root);
    JsonArray *choices = json_object_get_array_member(root_object, "choices");

    if (choices && json_array_get_length(choices) > 0) {
        response->response_text = safe_strdup(
            json_object_get_string_member(json_array_get_object_element(choices, 0), "text")
        );
    }

    if (json_object_has_member(root_object, "error")) {
        response->error = safe_strdup(json_object_get_string_member(root_object, "error"));
    }

    response->raw_json = g_strdup(raw_json);

    g_object_unref(parser);
    
    return TRUE;
}

/// @brief Execute LLM query using curl to connect to the LLM server
static gboolean execute_llm_query(const gchar *server_uri, const gchar *json_payload, LLMResponse *response) {
    GError *error = NULL;
    if (!response || !server_uri || !json_payload) {
        return FALSE;
    }
    
    CURL *curl = curl_easy_init();
    
    if (!curl) {
        response->error = g_strdup("Failed to initialize curl.");
        
        return FALSE;
    }
    
    GString *response_data = g_string_new(NULL);

    // --- Set cURL options ---
    curl_easy_setopt(curl, CURLOPT_URL, server_uri);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_data);

    // Set HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // --- Perform the request ---
    CURLcode res = curl_easy_perform(curl);

    // --- Check for errors ---
    if (res != CURLE_OK) {
        g_warning("cUrl error: %s", curl_easy_strerror(res));
        g_string_free(response_data, TRUE);
        curl_easy_cleanup(curl);
        
        return FALSE;
    } else {
        if (json_to_response(response, response_data->str, &error)) {
            g_print("Response Text: %s\n", response->response_text);
            g_print("Error: %s\n", response->error ? response->error : "No error");
            g_print("Raw JSON: %s\n", response->raw_json);
        } else {
            g_warning("Error parsing JSON: %s", error->message);
            g_clear_error(&error);
        }
    }
    g_string_free(response_data, TRUE);
    curl_easy_cleanup(curl);
    
    return TRUE;
}

LLMResponse *llm_query_completions(LLMPlugin *plugin, const gchar *query, const gchar *current_document, const LLMArgs *args){

    GUri *uri = NULL;
    gchar *json_payload = NULL;
    LLMResponse *response = g_new(LLMResponse, 1);

    const gchar *path = "/v1/completions";
    gchar *server_uri = construct_server_uri_string(plugin->llm_server_url, path);
    if (!server_uri) {
        response->error = g_strdup("Failed to convert URI to string\n");
        
        return response;
    }

    json_payload = construct_completion_json_payload(query, current_document, args);
    if (!json_payload) {
        g_free(server_uri);
        response->error = g_strdup("failed to construct json payload.");

        return response;
    }

    execute_llm_query(server_uri, json_payload, response);

    g_free(json_payload);
    g_free(server_uri);

    return response;
}

LLMResponse *llm_query_chat_completions(LLMPlugin *plugin, const gchar *query, const LLMArgs *args) {

    GUri *uri = NULL;
    gchar *json_payload = NULL;
    LLMResponse *response = g_new(LLMResponse, 1);

    const gchar *path = "/v1/chat/completions";
    gchar *server_uri = construct_server_uri_string(plugin->llm_server_url, path);
    if (!server_uri) {
        response->error = g_strdup("Failed to convert URI to string\n");
        
        return response;
    }

    json_payload = construct_chat_completion_json_payload(query, args);
    if (!json_payload) {
        g_free(server_uri);
        response->error = g_strdup("failed to construct json payload.");

        return response;
    }

    execute_llm_query(server_uri, json_payload, response);

    g_free(json_payload);
    g_free(server_uri);

    return response;
}

void llm_free_response(LLMResponse *response) {
    g_free(response->response_text);
    g_free(response->error);
    g_free(response->raw_json);
}

gpointer llm_thread_func(gpointer data) {
    ThreadData *thread_data = (ThreadData *)data;

    LLMPlugin *llm_plugin = thread_data->llm_plugin;
    gchar *query = thread_data->query;
    gchar *current_document = thread_data->current_document;

    // Call the blocking function in the worker thread
    LLMResponse *response = llm_query_completions(llm_plugin, query, current_document, llm_plugin->llm_args);

    // Free the query as it's no longer needed
    g_free(query);
    g_free(thread_data->current_document);
    
    // Pass the response back to the main thread
    gdk_threads_add_idle((GSourceFunc)llm_update_ui, response);

    // Free the thread data struct
    g_free(thread_data);

    return NULL;
}

gboolean llm_update_ui(LLMResponse *response) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(llm_plugin->output_text_view));

    if (response->error) {
        gtk_text_buffer_set_text(buffer, response->error, -1);
    } else {
        gtk_text_buffer_set_text(buffer, response->response_text, -1);
    }

    gtk_spinner_stop(GTK_SPINNER(llm_plugin->spinner));
    gtk_widget_hide(llm_plugin->spinner);

    llm_free_response(response);
    
    return FALSE;
}




