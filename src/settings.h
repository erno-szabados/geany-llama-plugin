#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include "plugin.h"

#define LLM_SERVER_URL_KEY "llm_server_url"
#define LLM_ARGS_MODEL_KEY "model"
#define LLM_ARGS_TEMPERATURE_KEY "temperature"
#define LLM_ARGS_MAX_TOKENS_KEY "max_tokens"
#define PROXY_URL_KEY "proxy"
#define LLM_API_KEY "api_key"

/**
 * Functions to load and save the plugin configuration.
 */

/// @brief save settings to config file.
void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data);

/// @brief Load settings from config file.
void llm_plugin_settings_load(gpointer user_data);

#endif //__SETTINGS_H__
