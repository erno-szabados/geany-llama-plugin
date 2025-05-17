#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>


#include "plugin.h"
#include "llm.h"
#include "llm_http.h"
#include "llm_json.h"
#include "llm_util.h"

/// @brief Thread function to handle LLM queries
gpointer llm_thread_func(gpointer data)
{
    ThreadData *thread_data = (ThreadData *)data;
    if (!thread_data) {
        g_warning("NULL thread_data received\n");
        return NULL;
    }

    LLMPlugin *plugin = thread_data->llm_plugin;
    LLMCallbacks *callbacks = thread_data->callbacks;
    
    if (!plugin) {
        g_warning("NULL plugin descriptor received.");
        if (callbacks && callbacks->on_error) {
            callbacks->on_error("Invalid plugin configuration", callbacks->user_data);
        }
        
        g_free(thread_data->query);
        g_free(thread_data->current_document);
        g_free(callbacks);
        g_free(thread_data);
        return NULL;
    }

    gchar *query = thread_data->query;
    gchar *current_document = thread_data->current_document;
    LLMArgs *args = plugin->llm_args;

    const gchar *path = "/v1/completions";
    gchar *json_payload = NULL;
    
    // Validate server URL before attempting to construct the URI
    if (!plugin->llm_server_url || plugin->llm_server_url[0] == '\0') {
        if (callbacks && callbacks->on_error) {
            callbacks->on_error("Server URL is not configured. Please set it in the plugin settings.", callbacks->user_data);
        }
        goto EXIT;
    }
    
    gchar *server_uri = llm_construct_server_uri_string(plugin->llm_server_url, path);

    if (!server_uri) {
        if (callbacks && callbacks->on_error) {
            callbacks->on_error("Failed to construct server URI", callbacks->user_data);
        }
        goto EXIT;
    }

    json_payload = llm_construct_completion_json_payload(query, current_document, args);
    if (!json_payload) {
        if (callbacks && callbacks->on_error) {
            callbacks->on_error("Failed to construct JSON payload", callbacks->user_data);
        }
        goto EXIT;
    }
    g_print("%s\n", json_payload);

    // Execute the query with callbacks
    llm_execute_query(server_uri, plugin->proxy_url, json_payload, callbacks, thread_data->cancel_flag);
    
EXIT:
    g_free(json_payload);
    g_free(server_uri);
    g_free(query);
    g_free(current_document);
    g_free(thread_data);
    // Don't free callbacks here as they might still be used in async operations
    
    // Free callbacks at the end
    if (callbacks) {
        g_free(callbacks);
    }

    return NULL;
}
