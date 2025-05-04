#include <curl/curl.h>

#include "plugin.h"
#include "settings.h"
#include "llm.h"
#include <Scintilla.h>
#include <SciLexer.h>

#include "llm_http.h"
#include "llm_json.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static GtkWidget *create_llm_input_widget(gpointer user_data);
static GtkWidget *create_llm_output_widget();

static void on_llm_data_received(const gchar *data_chunk, gpointer user_data);
static void on_llm_error(const gchar *error_message, gpointer user_data);
static void on_llm_complete(gpointer user_data);
static void on_stop_generation_clicked(GtkButton *button, gpointer user_data);
static void on_select_documents_clicked(GtkButton *button, gpointer user_data);
static void on_document_toggled(GtkCellRendererToggle *cell, gchar *path_str, gpointer data);
static void on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data);
static void update_document_button_state(LLMPlugin *plugin);

// Static instance of your plugin data
LLMPlugin *llm_plugin = NULL;

/// @brief Called when the plugin is initialized
gboolean llm_plugin_init(GeanyPlugin *plugin, gpointer pdata)
{
    g_print("LLM Plugin init\n");

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

// Helper function to disable a widget from an idle callback
static gboolean disable_widget_idle(gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    if (widget) {
        gtk_widget_set_sensitive(widget, FALSE);
    }
    // Return FALSE (G_SOURCE_REMOVE) so the callback is only called once
    return G_SOURCE_REMOVE;
}

// Helper function to stop spinner from an idle callback
static gboolean stop_spinner_idle(gpointer user_data) {
    GtkSpinner *spinner = GTK_SPINNER(user_data);
    if (spinner) {
        gtk_spinner_stop(spinner);
    }
    // Return FALSE (G_SOURCE_REMOVE) so the callback is only called once
    return G_SOURCE_REMOVE;
}

static void on_llm_complete(gpointer user_data) {
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin) {
        return;
    }

    // Reset generation state
    plugin->is_generating = FALSE;
    plugin->active_thread_data = NULL;
    plugin->cancel_requested = FALSE;

    // Stop spinner and disable stop button - use direct calls for reliability
    gdk_threads_add_idle_full(G_PRIORITY_HIGH_IDLE,
        stop_spinner_idle,  // Use the helper function
        plugin->spinner,    // Pass the spinner widget
        NULL);

    g_print("Disabling stop button\n");
    // Correctly disable the stop button using the helper function
    gdk_threads_add_idle_full(G_PRIORITY_HIGH_IDLE,
        disable_widget_idle, // Use the helper function
        plugin->stop_button, // Pass the button as user_data
        NULL); // No destroy notify needed for the widget pointer
}

/// @brief Handle send button click event in a separate thread.
static void on_input_send_clicked(GtkButton *button, gpointer user_data) {
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;
    if (!llm_plugin)
        return;
        
    // If we're already generating, don't start another request
    if (llm_plugin->is_generating) {
        g_warning("Generation already in progress");
        return;
    }

    const gchar *input_text = gtk_entry_get_text(GTK_ENTRY(llm_plugin->input_text_entry));
    if (!input_text || *input_text == '\0') {
        g_print("Input text is empty!\n");
        return;
    }

    // Start spinner and enable stop button
    gtk_spinner_start(GTK_SPINNER(llm_plugin->spinner));
    gtk_widget_set_sensitive(llm_plugin->stop_button, TRUE);

    // Clear previous output
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(llm_plugin->output_text_view));
    if (buffer) {
        gtk_text_buffer_set_text(buffer, "", -1);
    }

    // Set state
    llm_plugin->is_generating = TRUE;
    llm_plugin->cancel_requested = FALSE;

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
    thread_data->cancel_flag = &llm_plugin->cancel_requested;
    
    // Store the thread data for potential cancellation
    llm_plugin->active_thread_data = thread_data;
    
    // Create a new thread to handle the blocking network request
    thread_data->thread = g_thread_new("llm-thread", llm_thread_func, thread_data);
}
/// @brief Invoke the same functionality as the send button click
static void on_input_enter_activate(GtkEntry *entry, gpointer user_data) {
    on_input_send_clicked(GTK_BUTTON(NULL), user_data);
}

