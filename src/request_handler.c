#include "request_handler.h"

#include "llm_http.h"
#include "ui.h"

typedef struct {
    LLMPlugin *plugin;
    gchar *msg;
} StatusLabelData;

void on_llm_data_received(const gchar *data_chunk, gpointer user_data) {
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin || !data_chunk || !plugin->output_text_view) {
        return;
    }

    // Run in the main thread via GDK
    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, g_strdup(data_chunk));
}

static gboolean set_status_label_idle(gpointer user_data) {
    StatusLabelData *data = (StatusLabelData *)user_data;
    if (data && data->plugin && data->plugin->status_label) {
        gtk_label_set_text(GTK_LABEL(data->plugin->status_label), data->msg ? data->msg : "");
        gtk_widget_set_visible(data->plugin->status_label, data->msg && *data->msg);
    }
    if (data) g_free(data->msg);
    g_free(data);
    return G_SOURCE_REMOVE;
}

void on_llm_error(const gchar *error_message, gpointer user_data) {
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin || !error_message) {
        return;
    }

    // Update status label in the main thread
    StatusLabelData *data = g_new(StatusLabelData, 1);
    data->plugin = plugin;
    data->msg = g_strdup(error_message);
    gdk_threads_add_idle(set_status_label_idle, data);

    // Reset generation state and stop spinner
    plugin->is_generating = FALSE;
    plugin->active_thread_data = NULL;
    plugin->cancel_requested = FALSE;
    gdk_threads_add_idle_full(G_PRIORITY_HIGH_IDLE, stop_spinner_idle, plugin->spinner, NULL);
    gdk_threads_add_idle_full(G_PRIORITY_HIGH_IDLE, disable_widget_idle, plugin->stop_button, NULL);
}

void on_llm_complete(gpointer user_data) {
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

    // Clear status label on completion
    if (plugin->status_label) {
        gtk_label_set_text(GTK_LABEL(plugin->status_label), "");
        gtk_widget_set_visible(plugin->status_label, FALSE);
    }
}
