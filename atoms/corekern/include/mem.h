/**
 * @file mem.h
 * @brief 内核内存管理接口定义
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 提供内核级内存管理功能，包括内存分配、池化、泄漏检测等
 */

#ifndef AGENTOS_MEM_H
#define AGENTOS_MEM_H

#include <stddef.h>
#include <stdint.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 内存分配信息结构
 *
 * 用于内存追踪和泄漏检测
 */
typedef struct agentos_mem_alloc_info {
    void* ptr;                          /**< 分配的内存指针 */
    size_t size;                        /**< 分配的内存大小（字节） */
    const char* file;                  /**< 分配所在的源文件 */
    int line;                           /**< 分配所在的行号 */
    struct agentos_mem_alloc_info* next; /**< 下一个分配信息节点 */
} agentos_mem_alloc_info_t;

/**
 * @brief 内存池类型（不透明指针）
 *
 * 内存池用于高性能场景，减少内存分配开销
 */
typedef struct agentos_mem_pool agentos_mem_pool_t;

/**
 * @brief 初始化内存管理系统
 *
 * @param heap_size [in] 堆大小限制，0 表示使用默认值
 * @return agentos_error_t 错误码
 *
 * @ownership 内部管理所有内存资源
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @note 在使用内存管理 API 前必须调用
 *
 * @see agentos_mem_cleanup()
 */
AGENTOS_API agentos_error_t agentos_mem_init(size_t heap_size);

/**
 * @brief 分配指定大小的内存块
 *
 * @param size [in] 要分配的内存大小（字节）
 * @return void* 指向分配内存的指针，如果分配失败则返回 NULL
 *
 * @ownership 返回的指针由调用者管理，需通过 agentos_mem_free() 释放
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 分配的内存会自动清零
 * @note 失败时返回 NULL，调用者应检查返回值
 *
 * @see agentos_mem_free()
 * @see agentos_mem_alloc_ex()
 */
AGENTOS_API void* agentos_mem_alloc(size_t size);

/**
 * @brief 分配内存（带调试信息）
 *
 * @param size [in] 要分配的内存大小（字节）
 * @param file [in] 源文件名
 * @param line [in] 源文件行号
 * @return void* 指向分配内存的指针
 *
 * @ownership 返回的指针由调用者管理
 * @threadsafe 是
 * @reentrant 是
 *
 * @see agentos_mem_alloc()
 * @see agentos_mem_free()
 */
AGENTOS_API void* agentos_mem_alloc_ex(size_t size, const char* file, int line);

/**
 * @brief 分配对齐的内存块
 *
 * @param size [in] 要分配的内存大小（字节）
 * @param alignment [in] 对齐要求（必须是 2 的幂）
 * @return void* 指向分配内存的指针
 *
 * @ownership 返回的指针由调用者管理
 * @threadsafe 是
 * @reentrant 是
 *
 * @see agentos_mem_aligned_free()
 */
AGENTOS_API void* agentos_mem_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief 分配对齐的内存块（带调试信息）
 *
 * @param size [in] 要分配的内存大小（字节）
 * @param alignment [in] 对齐要求
 * @param file [in] 源文件名
 * @param line [in] 源文件行号
 * @return void* 指向分配内存的指针
 *
 * @ownership 返回的指针由调用者管理
 * @threadsafe 是
 * @reentrant 是
 *
 * @see agentos_mem_aligned_alloc()
 */
AGENTOS_API void* agentos_mem_aligned_alloc_ex(size_t size, size_t alignment, const char* file, int line);

/**
 * @brief 释放之前分配的内存
 *
 * @param ptr [in] 要释放的内存指针，如果为 NULL 则函数不执行任何操作
 *
 * @ownership 释放后指针变为无效
 * @threadsafe 是
 * @reentrant 是
 *
 * @note 此函数是空指针安全的（ptr 为 NULL 时无操作）
 * @note 只能释放通过 agentos_mem_alloc() 或相关函数分配的内存
 * @note 释放后指针变为悬空指针，不应再次使用
 *
 * @see agentos_mem_alloc()
 * @see agentos_mem_alloc_ex()
 */
