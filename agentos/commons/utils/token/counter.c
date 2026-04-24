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
    if (length == 0) return 0;

    agentos_token_config_t default_cfg = AGENTOS_TOKEN_CONFIG_DEFAULT;
    const agentos_token_config_t* cfg = config ? config : &default_cfg;

    size_t word_count = 0;
    size_t cjk_count = 0;
    size_t punct_count = 0;
    bool in_word = false;

    for (size_t i = 0; i < length; ) {
        unsigned char c = (unsigned char)text[i];

        if (c < 0x80) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                if (!in_word) { word_count++; in_word = true; }
            } else {
                in_word = false;
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    /* whitespace */
                } else {
                    punct_count++;
                }
            }
            i++;
        } else if (c < 0xE0) {
            cjk_count++;
            in_word = false;
            i += 2;
        } else if (c < 0xF0) {
            unsigned char c2 = (i + 1 < length) ? (unsigned char)text[i + 1] : 0;
            if ((c & 0x0F) == 0x0E && (c2 & 0xC0) == 0x80) {
                unsigned char c3 = (i + 2 < length) ? (unsigned char)text[i + 2] : 0;
                unsigned int cp = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                if (cp >= 0x4E00 && cp <= 0x9FFF) {
                    cjk_count++;
                } else {
                    word_count++;
                }
            } else {
                cjk_count++;
            }
            in_word = false;
            i += 3;
        } else {
            cjk_count++;
            in_word = false;
            i += 4;
        }
    }

    size_t count = word_count + cjk_count + (punct_count + 1) / 2;

    if (count == 0 && length > 0) {
        count = (length + 3) / 4;
    }

    switch (cfg->model_type) {
        case AGENTOS_TOKEN_MODEL_GPT4: count = (count * 4 + 2) / 3; break;
        case AGENTOS_TOKEN_MODEL_GPT35: count = (count * 5 + 2) / 4; break;
        case AGENTOS_TOKEN_MODEL_CLAUDE: count = (count * 7 + 2) / 5; break;
        case AGENTOS_TOKEN_MODEL_LLAMA: count = (count * 3 + 1) / 2; break;
        default: break;
    }

    return count;
}

static size_t count_tokens_by_model(const char* model_name, const char* text, size_t length) {
    agentos_token_config_t config = AGENTOS_TOKEN_CONFIG_DEFAULT;

    if (model_name) {
        if (strstr(model_name, "gpt-4") || strstr(model_name, "gpt-4o")) {
            config.model_type = AGENTOS_TOKEN_MODEL_GPT4;
        } else if (strstr(model_name, "gpt-35") || strstr(model_name, "gpt-3.5")) {
            config.model_type = AGENTOS_TOKEN_MODEL_GPT35;
        } else if (strstr(model_name, "claude")) {
            config.model_type = AGENTOS_TOKEN_MODEL_CLAUDE;
        } else if (strstr(model_name, "llama") || strstr(model_name, "vicuna") || strstr(model_name, "alpaca")) {
            config.model_type = AGENTOS_TOKEN_MODEL_LLAMA;
        }
    }

    return agentos_token_standard_count(text, length, &config);
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
