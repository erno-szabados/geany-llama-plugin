#include <glib.h>
#include <json-glib/json-glib.h> // dependency: libjson-glib-dev

#include "llm_json.h"
#include "llm_util.h"


/// @brief Construct the JSON request payload using json-glib for the completion endpoint
gchar* llm_construct_completion_json_payload(const gchar* query, const gchar *current_document, const LLMArgs* args) {
    JsonBuilder* builder = json_builder_new();

    json_builder_begin_object(builder);

    json_builder_set_member_name(builder, "model");
    json_builder_add_string_value(builder, args->model);

    GString *full_prompt = g_string_new("Analyze the following document:\n\n");
    g_string_append(full_prompt, current_document);
    g_string_append(full_prompt, "\n\nBased on the document, give a concise answer the following question:\n");
    g_string_append(full_prompt, query);

    json_builder_set_member_name(builder, "prompt");
    json_builder_add_string_value(builder, full_prompt->str);

    json_builder_set_member_name(builder, "max_tokens");
    json_builder_add_int_value(builder, args->max_tokens);

    json_builder_set_member_name(builder, "temperature");
    json_builder_add_double_value(builder, args->temperature);
    
    // Receiving streaming tokens
    json_builder_set_member_name(builder, "stream");
    json_builder_add_boolean_value(builder, TRUE); 

    json_builder_end_object(builder);

    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);

    gchar* json_payload = json_generator_to_data(generator, NULL);

    g_object_unref(generator);
    json_node_free(root);
    g_object_unref(builder);
    g_string_free(full_prompt, TRUE);

    return json_payload;
}

/// @brief Construct the JSON request payload using json-glib for the chat completion endpoint
gchar* llm_construct_chat_completion_json_payload(const gchar* query, const LLMArgs* args) {
    JsonBuilder* builder = json_builder_new();

    // Begin the root object
    json_builder_begin_object(builder);

    // Add "model" field
    json_builder_set_member_name(builder, "model");
    json_builder_add_string_value(builder, args->model);

    // Add "messages" array
    json_builder_set_member_name(builder, "messages");
    json_builder_begin_array(builder);
    
    // Add "system" message
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "role");
    json_builder_add_string_value(builder, "system");
    json_builder_set_member_name(builder, "content");
    json_builder_add_string_value(builder, args->system_instruction); // A predefined instruction in LLMArgs
    json_builder_end_object(builder);

    // Add "user" message (the query)
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "role");
    json_builder_add_string_value(builder, "user");
    json_builder_set_member_name(builder, "content");
    json_builder_add_string_value(builder, query);
    json_builder_end_object(builder);

    // Add more messages from args->messages (if any)
    for (guint i = 0; i < args->messages_length; ++i) {
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "role");
        json_builder_add_string_value(builder, args->messages[i].role);
        json_builder_set_member_name(builder, "content");
        json_builder_add_string_value(builder, args->messages[i].content);
        json_builder_end_object(builder);
    }

    // End the "messages" array
    json_builder_end_array(builder);

    // Add "max_tokens" field
    json_builder_set_member_name(builder, "max_tokens");
    json_builder_add_int_value(builder, args->max_tokens);

    // Add "temperature" field
    json_builder_set_member_name(builder, "temperature");
    json_builder_add_double_value(builder, args->temperature);
    
    json_builder_set_member_name(builder, "stream");
    json_builder_add_boolean_value(builder, TRUE); 

    // End the root object
    json_builder_end_object(builder);

    // Generate the JSON data
    JsonGenerator* generator = json_generator_new();
    JsonNode* root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);

    gchar* json_payload = json_generator_to_data(generator, NULL);

    // Clean up
    g_object_unref(generator);
    json_node_free(root);
    g_object_unref(builder);

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

    JsonParser *parser = json_parser_new();

    if (!response_buffer || !response_buffer->str || response_buffer->str[0] == '\0')
    {
        g_warning("Invalid or empty response buffer received!\n");
        g_object_unref(parser);
        return FALSE;
    }

    // Let the parser handle potential UTF-8 issues during parsing
    if (!json_parser_load_from_data(parser, response_buffer->str, -1, error))
    {
        g_warning("Failed to parse JSON data: %s\n", (*error) ? (*error)->message : "Unknown error");
        // Log the problematic buffer content for debugging
        g_warning("Problematic JSON buffer content: %s", response_buffer->str);
        g_object_unref(parser);
        return FALSE;
    }

    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root))
    { // Added NULL check for root
        g_set_error(error, g_quark_from_static_string("JSON Error"), 1, "Root is not a JSON object or parsing failed");
        g_object_unref(parser);
        return FALSE;
    }

    JsonObject *root_object = json_node_get_object(root);

    // Check for top-level error first (common in some APIs)
    if (json_object_has_member(root_object, "error"))
    {
        JsonNode *error_node = json_object_get_member(root_object, "error");
        if (JSON_NODE_HOLDS_VALUE(error_node))
        {
            response->error = safe_strdup(json_node_get_string(error_node));
        }
        else if (JSON_NODE_HOLDS_OBJECT(error_node))
        {
            // Try to extract a message field if the error is an object
            JsonObject *error_obj = json_node_get_object(error_node);
            if (json_object_has_member(error_obj, "message"))
            {
                response->error = safe_strdup(json_object_get_string_member(error_obj, "message"));
            }
            else
            {
                // Fallback: stringify the error object (might be verbose)
                JsonGenerator *gen = json_generator_new();
                json_generator_set_root(gen, error_node);
                response->error = json_generator_to_data(gen, NULL);
                g_object_unref(gen);
            }
        }
        // If there's an error, we might not have choices, so don't make it fatal yet
    }

    // Extract text from choices if available
    if (json_object_has_member(root_object, "choices"))
    {
        JsonArray *choices = json_object_get_array_member(root_object, "choices");
        if (choices && json_array_get_length(choices) > 0)
        {
            JsonNode *first_choice_node = json_array_get_element(choices, 0);
            if (JSON_NODE_HOLDS_OBJECT(first_choice_node))
            {
                JsonObject *first_choice_obj = json_node_get_object(first_choice_node);

                // Handle different potential structures (e.g., OpenAI chat vs completion)
                const gchar *text_content = NULL;
                if (json_object_has_member(first_choice_obj, "text"))
                { // Completion API style
                    text_content = json_object_get_string_member(first_choice_obj, "text");
                }
                else if (json_object_has_member(first_choice_obj, "delta"))
                { // Chat API streaming style (delta)
                    JsonObject *delta_obj = json_object_get_object_member(first_choice_obj, "delta");
                    if (json_object_has_member(delta_obj, "content"))
                    {
                        text_content = json_object_get_string_member(delta_obj, "content");
                    }
                }
                else if (json_object_has_member(first_choice_obj, "message"))
                { // Chat API non-streaming style (message)
                    JsonObject *message_obj = json_object_get_object_member(first_choice_obj, "message");
                    if (json_object_has_member(message_obj, "content"))
                    {
                        text_content = json_object_get_string_member(message_obj, "content");
                    }
                }
                response->response_text = safe_strdup(text_content);
            }
        }
    }

   if (response->error)
    {
        g_warning("API returned error: %s\n", response->error);
    }

    g_object_unref(parser);

    // Return TRUE if we got *something* (text or error), FALSE only on parse failure
    return (response->response_text != NULL || response->error != NULL);
}