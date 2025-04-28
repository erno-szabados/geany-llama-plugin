#ifndef __LLM_H__
#define __LLM_H__

#include "types.h"

/**
 * Functions interfacing with the LLM via the OpenAI API (libcurl).
 */

/// @brief Pass the LLM completions endpoint the specified query and return the response.
/// @param LLMPlugin *plugin
/// @param const gchar *query
/// @param const LLMArgs *args
/// @return LLMResponse the response on success, empty or error response on error.
LLMResponse *llm_query_completions(LLMPlugin *plugin, const gchar *query, const LLMArgs *args);

/// @brief Pass the LLM chat completions endpoint the specified query and return the response.
/// @param LLMPlugin *plugin
/// @param const gchar *query
/// @param const LLMArgs *args
/// @return LLMResponse the response on success, empty or error response on error.
LLMResponse *llm_query_chat_completions(LLMPlugin *plugin, const gchar *query, const LLMArgs *args);

/// @brief Free the response 
/// @param LLMResponse *response
void llm_free_response(LLMResponse *response);

/// @brief Function to update the UI (runs on the main thread)
gboolean llm_update_ui(LLMResponse *response);

/// @brief Worker thread function
gpointer llm_thread_func(gpointer data);

#endif // __LLM_H__
