
#include "llm_util.h"

/// @brief  Safely duplicate a string
/// @param str 
/// @return 
gchar *safe_strdup(const gchar *str)
{
    return str ? g_strdup(str) : NULL;
}


/// @brief Construct server URI from base uri and path
gchar* llm_construct_server_uri_string(const gchar* server_base_uri, const gchar *path)
{
    if (!server_base_uri || !path) {
        g_warning("Invalid server_base_uri or path");
        return NULL;
    }
    
    return g_strjoin(NULL, server_base_uri, path, NULL);
}