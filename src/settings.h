#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include "plugin.h"

#define LLM_SERVER_URL_KEY "llm_server_url"

void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data);

void llm_plugin_settings_load(gpointer user_data);

#endif //__SETTINGS_H__
