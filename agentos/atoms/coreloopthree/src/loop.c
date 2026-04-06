/**
 * @file loop.c
 * @brief 三层核心运行时主循环实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "loop.h"
#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <stdatomic.h>
#include <unistd.h>
#endif

/**
 * @brief 核心循环结构体
 */
struct agentos_core_loop {
    agentos_cognition_engine_t* cognition;
    agentos_execution_engine_t* execution;
    agentos_memory_engine_t* memory;
    agentos_loop_config_t manager;
    volatile int running;
    volatile int stop_requested;
    agentos_mutex_t* lock;
    agentos_cond_t* cond;
};

/* 辅助函数声明 - 用于重构降低圈复杂度 */
static agentos_error_t validate_loop_parameters(const agentos_loop_config_t* manager, agentos_core_loop_t** out_loop);
static agentos_core_loop_t* allocate_loop_memory(void);
static agentos_error_t initialize_loop_resources(agentos_core_loop_t* loop, const agentos_loop_config_t* manager);
static agentos_error_t create_loop_engines(agentos_core_loop_t* loop);
static void cleanup_loop_resources(agentos_core_loop_t* loop);

/* 默认配置 */
static void init_default_config(agentos_loop_config_t* manager) {
    memset(manager, 0, sizeof(agentos_loop_config_t));
    manager->loop_config_cognition_threads = 4;
    manager->loop_config_execution_threads = 8;
    manager->loop_config_memory_threads = 2;
    manager->loop_config_max_queued_tasks = 1000;
    manager->loop_config_stats_interval_ms = 60000;
}

/* ==================== 辅助函数实现 - 用于降低圈复杂度 ==================== */

/**
 * @brief 验证循环创建参数
 * @param manager 配置参数指针（可为 NULL）
 * @param out_loop 输出循环指针
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t validate_loop_parameters(const agentos_loop_config_t* manager, agentos_core_loop_t** out_loop)
{
    if (!out_loop) return AGENTOS_EINVAL;

    if (manager) {
        if (manager->loop_config_cognition_threads > 1024 ||
            manager->loop_config_execution_threads > 1024 ||
            manager->loop_config_memory_threads > 1024) {
            return AGENTOS_EINVAL;
        }

        if (manager->loop_config_max_queued_tasks == 0 ||
            manager->loop_config_max_queued_tasks > 100000) {
            return AGENTOS_EINVAL;
        }

        if (manager->loop_config_stats_interval_ms > 3600000) {
            return AGENTOS_EINVAL;
        }
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 分配循环内存
 * @return 分配的循环结构体指针，失败返回 NULL
 */
static agentos_core_loop_t* allocate_loop_memory(void)
{
    return (agentos_core_loop_t*)AGENTOS_CALLOC(1, sizeof(agentos_core_loop_t));
}

/**
 * @brief 初始化循环资源（互斥锁和条件变量）
 * @param loop 循环结构体指针
 * @param manager 配置参数指针（可为 NULL）
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t initialize_loop_resources(agentos_core_loop_t* loop, const agentos_loop_config_t* manager)
{
    if (manager) {
        memcpy(&loop->manager, manager, sizeof(agentos_loop_config_t));
    } else {
        init_default_config(&loop->manager);
    }

    loop->lock = agentos_mutex_create();
    if (!loop->lock) return AGENTOS_ENOMEM;

    loop->cond = agentos_cond_create();
    if (!loop->cond) {
        agentos_mutex_destroy(loop->lock);
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建循环引擎（认知、执行、记忆）
 * @param loop 循环结构体指针
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t create_loop_engines(agentos_core_loop_t* loop)
{
    agentos_error_t err;

    err = agentos_cognition_create_ex(
        NULL,
        loop->manager.loop_config_plan_strategy,
        loop->manager.loop_config_coord_strategy,
        loop->manager.loop_config_disp_strategy,
        &loop->cognition);

    if (err != AGENTOS_SUCCESS) return err;

    err = agentos_execution_create(
        loop->manager.loop_config_execution_threads > 0 ?
            loop->manager.loop_config_execution_threads : 8,
        &loop->execution);

    if (err != AGENTOS_SUCCESS) return err;

    err = agentos_memory_create(NULL, &loop->memory);
    if (err != AGENTOS_SUCCESS) return err;

    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理循环资源（反向释放所有资源）
 * @param loop 循环结构体指针
 */
