#ifndef __LLM_UTIL_H__
#define __LLM_UTIL_H__

#include <glib.h>

gchar *safe_strdup(const gchar *str);

gchar* llm_construct_server_uri_string(const gchar* server_base_uri, const gchar *path);

#endif // __LLM_UTIL_H__