/// @brief Handle stop button click event
static void on_stop_generation_clicked(GtkButton *button, gpointer user_data)
{
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin) {
        return;
    }
    
    // IMPORTANT: Guard against clicks when no generation is happening
    if (!plugin->is_generating || plugin->cancel_requested) {
        // Just disable the button and do nothing else - prevents freezes
        gtk_widget_set_sensitive(plugin->stop_button, FALSE);
        return;
    }
    
    g_print("Stop generation requested\n");
    
    // Set the cancel flag
    plugin->cancel_requested = TRUE;
    
    // Message to the output
    gchar *message = g_strdup("\n\n[Generation stopped by user]\n");
    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, message);
    
    // Update UI immediately by stopping spinner and disabling stop button
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
    stop_spinner_idle,  // Use the helper function 
    plugin->spinner,    // Pass the spinner widget
    NULL);
    
    // Disable the stop button since generation is stopping 
    gdk_threads_add_idle_full(G_PRIORITY_HIGH,
        disable_widget_idle, // Use the helper function
        plugin->stop_button, // Pass the button as user_data
        NULL); // No destroy notify needed
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

    // Create the "Stop" button with an icon
    GtkWidget *stop_button = gtk_button_new();
    GtkWidget *stop_icon = gtk_image_new_from_icon_name("process-stop", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(stop_button), stop_icon);
    gtk_widget_set_tooltip_text(stop_button, _("Stop generation"));
    g_signal_connect(G_OBJECT(stop_button), "clicked", G_CALLBACK(on_stop_generation_clicked), user_data);
    
    // IMPORTANT: Disable it initially - ensure it's inactive when starting
    gtk_widget_set_sensitive(stop_button, FALSE);
    
    // Store the reference to the stop button
    llm_plugin->stop_button = stop_button;

    // Create the "Documents" button with an icon
    GtkWidget *docs_button = gtk_button_new();
    GtkWidget *docs_icon = gtk_image_new_from_icon_name("document-properties", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(docs_button), docs_icon);
    gtk_widget_set_tooltip_text(docs_button, _("Select documents for context"));
    g_signal_connect(G_OBJECT(docs_button), "clicked", G_CALLBACK(on_select_documents_clicked), user_data);

    // Add label and buttons to the top row
    gtk_box_pack_start(GTK_BOX(top_row), input_label, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top_row), stop_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top_row), send_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top_row), clear_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top_row), docs_button, FALSE, FALSE, 0); // Add the new button
    
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
    
    // Create a box for the spinner only
    GtkWidget *spinner_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign(spinner_box, GTK_ALIGN_CENTER);

    // Add the spinner
    GtkWidget *spinner = gtk_spinner_new();
    gtk_widget_set_halign(spinner, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(spinner, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(spinner_box), spinner, FALSE, FALSE, 0);  
    
    // Store reference to spinner
    llm_plugin->spinner = spinner; 

    // Add the spinner box to the main box
    gtk_box_pack_start(GTK_BOX(main_box), spinner_box, FALSE, FALSE, 0);    

    return main_box;
}

