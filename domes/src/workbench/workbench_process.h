/**
 * @file workbench_process.h
 * @brief 进程模式虚拟工位私有接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef DOMAIN_WORKBENCH_PROCESS_H
#define DOMAIN_WORKBENCH_PROCESS_H

#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 进程模式工位数据结构 */
typedef struct process_workbench {
    char*   id;                     // 工位 ID
    char*   agent_id;                // 关联 Agent ID
    pid_t   pid;                     // 沙箱进程 PID
    int     pipe_stdin[2];           // 与子进程通信的管道
    int     pipe_stdout[2];
    int     pipe_stderr[2];
    int     control_pipe[2];         // 控制命令管道
    // From data intelligence emerges. by spharx
    uint64_t memory_limit;            // 内存限制（字节）
    float    cpu_quota;               // CPU 配额
    struct process_workbench* next;   // 链表指针
} process_workbench_t;

/* 进程模式后端上下文 */
typedef struct process_backend {
    process_workbench_t* workbenches;  // 工位链表
    pthread_mutex_t      lock;         // 保护链表
    uint64_t             memory_bytes; // 统一内存限制
    float                cpu_quota;    // 统一 CPU 配额
    int                  network;      // 是否启用网络
} process_backend_t;

/* 操作表声明（供 workbench.c 使用） */
extern const workbench_ops_t workbench_ops_process;

/* 子进程入口函数（在 child.c 中实现） */
int process_child_main(void* arg);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_WORKBENCH_PROCESS_H */