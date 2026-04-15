/**
 * @file memory.h
 * @brief AgentOS 统一内存管理接口
 *
 * 提供标准化的内存分配、释放和管理功能，包含安全包装器和调试支持。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEMORY_H
#define AGENTOS_MEMORY_H

#include <agentos/compat/stdint.h>
#include <agentos/compat/stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_api 内存管理API
 * @{
 */

/**
 * @brief 内存统计信息结构体
 */
typedef struct {
    uint64_t total_allocated;      /**< 总分配字节数 */
    uint64_t total_freed;          /**< 总释放字节数 */
    uint64_t current_usage;        /**< 当前使用字节数 */
    uint64_t peak_usage;           /**< 峰值使用字节数 */
    uint64_t allocation_count;     /**< 分配次数 */
    uint64_t free_count;           /**< 释放次数 */
} memory_stats_t;

/**
 * @brief 安全内存分配函数
 *
 * @param[in] size 分配大小（字节）
 * @param[in] tag  分配标签（用于调试和跟踪）
 * @return 分配的内存指针，失败返回NULL
 */
void* memory_alloc(size_t size, const char* tag);

/**
 * @brief 安全内存分配函数（清零）
 *
 * @param[in] size 分配大小（字节）
 * @param[in] tag  分配标签
 * @return 分配的内存指针，失败返回NULL
 */
void* memory_calloc(size_t size, const char* tag);

/**
 * @brief 安全内存重分配函数
 *
 * @param[in] ptr      原始指针
 * @param[in] new_size 新大小（字节）
 * @param[in] tag      分配标签
 * @return 重分配的内存指针，失败返回NULL
 */
void* memory_realloc(void* ptr, size_t new_size, const char* tag);

/**
 * @brief 安全内存释放函数
 *
 * @param[in] ptr 要释放的指针
 */
void memory_free(void* ptr);

/**
 * @brief 检查内存泄漏
 *
 * @param[in] dump_to_stderr 是否输出泄漏信息到stderr
 * @return 泄漏的字节数
 */
size_t memory_check_leaks(bool dump_to_stderr);

/**
 * @brief 获取内存统计信息
 *
 * @param[out] stats 统计信息结构体
 * @return 成功返回true，失败返回false
 */
bool memory_get_stats(memory_stats_t* stats);

/**
 * @brief 安全内存分配宏
 */
#define AGENTOS_MALLOC(size) memory_alloc((size), "default_malloc")

/**
 * @brief 安全内存分配（清零）宏
 */
#define AGENTOS_CALLOC(num, size) memory_calloc(((num)*(size)), "default_calloc")

/**
 * @brief 安全内存重分配宏
 */
#define AGENTOS_REALLOC(ptr, new_size) memory_realloc((ptr), (new_size), "default_realloc")

/**
 * @brief 安全内存释放宏
 */
#define AGENTOS_FREE(ptr) memory_free((ptr))

/**
 * @brief 安全字符串复制宏
 */
#define AGENTOS_STRDUP(str) agentos_strdup_internal((str), "default_strdup")

/**
 * @brief 内部字符串复制函数（供宏使用）
 */
static inline char* agentos_strdup_internal(const char* str, const char* tag) {
    if (str == NULL) return NULL;
    
    size_t len = 0;
    const char* p = str;
    while (*p++) len++;
    
    char* new_str = (char*)memory_alloc(len + 1, tag);
    if (new_str == NULL) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        new_str[i] = str[i];
    }
    new_str[len] = '\0';
    return new_str;
}

/** @} */ // end of memory_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_H */