/**
 * @file token_standard.c
 * @brief Token 计算标准化实现 - 统一 C/Python Token 计算算法
 *
 * 实现 token_standard.h 中定义的标准化接口，确保跨语言一致性。
 * 遵循 AgentOS 架构原则（E-3 资源确定性），提供确定性的 Token 计算。
 *
 * @version 0.1.0
 * @date 2026-04-07
 */

#include "token_standard.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"


/**
 * @brief 算法信息字符串
 */
static const char *ALGORITHM_INFO =
    "AgentOS Token Standard v1.0 - Unified Token Counting Algorithm";

/**
 * @brief 从UTF-8多字节序列解码Unicode代码点
 *
 * @param text 文本指针
 * @param i 当前位置
 * @param length 文本长度
 * @param[out] code_point 解码出的Unicode代码点
 * @param[out] bytes_consumed 消耗的字节数
 * @return 0成功，-1失败
 */
static int utf8_decode_code_point(const char *text, size_t i, size_t length, uint32_t *code_point,
                                  size_t *bytes_consumed)
{
    if (!text || !code_point || !bytes_consumed)
        return AGENTOS_EINVAL;
    if (i >= length)
        return AGENTOS_EINVAL;

    unsigned char c = (unsigned char)text[i];

    if (c < 0x80) {
        *code_point = (uint32_t)c;
        *bytes_consumed = 1;
        return 0;
    } else if (c >= 0xC2 && c <= 0xDF && i + 1 < length) {
        unsigned char n1 = (unsigned char)text[i + 1];
        if ((n1 & 0xC0) != 0x80)
            return AGENTOS_EINVAL;
        *code_point = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(n1 & 0x3F);
        *bytes_consumed = 2;
        return 0;
    } else if (c >= 0xE0 && c <= 0xEF && i + 2 < length) {
        unsigned char n1 = (unsigned char)text[i + 1];
        unsigned char n2 = (unsigned char)text[i + 2];
        if ((n1 & 0xC0) != 0x80 || (n2 & 0xC0) != 0x80)
            return AGENTOS_EINVAL;
        if (c == 0xE0 && n1 < 0xA0)
            return AGENTOS_EINVAL;
        if (c == 0xED && n1 > 0x9F)
            return AGENTOS_EINVAL;
        *code_point =
            ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(n1 & 0x3F) << 6) | (uint32_t)(n2 & 0x3F);
        *bytes_consumed = 3;
        return 0;
    } else if (c >= 0xF0 && c <= 0xF4 && i + 3 < length) {
        unsigned char n1 = (unsigned char)text[i + 1];
        unsigned char n2 = (unsigned char)text[i + 2];
        unsigned char n3 = (unsigned char)text[i + 3];
        if ((n1 & 0xC0) != 0x80 || (n2 & 0xC0) != 0x80 || (n3 & 0xC0) != 0x80)
            return AGENTOS_EINVAL;
        if (c == 0xF0 && n1 < 0x90)
            return AGENTOS_EINVAL;
        if (c == 0xF4 && n1 > 0x8F)
            return AGENTOS_EINVAL;
        *code_point = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(n1 & 0x3F) << 12) |
                      ((uint32_t)(n2 & 0x3F) << 6) | (uint32_t)(n3 & 0x3F);
        *bytes_consumed = 4;
        return 0;
    }

    return AGENTOS_EINVAL;
}

