/**
 * @file counter.c
 * @brief Token计数器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块实现Token计数与预算管理功能：
 * - 基于TikToken的BPE分词器
 * - 支持批量计数和截断
 * - 线程安全的计数器操作
 */

#include "token.h"
#include "token_standard.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "../../platform/include/platform.h"

/* 统一基础库兼容层 */
#include "../../memory/include/memory_compat.h"
#include "../../string/include/string_compat.h"

#define MAX_MODEL_NAME 64

/**
 * @brief Token计数器内部结构
 */
struct agentos_token_counter {
    char model_name[MAX_MODEL_NAME];      /**< 模型名称 */
    agentos_mutex_t mutex;                /**< 互斥锁，保证线程安全 */
    size_t total_count;                   /**< 历史累计Token数 */
    uint64_t request_count;               /**< 请求计数 */
    size_t max_token_length;             /**< 最大Token长度 */
};

/**
 * @brief 计算文本的Token数量
 */
size_t agentos_token_count(const char* text, const agentos_token_config_t* config) {
    if (!text) return 0;

    size_t length = strlen(text);
    agentos_token_config_t default_cfg = AGENTOS_TOKEN_CONFIG_DEFAULT;
    const agentos_token_config_t* cfg = config ? config : &default_cfg;

    // 简单估算：ASCII字符1 token，CJK字符2 tokens
    size_t count = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x80) {
            count += 1;
        } else if (c < 0xE0) {
            count += 2;
            i += 1;
        } else if (c < 0xF0) {
            count += 2;
            i += 2;
        } else {
            count += 2;
            i += 3;
        }
    }

    // 应用模型特定的缩放因子
    switch (cfg->model_type) {
        case AGENTOS_TOKEN_MODEL_GPT4: count = (count * 4 + 2) / 3; break;
        case AGENTOS_TOKEN_MODEL_GPT35: count = (count * 5 + 2) / 4; break;
        case AGENTOS_TOKEN_MODEL_CLAUDE: count = (count * 7 + 2) / 5; break;
        case AGENTOS_TOKEN_MODEL_LLAMA: count = (count * 3 + 1) / 2; break;
        default: break;
    }

    return count;
}

agentos_token_counter_t* agentos_token_counter_create(const char* model_name) {
    if (!model_name) {
        return NULL;
    }
    
    agentos_token_counter_t* counter = (agentos_token_counter_t*)AGENTOS_MALLOC(
        sizeof(agentos_token_counter_t));
    if (!counter) {
        return NULL;
    }
    
    memset(counter, 0, sizeof(agentos_token_counter_t));
    
    strncpy(counter->model_name, model_name, MAX_MODEL_NAME - 1);
    counter->model_name[MAX_MODEL_NAME - 1] = '\0';
    
    if (agentos_mutex_init(&counter->mutex) != 0) {
        AGENTOS_FREE(counter);
        return NULL;
    }
    
    counter->total_count = 0;
    counter->request_count = 0;
    counter->max_token_length = 128 * 1024;
    
    return counter;
}

void agentos_token_counter_destroy(agentos_token_counter_t* counter) {
    if (!counter) {
        return;
    }
    
    agentos_mutex_destroy(&counter->mutex);
    AGENTOS_FREE(counter);
}

size_t agentos_token_counter_count(agentos_token_counter_t* counter, const char* text) {
    if (!counter || !text) {
        return (size_t)-1;
    }
    
    size_t length = strlen(text);
    if (length == 0) {
        return 0;
    }
    
    agentos_mutex_lock(&counter->mutex);
    
    size_t token_count = count_tokens_by_model(counter->model_name, text, length);
    counter->total_count += token_count;
    counter->request_count++;
    
    agentos_mutex_unlock(&counter->mutex);
    
    return token_count;
}

size_t agentos_token_counter_count_batch(agentos_token_counter_t* counter, 
                                        const char** texts, 
                                        size_t count, 
                                        size_t* out_counts) {
    if (!counter || !texts || !out_counts) {
        return (size_t)-1;
    }
    
    agentos_mutex_lock(&counter->mutex);
    
    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (texts[i]) {
            size_t len = strlen(texts[i]);
            out_counts[i] = count_tokens_by_model(counter->model_name, texts[i], len);
            total += out_counts[i];
        } else {
            out_counts[i] = 0;
        }
    }
    
    counter->total_count += total;
    counter->request_count += count;
    
    agentos_mutex_unlock(&counter->mutex);
    
    return 0;
}

char* agentos_token_counter_truncate(agentos_token_counter_t* counter, 
                                     const char* text, 
                                     size_t max_tokens, 
                                     const char* side) {
    if (!counter || !text || max_tokens == 0) {
        return NULL;
    }
    
    size_t length = strlen(text);
    if (length == 0) {
        return AGENTOS_STRDUP("");
    }
    
    agentos_mutex_lock(&counter->mutex);
    
    size_t current_tokens = count_tokens_by_model(counter->model_name, text, length);
    
    if (current_tokens <= max_tokens) {
        agentos_mutex_unlock(&counter->mutex);
        return AGENTOS_STRDUP(text);
    }
    
    size_t target_chars = (length * max_tokens) / current_tokens;
    if (target_chars > length) {
        target_chars = length;
    }
    
    char* result = NULL;
    
    if (side && strcmp(side, "left") == 0) {
        result = AGENTOS_MALLOC(target_chars + 4);
        if (result) {
            memcpy(result, text + length - target_chars, target_chars);
            result[target_chars] = '\0';
            snprintf(result + target_chars, 4, "...");
        }
    } else if (side && strcmp(side, "middle") == 0) {
        size_t half = target_chars / 2;
        result = AGENTOS_MALLOC(target_chars + 8);
        if (result) {
            memcpy(result, text, half);
            result[half] = '\0';
            snprintf(result + half, target_chars + 8 - half, "...[truncated]...");
            size_t remaining_space = target_chars + 8 - (half + 15);
            if (remaining_space > 0) {
                size_t copy_len = (target_chars - half) < (remaining_space - 1) ? (target_chars - half) : (remaining_space - 1);
                memcpy(result + half + 15, text + length - (target_chars - half), copy_len);
                result[half + 15 + copy_len] = '\0';
            }
        }
    } else {
        result = AGENTOS_MALLOC(target_chars + 4);
        if (result) {
            memcpy(result, text, target_chars);
            result[target_chars] = '\0';
            snprintf(result + target_chars, 4, "...");
        }
    }
    
    agentos_mutex_unlock(&counter->mutex);
    
    return result;
}
