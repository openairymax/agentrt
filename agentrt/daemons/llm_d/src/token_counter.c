#include "memory_compat.h"
#include "error.h"
/**
 * @file token_counter.c
 * @brief Token 计数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "svc_logger.h"
#include "token_counter.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef HAVE_TIKTOKEN
#include "token_standard.h"
#endif

#ifdef HAVE_TIKTOKEN
#include <tiktoken.h>

struct token_counter {
    tiktoken_t *enc;
    char *encoding_name;
};

token_counter_t *token_counter_create(const char *encoding_name)
{
    token_counter_t *tc = AGENTRT_CALLOC(1, sizeof(token_counter_t));
    if (!tc) {
        SVC_LOG_ERROR("C-L02: TOKEN-COUNTER: CREATE-FAIL — OOM allocating struct");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }
    tc->enc = tiktoken_get_encoding(encoding_name);
    if (!tc->enc) {
        SVC_LOG_ERROR("C-L02: TOKEN-COUNTER: CREATE-FAIL — "
                      "tiktoken_get_encoding failed encoding=%s", encoding_name);
        AGENTRT_FREE(tc);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    tc->encoding_name = AGENTRT_STRDUP(encoding_name);
    SVC_LOG_INFO("C-L02: TOKEN-COUNTER: CREATE encoding=%s (tiktoken native)", encoding_name);
    return tc;
}

void token_counter_destroy(token_counter_t *tc)
{
    if (!tc) return;
    SVC_LOG_INFO("C-L02: TOKEN-COUNTER: DESTROY encoding=%s", tc->encoding_name);
    tiktoken_free(tc->enc);
    AGENTRT_FREE(tc->encoding_name);
    AGENTRT_FREE(tc);
}

size_t token_counter_count(token_counter_t *tc, const char *text)
{
    if (!tc || !text)
        return (size_t)-1;
    return tiktoken_count_tokens(tc->enc, text);
}

#else

struct token_counter {
    char *encoding_name;
    agentrt_token_config_t config;
};

static agentrt_token_model_t encoding_to_model_type(const char *enc)
{
    if (!enc)
        return AGENTRT_TOKEN_MODEL_GPT4;

    if (strstr(enc, "cl100k") || strstr(enc, "o200k"))
        return AGENTRT_TOKEN_MODEL_GPT4;
    if (strstr(enc, "p50k") || strstr(enc, "r50k"))
        return AGENTRT_TOKEN_MODEL_GPT35;
    if (strstr(enc, "claude"))
        return AGENTRT_TOKEN_MODEL_CLAUDE;
    if (strstr(enc, "llama") || strstr(enc, "deepseek"))
        return AGENTRT_TOKEN_MODEL_LLAMA;

    return AGENTRT_TOKEN_MODEL_GENERIC;
}

token_counter_t *token_counter_create(const char *encoding_name)
{
    token_counter_t *tc = AGENTRT_CALLOC(1, sizeof(token_counter_t));
    if (!tc) {
        SVC_LOG_ERROR("C-L02: TOKEN-COUNTER: CREATE-FAIL — OOM allocating struct");
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    tc->encoding_name = AGENTRT_STRDUP(encoding_name ? encoding_name : "cl100k_base");

    __builtin_memset(&tc->config, 0, sizeof(tc->config));
    tc->config.model_type = encoding_to_model_type(encoding_name);
    tc->config.model_name = tc->encoding_name;
    tc->config.cjk_ratio = 0.2f;
    tc->config.alpha_ratio = 0.4f;
    tc->config.flags = AGENTRT_TOKEN_FLAG_ACCURATE;

    SVC_LOG_INFO("C-L02: TOKEN-COUNTER: CREATE encoding=%s model_type=%d "
                 "(heuristic BPE estimation fallback)",
                 tc->encoding_name, tc->config.model_type);

    return tc;
}

void token_counter_destroy(token_counter_t *tc)
{
    if (!tc) return;
    SVC_LOG_INFO("C-L02: TOKEN-COUNTER: DESTROY encoding=%s (heuristic)", tc->encoding_name);
    AGENTRT_FREE(tc->encoding_name);
    AGENTRT_FREE(tc);
}

size_t token_counter_count(token_counter_t *tc, const char *text)
{
    if (!tc)
        return (size_t)-1;
    if (!text || text[0] == '\0')
        return 0;

    size_t count = agentrt_token_standard_count(text, 0, &tc->config);

    if (count == (size_t)-1) {
        SVC_LOG_WARN("token_counter: heuristic count failed, returning estimate");
        size_t len = strlen(text);
        count = len > 0 ? (len / 3 + 1) : 0;
    }

    return count;
}

#endif /* HAVE_TIKTOKEN */