static void on_select_documents_clicked(GtkButton *button, gpointer user_data) {
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;
    if (!llm_plugin) {
        return;
    }

    // Create a dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        _("Select Documents for Context"),
        GTK_WINDOW(llm_plugin->geany_data->main_widgets->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        _("OK"), GTK_RESPONSE_ACCEPT,
        _("Cancel"), GTK_RESPONSE_CANCEL,
        NULL);

    // Create list store and view for documents
    GtkListStore *store = gtk_list_store_new(3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    
    // Add a checkbox column
    GtkCellRenderer *toggle_renderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(toggle_renderer, "toggled", G_CALLBACK(on_document_toggled), store);
    
    GtkTreeViewColumn *toggle_column = gtk_tree_view_column_new_with_attributes(
        _("Include"), toggle_renderer, "active", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), toggle_column);
    
    // Add document name column
    GtkCellRenderer *text_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *text_column = gtk_tree_view_column_new_with_attributes(
        _("Document"), text_renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), text_column);
    
    // Populate the list with open documents
    GPtrArray *selected_docs = llm_plugin->selected_document_ids;
    guint i, doc_cnt = llm_plugin->geany_data->documents_array->len;
    
    for (i = 0; i < doc_cnt; i++) {
        GeanyDocument *doc = g_ptr_array_index(llm_plugin->geany_data->documents_array, i);
        if (doc && doc->is_valid) {
            gboolean is_selected = FALSE;
            
            // Check if this document is in the selected list
            if (selected_docs) {
                for (guint j = 0; j < selected_docs->len; j++) {
                    if (doc == g_ptr_array_index(selected_docs, j)) {
                        is_selected = TRUE;
                        break;
                    }
                }
            }
            
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter, 
                0, is_selected,
                1, doc->file_name,
                2, doc,
                -1);
        }
    }
    
    // Add the "include current document automatically" checkbox
    GtkWidget *auto_include = gtk_check_button_new_with_label(_("Automatically include current document"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_include), 
                               llm_plugin->include_current_document);
    
    // Set up scrolled window
    GtkWidget *scrollwin = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrollwin), treeview);
    gtk_widget_set_size_request(scrollwin, 400, 300);
    
    // Add content to the dialog
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_add(GTK_CONTAINER(content_area), scrollwin);
    gtk_container_add(GTK_CONTAINER(content_area), auto_include);
    
    // Show everything
    gtk_widget_show_all(dialog);
    
    // Run the dialog and process result
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        // Clear existing selection
        if (llm_plugin->selected_document_ids) {
            g_ptr_array_free(llm_plugin->selected_document_ids, TRUE);
        }
        
        // Create new selection list
        llm_plugin->selected_document_ids = g_ptr_array_new();
        
        // Update auto-include setting
        llm_plugin->include_current_document = 
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(auto_include));
        
        // Process selected documents
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
        GtkTreeIter iter;
        gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
        
        while (valid) {
            gboolean selected;
            GeanyDocument *doc;
            
            gtk_tree_model_get(model, &iter, 
                0, &selected, 
                2, &doc, 
                -1);
                
            if (selected && doc && doc->is_valid) {
                g_ptr_array_add(llm_plugin->selected_document_ids, doc);
            }
            
            valid = gtk_tree_model_iter_next(model, &iter);
        }
        
        // Update the document button appearance based on selection
        update_document_button_state(llm_plugin);
    }
    
    gtk_widget_destroy(dialog);
}

static void on_document_toggled(GtkCellRendererToggle *cell, gchar *path_str, gpointer data) {
    GtkTreeModel *model = (GtkTreeModel *)data;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    GtkTreeIter iter;
    gboolean active;
    
    // Get current state
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_tree_model_get(model, &iter, 0, &active, -1);
    
    // Toggle the state
    active ^= 1;
    
    // Update the model
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, active, -1);
    
    gtk_tree_path_free(path);
}

static void update_document_button_state(LLMPlugin *plugin) {
    // Find the documents button (assuming it's the 5th child of the top_row)
    GtkWidget *top_row = gtk_widget_get_parent(plugin->input_text_entry);
    if (!GTK_IS_CONTAINER(top_row)) {
        return;
    }
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(top_row));
    GtkWidget *docs_button = NULL;
    
    for (GList *l = children; l != NULL; l = l->next) {
        if (GTK_IS_BUTTON(l->data) && 
            gtk_widget_get_tooltip_text(GTK_WIDGET(l->data)) && 
            g_str_equal(gtk_widget_get_tooltip_text(GTK_WIDGET(l->data)), _("Select documents for context"))) {
            docs_button = GTK_WIDGET(l->data);
            break;
        }
    }
    
    g_list_free(children);
    
    if (!docs_button) {
        return;
    }
    
    // Update tooltip and style based on selection state
    guint selected_count = plugin->selected_document_ids ? plugin->selected_document_ids->len : 0;
    
    if (selected_count > 0) {
        gchar *tooltip = g_strdup_printf(_("Selected documents for context (%u)"), selected_count);
        gtk_widget_set_tooltip_text(docs_button, tooltip);
        g_free(tooltip);
        
        // Add a visual indicator
        GtkStyleContext *context = gtk_widget_get_style_context(docs_button);
        gtk_style_context_add_class(context, "suggested-action");
    } else {
        gtk_widget_set_tooltip_text(docs_button, _("Select documents for context"));
        
        // Remove visual indicator
        GtkStyleContext *context = gtk_widget_get_style_context(docs_button);
        gtk_style_context_remove_class(context, "suggested-action");
    }
}

// Handle document close events
static void on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data)
{
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin || !doc || !plugin->selected_document_ids) {
        return;
    }
    
    // Check if the closed document was in our selection
    gboolean removed = FALSE;
    for (guint i = 0; i < plugin->selected_document_ids->len; i++) {
        if (doc == g_ptr_array_index(plugin->selected_document_ids, i)) {
            g_ptr_array_remove_index(plugin->selected_document_ids, i);
            removed = TRUE;
            break;
        }
    }
    
    // Only update the button if we actually removed something
    if (removed) {
        update_document_button_state(plugin);
    }
}
