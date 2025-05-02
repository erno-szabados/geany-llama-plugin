
#include <curl/curl.h>
#include "llm_http.h"
#include "llm_json.h"
#include "llm_util.h"

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