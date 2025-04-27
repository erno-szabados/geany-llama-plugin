#include "plugin.h"
#include "settings.h"
#include <glib.h>

void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data) {
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;    
     if (!llm_plugin)
        return;
    
    gchar *config_path = g_build_path(G_DIR_SEPARATOR_S, llm_plugin->geany_data->app->configdir, "plugins", "geanyllm", "geanyllm.conf", NULL);

	if (g_mkdir_with_parents(g_path_get_dirname(config_path), 0755) != 0) {
        g_print("Error creating config directory: %s\n", g_strerror(errno));
        g_free(config_path);
        return;
    }

    const gchar *url = gtk_entry_get_text(GTK_ENTRY(llm_plugin->url_entry));
    g_free(llm_plugin->llm_server_url);
    llm_plugin->llm_server_url = g_strdup(url);

    GError *error = NULL;
    GKeyFile *key_file = g_key_file_new();
    g_key_file_set_string(key_file, "General", LLM_SERVER_URL_KEY, llm_plugin->llm_server_url);
    
     // Save settings to a file
    if (!g_key_file_save_to_file(key_file, config_path, &error)) {
        g_print("Error saving settings: %s\n", error->message);
        g_error_free(error);
    } else {
        g_print("Settings saved successfully.\n");
    }

    g_key_file_free(key_file);
    g_free(config_path);
}


void llm_plugin_settings_load(gpointer user_data)
{
    if (!llm_plugin)
        return;

    gchar *config_path = g_build_path(G_DIR_SEPARATOR_S, llm_plugin->geany_data->app->configdir, "plugins", "geanyllm", "geanyllm.conf", NULL);
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    g_print("Path built");
    
    // Load settings from a file
    if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error)) {
        g_print("Error loading settings from %s: %s\n",  config_path, error->message);
        g_error_free(error);
        error = NULL;
        return;
    } 
    g_print("Settings loaded");
    
    // Read a value
    llm_plugin->llm_server_url = g_key_file_get_string(key_file, "General", LLM_SERVER_URL_KEY, &error);
    if (!llm_plugin->llm_server_url) {
        g_print("Error reading %s: %s\n", LLM_SERVER_URL_KEY, error->message);
        g_error_free(error);
        error = NULL;
    }
    g_print("URL loaded");
    
    if (llm_plugin->url_entry && llm_plugin->llm_server_url)
    {
        gtk_entry_set_text(GTK_ENTRY(llm_plugin->url_entry), llm_plugin->llm_server_url);
    }
    else if (llm_plugin->url_entry && !llm_plugin->llm_server_url)
    {
         gtk_entry_set_text(GTK_ENTRY(llm_plugin->url_entry), "");
         gtk_entry_set_placeholder_text(GTK_ENTRY(llm_plugin->url_entry), "e.g., http://localhost:8080/completion");
    }
}
