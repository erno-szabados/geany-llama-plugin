#include <curl/curl.h>

#include "plugin.h"
#include "settings.h"
#include "llm.h"
#include <Scintilla.h>
#include <SciLexer.h>

#include "llm_http.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static GtkWidget *create_llm_input_widget(gpointer user_data);
static GtkWidget *create_llm_output_widget();

static void on_llm_data_received(const gchar *data_chunk, gpointer user_data);
static void on_llm_error(const gchar *error_message, gpointer user_data);
static void on_llm_complete(gpointer user_data);

// Static instance of your plugin data
LLMPlugin *llm_plugin = NULL;

/// @brief Called when the plugin is initialized
gboolean llm_plugin_init(GeanyPlugin *plugin, gpointer pdata)
{
    llm_plugin = g_new0(LLMPlugin, 1);
    llm_plugin->geany_plugin = plugin;
    llm_plugin->geany_data = plugin->geany_data;
    llm_plugin->llm_panel = NULL;
    llm_plugin->llm_server_url = NULL;
    llm_plugin->proxy_url = NULL;
    llm_plugin->llm_args = g_new0(LLMArgs, 1); 
    g_print("LLM Plugin init\n");

    // TODO: make them configurable
    llm_plugin->llm_args->max_tokens = 1024;
    llm_plugin->llm_args->temperature = 0.8f;
    llm_plugin->llm_args->model = NULL;

    llm_plugin_settings_load(llm_plugin);

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
    
    // Add the panel to the notebook
    llm_plugin->page_number = gtk_notebook_append_page(
            GTK_NOTEBOOK(llm_plugin->geany_data->main_widgets->sidebar_notebook),
            llm_plugin->llm_panel,
            gtk_label_new(_("AI Model")));

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


    // Pack the label and entry into the vertical box
    gtk_box_pack_start(GTK_BOX(vbox), url_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), llm_plugin->url_entry, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), proxy_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), llm_plugin->proxy_entry, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), model_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), llm_plugin->model_entry, FALSE, FALSE, 0);

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

/// @brief Handle clear button click event
static void on_input_clear_clicked(GtkButton *button, gpointer user_data)
{
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;    
    if (!llm_plugin) {
        return;
    }
    g_print("Clear Button was clicked!\n");
    gtk_entry_set_text(GTK_ENTRY(llm_plugin->input_text_entry), "");
}


/// @brief Get the current document as a string.
/// Remember to g_free(document_content) when done
static gchar *get_current_document(gpointer user_data)
{
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;
    if (!llm_plugin) {
        return NULL;
    }
    //GeanyDocument *doc = llm_plugin->geany_data->editor->document;
    GeanyDocument *doc = document_get_current();

    if (!doc) {
         g_warning("No active document found.");
        return NULL;
    }
    
    gsize length = scintilla_send_message(doc->editor->sci, SCI_GETTEXTLENGTH, 0, 0);
    gchar* document_content = g_malloc(length + 1); // +1 for null-terminator
    scintilla_send_message(doc->editor->sci, SCI_GETTEXT, length + 1, (sptr_t)document_content);
    
    if (!document_content) {
        g_warning("Failed to allocate memory for document content.");
        return NULL;
    }

    return document_content;
}

static void on_llm_data_received(const gchar *data_chunk, gpointer user_data) {
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin || !data_chunk || !plugin->output_text_view) {
        return;
    }

    // Run in the main thread via GDK
    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, g_strdup(data_chunk));
}

static void on_llm_error(const gchar *error_message, gpointer user_data) {
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin || !error_message) {
        return;
    }

    // Format and display the error message
    gchar *formatted_error = g_strdup_printf("Error: %s\n", error_message);
    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, formatted_error);
}

static void on_llm_complete(gpointer user_data) {
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin || !plugin->spinner) {
        return;
    }

    // Stop and hide the spinner
    gdk_threads_add_idle((GSourceFunc)gtk_spinner_stop, plugin->spinner);
    gdk_threads_add_idle((GSourceFunc)gtk_widget_hide, plugin->spinner);
}


