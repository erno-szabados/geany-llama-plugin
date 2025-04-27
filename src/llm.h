#ifndef __LLM_H__
#define __LLM_H__

#include "types.h"


// Function to query LLM server
LLMResponse *query_llm(LLMPlugin *plugin, const gchar *query, const LLMArgs *args);

#endif // __LLM_H__

