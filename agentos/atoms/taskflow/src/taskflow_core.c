// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file taskflow_core.c
 * @brief TaskFlow Core Implementation
 * 
 * TaskFlow 核心模块实现，提供引擎管理和基础功能。
 */

#include "taskflow.h"
#include "graph_engine.h"
#include "pregel_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#ifndef TASKFLOW_ERROR_NO_ACTIVE_VERTICES
#define TASKFLOW_ERROR_NO_ACTIVE_VERTICES ((taskflow_error_t)20)
#endif

#ifndef TASKFLOW_ERROR_ALREADY_RUNNING
#define TASKFLOW_ERROR_ALREADY_RUNNING ((taskflow_error_t)21)
#endif

typedef struct {
    vertex_id_t source;
    vertex_id_t target;
    void* payload;
    size_t payload_size;
} taskflow_message_t;

// ============================================================================
// 内部数据结构
// ============================================================================

typedef struct {
    uint64_t checkpoint_id;
    superstep_t superstep;
    size_t active_vertices;
    size_t message_count;
    uint64_t timestamp;
} checkpoint_entry_t;

struct taskflow_engine_s {
    taskflow_config_t config;
    graph_engine_handle_t graph_engine;
    pregel_engine_handle_t pregel_engine;
    bool initialized;
    bool running;
    bool paused;

    execution_stats_t stats;

    uint64_t last_checkpoint_id;
    checkpoint_entry_t checkpoints[64];
    size_t checkpoint_count;

    pthread_mutex_t engine_mutex;
    pthread_cond_t pause_cond;
    pthread_t worker_thread;
    bool worker_active;

    taskflow_message_t* message_queue;
    size_t message_count;
    size_t message_capacity;
    pthread_t async_thread;
    int async_thread_active;
    bool async_cancel_requested;
    pthread_cond_t async_complete_cond;
    taskflow_error_t async_result;

    taskflow_graph_handle_t async_graph;
    size_t async_max_supersteps;
    void (*async_callback)(taskflow_error_t result, void* user_data);
    void* async_user_data;

    void (*log_callback)(const char* message, void* user_data);
    void* log_user_data;
};

struct taskflow_graph_s {
    taskflow_handle_t engine;
    graph_engine_handle_t graph_engine;
    size_t vertex_count;
    size_t edge_count;
};

struct taskflow_partition_s {
    graph_partition_t partition;
    taskflow_graph_handle_t graph;
};

// ============================================================================
// 全局状态
// ============================================================================

static void (*g_taskflow_log_callback)(const char*, void*) = NULL;
static void* g_taskflow_log_user_data = NULL;

// ============================================================================
// 静态函数声明
// ============================================================================

static void taskflow_engine_init_defaults(taskflow_config_t* config);
static taskflow_error_t taskflow_engine_validate_config(const taskflow_config_t* config);
static taskflow_error_t taskflow_engine_stop_core(taskflow_handle_t engine);

// ============================================================================
// 核心API实现 (内部Pregel引擎接口)
// ============================================================================

taskflow_handle_t taskflow_engine_create_core(const taskflow_config_t* config)
{
    if (!config) {
        return NULL;
    }
    
    // 验证配置
    taskflow_error_t valid = taskflow_engine_validate_config(config);
    if (valid != TASKFLOW_SUCCESS) {
        return NULL;
    }
    
    // 分配引擎结构
    struct taskflow_engine_s* engine = (struct taskflow_engine_s*)calloc(1, sizeof(struct taskflow_engine_s));
    if (!engine) {
        return NULL;
    }
    
    // 复制配置
    engine->config = *config;
    
    // 初始化默认值
    taskflow_engine_init_defaults(&engine->config);
    
    // 创建图引擎
    engine->graph_engine = graph_engine_create(config);
    if (!engine->graph_engine) {
        free(engine);
        return NULL;
    }
    
    // 初始化统计信息
    memset(&engine->stats, 0, sizeof(engine->stats));
    
    engine->initialized = false;
    engine->running = false;
    engine->paused = false;
    engine->worker_active = false;
    engine->last_checkpoint_id = 0;

    // 初始化同步原语
    pthread_mutex_init(&engine->engine_mutex, NULL);
    pthread_cond_init(&engine->pause_cond, NULL);
    pthread_cond_init(&engine->async_complete_cond, NULL);

    // 初始化异步执行状态
    engine->async_thread_active = 0;
    engine->async_cancel_requested = false;
    engine->async_result = TASKFLOW_SUCCESS;
    engine->async_graph = NULL;
    engine->async_max_supersteps = 0;
    engine->async_callback = NULL;
    engine->async_user_data = NULL;

    // 初始化消息队列
    engine->message_capacity = 256;
    engine->message_queue = (taskflow_message_t*)calloc(engine->message_capacity, sizeof(taskflow_message_t));
    engine->message_count = 0;

    // 日志回调默认为空
    engine->log_callback = NULL;
    engine->log_user_data = NULL;
    
    return (taskflow_handle_t)engine;
}

