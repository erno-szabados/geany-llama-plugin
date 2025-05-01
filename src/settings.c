#include "plugin.h"
#include "settings.h"
#include <glib.h>

static gchar* get_config_path()
{
    return  g_build_path(G_DIR_SEPARATOR_S, llm_plugin->geany_data->app->configdir, "plugins", "geanyllm", "geanyllm.conf", NULL);
}

void on_configure_response(GtkDialog *dialog, gint response, gpointer user_data) {
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;
     if (!llm_plugin) {
        return;
    }

    gchar *config_path = get_config_path();
    if (!config_path) {
        g_print("Error getting config path\n");
        return;
    }

    if (g_mkdir_with_parents(g_path_get_dirname(config_path), 0755) != 0) {
        g_print("Error creating config directory: %s\n", g_strerror(errno));
        g_free(config_path);
        return;
    }

    g_print("Reading config values:\n");
    const gchar *url = gtk_entry_get_text(GTK_ENTRY(llm_plugin->url_entry));
    g_free(llm_plugin->llm_server_url);
    llm_plugin->llm_server_url = g_strdup(url);

    const gchar *model = gtk_entry_get_text(GTK_ENTRY(llm_plugin->model_entry));
    g_free(llm_plugin->llm_args->model);
    llm_plugin->llm_args->model = g_strdup(model);

    GError *error = NULL;
    GKeyFile *key_file = g_key_file_new();
    g_key_file_set_string(key_file, "General", LLM_SERVER_URL_KEY, llm_plugin->llm_server_url);
    g_key_file_set_string(key_file, "General", LLM_ARGS_MODEL_KEY, llm_plugin->llm_args->model);


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
    LLMPlugin *llm_plugin = (LLMPlugin *)user_data;

    // Validate
    if (!llm_plugin ||
        !llm_plugin->geany_data ||
        !llm_plugin->geany_data->app ||
        !llm_plugin->geany_data->app->configdir) {
        return;
    }

    gchar *config_path = get_config_path();
    if (!config_path) {
        g_print("Error getting config path\n");
        return;
    }

    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;

    // Load keyfile
    if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &error)) {
        g_print("Could not load plugin config %s: %s\n",  config_path, error->message);
        g_error_free(error);
        error = NULL;
        return;
    }

    // Read settings
    llm_plugin->llm_server_url = g_key_file_get_string(key_file, "General", LLM_SERVER_URL_KEY, &error);
    if (!llm_plugin->llm_server_url) {
        g_print("Error reading %s: %s\n", LLM_SERVER_URL_KEY, error->message);
        g_error_free(error);
        error = NULL;
    }

    llm_plugin->llm_args->model = g_key_file_get_string(key_file, "General", LLM_ARGS_MODEL_KEY, &error);
    if (!llm_plugin->llm_args->model) {
        g_print("Error reading %s: %s\n", LLM_ARGS_MODEL_KEY, error->message);
        g_error_free(error);
        error = NULL;
    }
}
