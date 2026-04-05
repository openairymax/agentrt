/**
 * @file sandbox_quota.h
 * @brief 沙箱资源配额管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_SANDBOX_QUOTA_H
#define AGENTOS_SANDBOX_QUOTA_H

#include "agentos.h"
#include <stdint.h>

/* 前向声明 */
typedef struct agentos_sandbox agentos_sandbox_t;

/**
 * @brief 资源类型枚举
 */
typedef enum {
    RESOURCE_MEMORY = 0,   /**< 内存 */
    RESOURCE_CPU,          /**< CPU 时间 */
    RESOURCE_IO,           /**< I/O 操作 */
    RESOURCE_NETWORK,      /**< 网络访问 */
    RESOURCE_FILE          /**< 文件访问 */
} resource_type_t;

/**
 * @brief 资源配额结构
 */
typedef struct resource_quota {
    uint64_t max_memory_bytes;    /**< 最大内存 */
    uint64_t current_memory;      /**< 当前内存使用 */
    uint64_t max_cpu_time_ms;     /**< 最大 CPU 时间 */
    uint64_t current_cpu_time_ms; /**< 当前 CPU 时间 */
    uint64_t max_io_ops;          /**< 最大 I/O 操作 */
    uint64_t current_io_ops;      /**< 当前 I/O 操作 */
    uint32_t max_file_size;       /**< 最大文件大小（MB） */
    uint32_t max_network_bytes;   /**< 最大网络传输（MB） */
} resource_quota_t;

/**
 * @brief 初始化资源配额
 * @param quota 资源配额结构
 */
void sandbox_quota_init(resource_quota_t* quota);

/**
 * @brief 检查资源配额
 * @param sandbox 沙箱句柄
 * @param resource 资源类型
 * @param amount 请求量
 * @return 1 表示允许，0 表示超限
 */
int sandbox_quota_check(agentos_sandbox_t* sandbox, resource_type_t resource, uint64_t amount);

/**
 * @brief 释放资源
 * @param sandbox 沙箱句柄
 * @param resource 资源类型
 * @param amount 释放量
 */
void sandbox_quota_release(agentos_sandbox_t* sandbox, resource_type_t resource, uint64_t amount);

/**
 * @brief 重置资源配额
 * @param sandbox 沙箱句柄
 */
void sandbox_quota_reset(agentos_sandbox_t* sandbox);

/**
 * @brief 获取资源使用率
 * @param sandbox 沙箱句柄
 * @param resource 资源类型
 * @return 使用率 (0.0-1.0)
 */
double sandbox_quota_get_usage_ratio(agentos_sandbox_t* sandbox, resource_type_t resource);

#endif /* AGENTOS_SANDBOX_QUOTA_H */
