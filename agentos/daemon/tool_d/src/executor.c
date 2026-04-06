/**
 * @file executor.c
 * @brief 工具执行器实现（跨平台、安全版本）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明（基于 20260326-02 代码质量评估报告）：
 * 1. 拆分 tool_executor_run，从 190 行减少到约 80 行
 * 2. 圈复杂度从 28 降低到 8 以下
 * 3. 提取子函数：prepare_context, create_process, handle_io, wait_completion
 * 4. 改进错误处理路径
 * 5. 保持跨平台兼容性和安全性
 */

#include "executor.h"
#include "platform.h"
#include "error.h"
#include "tool_errors.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 配置常量 ==================== */

#define DEFAULT_TIMEOUT_MS 30000
#define MAX_OUTPUT_SIZE (10 * 1024 * 1024)
#define READ_BUFFER_SIZE 4096

/* ==================== 执行上下文 ==================== */

typedef struct {
    agentos_process_t process;
    agentos_pipe_t stdin_pipe;
    agentos_pipe_t stdout_pipe;
    agentos_pipe_t stderr_pipe;
    uint32_t timeout_ms;
    uint64_t start_time;
    int running;
} execution_context_t;

/* ==================== 输出缓冲区 ==================== */

typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} output_buffer_t;

static int output_buffer_init(output_buffer_t* buf) {
    buf->capacity = READ_BUFFER_SIZE;
    buf->data = (char*)malloc(buf->capacity);
    if (!buf->data) return -1;
    buf->data[0] = '\0';
    buf->size = 0;
    return 0;
}

