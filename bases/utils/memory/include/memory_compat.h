/**
 * @file memory_compat.h
 * @brief 统一内存管理模块 - 向后兼容层
 * 
 * 提供与标准C库内存函数兼容的接口，便于现有代码逐步迁移到统一内存管理模块。
 * 包含安全包装器和迁移辅助宏。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEMORY_COMPAT_H
#define AGENTOS_MEMORY_COMPAT_H

#include <stddef.h>
#include "memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_compat_api 内存兼容API
 * @{
 */

/**
 * @brief 安全的内存分配函数（兼容malloc）
 * 
 * @param[in] size 分配大小
 * @return 分配的内存指针，失败返回NULL
 * 
 * @note 使用统一内存管理模块，支持调试和统计
 * @note 默认使用"compat_malloc"标签
 */
static inline void* agentos_malloc(size_t size) {
    return memory_alloc(size, "compat_malloc");
}

/**
 * @brief 安全的内存分配函数（兼容calloc）
 * 
 * @param[in] num 元素数量
 * @param[in] size 元素大小
 * @return 分配的内存指针，失败返回NULL
 * 
 * @note 内存会自动清零
 */
static inline void* agentos_calloc(size_t num, size_t size) {
    return memory_calloc(num * size, "compat_calloc");
}

/**
 * @brief 安全的内存重分配函数（兼容realloc）
 * 
 * @param[in] ptr 原始指针
 * @param[in] new_size 新大小
 * @return 重分配的内存指针，失败返回NULL
 */
static inline void* agentos_realloc(void* ptr, size_t new_size) {
    return memory_realloc(ptr, new_size, "compat_realloc");
}

/**
 * @brief 安全的内存释放函数（兼容free）
 * 
 * @param[in] ptr 要释放的指针
 */
static inline void agentos_free(void* ptr) {
    memory_free(ptr);
}

/**
 * @brief 安全的字符串复制函数（兼容strdup）
 * 
 * @param[in] str 源字符串
 * @return 复制的字符串，失败返回NULL
 */
static inline char* agentos_strdup(const char* str) {
    if (str == NULL) return NULL;
    size_t len = 0;
    const char* p = str;
    while (*p++) len++;
    
    char* new_str = (char*)memory_alloc(len + 1, "compat_strdup");
    if (new_str == NULL) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        new_str[i] = str[i];
    }
    new_str[len] = '\0';
    return new_str;
}

/**
 * @brief 安全的字符串复制函数（兼容strndup）
 * 
 * @param[in] str 源字符串
 * @param[in] n 最大复制长度
 * @return 复制的字符串，失败返回NULL
 */
static inline char* agentos_strndup(const char* str, size_t n) {
    if (str == NULL) return NULL;
    
    size_t len = 0;
    const char* p = str;
    while (len < n && *p) {
        len++;
        p++;
    }
    
    char* new_str = (char*)memory_alloc(len + 1, "compat_strndup");
    if (new_str == NULL) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        new_str[i] = str[i];
    }
    new_str[len] = '\0';
    return new_str;
}

/**
 * @brief 兼容性宏定义
 * 
 * 使用这些宏可以逐步替换现有代码中的标准库调用
 */

/**
 * @def AGENTOS_MALLOC(size)
 * @brief 安全内存分配宏
 */
#define AGENTOS_MALLOC(size) agentos_malloc(size)

/**
 * @def AGENTOS_CALLOC(num, size)
 * @brief 安全内存分配（清零）宏
 */
#define AGENTOS_CALLOC(num, size) agentos_calloc(num, size)

/**
 * @def AGENTOS_REALLOC(ptr, new_size)
 * @brief 安全内存重分配宏
 */
#define AGENTOS_REALLOC(ptr, new_size) agentos_realloc(ptr, new_size)

/**
 * @def AGENTOS_FREE(ptr)
 * @brief 安全内存释放宏
 */
#define AGENTOS_FREE(ptr) agentos_free(ptr)

/**
 * @def AGENTOS_STRDUP(str)
 * @brief 安全字符串复制宏
 */
#define AGENTOS_STRDUP(str) agentos_strdup(str)

/**
 * @def AGENTOS_STRNDUP(str, n)
 * @brief 安全字符串复制（带长度限制）宏
 */
#define AGENTOS_STRNDUP(str, n) agentos_strndup(str, n)

/**
 * @brief 迁移辅助宏：将malloc替换为安全版本
 * 
 * 在代码中可以使用以下模式：
 * 原代码：ptr = malloc(size);
 * 新代码：ptr = AGENTOS_MALLOC(size);
 */
#define MIGRATE_TO_AGENTOS_MALLOC

/**
 * @brief 检查内存泄漏
 * 
 * @param[in] dump_to_stderr 是否输出泄漏信息到stderr
 * @return 泄漏的字节数
 */
static inline size_t agentos_check_memory_leaks(bool dump_to_stderr) {
    return memory_check_leaks(dump_to_stderr);
}

/**
 * @brief 获取内存统计信息
 * 
 * @param[out] stats 统计信息结构体
 * @return 成功返回true，失败返回false
 */
static inline bool agentos_get_memory_stats(memory_stats_t* stats) {
    return memory_get_stats(stats);
}

/** @} */ // end of memory_compat_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_COMPAT_H */