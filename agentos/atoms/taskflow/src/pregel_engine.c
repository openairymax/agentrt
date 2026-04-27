// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file pregel_engine.c
 * @brief Pregel Engine Implementation
 * 
 * Pregel 超步引擎实现，提供分布式图计算功能。
 * 基于 Google Pregel 模型，支持迭代计算、消息传递和容错恢复。
 */

#include "pregel_engine.h"
#include "graph_engine.h"
#include "taskflow.h"
#include "taskflow_types.h"
#include <memory_compat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

// ============================================================================
// 内部数据结构
// ============================================================================

// 顶点状态（Pregel专用）
typedef struct {
    vertex_id_t vertex_id;
    void* value;
    size_t value_size;
    bool active;
    bool vote_to_halt;
    size_t incoming_message_count;
    graph_message_t* incoming_messages;
} pregel_vertex_state_t;

// 消息队列条目
typedef struct message_queue_entry_s {
    vertex_id_t target;
    void* payload;
    size_t payload_size;
    superstep_t step;
    struct message_queue_entry_s* next;
} message_queue_entry_t;

// 消息队列
typedef struct {
    message_queue_entry_t* front;
    message_queue_entry_t* rear;
    size_t size;
    size_t total_bytes;
} message_queue_t;

// 工作线程上下文
typedef struct {
    size_t worker_id;
    struct pregel_engine_s* engine;
    void* thread_handle;
    bool running;
} worker_context_t;

// Pregel引擎结构
struct pregel_engine_s {
    pregel_config_t config;
    graph_engine_handle_t graph_engine;
    
    // 顶点状态
    pregel_vertex_state_t* vertex_states;
    size_t vertex_state_count;
    
    // 消息队列
    message_queue_t** message_queues;  // 每个工作线程一个队列
    message_queue_t* next_step_queue;  // 下一步消息队列
    
    // 工作线程
    worker_context_t* workers;
    size_t worker_count;
    
    // 超步状态
    superstep_t current_superstep;
    size_t active_vertices;
    bool computation_done;
    
    // 同步原语
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;
    pthread_cond_t pause_cond;
    
    // 检查点管理
    uint64_t last_checkpoint_id;
    checkpoint_t* checkpoints;
    size_t checkpoint_count;
    
    // 统计信息
    execution_stats_t stats;
    
    // 控制标志
    bool initialized;
    bool running;
    bool paused;
};

// ============================================================================
// 静态辅助函数
// ============================================================================

// FNV-1a哈希函数（用于顶点ID到工作线程的映射）
static size_t vertex_id_hash(vertex_id_t id, size_t table_size) {
    const uint64_t FNV_offset_basis = 14695981039346656037ULL;
    const uint64_t FNV_prime = 1099511628211ULL;

    uint64_t hash = FNV_offset_basis;
    uint8_t* bytes = (uint8_t*)&id;

    for (size_t i = 0; i < sizeof(vertex_id_t); i++) {
        hash ^= bytes[i];
        hash *= FNV_prime;
    }

    return (table_size > 0) ? (hash % table_size) : 0;
}

// 创建消息队列
static message_queue_t* message_queue_create(void) {
    message_queue_t* queue = (message_queue_t*)calloc(1, sizeof(message_queue_t));
    return queue;
}

// 销毁消息队列
static void message_queue_destroy(message_queue_t* queue) {
    if (!queue) return;
    
    message_queue_entry_t* entry = queue->front;
    while (entry) {
        message_queue_entry_t* next = entry->next;
        if (entry->payload) free(entry->payload);
        free(entry);
        entry = next;
    }
    
    free(queue);
}

