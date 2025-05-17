#include <curl/curl.h>
#include "llm_http.h"
#include "llm_json.h"
#include "llm_util.h"

// Add this helper function


gboolean llm_append_to_output_buffer(gpointer user_data) {
    // The user_data is the string duplicated in data_received callback
    gchar *text_to_append = (gchar *)user_data;

    if (!text_to_append || !llm_plugin || !llm_plugin->output_text_view) {
        g_warning("Invalid text or plugin state.");
        g_free(text_to_append);
        return FALSE;          
    }

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(llm_plugin->output_text_view));
    if (buffer) {
        GtkTextIter end_iter;
        gtk_text_buffer_get_end_iter(buffer, &end_iter);
        // Insert the text chunk
        gtk_text_buffer_insert(buffer, &end_iter, text_to_append, -1);
    } else {
        g_warning("Failed to get text buffer.");
    }

    // Free the duplicated string passed via user_data
    g_free(text_to_append);

    return FALSE; // Remove the idle source
}

size_t llm_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t total_size = size * nmemb;
    WriteCallbackData *callback_data = (WriteCallbackData *)userp;
    
    if (!contents || !callback_data || !callback_data->accumulator) {
        g_warning("NULL ptr received in write_callback.");
        return 0; // Return 0 to signal error to curl
    }

    GString *json_accumulator = callback_data->accumulator;
    LLMCallbacks *callbacks = callback_data->callbacks;
    gboolean *cancel_flag = callback_data->cancel_flag;

    // Check if cancellation is requested
    if (cancel_flag && *cancel_flag) {
        // Signal completion to clean up UI
        if (callbacks && callbacks->on_complete) {
            callbacks->on_complete(callbacks->user_data);
        }
        // Return 0 to make curl abort the transfer
        return 0;
    }

    // Append the raw chunk received from curl directly
    g_string_append_len(json_accumulator, (const gchar *)contents, total_size);

    // Process accumulated data line by line (Server-Sent Events are line-based)
    gchar *current_data = json_accumulator->str;
    gchar *next_line;

    while ((next_line = strstr(current_data, "\n\n")) != NULL) { // SSE messages end with \n\n
        gsize message_len = next_line - current_data;
        gchar *message = g_strndup(current_data, message_len);

        // Process the complete message
        gchar **lines = g_strsplit(message, "\n", -1);
        GString *json_data_part = g_string_new(NULL); // Buffer for this message's JSON

        for (gint i = 0; lines[i] != NULL; i++) {
            if (g_str_has_prefix(lines[i], "data: ")) {
                const gchar *json_part = lines[i] + 6; // Skip "data: "
                if (strcmp(json_part, "[DONE]") == 0) {
                    g_print("Streaming completed ([DONE] received).\n");
                    
                    // Signal completion via callback
                    if (callbacks && callbacks->on_complete) {
                        callbacks->on_complete(callbacks->user_data);
                    }
                    
                    // Clear the accumulator completely as we are done
                    g_string_erase(json_accumulator, 0, -1);
                    g_string_free(json_data_part, TRUE);
                    g_strfreev(lines);
                    g_free(message);
                    
                    // Indicate processing success for the whole chunk containing [DONE]
                    return total_size;
                } else {
                    // Append JSON part of this line to the current message's JSON buffer
                    g_string_append(json_data_part, json_part);
                }
            }
            // Ignore other SSE lines like 'event:', 'id:', etc. for now
        }

        // Try to parse the accumulated JSON data for this message
        if (json_data_part->len > 0) {
            GError *error = NULL;
            LLMResponse response = {0}; // Stack variable for this message

            // Use the json_data_part GString which contains only this message's data
            if (llm_json_to_response(&response, json_data_part, &error)) {
                if (response.response_text && callbacks && callbacks->on_data_received) {
                    callbacks->on_data_received(response.response_text, callbacks->user_data);
                }
                
                if (response.error && callbacks && callbacks->on_error) {
                    callbacks->on_error(response.error, callbacks->user_data);
                }

                // Free memory allocated by json_to_response for this message
                g_free(response.response_text);
                g_free(response.error);
            } else if (callbacks && callbacks->on_error) {
                const gchar *error_msg = error ? error->message : "Unknown JSON parsing error";
                callbacks->on_error(error_msg, callbacks->user_data);
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

// Update the llm_execute_query function
gboolean llm_execute_query(
    const gchar *server_uri, 
    const gchar *proxy_url, 
    const gchar *json_payload, 
    LLMCallbacks *callbacks,
    gboolean *cancel_flag) {
    if (!server_uri || !json_payload) {
        if (callbacks && callbacks->on_error) {
            callbacks->on_error("Invalid server URI or JSON payload", callbacks->user_data);
        }
        return FALSE;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        if (callbacks && callbacks->on_error) {
            callbacks->on_error("Failed to initialize curl", callbacks->user_data);
        }
        return FALSE;
    }

    GString *accumulator_buffer = g_string_new(NULL);
    
    // Create WriteCallbackData to pass both the accumulator and callbacks
    WriteCallbackData callback_data = {
        .accumulator = accumulator_buffer,
        .callbacks = callbacks,
        .cancel_flag = cancel_flag
    };

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: text/event-stream"); 

    g_print("SERVER URI: %s\n", server_uri);
    curl_easy_setopt(curl, CURLOPT_URL, server_uri);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, llm_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &callback_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);           

    if (!IS_NULL_OR_EMPTY(proxy_url)) {
        curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url);
    }

    // Add Authorization header if API key is set
    if (llm_plugin && !IS_NULL_OR_EMPTY(llm_plugin->api_key)) {
        gchar *auth_header = g_strdup_printf("Authorization: Bearer %s", llm_plugin->api_key);
        headers = curl_slist_append(headers, auth_header);
        g_free(auth_header);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        if (callbacks && callbacks->on_error) {
            callbacks->on_error(curl_easy_strerror(res), callbacks->user_data);
        }
        g_string_free(accumulator_buffer, TRUE);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return FALSE;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400 && callbacks && callbacks->on_error) {
        gchar *error_msg = g_strdup_printf("HTTP error: %ld", http_code);
        callbacks->on_error(error_msg, callbacks->user_data);
        g_free(error_msg);
    }

    // Cleanup
    g_string_free(accumulator_buffer, TRUE);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return TRUE;
}
