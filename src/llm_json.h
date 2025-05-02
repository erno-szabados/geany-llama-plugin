#ifndef __LLM_JSON_H__
#define __LLM_JSON_H__

#include "plugin.h" // LLMPlugin

/// @brief Construct the JSON request payload using json-glib for the completion endpoint
gchar* llm_construct_completion_json_payload(
    const gchar* query, 
    const gchar *current_document, 
    const LLMArgs* args);

/// @brief Construct the JSON request payload using json-glib for the chat completion endpoint
gchar* llm_construct_chat_completion_json_payload(const gchar* query, 
    const LLMArgs* args);

/// @brief Populate LLMResponse from raw JSON data.
gboolean llm_json_to_response(LLMResponse *response, GString *response_buffer, GError **error);

#endif // __LLM_JSON_H__