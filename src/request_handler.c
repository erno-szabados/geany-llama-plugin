#include "request_handler.h"

#include "llm_http.h"
#include "ui.h"

void on_llm_data_received(const gchar *data_chunk, gpointer user_data) {
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin || !data_chunk || !plugin->output_text_view) {
        return;
    }

    // Run in the main thread via GDK
    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, g_strdup(data_chunk));
}

void on_llm_error(const gchar *error_message, gpointer user_data) {
    LLMPlugin *plugin = (LLMPlugin *)user_data;
    if (!plugin || !error_message) {
        return;
    }

    // Format and display the error message
    gchar *formatted_error = g_strdup_printf("Error: %s\n", error_message);
    gdk_threads_add_idle((GSourceFunc)llm_append_to_output_buffer, formatted_error);
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
}
