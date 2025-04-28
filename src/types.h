#ifndef __TYPES_H__
#define __TYPES_H__

/**
 * Shared plugin types.
 */



typedef struct {
    const gchar* role;    // "user", "assistant", "system"
    const gchar* content; // The message content
} ChatMessage;

/// @brief LLM arguments descriptor
typedef struct {
    gchar* model;
    guint max_tokens;
    gdouble temperature;
    const gchar* system_instruction; // E.g., "You are a helpful assistant."
    ChatMessage* messages;           // Array of previous messages
    guint messages_length;           // Number of messages
} LLMArgs;


/// @brief LLM response descriptor
typedef struct {
    gchar *response_text;
    gchar *error;
    gchar *raw_json;
} LLMResponse;

/// @brief Plugin data descriptor
typedef struct
{
    GeanyPlugin *geany_plugin;
    GeanyData   *geany_data;

    // Add your plugin's UI elements and data here
    GtkWidget *llm_panel; // pointer to LLM chat panel
    
    GtkWidget *input_widget; // Widget for user input
    GtkWidget *output_widget; // Widget for LLM output
    GtkWidget *input_text_entry; // User text view (entry for now)
    GtkWidget *output_text_view; // Output text area
    GtkWidget *spinner; // LLM interaction indicator
    
    GtkWidget *url_entry; // Entry for the LLM server URL
    GtkWidget *model_entry; // Entry for the LLM model
    
    gint page_number; // Tabindex
    
    // Plugin settings
     gchar *llm_server_url;

    // LLM arguments
    LLMArgs *llm_args; // LLM parameters (model, temp, max_token)
} LLMPlugin;

// Data structure to pass to the worker thread
typedef struct {
    LLMPlugin *llm_plugin;
    gchar *query;
} ThreadData;

#endif // __TYPES_H__