void taskflow_engine_destroy_core(taskflow_handle_t engine)
{
    if (!engine) return;
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    // 停止引擎（如果正在运行）
    if (e->running) {
        taskflow_engine_stop_core(engine);
    }
    
    // 销毁图引擎
    if (e->graph_engine) {
        graph_engine_destroy(e->graph_engine);
    }
    
    // 销毁Pregel引擎
    if (e->pregel_engine) {
        pregel_engine_destroy(e->pregel_engine);
    }

    // 清理消息队列
    if (e->message_queue) {
        for (size_t i = 0; i < e->message_count; i++) {
            if (e->message_queue[i].payload) free(e->message_queue[i].payload);
        }
        free(e->message_queue);
    }

    // 销毁同步原语
    pthread_mutex_destroy(&e->engine_mutex);
    pthread_cond_destroy(&e->pause_cond);
    pthread_cond_destroy(&e->async_complete_cond);

    free(e);
}

taskflow_error_t taskflow_engine_init(taskflow_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    if (e->initialized) {
        return TASKFLOW_ERROR_ALREADY_INITIALIZED;
    }
    
    // 初始化图引擎
    taskflow_error_t result = graph_engine_init(e->graph_engine);
    if (result != TASKFLOW_SUCCESS) {
        return result;
    }
    
    // 如果启用了计算功能，创建Pregel引擎
    if (e->config.compute_func) {
        // 创建Pregel配置
        pregel_config_t pregel_config;
        memset(&pregel_config, 0, sizeof(pregel_config));
        
        pregel_config.max_workers = e->config.worker_threads;
        pregel_config.message_buffer_size = e->config.message_buffer_size;
        pregel_config.superstep_timeout_ms = e->config.superstep_timeout_ms;
        pregel_config.compute_func = (pregel_compute_func_t)e->config.compute_func;
        pregel_config.send_func = (pregel_send_func_t)e->config.send_func;
        pregel_config.user_context = e->config.user_context;
        pregel_config.enable_fault_tolerance = e->config.enable_fault_tolerance;
        pregel_config.checkpoint_interval = e->config.checkpoint_interval;
        pregel_config.enable_message_combining = e->config.enable_message_combining;
        pregel_config.enable_edge_caching = e->config.enable_edge_caching;
        pregel_config.batch_size = e->config.batch_size;
        
        e->pregel_engine = pregel_engine_create(&pregel_config);
        if (!e->pregel_engine) {
            return TASKFLOW_ERROR_INTERNAL;
        }
        
        // 初始化Pregel引擎
        result = pregel_engine_init(e->pregel_engine, e->graph_engine);
        if (result != TASKFLOW_SUCCESS) {
            pregel_engine_destroy(e->pregel_engine);
            e->pregel_engine = NULL;
            return result;
        }
    }
    
    e->initialized = true;
    return TASKFLOW_SUCCESS;
}

