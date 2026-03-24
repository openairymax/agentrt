/**
 * @file executor.c
 * @brief 工具执行器实现（跨平台、安全版本）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 改进：
 * 1. 跨平台进程管理（Windows/POSIX）
 * 2. 安全的进程创建（避免 fork 后调用非 async-signal-safe 函数）
 * 3. 超时控制
 * 4. 非阻塞 I/O
 * 5. 资源清理
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
#define MAX_OUTPUT_SIZE (10 * 1024 * 1024)  /* 10MB */
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
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
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

/* ==================== 执行器接口实现 ==================== */

/**
 * @brief 创建执行器
 */
tool_executor_t* tool_executor_create(const tool_executor_config_t* config) {
    if (!config) {
        agentos_error_push_ex(AGENTOS_ERR_INVALID_PARAM, __FILE__, __LINE__, 
                              __func__, "config is NULL");
        return NULL;
    }
    
    tool_executor_t* exec = (tool_executor_t*)calloc(1, sizeof(tool_executor_t));
    if (!exec) {
        agentos_error_push_ex(AGENTOS_ERR_OUT_OF_MEMORY, __FILE__, __LINE__,
                              __func__, "Failed to allocate executor");
        return NULL;
    }
    
    exec->config = *config;
    
    if (exec->config.timeout_ms == 0) {
        exec->config.timeout_ms = DEFAULT_TIMEOUT_MS;
    }
    
    if (agentos_mutex_init(&exec->lock) != 0) {
        free(exec);
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__,
                              __func__, "Failed to initialize mutex");
        return NULL;
    }
    
    return exec;
}

/**
 * @brief 销毁执行器
 */
void tool_executor_destroy(tool_executor_t* exec) {
    if (!exec) return;
    
    agentos_mutex_destroy(&exec->lock);
    free(exec);
}

/**
 * @brief 执行工具（跨平台安全版本）
 */