/**
 * @brief 检测Unicode代码点是否为CJK字符
 *
 * 使用精确的Unicode编码范围检测，覆盖所有CJK统一表意文字区块：
 * - U+4E00..U+9FFF  CJK Unified Ideographs
 * - U+3400..U+4DBF  CJK Unified Ideographs Extension A
 * - U+20000..U+2A6DF CJK Unified Ideographs Extension B
 * - U+2A700..U+2B73F CJK Unified Ideographs Extension C
 * - U+2B740..U+2B81F CJK Unified Ideographs Extension D
 * - U+2B820..U+2CEAF CJK Unified Ideographs Extension E
 * - U+2CEB0..U+2EBEF CJK Unified Ideographs Extension F
 * - U+30000..U+3134F CJK Unified Ideographs Extension G
 * - U+31350..U+323AF CJK Unified Ideographs Extension H
 * - U+F900..U+FAFF  CJK Compatibility Ideographs
 * - U+2F800..U+2FA1F CJK Compatibility Ideographs Supplement
 * - U+3000..U+303F  CJK Symbols and Punctuation
 * - U+3040..U+309F  Hiragana
 * - U+30A0..U+30FF  Katakana
 * - U+AC00..U+D7AF  Hangul Syllables
 * - U+FF00..U+FFEF  Halfwidth and Fullwidth Forms
 *
 * @param code_point Unicode代码点
 * @return 是CJK相关字符返回1，否则返回0
 */
static int is_cjk_code_point(uint32_t code_point)
{
    if (code_point >= 0x4E00 && code_point <= 0x9FFF)
        return 1;
    if (code_point >= 0x3400 && code_point <= 0x4DBF)
        return 1;
    if (code_point >= 0x20000 && code_point <= 0x2A6DF)
        return 1;
    if (code_point >= 0x2A700 && code_point <= 0x2B73F)
        return 1;
    if (code_point >= 0x2B740 && code_point <= 0x2B81F)
        return 1;
    if (code_point >= 0x2B820 && code_point <= 0x2CEAF)
        return 1;
    if (code_point >= 0x2CEB0 && code_point <= 0x2EBEF)
        return 1;
    if (code_point >= 0x30000 && code_point <= 0x3134F)
        return 1;
    if (code_point >= 0x31350 && code_point <= 0x323AF)
        return 1;
    if (code_point >= 0xF900 && code_point <= 0xFAFF)
        return 1;
    if (code_point >= 0x2F800 && code_point <= 0x2FA1F)
        return 1;
    if (code_point >= 0x3000 && code_point <= 0x303F)
        return 1;
    if (code_point >= 0x3040 && code_point <= 0x309F)
        return 1;
    if (code_point >= 0x30A0 && code_point <= 0x30FF)
        return 1;
    if (code_point >= 0xAC00 && code_point <= 0xD7AF)
        return 1;
    if (code_point >= 0xFF00 && code_point <= 0xFFEF)
        return 1;
    return 0;
}

/**
 * @brief 分析文本语言特征
 */
int agentos_token_analyze_text(const char *text, size_t length, size_t *out_cjk_chars,
                               size_t *out_alpha_chars, size_t *out_total_chars)
{
    if (!text || !out_cjk_chars || !out_alpha_chars || !out_total_chars) {
        return AGENTOS_EINVAL;
    }

    size_t cjk_chars = 0;
    size_t alpha_chars = 0;
    size_t total_chars = 0;

    size_t i = 0;
    while (i < length) {
        uint32_t code_point = 0;
        size_t bytes_consumed = 0;

        if (utf8_decode_code_point(text, i, length, &code_point, &bytes_consumed) == 0) {
            total_chars++;
            if (code_point < 0x80 && isalpha((int)code_point)) {
                alpha_chars++;
            } else if (is_cjk_code_point(code_point)) {
                cjk_chars++;
            }
            i += bytes_consumed;
        } else {
            i++;
        }
    }

    *out_cjk_chars = cjk_chars;
    *out_alpha_chars = alpha_chars;
    *out_total_chars = total_chars;

    return 0;
}

/**
 * @brief 标准化 Token 计算函数
 */
