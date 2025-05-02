#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <json-glib/json-glib.h> // dependency: libjson-glib-dev
#include <curl/curl.h>

#include "plugin.h"
#include "llm.h"

gchar *safe_strdup(const gchar *str)
{
    return str ? g_strdup(str) : NULL;
}

/// @brief Construct server URI from base uri and path
gchar* llm_construct_server_uri_string(const gchar* server_base_uri, const gchar *path)
{
    if (!server_base_uri || !path) {
        return NULL;
    }
    
    return g_strjoin(NULL, server_base_uri, path, NULL);
}

/// @brief Construct the JSON request payload using json-glib for the completion endpoint
gchar* llm_construct_completion_json_payload(const gchar* query, const gchar *current_document, const LLMArgs* args) {
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
    
    // Receiving streaming tokens
    json_builder_set_member_name(builder, "stream");
    json_builder_add_boolean_value(builder, TRUE); 

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
gchar* llm_construct_chat_completion_json_payload(const gchar* query, const LLMArgs* args) {
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
    
    json_builder_set_member_name(builder, "stream");
    json_builder_add_boolean_value(builder, TRUE); 

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


// Modified append_to_output_buffer
gboolean llm_append_to_output_buffer(gpointer user_data)
{
    // The user_data is the string duplicated in write_callback
    gchar *text_to_append = (gchar *)user_data;

    if (!text_to_append || !llm_plugin || !llm_plugin->output_text_view)
    {
        g_warning("Invalid text or plugin state.");
        g_free(text_to_append);
        return FALSE;          
    }

    // Directly insert the text chunk received.
    // It should already be valid UTF-8 from json_to_response.
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(llm_plugin->output_text_view));
    if (buffer)
    {
        GtkTextIter end_iter;
        gtk_text_buffer_get_end_iter(buffer, &end_iter);
        // Insert the text chunk
        gtk_text_buffer_insert(buffer, &end_iter, text_to_append, -1);
    }
    else
    {
        g_warning("Failed to get text buffer.");
    }

    // Free the duplicated string passed via user_data
    g_free(text_to_append);

    return FALSE; // Remove the idle source
}

/// @brief populate LLMResponse from raw JSON data
gboolean llm_json_to_response(LLMResponse *response, GString *response_buffer, GError **error)
{
    if (!response)
    {
        g_warning("NULL response received!\n");
        return FALSE;
    }
    memset(response, 0, sizeof(LLMResponse));

    JsonParser *parser = json_parser_new();

    if (!response_buffer || !response_buffer->str || response_buffer->str[0] == '\0')
    {
        g_warning("Invalid or empty response buffer received!\n");
        g_object_unref(parser);
        return FALSE;
    }

    // Let the parser handle potential UTF-8 issues during parsing
    if (!json_parser_load_from_data(parser, response_buffer->str, -1, error))
    {
        g_warning("Failed to parse JSON data: %s\n", (*error) ? (*error)->message : "Unknown error");
        // Log the problematic buffer content for debugging
        g_warning("Problematic JSON buffer content: %s", response_buffer->str);
        g_object_unref(parser);
        return FALSE;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root))
    { // Added NULL check for root
        g_set_error(error, g_quark_from_static_string("JSON Error"), 1, "Root is not a JSON object or parsing failed");
        g_object_unref(parser);
        return FALSE;
    }

    JsonObject *root_object = json_node_get_object(root);

    // Check for top-level error first (common in some APIs)
    if (json_object_has_member(root_object, "error"))
    {
        JsonNode *error_node = json_object_get_member(root_object, "error");
        if (JSON_NODE_HOLDS_VALUE(error_node))
        {
            response->error = safe_strdup(json_node_get_string(error_node));
        }
        else if (JSON_NODE_HOLDS_OBJECT(error_node))
        {
            // Try to extract a message field if the error is an object
            JsonObject *error_obj = json_node_get_object(error_node);
            if (json_object_has_member(error_obj, "message"))
            {
                response->error = safe_strdup(json_object_get_string_member(error_obj, "message"));
            }
            else
            {
                // Fallback: stringify the error object (might be verbose)
                JsonGenerator *gen = json_generator_new();
                json_generator_set_root(gen, error_node);
                response->error = json_generator_to_data(gen, NULL);
                g_object_unref(gen);
            }
        }
        // If there's an error, we might not have choices, so don't make it fatal yet
    }

    // Extract text from choices if available
    if (json_object_has_member(root_object, "choices"))
    {
        JsonArray *choices = json_object_get_array_member(root_object, "choices");
        if (choices && json_array_get_length(choices) > 0)
        {
            JsonNode *first_choice_node = json_array_get_element(choices, 0);
            if (JSON_NODE_HOLDS_OBJECT(first_choice_node))
            {
                JsonObject *first_choice_obj = json_node_get_object(first_choice_node);

                // Handle different potential structures (e.g., OpenAI chat vs completion)
                const gchar *text_content = NULL;
                if (json_object_has_member(first_choice_obj, "text"))
                { // Completion API style
                    text_content = json_object_get_string_member(first_choice_obj, "text");
                }
                else if (json_object_has_member(first_choice_obj, "delta"))
                { // Chat API streaming style (delta)
                    JsonObject *delta_obj = json_object_get_object_member(first_choice_obj, "delta");
                    if (json_object_has_member(delta_obj, "content"))
                    {
                        text_content = json_object_get_string_member(delta_obj, "content");
                    }
                }
                else if (json_object_has_member(first_choice_obj, "message"))
                { // Chat API non-streaming style (message)
                    JsonObject *message_obj = json_object_get_object_member(first_choice_obj, "message");
                    if (json_object_has_member(message_obj, "content"))
                    {
                        text_content = json_object_get_string_member(message_obj, "content");
                    }
                }
                response->response_text = safe_strdup(text_content);
            }
        }
    }

   if (response->error)
    {
        g_warning("API returned error: %s\n", response->error);
    }

    g_object_unref(parser);

    // Return TRUE if we got *something* (text or error), FALSE only on parse failure
    return (response->response_text != NULL || response->error != NULL);
}

