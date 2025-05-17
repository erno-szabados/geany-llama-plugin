#include "llm_util.h"


/// @brief Construct server URI from base uri and path
gchar* llm_construct_server_uri_string(const gchar* server_base_uri, const gchar *path)
{
    if (!server_base_uri || !path) {
        g_warning("Invalid server_base_uri or path: server_base_uri=%p, path=%p", 
                 (void*)server_base_uri, (void*)path);
        return NULL;
    }
    
    // Check if the base URI is empty
    if (server_base_uri[0] == '\0') {
        g_warning("Empty server_base_uri provided");
        return NULL;
    }
    
    return g_strjoin(NULL, server_base_uri, path, NULL);
}