size_t agentos_token_standard_count(const char *text, size_t length,
                                    const agentos_token_config_t *config)
{
    if (!text) {
        return (size_t)-1;
    }

    // 使用默认配置如果未提供
    agentos_token_config_t default_config = AGENTOS_TOKEN_CONFIG_DEFAULT;
    const agentos_token_config_t *cfg = config ? config : &default_config;

    // 验证配置
    if (agentos_token_validate_config(cfg) != 0) {
        return (size_t)-1;
    }

    // 自动计算长度如果未提供
    size_t text_len = length;
    if (text_len == 0) {
        text_len = strlen(text);
    }

    // 分析文本特征
    size_t cjk_chars = 0, alpha_chars = 0, total_chars = 0;
    if (agentos_token_analyze_text(text, text_len, &cjk_chars, &alpha_chars, &total_chars) != 0) {
        return (size_t)-1;
    }

    // 根据配置选择计算策略
    size_t token_count = 0;

    if (cfg->flags & AGENTOS_TOKEN_FLAG_ACCURATE) {
        // 高精度模式：根据模型类型使用不同算法
        switch (cfg->model_type) {
        case AGENTOS_TOKEN_MODEL_GPT4:
        case AGENTOS_TOKEN_MODEL_GPT35:
            // GPT 系列：中文字符 1.5 字符/Token，英文 4 字符/Token
            token_count = (size_t)(cjk_chars / 1.5f + (total_chars - cjk_chars) / 4.0f);
            break;

        case AGENTOS_TOKEN_MODEL_CLAUDE:
            // Claude 系列：统一 3.5 字符/Token
            token_count = (size_t)(total_chars / 3.5f);
            break;

        case AGENTOS_TOKEN_MODEL_LLAMA:
            // LLaMA 系列：中文字符 2 字符/Token，英文 4 字符/Token
            token_count = (size_t)(cjk_chars / 2.0f + (total_chars - cjk_chars) / 4.0f);
            break;

        default:
            // 通用模型：自适应算法
            if (cjk_chars > total_chars * cfg->cjk_ratio) {
                // 主要为中文文本
                token_count = (size_t)(cjk_chars / 1.5f + (total_chars - cjk_chars) / 4.0f);
            } else if (alpha_chars > total_chars * cfg->alpha_ratio) {
                // 主要为英文文本
                token_count = (size_t)(total_chars / 4.0f);
            } else {
                // 混合文本
                token_count = (size_t)(total_chars / 3.0f);
            }
            break;
        }
    } else {
        // 估算模式（默认）：快速估算
        if (cjk_chars > total_chars * cfg->cjk_ratio) {
            // 主要为中文文本：1.5 字符/Token
            token_count = (size_t)(total_chars / 1.5f);
        } else if (alpha_chars > total_chars * cfg->alpha_ratio) {
            // 主要为英文文本：4 字符/Token
            token_count = (size_t)(total_chars / 4.0f);
        } else {
            // 混合文本：3 字符/Token
            token_count = (size_t)(total_chars / 3.0f);
        }
    }

    // 确保至少返回 1 个 Token
    if (token_count == 0 && total_chars > 0) {
        token_count = 1;
    }

    return token_count;
}

/**
 * @brief 批量 Token 计算
 */
int agentos_token_standard_count_batch(const char **texts, const size_t *lengths, size_t count,
                                       size_t *out_counts, const agentos_token_config_t *config)
{
    if (!texts || !out_counts || count == 0) {
        return AGENTOS_EINVAL;
    }

    // 使用默认配置如果未提供
    agentos_token_config_t default_config = AGENTOS_TOKEN_CONFIG_DEFAULT;
    const agentos_token_config_t *cfg = config ? config : &default_config;

    // 验证配置
    if (agentos_token_validate_config(cfg) != 0) {
        return AGENTOS_EINVAL;
    }

    // 逐个计算 Token 数量
    for (size_t i = 0; i < count; i++) {
        if (!texts[i]) {
            out_counts[i] = 0;
            continue;
        }

        size_t length = lengths ? lengths[i] : 0;
        out_counts[i] = agentos_token_standard_count(texts[i], length, cfg);

        // 检查错误
        if (out_counts[i] == (size_t)-1) {
            return AGENTOS_EINVAL;
        }
    }

    return 0;
}

