/**
 * @file intent_utils.c
 * @brief 意图解析工具函数
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"

/**
 * @brief 字符串转换为小写（原地修改）
 * @param str 输入字符串
 * @return 转换后的字符串
 */
char* intent_to_lowercase(char* str) {
    if (!str) return NULL;
    for (char* p = str; *p; ++p) {
        *p = tolower(*p);
    }
    return str;
}

/**
 * @brief 检查字符串是否包含子串（不区分大小写）
 * @param haystack 源字符串
 * @param needle 子串
 * @return 1 表示包含，0 表示不包含
 */
int intent_contains_ignore_case(const char* haystack, const char* needle) {
    if (!haystack || !needle) return 0;

    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);

    if (needle_len > haystack_len) return 0;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower(haystack[i + j]) != tolower(needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

/**
 * @brief 计算字符串相似度（基于编辑距离）
 * @param s1 字符串 1
 * @param s2 字符串 2
 * @return 相似度得分（0-1）
 */
float intent_string_similarity(const char* s1, const char* s2) {
    if (!s1 || !s2) return 0.0f;

    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);

    // 简单实现：基于公共子串
    int commons = 0;
    for (size_t i = 0; i < len1; i++) {
        for (size_t j = 0; j < len2; j++) {
            size_t k = 0;
            while (i + k < len1 && j + k < len2 &&
                   tolower(s1[i + k]) == tolower(s2[j + k])) {
                k++;
            }
            if (k > commons) commons = k;
        }
    }

    return (float)commons / (float)(len1 > len2 ? len1 : len2);
}

/**
 * @brief 提取关键词从文本中
 * @param text 输入文本
 * @param keywords 关键词数组
 * @param max_keywords 最大关键词数
 * @return 提取的关键词数量
 */
size_t intent_extract_keywords(const char* text, char** keywords, size_t max_keywords) {
    if (!text || !keywords || max_keywords == 0) return 0;

    // 简单实现：按空格分割
    size_t count = 0;
    char* copy = AGENTOS_STRDUP(text);
    if (!copy) return 0;

    char* token = strtok(copy, " ,.!?;:\t\n\r");
    while (token && count < max_keywords) {
        // 过滤短词
        if (strlen(token) > 2) {
            keywords[count] = AGENTOS_STRDUP(token);
            if (keywords[count]) {
                intent_to_lowercase(keywords[count]);
                count++;
            }
        }
        token = strtok(NULL, " ,.!?;:\t\n\r");
    }

    AGENTOS_FREE(copy);
    return count;
}

/**
 * @brief 释放关键词数组
 * @param keywords 关键词数组
 * @param count 关键词数量
 */
void intent_free_keywords(char** keywords, size_t count) {
    if (!keywords) return;
    for (size_t i = 0; i < count; i++) {
        if (keywords[i]) AGENTOS_FREE(keywords[i]);
    }
}
