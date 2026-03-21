/**
 * @file workbench_process_core.c
 * @brief 进程模式工位核心管理（创建、销毁、列表、生命周期）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#define _GNU_SOURCE
#include "workbench.h"
#include "workbench_process.h"
#include "logger.h"
#include <sched.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>

/* 创建后端上下文 */
void* process_create_ctx(workbench_manager_t* mgr) {
    process_backend_t* ctx = (process_backend_t*)calloc(1, sizeof(process_backend_t));
    if (!ctx) return NULL;
    // From data intelligence emerges. by spharx
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->memory_bytes = mgr->memory_bytes;
    ctx->cpu_quota = mgr->cpu_quota;
    ctx->network = mgr->network;
    return ctx;
}

/* 销毁后端上下文（同时销毁所有工位） */
static void process_cleanup_ctx(void* ctx) {
    process_backend_t* bctx = (process_backend_t*)ctx;
    if (!bctx) return;
    pthread_mutex_lock(&bctx->lock);
    process_workbench_t* w = bctx->workbenches;
    while (w) {
        process_workbench_t* next = w->next;
        if (w->pid > 0) kill(w->pid, SIGKILL);
        waitpid(w->pid, NULL, 0);
        close(w->pipe_stdin[1]);
        close(w->pipe_stdout[0]);
        close(w->pipe_stderr[0]);
        close(w->control_pipe[1]);
        free(w->id);
        free(w->agent_id);
        free(w);
        w = next;
    }
    pthread_mutex_unlock(&bctx->lock);
    pthread_mutex_destroy(&bctx->lock);
    free(bctx);
}

/* 创建新工位（进程模式） */
int process_create_workbench(void* ctx, const char* agent_id, char** out_id) {
    process_backend_t* bctx = (process_backend_t*)ctx;
    process_workbench_t* wb = (process_workbench_t*)calloc(1, sizeof(process_workbench_t));
    if (!wb) {
        AGENTOS_LOG_ERROR("process_create_workbench: calloc failed");
        return -1;
    }

    wb->id = strdup(*out_id);
    wb->agent_id = strdup(agent_id);
    wb->memory_limit = bctx->memory_bytes;
    wb->cpu_quota = bctx->cpu_quota;

    // 创建管道
    if (pipe(wb->pipe_stdin) < 0 || pipe(wb->pipe_stdout) < 0 ||
        pipe(wb->pipe_stderr) < 0 || pipe(wb->control_pipe) < 0) {
        AGENTOS_LOG_ERROR("process_create_workbench: pipe failed: %s", strerror(errno));
        goto fail;
    }

    // 使用 clone 创建子进程（带命名空间隔离）
    const int clone_flags = CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUTS | SIGCHLD;
    void* stack = malloc(1024 * 1024);
    if (!stack) {
        AGENTOS_LOG_ERROR("process_create_workbench: stack alloc failed");
        goto fail;
    }

    // 子进程入口在 workbench_process_child.c 中
    pid_t pid = clone(process_child_main, (char*)stack + 1024 * 1024, clone_flags, wb);
    free(stack);
    if (pid < 0) {
        AGENTOS_LOG_ERROR("process_create_workbench: clone failed: %s", strerror(errno));
        goto fail;
    }
    wb->pid = pid;

    // 关闭父进程不需要的管道端
    close(wb->pipe_stdin[0]);
    close(wb->pipe_stdout[1]);
    close(wb->pipe_stderr[1]);
    close(wb->control_pipe[0]);

    // 加入链表
    pthread_mutex_lock(&bctx->lock);
    wb->next = bctx->workbenches;
    bctx->workbenches = wb;
    pthread_mutex_unlock(&bctx->lock);

    return 0;

fail:
    if (wb->pipe_stdin[0] > 0) close(wb->pipe_stdin[0]);
    if (wb->pipe_stdin[1] > 0) close(wb->pipe_stdin[1]);
    if (wb->pipe_stdout[0] > 0) close(wb->pipe_stdout[0]);
    if (wb->pipe_stdout[1] > 0) close(wb->pipe_stdout[1]);
    if (wb->pipe_stderr[0] > 0) close(wb->pipe_stderr[0]);
    if (wb->pipe_stderr[1] > 0) close(wb->pipe_stderr[1]);
    if (wb->control_pipe[0] > 0) close(wb->control_pipe[0]);
    if (wb->control_pipe[1] > 0) close(wb->control_pipe[1]);
    free(wb->id);
    free(wb->agent_id);
    free(wb);
    return -1;
}

/* 销毁指定工位 */
void process_destroy_workbench(void* ctx, const char* workbench_id) {
    process_backend_t* bctx = (process_backend_t*)ctx;
    pthread_mutex_lock(&bctx->lock);
    process_workbench_t** p = &bctx->workbenches;
    while (*p) {
        if (strcmp((*p)->id, workbench_id) == 0) {
            process_workbench_t* victim = *p;
            *p = victim->next;
            // 终止子进程
            kill(victim->pid, SIGKILL);
            waitpid(victim->pid, NULL, 0);
            // 关闭管道
            close(victim->pipe_stdin[1]);
            close(victim->pipe_stdout[0]);
            close(victim->pipe_stderr[0]);
            close(victim->control_pipe[1]);
            free(victim->id);
            free(victim->agent_id);
            free(victim);
            break;
        }
        p = &(*p)->next;
    }
    pthread_mutex_unlock(&bctx->lock);
}

/* 列表（由通用层处理，后端无需实现） */
void process_list_workbenches(void* ctx, char*** out_ids, size_t* out_count) {
    // 空实现，实际列表由通用层维护
}

/* 操作表定义 */
const workbench_ops_t workbench_ops_process = {
    .create_ctx = process_create_ctx,
    .create = process_create_workbench,
    .exec = process_exec_workbench,      // 实现在 exec.c 中
    .destroy = process_destroy_workbench,
    .list = process_list_workbenches,
    .cleanup = process_cleanup_ctx,
};