/**
 * @brief 获取 Token 计算算法信息
 */
const char *agentos_token_get_algorithm_info(void)
{
    return ALGORITHM_INFO;
}

/**
 * @brief 验证 Token 计算配置
 */
int agentos_token_validate_config(const agentos_token_config_t *config)
{
    if (!config) {
        return AGENTOS_EINVAL;
    }

    // 检查模型类型有效性
    if (config->model_type < AGENTOS_TOKEN_MODEL_GENERIC ||
        config->model_type > AGENTOS_TOKEN_MODEL_CUSTOM) {
        return AGENTOS_EINVAL;
    }

    // 检查比例阈值有效性
    if (config->cjk_ratio <= 0.0f || config->cjk_ratio >= 1.0f) {
        return AGENTOS_EINVAL;
    }

    if (config->alpha_ratio <= 0.0f || config->alpha_ratio >= 1.0f) {
        return AGENTOS_EINVAL;
    }

    // 检查标志位有效性
    if ((config->flags & AGENTOS_TOKEN_FLAG_ACCURATE) &&
        (config->flags & AGENTOS_TOKEN_FLAG_ESTIMATE)) {
        // 不能同时设置准确和估算标志
        return AGENTOS_EINVAL;
    }

    return 0;
}

/**
 * @brief 设置 Token 计算精度
 */
int agentos_token_set_precision(agentos_token_precision_t precision, agentos_token_config_t *config)
{
    if (!config) {
        return AGENTOS_EINVAL;
    }

    switch (precision) {
    case AGENTOS_TOKEN_PRECISION_LOW:
        config->flags = AGENTOS_TOKEN_FLAG_ESTIMATE;
        config->cjk_ratio = 0.3f;
        config->alpha_ratio = 0.5f;
        break;

    case AGENTOS_TOKEN_PRECISION_MEDIUM:
        config->flags = AGENTOS_TOKEN_FLAG_ESTIMATE;
        config->cjk_ratio = 0.2f;
        config->alpha_ratio = 0.4f;
        break;

    case AGENTOS_TOKEN_PRECISION_HIGH:
        config->flags = AGENTOS_TOKEN_FLAG_ACCURATE;
        config->cjk_ratio = 0.1f;
        config->alpha_ratio = 0.3f;
        break;

    default:
        return AGENTOS_EINVAL;
    }

    return 0;
}

/**
 * @brief 检查资源配额是否足够
 */
int agentos_token_check_quota(const agentos_token_quota_t *quota, size_t requested_tokens,
                              const agentos_token_usage_t *current_usage)
{
    if (!quota) {
        return AGENTOS_EINVAL;
    }

    if (quota->max_tokens_per_request > 0 && requested_tokens > quota->max_tokens_per_request) {
        return 1;
    }

    if (current_usage) {
        if (quota->max_tokens_per_minute > 0 &&
            current_usage->tokens_used_per_minute + requested_tokens >
                quota->max_tokens_per_minute) {
            return 2;
        }

        if (quota->max_tokens_per_hour > 0 &&
            current_usage->tokens_used_per_hour + requested_tokens > quota->max_tokens_per_hour) {
            return 3;
        }

        if (quota->max_tokens_per_day > 0 &&
            current_usage->tokens_used_per_day + requested_tokens > quota->max_tokens_per_day) {
            return 4;
        }

        if (quota->max_requests_per_minute > 0 &&
            current_usage->requests_used_per_minute + 1 > quota->max_requests_per_minute) {
            return 5;
        }

        if (quota->max_requests_per_hour > 0 &&
            current_usage->requests_used_per_hour + 1 > quota->max_requests_per_hour) {
            return 6;
        }

        if (quota->max_requests_per_day > 0 &&
            current_usage->requests_used_per_day + 1 > quota->max_requests_per_day) {
            return 7;
        }
    }

    return 0;
}