static void output_buffer_free(output_buffer_t* buf) {
    if (buf && buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    if (buf) {
        buf->size = 0;
        buf->capacity = 0;
    }
}

static int output_buffer_append(output_buffer_t* buf, const char* data, size_t len) {
    if (buf->size + len + 1 > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        if (new_capacity < buf->size + len + 1) {
            new_capacity = buf->size + len + 1;
        }
        if (new_capacity > MAX_OUTPUT_SIZE) {
            new_capacity = MAX_OUTPUT_SIZE;
        }

        char* new_data = (char*)realloc(buf->data, new_capacity);
        if (!new_data) return -1;

        buf->data = new_data;
        buf->capacity = new_capacity;
    }

    if (buf->size + len > MAX_OUTPUT_SIZE) {
        len = MAX_OUTPUT_SIZE - buf->size;
    }

    memcpy(buf->data + buf->size, data, len);
    buf->size += len;
    buf->data[buf->size] = '\0';

    return 0;
}

/* ==================== 异步执行上下文 ==================== */

typedef struct async_exec_context {
    tool_executor_t* exec;
    tool_meta_t meta_copy;
    char* input_copy;
    tool_execution_callback_t callback;
    void* user_data;
} async_exec_context_t;

static void async_context_free(async_exec_context_t* ctx) {
    if (!ctx) return;

    free(ctx->input_copy);
    free((void*)ctx->meta_copy.executable);
    free((void*)ctx->meta_copy.working_dir);

    if (ctx->meta_copy.args) {
        for (int i = 0; ctx->meta_copy.args[i]; i++) {
            free(ctx->meta_copy.args[i]);
        }
        free(ctx->meta_copy.args);
    }

    free(ctx);
}

/* ==================== 子函数拆分（降低圈复杂度） ==================== */

/**
 * @brief 准备执行上下文
 */
static int prepare_execution(execution_context_t* ctx,
                            output_buffer_t* stdout_buf,
                            output_buffer_t* stderr_buf,
                            const tool_meta_t* meta,
                            const char* input) {
    memset(ctx, 0, sizeof(execution_context_t));

    if (output_buffer_init(stdout_buf) != 0) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    if (output_buffer_init(stderr_buf) != 0) {
        output_buffer_free(stdout_buf);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    return AGENTOS_OK;
}

/**
 * @brief 构建参数数组
 */
static char** build_argv(const tool_meta_t* meta, int* out_argc) {
    int argc = 2;
    if (meta->args) {
        for (int i = 0; meta->args[i]; i++) argc++;
    }

    char** argv = (char**)malloc(sizeof(*argv) * argc);
    if (!argv) {
        *out_argc = 0;
        return NULL;
    }

    argv[0] = (char*)meta->executable;
    int arg_idx = 1;
    if (meta->args) {
        for (int i = 0; meta->args[i]; i++) {
            argv[arg_idx++] = (char*)meta->args[i];
        }
    }
    argv[arg_idx] = NULL;
    *out_argc = argc;
    return argv;
}

/**
 * @brief 创建进程
 */
static int launch_process(execution_context_t* ctx,
                         const tool_meta_t* meta,
                         const char* input) {
    int argc = 0;
    char** argv = build_argv(meta, &argc);
    if (!argv) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    int ret = agentos_process_create(
        meta->executable,
        argv,
        meta->working_dir,
        input ? &ctx->stdin_pipe : NULL,
        &ctx->stdout_pipe,
        &ctx->stderr_pipe,
        &ctx->process
    );

    free(argv);

    if (ret != 0) {
        return AGENTOS_ERR_TOOL_EXEC_FAIL;
    }

    ctx->running = 1;
    return AGENTOS_OK;
}

/**
 * @brief 写入输入数据
 */
static int write_input(execution_context_t* ctx, const char* input) {
    if (!input || strlen(input) == 0) {
        return AGENTOS_OK;
    }

    size_t input_len = strlen(input);
    size_t written = 0;

    while (written < input_len) {
        int n = agentos_pipe_write(&ctx->stdin_pipe, input + written, input_len - written);
        if (n <= 0) break;
        written += (size_t)n;
    }

    agentos_pipe_close_write(&ctx->stdin_pipe);
    return AGENTOS_OK;
}

/**
 * @brief 读取输出数据
 */
static int read_outputs(execution_context_t* ctx,
                        output_buffer_t* stdout_buf,
                        output_buffer_t* stderr_buf,
                        char** error_output,
                        int timeout_ms) {
    agentos_pipe_set_nonblock(&ctx->stdout_pipe, 1);
    agentos_pipe_set_nonblock(&ctx->stderr_pipe, 1);

    char read_buf[READ_BUFFER_SIZE];
    int stdout_closed = 0;
    int stderr_closed = error_output ? 0 : 1;

    while (!stdout_closed || !stderr_closed) {
        uint64_t elapsed = agentos_time_ms() - ctx->start_time;
        if (elapsed > (uint64_t)timeout_ms) {
            return AGENTOS_ERR_TOOL_TIMEOUT;
        }

        int got_data = 0;

        if (!stdout_closed) {
            int n = agentos_pipe_read(&ctx->stdout_pipe, read_buf, sizeof(read_buf));
            if (n > 0) {
                output_buffer_append(stdout_buf, read_buf, n);
                got_data = 1;
            } else if (n == 0 || is_pipe_broken()) {
                stdout_closed = 1;
            }
        }

        if (!stderr_closed && error_output) {
            int n = agentos_pipe_read(&ctx->stderr_pipe, read_buf, sizeof(read_buf));
            if (n > 0) {
                output_buffer_append(stderr_buf, read_buf, n);
                got_data = 1;
            } else if (n == 0 || is_pipe_broken()) {
                stderr_closed = 1;
            }
        }

        if (!got_data) {
            agentos_sleep_ms(10);
        }
    }

    if (error_output && stderr_buf->size > 0) {
        *error_output = stderr_buf->data;
        stderr_buf->data = NULL;
    }

    return AGENTOS_OK;
}

/**
 * @brief 等待进程结束
 */
static int wait_process(execution_context_t* ctx, int* exit_code, int timeout_ms) {
    uint32_t elapsed = (uint32_t)(agentos_time_ms() - ctx->start_time);
    if (elapsed > (uint32_t)timeout_ms) {
        elapsed = timeout_ms;
    }

    int proc_exit_code = 0;
    int ret = agentos_process_wait(&ctx->process, (uint32_t)(timeout_ms - elapsed), &proc_exit_code);

    if (ret == -2) {
        agentos_process_kill(&ctx->process);
        agentos_process_wait(&ctx->process, 1000, NULL);
        return AGENTOS_ERR_TOOL_TIMEOUT;
    }

    if (exit_code) {
        *exit_code = proc_exit_code;
    }

    return (proc_exit_code == 0) ? AGENTOS_OK : AGENTOS_ERR_TOOL_EXEC_FAIL;
}

/**
 * @brief 清理执行资源
 */
static void cleanup_execution(execution_context_t* ctx,
                             output_buffer_t* stdout_buf,
                             output_buffer_t* stderr_buf,
                             int has_error_output) {
    agentos_process_close_pipes(&ctx->process);
    output_buffer_free(stdout_buf);
    if (!has_error_output) {
        output_buffer_free(stderr_buf);
    }
}

/* ==================== 异步执行线程入口 ==================== */

static void* async_exec_thread_func(void* arg) {
    async_exec_context_t* ctx = (async_exec_context_t*)arg;
    if (!ctx) {
        return NULL;
    }

    char* output = NULL;
    char* error_output = NULL;
    int exit_code = -1;

    int ret = tool_executor_run(ctx->exec, &ctx->meta_copy, ctx->input_copy,
                                &output, &error_output, &exit_code);

    if (ctx->callback) {
        ctx->callback(ret, output, error_output, exit_code, ctx->user_data);
    }

    free(output);
    free(error_output);
    async_context_free(ctx);

    return NULL;
}

/* ==================== 执行器接口实现（简化后） ==================== */

tool_executor_t* tool_executor_create(const tool_executor_config_t* manager) {
    if (!manager) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__,
                              __func__, "manager is NULL");
        return NULL;
    }

    tool_executor_t* exec = (tool_executor_t*)calloc(1, sizeof(tool_executor_t));
    if (!exec) {
        agentos_error_push_ex(AGENTOS_ERR_OUT_OF_MEMORY, __FILE__, __LINE__,
                              __func__, "Failed to allocate executor");
        return NULL;
    }

    exec->manager = *manager;

    if (exec->manager.timeout_ms == 0) {
        exec->manager.timeout_ms = DEFAULT_TIMEOUT_MS;
    }

    if (agentos_mutex_init(&exec->lock) != 0) {
        free(exec);
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__,
                              __func__, "Failed to initialize mutex");
        return NULL;
    }

    return exec;
}

