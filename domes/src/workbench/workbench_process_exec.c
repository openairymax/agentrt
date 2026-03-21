/**
 * @file workbench_process_exec.c
 * @brief 进程模式工位命令执行（父进程端）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "workbench.h"
#include "workbench_process.h"
#include "logger.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

/* 命令执行状态 */
typedef struct exec_state {
    pid_t           child_pid;
    int             stdout_pipe[2];
    int             stderr_pipe[2];
    int             control_pipe[2];
    // From data intelligence emerges. by spharx
    char*           workbench_id;
    char**          argv;
    uint32_t        timeout_ms;
    volatile int    completed;
    volatile int    timed_out;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} exec_state_t;

/* 查找工位（内部函数） */
static process_workbench_t* find_workbench(process_backend_t* bctx, const char* id) {
    process_workbench_t* w = bctx->workbenches;
    while (w) {
        if (strcmp(w->id, id) == 0) return w;
        w = w->next;
    }
    return NULL;
}

/* 超时线程函数 */
static void* timeout_thread_func(void* arg) {
    exec_state_t* state = (exec_state_t*)arg;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    uint64_t timeout_ns = state->timeout_ms * 1000000ULL;
    ts.tv_sec += timeout_ns / 1000000000ULL;
    ts.tv_nsec += timeout_ns % 1000000000ULL;
    if (ts.tv_nsec >= 1000000000ULL) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000ULL;
    }

    pthread_mutex_lock(&state->lock);
    while (!state->completed && !state->timed_out) {
        int ret = pthread_cond_timedwait(&state->cond, &state->lock, &ts);
        if (ret == ETIMEDOUT) {
            state->timed_out = 1;
            kill(state->child_pid, SIGKILL);
            break;
        }
    }
    pthread_mutex_unlock(&state->lock);
    return NULL;
}

/* 读取管道数据到动态缓冲区 */
static char* read_pipe_to_string(int fd) {
    char buffer[4096];
    char* result = NULL;
    size_t total = 0;
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        char* new_result = realloc(result, total + n + 1);
        if (!new_result) {
            free(result);
            return NULL;
        }
        result = new_result;
        memcpy(result + total, buffer, n);
        total += n;
        result[total] = '\0';
    }
    if (n < 0) {
        free(result);
        return NULL;
    }
    return result ? result : strdup("");
}

int process_exec_workbench(void* ctx, const char* workbench_id,
                            const char* const* argv,
                            uint32_t timeout_ms,
                            char** out_stdout,
                            char** out_stderr,
                            int* out_exit_code,
                            char** out_error) {
    process_backend_t* bctx = (process_backend_t*)ctx;
    if (!bctx || !workbench_id || !argv || !out_stdout || !out_stderr || !out_exit_code || !out_error)
        return -1;

    // 查找工位
    pthread_mutex_lock(&bctx->lock);
    process_workbench_t* wb = find_workbench(bctx, workbench_id);
    pthread_mutex_unlock(&bctx->lock);
    if (!wb) {
        *out_error = strdup("workbench not found");
        return -1;
    }

    // 创建状态对象
    exec_state_t state;
    memset(&state, 0, sizeof(state));
    state.workbench_id = (char*)workbench_id;
    state.argv = (char**)argv;
    state.timeout_ms = timeout_ms;
    pthread_mutex_init(&state.lock, NULL);
    pthread_cond_init(&state.cond, NULL);

    // 创建管道用于捕获子进程输出
    if (pipe(state.stdout_pipe) < 0 || pipe(state.stderr_pipe) < 0) {
        *out_error = strdup("pipe creation failed");
        pthread_mutex_destroy(&state.lock);
        pthread_cond_destroy(&state.cond);
        return -1;
    }

    // 设置非阻塞标志（避免读阻塞）
    fcntl(state.stdout_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(state.stderr_pipe[0], F_SETFL, O_NONBLOCK);

    // 创建控制管道（用于与子进程同步）
    int control_pipe[2];
    if (pipe(control_pipe) < 0) {
        close(state.stdout_pipe[0]); close(state.stdout_pipe[1]);
        close(state.stderr_pipe[0]); close(state.stderr_pipe[1]);
        *out_error = strdup("control pipe creation failed");
        pthread_mutex_destroy(&state.lock);
        pthread_cond_destroy(&state.cond);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(state.stdout_pipe[0]); close(state.stdout_pipe[1]);
        close(state.stderr_pipe[0]); close(state.stderr_pipe[1]);
        close(control_pipe[0]); close(control_pipe[1]);
        *out_error = strdup("fork failed");
        pthread_mutex_destroy(&state.lock);
        pthread_cond_destroy(&state.cond);
        return -1;
    }

    if (pid == 0) {
        // 子进程
        close(state.stdout_pipe[0]);
        close(state.stderr_pipe[0]);
        close(control_pipe[1]);

        // 重定向标准输出
        dup2(state.stdout_pipe[1], STDOUT_FILENO);
        dup2(state.stderr_pipe[1], STDERR_FILENO);
        close(state.stdout_pipe[1]);
        close(state.stderr_pipe[1]);

        // 将 argv 序列化到控制管道
        FILE* fp = fdopen(control_pipe[1], "w");
        if (!fp) _exit(127);
        for (int i = 0; argv[i] != NULL; i++) {
            fprintf(fp, "%s\n", argv[i]);
        }
        fclose(fp);
        _exit(0);
    }

    // 父进程
    close(state.stdout_pipe[1]);
    close(state.stderr_pipe[1]);
    close(control_pipe[0]);

    state.child_pid = pid;

    // 启动超时线程
    pthread_t timeout_thread;
    int timeout_thread_created = 0;
    if (timeout_ms > 0) {
        if (pthread_create(&timeout_thread, NULL, timeout_thread_func, &state) == 0) {
            timeout_thread_created = 1;
        } else {
            AGENTOS_LOG_WARN("Failed to create timeout thread, exec may block");
        }
    }

    // 等待子进程退出
    int status;
    pid_t ret = waitpid(pid, &status, 0);

    pthread_mutex_lock(&state.lock);
    state.completed = 1;
    pthread_cond_signal(&state.cond);
    pthread_mutex_unlock(&state.lock);

    if (timeout_thread_created) {
        pthread_join(timeout_thread, NULL);
    }

    // 读取输出
    char* stdout_data = read_pipe_to_string(state.stdout_pipe[0]);
    char* stderr_data = read_pipe_to_string(state.stderr_pipe[0]);

    close(state.stdout_pipe[0]);
    close(state.stderr_pipe[0]);
    close(control_pipe[1]);

    pthread_mutex_destroy(&state.lock);
    pthread_cond_destroy(&state.cond);

    if (state.timed_out) {
        *out_stdout = stdout_data ? stdout_data : strdup("");
        *out_stderr = stderr_data ? stderr_data : strdup("");
        *out_exit_code = -1;
        *out_error = strdup("execution timed out");
        return 0;  // 仍返回成功，但错误信息已填充
    }

    if (ret < 0) {
        free(stdout_data);
        free(stderr_data);
        *out_error = strdup("waitpid failed");
        return -1;
    }

    *out_stdout = stdout_data ? stdout_data : strdup("");
    *out_stderr = stderr_data ? stderr_data : strdup("");
    *out_exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    *out_error = NULL;
    return 0;
}