/// @brief Handle send button click event in a separate thread.
static void on_input_send_clicked(GtkButton *button, gpointer user_data) {
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;
    if (!llm_plugin)
        return;

    const gchar *input_text = gtk_entry_get_text(GTK_ENTRY(llm_plugin->input_text_entry));
    if (!input_text || *input_text == '\0') {
        g_print("Input text is empty!\n");
        return;
    }

    // Show and start the spinner
    gtk_widget_show(llm_plugin->spinner);
    gtk_spinner_start(GTK_SPINNER(llm_plugin->spinner));

    // Clear previous output
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(llm_plugin->output_text_view));
    if (buffer) {
        gtk_text_buffer_set_text(buffer, "", -1);
    }

    // Create a copy of the query and thread data
    gchar *query = g_strdup(input_text);
    gchar *current_document = get_current_document(llm_plugin);
    
    // Create callbacks structure
    LLMCallbacks *callbacks = g_new0(LLMCallbacks, 1);
    callbacks->on_data_received = on_llm_data_received;
    callbacks->on_error = on_llm_error;
    callbacks->on_complete = on_llm_complete;
    callbacks->user_data = llm_plugin;
    
    ThreadData *thread_data = g_malloc(sizeof(ThreadData));
    thread_data->llm_plugin = llm_plugin;
    thread_data->query = query;
    thread_data->current_document = current_document;
    thread_data->callbacks = callbacks;

    // Create a new thread to handle the blocking network request
    g_thread_new("llm-thread", llm_thread_func, thread_data);
}

/// @brief Invoke the same functionality as the send button click
static void on_input_enter_activate(GtkEntry *entry, gpointer user_data) {
    on_input_send_clicked(GTK_BUTTON(NULL), user_data);
}

/// @brief Create the input part of the plugin window.
static GtkWidget *create_llm_input_widget(gpointer user_data) {
    // Create the main vertical box
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    // Create the top row (horizontal box) for label and buttons
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    // Create the label
    GtkWidget *input_label = gtk_label_new(_("Request"));

    // Create the "Clear" button with an icon
    GtkWidget *clear_button = gtk_button_new();
    GtkWidget *clear_icon = gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(clear_button), clear_icon);
    g_signal_connect(G_OBJECT(clear_button), "clicked", G_CALLBACK(on_input_clear_clicked), user_data);

    // Create the "Send" button with an icon
    GtkWidget *send_button = gtk_button_new();
    GtkWidget *send_icon = gtk_image_new_from_icon_name("document-send", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(send_button), send_icon);
    g_signal_connect(G_OBJECT(send_button), "clicked", G_CALLBACK(on_input_send_clicked), user_data);

    // Add label and buttons to the top row
    gtk_box_pack_start(GTK_BOX(top_row), input_label, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top_row), send_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top_row), clear_button, FALSE, FALSE, 0);

    // Create a text view (textarea) for the query
    GtkWidget *text_entry = gtk_entry_new();
    llm_plugin->input_text_entry = text_entry;
    g_signal_connect(llm_plugin->input_text_entry, "activate", G_CALLBACK(on_input_enter_activate), llm_plugin);
    
    // Add the top row and text view to the main box
    gtk_box_pack_start(GTK_BOX(main_box), top_row, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), text_entry, TRUE, TRUE, 0);

    return main_box;
}

/// @brief Create the output part of the plugin window.
static GtkWidget *create_llm_output_widget() 
{
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *text_view = gtk_text_view_new();
    llm_plugin->output_text_view = text_view;
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    
    // Create the top row (horizontal box) for label and buttons
    GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    // Create the label
    GtkWidget *output_label = gtk_label_new(_("Answer"));
    gtk_box_pack_start(GTK_BOX(top_row), output_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_box), top_row, FALSE, FALSE, 0);  
    
    GtkWidget *scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scrollwin, TRUE);
    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scrollwin),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrollwin), text_view);
    gtk_box_pack_start(GTK_BOX(main_box), scrollwin, TRUE, TRUE, 0);    
    
    GtkWidget *spinner = gtk_spinner_new();
    gtk_widget_set_halign(spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(spinner, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(main_box), spinner, FALSE, FALSE, 0);  
    gtk_widget_hide(spinner); // Hide it initially
    llm_plugin->spinner = spinner; // Save the spinner in the plugin struct

    return main_box;
}

