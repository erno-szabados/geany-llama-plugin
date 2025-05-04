#ifndef __REQUEST_HANDLER_H__
#define __REQUEST_HANDLER_H__

#include <glib.h>
#include <gtk/gtk.h>
#include "plugin.h"

void on_llm_data_received(const gchar *data_chunk, gpointer user_data);
void on_llm_error(const gchar *error_message, gpointer user_data);
void on_llm_complete(gpointer user_data);

#endif // REQUEST_HANDLER_H__