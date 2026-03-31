/**
 * @file sanitizer_rules.h
 * @brief 净化规则管理内部接口
 * @author Spharx
 * @date 2024
 */

#ifndef CUPOLAS_SANITIZER_RULES_H
#define CUPOLAS_SANITIZER_RULES_H

#include "../platform/platform.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sanitizer_rules sanitizer_rules_t;

/**
 * @brief 创建规则管理器
 * @param rules_path 规则文件路径
 * @return 规则管理器句柄
 */
sanitizer_rules_t* sanitizer_rules_create(const char* rules_path);

/**
 * @brief 销毁规则管理器
 * @param rules 规则管理器句柄
 */
void sanitizer_rules_destroy(sanitizer_rules_t* rules);

/**
 * @brief 添加规则
 * @param rules 规则管理器句柄
 * @param pattern 匹配模式
 * @param replacement 替换字符串
 * @return 0 成功，其他失败
 */
int sanitizer_rules_add(sanitizer_rules_t* rules, const char* pattern, const char* replacement);

/**
 * @brief 应用规则
 * @param rules 规则管理器句柄
 * @param input 输入字符串
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 0 成功，其他失败
 */
int sanitizer_rules_apply(sanitizer_rules_t* rules, const char* input, char* output, size_t output_size);

/**
 * @brief 清除所有规则
 * @param rules 规则管理器句柄
 */
void sanitizer_rules_clear(sanitizer_rules_t* rules);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_SANITIZER_RULES_H */
