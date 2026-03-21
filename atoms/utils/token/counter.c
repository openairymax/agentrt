/**
 * @file counter.c
 * @brief Token计数实现（基于 tiktoken）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "token.h"
#include <tiktoken.h>
#include <stdlib.h>
#include <string.h>

struct agentos_token_counter {
    tiktoken_t* enc;
    char* model_name;
};

agentos_token_counter_t* agentos_token_counter_create(const char* model_name) {
    if (!model_name) return NULL;
    agentos_token_counter_t* counter = (agentos_token_counter_t*)malloc(sizeof(agentos_token_counter_t));
    if (!counter) return NULL;
    counter->model_name = strdup(model_name);
    if (!counter->model_name) {
        free(counter);
        return NULL;
    }
    counter->enc = tiktoken_get_encoding(model_name);
    if (!counter->enc) {
        // 尝试 fallback
        counter->enc = tiktoken_get_encoding("cl100k_base");
        if (!counter->enc) {
            free(counter->model_name);
            free(counter);
            return NULL;
        }
    }
    return counter;
}

void agentos_token_counter_destroy(agentos_token_counter_t* counter) {
    if (!counter) return;
    if (counter->enc) tiktoken_free(counter->enc);
    free(counter->model_name);
    free(counter);
}

size_t agentos_token_counter_count(agentos_token_counter_t* counter, const char* text) {
    if (!counter || !text) return (size_t)-1;
    return tiktoken_count_tokens(counter->enc, text);
}

/**
 * @brief 批量计算多个文本的 token 数量
 * @param counter token 计数器
 * @param texts 文本数组
 * @param count 文本数量
 * @param out_counts 输出 token 数量数组
 * @return 0 成功，-1 失败
 */
size_t agentos_token_counter_count_batch(agentos_token_counter_t* counter, const char** texts, size_t count, size_t* out_counts) {
    if (!counter || !texts || !out_counts) return -1;
    for (size_t i = 0; i < count; i++) {
        out_counts[i] = tiktoken_count_tokens(counter->enc, texts[i]);
    }
    return 0;
}

char* agentos_token_counter_truncate(agentos_token_counter_t* counter, const char* text, size_t max_tokens, const char* side) {
    if (!counter || !text || !side) return NULL;
    int* tokens = NULL;
    int len = tiktoken_encode(counter->enc, text, &tokens);
    if (len < 0) return NULL;
    if ((size_t)len <= max_tokens) {
        free(tokens);
        return strdup(text);
    }
    int new_len = (int)max_tokens;
    int* new_tokens = NULL;
    if (strcmp(side, "left") == 0) {
        new_tokens = tokens + len - new_len;
    } else if (strcmp(side, "right") == 0) {
        new_tokens = tokens;
    } else if (strcmp(side, "middle") == 0) {
        // 保留前后各一半
        int half = new_len / 2;
        int* buf = (int*)malloc(new_len * sizeof(int));
        if (!buf) {
            free(tokens);
            return NULL;
        }
        memcpy(buf, tokens, half * sizeof(int));
        memcpy(buf + half, tokens + len - (new_len - half), (new_len - half) * sizeof(int));
        char* result = tiktoken_decode(counter->enc, buf, new_len);
        free(buf);
        free(tokens);
        return result;
    } else {
        free(tokens);
        return NULL;
    }
    char* result = tiktoken_decode(counter->enc, new_tokens, new_len);
    free(tokens);
    return result;
}