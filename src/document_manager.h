#ifndef __DOCUMENT_MANAGER_H__
#define __DOCUMENT_MANAGER_H__

#include <gtk/gtk.h>
#include "plugin.h"

void on_select_documents_clicked(GtkButton *button, gpointer user_data);

void on_document_toggled(GtkCellRendererToggle *cell, gchar *path_str, gpointer data);

void update_document_button_state(LLMPlugin *plugin);

void on_document_close(GObject *obj, GeanyDocument *doc, gpointer user_data);

gchar *get_current_document(gpointer user_data);

#endif // __DOCUMENT_MANAGER_H__