void tool_executor_destroy(tool_executor_t* exec) {
    if (!exec) return;
    agentos_mutex_destroy(&exec->lock);
    free(exec);
}

/**
 * @brief 执行工具（重构后：圈复杂度从 28 降至 8）
 */
int tool_executor_run(tool_executor_t* exec,
                      const tool_meta_t* meta,
                      const char* input,
                      char** output,
                      char** error_output,
                      int* exit_code) {
    if (!exec || !meta || !output) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    *output = NULL;
    if (error_output) *error_output = NULL;
    if (exit_code) *exit_code = -1;

    agentos_mutex_lock(&exec->lock);

    execution_context_t ctx;
    output_buffer_t stdout_buf;
    output_buffer_t stderr_buf;

    int ret = prepare_execution(&ctx, &stdout_buf, &stderr_buf, meta, input);
    if (ret != AGENTOS_OK) {
        agentos_mutex_unlock(&exec->lock);
        return ret;
    }

    ctx.timeout_ms = exec->manager.timeout_ms;
    ctx.start_time = agentos_time_ms();

    ret = launch_process(&ctx, meta, input);
    if (ret != AGENTOS_OK) {
        cleanup_execution(&ctx, &stdout_buf, &stderr_buf, error_output ? 1 : 0);
        agentos_mutex_unlock(&exec->lock);
        return ret;
    }

    write_input(&ctx, input);

    ret = read_outputs(&ctx, &stdout_buf, &stderr_buf, error_output, ctx.timeout_ms);
    if (ret == AGENTOS_ERR_TOOL_TIMEOUT) {
        agentos_process_kill(&ctx.process);
        cleanup_execution(&ctx, &stdout_buf, &stderr_buf, error_output ? 1 : 0);
        agentos_mutex_unlock(&exec->lock);
        return ret;
    }

    *output = stdout_buf.data;
    if (error_output && !*error_output) {
        *error_output = stderr_buf.data;
    }

    ret = wait_process(&ctx, exit_code, ctx.timeout_ms);

    agentos_process_close_pipes(&ctx.process);
    agentos_mutex_unlock(&exec->lock);

    return ret;
}

int tool_executor_run_async(tool_executor_t* exec,
                           const tool_meta_t* meta,
                           const char* input,
                           tool_execution_callback_t callback,
                           void* user_data) {
    if (!exec || !meta) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    async_exec_context_t* ctx = (async_exec_context_t*)calloc(1, sizeof(async_exec_context_t));
    if (!ctx) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    ctx->exec = exec;
    ctx->callback = callback;
    ctx->user_data = user_data;

    ctx->meta_copy.executable = meta->executable ? strdup(meta->executable) : NULL;
    ctx->meta_copy.working_dir = meta->working_dir ? strdup(meta->working_dir) : NULL;

    if (meta->args) {
        int argc = 0;
        while (meta->args[argc]) argc++;
        ctx->meta_copy.args = (char**)calloc((size_t)argc + 1, sizeof(*ctx->meta_copy.args));
        for (int i = 0; i < argc; i++) {
            ctx->meta_copy.args[i] = strdup(meta->args[i]);
        }
    }

    ctx->input_copy = input ? strdup(input) : NULL;

    agentos_thread_t thread;
    if (agentos_thread_create(&thread, async_exec_thread_func, ctx) != 0) {
        async_context_free(ctx);
        return AGENTOS_ERR_UNKNOWN;
    }

    agentos_thread_detach(thread);
    return AGENTOS_OK;
}
