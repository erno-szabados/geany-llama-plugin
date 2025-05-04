#include "document_manager.h"

void on_select_documents_clicked(GtkButton *button, gpointer user_data) {
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
    gtk_widget_set_size_request(scrollwin, 600, 300);
    
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

void on_document_toggled(GtkCellRendererToggle *cell, gchar *path_str, gpointer data) {
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

void update_document_button_state(LLMPlugin *plugin) {
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
void on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data)
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

/// @brief Get the current document as a string.
/// Remember to g_free(document_content) when done
gchar *get_current_document(gpointer user_data)
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