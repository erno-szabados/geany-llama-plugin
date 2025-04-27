#include "plugin.h"

// Static instance of your plugin data
static LLMPlugin *llm_plugin = NULL;

// --- Geany Plugin Entry Points ---

// Called when the plugin is initialized
gboolean llm_plugin_init(GeanyPlugin *plugin, gpointer pdata)
{
    llm_plugin = g_new(LLMPlugin, 1);
    llm_plugin->geany_plugin = plugin;
    llm_plugin->geany_data = plugin->geany_data;
    g_print("LLM Plugin init");

    // --- Create your plugin's UI ---
    // This is where you would create your GTK widgets for the LLM panel,	
    // input, output, etc. and add them to Geany's UI (e.g., in the message window area).
    // You'll need to refer to the Geany and GTK API documentation for this.

    // Example (conceptual): Create a panel
    // llm_plugin->llm_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    // geany_plugin_add_widget(plugin, GEANY_PLUGIN_WIDGET_MSGWIN, llm_plugin->llm_panel, FALSE, FALSE);

    // --- Connect signals ---
    // Connect GTK signals from your UI elements (e.g., button clicks)
    // Connect Geany signals to react to editor events (e.g., file opened)

    // Example (conceptual): Connect a button click signal
    // g_signal_connect(button, "clicked", G_CALLBACK(on_send_button_clicked), NULL);

    // geany_signal_connect(geany_plugin->geany_data, "document-open", &on_document_open, NULL);


    // --- Initialize other plugin components ---
    // Initialize libcurl, gettext, etc. if needed at startup
    // curl_global_init(CURL_GLOBAL_DEFAULT);
    return TRUE;
}

// Called when the plugin is unloaded
void llm_plugin_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
	g_print("LLM Plugin cleanup");
    // --- Clean up resources ---
    // Destroy your GTK UI elements
    // Free allocated memory
    // Clean up libcurl, gettext, etc.

    if (llm_plugin)
    {
        // Example (conceptual): Destroy the panel
        if (llm_plugin->llm_panel)
            gtk_widget_destroy(llm_plugin->llm_panel);
        if (llm_plugin->config_panel)
			gtk_widget_destroy(llm_plugin->config_panel);

        g_free(llm_plugin);
        llm_plugin = NULL;
    }

    // curl_global_cleanup();
}

GtkWidget *llm_plugin_configure(GeanyPlugin *plugin, GtkDialog *dialog, gpointer pdata) 
{
	g_print("LLM Plugin configure");
	// TODO!
	return llm_plugin->config_panel;
}

void geany_load_module(GeanyPlugin *plugin) 
{
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	/* Step 1: Set metadata */
    plugin->info->name = GEANY_LLM_PLUGIN_NAME;
    plugin->info->description = GEANY_LLM_PLUGIN_DESCRIPTION;
    plugin->info->version = GEANY_LLM_PLUGIN_VERSION;
    plugin->info->author =  GEANY_LLM_PLUGIN_AUTHOR;
 
    /* Step 2: Set functions */
    plugin->funcs->init = llm_plugin_init;
    plugin->funcs->cleanup = llm_plugin_cleanup;
	plugin->funcs->configure = llm_plugin_configure;	
 
    /* Step 3: Register! */
    GEANY_PLUGIN_REGISTER(plugin, 225);
}



// --- Implement your plugin's features here ---

// Example (conceptual): Function to handle sending a query to the LLM server
// void on_send_button_clicked(GtkButton *button, gpointer user_data)
// {
//     const char *user_query = gtk_entry_get_text(GTK_ENTRY(llm_plugin->input_entry));
//     // Get editor context using Geany API
//     // Construct HTTP request with libcurl
//     // Send request asynchronously
//     // Handle response and update output_view
// }

// Example (conceptual): Function to handle document opening event
// void on_document_open(GSignalHandlerBypass *bypass, gpointer data)
// {
//     // React to a document being opened if needed
// }