static void* engine_worker_thread(void* arg) {
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)arg;
    if (!e) return NULL;

    while (e->worker_active) {
        pthread_mutex_lock(&e->engine_mutex);

        while (e->paused && e->worker_active) {
            pthread_cond_wait(&e->pause_cond, &e->engine_mutex);
        }

        if (!e->worker_active) {
            pthread_mutex_unlock(&e->engine_mutex);
            break;
        }

        if (e->pregel_engine) {
            taskflow_error_t err = pregel_engine_run_superstep(e->pregel_engine);
            if (err == TASKFLOW_ERROR_NO_ACTIVE_VERTICES || err == TASKFLOW_ERROR_NOT_INITIALIZED) {
                pthread_mutex_unlock(&e->engine_mutex);
                break;
            }
        }

        pthread_mutex_unlock(&e->engine_mutex);

        struct timespec ts = {0, 10000000L};
        nanosleep(&ts, NULL);
    }

    return NULL;
}

taskflow_error_t taskflow_engine_start_core(taskflow_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    if (e->running) {
        return TASKFLOW_SUCCESS;
    }
    
    pthread_mutex_lock(&e->engine_mutex);
    e->paused = false;
    e->worker_active = true;
    pthread_mutex_unlock(&e->engine_mutex);

    int ret = pthread_create(&e->worker_thread, NULL, engine_worker_thread, e);
    if (ret != 0) {
        e->worker_active = false;
        return TASKFLOW_ERROR_INTERNAL;
    }

    e->running = true;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_engine_stop_core(taskflow_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    if (!e->running) {
        return TASKFLOW_SUCCESS; // 已经停止
    }
    
    // 停止Pregel引擎（如果存在）
    if (e->pregel_engine) {
        pregel_engine_stop(e->pregel_engine);
    }
    
    pthread_mutex_lock(&e->engine_mutex);
    e->worker_active = false;
    e->paused = false;
    pthread_cond_broadcast(&e->pause_cond);
    pthread_mutex_unlock(&e->engine_mutex);

    pthread_join(e->worker_thread, NULL);
    
    e->running = false;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_engine_pause_core(taskflow_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    if (!e->running) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 暂停Pregel引擎（如果存在）
    if (e->pregel_engine) {
        return pregel_engine_pause(e->pregel_engine);
    }
    
    // 暂停其他组件（消息处理等）
    pthread_mutex_lock(&e->engine_mutex);
    e->paused = true;
    pthread_mutex_unlock(&e->engine_mutex);
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_engine_resume_core(taskflow_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    if (!e->running) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 恢复Pregel引擎（如果存在）
    if (e->pregel_engine) {
        return pregel_engine_resume(e->pregel_engine);
    }
    
    // 恢复其他组件（消息处理等）
    pthread_mutex_lock(&e->engine_mutex);
    e->paused = false;
    pthread_cond_broadcast(&e->pause_cond);
    pthread_mutex_unlock(&e->engine_mutex);
    
    return TASKFLOW_SUCCESS;
}

// ============================================================================
// 图管理API实现
// ============================================================================

taskflow_graph_handle_t taskflow_graph_create(taskflow_handle_t engine)
{
    if (!engine) {
        return NULL;
    }
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    struct taskflow_graph_s* graph = (struct taskflow_graph_s*)calloc(1, sizeof(struct taskflow_graph_s));
    if (!graph) {
        return NULL;
    }
    
    graph->engine = engine;
    graph->graph_engine = e->graph_engine;
    graph->vertex_count = 0;
    graph->edge_count = 0;
    
    return (taskflow_graph_handle_t)graph;
}

void taskflow_graph_destroy(taskflow_graph_handle_t graph)
{
    if (!graph) return;
    
    struct taskflow_graph_s* g = (struct taskflow_graph_s*)graph;
    free(g);
}

taskflow_error_t taskflow_graph_add_vertex(taskflow_graph_handle_t graph, const graph_vertex_t* vertex)
{
    if (!graph || !vertex) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_graph_s* g = (struct taskflow_graph_s*)graph;
    
    // 验证顶点ID
    if (vertex->id == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 添加到图引擎
    taskflow_error_t result = graph_engine_add_vertex(g->graph_engine, vertex);
    if (result == TASKFLOW_SUCCESS) {
        g->vertex_count++;
    }
    
    return result;
}

taskflow_error_t taskflow_graph_remove_vertex(taskflow_graph_handle_t graph, vertex_id_t vertex_id)
{
    if (!graph || vertex_id == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_graph_s* g = (struct taskflow_graph_s*)graph;
    
    taskflow_error_t result = graph_engine_remove_vertex(g->graph_engine, vertex_id);
    if (result == TASKFLOW_SUCCESS && g->vertex_count > 0) {
        g->vertex_count--;
    }
    
    return result;
}

taskflow_error_t taskflow_graph_add_edge(taskflow_graph_handle_t graph, const graph_edge_t* edge)
{
    if (!graph || !edge) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_graph_s* g = (struct taskflow_graph_s*)graph;
    
    // 验证边ID和顶点ID
    if (edge->id == 0 || edge->source == 0 || edge->target == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    taskflow_error_t result = graph_engine_add_edge(g->graph_engine, edge);
    if (result == TASKFLOW_SUCCESS) {
        g->edge_count++;
    }
    
    return result;
}

taskflow_error_t taskflow_graph_remove_edge(taskflow_graph_handle_t graph, edge_id_t edge_id)
{
    if (!graph || edge_id == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_graph_s* g = (struct taskflow_graph_s*)graph;
    
    taskflow_error_t result = graph_engine_remove_edge(g->graph_engine, edge_id);
    if (result == TASKFLOW_SUCCESS && g->edge_count > 0) {
        g->edge_count--;
    }
    
    return result;
}

size_t taskflow_graph_get_vertex_count(taskflow_graph_handle_t graph)
{
    if (!graph) return 0;
    
    struct taskflow_graph_s* g = (struct taskflow_graph_s*)graph;
    return g->vertex_count;
}

size_t taskflow_graph_get_edge_count(taskflow_graph_handle_t graph)
{
    if (!graph) return 0;
    
    struct taskflow_graph_s* g = (struct taskflow_graph_s*)graph;
    return g->edge_count;
}

// ============================================================================
// 其他API实现（存根）
// ============================================================================

taskflow_error_t taskflow_execute_sync(taskflow_handle_t engine,
                                      taskflow_graph_handle_t graph,
                                      size_t max_supersteps)
{
    if (!engine || !graph) return TASKFLOW_ERROR_INVALID_ARG;
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    if (!e->running || !e->initialized) return TASKFLOW_ERROR_NOT_INITIALIZED;

    size_t vertex_count = 0;
    graph_engine_get_stats(e->graph_engine, &vertex_count, NULL, NULL, NULL);

    for (size_t step = 0; step < max_supersteps || max_supersteps == 0; step++) {
        pthread_mutex_lock(&e->engine_mutex);
        while (e->paused) {
            pthread_cond_wait(&e->pause_cond, &e->engine_mutex);
        }
        bool still_running = e->running && e->worker_active;
        pthread_mutex_unlock(&e->engine_mutex);
        if (!still_running) break;

        if (e->pregel_engine) {
            taskflow_error_t perr = pregel_engine_run_superstep(e->pregel_engine);
            if (perr != TASKFLOW_SUCCESS) {
                if (perr == TASKFLOW_ERROR_NO_ACTIVE_VERTICES) break;
            }
        } else if (e->graph_engine && vertex_count > 0) {
            for (size_t v = 0; v < vertex_count; v++) {
                if (e->config.compute_func) {
                    e->config.compute_func((vertex_id_t)v, NULL, 0, NULL, 0, e->config.user_context);
                }
            }
        }

        e->stats.completed_supersteps++;
        if (max_supersteps > 0 && step >= max_supersteps - 1) break;
    }

    return TASKFLOW_SUCCESS;
}

// ============================================================================
// 异步执行内部实现
// ============================================================================

static void* async_execute_worker(void* arg) {
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)arg;
    if (!e) return NULL;

    // 执行同步计算
    e->async_result = taskflow_execute_sync(e, e->async_graph, e->async_max_supersteps);

    // 通知完成
    pthread_mutex_lock(&e->engine_mutex);
    e->async_thread_active = 0;
    pthread_cond_broadcast(&e->async_complete_cond);
    pthread_mutex_unlock(&e->engine_mutex);

    // 调用用户回调
    if (e->async_callback) {
        e->async_callback(e->async_result, e->async_user_data);
    }

    return NULL;
}

taskflow_error_t taskflow_execute_async(taskflow_handle_t engine,
                                       taskflow_graph_handle_t graph,
                                       size_t max_supersteps,
                                       void (*callback)(taskflow_error_t result, void* user_data),
                                       void* user_data)
{
    if (!engine || !graph) return TASKFLOW_ERROR_INVALID_ARG;
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    if (!e->running || !e->initialized) return TASKFLOW_ERROR_NOT_INITIALIZED;

    pthread_mutex_lock(&e->engine_mutex);
    if (e->async_thread_active) {
        pthread_mutex_unlock(&e->engine_mutex);
        return TASKFLOW_ERROR_ALREADY_RUNNING;
    }

    // 保存异步执行参数
    e->async_cancel_requested = false;
    e->async_graph = graph;
    e->async_max_supersteps = max_supersteps;
    e->async_callback = callback;
    e->async_user_data = user_data;
    e->async_thread_active = 1;

    // 创建异步工作线程
    int ret = pthread_create(&e->async_thread, NULL, async_execute_worker, e);
    if (ret != 0) {
        e->async_thread_active = 0;
        pthread_mutex_unlock(&e->engine_mutex);
        return TASKFLOW_ERROR_INTERNAL;
    }
    pthread_mutex_unlock(&e->engine_mutex);

    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_execute_cancel(taskflow_handle_t engine)
{
    if (!engine) return TASKFLOW_ERROR_INVALID_ARG;
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    pthread_mutex_lock(&e->engine_mutex);
    if (!e->async_thread_active) {
        pthread_mutex_unlock(&e->engine_mutex);
        return TASKFLOW_SUCCESS;
    }
    e->async_cancel_requested = true;
    pthread_mutex_unlock(&e->engine_mutex);

    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_execute_wait(taskflow_handle_t engine, uint32_t timeout_ms)
{
    if (!engine) return TASKFLOW_ERROR_INVALID_ARG;
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    pthread_mutex_lock(&e->engine_mutex);
    if (!e->async_thread_active) {
        pthread_mutex_unlock(&e->engine_mutex);
        return TASKFLOW_SUCCESS;
    }

    if (timeout_ms == 0 || timeout_ms == UINT32_MAX) {
        while (e->async_thread_active) {
            pthread_cond_wait(&e->async_complete_cond, &e->engine_mutex);
        }
        pthread_mutex_unlock(&e->engine_mutex);
        return e->async_result;
    }

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    uint64_t extra_ns = (uint64_t)timeout_ms * 1000000ULL;
    deadline.tv_sec += (time_t)(extra_ns / 1000000000ULL);
    deadline.tv_nsec += (long)(extra_ns % 1000000000ULL);
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    while (e->async_thread_active) {
        int ret = pthread_cond_timedwait(&e->async_complete_cond, &e->engine_mutex, &deadline);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&e->engine_mutex);
            return TASKFLOW_ERROR_TIMEOUT;
        }
    }

    taskflow_error_t result = e->async_result;
    pthread_mutex_unlock(&e->engine_mutex);
    return result;
}

taskflow_error_t taskflow_send_message(taskflow_handle_t engine,
                                      vertex_id_t source,
                                      vertex_id_t target,
                                      const void* payload,
                                      size_t payload_size)
{
    if (!engine || source == 0 || target == 0 || !payload || payload_size == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    if (e->pregel_engine) {
        return pregel_engine_send_message(e->pregel_engine, source, target, payload, payload_size);
    }

    pthread_mutex_lock(&e->engine_mutex);

    if (e->message_count >= e->message_capacity) {
        pthread_mutex_unlock(&e->engine_mutex);
        return TASKFLOW_ERROR_GRAPH_TOO_LARGE;
    }

    taskflow_message_t* msg = &e->message_queue[e->message_count];
    msg->source = source;
    msg->target = target;
    msg->payload_size = payload_size;
    msg->payload = malloc(payload_size);
    if (!msg->payload) {
        pthread_mutex_unlock(&e->engine_mutex);
        return TASKFLOW_ERROR_MEMORY;
    }
    memcpy(msg->payload, payload, payload_size);
    e->message_count++;
    e->stats.total_messages++;

    pthread_mutex_unlock(&e->engine_mutex);
    return TASKFLOW_SUCCESS;
}

superstep_t taskflow_get_current_superstep(taskflow_handle_t engine)
{
    if (!engine) return 0;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    if (e->pregel_engine) {
        return pregel_engine_get_current_superstep(e->pregel_engine);
    }

    return 0;
}

// ============================================================================
// 消息传递API实现
// ============================================================================

taskflow_error_t taskflow_broadcast_message(taskflow_handle_t engine,
                                           vertex_id_t source,
                                           const void* payload,
                                           size_t payload_size)
{
    if (!engine || source == 0 || !payload || payload_size == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    if (e->pregel_engine) {
        return pregel_engine_broadcast_message(e->pregel_engine, source, payload, payload_size);
    }

    // 无Pregel引擎时遍历图引擎顶点广播
    size_t vertex_count = 0;
    graph_engine_get_stats(e->graph_engine, &vertex_count, NULL, NULL, NULL);
    for (size_t i = 1; i <= vertex_count; i++) {
        if ((vertex_id_t)i != source) {
            taskflow_send_message(engine, source, (vertex_id_t)i, payload, payload_size);
        }
    }
    return TASKFLOW_SUCCESS;
}

size_t taskflow_get_incoming_messages(taskflow_handle_t engine,
                                     vertex_id_t vertex_id,
                                     graph_message_t* messages,
                                     size_t max_count)
{
    if (!engine || vertex_id == 0 || !messages || max_count == 0) return 0;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    size_t found = 0;

    pthread_mutex_lock(&e->engine_mutex);
    for (size_t i = 0; i < e->message_count && found < max_count; i++) {
        if (e->message_queue[i].target == vertex_id) {
            messages[found].id = (message_id_t)(i + 1);
            messages[found].sender = e->message_queue[i].source;
            messages[found].receiver = vertex_id;
            messages[found].payload = e->message_queue[i].payload;
            messages[found].payload_size = e->message_queue[i].payload_size;
            messages[found].step = 0;
            messages[found].direction = MESSAGE_INCOMING;
            found++;
        }
    }
    pthread_mutex_unlock(&e->engine_mutex);

    return found;
}

taskflow_error_t taskflow_clear_messages(taskflow_handle_t engine, vertex_id_t vertex_id)
{
    if (!engine || vertex_id == 0) return TASKFLOW_ERROR_INVALID_ARG;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    pthread_mutex_lock(&e->engine_mutex);
    size_t write_idx = 0;
    for (size_t i = 0; i < e->message_count; i++) {
        if (e->message_queue[i].target == vertex_id) {
            if (e->message_queue[i].payload) free(e->message_queue[i].payload);
        } else {
            if (write_idx != i) {
                e->message_queue[write_idx] = e->message_queue[i];
            }
            write_idx++;
        }
    }
    e->message_count = write_idx;
    pthread_mutex_unlock(&e->engine_mutex);

    return TASKFLOW_SUCCESS;
}

// ============================================================================
// 检查点与容错API实现
// ============================================================================

uint64_t taskflow_create_checkpoint(taskflow_handle_t engine)
{
    if (!engine) return 0;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    if (e->pregel_engine) {
        uint64_t cp_id = pregel_engine_create_checkpoint(e->pregel_engine);
        if (cp_id > 0) {
            e->last_checkpoint_id = cp_id;
            e->stats.checkpoints_taken++;
            if (e->checkpoint_count < 64) {
                checkpoint_entry_t* cp = &e->checkpoints[e->checkpoint_count];
                cp->checkpoint_id = cp_id;
                cp->superstep = pregel_engine_get_current_superstep(e->pregel_engine);
                cp->active_vertices = pregel_engine_get_active_vertices(e->pregel_engine);
                cp->message_count = pregel_engine_get_queued_messages(e->pregel_engine);
                cp->timestamp = (uint64_t)time(NULL);
                e->checkpoint_count++;
            }
        }
        return cp_id;
    }

    e->last_checkpoint_id++;
    e->stats.checkpoints_taken++;
    if (e->checkpoint_count < 64) {
        checkpoint_entry_t* cp = &e->checkpoints[e->checkpoint_count];
        cp->checkpoint_id = e->last_checkpoint_id;
        cp->superstep = 0;
        cp->active_vertices = 0;
        cp->message_count = e->message_count;
        cp->timestamp = (uint64_t)time(NULL);
        e->checkpoint_count++;
    }
    return e->last_checkpoint_id;
}

taskflow_error_t taskflow_restore_checkpoint(taskflow_handle_t engine, uint64_t checkpoint_id)
{
    if (!engine || checkpoint_id == 0) return TASKFLOW_ERROR_INVALID_ARG;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    if (e->pregel_engine) {
        return pregel_engine_restore_checkpoint(e->pregel_engine, checkpoint_id);
    }

    return TASKFLOW_ERROR_INVALID_ARG;
}

taskflow_error_t taskflow_delete_checkpoint(taskflow_handle_t engine, uint64_t checkpoint_id)
{
    if (!engine || checkpoint_id == 0) return TASKFLOW_ERROR_INVALID_ARG;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    if (e->pregel_engine) {
        taskflow_error_t err = pregel_engine_restore_checkpoint(e->pregel_engine, checkpoint_id);
        if (err != TASKFLOW_SUCCESS) return err;
    }

    for (size_t i = 0; i < e->checkpoint_count; i++) {
        if (e->checkpoints[i].checkpoint_id == checkpoint_id) {
            memmove(&e->checkpoints[i], &e->checkpoints[i + 1],
                    (e->checkpoint_count - i - 1) * sizeof(checkpoint_entry_t));
            e->checkpoint_count--;
            return TASKFLOW_SUCCESS;
        }
    }

    return TASKFLOW_ERROR_INVALID_ARG;
}

size_t taskflow_list_checkpoints(taskflow_handle_t engine,
                                uint64_t* checkpoints,
                                size_t max_count)
{
    if (!engine || !checkpoints || max_count == 0) return 0;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    size_t fill = e->checkpoint_count < max_count ? e->checkpoint_count : max_count;
    for (size_t i = 0; i < fill; i++) {
        checkpoints[i] = e->checkpoints[i].checkpoint_id;
    }
    return fill;
}

// ============================================================================
// 统计与监控API实现
// ============================================================================

taskflow_error_t taskflow_get_stats(taskflow_handle_t engine, execution_stats_t* stats)
{
    if (!engine || !stats) return TASKFLOW_ERROR_INVALID_ARG;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    *stats = e->stats;

    if (e->pregel_engine) {
        pregel_engine_get_stats(e->pregel_engine, stats);
    }

    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_reset_stats(taskflow_handle_t engine)
{
    if (!engine) return TASKFLOW_ERROR_INVALID_ARG;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    memset(&e->stats, 0, sizeof(e->stats));

    if (e->pregel_engine) {
        pregel_engine_reset_stats(e->pregel_engine);
    }

    return TASKFLOW_SUCCESS;
}

size_t taskflow_get_active_vertex_count(taskflow_handle_t engine)
{
    if (!engine) return 0;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    if (e->pregel_engine) {
        return pregel_engine_get_active_vertices(e->pregel_engine);
    }

    size_t vertex_count = 0;
    graph_engine_get_stats(e->graph_engine, &vertex_count, NULL, NULL, NULL);
    return vertex_count;
}

size_t taskflow_get_queued_message_count(taskflow_handle_t engine)
{
    if (!engine) return 0;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;

    if (e->pregel_engine) {
        return pregel_engine_get_queued_messages(e->pregel_engine);
    }

    return e->message_count;
}

// ============================================================================
// 工具函数实现
// ============================================================================

const char* taskflow_error_to_string(taskflow_error_t error)
{
    static const char* error_strings[] = {
        "Success",
        "Invalid argument",
        "Memory allocation failed",
        "Not initialized",
        "Already initialized",
        "Graph too large",
        "Partition error",
        "Checkpoint error",
        "Timeout",
        "Fault detected",
        "Communication error",
        "Internal error"
    };
    
    if (error < 0 || error > TASKFLOW_ERROR_INTERNAL) {
        return "Unknown error";
    }
    
    return error_strings[error];
}

const char* taskflow_get_version(void)
{
    return "1.0.0";
}

taskflow_error_t taskflow_graph_partition(taskflow_graph_handle_t graph,
                                         partition_strategy_t strategy,
                                         size_t partition_count) {
    if (!graph || partition_count == 0) return TASKFLOW_ERROR_INVALID_ARG;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)graph;
    if (!e->graph_engine) return TASKFLOW_ERROR_NOT_INITIALIZED;

    e->config.partition_count = partition_count;
    e->config.partition_strategy = strategy;

    size_t vertex_count = 0;
    graph_engine_get_stats(e->graph_engine, &vertex_count, NULL, NULL, NULL);

    if (vertex_count == 0) return TASKFLOW_SUCCESS;

    if (strategy == PARTITION_HASH) {
        for (size_t i = 0; i < vertex_count; i++) {
            vertex_id_t vid = (vertex_id_t)i;
            size_t partition = vid % partition_count;
            (void)partition;
        }
    } else if (strategy == PARTITION_RANGE) {
        size_t range_size = vertex_count / partition_count;
        if (range_size == 0) range_size = 1;
        for (size_t i = 0; i < vertex_count; i++) {
            size_t partition = i / range_size;
            if (partition >= partition_count) partition = partition_count - 1;
            (void)partition;
        }
    }

    return TASKFLOW_SUCCESS;
}

size_t taskflow_graph_get_partitions(taskflow_graph_handle_t graph,
                                    taskflow_partition_handle_t* partitions,
                                    size_t max_count) {
    if (!graph) return 0;

    struct taskflow_engine_s* e = (struct taskflow_engine_s*)graph;
    size_t count = e->config.partition_count;
    if (count == 0) count = 1;

    if (partitions && max_count > 0) {
        size_t fill = count < max_count ? count : max_count;
        for (size_t i = 0; i < fill; i++) {
            partitions[i] = (taskflow_partition_handle_t)(uintptr_t)(i + 1);
        }
    }

    return count;
}

void taskflow_set_log_callback(void (*callback)(const char* message, void* user_data),
                              void* user_data)
{
    // 全局日志回调暂存（线程安全版本需使用读写锁）
    g_taskflow_log_callback = callback;
    g_taskflow_log_user_data = user_data;
}

// ============================================================================
// 静态函数实现
// ============================================================================

static void taskflow_engine_init_defaults(taskflow_config_t* config)
{
    if (!config) return;
    
    // 设置默认值
    if (config->max_vertices == 0) {
        config->max_vertices = 1000000; // 默认100万顶点
    }
    
    if (config->max_edges == 0) {
        config->max_edges = 5000000; // 默认500万边
    }
    
    if (config->worker_threads == 0) {
        config->worker_threads = 4; // 默认4个工作线程
    }
    
    if (config->partition_count == 0) {
        config->partition_count = 1; // 默认1个分区
    }
    
    if (config->max_supersteps == 0) {
        config->max_supersteps = 100; // 默认100个超步
    }
    
    if (config->superstep_timeout_ms == 0) {
        config->superstep_timeout_ms = 30000; // 默认30秒超时
    }
    
    if (config->checkpoint_interval == 0) {
        config->checkpoint_interval = 10; // 默认每10个超步一个检查点
    }
    
    if (config->message_buffer_size == 0) {
        config->message_buffer_size = 1024 * 1024; // 默认1MB消息缓冲区
    }
    
    if (config->batch_size == 0) {
        config->batch_size = 1000; // 默认批处理大小1000
    }
}

static taskflow_error_t taskflow_engine_validate_config(const taskflow_config_t* config)
{
    if (!config) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 检查必要的配置项
    if (config->max_vertices == 0 || config->max_edges == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 检查分区策略
    if (config->partition_strategy < PARTITION_HASH || config->partition_strategy > PARTITION_CUSTOM) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    if (config->compute_func == NULL && config->partition_strategy != PARTITION_CUSTOM) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    return TASKFLOW_SUCCESS;
}