// 向消息队列添加消息
static bool message_queue_enqueue(message_queue_t* queue,
                                 vertex_id_t target,
                                 const void* payload,
                                 size_t payload_size,
                                 superstep_t step) {
    if (!queue) return false;
    
    message_queue_entry_t* entry = (message_queue_entry_t*)calloc(1, sizeof(message_queue_entry_t));
    if (!entry) return false;
    
    entry->target = target;
    entry->payload_size = payload_size;
    entry->step = step;
    
    if (payload_size > 0 && payload) {
        entry->payload = malloc(payload_size);
        if (!entry->payload) {
            free(entry);
            return false;
        }
        memcpy(entry->payload, payload, payload_size);
    } else {
        entry->payload = NULL;
    }
    
    entry->next = NULL;
    
    if (!queue->rear) {
        queue->front = queue->rear = entry;
    } else {
        queue->rear->next = entry;
        queue->rear = entry;
    }
    
    queue->size++;
    queue->total_bytes += payload_size;
    
    return true;
}

// 从消息队列取出消息
static message_queue_entry_t* message_queue_dequeue(message_queue_t* queue) {
    if (!queue || !queue->front) return NULL;
    
    message_queue_entry_t* entry = queue->front;
    queue->front = entry->next;
    
    if (!queue->front) queue->rear = NULL;
    
    queue->size--;
    queue->total_bytes -= entry->payload_size;
    
    return entry;
}

// 检查消息队列是否为空
static bool message_queue_is_empty(const message_queue_t* queue) {
    return !queue || !queue->front;
}

// 获取顶点状态索引
static size_t get_vertex_state_index(struct pregel_engine_s* engine, vertex_id_t vertex_id) {
    // 简单线性搜索（可优化为哈希表）
    for (size_t i = 0; i < engine->vertex_state_count; i++) {
        if (engine->vertex_states[i].vertex_id == vertex_id) {
            return i;
        }
    }
    return SIZE_MAX;
}

// 初始化顶点状态
static bool init_vertex_states(struct pregel_engine_s* engine) {
    if (!engine->graph_engine) return false;
    
    // 获取图信息
    size_t vertex_count = 0;
    size_t edge_count = 0;
    graph_engine_get_stats(engine->graph_engine, &vertex_count, &edge_count, NULL, NULL);
    
    if (vertex_count == 0) return true;
    
    // 分配顶点状态数组
    engine->vertex_states = (pregel_vertex_state_t*)calloc(vertex_count, sizeof(pregel_vertex_state_t));
    if (!engine->vertex_states) return false;
    
    engine->vertex_state_count = vertex_count;
    
    // 初始化每个顶点的状态
    for (size_t i = 0; i < vertex_count; i++) {
        engine->vertex_states[i].vertex_id = i + 1;
        engine->vertex_states[i].active = true;
        engine->vertex_states[i].vote_to_halt = false;
        engine->vertex_states[i].incoming_message_count = 0;
        engine->vertex_states[i].incoming_messages = NULL;
    }

    // 尝试从图引擎获取真实顶点ID映射
    {
        vertex_id_t* real_ids = (vertex_id_t*)calloc(vertex_count, sizeof(vertex_id_t));
        if (real_ids) {
            size_t fetched = 0;
            if (graph_engine_get_vertex_ids(engine->graph_engine, real_ids, vertex_count, &fetched)
                == TASKFLOW_SUCCESS && fetched == vertex_count) {
                for (size_t i = 0; i < vertex_count; i++) {
                    engine->vertex_states[i].vertex_id = real_ids[i];
                }
            }
            free(real_ids);
        }
    }
    
    return true;
}

