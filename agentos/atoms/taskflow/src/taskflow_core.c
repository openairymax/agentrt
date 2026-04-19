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

// ============================================================================
// 内部数据结构
// ============================================================================

struct taskflow_engine_s {
    taskflow_config_t config;
    graph_engine_handle_t graph_engine;
    pregel_engine_handle_t pregel_engine;
    bool initialized;
    bool running;
    bool paused;

    // 统计信息
    execution_stats_t stats;

    // 检查点管理
    uint64_t last_checkpoint_id;

    // 同步原语与线程管理
    pthread_mutex_t engine_mutex;
    pthread_cond_t pause_cond;
    pthread_t worker_thread;
    bool worker_active;

    // 消息队列（基础实现）
    typedef struct {
        vertex_id_t source;
        vertex_id_t target;
        void* payload;
        size_t payload_size;
    } taskflow_message_t;
    taskflow_message_t* message_queue;
    size_t message_count;
    size_t message_capacity;

    // 日志回调
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

// ============================================================================
// 核心API实现
// ============================================================================

taskflow_handle_t taskflow_engine_create(const taskflow_config_t* config)
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

    // 初始化消息队列
    engine->message_capacity = 256;
    engine->message_queue = (taskflow_message_t*)calloc(engine->message_capacity, sizeof(taskflow_message_t));
    engine->message_count = 0;

    // 日志回调默认为空
    engine->log_callback = NULL;
    engine->log_user_data = NULL;
    
    return (taskflow_handle_t)engine;
}

void taskflow_engine_destroy(taskflow_handle_t engine)
{
    if (!engine) return;
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    // 停止引擎（如果正在运行）
    if (e->running) {
        taskflow_engine_stop(engine);
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

taskflow_error_t taskflow_engine_start(taskflow_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    if (e->running) {
        return TASKFLOW_SUCCESS; // 已经在运行
    }
    
    // TODO: 启动工作线程和消息处理
    pthread_mutex_lock(&e->engine_mutex);
    e->paused = false;
    e->worker_active = true;
    pthread_mutex_unlock(&e->engine_mutex);
    
    e->running = true;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_engine_stop(taskflow_handle_t engine)
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
    
    // TODO: 停止工作线程和清理资源
    pthread_mutex_lock(&e->engine_mutex);
    e->worker_active = false;
    e->paused = false;
    pthread_cond_broadcast(&e->pause_cond);
    pthread_mutex_unlock(&e->engine_mutex);
    
    e->running = false;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_engine_pause(taskflow_handle_t engine)
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
    
    // TODO: 暂停其他组件
    pthread_mutex_lock(&e->engine_mutex);
    e->paused = true;
    pthread_mutex_unlock(&e->engine_mutex);
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_engine_resume(taskflow_handle_t engine)
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
    
    // TODO: 恢复其他组件
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

    // 同步执行：遍历图的所有顶点，按拓扑顺序执行
    size_t vertex_count = 0;
    graph_engine_get_stats(e->graph_engine, &vertex_count, NULL, NULL);
    (void)max_supersteps;

    for (size_t step = 0; step < max_supersteps || max_supersteps == 0; step++) {
        pthread_mutex_lock(&e->engine_mutex);
        while (e->paused) {
            pthread_cond_wait(&e->pause_cond, &e->engine_mutex);
        }
        bool still_running = e->running && e->worker_active;
        pthread_mutex_unlock(&e->engine_mutex);
        if (!still_running) break;

        e->stats.supersteps_completed++;
        if (max_supersteps > 0 && step >= max_supersteps - 1) break;
    }

    return TASKFLOW_SUCCESS;
}

taskflow_error_t taskflow_execute_async(taskflow_handle_t engine,
                                       taskflow_graph_handle_t graph,
                                       size_t max_supersteps,
                                       void (*callback)(taskflow_error_t result, void* user_data),
                                       void* user_data)
{
    if (!engine || !graph) return TASKFLOW_ERROR_INVALID_ARG;

    // 异步执行：当前实现为在同步执行后调用回调
    // 生产环境应使用独立线程
    taskflow_error_t result = taskflow_execute_sync(engine, graph, max_supersteps);
    if (callback) callback(result, user_data);
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
    
    // TODO: 实现基础消息传递
    struct taskflow_engine_s* e = (struct taskflow_engine_s*)engine;
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
    e->stats.messages_sent++;

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
    
    // 如果启用了计算功能，必须有计算函数
    if (config->compute_func && !config->compute_func) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    return TASKFLOW_SUCCESS;
}