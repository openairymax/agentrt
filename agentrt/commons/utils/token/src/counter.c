/**
 * @file counter.c
 * @brief Token计数器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现Token计数与预算管理功能：
 * - 基于字符级启发式近似（word/CJK/punctuation 分词）
 * - 支持按模型类型调整系数（GPT-4/GPT-3.5/Claude/Llama）
 * - 支持批量计数和截断
 * - 线程安全的计数器操作
 *
 * @note 本实现使用轻量级字符启发式算法，非完整BPE编码器。
 *       对于生产环境高精度需求，建议集成TikToken或等效库。
 */

#include "../../platform/include/platform.h"
#include "token.h"
#include "token_standard.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 统一基础库兼容层 */
#include "../../memory/include/memory_compat.h"
#include "../../string/include/string_compat.h"
#include "error.h"

#define MAX_MODEL_NAME 64

/**
 * @brief Token计数器内部结构
 */
struct agentrt_token_counter {
    char model_name[MAX_MODEL_NAME]; /**< 模型名称 */
    agentrt_mutex_t mutex;           /**< 互斥锁，保证线程安全 */
    size_t total_count;              /**< 历史累计Token数 */
    uint64_t request_count;          /**< 请求计数 */
    size_t max_token_length;         /**< 最大Token长度 */
};

/**
 * @brief 计算文本的Token数量
 */