// 工作线程函数
static void worker_thread_func(void* arg) {
    worker_context_t* context = (worker_context_t*)arg;
    if (!context || !context->engine) return;

    struct pregel_engine_s* engine = context->engine;

    while (context->running) {
        pthread_mutex_lock(&engine->mutex);

        while (context->running && engine->paused) {
            pthread_cond_wait(&engine->pause_cond, &engine->mutex);
        }

        if (!context->running) {
            pthread_mutex_unlock(&engine->mutex);
            break;
        }

        // 检查是否有活跃顶点需要处理
        bool has_work = false;
        for (size_t i = 0; i < engine->vertex_state_count; i++) {
            if (engine->vertex_states[i].active && !engine->vertex_states[i].vote_to_halt) {
                has_work = true;
                break;
            }
        }

        if (!has_work || engine->computation_done) {
            pthread_cond_timedwait(&engine->cond_var, &engine->mutex,
                &(struct timespec){.tv_sec = time(NULL) + 1});
            pthread_mutex_unlock(&engine->mutex);
            continue;
        }

        // 处理分配到该工作线程的顶点（简化：按worker_id取模分配）
        for (size_t i = context->worker_id; i < engine->vertex_state_count; i += engine->worker_count) {
            if (engine->vertex_states[i].active && !engine->vertex_states[i].vote_to_halt
                && engine->config.compute_func) {
                engine->config.compute_func(
                    engine->vertex_states[i].vertex_id,
                    engine->vertex_states[i].value,
                    engine->vertex_states[i].value_size,
                    engine->vertex_states[i].incoming_messages,
                    engine->vertex_states[i].incoming_message_count,
                    engine->config.user_context);
            }
        }

        pthread_mutex_unlock(&engine->mutex);

        // 避免忙等待
        struct timespec ts = {.tv_nsec = 10000000}; // 10ms
        nanosleep(&ts, NULL);
    }
}

// ============================================================================
// 核心API实现
// ============================================================================

pregel_engine_handle_t pregel_engine_create(const pregel_config_t* config)
{
    if (!config) {
        return NULL;
    }
    
    struct pregel_engine_s* engine = (struct pregel_engine_s*)calloc(1, sizeof(struct pregel_engine_s));
    if (!engine) {
        return NULL;
    }
    
    // 复制配置
    engine->config = *config;
    
    // 设置默认值
    if (engine->config.max_workers == 0) {
        engine->config.max_workers = 4;
    }
    
    if (engine->config.message_buffer_size == 0) {
        engine->config.message_buffer_size = 1024 * 1024; // 1MB
    }
    
    if (engine->config.superstep_timeout_ms == 0) {
        engine->config.superstep_timeout_ms = 30000; // 30秒
    }
    
    if (engine->config.batch_size == 0) {
        engine->config.batch_size = 1000;
    }
    
    // 初始化消息队列数组
    engine->message_queues = (message_queue_t**)calloc(engine->config.max_workers, sizeof(message_queue_t*));
    if (!engine->message_queues) {
        free(engine);
        return NULL;
    }
    
    for (size_t i = 0; i < engine->config.max_workers; i++) {
        engine->message_queues[i] = message_queue_create();
        if (!engine->message_queues[i]) {
            // 清理已创建的队列
            for (size_t j = 0; j < i; j++) {
                message_queue_destroy(engine->message_queues[j]);
            }
            free(engine->message_queues);
            free(engine);
            return NULL;
        }
    }
    
    // 创建下一步消息队列
    engine->next_step_queue = message_queue_create();
    if (!engine->next_step_queue) {
        for (size_t i = 0; i < engine->config.max_workers; i++) {
            message_queue_destroy(engine->message_queues[i]);
        }
        free(engine->message_queues);
        free(engine);
        return NULL;
    }
    
    // 初始化工作线程数组
    engine->workers = (worker_context_t*)calloc(engine->config.max_workers, sizeof(worker_context_t));
    if (!engine->workers) {
        message_queue_destroy(engine->next_step_queue);
        for (size_t i = 0; i < engine->config.max_workers; i++) {
            message_queue_destroy(engine->message_queues[i]);
        }
        free(engine->message_queues);
        free(engine);
        return NULL;
    }
    
    // 初始化检查点数组
    engine->checkpoints = NULL;
    engine->checkpoint_count = 0;
    
    // 初始化统计信息
    memset(&engine->stats, 0, sizeof(engine->stats));
    
    // 初始化状态
    engine->current_superstep = 0;
    engine->active_vertices = 0;
    engine->computation_done = false;
    engine->last_checkpoint_id = 0;
    engine->initialized = false;
    engine->running = false;
    engine->paused = false;
    
    return (pregel_engine_handle_t)engine;
}