/// @brief Callback function for writing response data.
size_t llm_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    // Cast userp back to GString* which accumulates JSON parts
    GString *json_accumulator = (GString *)userp;

    if (!contents || !json_accumulator)
    {
        g_warning("NULL ptr received in write_callback.");
        return 0; // Return 0 to signal error to curl
    }

    // Append the raw chunk received from curl directly
    g_string_append_len(json_accumulator, (const gchar *)contents, total_size);

    // Process accumulated data line by line (Server-Sent Events are line-based)
    gchar *current_data = json_accumulator->str;
    gchar *next_line;

    while ((next_line = strstr(current_data, "\n\n")) != NULL)
    { // SSE messages end with \n\n
        gsize message_len = next_line - current_data;
        gchar *message = g_strndup(current_data, message_len);

        // Process the complete message
        gchar **lines = g_strsplit(message, "\n", -1);
        GString *json_data_part = g_string_new(NULL); // Buffer for this message's JSON

        for (gint i = 0; lines[i] != NULL; i++)
        {
            if (g_str_has_prefix(lines[i], "data: "))
            {
                const gchar *json_part = lines[i] + 6; // Skip "data: "
                if (strcmp(json_part, "[DONE]") == 0)
                {
                    g_print("Streaming completed ([DONE] received).\n");
                    // Stop spinner etc.
                    gdk_threads_add_idle((GSourceFunc)gtk_spinner_stop, llm_plugin->spinner);
                    gdk_threads_add_idle((GSourceFunc)gtk_widget_hide, llm_plugin->spinner);
                    // Clear the accumulator completely as we are done
                    g_string_erase(json_accumulator, 0, -1);
                    g_string_free(json_data_part, TRUE);
                    g_strfreev(lines);
                    g_free(message);
                    // Indicate processing success for the whole chunk containing [DONE]
                    return total_size;
                }
                else
                {
                    // Append JSON part of this line to the current message's JSON buffer
                    g_string_append(json_data_part, json_part);
                }
            }
            // Ignore other SSE lines like 'event:', 'id:', etc. for now
        }

        // Try to parse the accumulated JSON data for this message
        if (json_data_part->len > 0)
        {
            GError *error = NULL;
            LLMResponse response; // Stack variable for this message

            // Use the json_data_part GString which contains only this message's data
            if (llm_json_to_response(&response, json_data_part, &error))
            {
                if (response.response_text)
                {
                    // Pass ownership of a *copy* to the idle function
                    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, g_strdup(response.response_text));
                }
                if (response.error)
                {
                    // Handle error display if needed, maybe append to output too?
                    g_warning("Received error message from API: %s", response.error);
                    gchar *error_msg = g_strdup_printf("API Error: %s\n", response.error);
                    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, error_msg); 
                }

                // Free memory allocated by json_to_response for this message
                g_free(response.response_text);
                g_free(response.error);
                g_free(response.raw_json); // Free raw_json if it was stored
            }
            else
            {
                g_warning("Error parsing JSON message: %s", error ? error->message : "Unknown error");
                g_clear_error(&error);
            }
        }

        g_string_free(json_data_part, TRUE);
        g_strfreev(lines);
        g_free(message);

        gsize consumed_len = message_len + 2; // +2 for the \n\n
        g_string_erase(json_accumulator, 0, consumed_len);
        current_data = json_accumulator->str; // Point to the remaining data
    }

    return total_size;
}