int tool_executor_run(tool_executor_t* exec,
                      const tool_meta_t* meta,
                      const char* input,
                      char** output,
                      char** error_output,
                      int* exit_code) {
    if (!exec || !meta || !output) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    *output = NULL;
    if (error_output) *error_output = NULL;
    if (exit_code) *exit_code = -1;
    
    agentos_mutex_lock(&exec->lock);
    
    execution_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.timeout_ms = exec->config.timeout_ms;
    ctx.start_time = agentos_time_ms();
    
    output_buffer_t stdout_buf;
    output_buffer_t stderr_buf;
    
    if (output_buffer_init(&stdout_buf) != 0) {
        agentos_mutex_unlock(&exec->lock);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to init stdout buffer");
    }
    
    if (output_buffer_init(&stderr_buf) != 0) {
        output_buffer_free(&stdout_buf);
        agentos_mutex_unlock(&exec->lock);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to init stderr buffer");
    }
    
    /* 构建参数数组 */
    int argc = 2;  /* executable + NULL */
    if (meta->args) {
        for (int i = 0; meta->args[i]; i++) argc++;
    }
    
    char** argv = (char**)malloc(sizeof(char*) * argc);
    if (!argv) {
        output_buffer_free(&stdout_buf);
        output_buffer_free(&stderr_buf);
        agentos_mutex_unlock(&exec->lock);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to allocate argv");
    }
    
    argv[0] = (char*)meta->executable;
    int arg_idx = 1;
    if (meta->args) {
        for (int i = 0; meta->args[i]; i++) {
            argv[arg_idx++] = (char*)meta->args[i];
        }
    }
    argv[arg_idx] = NULL;
    
    /* 创建进程 */
    int ret = agentos_process_create(
        meta->executable,
        argv,
        meta->working_dir,
        input ? &ctx.stdin_pipe : NULL,
        &ctx.stdout_pipe,
        error_output ? &ctx.stderr_pipe : NULL,
        &ctx.process
    );
    
    free(argv);
    
    if (ret != 0) {
        output_buffer_free(&stdout_buf);
        output_buffer_free(&stderr_buf);
        agentos_mutex_unlock(&exec->lock);
        AGENTOS_ERROR(AGENTOS_ERR_TOOL_EXEC_FAIL, "Failed to create process");
    }
    
    ctx.running = 1;
    
    /* 写入输入数据 */
    if (input && strlen(input) > 0) {
        size_t input_len = strlen(input);
        size_t written = 0;
        
        while (written < input_len) {
            int n = agentos_pipe_write(&ctx.stdin_pipe, input + written, input_len - written);
            if (n <= 0) break;
            written += (size_t)n;
        }
        
        agentos_pipe_close_write(&ctx.stdin_pipe);
    }
    
    /* 设置非阻塞读取 */
    agentos_pipe_set_nonblock(&ctx.stdout_pipe, 1);
    if (error_output) {
        agentos_pipe_set_nonblock(&ctx.stderr_pipe, 1);
    }
    
    /* 读取输出（带超时） */
    char read_buf[READ_BUFFER_SIZE];
    int stdout_closed = 0;
    int stderr_closed = error_output ? 0 : 1;
    
    while (!stdout_closed || !stderr_closed) {
        /* 检查超时 */
        uint64_t elapsed = agentos_time_ms() - ctx.start_time;
        if (elapsed > ctx.timeout_ms) {
            agentos_process_kill(&ctx.process);
            ctx.running = 0;
            output_buffer_free(&stdout_buf);
            output_buffer_free(&stderr_buf);
            agentos_process_close_pipes(&ctx.process);
            agentos_mutex_unlock(&exec->lock);
            AGENTOS_ERROR(AGENTOS_ERR_TOOL_TIMEOUT, "Tool execution timeout");
        }
        
        int got_data = 0;
        
        /* 读取 stdout */
        if (!stdout_closed) {
            int n = agentos_pipe_read(&ctx.stdout_pipe, read_buf, sizeof(read_buf));
            if (n > 0) {
                output_buffer_append(&stdout_buf, read_buf, n);
                got_data = 1;
            } else if (n == 0 || 
#if defined(AGENTOS_PLATFORM_WINDOWS)
                      GetLastError() == ERROR_BROKEN_PIPE
#else
                      errno == EPIPE
#endif
                      ) {
                stdout_closed = 1;
            }
        }
        
        /* 读取 stderr */
        if (!stderr_closed && error_output) {
            int n = agentos_pipe_read(&ctx.stderr_pipe, read_buf, sizeof(read_buf));
            if (n > 0) {
                output_buffer_append(&stderr_buf, read_buf, n);
                got_data = 1;
            } else if (n == 0 ||
#if defined(AGENTOS_PLATFORM_WINDOWS)
                      GetLastError() == ERROR_BROKEN_PIPE
#else
                      errno == EPIPE
#endif
                      ) {
                stderr_closed = 1;
            }
        }
        
        /* 如果没有数据，短暂等待 */
        if (!got_data) {
            agentos_sleep_ms(10);
        }
    }
    
    /* 等待进程结束 */
    int proc_exit_code = 0;
    ret = agentos_process_wait(&ctx.process, ctx.timeout_ms - 
                               (uint32_t)(agentos_time_ms() - ctx.start_time), 
                               &proc_exit_code);
    
    if (ret == -2) {
        /* 超时，强制终止 */
        agentos_process_kill(&ctx.process);
        agentos_process_wait(&ctx.process, 1000, NULL);
    }
    
    /* 关闭管道 */
    agentos_process_close_pipes(&ctx.process);
    
    /* 返回结果 */
    *output = stdout_buf.data;
    if (error_output) {
        *error_output = stderr_buf.data;
    } else {
        output_buffer_free(&stderr_buf);
    }
    
    if (exit_code) {
        *exit_code = proc_exit_code;
    }
    
    agentos_mutex_unlock(&exec->lock);
    
    return (proc_exit_code == 0) ? AGENTOS_OK : AGENTOS_ERR_TOOL_EXEC_FAIL;
}

/* ==================== 异步执行上下文 ==================== */

/**
 * @brief 异步执行上下文结构
 * @note 用于在线程间传递执行参数
 */
typedef struct async_exec_context {
    tool_executor_t* exec;
    tool_meta_t meta_copy;
    char* input_copy;
    tool_execution_callback_t callback;
    void* user_data;
} async_exec_context_t;

