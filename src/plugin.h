#ifndef GEANY_LLM_PLUGIN_H
#define GEANY_LLM_PLUGIN_H

#include <geanyplugin.h>
#include <gtk/gtk.h> 

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

// Structure to hold your plugin's data
typedef struct
{
    GeanyPlugin *geany_plugin;
    GeanyData   *geany_data;

    // Add your plugin's UI elements and data here
    GtkWidget *llm_panel; // Example: A pointer to your LLM chat panel
    GtkWidget *input_entry; // Example: Text entry for user input
    GtkWidget *output_view; // Example: Text view for LLM output
    GtkWidget *url_entry; // Entry for the LLM server URL
    
    // Plugin settings
     gchar *llm_server_url;

} LLMPlugin;

extern LLMPlugin *llm_plugin;

#endif // GEANY_LLM_PLUGIN_H