static void cleanup_loop_resources(agentos_core_loop_t* loop)
{
    if (!loop) return;

    if (loop->memory) {
        agentos_memory_destroy(loop->memory);
        loop->memory = NULL;
    }

    if (loop->execution) {
        agentos_execution_destroy(loop->execution);
        loop->execution = NULL;
    }

    if (loop->cognition) {
        agentos_cognition_destroy(loop->cognition);
        loop->cognition = NULL;
    }

    if (loop->cond) {
        agentos_cond_destroy(loop->cond);
        loop->cond = NULL;
    }

    if (loop->lock) {
        agentos_mutex_destroy(loop->lock);
        loop->lock = NULL;
    }

    AGENTOS_FREE(loop);
}

/* ==================== 公共 API 函数实现 ==================== */

AGENTOS_API agentos_error_t agentos_loop_create(
    const agentos_loop_config_t* manager,
    agentos_core_loop_t** out_loop)
{
    agentos_error_t err;
    agentos_core_loop_t* loop = NULL;

    err = validate_loop_parameters(manager, out_loop);
    if (err != AGENTOS_SUCCESS) return err;

    loop = allocate_loop_memory();
    if (!loop) return AGENTOS_ENOMEM;

    err = initialize_loop_resources(loop, manager);
    if (err != AGENTOS_SUCCESS) {
        cleanup_loop_resources(loop);
        return err;
    }

    err = create_loop_engines(loop);
    if (err != AGENTOS_SUCCESS) {
        cleanup_loop_resources(loop);
        return err;
    }

    loop->running = 0;
    loop->stop_requested = 0;

    *out_loop = loop;
    return AGENTOS_SUCCESS;
}

AGENTOS_API void agentos_loop_destroy(agentos_core_loop_t* loop)
{
    if (!loop) return;

    if (loop->running) {
        agentos_loop_stop(loop);
    }

    if (loop->memory) {
        agentos_memory_destroy(loop->memory);
    }
    if (loop->execution) {
        agentos_execution_destroy(loop->execution);
    }
    if (loop->cognition) {
        agentos_cognition_destroy(loop->cognition);
    }
    if (loop->cond) {
        agentos_cond_destroy(loop->cond);
    }
    if (loop->lock) {
        agentos_mutex_destroy(loop->lock);
    }

    AGENTOS_FREE(loop);
}

