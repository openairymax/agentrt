/**
 * @file token_counter.c
 * @brief Token 计数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "token_counter.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#ifndef HAVE_TIKTOKEN
#include "token_standard.h"
#endif

#ifdef HAVE_TIKTOKEN
#include <tiktoken.h>

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

#else

struct token_counter {
    char* encoding_name;
    agentos_token_config_t config;
};

static agentos_token_model_t encoding_to_model_type(const char* enc) {
    if (!enc) return AGENTOS_TOKEN_MODEL_GPT4;

    if (strstr(enc, "cl100k") || strstr(enc, "o200k"))
        return AGENTOS_TOKEN_MODEL_GPT4;
    if (strstr(enc, "p50k") || strstr(enc, "r50k"))
        return AGENTOS_TOKEN_MODEL_GPT35;
    if (strstr(enc, "claude"))
        return AGENTOS_TOKEN_MODEL_CLAUDE;
    if (strstr(enc, "llama") || strstr(enc, "deepseek"))
        return AGENTOS_TOKEN_MODEL_LLAMA;

    return AGENTOS_TOKEN_MODEL_GENERIC;
}

token_counter_t* token_counter_create(const char* encoding_name) {
    token_counter_t* tc = calloc(1, sizeof(token_counter_t));
    if (!tc) return NULL;

    tc->encoding_name = strdup(encoding_name ? encoding_name : "cl100k_base");

    memset(&tc->config, 0, sizeof(tc->config));
    tc->config.model_type = encoding_to_model_type(encoding_name);
    tc->config.model_name = tc->encoding_name;
    tc->config.cjk_ratio = 0.2f;
    tc->config.alpha_ratio = 0.4f;
    tc->config.flags = AGENTOS_TOKEN_FLAG_ACCURATE;

    SVC_LOG_INFO("token_counter: using heuristic BPE estimation (encoding=%s, model_type=%d)",
                 tc->encoding_name, tc->config.model_type);

    return tc;
}

void token_counter_destroy(token_counter_t* tc) {
    if (!tc) return;
    free(tc->encoding_name);
    free(tc);
}

size_t token_counter_count(token_counter_t* tc, const char* text) {
    if (!tc || !text) return (size_t)-1;
    if (text[0] == '\0') return 0;

    size_t count = agentos_token_standard_count(text, 0, &tc->config);

    if (count == (size_t)-1) {
        SVC_LOG_WARN("token_counter: heuristic count failed, returning estimate");
        size_t len = strlen(text);
        count = len > 0 ? (len / 3 + 1) : 0;
    }

    return count;
}

#endif /* HAVE_TIKTOKEN */
