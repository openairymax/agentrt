/**
 * @file workbench_limits.h
 * @brief 资源限制运行时强制 - 跨平台实现
 * @author Spharx
 * @date 2024
 *
 * 设计原则：
 * - 跨平台支持：Windows Job Objects / Linux cgroups / macOS Mach ports
 * - 运行时强制：超越配置层面的强制执行
 * - 渐进式限制：支持软限制和硬限制
 * - 可观测性：限制触发时记录审计日志
 *
 * 平台实现：
 * - Linux: cgroups v2 (memory, cpu, pids)
 * - Windows: Job Objects with CPU/MSR/HPC limits
 * - macOS: Mach task with resource袋
 */

#ifndef DOMES_WORKBENCH_LIMITS_H
#define DOMES_WORKBENCH_LIMITS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 资源类型 */
typedef enum limit_type {
    LIMIT_TYPE_MEMORY = 0,
    LIMIT_TYPE_CPU_TIME,
    LIMIT_TYPE_CPU_WEIGHT,
    LIMIT_TYPE_PROCESSES,
    LIMIT_TYPE_THREADS,
    LIMIT_TYPE_FILE_SIZE,
    LIMIT_TYPE_FILE_DESCRIPTORS,
    LIMIT_TYPE_NETWORK_BANDWIDTH,
    LIMIT_TYPE_IO_BANDWIDTH
} limit_type_t;

/* 限制模式 */
typedef enum limit_mode {
    LIMIT_MODE_SOFT = 0,
    LIMIT_MODE_HARD,
    LIMIT_MODE_ENFORCED
} limit_mode_t;

/* 限制状态 */
typedef enum limit_status {
    LIMIT_STATUS_OK = 0,
    LIMIT_STATUS_SOFT_EXCEEDED,
    LIMIT_STATUS_HARD_EXCEEDED,
    LIMIT_STATUS_KILLED
} limit_status_t;

/* 资源统计 */
typedef struct resource_stats {
    size_t memory_current;
    size_t memory_peak;
    size_t memory_limit;

    uint64_t cpu_time_ns;
    uint32_t cpu_weight;

    uint32_t processes_current;
    uint32_t processes_limit;

    uint32_t threads_current;
    uint32_t threads_limit;

    size_t file_size_current;
    size_t file_size_limit;

    uint32_t file_descriptors_current;
    uint32_t file_descriptors_limit;
} resource_stats_t;

/* 限制上下文 */
typedef struct limit_context limit_context_t;

/**
 * @brief 创建限制上下文
 * @param memory_limit_bytes 内存限制（字节），0 表示无限制
 * @param cpu_time_limit_ms CPU 时间限制（毫秒），0 表示无限制
 * @param processes_limit 最大进程数，0 表示无限制
 * @return 限制上下文句柄，失败返回 NULL
 */
limit_context_t* limits_create(size_t memory_limit_bytes,
                               uint32_t cpu_time_limit_ms,
                               uint32_t processes_limit);

/**
 * @brief 销毁限制上下文
 * @param ctx 限制上下文句柄
 */
void limits_destroy(limit_context_t* ctx);

/**
 * @brief 将当前进程/线程加入限制上下文
 * @param ctx 限制上下文句柄
 * @return 0 成功，其他失败
 */
int limits_attach(limit_context_t* ctx);

/**
 * @brief 从限制上下文分离
 * @param ctx 限制上下文句柄
 */
void limits_detach(limit_context_t* ctx);

/**
 * @brief 设置内存限制
 * @param ctx 限制上下文句柄
 * @param limit_bytes 内存限制（字节）
 * @param mode 限制模式
 * @return 0 成功，其他失败
 */
int limits_set_memory(limit_context_t* ctx, size_t limit_bytes, limit_mode_t mode);

/**
 * @brief 设置 CPU 时间限制
 * @param ctx 限制上下文句柄
 * @param limit_ms CPU 时间限制（毫秒）
 * @param mode 限制模式
 * @return 0 成功，其他失败
 */
int limits_set_cpu_time(limit_context_t* ctx, uint32_t limit_ms, limit_mode_t mode);

/**
 * @brief 设置 CPU 权重
 * @param ctx 限制上下文句柄
 * @param weight CPU 权重 (1-10000)
 * @param mode 限制模式
 * @return 0 成功，其他失败
 */
int limits_set_cpu_weight(limit_context_t* ctx, uint32_t weight, limit_mode_t mode);

/**
 * @brief 设置进程数限制
 * @param ctx 限制上下文句柄
 * @param limit 最大进程数
 * @param mode 限制模式
 * @return 0 成功，其他失败
 */
int limits_set_processes(limit_context_t* ctx, uint32_t limit, limit_mode_t mode);

/**
 * @brief 设置线程数限制
 * @param ctx 限制上下文句柄
 * @param limit 最大线程数
 * @param mode 限制模式
 * @return 0 成功，其他失败
 */
int limits_set_threads(limit_context_t* ctx, uint32_t limit, limit_mode_t mode);

/**
 * @brief 设置文件大小限制
 * @param ctx 限制上下文句柄
 * @param limit_bytes 文件大小限制（字节）
 * @param mode 限制模式
 * @return 0 成功，其他失败
 */
int limits_set_file_size(limit_context_t* ctx, size_t limit_bytes, limit_mode_t mode);

/**
 * @brief 设置文件描述符限制
 * @param ctx 限制上下文句柄
 * @param limit 最大文件描述符数
 * @param mode 限制模式
 * @return 0 成功，其他失败
 */
int limits_set_file_descriptors(limit_context_t* ctx, uint32_t limit, limit_mode_t mode);

/**
 * @brief 获取资源统计
 * @param ctx 限制上下文句柄
 * @param stats 统计输出
 * @return 0 成功，其他失败
 */
int limits_get_stats(limit_context_t* ctx, resource_stats_t* stats);

/**
 * @brief 检查是否超过限制
 * @param ctx 限制上下文句柄
 * @param type 资源类型
 * @param status 状态输出
 * @return true 超过限制，false 未超过
 */
bool limits_check(limit_context_t* ctx, limit_type_t type, limit_status_t* status);

/**
 * @brief 强制执行限制（杀死超限进程）
 * @param ctx 限制上下文句柄
 * @return 被杀死的进程数
 */
int limits_enforce(limit_context_t* ctx);

/**
 * @brief 获取限制状态描述
 * @param status 限制状态
 * @return 状态描述字符串
 */
const char* limits_status_string(limit_status_t status);

/**
 * @brief 设置限制超出时的回调
 * @param ctx 限制上下文句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 */
typedef void (*limits_exceeded_callback_t)(limit_type_t type, limit_status_t status,
                                           void* user_data);

void limits_set_exceeded_callback(limit_context_t* ctx,
                                  limits_exceeded_callback_t callback,
                                  void* user_data);

/**
 * @brief 检查资源限制是否可用
 * @return true 可用，false 不可用
 */
bool limits_is_available(void);

#ifdef __cplusplus
}
#endif

#endif /* DOMES_WORKBENCH_LIMITS_H */