AGENTOS_API agentos_error_t agentos_loop_run(agentos_core_loop_t* loop)
{
    if (!loop) return AGENTOS_EINVAL;

    agentos_mutex_lock(loop->lock);
    loop->running = 1;
    loop->stop_requested = 0;
    agentos_mutex_unlock(loop->lock);

    while (1) {
        agentos_mutex_lock(loop->lock);
        if (loop->stop_requested) {
            loop->running = 0;
            agentos_cond_broadcast(loop->cond);
            agentos_mutex_unlock(loop->lock);
            break;
        }
        agentos_mutex_unlock(loop->lock);

#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }

    return AGENTOS_SUCCESS;
}

AGENTOS_API void agentos_loop_stop(agentos_core_loop_t* loop)
{
    if (!loop) return;

    agentos_mutex_lock(loop->lock);
    loop->stop_requested = 1;
    while (loop->running) {
        agentos_cond_wait(loop->cond, loop->lock);
    }
    agentos_mutex_unlock(loop->lock);
}

AGENTOS_API agentos_error_t agentos_loop_submit(
    agentos_core_loop_t* loop,
    const char* input,
    size_t input_len,
    char** out_task_id)
{
    if (!loop || !input || !out_task_id) return AGENTOS_EINVAL;
    if (!loop->cognition || !loop->execution || !loop->memory) return AGENTOS_ENOTINIT;

    /* 步骤 1: 从记忆中检索相关上下文 */
    agentos_memory_t* memories = NULL;
    size_t memory_count = 0;
    agentos_error_t err = agentos_memory_retrieve(loop->memory, input, input_len, 5, &memories, &memory_count);
    
    /* 构建增强输入（如果有相关记忆） */
    char* enhanced_input = NULL;
    if (err == AGENTOS_SUCCESS && memory_count > 0) {
        /* 分配足够空间：原始输入 + 记忆内容 + 格式标记 */
        size_t total_len = input_len + 1024;
        for (size_t i = 0; i < memory_count; i++) {
            if (memories[i].content) {
                total_len += memories[i].content_len + 64;
            }
        }
        
        enhanced_input = (char*)AGENTOS_MALLOC(total_len);
        if (enhanced_input) {
            /* 构建带上下文的输入 */
            size_t pos = 0;
            pos += snprintf(enhanced_input + pos, total_len - pos, 
                "[上下文增强]\n相关记忆数量：%zu\n\n", memory_count);
            
            for (size_t i = 0; i < memory_count && i < 5; i++) {
                if (memories[i].content) {
                    pos += snprintf(enhanced_input + pos, total_len - pos,
                        "记忆 %zu: %.*s\n", i + 1, (int)memories[i].content_len, memories[i].content);
                }
            }
            
            pos += snprintf(enhanced_input + pos, total_len - pos,
                "\n[用户输入]\n%.*s", (int)input_len, input);
        }
        
        /* 无论enhanced_input是否分配成功，都释放记忆结果 */
        for (size_t i = 0; i < memory_count; i++) {
            if (memories[i].content) {
                AGENTOS_FREE(memories[i].content);
            }
        }
        AGENTOS_FREE(memories);
    }
    
    /* 步骤 2: 认知层处理（带上下文增强） */
    const char* process_input = enhanced_input ? enhanced_input : input;
    size_t process_len = enhanced_input ? strlen(enhanced_input) : input_len;
    
    agentos_task_plan_t* plan = NULL;
    err = agentos_cognition_process(loop->cognition, process_input, process_len, &plan);
    if (err != AGENTOS_SUCCESS) {
        if (enhanced_input) AGENTOS_FREE(enhanced_input);
        return err;
    }

    if (!plan || plan->task_plan_node_count == 0) {
        agentos_task_plan_free(plan);
        if (enhanced_input) AGENTOS_FREE(enhanced_input);
        return AGENTOS_EINVAL;
    }

    /* 步骤 3: 执行层提交任务 */
    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_input = (void*)process_input;
    task.task_timeout_ms = 30000;

    err = agentos_execution_submit(loop->execution, &task, out_task_id);
    
    /* 清理临时资源 */
    agentos_task_plan_free(plan);
    if (enhanced_input) AGENTOS_FREE(enhanced_input);

    return err;
}

AGENTOS_API agentos_error_t agentos_loop_wait(
    agentos_core_loop_t* loop,
    const char* task_id,
    uint32_t timeout_ms,
    char** out_result,
    size_t* out_result_len)
{
    if (!loop || !task_id || !out_result || !out_result_len) return AGENTOS_EINVAL;
    if (!loop->execution || !loop->memory) return AGENTOS_ENOTINIT;

    /* 等待执行完成 */
    agentos_task_t* result_task = NULL;
    agentos_error_t err = agentos_execution_wait(loop->execution, task_id, timeout_ms, &result_task);

    if (err == AGENTOS_SUCCESS && result_task) {
        /* 提取输出结果 */
        if (result_task->task_output) {
            size_t len = 0;
            const char* output = (const char*)result_task->task_output;
            while (output[len] != '\0') len++;
            *out_result = AGENTOS_STRDUP(output);
            *out_result_len = len;
        } else {
            *out_result = AGENTOS_STRDUP("");
            *out_result_len = 0;
        }
        
        /* 步骤 4: 将执行结果存储到记忆中（形成闭环） */
        if (*out_result && *out_result_len > 0) {
            agentos_error_t store_err = agentos_memory_store(
                loop->memory,
                *out_result,
                *out_result_len,
                0.7f
            );
            if (store_err != AGENTOS_SUCCESS) {
                AGENTOS_LOG_WARN("Failed to store execution result to memory: %d", store_err);
            } else {
                AGENTOS_LOG_INFO("Successfully stored execution result to memory");
            }
        }
        
        agentos_task_free(result_task);

        if (!*out_result) return AGENTOS_ENOMEM;
    }

    return err;
}

AGENTOS_API void agentos_loop_get_engines(
    agentos_core_loop_t* loop,
    agentos_cognition_engine_t** out_cognition,
    agentos_execution_engine_t** out_execution,
    agentos_memory_engine_t** out_memory)
{
    if (!loop) return;

    if (out_cognition) *out_cognition = loop->cognition;
    if (out_execution) *out_execution = loop->execution;
    if (out_memory) *out_memory = loop->memory;
}
