#ifndef __TYPES_H__
#define __TYPES_H__

#define IS_NULL_OR_EMPTY(str) ((str) == NULL || (str)[0] == '\0')

/**
 * Shared plugin types.
 */

typedef void (*LLMDataCallback)(const gchar *data_chunk, gpointer user_data);
typedef void (*LLMErrorCallback)(const gchar *error_message, gpointer user_data);
typedef void (*LLMCompleteCallback)(gpointer user_data);

/// @brief Structure to hold the callbacks
typedef struct {
    LLMDataCallback on_data_received;
    LLMErrorCallback on_error;
    LLMCompleteCallback on_complete;
    gpointer user_data; // Data to be passed to callbacks (e.g., LLMPlugin*)
} LLMCallbacks;

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
} LLMResponse;

/// @brief Forward declaration of ThreadData
typedef struct ThreadData ThreadData;

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
    GtkWidget *stop_button; // Button to stop the LLM generation
    
    GtkWidget *url_entry; // Entry for the LLM server URL
    GtkWidget *proxy_entry;
    GtkWidget *model_entry; // Entry for the LLM model
    
    gint page_number; // Tabindex
    
    // Plugin settings
    gchar *llm_server_url;
    gchar *proxy_url;

    // LLM arguments
    LLMArgs *llm_args; // LLM parameters (model, temp, max_token)

    ThreadData *active_thread_data;
    gboolean is_generating;
    gboolean cancel_requested;

    // Document context management
    GPtrArray *selected_document_ids; // Array of document pointers or IDs
    gboolean include_current_document; // Whether to include the current document automatically
} LLMPlugin;

/// @brief Data structure to pass to the worker thread
typedef struct ThreadData {
    LLMPlugin *llm_plugin;
    gchar *query;
    gchar *current_document;
    LLMCallbacks *callbacks;
    // Pointer to a boolean flag for cancellation 
    gboolean *cancel_flag;  
    // Store the thread handle
    GThread *thread;        
} ThreadData;

/// @brief structure to pass necessary info to write_callback
typedef struct {
    GString *accumulator;
    LLMCallbacks *callbacks;
    gboolean *cancel_flag;  
} WriteCallbackData;


#endif // __TYPES_H__
