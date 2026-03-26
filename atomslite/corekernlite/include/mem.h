/**
 * @file mem.h
 * @brief 内存管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LITE_MEM_H
#define AGENTOS_LITE_MEM_H

#include "export.h"
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化内存子系统
 * @param max_size 最大内存大小（0表示使用默认值）
 * @return 错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mem_init(size_t max_size);

/**
 * @brief 清理内存子系统
 */
AGENTOS_LITE_API void agentos_lite_mem_cleanup(void);

/**
 * @brief 分配内存
 * @param size 分配大小
 * @return 分配的内存指针，失败返回NULL
 */
AGENTOS_LITE_API void* agentos_lite_mem_alloc(size_t size);

/**
 * @brief 释放内存
 * @param ptr 内存指针
 */
AGENTOS_LITE_API void agentos_lite_mem_free(void* ptr);

/**
 * @brief 对齐内存分配
 * @param size 分配大小
 * @param alignment 对齐要求（必须是2的幂）
 * @return 分配的内存指针，失败返回NULL
 */
AGENTOS_LITE_API void* agentos_lite_mem_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief 释放对齐内存
 * @param ptr 内存指针
 */
AGENTOS_LITE_API void agentos_lite_mem_aligned_free(void* ptr);

/**
 * @brief 获取内存统计信息
 * @param[out] total 总内存大小
 * @param[out] used 已使用内存大小
 * @param[out] peak 峰值内存使用量
 */
AGENTOS_LITE_API void agentos_lite_mem_stats(size_t* total, size_t* used, size_t* peak);

/**
 * @brief 检查内存泄漏
 * @return 泄漏的内存块数量
 */
AGENTOS_LITE_API size_t agentos_lite_mem_check_leaks(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_MEM_H */