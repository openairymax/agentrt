/**
 * @file async_storage_engine.c
 * @brief L1 原始卷异步存储引擎（生产级）- 重构版
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 生产级异步存储引擎主文件，整合I/O操作和队列管理模块。
 * 本文件仅包含工作线程和公共API实现。
 *
 * 核心特性：
 * 1. 批量异步写入：支持高吞吐量批量操作
 * 2. 错误恢复机制：写入失败自动重试
 * 3. 监控指标收集：实时性能指标
 * 4. 可观测性集成：与AgentOS可观测性子系统深度集成
 */

#include "../include/layer1_raw.h"
#include "async_storage_io.h"
#include "async_storage_queue.h"
#include "agentos.h"
#include "logger.h"
#include "observability.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* ==================== 常量定义 ==================== */

/** @brief 默认工作线程数 */
#define DEFAULT_WORKER_THREADS 8

/** @brief 批量写入大小（条目数） */
#define BATCH_WRITE_SIZE 100

/* ==================== 内部数据结构 ==================== */

typedef struct worker_context {
    int worker_id;
    async_queue_t* queue;
    char* storage_path;
    int running;

    uint64_t processed_count;
    uint64_t success_count;
    uint64_t failure_count;
    uint64_t total_processing_time_ns;

    agentos_observability_t* obs;
} worker_context_t;

typedef struct storage_engine_inner {
    char storage_path[MAX_FILE_PATH_LENGTH];
    async_queue_t* queue;
    worker_context_t** workers;
    size_t worker_count;

    uint64_t total_writes;
    uint64_t successful_writes;
    uint64_t failed_writes;
    uint64_t total_write_time_ns;
    uint64_t peak_queue_depth;
    uint64_t queue_full_errors;

    int healthy;
    char* last_error;
    uint64_t last_error_time_ns;

    agentos_observability_t* obs;
    char* engine_id;

#ifdef _WIN32
    HANDLE* worker_threads;
#else
    pthread_t* worker_threads;
#endif

    int shutdown;
} storage_engine_inner_t;

/* ==================== 工作线程函数 ==================== */