AGENTOS_API void agentos_mem_free(void* ptr);

/**
 * @brief 释放对齐的内存
 *
 * @param ptr [in] 要释放的内存指针
 *
 * @ownership 释放后指针变为无效
 * @threadsafe 是
 * @reentrant 是
 *
 * @see agentos_mem_aligned_alloc()
 */
AGENTOS_API void agentos_mem_aligned_free(void* ptr);

/**
 * @brief 重新分配内存
 *
 * @param ptr [in] 原内存指针
 * @param new_size [in] 新的内存大小（字节）
 * @return void* 新内存块的指针
 *
 * @ownership 返回的指针由调用者管理
 * @threadsafe 是
 * @reentrant 否
 *
 * @note 如果 ptr 为 NULL，效果等同于 agentos_mem_alloc()
 * @note 如果 new_size 为 0，效果等同于 agentos_mem_free()
 *
 * @see agentos_mem_alloc()
 * @see agentos_mem_free()
 */
AGENTOS_API void* agentos_mem_realloc(void* ptr, size_t new_size);

/**
 * @brief 重新分配内存（带调试信息）
 *
 * @param ptr [in] 原内存指针
 * @param new_size [in] 新的内存大小（字节）
 * @param file [in] 源文件名
 * @param line [in] 源文件行号
 * @return void* 新内存块的指针
 *
 * @ownership 返回的指针由调用者管理
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_mem_realloc()
 */
AGENTOS_API void* agentos_mem_realloc_ex(void* ptr, size_t new_size, const char* file, int line);

/**
 * @brief 创建内存池
 *
 * @param block_size [in] 每个内存块的大小（字节）
 * @param block_count [in] 内存块数量
 * @return agentos_mem_pool_t* 内存池句柄
 *
 * @ownership 返回的句柄由调用者管理，需通过 agentos_mem_pool_destroy() 释放
 * @threadsafe 否，创建后使用是线程安全的
 * @reentrant 否
 *
 * @see agentos_mem_pool_destroy()
 * @see agentos_mem_pool_alloc()
 * @see agentos_mem_pool_free()
 */
AGENTOS_API agentos_mem_pool_t* agentos_mem_pool_create(size_t block_size, uint32_t block_count);

/**
 * @brief 从内存池分配
 *
 * @param pool [in] 内存池句柄
 * @return void* 分配的内存指针
 *
 * @ownership 返回的指针来自池中，无需单独释放
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_mem_pool_create()
 * @see agentos_mem_pool_free()
 */
AGENTOS_API void* agentos_mem_pool_alloc(agentos_mem_pool_t* pool);

/**
 * @brief 释放到内存池
 *
 * @param pool [in] 内存池句柄
 * @param ptr [in] 要释放的内存指针
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_mem_pool_create()
 * @see agentos_mem_pool_alloc()
 */
AGENTOS_API void agentos_mem_pool_free(agentos_mem_pool_t* pool, void* ptr);

/**
 * @brief 销毁内存池
 *
 * @param pool [in] 内存池句柄
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_mem_pool_create()
 */
AGENTOS_API void agentos_mem_pool_destroy(agentos_mem_pool_t* pool);

/**
 * @brief 获取内存使用统计
 *
 * @param out_total [out] 输出总内存大小
 * @param out_used [out] 输出已使用内存大小
 * @param out_peak [out] 输出峰值内存使用
 *
 * @ownership 调用者负责输出参数的分配和释放
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API void agentos_mem_stats(size_t* out_total, size_t* out_used, size_t* out_peak);

/**
 * @brief 检查内存泄漏
 *
 * @return size_t 泄漏的内存块数量
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @note 调用此函数前应确保所有正常分配的内存都已释放
 */
AGENTOS_API size_t agentos_mem_check_leaks(void);

/**
 * @brief 清理内存系统
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_mem_init()
 */
AGENTOS_API void agentos_mem_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEM_H */
