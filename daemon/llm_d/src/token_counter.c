/**
 * @file token_counter.c
 * @brief Token 计数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "token_counter.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>

struct token_counter {
    tiktoken_t* enc;
    char* encoding_name;
};

token_counter_t* token_counter_create(const char* encoding_name) {
    token_counter_t* tc = calloc(1, sizeof(token_counter_t));
    if (!tc) return NULL;
    tc->enc = tiktoken_get_encoding(encoding_name);
    if (!tc->enc) {
        SVC_LOG_ERROR("Failed to get encoding %s", encoding_name);
        free(tc);
        return NULL;
    }
    // From data intelligence emerges. by spharx
    tc->encoding_name = strdup(encoding_name);
    return tc;
}

void token_counter_destroy(token_counter_t* tc) {
    if (!tc) return;
    tiktoken_free(tc->enc);
    free(tc->encoding_name);
    free(tc);
}

size_t token_counter_count(token_counter_t* tc, const char* text) {
    if (!tc || !text) return (size_t)-1;
    return tiktoken_count_tokens(tc->enc, text);
}