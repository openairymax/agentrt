/**
 * @file workbench_container.h
 * @brief 容器模式实现 - 基于 Docker/runc 的隔离执行
 * @author Spharx
 * @date 2024
 *
 * 设计原则：
 * - 轻量级隔离：使用 runc/docker 作为运行时
 * - 资源控制：cgroups 资源限制
 * - 网络隔离：独立网络栈
 * - 文件系统隔离：独立根文件系统
 *
 * 支持的容器运行时：
 * - Docker：成熟的生产级容器解决方案
 * - runc：OCI 标准的轻量级运行时
 */

#ifndef cupolas_WORKBENCH_CONTAINER_H
#define cupolas_WORKBENCH_CONTAINER_H

#include "workbench.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 容器运行时类型 */
typedef enum container_runtime {
    CONTAINER_RUNTIME_DOCKER = 0,
    CONTAINER_RUNTIME_RUNC,
    CONTAINER_RUNTIME_CRUN,
    CONTAINER_RUNTIME_AUTO
} container_runtime_t;

/* 容器配置 */
typedef struct container_config {
    const char* image;                  /* 容器镜像 */
    const char* command;                /* 执行命令 */
    const char** args;                  /* 命令参数 */
    size_t args_count;                  /* 参数数量 */

    const char* workdir;               /* 工作目录 */
    const char** env_vars;             /* 环境变量 */
    size_t env_count;                   /* 环境变量数量 */

    container_runtime_t runtime;        /* 容器运行时 */

    struct {
        const char* network_mode;       /* 网络模式：bridge, none, host */
        bool readonly_rootfs;           /* 只读根文件系统 */
        const char* user;               /* 用户/组 ID */
        size_t memory_limit;            /* 内存限制（字节） */
        int cpu_shares;                 /* CPU 权重 */
        int cpu_quota;                  /* CPU 配额（微秒） */
        int pids_limit;                 /* 进程数限制 */
    } resources;

    struct {
        bool enable_logging;            /* 启用容器日志 */
        const char* log_driver;        /* 日志驱动：json-file, syslog */
        size_t log_max_size;           /* 单个日志文件大小 */
        int log_max_files;             /* 保留日志文件数 */
    } logging;

    struct {
        bool use_cache;                 /* 使用镜像缓存 */
        bool pull_latest;               /* 总是拉取最新镜像 */
        const char* registry_auth;      /* 镜像仓库认证文件 */
    } image_policy;
} container_config_t;

/* 容器状态 */
typedef enum container_state {
    CONTAINER_STATE_CREATED = 0,
    CONTAINER_STATE_RUNNING,
    CONTAINER_STATE_PAUSED,
    CONTAINER_STATE_STOPPED,
    CONTAINER_STATE_RESTARTING,
    CONTAINER_STATE_DEAD,
    CONTAINER_STATE_UNKNOWN
} container_state_t;

/* 容器信息 */
typedef struct container_info {
    char container_id[64];              /* 容器 ID（64 字符） */
    char name[256];                     /* 容器名称 */
    container_state_t state;            /* 容器状态 */
    int exit_code;                      /* 退出码 */
    uint64_t exit_time;                 /* 退出时间戳 */

    struct {
        size_t memory_usage;            /* 当前内存使用（字节） */
        size_t memory_limit;            /* 内存限制（字节） */
        uint64_t cpu_usage;             /* CPU 使用（纳秒） */
        uint64_t pids_current;          /* 当前进程数 */
    } stats;

    struct {
        uint64_t started_at;            /* 启动时间戳 */
        uint64_t finished_at;          /* 结束时间戳 */
        size_t rx_bytes;                /* 接收字节数 */
        size_t tx_bytes;                /* 发送字节数 */
    } metrics;
} container_info_t;

