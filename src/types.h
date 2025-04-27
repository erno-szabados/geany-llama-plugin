#ifndef __TYPES_H__
#define __TYPES_H__

// Struct for LLM arguments
typedef struct {
    gchar *model;
    gdouble temperature;
    gint max_tokens;
} LLMArgs;

// Struct for LLM response
typedef struct {
    gchar *response_text;
    gchar *error;
    gchar *raw_json;
} LLMResponse;

// Structure to hold your plugin's data
typedef struct
{
    GeanyPlugin *geany_plugin;
    GeanyData   *geany_data;

    // Add your plugin's UI elements and data here
    GtkWidget *llm_panel; // pointer to LLM chat panel
    
    GtkWidget *input_widget; // Widget for user input
    GtkWidget *output_widget; // Widget for LLM output
    GtkWidget *input_text_entry; // User text view (entry)
    
    GtkWidget *url_entry; // Entry for the LLM server URL
    GtkWidget *model_entry; // Entry for the LLM model
    
    gint page_number; // Tabindex
    
    // Plugin settings
     gchar *llm_server_url;

    // LLM arguments
    LLMArgs *llm_args; // LLM parameters (model, temp, max_token)
} LLMPlugin;

#endif // __TYPES_H__