#ifdef _WIN32
static DWORD WINAPI worker_thread_func(LPVOID param) {
#else
static void* worker_thread_func(void* param) {
#endif
    worker_context_t* ctx = (worker_context_t*)param;
    if (!ctx) {
#ifdef _WIN32
        return 0;
#else
        return NULL;
#endif
    }

    AGENTOS_LOG_INFO("Storage worker thread started: %d", ctx->worker_id);

    while (ctx->running) {
        write_request_t* request = async_queue_dequeue(ctx->queue, 100);
        if (!request) {
            if (!ctx->running) break;
            continue;
        }

        uint64_t start_time_ns = agentos_get_monotonic_time_ns();
        agentos_error_t result = AGENTOS_SUCCESS;

        char file_path[MAX_FILE_PATH_LENGTH];
        if (build_file_path(ctx->storage_path, request->id, file_path, sizeof(file_path)) == AGENTOS_SUCCESS) {
            result = safe_write_file(file_path, request->data, request->data_len,
                                    request->retry_count > 0 ? request->retry_count : 5);
        } else {
            result = AGENTOS_EINVAL;
        }

        uint64_t end_time_ns = agentos_get_monotonic_time_ns();
        uint64_t processing_time_ns = end_time_ns - start_time_ns;

        ctx->processed_count++;
        ctx->total_processing_time_ns += processing_time_ns;

        if (result == AGENTOS_SUCCESS) {
            ctx->success_count++;

            if (ctx->obs) {
                agentos_observability_increment_counter(ctx->obs, "storage_write_success_total", 1);
                agentos_observability_record_histogram(ctx->obs, "storage_write_duration_seconds",
                                                      (double)processing_time_ns / 1e9);
            }
        } else {
            ctx->failure_count++;

            if (ctx->obs) {
                agentos_observability_increment_counter(ctx->obs, "storage_write_failure_total", 1);
            }

            AGENTOS_LOG_WARN("Worker %d failed to write record %s: error=%d",
                            ctx->worker_id, request->id, result);
        }

        if (request->id) AGENTOS_FREE(request->id);
        if (request->data) AGENTOS_FREE(request->data);
        AGENTOS_FREE(request);
    }

    AGENTOS_LOG_INFO("Storage worker thread stopped: %d", ctx->worker_id);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ==================== 公共API实现 ==================== */

agentos_error_t agentos_layer1_raw_create_production(
    const agentos_layer1_raw_config_t* manager,
    agentos_layer1_raw_t** out_engine) {

    if (!manager || !out_engine || !manager->storage_path) {
        return AGENTOS_EINVAL;
    }

    agentos_layer1_raw_t* engine = (agentos_layer1_raw_t*)AGENTOS_CALLOC(1, sizeof(agentos_layer1_raw_t));
    if (!engine) {
        AGENTOS_LOG_ERROR("Failed to allocate storage engine");
        return AGENTOS_ENOMEM;
    }

    engine->inner = (storage_engine_inner_t*)AGENTOS_CALLOC(1, sizeof(storage_engine_inner_t));
    if (!engine->inner) {
        AGENTOS_FREE(engine);
        return AGENTOS_ENOMEM;
    }

    strncpy(engine->inner->storage_path, manager->storage_path,
            sizeof(engine->inner->storage_path) - 1);
    engine->inner->storage_path[sizeof(engine->inner->storage_path) - 1] = '\0';

    agentos_error_t dir_result = ensure_directory_exists(manager->storage_path);
    if (dir_result != AGENTOS_SUCCESS) {
        AGENTOS_ERROR("Failed to create storage directory: %s", manager->storage_path);
        AGENTOS_FREE(engine->inner);
        AGENTOS_FREE(engine);
        return dir_result;
    }

    size_t queue_capacity = manager->queue_size > 0 ? manager->queue_size : DEFAULT_ASYNC_QUEUE_SIZE;
    engine->inner->queue = async_queue_create(queue_capacity);
    if (!engine->inner->queue) {
        AGENTOS_ERROR("Failed to create async queue");
        AGENTOS_FREE(engine->inner);
        AGENTOS_FREE(engine);
        return AGENTOS_ENOMEM;
    }

    size_t worker_count = manager->async_workers > 0 ? manager->async_workers : DEFAULT_WORKER_THREADS;
    engine->inner->worker_count = worker_count;

    engine->inner->workers = (worker_context_t**)AGENTOS_CALLOC(worker_count, sizeof(worker_context_t*));
    if (!engine->inner->workers) {
        async_queue_destroy(engine->inner->queue);
        AGENTOS_FREE(engine->inner);
        AGENTOS_FREE(engine);
        return AGENTOS_ENOMEM;
    }

#ifdef _WIN32
    engine->inner->worker_threads = (HANDLE*)AGENTOS_CALLOC(worker_count, sizeof(HANDLE));
#else
    engine->inner->worker_threads = (pthread_t*)AGENTOS_CALLOC(worker_count, sizeof(pthread_t));
#endif

    if (!engine->inner->worker_threads) {
        AGENTOS_FREE(engine->inner->workers);
        async_queue_destroy(engine->inner->queue);
        AGENTOS_FREE(engine->inner);
        AGENTOS_FREE(engine);
        return AGENTOS_ENOMEM;
    }

    engine->inner->obs = agentos_observability_create();
    if (engine->inner->obs) {
        agentos_observability_register_metric(engine->inner->obs, "storage_write_total",
                                              AGENTOS_METRIC_COUNTER, "Total write operations");
        agentos_observability_register_metric(engine->inner->obs, "storage_write_success_total",
                                              AGENTOS_METRIC_COUNTER, "Successful writes");
        agentos_observability_register_metric(engine->inner->obs, "storage_write_failure_total",
                                              AGENTOS_METRIC_COUNTER, "Failed writes");
        agentos_observability_register_metric(engine->inner->obs, "storage_write_duration_seconds",
                                              AGENTOS_METRIC_HISTOGRAM, "Write duration");
        agentos_observability_register_metric(engine->inner->obs, "storage_queue_depth",
                                              AGENTOS_METRIC_GAUGE, "Queue depth");
    }

    engine->inner->engine_id = agentos_generate_uuid();
    if (!engine->inner->engine_id) {
        engine->inner->engine_id = AGENTOS_STRDUP("storage_engine_default");
    }

    engine->inner->healthy = 1;
    engine->inner->last_error = NULL;

    for (size_t i = 0; i < worker_count; i++) {
        worker_context_t* worker = (worker_context_t*)AGENTOS_CALLOC(1, sizeof(worker_context_t));
        if (!worker) continue;

        worker->worker_id = (int)i;
        worker->queue = engine->inner->queue;
        worker->storage_path = AGENTOS_STRDUP(manager->storage_path);
        worker->running = 1;
        worker->processed_count = 0;
        worker->success_count = 0;
        worker->failure_count = 0;
        worker->total_processing_time_ns = 0;
        worker->obs = engine->inner->obs;

        engine->inner->workers[i] = worker;

#ifdef _WIN32
        engine->inner->worker_threads[i] = CreateThread(NULL, 0, worker_thread_func, worker, 0, NULL);
        if (!engine->inner->worker_threads[i]) {
            worker->running = 0;
            AGENTOS_FREE(worker->storage_path);
            AGENTOS_FREE(worker);
            engine->inner->workers[i] = NULL;
        }
#else
        if (pthread_create(&engine->inner->worker_threads[i], NULL, worker_thread_func, worker) != 0) {
            worker->running = 0;
            AGENTOS_FREE(worker->storage_path);
            AGENTOS_FREE(worker);
            engine->inner->workers[i] = NULL;
        }
#endif
    }

    int active_threads = 0;
    for (size_t i = 0; i < worker_count; i++) {
        if (engine->inner->workers[i]) active_threads++;
    }

    if (active_threads == 0) {
        if (engine->inner->obs) agentos_observability_destroy(engine->inner->obs);
        if (engine->inner->engine_id) AGENTOS_FREE(engine->inner->engine_id);
        AGENTOS_FREE(engine->inner->worker_threads);
        AGENTOS_FREE(engine->inner->workers);
        async_queue_destroy(engine->inner->queue);
        AGENTOS_FREE(engine->inner);
        AGENTOS_FREE(engine);
        return AGENTOS_EIO;
    }

    engine->inner->worker_count = active_threads;
    engine->inner->shutdown = 0;

    AGENTOS_LOG_INFO("Production storage engine created: %s (workers: %d, queue: %zu)",
                    engine->inner->engine_id, active_threads, queue_capacity);

    *out_engine = engine;
    return AGENTOS_SUCCESS;
}

void agentos_layer1_raw_destroy_production(agentos_layer1_raw_t* engine) {
    if (!engine || !engine->inner) return;

    AGENTOS_LOG_INFO("Destroying production storage engine: %s", engine->inner->engine_id);

    engine->inner->shutdown = 1;
    engine->inner->healthy = 0;

    for (size_t i = 0; i < engine->inner->worker_count; i++) {
        if (engine->inner->workers[i]) {
            engine->inner->workers[i]->running = 0;
        }
    }

    async_queue_destroy(engine->inner->queue);
    engine->inner->queue = NULL;

#ifdef _WIN32
    if (engine->inner->worker_threads) {
        WaitForMultipleObjects((DWORD)engine->inner->worker_count,
                               engine->inner->worker_threads, TRUE, 5000);
        for (size_t i = 0; i < engine->inner->worker_count; i++) {
            if (engine->inner->worker_threads[i]) {
                CloseHandle(engine->inner->worker_threads[i]);
            }
        }
    }
#else
    if (engine->inner->worker_threads) {
        for (size_t i = 0; i < engine->inner->worker_count; i++) {
            if (engine->inner->worker_threads[i]) {
                pthread_join(engine->inner->worker_threads[i], NULL);
            }
        }
    }
#endif

    if (engine->inner->workers) {
        for (size_t i = 0; i < engine->inner->worker_count; i++) {
            if (engine->inner->workers[i]) {
                if (engine->inner->workers[i]->storage_path) {
                    AGENTOS_FREE(engine->inner->workers[i]->storage_path);
                }
                AGENTOS_FREE(engine->inner->workers[i]);
            }
        }
        AGENTOS_FREE(engine->inner->workers);
    }

    if (engine->inner->worker_threads) {
        AGENTOS_FREE(engine->inner->worker_threads);
    }

    if (engine->inner->obs) {
        agentos_observability_destroy(engine->inner->obs);
    }

    if (engine->inner->engine_id) {
        AGENTOS_FREE(engine->inner->engine_id);
    }

    if (engine->inner->last_error) {
        AGENTOS_FREE(engine->inner->last_error);
    }

    AGENTOS_FREE(engine->inner);
    AGENTOS_FREE(engine);

    AGENTOS_LOG_INFO("Production storage engine destroyed");
}

agentos_error_t agentos_layer1_raw_write_async_production(
    agentos_layer1_raw_t* engine,
    const char* id,
    const void* data,
    size_t len,
    uint8_t priority,
    uint32_t timeout_ms) {

    if (!engine || !engine->inner || !id || !data || len == 0) {
        return AGENTOS_EINVAL;
    }

    if (!engine->inner->healthy) {
        return AGENTOS_EIO;
    }

    write_request_t* request = (write_request_t*)AGENTOS_CALLOC(1, sizeof(write_request_t));
    if (!request) {
        return AGENTOS_ENOMEM;
    }

    request->id = AGENTOS_STRDUP(id);
    if (!request->id) {
        AGENTOS_FREE(request);
        return AGENTOS_ENOMEM;
    }

    request->data = AGENTOS_MALLOC(len);
    if (!request->data) {
        AGENTOS_FREE(request->id);
        AGENTOS_FREE(request);
        return AGENTOS_ENOMEM;
    }

    memcpy(request->data, data, len);
    request->data_len = len;
    request->timestamp_ns = agentos_get_monotonic_time_ns();
    request->retry_count = 0;
    request->priority = priority;
    request->flags = 0;
    request->next = NULL;

    agentos_error_t result = async_queue_enqueue(engine->inner->queue, request, timeout_ms);
    if (result != AGENTOS_SUCCESS) {
        AGENTOS_FREE(request->data);
        AGENTOS_FREE(request->id);
        AGENTOS_FREE(request);
        return result;
    }

    engine->inner->total_writes++;

    size_t current_depth = async_queue_get_count(engine->inner->queue);
    if (current_depth > engine->inner->peak_queue_depth) {
        engine->inner->peak_queue_depth = current_depth;
    }

    return AGENTOS_SUCCESS;
}
