/**
 * @file partdata.h
 * @brief AgentOS 数据分区核心接口
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#ifndef AGENTOS_PARTDATA_H
#define AGENTOS_PARTDATA_H

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
    PARTDATA_SUCCESS = 0,
    PARTDATA_ERR_INVALID_PARAM = -1,
    PARTDATA_ERR_NOT_INITIALIZED = -2,
    PARTDATA_ERR_ALREADY_INITIALIZED = -3,
    PARTDATA_ERR_DIR_CREATE_FAILED = -4,
    PARTDATA_ERR_DIR_NOT_FOUND = -5,
    PARTDATA_ERR_PERMISSION_DENIED = -6,
    PARTDATA_ERR_OUT_OF_MEMORY = -7,
    PARTDATA_ERR_DB_INIT_FAILED = -8,
    PARTDATA_ERR_DB_QUERY_FAILED = -9,
    PARTDATA_ERR_FILE_OPEN_FAILED = -10,
    PARTDATA_ERR_CONFIG_INVALID = -11,
    PARTDATA_ERR_NOT_FOUND = -12,
    PARTDATA_ERR_INTERNAL = -99
} partdata_error_t;

/**
 * @brief 数据分区路径类型
 */
typedef enum {
    PARTDATA_PATH_KERNEL,         /* 内核数据路径 */
    PARTDATA_PATH_LOGS,           /* 日志文件路径 */
    PARTDATA_PATH_REGISTRY,       /* 注册表路径 */
    PARTDATA_PATH_SERVICES,       /* 服务数据路径 */
    PARTDATA_PATH_TRACES,         /* 追踪数据路径 */
    PARTDATA_PATH_KERNEL_IPC,     /* 内核 IPC 数据 */
    PARTDATA_PATH_KERNEL_MEMORY,  /* 内核内存数据 */
    PARTDATA_PATH_MAX
} partdata_path_type_t;

/**
 * @brief 配置项结构
 */
typedef struct partdata_config {
    const char* root_path;            /* 数据分区根路径 */
    size_t max_log_size_mb;           /* 最大日志文件大小(MB) */
    int log_retention_days;           /* 日志保留天数 */
    int trace_retention_days;         /* 追踪数据保留天数 */
    bool enable_auto_cleanup;          /* 启用自动清理 */
    bool enable_log_rotation;          /* 启用日志轮转 */
    bool enable_trace_export;          /* 启用追踪导出 */
    int db_vacuum_interval_days;      /* 数据库 Vacuum 间隔(天) */
} partdata_config_t;

/**
 * @brief 统计信息结构
 */
typedef struct partdata_stats {
    uint64_t total_disk_usage_bytes;  /* 总磁盘使用量 */
    uint64_t log_usage_bytes;         /* 日志使用量 */
    uint64_t registry_usage_bytes;    /* 注册表使用量 */
    uint64_t trace_usage_bytes;       /* 追踪数据使用量 */
    uint64_t ipc_usage_bytes;         /* IPC 数据使用量 */
    uint64_t memory_usage_bytes;      /* 内存数据使用量 */
    uint32_t log_file_count;          /* 日志文件数量 */
    uint32_t trace_file_count;        /* 追踪文件数量 */
} partdata_stats_t;

/**
 * @brief 初始化数据分区
 *
 * @param config 配置参数（如果为 NULL，使用默认配置）
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_init(const partdata_config_t* config);

/**
 * @brief 关闭数据分区并清理资源
 */
void partdata_shutdown(void);

/**
 * @brief 检查数据分区是否已初始化
 *
 * @return bool 已初始化返回 true
 */
bool partdata_is_initialized(void);

/**
 * @brief 获取数据分区根路径
 *
 * @return const char* 根路径字符串
 */
const char* partdata_get_root(void);

/**
 * @brief 获取指定类型的路径
 *
 * @param type 路径类型
 * @return const char* 路径字符串（不包含根路径前缀）
 */
const char* partdata_get_path(partdata_path_type_t type);

/**
 * @brief 获取完整路径
 *
 * @param type 路径类型
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_get_full_path(partdata_path_type_t type, char* buffer, size_t buffer_size);

/**
 * @brief 获取统计信息
 *
 * @param stats 输出统计信息结构
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_get_stats(partdata_stats_t* stats);

/**
 * @brief 清理过期数据
 *
 * @param dry_run 如果为 true，仅返回将清理的数据量，不实际清理
 * @param freed_bytes 输出实际释放的字节数
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_cleanup(bool dry_run, uint64_t* freed_bytes);

/**
 * @brief 获取错误码对应的描述字符串
 *
 * @param err 错误码
 * @return const char* 错误描述
 */
const char* partdata_strerror(partdata_error_t err);

/**
 * @brief 重新加载配置
 *
 * @param config 新配置
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_reload_config(const partdata_config_t* config);

/**
 * @brief 强制刷新所有待写入的数据
 *
 * @return partdata_error_t 错误码
 */
partdata_error_t partdata_flush(void);

/**
 * @brief 健康检查接口，用于检查各子系统状态
 *
 * @param registry_ok 注册表系统是否健康，可为 NULL
 * @param trace_ok 追踪系统是否健康，可为 NULL
 * @param log_ok 日志系统是否健康，可为 NULL
 * @param ipc_ok IPC 系统是否健康，可为 NULL
 * @param memory_ok 内存系统是否健康，可为 NULL
 * @return partdata_error_t 错误码，PARTDATA_SUCCESS 表示整体健康
 */
partdata_error_t partdata_health_check(bool* registry_ok, 
                                       bool* trace_ok, 
                                       bool* log_ok, 
                                       bool* ipc_ok, 
                                       bool* memory_ok);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PARTDATA_H */
