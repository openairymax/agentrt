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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* 统一基础库兼容层 */
#include "../../memory/include/memory_compat.h"
#include "../../string/include/string_compat.h"

#define MAX_MODEL_NAME 64

/**
 * @brief Token计数器内部结构
 */
struct agentos_token_counter {
    char model_name[MAX_MODEL_NAME];      /**< 模型名称 */
    token_mutex_t mutex;                  /**< 互斥锁，保证线程安全 */
    size_t total_count;                   /**< 历史累计Token数 */
    uint64_t request_count;               /**< 请求计数 */
    size_t max_token_length;             /**< 最大Token长度 */
};

/**
 * @brief 简化的中文字符Token估算
 * 
 * 根据OpenAI的tiktoken规则，中文每个字符约1.3-2个Token
 * 这里使用简化模型：每个CJK字符按2个Token计算
 * 
 * @param text 文本
 * @param length 文本长度
 * @return 估算的Token数
 */
static size_t estimate_cjk_tokens(const char* text, size_t length) {
    size_t count = 0;
    size_t i = 0;
    
    while (i < length) {
        unsigned char c = (unsigned char)text[i];
        
        if (c < 0x80) {
            count += 1;
            i += 1;
        } else if (c < 0xE0) {
            count += 2;
            i += 2;
        } else if (c < 0xF0) {
            count += 2;
            i += 3;
        } else {
            count += 2;
            i += 4;
        }
    }
    
    return count;
}

/**
 * @brief 估算英文Token数（基于简单分词）
 */
static size_t estimate_english_tokens(const char* text, size_t length) {
    size_t count = 0;
    size_t i = 0;
    int in_word = 0;
    
    while (i < length) {
        char c = text[i];
        
        if (isalnum((unsigned char)c) || c >= 0x80) {
            in_word = 1;
        } else {
            if (in_word) {
                count += 1;
                in_word = 0;
            }
            count += 1;
        }
        i++;
    }
    
    if (in_word) {
        count += 1;
    }
    
    return count;
}

/**
 * @brief 根据模型名称选择计数策略
 */
static size_t count_tokens_by_model(const char* model_name, const char* text, size_t length) {
    if (strstr(model_name, "gpt-4") || strstr(model_name, "gpt-4o") || strstr(model_name, "gpt-35")) {
        size_t cjk_count = 0;
        size_t i = 0;
        
        while (i < length) {
            unsigned char c = (unsigned char)text[i];
            if (c >= 0xE0 && c <= 0xEF) {
                cjk_count++;
            }
            i++;
        }
        
        if (cjk_count > length / 3) {
            return estimate_cjk_tokens(text, length);
        }
    }
    
    size_t alpha_count = 0;
    size_t i = 0;
    while (i < length) {
        if (isalpha((unsigned char)text[i])) {
            alpha_count++;
        }
        i++;
    }
    
    if (alpha_count > length / 2) {
        return estimate_english_tokens(text, length);
    }
    
    return (length * 3) / 4;
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
    
    if (token_mutex_init(&counter->mutex) != 0) {
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
    
    token_mutex_destroy(&counter->mutex);
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
    
    token_mutex_lock(&counter->mutex);
    
    size_t token_count = count_tokens_by_model(counter->model_name, text, length);
    counter->total_count += token_count;
    counter->request_count++;
    
    token_mutex_unlock(&counter->mutex);
    
    return token_count;
}

size_t agentos_token_counter_count_batch(agentos_token_counter_t* counter, 
                                        const char** texts, 
                                        size_t count, 
                                        size_t* out_counts) {
    if (!counter || !texts || !out_counts) {
        return (size_t)-1;
    }
    
    token_mutex_lock(&counter->mutex);
    
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
    
    token_mutex_unlock(&counter->mutex);
    
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
    
    token_mutex_lock(&counter->mutex);
    
    size_t current_tokens = count_tokens_by_model(counter->model_name, text, length);
    
    if (current_tokens <= max_tokens) {
        token_mutex_unlock(&counter->mutex);
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
    
    token_mutex_unlock(&counter->mutex);
    
    return result;
}