/* 容器操作结果 */
typedef struct container_result {
    int exit_code;                     /* 进程退出码 */
    char* stdout_data;                 /* 标准输出 */
    size_t stdout_size;                /* 输出大小 */
    char* stderr_data;                 /* 错误输出 */
    size_t stderr_size;                /* 错误大小 */
    uint64_t duration_ns;              /* 执行时长（纳秒） */
    int oom_killed;                    /* 是否被 OOM 杀死 */
} container_result_t;

/**
 * @brief 创建容器管理器
 * @param manager 容器配置（NULL 使用默认配置）
 * @return 容器管理器句柄，失败返回 NULL
 */
void* container_manager_create(const container_config_t* manager);

/**
 * @brief 销毁容器管理器
 * @param mgr 容器管理器句柄
 */
void container_manager_destroy(void* mgr);

/**
 * @brief 拉取容器镜像
 * @param mgr 容器管理器句柄
 * @param image 镜像名称
 * @return 0 成功，其他失败
 */
int container_pull_image(void* mgr, const char* image);

/**
 * @brief 创建并启动容器
 * @param mgr 容器管理器句柄
 * @param name 容器名称（NULL 自动生成）
 * @param result 执行结果输出（可为 NULL）
 * @return 0 成功，其他失败
 */
int container_start(void* mgr, const char* name, container_result_t* result);

/**
 * @brief 停止容器
 * @param mgr 容器管理器句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，其他失败
 */
int container_stop(void* mgr, uint32_t timeout_ms);

/**
 * @brief 删除容器
 * @param mgr 容器管理器句柄
 * @return 0 成功，其他失败
 */
int container_remove(void* mgr);

/**
 * @brief 获取容器状态
 * @param mgr 容器管理器句柄
 * @param info 状态信息输出
 * @return 0 成功，其他失败
 */
int container_get_info(void* mgr, container_info_t* info);

/**
 * @brief 获取容器资源使用统计
 * @param mgr 容器管理器句柄
 * @param info 统计信息输出
 * @return 0 成功，其他失败
 */
int container_get_stats(void* mgr, container_info_t* info);

/**
 * @brief 暂停容器
 * @param mgr 容器管理器句柄
 * @return 0 成功，其他失败
 */
int container_pause(void* mgr);

/**
 * @brief 恢复容器
 * @param mgr 容器管理器句柄
 * @return 0 成功，其他失败
 */
int container_unpause(void* mgr);

/**
 * @brief 等待容器结束
 * @param mgr 容器管理器句柄
 * @param timeout_ms 超时时间（毫秒），0 表示无限等待
 * @param exit_code 退出码输出（可为 NULL）
 * @return 0 成功，AGENTOS_ERR_TIMEOUT 超时，其他失败
 */
int container_wait(void* mgr, uint32_t timeout_ms, int* exit_code);

/**
 * @brief 执行容器内命令（不创建新容器）
 * @param mgr 容器管理器句柄
 * @param command 执行命令
 * @param args 命令参数
 * @param arg_count 参数数量
 * @param result 执行结果输出
 * @return 0 成功，其他失败
 */
int container_exec(void* mgr, const char* command, const char** args,
                  size_t arg_count, container_result_t* result);

/**
 * @brief 获取容器日志
 * @param mgr 容器管理器句柄
 * @param tail 行数（0 表示全部）
 * @param output 输出缓冲区
 * @param size 缓冲区大小
 * @return 0 成功，其他失败
 */
int container_get_logs(void* mgr, size_t tail, char* output, size_t size);

/**
 * @brief 检查容器运行时是否可用
 * @param runtime 运行时类型
 * @return true 可用，false 不可用
 */
bool container_runtime_is_available(container_runtime_t runtime);

/**
 * @brief 获取默认容器配置
 * @param manager 配置输出
 */
void container_config_init(container_config_t* manager);

/**
 * @brief 释放容器结果内存
 * @param result 容器执行结果
 */
void container_result_free(container_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* cupolas_WORKBENCH_CONTAINER_H */