void pregel_engine_destroy(pregel_engine_handle_t engine)
{
    if (!engine) return;
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    // 停止引擎（如果正在运行）
    if (e->running) {
        pregel_engine_stop(engine);
    }
    
    // 释放顶点状态
    if (e->vertex_states) {
        for (size_t i = 0; i < e->vertex_state_count; i++) {
            if (e->vertex_states[i].value) {
                free(e->vertex_states[i].value);
            }
            if (e->vertex_states[i].incoming_messages) {
                graph_message_t* msg = e->vertex_states[i].incoming_messages;
                while (msg) {
                    graph_message_t* next = msg->next;
                    if (msg->payload) free(msg->payload);
                    free(msg);
                    msg = next;
                }
            }
        }
        free(e->vertex_states);
    }
    
    // 释放消息队列
    for (size_t i = 0; i < e->config.max_workers; i++) {
        message_queue_destroy(e->message_queues[i]);
    }
    free(e->message_queues);
    
    message_queue_destroy(e->next_step_queue);
    
    // 释放工作线程上下文
    if (e->workers) {
        free(e->workers);
    }

    // 销毁同步原语
    pthread_mutex_destroy(&e->mutex);
    pthread_cond_destroy(&e->cond_var);
    pthread_cond_destroy(&e->pause_cond);
    
    // 释放检查点
    if (e->checkpoints) {
        for (size_t i = 0; i < e->checkpoint_count; i++) {
            if (e->checkpoints[i].snapshot_data) {
                free(e->checkpoints[i].snapshot_data);
            }
        }
        free(e->checkpoints);
    }
    
    free(e);
}