/**
 * @brief 释放异步执行上下文
 * @param ctx 上下文指针
 */
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

/**
 * @brief 异步执行线程入口函数
 * @param arg 异步执行上下文指针
 * @return NULL
 * @note 标准C函数，跨平台兼容
 */
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

/**
 * @brief 异步执行工具
 */
int tool_executor_run_async(tool_executor_t* exec,
                            const tool_meta_t* meta,
                            const char* input,
                            tool_execution_callback_t callback,
                            void* user_data) {
    if (!exec || !meta || !callback) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "Invalid parameters");
    }
    
    async_exec_context_t* ctx = (async_exec_context_t*)calloc(1, sizeof(async_exec_context_t));
    if (!ctx) {
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to allocate async context");
    }
    
    ctx->exec = exec;
    ctx->callback = callback;
    ctx->user_data = user_data;
    
    ctx->meta_copy.executable = strdup(meta->executable);
    if (!ctx->meta_copy.executable) {
        free(ctx);
        AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to copy executable");
    }
    
    if (meta->working_dir) {
        ctx->meta_copy.working_dir = strdup(meta->working_dir);
    }
    
    if (input) {
        ctx->input_copy = strdup(input);
    }
    
    ctx->meta_copy.timeout_ms = meta->timeout_ms;
    
    if (meta->args) {
        int argc = 0;
        for (int i = 0; meta->args[i]; i++) argc++;
        
        ctx->meta_copy.args = (char**)calloc(argc + 1, sizeof(char*));
        if (!ctx->meta_copy.args) {
            async_context_free(ctx);
            AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to allocate args");
        }
        
        for (int i = 0; meta->args[i]; i++) {
            ctx->meta_copy.args[i] = strdup(meta->args[i]);
            if (!ctx->meta_copy.args[i]) {
                async_context_free(ctx);
                AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "Failed to copy arg");
            }
        }
        ctx->meta_copy.args[argc] = NULL;
    }
    
    agentos_thread_t thread;
    int ret = agentos_thread_create(&thread, async_exec_thread_func, ctx);
    if (ret != 0) {
        async_context_free(ctx);
        AGENTOS_ERROR(AGENTOS_ERR_UNKNOWN, "Failed to create async thread");
    }
    
    agentos_thread_detach(thread);
    
    return AGENTOS_OK;
}

/**
 * @brief 取消执行
 */
int tool_executor_cancel(tool_executor_t* exec, uint64_t execution_id) {
    if (!exec) {
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "exec is NULL");
    }
    
    (void)execution_id;  /* 暂未实现执行ID追踪 */
    
    /* TODO: 实现执行取消逻辑 */
    
    return AGENTOS_OK;
}

/* ==================== 工具元数据管理 ==================== */

/**
 * @brief 创建工具元数据
 */
tool_meta_t* tool_meta_create(const char* executable,
                               const char* const* args,
                               const char* working_dir,
                               uint32_t timeout_ms) {
    if (!executable) {
        return NULL;
    }
    
    tool_meta_t* meta = (tool_meta_t*)calloc(1, sizeof(tool_meta_t));
    if (!meta) return NULL;
    
    meta->executable = strdup(executable);
    if (!meta->executable) {
        free(meta);
        return NULL;
    }
    
    if (working_dir) {
        meta->working_dir = strdup(working_dir);
    }
    
    meta->timeout_ms = timeout_ms;
    
    if (args) {
        int argc = 0;
        for (int i = 0; args[i]; i++) argc++;
        
        meta->args = (char**)malloc(sizeof(char*) * (argc + 1));
        if (meta->args) {
            for (int i = 0; args[i]; i++) {
                meta->args[i] = strdup(args[i]);
            }
            meta->args[argc] = NULL;
        }
    }
    
    return meta;
}

/**
 * @brief 销毁工具元数据
 */
void tool_meta_destroy(tool_meta_t* meta) {
    if (!meta) return;
    
    free((void*)meta->executable);
    free((void*)meta->working_dir);
    
    if (meta->args) {
        for (int i = 0; meta->args[i]; i++) {
            free(meta->args[i]);
        }
        free(meta->args);
    }
    
    free(meta);
}