size_t agentrt_token_count(const char *text, const agentrt_token_config_t *config)
{
    if (!text)
        return 0;

    size_t length = strlen(text);
    if (length == 0)
        return 0;

    agentrt_token_config_t default_cfg = AGENTRT_TOKEN_CONFIG_DEFAULT;
    const agentrt_token_config_t *cfg = config ? config : &default_cfg;

    size_t word_count = 0;
    size_t cjk_count = 0;
    size_t punct_count = 0;
    bool in_word = false;

    for (size_t i = 0; i < length;) {
        unsigned char c = (unsigned char)text[i];

        if (c < 0x80) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_') {
                if (!in_word) {
                    word_count++;
                    in_word = true;
                }
            } else {
                in_word = false;
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    /* whitespace */
                } else {
                    punct_count++;
                }
            }
            i++;
        } else {
            uint32_t code_point = 0;
            size_t bytes_consumed = 0;

            if (c >= 0xC2 && c <= 0xDF && i + 1 < length) {
                unsigned char n1 = (unsigned char)text[i + 1];
                if ((n1 & 0xC0) == 0x80) {
                    code_point = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(n1 & 0x3F);
                    bytes_consumed = 2;
                }
            } else if (c >= 0xE0 && c <= 0xEF && i + 2 < length) {
                unsigned char n1 = (unsigned char)text[i + 1];
                unsigned char n2 = (unsigned char)text[i + 2];
                if ((n1 & 0xC0) == 0x80 && (n2 & 0xC0) == 0x80) {
                    if (c == 0xE0 && n1 < 0xA0) {
                        i++;
                        continue;
                    }
                    if (c == 0xED && n1 > 0x9F) {
                        i++;
                        continue;
                    }
                    code_point = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(n1 & 0x3F) << 6) |
                                 (uint32_t)(n2 & 0x3F);
                    bytes_consumed = 3;
                }
            } else if (c >= 0xF0 && c <= 0xF4 && i + 3 < length) {
                unsigned char n1 = (unsigned char)text[i + 1];
                unsigned char n2 = (unsigned char)text[i + 2];
                unsigned char n3 = (unsigned char)text[i + 3];
                if ((n1 & 0xC0) == 0x80 && (n2 & 0xC0) == 0x80 && (n3 & 0xC0) == 0x80) {
                    if (c == 0xF0 && n1 < 0x90) {
                        i++;
                        continue;
                    }
                    if (c == 0xF4 && n1 > 0x8F) {
                        i++;
                        continue;
                    }
                    code_point = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(n1 & 0x3F) << 12) |
                                 ((uint32_t)(n2 & 0x3F) << 6) | (uint32_t)(n3 & 0x3F);
                    bytes_consumed = 4;
                }
            }

            if (bytes_consumed > 0) {
                int is_cjk = 0;
                if (code_point >= 0x4E00 && code_point <= 0x9FFF)
                    is_cjk = 1;
                else if (code_point >= 0x3400 && code_point <= 0x4DBF)
                    is_cjk = 1;
                else if (code_point >= 0x20000 && code_point <= 0x2A6DF)
                    is_cjk = 1;
                else if (code_point >= 0x2A700 && code_point <= 0x2B73F)
                    is_cjk = 1;
                else if (code_point >= 0x2B740 && code_point <= 0x2B81F)
                    is_cjk = 1;
                else if (code_point >= 0x2B820 && code_point <= 0x2CEAF)
                    is_cjk = 1;
                else if (code_point >= 0x2CEB0 && code_point <= 0x2EBEF)
                    is_cjk = 1;
                else if (code_point >= 0x30000 && code_point <= 0x3134F)
                    is_cjk = 1;
                else if (code_point >= 0x31350 && code_point <= 0x323AF)
                    is_cjk = 1;
                else if (code_point >= 0xF900 && code_point <= 0xFAFF)
                    is_cjk = 1;
                else if (code_point >= 0x2F800 && code_point <= 0x2FA1F)
                    is_cjk = 1;
                else if (code_point >= 0x3000 && code_point <= 0x303F)
                    is_cjk = 1;
                else if (code_point >= 0x3040 && code_point <= 0x309F)
                    is_cjk = 1;
                else if (code_point >= 0x30A0 && code_point <= 0x30FF)
                    is_cjk = 1;
                else if (code_point >= 0xAC00 && code_point <= 0xD7AF)
                    is_cjk = 1;
                else if (code_point >= 0xFF00 && code_point <= 0xFFEF)
                    is_cjk = 1;

                if (is_cjk) {
                    cjk_count++;
                } else {
                    word_count++;
                }
                in_word = false;
                i += bytes_consumed;
            } else {
                i++;
            }
        }
    }

    size_t count = word_count + cjk_count + (punct_count + 1) / 2;

    if (count == 0 && length > 0) {
        count = (length + 3) / 4;
    }

    switch (cfg->model_type) {
    case AGENTRT_TOKEN_MODEL_GPT4:
        count = (count * 4 + 2) / 3;
        break;
    case AGENTRT_TOKEN_MODEL_GPT35:
        count = (count * 5 + 2) / 4;
        break;
    case AGENTRT_TOKEN_MODEL_CLAUDE:
        count = (count * 7 + 2) / 5;
        break;
    case AGENTRT_TOKEN_MODEL_LLAMA:
        count = (count * 3 + 1) / 2;
        break;
    default:
        break;
    }

    return count;
}

static size_t count_tokens_by_model(const char *model_name, const char *text, size_t length)
{
    agentrt_token_config_t config = AGENTRT_TOKEN_CONFIG_DEFAULT;

    if (model_name) {
        if (strstr(model_name, "gpt-4") || strstr(model_name, "gpt-4o")) {
            config.model_type = AGENTRT_TOKEN_MODEL_GPT4;
        } else if (strstr(model_name, "gpt-35") || strstr(model_name, "gpt-3.5")) {
            config.model_type = AGENTRT_TOKEN_MODEL_GPT35;
        } else if (strstr(model_name, "claude")) {
            config.model_type = AGENTRT_TOKEN_MODEL_CLAUDE;
        } else if (strstr(model_name, "llama") || strstr(model_name, "vicuna") ||
                   strstr(model_name, "alpaca")) {
            config.model_type = AGENTRT_TOKEN_MODEL_LLAMA;
        }
    }

    return agentrt_token_standard_count(text, length, &config);
}