/// @brief Execute LLM query using curl to connect to the LLM server
gboolean llm_execute_query(const gchar *server_uri, const gchar *proxy_url, const gchar *json_payload, LLMResponse *response)
{
    if (!server_uri || !json_payload)
    {

        return FALSE;
    }

    CURL *curl = curl_easy_init();

    if (!curl)
    {
        if (response)
            response->error = g_strdup("Failed to initialize curl.");
        return FALSE;
    }

    GString *accumulator_buffer = g_string_new(NULL);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream"); 

    curl_easy_setopt(curl, CURLOPT_URL, server_uri);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, llm_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, accumulator_buffer); 
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);           

    if (!IS_NULL_OR_EMPTY(proxy_url))
    {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        g_warning("cURL error: %s", curl_easy_strerror(res));
        if (response)
            response->error = g_strdup_printf("cURL error: %s", curl_easy_strerror(res));
        // Cleanup
        g_string_free(accumulator_buffer, TRUE);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return FALSE;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400)
    {
        g_warning("HTTP error: %ld", http_code);
        if (response && !response->error)
        {
            response->error = g_strdup_printf("HTTP error: %ld", http_code);
        }
    }

    // Cleanup
    g_string_free(accumulator_buffer, TRUE);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return TRUE;
}
/// @brief Thread function to handle LLM queries
gpointer llm_thread_func(gpointer data)
{
    ThreadData *thread_data = (ThreadData *)data;
    if (!data)
    {
        g_warning("NULL thread_data received\n");
        return NULL;
    }

    LLMPlugin *plugin = thread_data->llm_plugin;
    if (!plugin)
    {
        g_warning("NULL plugin descriptor received.");
        g_free(thread_data->query);
        g_free(thread_data->current_document);
        g_free(thread_data);
        return NULL;
    }

    gchar *query = thread_data->query;
    gchar *current_document = thread_data->current_document;
    LLMArgs *args = plugin->llm_args; 
    LLMResponse response = {0};

    const gchar *path = NULL;
    gchar *json_payload = NULL;
    gchar *server_uri = NULL;

    path = "/v1/completions"; 
    server_uri = llm_construct_server_uri_string(plugin->llm_server_url, path);
    if (!server_uri)
    {
        g_warning("Failed to construct server URI");
    }
    else
    {
        json_payload = llm_construct_completion_json_payload(query, current_document, args);
        if (!json_payload)
        {
            g_warning("Failed to construct JSON payload");
        }
        else
        {
            if (!llm_execute_query(server_uri, plugin->proxy_url, json_payload, &response))
            {
                // Curl level error occurred
                g_warning("execute_llm_query failed: %s", response.error ? response.error : "Unknown curl error");

                if (response.error)
                {
                    gchar *error_copy = g_strdup(response.error);
                    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, error_copy);
                }
            }
            else if (response.error)
            {
                g_warning("execute_llm_query completed with HTTP/API error: %s", response.error);

            }
        }
    }

    g_free(response.error); 
    g_free(json_payload);
    g_free(server_uri);
    g_free(query);
    g_free(current_document);
    g_free(thread_data);

    return NULL;
}