taskflow_error_t pregel_engine_init(pregel_engine_handle_t engine,
                                   graph_engine_handle_t graph_engine)
{
    if (!engine || !graph_engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    if (e->initialized) {
        return TASKFLOW_ERROR_ALREADY_INITIALIZED;
    }
    
    e->graph_engine = graph_engine;
    
    // 初始化顶点状态
    if (!init_vertex_states(e)) {
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 初始化同步原语（mutex, cond_var）
    pthread_mutex_init(&e->mutex, NULL);
    pthread_cond_init(&e->cond_var, NULL);
    pthread_cond_init(&e->pause_cond, NULL);

    // 创建工作线程
    size_t num_workers = e->config.max_workers > 0 ? e->config.max_workers : 4;
    e->workers = (worker_context_t*)calloc(num_workers, sizeof(worker_context_t));
    if (!e->workers) {
        pthread_mutex_destroy(&e->mutex);
        pthread_cond_destroy(&e->cond_var);
        pthread_cond_destroy(&e->pause_cond);
        return TASKFLOW_ERROR_MEMORY;
    }
    e->worker_count = num_workers;

    for (size_t i = 0; i < num_workers; i++) {
        e->workers[i].worker_id = i;
        e->workers[i].engine = e;
        e->workers[i].running = false;
    }
    
    e->initialized = true;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t pregel_engine_start(pregel_engine_handle_t engine,
                                    size_t max_supersteps)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    if (e->running) {
        return TASKFLOW_SUCCESS; // 已经在运行
    }
    
    if (!e->config.compute_func) {
        return TASKFLOW_ERROR_INVALID_ARG; // 没有计算函数
    }
    
    // 启动工作线程
    pthread_mutex_lock(&e->mutex);
    for (size_t i = 0; i < e->worker_count; i++) {
        e->workers[i].running = true;
        pthread_create((pthread_t*)&e->workers[i].thread_handle, NULL,
                       (void*(*)(void*))worker_thread_func, &e->workers[i]);
    }
    pthread_mutex_unlock(&e->mutex);
    
    e->running = true;
    e->paused = false;
    e->computation_done = false;
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t pregel_engine_stop(pregel_engine_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    if (!e->running) {
        return TASKFLOW_SUCCESS;
    }

    pthread_mutex_lock(&e->mutex);
    for (size_t i = 0; i < e->worker_count; i++) {
        e->workers[i].running = false;
    }
    pthread_cond_broadcast(&e->cond_var);
    pthread_cond_broadcast(&e->pause_cond);
    pthread_mutex_unlock(&e->mutex);

    for (size_t i = 0; i < e->worker_count; i++) {
        if (e->workers[i].thread_handle) {
            pthread_join((pthread_t)e->workers[i].thread_handle, NULL);
            e->workers[i].thread_handle = NULL;
        }
    }
    
    e->running = false;
    e->paused = false;
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t pregel_engine_pause(pregel_engine_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    if (!e->running) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    if (e->paused) {
        return TASKFLOW_SUCCESS; // 已经暂停
    }
    
    e->paused = true;

    pthread_mutex_lock(&e->mutex);
    pthread_cond_broadcast(&e->cond_var);
    pthread_mutex_unlock(&e->mutex);
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t pregel_engine_resume(pregel_engine_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    if (!e->running) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    if (!e->paused) {
        return TASKFLOW_SUCCESS; // 未暂停
    }
    
    e->paused = false;

    // 通过broadcast pause_cond唤醒所有等待的工作线程
    pthread_mutex_lock(&e->mutex);
    pthread_cond_broadcast(&e->pause_cond);
    pthread_mutex_unlock(&e->mutex);
    
    return TASKFLOW_SUCCESS;
}

superstep_t pregel_engine_get_current_superstep(pregel_engine_handle_t engine)
{
    if (!engine) return 0;
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    return e->current_superstep;
}

size_t pregel_engine_get_active_vertices(pregel_engine_handle_t engine)
{
    if (!engine) return 0;
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    return e->active_vertices;
}

size_t pregel_engine_get_queued_messages(pregel_engine_handle_t engine)
{
    if (!engine) return 0;
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    size_t total_messages = 0;
    for (size_t i = 0; i < e->config.max_workers; i++) {
        if (e->message_queues[i]) {
            total_messages += e->message_queues[i]->size;
        }
    }
    
    return total_messages;
}

taskflow_error_t pregel_engine_send_message(pregel_engine_handle_t engine,
                                           vertex_id_t source,
                                           vertex_id_t target,
                                           const void* payload,
                                           size_t payload_size)
{
    if (!engine || source == 0 || target == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 检查目标顶点是否存在
    if (get_vertex_state_index(e, target) == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 选择工作线程（简单哈希）
    size_t worker_id = vertex_id_hash(target, e->config.max_workers);
    
    // 添加到消息队列
    if (!message_queue_enqueue(e->message_queues[worker_id], target, payload, payload_size, e->current_superstep)) {
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 更新统计信息
    e->stats.total_messages++;
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t pregel_engine_broadcast_message(pregel_engine_handle_t engine,
                                                vertex_id_t source,
                                                const void* payload,
                                                size_t payload_size)
{
    if (!engine || source == 0 || !payload || payload_size == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    pthread_mutex_lock(&e->mutex);
    taskflow_error_t result = TASKFLOW_SUCCESS;
    size_t sent_count = 0;

    for (size_t i = 0; i < e->vertex_state_count; i++) {
        vertex_id_t target = e->vertex_states[i].vertex_id;
        if (target == source) continue; // 跳过源顶点

        size_t worker_id = target % e->worker_count;
        if (!message_queue_enqueue(e->message_queues[worker_id], target, payload, payload_size, e->current_superstep)) {
            result = TASKFLOW_ERROR_MEMORY;
            break;
        }
        sent_count++;
        e->stats.total_messages++;
    }
    pthread_mutex_unlock(&e->mutex);

    return (sent_count > 0) ? result : TASKFLOW_SUCCESS;
}

bool pregel_engine_get_vote_to_halt(pregel_engine_handle_t engine,
                                   vertex_id_t vertex_id)
{
    if (!engine || vertex_id == 0) return false;
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    size_t idx = get_vertex_state_index(e, vertex_id);
    if (idx == SIZE_MAX) return false;
    
    return e->vertex_states[idx].vote_to_halt;
}

taskflow_error_t pregel_engine_set_vote_to_halt(pregel_engine_handle_t engine,
                                              vertex_id_t vertex_id,
                                              bool vote_to_halt)
{
    if (!engine || vertex_id == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    size_t idx = get_vertex_state_index(e, vertex_id);
    if (idx == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    e->vertex_states[idx].vote_to_halt = vote_to_halt;
    
    if (vote_to_halt && e->vertex_states[idx].active) {
        e->vertex_states[idx].active = false;
        if (e->active_vertices > 0) e->active_vertices--;
    }
    
    return TASKFLOW_SUCCESS;
}

uint64_t pregel_engine_create_checkpoint(pregel_engine_handle_t engine)
{
    if (!engine) return 0;

    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;

    if (!e->initialized) {
        return 0;
    }

    pthread_mutex_lock(&e->mutex);

    static uint64_t next_checkpoint_id = 1;
    uint64_t cp_id = next_checkpoint_id++;

    size_t states_size = e->vertex_state_count * sizeof(pregel_vertex_state_t);
    size_t snapshot_size = states_size + sizeof(superstep_t) + sizeof(size_t);

    void* snapshot = AGENTOS_MALLOC(snapshot_size);
    if (snapshot) {
        size_t offset = 0;
        memcpy((char*)snapshot + offset, &e->current_superstep, sizeof(superstep_t));
        offset += sizeof(superstep_t);
        memcpy((char*)snapshot + offset, &e->active_vertices, sizeof(size_t));
        offset += sizeof(size_t);
        if (e->vertex_states && states_size > 0) {
            memcpy((char*)snapshot + offset, e->vertex_states, states_size);
        }
    }

    if (e->checkpoint_count < 16) {
        e->checkpoints = (checkpoint_t*)AGENTOS_REALLOC(
            e->checkpoints, (e->checkpoint_count + 1) * sizeof(checkpoint_t));
        if (e->checkpoints) {
            size_t idx = e->checkpoint_count;
            e->checkpoints[idx].checkpoint_id = cp_id;
            e->checkpoints[idx].superstep = e->current_superstep;
            e->checkpoints[idx].timestamp = (uint64_t)time(NULL);
            e->checkpoints[idx].data_size = snapshot_size;
            e->checkpoints[idx].snapshot_data = snapshot;
            e->checkpoints[idx].is_consistent = true;
            e->checkpoint_count++;
        }
    } else {
        if (e->checkpoints[0].snapshot_data) AGENTOS_FREE(e->checkpoints[0].snapshot_data);
        memmove(&e->checkpoints[0], &e->checkpoints[1],
                15 * sizeof(checkpoint_t));
        e->checkpoints[15].checkpoint_id = cp_id;
        e->checkpoints[15].superstep = e->current_superstep;
        e->checkpoints[15].timestamp = (uint64_t)time(NULL);
        e->checkpoints[15].data_size = snapshot_size;
        e->checkpoints[15].snapshot_data = snapshot;
        e->checkpoints[15].is_consistent = true;
    }

    e->last_checkpoint_id = cp_id;

    pthread_mutex_unlock(&e->mutex);
    return cp_id;
}

taskflow_error_t pregel_engine_restore_checkpoint(pregel_engine_handle_t engine,
                                                 uint64_t checkpoint_id)
{
    if (!engine || checkpoint_id == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }

    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;

    if (!e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&e->mutex);

    checkpoint_t* cp = NULL;
    for (size_t i = 0; i < e->checkpoint_count; i++) {
        if (e->checkpoints[i].checkpoint_id == checkpoint_id) {
            cp = &e->checkpoints[i];
            break;
        }
    }

    if (!cp || !cp->snapshot_data) {
        pthread_mutex_unlock(&e->mutex);
        return TASKFLOW_ERROR_INVALID_ARG;
    }

    char* data = (char*)cp->snapshot_data;
    size_t offset = 0;

    memcpy(&e->current_superstep, data + offset, sizeof(superstep_t));
    offset += sizeof(superstep_t);

    memcpy(&e->active_vertices, data + offset, sizeof(size_t));
    offset += sizeof(size_t);

    size_t states_size = e->vertex_state_count * sizeof(pregel_vertex_state_t);
    if (e->vertex_states && states_size > 0 && cp->data_size >= offset + states_size) {
        memcpy(e->vertex_states, data + offset, states_size);
    }

    e->computation_done = false;

    pthread_mutex_unlock(&e->mutex);
    return TASKFLOW_SUCCESS;
}

taskflow_error_t pregel_engine_get_stats(pregel_engine_handle_t engine,
                                        execution_stats_t* stats)
{
    if (!engine || !stats) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    *stats = e->stats;
    
    // 更新动态统计信息
    stats->active_supersteps = e->current_superstep;
    stats->completed_supersteps = e->current_superstep;
    
    // 获取图统计信息
    if (e->graph_engine) {
        size_t vertex_count, edge_count;
        uint32_t max_out_degree, max_in_degree;
        graph_engine_get_stats(e->graph_engine, &vertex_count, &edge_count, 
                              &max_out_degree, &max_in_degree);
        
        stats->total_vertices = vertex_count;
        stats->total_edges = edge_count;
    }
    
    stats->total_messages = pregel_engine_get_queued_messages(engine);
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t pregel_engine_reset_stats(pregel_engine_handle_t engine)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;
    
    memset(&e->stats, 0, sizeof(e->stats));
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t pregel_engine_wait_for_completion(pregel_engine_handle_t engine,
                                                  uint32_t timeout_ms)
{
    if (!engine) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }

    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;

    if (!e->running || e->computation_done || e->active_vertices == 0) {
        return TASKFLOW_SUCCESS;
    }

    pthread_mutex_lock(&e->mutex);

    uint64_t deadline_ns = 0;
    if (timeout_ms > 0 && timeout_ms != UINT32_MAX) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        deadline_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec +
                     (uint64_t)timeout_ms * 1000000ULL;
    }

    while (e->running && !e->computation_done && e->active_vertices > 0) {
        if (timeout_ms == 0 || timeout_ms == UINT32_MAX) {
            pthread_cond_wait(&e->cond_var, &e->mutex);
        } else {
            struct timespec now_ts;
            clock_gettime(CLOCK_REALTIME, &now_ts);
            uint64_t now_ns = (uint64_t)now_ts.tv_sec * 1000000000ULL + (uint64_t)now_ts.tv_nsec;
            if (now_ns >= deadline_ns) {
                pthread_mutex_unlock(&e->mutex);
                return TASKFLOW_ERROR_TIMEOUT;
            }
            int remaining_ms = (int)((deadline_ns - now_ns) / 1000000ULL);
            if (remaining_ms <= 0) remaining_ms = 1;
            struct timespec abs_timeout;
            abs_timeout.tv_sec = now_ts.tv_sec + (remaining_ms / 1000);
            abs_timeout.tv_nsec = now_ts.tv_nsec + ((remaining_ms % 1000) * 1000000L);
            if (abs_timeout.tv_nsec >= 1000000000L) {
                abs_timeout.tv_sec++;
                abs_timeout.tv_nsec -= 1000000000L;
            }
            int ret = pthread_cond_timedwait(&e->cond_var, &e->mutex, &abs_timeout);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&e->mutex);
                return TASKFLOW_ERROR_TIMEOUT;
            }
        }
    }

    taskflow_error_t result = (e->computation_done || e->active_vertices == 0)
        ? TASKFLOW_SUCCESS : TASKFLOW_ERROR_INTERNAL;

    pthread_mutex_unlock(&e->mutex);
    return result;
}

taskflow_error_t pregel_engine_run_superstep(pregel_engine_handle_t engine)
{
    if (!engine) return TASKFLOW_ERROR_INVALID_ARG;

    struct pregel_engine_s* e = (struct pregel_engine_s*)engine;

    if (!e->running || !e->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&e->mutex);

    // 检查是否还有活跃顶点
    size_t active_count = 0;
    for (size_t i = 0; i < e->vertex_state_count; i++) {
        if (e->vertex_states[i].active && !e->vertex_states[i].vote_to_halt) {
            active_count++;
        }
    }

    if (active_count == 0) {
        e->computation_done = true;
        e->active_vertices = 0;
        pthread_cond_broadcast(&e->cond_var);
        pthread_mutex_unlock(&e->mutex);
        return (taskflow_error_t)20;
    }

    e->active_vertices = active_count;
    e->current_superstep++;
    e->stats.completed_supersteps++;
    e->stats.active_supersteps = e->current_superstep;

    for (size_t i = 0; i < e->vertex_state_count; i++) {
        pregel_vertex_state_t* vs = &e->vertex_states[i];
        if (vs->incoming_messages) {
            for (size_t j = 0; j < vs->incoming_message_count; j++) {
                if (vs->incoming_messages[j].payload) {
                    free(vs->incoming_messages[j].payload);
                }
            }
            free(vs->incoming_messages);
            vs->incoming_messages = NULL;
        }
        vs->incoming_message_count = 0;
    }

    if (e->config.start_func) {
        e->config.start_func(e->current_superstep, e->config.user_context);
    }

    // 处理消息交换：将next_step_queue中的消息分发到各工作线程队列
    if (e->next_step_queue && !message_queue_is_empty(e->next_step_queue)) {
        while (!message_queue_is_empty(e->next_step_queue)) {
            message_queue_entry_t* entry = message_queue_dequeue(e->next_step_queue);
            if (entry) {
                size_t target_idx = get_vertex_state_index(e, entry->target);
                if (target_idx != SIZE_MAX) {
                    pregel_vertex_state_t* vs = &e->vertex_states[target_idx];
                    size_t new_count = vs->incoming_message_count + 1;
                    graph_message_t* new_msgs = (graph_message_t*)realloc(
                        vs->incoming_messages, new_count * sizeof(graph_message_t));
                    if (new_msgs) {
                        vs->incoming_messages = new_msgs;
                        graph_message_t* msg = &vs->incoming_messages[vs->incoming_message_count];
                        memset(msg, 0, sizeof(graph_message_t));
                        msg->id = (message_id_t)(vs->incoming_message_count + 1);
                        msg->sender = entry->target;
                        msg->receiver = entry->target;
                        msg->payload = entry->payload;
                        msg->payload_size = entry->payload_size;
                        msg->step = entry->step;
                        msg->direction = MESSAGE_INCOMING;
                        vs->incoming_message_count = new_count;
                        entry->payload = NULL;
                    }
                }
                if (entry->payload) free(entry->payload);
                free(entry);
            }
        }
    }

    pthread_mutex_unlock(&e->mutex);

    // 工作线程会自动处理活跃顶点的计算（在worker_thread_func中）

    // 执行超步结束回调
    if (e->config.end_func) {
        e->config.end_func(e->current_superstep, e->config.user_context);
    }

    // 检查点间隔检查
    if (e->config.checkpoint_interval > 0 &&
        e->current_superstep % e->config.checkpoint_interval == 0) {
        pregel_engine_create_checkpoint(engine);
    }

    return TASKFLOW_SUCCESS;
}