/**
 * @file intent_utils.h
 * @brief 意图解析工具函数接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_INTENT_UTILS_H
#define AGENTOS_INTENT_UTILS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 字符串转换为小写（原地修改）
 * @param str 输入字符串
 * @return 转换后的字符串
 */
char* intent_to_lowercase(char* str);

/**
 * @brief 检查字符串是否包含子串（不区分大小写）
 * @param haystack 源字符串
 * @param needle 子串
 * @return 1 表示包含，0 表示不包含
 */
int intent_contains_ignore_case(const char* haystack, const char* needle);

/**
 * @brief 计算字符串相似度（基于编辑距离）
 * @param s1 字符串 1
 * @param s2 字符串 2
 * @return 相似度得分（0-1）
 */
float intent_string_similarity(const char* s1, const char* s2);

/**
 * @brief 提取关键词从文本中
 * @param text 输入文本
 * @param keywords 关键词数组
 * @param max_keywords 最大关键词数
 * @return 提取的关键词数量
 */
size_t intent_extract_keywords(const char* text, char** keywords, size_t max_keywords);

/**
 * @brief 释放关键词数组
 * @param keywords 关键词数组
 * @param count 关键词数量
 */
void intent_free_keywords(char** keywords, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_INTENT_UTILS_H */
