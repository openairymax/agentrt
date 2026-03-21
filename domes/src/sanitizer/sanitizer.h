/**
 * @file sanitizer.h
 * @brief 输入净化器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef DOMAIN_SANITIZER_H
#define DOMAIN_SANITIZER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct sanitizer sanitizer_t;

/**
 * @brief 创建输入净化器
 * @param max_input_len 最大输入长度（超过此长度直接截断）
 * @param rules_path 规则文件路径（JSON格式），若为 NULL 则不加载规则（仅截断）
 * @param cache_capacity 缓存条目数，0表示不缓存
 * @param cache_ttl_ms 缓存 TTL（毫秒），0表示永久
 * @return 净化器句柄，失败返回 NULL
 */
sanitizer_t* sanitizer_create(uint32_t max_input_len,
                               const char* rules_path,
                               size_t cache_capacity,
                               uint32_t cache_ttl_ms);

/**
 * @brief 销毁净化器
 * @param s 净化器句柄
 */
void sanitizer_destroy(sanitizer_t* s);

/**
 * @brief 净化输入字符串
 * @param s 净化器
 * @param input 原始输入
 * @param out_cleaned 输出净化后的字符串（需调用者 free）
 * @param out_risk_level 输出风险等级（0 无风险，1 低，2 中，3 高）
 * @return 0 成功，-1 失败
 */
int sanitizer_clean(sanitizer_t* s, const char* input,
                    char** out_cleaned, int* out_risk_level);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_SANITIZER_H */