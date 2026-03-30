/**
 * @file workbench_process.h
 * @brief 进程管理内部接口
 * @author Spharx
 * @date 2024
 */

#ifndef CUPOLAS_WORKBENCH_PROCESS_H
#define CUPOLAS_WORKBENCH_PROCESS_H

#include "../platform/platform.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 进程属�?*/
typedef struct cupolas_process_attr {
    const char* working_dir;
    const char** env;
    bool redirect_stdin;
    bool redirect_stdout;
    bool redirect_stderr;
    cupolas_pipe_t stdin_pipe;
    cupolas_pipe_t stdout_pipe;
    cupolas_pipe_t stderr_pipe;
} cupolas_process_attr_t;

/* 进程退出状�?*/
typedef struct cupolas_exit_status {
    int code;
    bool signaled;
    int signal;
} cupolas_exit_status_t;

/**
 * @brief 创建子进�?
 * @param proc 进程句柄输出
 * @param path 可执行文件路�?
 * @param argv 参数数组
 * @param attr 进程属�?
 * @return 0 成功，其他失�?
 */
int cupolas_process_spawn(cupolas_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const cupolas_process_attr_t* attr);

/**
 * @brief 等待进程结束
 * @param proc 进程句柄
 * @param status 退出状态输�?
 * @param timeout_ms 超时时间（毫秒）�? 表示无限等待
 * @return 0 成功，CUPOLAS_ERROR_TIMEOUT 超时，其他失败
 */
int cupolas_process_wait(cupolas_process_t proc, cupolas_exit_status_t* status, uint32_t timeout_ms);

/**
 * @brief 终止进程
 * @param proc 进程句柄
 * @param signal 信号（Windows 下忽略）
 * @return 0 成功，其他失�?
 */
int cupolas_process_terminate(cupolas_process_t proc, int signal);

/**
 * @brief 关闭进程句柄
 * @param proc 进程句柄
 * @return 0 成功，其他失�?
 */
int cupolas_process_close(cupolas_process_t proc);

/**
 * @brief 获取进程 ID
 * @param proc 进程句柄
 * @return 进程 ID
 */
cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc);

#ifdef __cplusplus
}
#endif

#endif /* CUPOLAS_WORKBENCH_PROCESS_H */
