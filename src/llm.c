#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <json-glib/json-glib.h> // libjson-glib-dev
#include <curl/curl.h>

#include "plugin.h"
#include "llm.h"

void free_llm_response(LLMResponse *response);

gboolean populate_llm_response(LLMResponse *response, const gchar *raw_json, GError **error);

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);

// Function to query LLM server
// Fix the following function to properly use host and port from llm_server_url
LLMResponse *query_llm(LLMPlugin *plugin, const gchar *query, const LLMArgs *args) {
    CURL *curl;
    CURLcode res;
    GUri *uri = NULL;
    gchar *uri_string = NULL;
    gchar *json_payload = NULL;
    GError *error = NULL;
    LLMResponse *response = g_new(LLMResponse, 1); // Initialize response struct

    // Variables for extracted host and port
    gchar *extracted_host = NULL;
    gint extracted_port = -1;
    gchar *temp_uri_string = NULL;
    GUri *temp_uri = NULL;


    // Initialize libcurl
    curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "curl_easy_init() failed\n");
        return response; // Return empty response on failure
    }

    // --- Split host and port from plugin->llm_server_url ---
    // We prepend a dummy scheme to use g_uri_parse for robust splitting
    temp_uri_string = g_strdup_printf("dummy://%s", plugin->llm_server_url);
    if (!temp_uri_string) {
         fprintf(stderr, "Failed to allocate memory for temporary URI string\n");
         curl_easy_cleanup(curl);
         return response;
    }

    temp_uri = g_uri_parse(temp_uri_string, G_URI_FLAGS_NONE, &error);
    g_free(temp_uri_string); // Free the temporary string immediately

    if (error) {
        g_printerr("Error parsing LLM server URL '%s': %s\n", plugin->llm_server_url, error->message);
        g_clear_error(&error);
        curl_easy_cleanup(curl);
        return response;
    }

    // Extract host and port
    extracted_host = (gchar*)g_uri_get_host(temp_uri); // g_uri_get_host returns a new string
    extracted_port = g_uri_get_port(temp_uri);

    // Check if host extraction was successful
    if (!extracted_host) {
        g_printerr("Could not extract host from LLM server URL '%s'\n", plugin->llm_server_url);
        g_object_unref(temp_uri);
        curl_easy_cleanup(curl);
        return response;
    }

    // We are done with the temporary URI object
    g_object_unref(temp_uri);

    // --- Construct the JSON payload using GLib ---
    // Basic escaping for quotes in prompt. For full JSON safety,
    // you might need a proper JSON library or more robust escaping.
    gchar *escaped_query = g_strescape(query, "\"\\");

    json_payload = g_strdup_printf("{"
                                     "\"model\": \"%s\","
                                     "\"prompt\": \"%s\","
                                     "\"max_tokens\": %d,"
                                     "\"temperature\": %.2f"
                                     "}",
                                     args->model, escaped_query, args->max_tokens, args->temperature);

    g_free(escaped_query); // Free the escaped query string

    if (!json_payload) {
         fprintf(stderr, "Failed to allocate memory for JSON payload\n");
         g_free(extracted_host); // Free extracted host
         curl_easy_cleanup(curl);
         return response;
    }


    // --- Construct the full request URL using g_uri_build with separated host and port ---
    const gchar *scheme = "http"; // Assuming http for the LLM server connection
    const gchar *path = "/v1/completions";
    const gchar *userinfo = NULL; // Assuming no userinfo for this endpoint

    uri = g_uri_build (G_URI_FLAGS_NONE,
                       scheme,
                       userinfo,
                       extracted_host, // Use the extracted host
                       extracted_port, // Use the extracted port
                       path,
                       NULL, // No query parameters for this path
                       NULL); // No fragment for this path

    // Free the extracted host string after using it in g_uri_build
    g_free(extracted_host);

    // Check if URI building was successful
    if (uri == NULL) {
        // g_uri_build sets the error if it fails, but we check explicitly too
        g_printerr ("Error building final HTTP URI: %s\n", error ? error->message : "unknown error");
        g_clear_error (&error);
        g_free(json_payload);
        curl_easy_cleanup(curl);
        return response;
    }

    // Convert the GUri back to a string for curl
    uri_string = g_uri_to_string(uri);

    // We are done with the GUri object
    g_object_unref(uri);

    if (!uri_string) {
        fprintf(stderr, "Failed to convert URI to string\n");
        g_free(json_payload);
        curl_easy_cleanup(curl);
        return response;
    }
    
    GString *response_data = g_string_new(NULL);


    // --- Set cURL options ---
    curl_easy_setopt(curl, CURLOPT_URL, uri_string);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_data);

    // Set HTTP headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    // Add other headers as needed, e.g., Authorization
    // headers = curl_slist_append(headers, "Authorization: Bearer YOUR_API_KEY");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);


    // --- Perform the request ---
    res = curl_easy_perform(curl);

    // --- Check for errors ---
    if (res != CURLE_OK) {
            g_warning("curl_easy_perform() failed: %s", curl_easy_strerror(res));
        } else {
            // Populate the response struct
            if (populate_llm_response(response, response_data->str, &error)) {
                g_print("Response Text: %s\n", response->response_text);
                g_print("Error: %s\n", response->error ? response->error : "No error");
                g_print("Raw JSON: %s\n", response->raw_json);
            } else {
                g_warning("Error parsing JSON: %s", error->message);
                g_clear_error(&error);
            }
        }

        // Cleanup
    curl_easy_cleanup(curl);
    g_string_free(response_data, TRUE);
    free_llm_response(response);
    g_free(json_payload);       // Free GLib-allocated memory for payload
    g_free(uri_string);         // Free GLib-allocated memory for URI string

    return response;
}

// Callback function for writing response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    GString *response = (GString *)userp;

    g_string_append_len(response, contents, total_size);

    return total_size;
}

// Function to populate LLMResponse from raw JSON data
gboolean populate_llm_response(LLMResponse *response, const gchar *raw_json, GError **error) {
    JsonParser *parser = json_parser_new();
    gboolean success = json_parser_load_from_data(parser, raw_json, -1, error);

    if (!success) {
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
        JsonObject *first_choice = json_array_get_object_element(choices, 0);
        const gchar *text = json_object_get_string_member(first_choice, "text");
        response->response_text = g_strdup(text);
    } else {
        response->response_text = NULL;
    }

    if (json_object_has_member(root_object, "error")) {
        const gchar *error_message = json_object_get_string_member(root_object, "error");
        response->error = g_strdup(error_message);
    } else {
        response->error = NULL;
    }

    response->raw_json = g_strdup(raw_json);

    g_object_unref(parser);
    return TRUE;
}

// Clean up the LLMResponse
void free_llm_response(LLMResponse *response) {
    g_free(response->response_text);
    g_free(response->error);
    g_free(response->raw_json);
}



