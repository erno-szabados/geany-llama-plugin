#include "ui.h"
#include "llm_http.h"
#include "llm.h"
#include "document_manager.h"
#include "request_handler.h"


/// @brief Create the input part of the plugin window.
GtkWidget *create_llm_input_widget(gpointer user_data) {
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
    gtk_widget_set_tooltip_text(clear_button, _("Clear request and response"));
    g_signal_connect(G_OBJECT(clear_button), "clicked", G_CALLBACK(on_input_clear_clicked), user_data);

    // Create the "Send" button with an icon
    GtkWidget *send_button = gtk_button_new();
    GtkWidget *send_icon = gtk_image_new_from_icon_name("document-send", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(send_button), send_icon);
    gtk_widget_set_tooltip_text(send_button, _("Send request"));
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
GtkWidget *create_llm_output_widget() 
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

/// @brief Handle send button click event in a separate thread.
void on_input_send_clicked(GtkButton *button, gpointer user_data) {
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
void on_input_enter_activate(GtkEntry *entry, gpointer user_data) {
    on_input_send_clicked(GTK_BUTTON(NULL), user_data);
}

/// @brief Handle stop button click event
void on_stop_generation_clicked(GtkButton *button, gpointer user_data)
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

void on_input_clear_clicked(GtkButton *button, gpointer user_data)
{
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;    
    if (!llm_plugin) {
        return;
    }
    g_print("Clear Button was clicked!\n");
    gtk_entry_set_text(GTK_ENTRY(llm_plugin->input_text_entry), "");
      // Get the existing buffer and clear it instead
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(llm_plugin->output_text_view));
    if (buffer) {
        gtk_text_buffer_set_text(buffer, "", -1);
    }
}

// Helper function to disable a widget from an idle callback
gboolean disable_widget_idle(gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    if (widget) {
        gtk_widget_set_sensitive(widget, FALSE);
    }
    // Return FALSE (G_SOURCE_REMOVE) so the callback is only called once
    return G_SOURCE_REMOVE;
}

// Helper function to stop spinner from an idle callback
gboolean stop_spinner_idle(gpointer user_data) {
    GtkSpinner *spinner = GTK_SPINNER(user_data);
    if (spinner) {
        gtk_spinner_stop(spinner);
    }
    // Return FALSE (G_SOURCE_REMOVE) so the callback is only called once
    return G_SOURCE_REMOVE;
}
