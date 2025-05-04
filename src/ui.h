#ifndef __UI_H__
#define __UI_H__

#include <gtk/gtk.h>
#include "plugin.h"

/// @brief Create the input part of the plugin window.
GtkWidget *create_llm_input_widget(gpointer user_data);
/// @brief Create the output part of the plugin window.
GtkWidget *create_llm_output_widget();
/// @brief Handle send button click event in a separate thread.
void on_input_send_clicked(GtkButton *button, gpointer user_data);
/// @brief Invoke the same functionality as the send button click
void on_input_enter_activate(GtkEntry *entry, gpointer user_data);
/// @brief Handle stop button click event
void on_stop_generation_clicked(GtkButton *button, gpointer user_data);
/// @brief Handle clear button click event
void on_input_clear_clicked(GtkButton *button, gpointer user_data);
/// @brief  Disable a widget from an idle callback
gboolean disable_widget_idle(gpointer user_data);
/// @brief  Stop the spinner from an idle callback
gboolean stop_spinner_idle(gpointer user_data) ;

#endif // __UI_H__