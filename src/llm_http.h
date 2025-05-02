#ifndef __LLM_HTTP_H__
#define __LLM_HTTP_H__

#include "plugin.h" // LLMPlugin

//// @brief  Append the received data to the output buffer
/// @param user_data the string duplicated in data_received callback
/// @return 
gboolean llm_append_to_output_buffer(gpointer user_data);

/// @brief Callback function for writing response data.
size_t llm_write_callback(
    void *contents, 
    size_t size, 
    size_t nmemb, 
    void *userp);

/// @brief Execute LLM query using curl to connect to the LLM server
// Update the function signature

gboolean llm_execute_query(
    const gchar *server_uri, 
    const gchar *proxy_url, 
    const gchar *json_payload, 
    LLMCallbacks *callbacks);
#endif // __LLM_HTTP_H__