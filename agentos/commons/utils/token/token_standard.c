/**
 * @file token_standard.c
 * @brief Token 计算标准化实现 - 统一 C/Python Token 计算算法
 * 
 * 实现 token_standard.h 中定义的标准化接口，确保跨语言一致性。
 * 遵循 AgentOS 架构原则（E-3 资源确定性），提供确定性的 Token 计算。
 * 
 * @version 1.0.0
 * @date 2026-04-07
 */

#include "token_standard.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/**
 * @brief 算法信息字符串
 */
static const char* ALGORITHM_INFO = 
    "AgentOS Token Standard v1.0 - Unified Token Counting Algorithm";

/**
 * @brief 检测字符是否为中日韩字符（CJK）
 * 
 * 检查 Unicode 代码点是否在中日韩字符范围内。
 * 
 * @param c 字符（UTF-8 编码的第一个字节）
 * @param next 下一个字符（用于多字节字符）
 * @return 是中日韩字符返回 1，否则返回 0
 */
static int is_cjk_char(unsigned char c, unsigned char next) {
    // 基本 CJK 统一表意文字（U+4E00 到 U+9FFF）
    if (c == 0xE4 && next >= 0x80 && next <= 0xBF) {
        return 1;
    }
    // 扩展 A (U+3400 到 U+4DBF)
    if (c == 0xE3 && next >= 0x90 && next <= 0xBF) {
        return 1;
    }
    // 扩展 B-G 等（简化处理）
    return 0;
}

/**
 * @brief 分析文本语言特征
 */
int agentos_token_analyze_text(
    const char* text,
    size_t length,
    size_t* out_cjk_chars,
    size_t* out_alpha_chars,
    size_t* out_total_chars
) {
    if (!text || !out_cjk_chars || !out_alpha_chars || !out_total_chars) {
        return -1;
    }
    
    size_t cjk_chars = 0;
    size_t alpha_chars = 0;
    size_t total_chars = 0;
    
    size_t i = 0;
    while (i < length) {
        unsigned char c = (unsigned char)text[i];
        
        // 处理 ASCII 字符
        if (c < 0x80) {
            total_chars++;
            if (isalpha(c)) {
                alpha_chars++;
            }
            i++;
        }
        // 处理 UTF-8 多字节字符
        else if (c >= 0xC0 && c <= 0xDF && i + 1 < length) {
            // 2 字节字符
            unsigned char next = (unsigned char)text[i + 1];
            if (is_cjk_char(c, next)) {
                cjk_chars++;
            }
            total_chars++;
            i += 2;
        }
        else if (c >= 0xE0 && c <= 0xEF && i + 2 < length) {
            // 3 字节字符（主要 CJK 字符）
            unsigned char next1 __attribute__((unused)) = (unsigned char)text[i + 1];
            unsigned char next2 __attribute__((unused)) = (unsigned char)text[i + 2];
            // 简化检测：3字节 UTF-8 字符很可能是 CJK
            if (c == 0xE4 || c == 0xE5 || c == 0xE6 || c == 0xE7 || 
                c == 0xE8 || c == 0xE9 || c == 0xEA || c == 0xEB || 
                c == 0xEC || c == 0xED || c == 0xEE || c == 0xEF) {
                cjk_chars++;
            }
            total_chars++;
            i += 3;
        }
        else if (c >= 0xF0 && c <= 0xF7 && i + 3 < length) {
            // 4 字节字符
            total_chars++;
            i += 4;
        }
        else {
            // 非法 UTF-8 序列，跳过
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
size_t agentos_token_standard_count(
    const char* text,
    size_t length,
    const agentos_token_config_t* config
) {
    if (!text) {
        return (size_t)-1;
    }
    
    // 使用默认配置如果未提供
    agentos_token_config_t default_config = AGENTOS_TOKEN_CONFIG_DEFAULT;
    const agentos_token_config_t* cfg = config ? config : &default_config;
    
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
int agentos_token_standard_count_batch(
    const char** texts,
    const size_t* lengths,
    size_t count,
    size_t* out_counts,
    const agentos_token_config_t* config
) {
    if (!texts || !out_counts || count == 0) {
        return -1;
    }
    
    // 使用默认配置如果未提供
    agentos_token_config_t default_config = AGENTOS_TOKEN_CONFIG_DEFAULT;
    const agentos_token_config_t* cfg = config ? config : &default_config;
    
    // 验证配置
    if (agentos_token_validate_config(cfg) != 0) {
        return -1;
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
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief 获取 Token 计算算法信息
 */
const char* agentos_token_get_algorithm_info(void) {
    return ALGORITHM_INFO;
}

/**
 * @brief 验证 Token 计算配置
 */
int agentos_token_validate_config(const agentos_token_config_t* config) {
    if (!config) {
        return -1;
    }
    
    // 检查模型类型有效性
    if (config->model_type < AGENTOS_TOKEN_MODEL_GENERIC || 
        config->model_type > AGENTOS_TOKEN_MODEL_CUSTOM) {
        return -1;
    }
    
    // 检查比例阈值有效性
    if (config->cjk_ratio <= 0.0f || config->cjk_ratio >= 1.0f) {
        return -1;
    }
    
    if (config->alpha_ratio <= 0.0f || config->alpha_ratio >= 1.0f) {
        return -1;
    }
    
    // 检查标志位有效性
    if ((config->flags & AGENTOS_TOKEN_FLAG_ACCURATE) && 
        (config->flags & AGENTOS_TOKEN_FLAG_ESTIMATE)) {
        // 不能同时设置准确和估算标志
        return -1;
    }
    
    return 0;
}

/**
 * @brief 设置 Token 计算精度
 */
int agentos_token_set_precision(
    agentos_token_precision_t precision,
    agentos_token_config_t* config
) {
    if (!config) {
        return -1;
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
            return -1;
    }
    
    return 0;
}

/**
 * @brief 检查资源配额是否足够
 */
int agentos_token_check_quota(
    const agentos_token_quota_t* quota,
    size_t requested_tokens,
    const void* current_usage
) {
    if (!quota) {
        return -1;
    }
    
    // 检查单次请求限制
    if (quota->max_tokens_per_request > 0 && 
        requested_tokens > quota->max_tokens_per_request) {
        return 1;  // 超出单次请求限制
    }
    
    // 注意：当前使用情况检查需要外部实现
    // 这里只提供框架，实际使用需要集成监控系统
    (void)current_usage;  // 暂时未使用
    
    return 0;  // 配额足够
}