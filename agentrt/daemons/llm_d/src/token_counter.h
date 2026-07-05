/**
 * @file token_counter.h
 * @brief Token 计数接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_LLM_TOKEN_COUNTER_H
#define AGENTRT_LLM_TOKEN_COUNTER_H

#include <stddef.h>
#ifdef HAVE_TIKTOKEN
#include <tiktoken.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct token_counter token_counter_t;

token_counter_t *token_counter_create(const char *encoding_name);
void token_counter_destroy(token_counter_t *tc);
size_t token_counter_count(token_counter_t *tc, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_LLM_TOKEN_COUNTER_H */