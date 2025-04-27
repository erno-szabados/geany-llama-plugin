#ifndef GEANY_LLM_PLUGIN_H
#define GEANY_LLM_PLUGIN_H

#include <geanyplugin.h>
#include <gtk/gtk.h> 

#include "types.h"

#define GEANY_LLM_PLUGIN_NAME _("LLama Assistant")
#define GEANY_LLM_PLUGIN_CONFIGNAME "geanyllm"
#define GEANY_LLM_PLUGIN_VERSION "0.1.0"
#define GEANY_LLM_PLUGIN_DESCRIPTION _("Integrates LLM capabilities into Geany.")
#define GEANY_LLM_PLUGIN_AUTHOR "Erno Szabados <erno.szabados@windowslive.com>"

gboolean llm_plugin_init(GeanyPlugin *plugin, gpointer pdata);
void llm_plugin_cleanup(GeanyPlugin *plugin, gpointer pdata);

G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin);

// Add function declarations for your plugin's features here
// For example, functions to create the UI, handle button clicks, etc.

extern LLMPlugin* llm_plugin;

#endif // GEANY_LLM_PLUGIN_H
