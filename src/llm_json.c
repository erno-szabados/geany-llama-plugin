#include <glib.h>
#include <json-c/json.h> 

#include "llm_json.h"
#include "llm_util.h"


/// @brief Construct the JSON request payload using json-c for the completion endpoint
gchar* llm_construct_completion_json_payload(const gchar* query, const gchar *current_document, const LLMArgs* args) {
    // Create the root object
    struct json_object *root = json_object_new_object();
    
    // Add model field
    json_object_object_add(root, "model", 
        json_object_new_string(args->model));
    
    // Construct the full prompt
    GString *full_prompt = g_string_new("Analyze the following document:\n\n");
    g_string_append(full_prompt, current_document);
    g_string_append(full_prompt, "\n\nBased on the document, give a concise answer the following question:\n");
    g_string_append(full_prompt, query);
    
    // Add prompt field
    json_object_object_add(root, "prompt", 
        json_object_new_string(full_prompt->str));
    
    // Add max_tokens field
    json_object_object_add(root, "max_tokens", 
        json_object_new_int(args->max_tokens));
    
    // Add temperature field
    json_object_object_add(root, "temperature", 
        json_object_new_double(args->temperature));
    
    // Add stream field (TRUE for streaming tokens)
    json_object_object_add(root, "stream", 
        json_object_new_boolean(TRUE));
    
    // Convert the JSON object to a string
    const char *json_string = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    gchar *json_payload = g_strdup(json_string);
    
    // Clean up
    json_object_put(root);  // Decrements refcount and frees when zero
    g_string_free(full_prompt, TRUE);
    
    return json_payload;
}


/// @brief populate LLMResponse from raw JSON data
gboolean llm_json_to_response(LLMResponse *response, GString *response_buffer, GError **error)
{
    if (!response)
    {
        g_warning("NULL response received!\n");
        return FALSE;
    }
    memset(response, 0, sizeof(LLMResponse));

    if (!response_buffer || !response_buffer->str || response_buffer->str[0] == '\0')
    {
        g_warning("Invalid or empty response buffer received!\n");
        return FALSE;
    }

    // Parse the JSON response
    struct json_object *root = NULL;
    enum json_tokener_error jerr = json_tokener_success;
    struct json_tokener *tok = json_tokener_new();
    
    root = json_tokener_parse_ex(tok, response_buffer->str, -1);
    jerr = json_tokener_get_error(tok);
    
    if (jerr != json_tokener_success)
    {
        g_warning("Failed to parse JSON data: %s\n", json_tokener_error_desc(jerr));
        g_warning("Problematic JSON buffer content: %s", response_buffer->str);
        if (error) {
            g_set_error(error, g_quark_from_static_string("JSON Error"), 1, 
                        "JSON parsing failed: %s", json_tokener_error_desc(jerr));
        }
        // Free root object if it was created despite the error
        if (root) {
            json_object_put(root);
        }
        json_tokener_free(tok);
        return FALSE;
    }
    
    // Check if root is null (shouldn't happen if jerr is success, but being defensive)
    if (!root) {
        g_warning("Failed to parse JSON data: root object is NULL\n");
        if (error) {
            g_set_error(error, g_quark_from_static_string("JSON Error"), 1, 
                        "JSON parsing failed: root object is NULL");
        }
        json_tokener_free(tok);
        return FALSE;
    }
    
    json_tokener_free(tok);
    
    // Ensure root is an object
    if (!json_object_is_type(root, json_type_object))
    {
        g_set_error(error, g_quark_from_static_string("JSON Error"), 1, 
                    "Root is not a JSON object");
        json_object_put(root);
        return FALSE;
    }

    // Check for top-level error first (common in some APIs)
    struct json_object *error_obj = NULL;
    if (json_object_object_get_ex(root, "error", &error_obj))
    {
        if (json_object_is_type(error_obj, json_type_string))
        {
            response->error = g_strdup(json_object_get_string(error_obj));
        }
        else if (json_object_is_type(error_obj, json_type_object))
        {
            // Try to extract a message field if the error is an object
            struct json_object *message_obj = NULL;
            if (json_object_object_get_ex(error_obj, "message", &message_obj) && 
                json_object_is_type(message_obj, json_type_string))
            {
                response->error = g_strdup(json_object_get_string(message_obj));
            }
            else
            {
                // Fallback: stringify the error object (might be verbose)
                response->error = g_strdup(json_object_to_json_string_ext(error_obj, JSON_C_TO_STRING_PRETTY));
            }
        }
        // If there's an error, we might not have choices, so don't make it fatal yet
    }

    // Extract text from choices if available
    struct json_object *choices_obj = NULL;
    if (json_object_object_get_ex(root, "choices", &choices_obj) && 
        json_object_is_type(choices_obj, json_type_array))
    {
        size_t choices_len = json_object_array_length(choices_obj);
        if (choices_len > 0)
        {
            struct json_object *first_choice = json_object_array_get_idx(choices_obj, 0);
            if (json_object_is_type(first_choice, json_type_object))
            {
                // Handle different potential structures (e.g., OpenAI chat vs completion)
                const char *text_content = NULL;
                
                // Completion API style
                struct json_object *text_obj = NULL;
                if (json_object_object_get_ex(first_choice, "text", &text_obj) && 
                    json_object_is_type(text_obj, json_type_string))
                {
                    text_content = json_object_get_string(text_obj);
                }
                // Chat API streaming style (delta)
                else
                {
                    struct json_object *delta_obj = NULL;
                    if (json_object_object_get_ex(first_choice, "delta", &delta_obj) && 
                        json_object_is_type(delta_obj, json_type_object))
                    {
                        struct json_object *content_obj = NULL;
                        if (json_object_object_get_ex(delta_obj, "content", &content_obj) && 
                            json_object_is_type(content_obj, json_type_string))
                        {
                            text_content = json_object_get_string(content_obj);
                        }
                    }
                    // Chat API non-streaming style (message)
                    else
                    {
                        struct json_object *message_obj = NULL;
                        if (json_object_object_get_ex(first_choice, "message", &message_obj) && 
                            json_object_is_type(message_obj, json_type_object))
                        {
                            struct json_object *content_obj = NULL;
                            if (json_object_object_get_ex(message_obj, "content", &content_obj) && 
                                json_object_is_type(content_obj, json_type_string))
                            {
                                text_content = json_object_get_string(content_obj);
                            }
                        }
                    }
                }
                
                if (text_content) {
                    response->response_text = g_strdup(text_content);
                }
            }
        }
    }

    if (response->error)
    {
        g_warning("API returned error: %s\n", response->error);
    }

    json_object_put(root);  // Free the parsed JSON object

    // Return TRUE if we got *something* (text or error), FALSE only on parse failure
    return (response->response_text != NULL || response->error != NULL);
}