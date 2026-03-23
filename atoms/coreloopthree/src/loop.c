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
    agentos_loop_config_t config;
    volatile int running;
    volatile int stop_requested;
    agentos_mutex_t* lock;
    agentos_cond_t* cond;
};

/* 默认配置 */
static void init_default_config(agentos_loop_config_t* config) {
    memset(config, 0, sizeof(agentos_loop_config_t));
    config->loop_config_cognition_threads = 4;
    config->loop_config_execution_threads = 8;
    config->loop_config_memory_threads = 2;
    config->loop_config_max_queued_tasks = 1000;
    config->loop_config_stats_interval_ms = 60000;
}

AGENTOS_API agentos_error_t agentos_loop_create(
    const agentos_loop_config_t* config,
    agentos_core_loop_t** out_loop)
{
    if (!out_loop) return AGENTOS_EINVAL;
    
    agentos_core_loop_t* loop = (agentos_core_loop_t*)calloc(1, sizeof(agentos_core_loop_t));
    if (!loop) return AGENTOS_ENOMEM;
    
    if (config) {
        memcpy(&loop->config, config, sizeof(agentos_loop_config_t));
    } else {
        init_default_config(&loop->config);
    }
    
    loop->lock = agentos_mutex_create();
    if (!loop->lock) {
        free(loop);
        return AGENTOS_ENOMEM;
    }
    
    loop->cond = agentos_cond_create();
    if (!loop->cond) {
        agentos_mutex_destroy(loop->lock);
        free(loop);
        return AGENTOS_ENOMEM;
    }
    
    agentos_error_t err = agentos_cognition_create_ex(
        NULL,
        loop->config.loop_config_plan_strategy,
        loop->config.loop_config_coord_strategy,
        loop->config.loop_config_disp_strategy,
        &loop->cognition);
    
    if (err != AGENTOS_SUCCESS) {
        agentos_cond_destroy(loop->cond);
        agentos_mutex_destroy(loop->lock);
        free(loop);
        return err;
    }
    
    err = agentos_execution_create(
        loop->config.loop_config_execution_threads > 0 ? 
            loop->config.loop_config_execution_threads : 8,
        &loop->execution);
    
    if (err != AGENTOS_SUCCESS) {
        agentos_cognition_destroy(loop->cognition);
        agentos_cond_destroy(loop->cond);
        agentos_mutex_destroy(loop->lock);
        free(loop);
        return err;
    }
    
    err = agentos_memory_create(NULL, &loop->memory);
    if (err != AGENTOS_SUCCESS) {
        agentos_execution_destroy(loop->execution);
        agentos_cognition_destroy(loop->cognition);
        agentos_cond_destroy(loop->cond);
        agentos_mutex_destroy(loop->lock);
        free(loop);
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
    
    free(loop);
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
    if (!loop->cognition || !loop->execution) return AGENTOS_ENOTINIT;
    
    agentos_task_plan_t* plan = NULL;
    agentos_error_t err = agentos_cognition_process(loop->cognition, input, input_len, &plan);
    if (err != AGENTOS_SUCCESS) return err;
    
    if (!plan || plan->task_plan_node_count == 0) {
        agentos_task_plan_free(plan);
        return AGENTOS_EINVAL;
    }
    
    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_input = (void*)input;
    task.task_timeout_ms = 30000;
    
    err = agentos_execution_submit(loop->execution, &task, out_task_id);
    agentos_task_plan_free(plan);
    
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
    if (!loop->execution) return AGENTOS_ENOTINIT;
    
    agentos_task_t* result_task = NULL;
    agentos_error_t err = agentos_execution_wait(loop->execution, task_id, timeout_ms, &result_task);
    
    if (err == AGENTOS_SUCCESS && result_task) {
        if (result_task->task_output) {
            size_t len = 0;
            const char* output = (const char*)result_task->task_output;
            while (output[len] != '\0') len++;
            *out_result = strdup(output);
            *out_result_len = len;
        } else {
            *out_result = strdup("");
            *out_result_len = 0;
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
