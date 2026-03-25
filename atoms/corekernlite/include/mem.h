/**
 * @file mem.h
 * @brief AgentOS Lite 内存管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供轻量级的内存管理功能：
 * - 基本的内存分配/释放
 * - 对齐内存分配
 * - 内存统计
 */

#ifndef AGENTOS_LITE_MEM_H
#define AGENTOS_LITE_MEM_H

#include <stddef.h>
#include <stdint.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化内存管理子系统
 * @param heap_size 堆大小（字节），0表示使用系统默认
 * @return 成功返回AGENTOS_LITE_SUCCESS，失败返回错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mem_init(size_t heap_size);

/**
 * @brief 清理内存管理子系统
 */
AGENTOS_LITE_API void agentos_lite_mem_cleanup(void);

/**
 * @brief 分配内存
 * @param size 分配大小（字节）
 * @return 成功返回内存指针，失败返回NULL
 */
AGENTOS_LITE_API void* agentos_lite_mem_alloc(size_t size);

/**
 * @brief 分配对齐内存
 * @param size 分配大小（字节）
 * @param alignment 对齐值（必须是2的幂）
 * @return 成功返回内存指针，失败返回NULL
 */
AGENTOS_LITE_API void* agentos_lite_mem_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief 释放内存
 * @param ptr 内存指针
 */
AGENTOS_LITE_API void agentos_lite_mem_free(void* ptr);

/**
 * @brief 释放对齐内存
 * @param ptr 内存指针
 */
AGENTOS_LITE_API void agentos_lite_mem_aligned_free(void* ptr);

/**
 * @brief 重新分配内存
 * @param ptr 原内存指针
 * @param new_size 新大小（字节）
 * @return 成功返回新内存指针，失败返回NULL
 */
AGENTOS_LITE_API void* agentos_lite_mem_realloc(void* ptr, size_t new_size);

/**
 * @brief 获取内存统计信息
 * @param out_total 输出总内存（字节），可为NULL
 * @param out_used 输出已用内存（字节），可为NULL
 * @param out_peak 输出峰值内存（字节），可为NULL
 */
AGENTOS_LITE_API void agentos_lite_mem_stats(size_t* out_total, size_t* out_used, size_t* out_peak);

/**
 * @brief 检查内存泄漏
 * @return 泄漏的内存块数量
 */
AGENTOS_LITE_API size_t agentos_lite_mem_check_leaks(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_MEM_H */
