/**
 * @file heapstore_memory.h
 * @brief AgentOS 数据分区内存管理数据存储接口
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_heapstore_MEMORY_H
#define AGENTOS_heapstore_MEMORY_H

#include "heapstore.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 内存池记录
 */
typedef struct {
    char pool_id[128];
    char name[256];
    size_t total_size;
    size_t used_size;
    size_t block_size;
    uint32_t block_count;
    uint32_t free_block_count;
    uint64_t created_at;
    char status[32];
} heapstore_memory_pool_t;

/**
 * @brief 内存分配记录
 */
typedef struct {
    char allocation_id[128];
    char pool_id[128];
    size_t size;
    uint64_t address;
    uint64_t allocated_at;
    uint64_t freed_at;
    char status[32];
} heapstore_memory_allocation_t;

/**
 * @brief 初始化内存数据存储
 *
 * @return heapstore_error_t 错误码
 *
 * @ownership 内部管理所有资源
 * @threadsafe 否，不可多线程同时调用
 * @reentrant 否
 *
 * @see heapstore_memory_shutdown()
 */
heapstore_error_t heapstore_memory_init(void);

/**
 * @brief 关闭内存数据存储
 *
 * @ownership 内部释放所有资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see heapstore_memory_init()
 */
void heapstore_memory_shutdown(void);

/**
 * @brief 记录内存池信息
 *
 * @param pool [in] 内存池信息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 pool 的生命周期
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_memory_record_pool(const heapstore_memory_pool_t* pool);

/**
 * @brief 获取内存池信息
 *
 * @param pool_id [in] 内存池 ID
 * @param pool [out] 输出内存池信息
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 pool 的分配和释放
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_memory_get_pool(const char* pool_id, heapstore_memory_pool_t* pool);

/**
 * @brief 更新内存池使用情况
 *
 * @param pool_id [in] 内存池 ID
 * @param used_size [in] 当前使用大小
 * @param free_block_count [in] 空闲块数量
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_memory_update_pool_usage(const char* pool_id, size_t used_size, uint32_t free_block_count);

/**
 * @brief 记录内存分配
 *
 * @param allocation [in] 分配记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 allocation 的生命周期
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_memory_record_allocation(const heapstore_memory_allocation_t* allocation);

/**
 * @brief 获取内存分配记录
 *
 * @param allocation_id [in] 分配 ID
 * @param allocation [out] 输出分配记录
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责 allocation 的分配和释放
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_memory_get_allocation(const char* allocation_id, heapstore_memory_allocation_t* allocation);

/**
 * @brief 更新分配状态（释放）
 *
 * @param allocation_id [in] 分配 ID
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 是
 * @reentrant 否
 */
heapstore_error_t heapstore_memory_free_allocation(const char* allocation_id);

/**
 * @brief 获取内存存储统计信息
 *
 * @param pool_count [out] 输出内存池数量
 * @param total_allocations [out] 输出总分配次数
 * @param total_size [out] 输出总大小
 * @return heapstore_error_t 错误码
 *
 * @ownership 调用者负责所有输出参数的分配和释放
 * @threadsafe 是
 * @reentrant 是
 */
heapstore_error_t heapstore_memory_get_stats(uint32_t* pool_count, uint32_t* total_allocations, uint64_t* total_size);

/**
 * @brief 检查内存系统是否健康
 *
 * @return bool 健康返回 true
 *
 * @threadsafe 是
 * @reentrant 是
 */
bool heapstore_memory_is_healthy(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_MEMORY_H */