agentrt_token_counter_t *agentrt_token_counter_create(const char *model_name)
{
    if (!model_name) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    agentrt_token_counter_t *counter =
        (agentrt_token_counter_t *)AGENTRT_MALLOC(sizeof(agentrt_token_counter_t));
    if (!counter) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    AGENTRT_MEMSET(counter, 0, sizeof(agentrt_token_counter_t));

    AGENTRT_STRNCPY_TERM(counter->model_name, model_name, MAX_MODEL_NAME);
    counter->model_name[MAX_MODEL_NAME - 1] = '\0';

    if (agentrt_mutex_init(&counter->mutex) != 0) {
        AGENTRT_FREE(counter);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }

    counter->total_count = 0;
    counter->request_count = 0;
    counter->max_token_length = 128 * 1024;

    return counter;
}

void agentrt_token_counter_destroy(agentrt_token_counter_t *counter)
{
    if (!counter) {
        return;
    }

    agentrt_mutex_destroy(&counter->mutex);
    AGENTRT_FREE(counter);
}

size_t agentrt_token_counter_count(agentrt_token_counter_t *counter, const char *text)
{
    if (!counter || !text) {
        return (size_t)-1;
    }

    size_t length = strlen(text);
    if (length == 0) {
        return 0;
    }

    agentrt_mutex_lock(&counter->mutex);

    size_t token_count = count_tokens_by_model(counter->model_name, text, length);
    counter->total_count += token_count;
    counter->request_count++;

    agentrt_mutex_unlock(&counter->mutex);

    return token_count;
}

size_t agentrt_token_counter_count_batch(agentrt_token_counter_t *counter, const char **texts,
                                         size_t count, size_t *out_counts)
{
    if (!counter || !texts || !out_counts) {
        return (size_t)-1;
    }

    agentrt_mutex_lock(&counter->mutex);

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

    agentrt_mutex_unlock(&counter->mutex);

    return 0;
}

char *agentrt_token_counter_truncate(agentrt_token_counter_t *counter, const char *text,
                                     size_t max_tokens, const char *side)
{
    if (!counter || !text || max_tokens == 0) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }

    size_t length = strlen(text);
    if (length == 0) {
        return AGENTRT_STRDUP("");
    }

    agentrt_mutex_lock(&counter->mutex);

    size_t current_tokens = count_tokens_by_model(counter->model_name, text, length);

    if (current_tokens <= max_tokens) {
        agentrt_mutex_unlock(&counter->mutex);
        return AGENTRT_STRDUP(text);
    }

    size_t target_chars = (length * max_tokens) / current_tokens;
    if (target_chars > length) {
        target_chars = length;
    }

    char *result = NULL;

    if (side && strcmp(side, "left") == 0) {
        result = AGENTRT_MALLOC(target_chars + 4);
        if (result) {
            __builtin_memcpy(result, text + length - target_chars, target_chars);
            result[target_chars] = '\0';
            snprintf(result + target_chars, 4, "...");
        }
    } else if (side && strcmp(side, "middle") == 0) {
        size_t half = target_chars / 2;
        result = AGENTRT_MALLOC(target_chars + 8);
        if (result) {
            __builtin_memcpy(result, text, half);
            result[half] = '\0';
            snprintf(result + half, target_chars + 8 - half, "...[truncated]...");
            size_t remaining_space = target_chars + 8 - (half + 15);
            if (remaining_space > 0) {
                size_t copy_len = (target_chars - half) < (remaining_space - 1)
                                      ? (target_chars - half)
                                      : (remaining_space - 1);
                __builtin_memcpy(result + half + 15, text + length - (target_chars - half), copy_len);
                result[half + 15 + copy_len] = '\0';
            }
        }
    } else {
        result = AGENTRT_MALLOC(target_chars + 4);
        if (result) {
            __builtin_memcpy(result, text, target_chars);
            result[target_chars] = '\0';
            snprintf(result + target_chars, 4, "...");
        }
    }

    agentrt_mutex_unlock(&counter->mutex);

    return result;
}
