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
    if (!thread_data)
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

    const gchar *path = "/v1/completions";
    gchar *json_payload = NULL;
    gchar *server_uri = llm_construct_server_uri_string(plugin->llm_server_url, path);

    if (!server_uri)
    {
        g_warning("Failed to construct server URI");
        goto EXIT;
    }

    json_payload = llm_construct_completion_json_payload(query, current_document, args);
    if (!json_payload)
    {
        g_warning("Failed to construct JSON payload");
        goto EXIT;
    }

    if (!llm_execute_query(server_uri, plugin->proxy_url, json_payload, &response))
    {
        // Curl level error occurred
        g_warning("execute_llm_query failed: %s", response.error ? response.error : "Unknown cUrl error");

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
    
EXIT:
    g_free(response.error); 
    g_free(json_payload);
    g_free(server_uri);
    g_free(query);
    g_free(current_document);
    g_free(thread_data);

    return NULL;
}
