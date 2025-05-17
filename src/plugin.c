#include <curl/curl.h>
#include <locale.h>

#include "plugin.h"
#include "settings.h"
#include "llm.h"
#include <Scintilla.h>
#include <SciLexer.h>

#include "document_manager.h"
#include "ui.h"
#include "request_handler.h"

#include "llm_http.h"
#include "llm_json.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// Static instance of your plugin data
LLMPlugin *llm_plugin = NULL;

/// @brief Called when the plugin is initialized
gboolean llm_plugin_init(GeanyPlugin *plugin, gpointer pdata)
{
    g_print("LLM Plugin init\n");
    // Set the locale to "C" for numeric formatting
    setlocale(LC_NUMERIC, "C");

    llm_plugin = g_new0(LLMPlugin, 1);
    llm_plugin->geany_plugin = plugin;
    llm_plugin->geany_data = plugin->geany_data;
    llm_plugin->llm_panel = NULL;
    llm_plugin->llm_server_url = NULL;
    llm_plugin->proxy_url = NULL;
    llm_plugin->llm_args = g_new0(LLMArgs, 1); 

    llm_plugin->is_generating = FALSE;
    llm_plugin->cancel_requested = FALSE;
    llm_plugin->active_thread_data = NULL;

    // TODO: make them configurable
    llm_plugin->llm_args->max_tokens = 1024;
    llm_plugin->llm_args->temperature = 0.8f;
    llm_plugin->llm_args->model = NULL;

    llm_plugin_settings_load(llm_plugin);

    llm_plugin->selected_document_ids = NULL;
    llm_plugin->include_current_document = TRUE; // Default to including current document

    // Parent panel
    llm_plugin->llm_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_vexpand(llm_plugin->llm_panel, TRUE);
    
    // Input view
    llm_plugin->input_widget = create_llm_input_widget(llm_plugin);
    gtk_box_pack_start(GTK_BOX(llm_plugin->llm_panel), llm_plugin->input_widget, FALSE, FALSE, 5);

    // Output view 
    llm_plugin->output_widget = create_llm_output_widget();
    gtk_box_pack_start(GTK_BOX(llm_plugin->llm_panel), llm_plugin->output_widget, TRUE, TRUE, 5);
    
    gtk_widget_show_all(llm_plugin->llm_panel);
    
    // Extra safeguard: Make sure the stop button is disabled at initialization
    if (llm_plugin->stop_button) {
        gtk_widget_set_sensitive(llm_plugin->stop_button, FALSE);
    }
    
    // Add the panel to the notebook
    llm_plugin->page_number = gtk_notebook_append_page(
            GTK_NOTEBOOK(llm_plugin->geany_data->main_widgets->sidebar_notebook),
            llm_plugin->llm_panel,
            gtk_label_new(_("AI Model")));
            
    // After UI is created, update the document button state
    update_document_button_state(llm_plugin);

    // Connect to the document-close signal
    plugin_signal_connect(plugin, NULL, "document-close", TRUE, 
                         G_CALLBACK(on_document_close), llm_plugin);

    return TRUE;
}

/// @brief Called when the plugin is unloaded
void llm_plugin_cleanup(GeanyPlugin *plugin, gpointer pdata)
{
    g_print("LLM Plugin cleanup\n");
    if (llm_plugin)
    {
        g_free(llm_plugin->llm_args);
        g_free(llm_plugin->llm_server_url);
        g_free(llm_plugin->proxy_url);
        if (llm_plugin->llm_panel)
            gtk_widget_destroy(llm_plugin->llm_panel);
        if (llm_plugin->selected_document_ids)
            g_ptr_array_free(llm_plugin->selected_document_ids, TRUE);
        g_free(llm_plugin);
        llm_plugin = NULL;
    }

    curl_global_cleanup();
}

/// @brief Function to create the configuration widget for the plugin
GtkWidget *llm_plugin_configure(GeanyPlugin *plugin, GtkDialog *dialog, gpointer pdata)
{
    GtkWidget *vbox = NULL; // Main container for the configuration options
    GtkWidget *url_label = NULL; 
    GtkWidget *proxy_label = NULL;
    GtkWidget *model_label = NULL; 
    GtkWidget *temperature_label = NULL;
    GtkWidget *temperature_spin = NULL;

    // Create a vertical box to hold the configuration widgets
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10); // Add some padding
    url_label = gtk_label_new(_("LLM Server URL:"));
    // Set label alignment to the left
    gtk_widget_set_halign(url_label, GTK_ALIGN_START);

    // Create an entry field for the URL
    llm_plugin->url_entry = gtk_entry_new();
    if (llm_plugin->llm_server_url)
    {
        gtk_entry_set_text(GTK_ENTRY(llm_plugin->url_entry), llm_plugin->llm_server_url);
    }
    else
    {
         gtk_entry_set_text(GTK_ENTRY(llm_plugin->url_entry), "");
    }
    
    gtk_entry_set_placeholder_text(GTK_ENTRY(llm_plugin->url_entry), "e.g., http://localhost:8080/completion");
    
    proxy_label = gtk_label_new(_("Proxy (optional):"));
    gtk_widget_set_halign(proxy_label, GTK_ALIGN_START);
    // Create an entry field for the Proxy
    llm_plugin->proxy_entry = gtk_entry_new();
    if (llm_plugin->proxy_url)
    {
        gtk_entry_set_text(GTK_ENTRY(llm_plugin->proxy_entry), llm_plugin->proxy_url);
    }
    else
    {
         gtk_entry_set_text(GTK_ENTRY(llm_plugin->proxy_entry), "");
    }
    
    gtk_entry_set_placeholder_text(GTK_ENTRY(llm_plugin->proxy_entry), "e.g., http://my.proxy.com:1080");

    // Create an entry field for the Model
    model_label = gtk_label_new(_("LLM Model:"));
    gtk_widget_set_halign(model_label, GTK_ALIGN_START);

    llm_plugin->model_entry = gtk_entry_new();
    if (llm_plugin->llm_args && llm_plugin->llm_args->model)
    {
        gtk_entry_set_text(GTK_ENTRY(llm_plugin->model_entry), llm_plugin->llm_args->model);
    }
    else
    {
         gtk_entry_set_text(GTK_ENTRY(llm_plugin->model_entry), "");
    }
    gtk_entry_set_placeholder_text(GTK_ENTRY(llm_plugin->model_entry), "e.g. qwen-coder-2.5");

    // Temperature label and spin button
    temperature_label = gtk_label_new(_("Temperature:"));
    gtk_widget_set_halign(temperature_label, GTK_ALIGN_START);
    temperature_spin = gtk_spin_button_new_with_range(0.0, 2.0, 0.01);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(temperature_spin), 2);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(temperature_spin), llm_plugin->llm_args ? llm_plugin->llm_args->temperature : 0.8);
    llm_plugin->temperature_spin = temperature_spin;

    // Pack the label and entry into the vertical box
    gtk_box_pack_start(GTK_BOX(vbox), url_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), llm_plugin->url_entry, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), proxy_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), llm_plugin->proxy_entry, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), model_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), llm_plugin->model_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), temperature_label, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), temperature_spin, FALSE, FALSE, 2);

    // Add any other configuration options here in a similar manner
    g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), llm_plugin);
    // Show all widgets in the box
    gtk_widget_show_all(vbox);

    // Return the main container widget. Geany will embed this in its configuration dialog.
    return vbox;
}

/// @brief Load the module
void geany_load_module(GeanyPlugin *plugin) 
{
        main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
        curl_global_init(CURL_GLOBAL_ALL);
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
