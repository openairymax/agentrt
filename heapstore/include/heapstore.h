/**
 * @file heapstore.h
 * @brief AgentOS 数据分区核心接口
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_heapstore_H
#define AGENTOS_heapstore_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 错误码定义
 */
typedef enum {
    heapstore_SUCCESS = 0,
    heapstore_ERR_INVALID_PARAM = -1,
    heapstore_ERR_NOT_INITIALIZED = -2,
    heapstore_ERR_ALREADY_INITIALIZED = -3,
    heapstore_ERR_DIR_CREATE_FAILED = -4,
    heapstore_ERR_DIR_NOT_FOUND = -5,
    heapstore_ERR_PERMISSION_DENIED = -6,
    heapstore_ERR_OUT_OF_MEMORY = -7,
    heapstore_ERR_DB_INIT_FAILED = -8,
    heapstore_ERR_DB_QUERY_FAILED = -9,
    heapstore_ERR_FILE_OPEN_FAILED = -10,
    heapstore_ERR_CONFIG_INVALID = -11,
    heapstore_ERR_NOT_FOUND = -12,
    heapstore_ERR_INTERNAL = -99
} heapstore_error_t;

/**
 * @brief 数据分区路径类型
 */
typedef enum {
    heapstore_PATH_KERNEL,         /* 内核数据路径 */
    heapstore_PATH_LOGS,           /* 日志文件路径 */
    heapstore_PATH_REGISTRY,       /* 注册表路径 */
    heapstore_PATH_SERVICES,       /* 服务数据路径 */
    heapstore_PATH_TRACES,         /* 追踪数据路径 */
    heapstore_PATH_KERNEL_IPC,     /* 内核 IPC 数据 */
    heapstore_PATH_KERNEL_MEMORY,  /* 内核内存数据 */
    heapstore_PATH_MAX
} heapstore_path_type_t;

/**
 * @brief 配置项结构
 */
typedef struct heapstore_config {
    const char* root_path;            /* 数据分区根路径 */
    size_t max_log_size_mb;           /* 最大日志文件大小(MB) */
    int log_retention_days;           /* 日志保留天数 */
    int trace_retention_days;         /* 追踪数据保留天数 */
    bool enable_auto_cleanup;          /* 启用自动清理 */
    bool enable_log_rotation;          /* 启用日志轮转 */
    bool enable_trace_export;          /* 启用追踪导出 */
    int db_vacuum_interval_days;      /* 数据库 Vacuum 间隔(天) */
} heapstore_config_t;

/**
 * @brief 统计信息结构
 */
typedef struct heapstore_stats {
    uint64_t total_disk_usage_bytes;  /* 总磁盘使用量 */
    uint64_t log_usage_bytes;         /* 日志使用量 */
    uint64_t registry_usage_bytes;    /* 注册表使用量 */
    uint64_t trace_usage_bytes;       /* 追踪数据使用量 */
    uint64_t ipc_usage_bytes;         /* IPC 数据使用量 */
    uint64_t memory_usage_bytes;      /* 内存数据使用量 */
    uint32_t log_file_count;          /* 日志文件数量 */
    uint32_t trace_file_count;        /* 追踪文件数量 */
} heapstore_stats_t;

/**
 * @brief 初始化数据分区
 *
 * @param manager 配置参数（如果为 NULL，使用默认配置）
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_init(const heapstore_config_t* manager);

/**
 * @brief 关闭数据分区并清理资源
 */
void heapstore_shutdown(void);

/**
 * @brief 检查数据分区是否已初始化
 *
 * @return bool 已初始化返回 true
 */
bool heapstore_is_initialized(void);

/**
 * @brief 获取数据分区根路径
 *
 * @return const char* 根路径字符串
 */
const char* heapstore_get_root(void);

/**
 * @brief 获取指定类型的路径
 *
 * @param type 路径类型
 * @return const char* 路径字符串（不包含根路径前缀）
 */
const char* heapstore_get_path(heapstore_path_type_t type);

/**
 * @brief 获取完整路径
 *
 * @param type 路径类型
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_get_full_path(heapstore_path_type_t type, char* buffer, size_t buffer_size);

/**
 * @brief 获取统计信息
 *
 * @param stats 输出统计信息结构
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_get_stats(heapstore_stats_t* stats);

/**
 * @brief 清理过期数据
 *
 * @param dry_run 如果为 true，仅返回将清理的数据量，不实际清理
 * @param freed_bytes 输出实际释放的字节数
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_cleanup(bool dry_run, uint64_t* freed_bytes);

/**
 * @brief 获取错误码对应的描述字符串
 *
 * @param err 错误码
 * @return const char* 错误描述
 */
const char* heapstore_strerror(heapstore_error_t err);

/**
 * @brief 重新加载配置
 *
 * @param manager 新配置
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_reload_config(const heapstore_config_t* manager);

/**
 * @brief 强制刷新所有待写入的数据
 *
 * @return heapstore_error_t 错误码
 */
heapstore_error_t heapstore_flush(void);

/**
 * @brief 健康检查接口，用于检查各子系统状态
 *
 * @param registry_ok 注册表系统是否健康，可为 NULL
 * @param trace_ok 追踪系统是否健康，可为 NULL
 * @param log_ok 日志系统是否健康，可为 NULL
 * @param ipc_ok IPC 系统是否健康，可为 NULL
 * @param memory_ok 内存系统是否健康，可为 NULL
 * @return heapstore_error_t 错误码，heapstore_SUCCESS 表示整体健康
 */
heapstore_error_t heapstore_health_check(bool* registry_ok, 
                                       bool* trace_ok, 
                                       bool* log_ok, 
                                       bool* ipc_ok, 
                                       bool* memory_ok);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_heapstore_H */

