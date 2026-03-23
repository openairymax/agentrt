/**
 * @file workbench.h
 * @brief 虚拟工位公共接口 - 隔离的执行环境
 * @author Spharx
 * @date 2024
 * 
 * 设计原则：
 * - 隔离执行：每个 Agent 运行在独立工位
 * - 资源限制：CPU、内存、时间限制
 * - 安全边界：文件系统、网络隔离
 * - 可观测性：输出捕获、状态监控
 */

#ifndef DOMAIN_WORKBENCH_H
#define DOMAIN_WORKBENCH_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 工位状态 */
typedef enum workbench_state {
    WORKBENCH_STATE_IDLE = 0,
    WORKBENCH_STATE_RUNNING,
    WORKBENCH_STATE_STOPPED,
    WORKBENCH_STATE_ERROR
} workbench_state_t;

/* 工位配置 */
typedef struct workbench_config {
    const char* working_dir;
    const char** env_vars;
    size_t env_count;
    uint32_t timeout_ms;
    size_t max_output_size;
    bool redirect_stdin;
    bool redirect_stdout;
    bool redirect_stderr;
} workbench_config_t;

/* 工位执行结果 */
typedef struct workbench_result {
    int exit_code;
    bool timed_out;
    bool signaled;
    int signal;
    char* stdout_data;
    size_t stdout_size;
    char* stderr_data;
    size_t stderr_size;
    uint64_t start_time_ms;
    uint64_t end_time_ms;
} workbench_result_t;

/* 工位句柄 */
typedef struct workbench workbench_t;

/**
 * @brief 创建工位
 * @param config 配置（可选，NULL 使用默认配置）
 * @return 工位句柄，失败返回 NULL
 */
workbench_t* workbench_create(const workbench_config_t* config);

/**
 * @brief 销毁工位
 * @param wb 工位句柄
 */
void workbench_destroy(workbench_t* wb);

/**
 * @brief 执行命令（同步）
 * @param wb 工位句柄
 * @param command 命令
 * @param argv 参数数组（以 NULL 结尾）
 * @param result 执行结果
 * @return 0 成功，其他失败
 */
int workbench_execute(workbench_t* wb, const char* command, char* const argv[],
                      workbench_result_t* result);

/**
 * @brief 执行命令（异步）
 * @param wb 工位句柄
 * @param command 命令
 * @param argv 参数数组（以 NULL 结尾）
 * @return 0 成功，其他失败
 */
int workbench_execute_async(workbench_t* wb, const char* command, char* const argv[]);

/**
 * @brief 等待执行完成
 * @param wb 工位句柄
 * @param result 执行结果
 * @param timeout_ms 超时时间（毫秒），0 表示无限等待
 * @return 0 成功，DOMES_ERROR_TIMEOUT 超时，其他失败
 */
int workbench_wait(workbench_t* wb, workbench_result_t* result, uint32_t timeout_ms);

/**
 * @brief 终止执行
 * @param wb 工位句柄
 * @return 0 成功，其他失败
 */
int workbench_terminate(workbench_t* wb);

/**
 * @brief 获取工位状态
 * @param wb 工位句柄
 * @return 工位状态
 */
workbench_state_t workbench_get_state(workbench_t* wb);

/**
 * @brief 获取进程 ID
 * @param wb 工位句柄
 * @return 进程 ID，失败返回 -1
 */
int64_t workbench_get_pid(workbench_t* wb);

/**
 * @brief 写入标准输入
 * @param wb 工位句柄
 * @param data 数据
 * @param size 数据大小
 * @param written 实际写入大小
 * @return 0 成功，其他失败
 */
int workbench_write_stdin(workbench_t* wb, const void* data, size_t size, size_t* written);

/**
 * @brief 读取标准输出
 * @param wb 工位句柄
 * @param buf 缓冲区
 * @param size 缓冲区大小
 * @param read_size 实际读取大小
 * @return 0 成功，其他失败
 */
int workbench_read_stdout(workbench_t* wb, void* buf, size_t size, size_t* read_size);

/**
 * @brief 读取标准错误
 * @param wb 工位句柄
 * @param buf 缓冲区
 * @param size 缓冲区大小
 * @param read_size 实际读取大小
 * @return 0 成功，其他失败
 */
int workbench_read_stderr(workbench_t* wb, void* buf, size_t size, size_t* read_size);

/**
 * @brief 释放执行结果
 * @param result 执行结果
 */
void workbench_result_free(workbench_result_t* result);

/**
 * @brief 获取默认配置
 * @param config 配置输出
 */
void workbench_default_config(workbench_config_t* config);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_WORKBENCH_H */
