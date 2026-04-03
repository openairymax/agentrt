/**
 * @file heapstore_core.c
 * @brief AgentOS 数据分区核心实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "heapstore.h"
#include "private.h"
#include "heapstore_log.h"
#include "heapstore_trace.h"
#include "heapstore_registry.h"
#include "heapstore_ipc.h"
#include "heapstore_memory.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <stdatomic.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define stat _stat
#define S_ISDIR(m) (((m) & _S_IFDIR) == _S_IFDIR)
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/resource.h>
#endif

#define heapstore_MAX_PATH_LEN 512
#define heapstore_MAX_SUBPATHS 32
#define heapstore_MAX_SERVICE_LOGS 32

#define heapstore_DEFAULT_CIRCUIT_THRESHOLD 5
#define heapstore_DEFAULT_CIRCUIT_TIMEOUT_SEC 30

static bool s_initialized = false;
static char s_root_path[heapstore_MAX_PATH_LEN];
static heapstore_config_t s_config;

static heapstore_path_type_t s_path_order[] = {
    heapstore_PATH_KERNEL,
    heapstore_PATH_LOGS,
    heapstore_PATH_REGISTRY,
    heapstore_PATH_SERVICES,
    heapstore_PATH_TRACES,
    heapstore_PATH_KERNEL_IPC,
    heapstore_PATH_KERNEL_MEMORY
};

static const char* s_path_names[] = {
    "kernel",
    "logs",
    "registry",
    "services",
    "traces",
    "kernel/ipc",
    "kernel/memory"
};

static const char* s_subpath_map[][heapstore_MAX_SUBPATHS] = {
    {NULL},
    {"apps", "kernel", "services", NULL},
    {NULL},
    {"llm_d", "market_d", "tool_d", NULL},
    {"spans", NULL},
    {"channels", "buffers", NULL},
    {"pools", "allocations", "stats", "index", "meta", "patterns", "raw", NULL}
};

static const char* s_default_root = "heapstore";

typedef struct {
    atomic_uint_fast32_t state;
    atomic_uint_fast32_t failure_count;
    atomic_uint_fast64_t last_failure_time;
    uint32_t threshold;
    uint32_t timeout_sec;
} heapstore_circuit_breaker_t;

typedef struct {
    atomic_uint_fast64_t total_operations;
    atomic_uint_fast64_t failed_operations;
    atomic_uint_fast64_t fast_path_operations;
    atomic_uint_fast64_t slow_path_operations;
    atomic_uint_fast64_t circuit_breaker_trips;
    atomic_uint_fast64_t total_operation_time_ns;
    atomic_uint_fast64_t peak_concurrent_ops;
    atomic_uint_fast32_t current_concurrent_ops;
} heapstore_metrics_t;

static heapstore_circuit_breaker_t s_circuit_breaker = {
    .state = 0,
    .failure_count = 0,
    .last_failure_time = 0,
    .threshold = heapstore_DEFAULT_CIRCUIT_THRESHOLD,
    .timeout_sec = heapstore_DEFAULT_CIRCUIT_TIMEOUT_SEC
};

static heapstore_metrics_t s_metrics = {
    .total_operations = 0,
    .failed_operations = 0,
    .fast_path_operations = 0,
    .slow_path_operations = 0,
    .circuit_breaker_trips = 0,
    .total_operation_time_ns = 0,
    .peak_concurrent_ops = 0,
    .current_concurrent_ops = 0
};

static void set_default_config(void) {
    memset(&s_config, 0, sizeof(s_config));
    s_config.root_path = s_default_root;
    s_config.max_log_size_mb = 100;
    s_config.log_retention_days = 7;
    s_config.trace_retention_days = 3;
    s_config.enable_auto_cleanup = true;
    s_config.enable_log_rotation = true;
    s_config.enable_trace_export = true;
    s_config.db_vacuum_interval_days = 7;
    s_config.circuit_breaker_threshold = heapstore_DEFAULT_CIRCUIT_THRESHOLD;
    s_config.circuit_breaker_timeout_sec = heapstore_DEFAULT_CIRCUIT_TIMEOUT_SEC;
}

static inline void circuit_breaker_record_success(void) {
    atomic_store(&s_circuit_breaker.failure_count, 0);
    atomic_store(&s_circuit_breaker.state, 0);
}

static inline void circuit_breaker_record_failure(void) {
    uint32_t count = atomic_fetch_add(&s_circuit_breaker.failure_count, 1) + 1;
    uint64_t now = (uint64_t)time(NULL);
    atomic_store(&s_circuit_breaker.last_failure_time, now);

    if (count >= s_circuit_breaker.threshold) {
        atomic_store(&s_circuit_breaker.state, 1);
        atomic_fetch_add(&s_metrics.circuit_breaker_trips, 1);
    }
}

static inline bool circuit_breaker_is_open(void) {
    uint32_t state = atomic_load(&s_circuit_breaker.state);
    if (state == 0) {
        return false;
    }
    if (state == 2) {
        return false;
    }

    uint64_t last_failure = atomic_load(&s_circuit_breaker.last_failure_time);
    uint64_t now = (uint64_t)time(NULL);
    if (now - last_failure >= s_circuit_breaker.timeout_sec) {
        atomic_store(&s_circuit_breaker.state, 2);
        return false;
    }
    return true;
}

static inline void update_metrics(uint64_t elapsed_ns, bool is_fast_path, bool is_failed) {
    atomic_fetch_add(&s_metrics.total_operations, 1);
    atomic_fetch_add(&s_metrics.total_operation_time_ns, elapsed_ns);

    uint32_t current_ops = atomic_fetch_add(&s_metrics.current_concurrent_ops, 1) + 1;
    uint64_t peak = atomic_load(&s_metrics.peak_concurrent_ops);
    while (current_ops > peak) {
        if (atomic_compare_exchange_weak(&s_metrics.peak_concurrent_ops, &peak, current_ops)) {
            break;
        }
    }
    atomic_fetch_sub(&s_metrics.current_concurrent_ops, 1);

    if (is_fast_path) {
        atomic_fetch_add(&s_metrics.fast_path_operations, 1);
    } else {
        atomic_fetch_add(&s_metrics.slow_path_operations, 1);
    }

    if (is_failed) {
        atomic_fetch_add(&s_metrics.failed_operations, 1);
    }
}

heapstore_error_t heapstore_init(const heapstore_config_t* manager) {
    if (s_initialized) {
        return heapstore_ERR_ALREADY_INITIALIZED;
    }

    set_default_config();

    if (manager && manager->root_path) {
        s_config.root_path = manager->root_path;
        if (manager->max_log_size_mb > 0) {
            s_config.max_log_size_mb = manager->max_log_size_mb;
        }
        if (manager->log_retention_days > 0) {
            s_config.log_retention_days = manager->log_retention_days;
        }
        if (manager->trace_retention_days > 0) {
            s_config.trace_retention_days = manager->trace_retention_days;
        }
        s_config.enable_auto_cleanup = manager->enable_auto_cleanup;
        s_config.enable_log_rotation = manager->enable_log_rotation;
        s_config.enable_trace_export = manager->enable_trace_export;
        if (manager->db_vacuum_interval_days > 0) {
            s_config.db_vacuum_interval_days = manager->db_vacuum_interval_days;
        }
        if (manager->circuit_breaker_threshold > 0) {
            s_config.circuit_breaker_threshold = manager->circuit_breaker_threshold;
            s_circuit_breaker.threshold = manager->circuit_breaker_threshold;
        }
        if (manager->circuit_breaker_timeout_sec > 0) {
            s_config.circuit_breaker_timeout_sec = manager->circuit_breaker_timeout_sec;
            s_circuit_breaker.timeout_sec = manager->circuit_breaker_timeout_sec;
        }
    }

    strncpy(s_root_path, s_config.root_path, sizeof(s_root_path) - 1);
    s_root_path[sizeof(s_root_path) - 1] = '\0';

    if (!heapstore_ensure_directory(s_root_path)) {
        return heapstore_ERR_DIR_CREATE_FAILED;
    }

    for (size_t i = 0; i < sizeof(s_path_order) / sizeof(s_path_order[0]); i++) {
        char full_path[heapstore_MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", s_root_path, s_path_names[i]);

        if (!heapstore_ensure_directory(full_path)) {
            return heapstore_ERR_DIR_CREATE_FAILED;
        }

        size_t subpath_idx = (size_t)s_path_order[i];
        if (subpath_idx < sizeof(s_subpath_map) / sizeof(s_subpath_map[0])) {
            for (size_t j = 0; s_subpath_map[subpath_idx][j] != NULL; j++) {
                char sub_path[heapstore_MAX_PATH_LEN];
                snprintf(sub_path, sizeof(sub_path), "%s/%s", full_path, s_subpath_map[subpath_idx][j]);
                if (!heapstore_ensure_directory(sub_path)) {
                    return heapstore_ERR_DIR_CREATE_FAILED;
                }
            }
        }
    }

    atomic_init(&s_circuit_breaker.state, 0);
    atomic_init(&s_circuit_breaker.failure_count, 0);
    atomic_init(&s_circuit_breaker.last_failure_time, 0);

    atomic_init(&s_metrics.total_operations, 0);
    atomic_init(&s_metrics.failed_operations, 0);
    atomic_init(&s_metrics.fast_path_operations, 0);
    atomic_init(&s_metrics.slow_path_operations, 0);
    atomic_init(&s_metrics.circuit_breaker_trips, 0);
    atomic_init(&s_metrics.total_operation_time_ns, 0);
    atomic_init(&s_metrics.peak_concurrent_ops, 0);
    atomic_init(&s_metrics.current_concurrent_ops, 0);

    s_initialized = true;

    heapstore_error_t err = heapstore_registry_init();
    if (err != heapstore_SUCCESS) {
        s_initialized = false;
        return err;
    }

    err = heapstore_trace_init();
    if (err != heapstore_SUCCESS) {
        heapstore_registry_shutdown();
        s_initialized = false;
        return err;
    }

    err = heapstore_ipc_init();
    if (err != heapstore_SUCCESS) {
        heapstore_trace_shutdown();
        heapstore_registry_shutdown();
        s_initialized = false;
        return err;
    }

    err = heapstore_memory_init();
    if (err != heapstore_SUCCESS) {
        heapstore_ipc_shutdown();
        heapstore_trace_shutdown();
        heapstore_registry_shutdown();
        s_initialized = false;
        return err;
    }

    err = heapstore_log_init();
    if (err != heapstore_SUCCESS) {
        heapstore_memory_shutdown();
        heapstore_ipc_shutdown();
        heapstore_trace_shutdown();
        heapstore_registry_shutdown();
        s_initialized = false;
        return err;
    }

    return heapstore_SUCCESS;
}

void heapstore_shutdown(void) {
    if (s_initialized) {
        heapstore_log_shutdown();
        heapstore_trace_shutdown();
        heapstore_ipc_shutdown();
        heapstore_memory_shutdown();
        heapstore_registry_shutdown();
        s_initialized = false;
    }
}

bool heapstore_is_initialized(void) {
    return s_initialized;
}

const char* heapstore_get_root(void) {
    return s_root_path;
}

const char* heapstore_get_path(heapstore_path_type_t type) {
    if (type < 0 || type >= heapstore_PATH_MAX) {
        return NULL;
    }
    return s_path_names[type];
}

heapstore_error_t heapstore_get_full_path(heapstore_path_type_t type, char* buffer, size_t buffer_size) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!buffer || buffer_size == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (type < 0 || type >= heapstore_PATH_MAX) {
        return heapstore_ERR_INVALID_PARAM;
    }

    snprintf(buffer, buffer_size, "%s/%s", s_root_path, s_path_names[type]);
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_get_stats(heapstore_stats_t* stats) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!stats) {
        return heapstore_ERR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(*stats));

    for (size_t i = 0; i < sizeof(s_path_order) / sizeof(s_path_order[0]); i++) {
        uint64_t dir_size = 0;
        uint32_t file_count = 0;

        /* 获取对应路径的完整路径 */
        char full_path[512];
        const char* path_name = NULL;
        
        switch (s_path_order[i]) {
            case heapstore_PATH_LOGS:
                path_name = "logs";
                break;
            case heapstore_PATH_REGISTRY:
                path_name = "registry";
                break;
            case heapstore_PATH_TRACES:
                path_name = "traces";
                break;
            case heapstore_PATH_KERNEL_IPC:
                path_name = "kernel/ipc";
                break;
            case heapstore_PATH_KERNEL_MEMORY:
                path_name = "kernel/memory";
                break;
            default:
                continue;
        }
        
        snprintf(full_path, sizeof(full_path), "%s/%s", s_root_path, path_name);
        
        /* 计算目录大小 */
        heapstore_calculate_directory_size(full_path, &dir_size, &file_count);

        switch (s_path_order[i]) {
            case heapstore_PATH_LOGS:
                stats->log_usage_bytes += dir_size;
                stats->log_file_count += file_count;
                break;
            case heapstore_PATH_REGISTRY:
                stats->registry_usage_bytes += dir_size;
                break;
            case heapstore_PATH_TRACES:
                stats->trace_usage_bytes += dir_size;
                stats->trace_file_count += file_count;
                break;
            case heapstore_PATH_KERNEL_IPC:
                stats->ipc_usage_bytes += dir_size;
                break;
            case heapstore_PATH_KERNEL_MEMORY:
                stats->memory_usage_bytes += dir_size;
                break;
            default:
                break;
        }
        stats->total_disk_usage_bytes += dir_size;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_log_write_fast(const char* service, int level, const char* message) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!message) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (circuit_breaker_is_open()) {
        return heapstore_ERR_CIRCUIT_OPEN;
    }

    uint64_t start_time = 0;
    bool is_failed = false;

    heapstore_error_t result = heapstore_log_write(level, service, NULL, NULL, 0, message);

    if (result != heapstore_SUCCESS) {
        is_failed = true;
        circuit_breaker_record_failure();
    } else {
        circuit_breaker_record_success();
    }

    update_metrics(0, true, is_failed);

    return result;
}

heapstore_error_t heapstore_log_write_slow(const char* service, int level, const char* message, const char* trace_id, uint32_t timeout_ms) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!message) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (circuit_breaker_is_open()) {
        return heapstore_ERR_CIRCUIT_OPEN;
    }

    bool is_failed = false;

    heapstore_error_t result = heapstore_log_write(level, service, trace_id, NULL, 0, message);

    if (result != heapstore_SUCCESS) {
        is_failed = true;
        circuit_breaker_record_failure();
    } else {
        circuit_breaker_record_success();
    }

    update_metrics(0, false, is_failed);

    return result;
}

heapstore_error_t heapstore_cleanup(bool dry_run, uint64_t* freed_bytes) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!s_config.enable_auto_cleanup) {
        if (freed_bytes) {
            *freed_bytes = 0;
        }
        return heapstore_SUCCESS;
    }

    uint64_t total_freed = 0;
    heapstore_error_t result = heapstore_SUCCESS;

    uint64_t log_freed = 0;
    heapstore_error_t log_err = heapstore_log_cleanup(s_config.log_retention_days, &log_freed);
    if (log_err == heapstore_SUCCESS) {
        total_freed += log_freed;
    } else {
        result = log_err;
    }

    uint64_t trace_freed = 0;
    heapstore_error_t trace_err = heapstore_trace_cleanup(s_config.trace_retention_days, &trace_freed);
    if (trace_err == heapstore_SUCCESS) {
        total_freed += trace_freed;
    } else {
        if (result == heapstore_SUCCESS) {
            result = trace_err;
        }
    }

    if (freed_bytes) {
        *freed_bytes = dry_run ? 0 : total_freed;
    }

    return result;
}

const char* heapstore_strerror(heapstore_error_t err) {
    switch (err) {
        case heapstore_SUCCESS:
            return "Success";
        case heapstore_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case heapstore_ERR_NOT_INITIALIZED:
            return "heapstore not initialized";
        case heapstore_ERR_ALREADY_INITIALIZED:
            return "heapstore already initialized";
        case heapstore_ERR_DIR_CREATE_FAILED:
            return "Failed to create directory";
        case heapstore_ERR_DIR_NOT_FOUND:
            return "Directory not found";
        case heapstore_ERR_PERMISSION_DENIED:
            return "Permission denied";
        case heapstore_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case heapstore_ERR_DB_INIT_FAILED:
            return "Database initialization failed";
        case heapstore_ERR_DB_QUERY_FAILED:
            return "Database query failed";
        case heapstore_ERR_FILE_OPEN_FAILED:
            return "Failed to open file";
        case heapstore_ERR_CONFIG_INVALID:
            return "Invalid configuration";
        case heapstore_ERR_FILE_OPERATION_FAILED:
            return "File operation failed";
        case heapstore_ERR_FILE_NOT_FOUND:
            return "File not found";
        case heapstore_ERR_NOT_FOUND:
            return "Not found";
        case heapstore_ERR_CIRCUIT_OPEN:
            return "Circuit breaker is open";
        case heapstore_ERR_TIMEOUT:
            return "Operation timeout";
        case heapstore_ERR_INTERNAL:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

heapstore_error_t heapstore_reload_config(const heapstore_config_t* manager) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!manager) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (manager->max_log_size_mb > 0) {
        s_config.max_log_size_mb = manager->max_log_size_mb;
    }
    if (manager->log_retention_days > 0) {
        s_config.log_retention_days = manager->log_retention_days;
    }
    if (manager->trace_retention_days > 0) {
        s_config.trace_retention_days = manager->trace_retention_days;
    }
    s_config.enable_auto_cleanup = manager->enable_auto_cleanup;
    s_config.enable_log_rotation = manager->enable_log_rotation;
    s_config.enable_trace_export = manager->enable_trace_export;
    if (manager->db_vacuum_interval_days > 0) {
        s_config.db_vacuum_interval_days = manager->db_vacuum_interval_days;
    }
    if (manager->circuit_breaker_threshold > 0) {
        s_config.circuit_breaker_threshold = manager->circuit_breaker_threshold;
        s_circuit_breaker.threshold = manager->circuit_breaker_threshold;
    }
    if (manager->circuit_breaker_timeout_sec > 0) {
        s_config.circuit_breaker_timeout_sec = manager->circuit_breaker_timeout_sec;
        s_circuit_breaker.timeout_sec = manager->circuit_breaker_timeout_sec;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_flush(void) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    heapstore_error_t err = heapstore_trace_flush();
    if (err != heapstore_SUCCESS) {
        return err;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_health_check(bool* registry_ok, 
                                       bool* trace_ok, 
                                       bool* log_ok, 
                                       bool* ipc_ok, 
                                       bool* memory_ok) {
    if (!s_initialized) {
        if (registry_ok) *registry_ok = false;
        if (trace_ok) *trace_ok = false;
        if (log_ok) *log_ok = false;
        if (ipc_ok) *ipc_ok = false;
        if (memory_ok) *memory_ok = false;
        return heapstore_ERR_NOT_INITIALIZED;
    }

    bool all_healthy = true;

    if (registry_ok) {
        *registry_ok = heapstore_registry_is_healthy();
        if (!*registry_ok) all_healthy = false;
    }
    
    if (trace_ok) {
        *trace_ok = heapstore_trace_is_healthy();
        if (!*trace_ok) all_healthy = false;
    }
    
    if (log_ok) {
        *log_ok = heapstore_log_is_healthy();
        if (!*log_ok) all_healthy = false;
    }
    
    if (ipc_ok) {
        *ipc_ok = heapstore_ipc_is_healthy();
        if (!*ipc_ok) all_healthy = false;
    }
    
    if (memory_ok) {
        *memory_ok = heapstore_memory_is_healthy();
        if (!*memory_ok) all_healthy = false;
    }

    if (circuit_breaker_is_open()) {
        all_healthy = false;
    }
    
    return all_healthy ? heapstore_SUCCESS : heapstore_ERR_INTERNAL;
}

heapstore_error_t heapstore_get_metrics(heapstore_metrics_t* metrics) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!metrics) {
        return heapstore_ERR_INVALID_PARAM;
    }

    metrics->total_operations = atomic_load(&s_metrics.total_operations);
    metrics->failed_operations = atomic_load(&s_metrics.failed_operations);
    metrics->fast_path_operations = atomic_load(&s_metrics.fast_path_operations);
    metrics->slow_path_operations = atomic_load(&s_metrics.slow_path_operations);
    metrics->circuit_breaker_trips = atomic_load(&s_metrics.circuit_breaker_trips);
    metrics->peak_concurrent_ops = atomic_load(&s_metrics.peak_concurrent_ops);

    uint64_t total_ops = atomic_load(&s_metrics.total_operations);
    uint64_t total_time = atomic_load(&s_metrics.total_operation_time_ns);
    metrics->avg_operation_time_ns = (total_ops > 0) ? (double)total_time / total_ops : 0.0;

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_reset_metrics(void) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    atomic_store(&s_metrics.total_operations, 0);
    atomic_store(&s_metrics.failed_operations, 0);
    atomic_store(&s_metrics.fast_path_operations, 0);
    atomic_store(&s_metrics.slow_path_operations, 0);
    atomic_store(&s_metrics.circuit_breaker_trips, 0);
    atomic_store(&s_metrics.total_operation_time_ns, 0);
    atomic_store(&s_metrics.peak_concurrent_ops, 0);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_get_circuit_state(heapstore_circuit_info_t* info) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!info) {
        return heapstore_ERR_INVALID_PARAM;
    }

    uint32_t state = atomic_load(&s_circuit_breaker.state);
    info->state = (heapstore_circuit_state_t)state;
    info->failure_count = atomic_load(&s_circuit_breaker.failure_count);
    info->last_failure_time = atomic_load(&s_circuit_breaker.last_failure_time);
    info->threshold = s_circuit_breaker.threshold;
    info->timeout_sec = s_circuit_breaker.timeout_sec;

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_reset_circuit(void) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    atomic_store(&s_circuit_breaker.state, 0);
    atomic_store(&s_circuit_breaker.failure_count, 0);
    atomic_store(&s_circuit_breaker.last_failure_time, 0);

    return heapstore_SUCCESS;
}

/* ==================== 批量写入实现 ==================== */

#define HEAPSTORE_BATCH_MAX_ITEMS 1024

typedef enum {
    HEAPSTORE_BATCH_ITEM_LOG,
    HEAPSTORE_BATCH_ITEM_SPAN,
    HEAPSTORE_BATCH_ITEM_SESSION,
    HEAPSTORE_BATCH_ITEM_AGENT,
    HEAPSTORE_BATCH_ITEM_SKILL,
    HEAPSTORE_BATCH_ITEM_MEMORY_POOL,
    HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC,
    HEAPSTORE_BATCH_ITEM_IPC_CHANNEL,
    HEAPSTORE_BATCH_ITEM_IPC_BUFFER
} heapstore_batch_item_type_t;

typedef struct heapstore_batch_item {
    heapstore_batch_item_type_t type;
    union {
        struct {
            char service[128];
            int level;
            char trace_id[64];
            char message[1024];
        } log;
        struct {
            char trace_id[64];
            char span_id[64];
            char parent_span_id[64];
            char name[256];
            int64_t start_time_us;
            int64_t end_time_us;
            int status;
            char attributes[2048];
        } span;
        heapstore_session_record_t session;
        heapstore_agent_record_t agent;
        heapstore_skill_record_t skill;
        heapstore_memory_pool_t memory_pool;
        heapstore_memory_allocation_t memory_alloc;
        heapstore_ipc_channel_t ipc_channel;
        heapstore_ipc_buffer_t ipc_buffer;
    } data;
    struct heapstore_batch_item* next;
} heapstore_batch_item_t;

struct heapstore_batch_context {
    size_t capacity;
    size_t count;
    heapstore_batch_item_t* head;
    heapstore_batch_item_t* tail;
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
};

heapstore_batch_context_t* heapstore_batch_begin(size_t batch_size) {
    heapstore_batch_context_t* ctx = (heapstore_batch_context_t*)malloc(sizeof(heapstore_batch_context_t));
    if (!ctx) {
        return NULL;
    }
    memset(ctx, 0, sizeof(heapstore_batch_context_t));
    ctx->capacity = (batch_size > 0) ? batch_size : HEAPSTORE_BATCH_MAX_ITEMS;
    if (ctx->capacity > HEAPSTORE_BATCH_MAX_ITEMS) {
        ctx->capacity = HEAPSTORE_BATCH_MAX_ITEMS;
    }
#ifdef _WIN32
    InitializeCriticalSection(&ctx->lock);
#else
    pthread_mutex_init(&ctx->lock, NULL);
#endif
    return ctx;
}

heapstore_error_t heapstore_batch_add_log(
    heapstore_batch_context_t* ctx,
    const char* service,
    int level,
    const char* message) {
    if (!ctx || !service || !message) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_LOG;
    strncpy(item->data.log.service, service, sizeof(item->data.log.service) - 1);
    item->data.log.level = level;
    if (message) {
        strncpy(item->data.log.message, message, sizeof(item->data.log.message) - 1);
    }

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_log_with_trace(
    heapstore_batch_context_t* ctx,
    const char* service,
    int level,
    const char* trace_id,
    const char* message) {
    if (!ctx || !service || !message) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_LOG;
    strncpy(item->data.log.service, service, sizeof(item->data.log.service) - 1);
    item->data.log.level = level;
    if (trace_id) {
        strncpy(item->data.log.trace_id, trace_id, sizeof(item->data.log.trace_id) - 1);
    }
    if (message) {
        strncpy(item->data.log.message, message, sizeof(item->data.log.message) - 1);
    }

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_trace(
    heapstore_batch_context_t* ctx,
    const char* trace_id,
    const char* span_id,
    const char* parent_span_id,
    const char* name,
    int64_t start_time_us,
    int64_t end_time_us,
    int status,
    const char* attributes) {
    if (!ctx || !trace_id || !span_id || !name) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_SPAN;
    strncpy(item->data.span.trace_id, trace_id, sizeof(item->data.span.trace_id) - 1);
    strncpy(item->data.span.span_id, span_id, sizeof(item->data.span.span_id) - 1);
    if (parent_span_id) {
        strncpy(item->data.span.parent_span_id, parent_span_id, sizeof(item->data.span.parent_span_id) - 1);
    }
    strncpy(item->data.span.name, name, sizeof(item->data.span.name) - 1);
    item->data.span.start_time_us = start_time_us;
    item->data.span.end_time_us = end_time_us;
    item->data.span.status = status;
    if (attributes) {
        strncpy(item->data.span.attributes, attributes, sizeof(item->data.span.attributes) - 1);
    }

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_session(
    heapstore_batch_context_t* ctx,
    const heapstore_session_record_t* record) {
    if (!ctx || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_SESSION;
    memcpy(&item->data.session, record, sizeof(heapstore_session_record_t));

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_agent(
    heapstore_batch_context_t* ctx,
    const heapstore_agent_record_t* record) {
    if (!ctx || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_AGENT;
    memcpy(&item->data.agent, record, sizeof(heapstore_agent_record_t));

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_skill(
    heapstore_batch_context_t* ctx,
    const heapstore_skill_record_t* record) {
    if (!ctx || !record) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_SKILL;
    memcpy(&item->data.skill, record, sizeof(heapstore_skill_record_t));

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_memory_pool(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_pool_t* pool) {
    if (!ctx || !pool) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_MEMORY_POOL;
    memcpy(&item->data.memory_pool, pool, sizeof(heapstore_memory_pool_t));

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_allocation(
    heapstore_batch_context_t* ctx,
    const heapstore_memory_allocation_t* allocation) {
    if (!ctx || !allocation) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC;
    memcpy(&item->data.memory_alloc, allocation, sizeof(heapstore_memory_allocation_t));

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_ipc_channel(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_channel_t* channel) {
    if (!ctx || !channel) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_IPC_CHANNEL;
    memcpy(&item->data.ipc_channel, channel, sizeof(heapstore_ipc_channel_t));

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_ipc_buffer(
    heapstore_batch_context_t* ctx,
    const heapstore_ipc_buffer_t* buffer) {
    if (!ctx || !buffer) {
        return heapstore_ERR_INVALID_PARAM;
    }
    if (ctx->count >= ctx->capacity) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    heapstore_batch_item_t* item = (heapstore_batch_item_t*)malloc(sizeof(heapstore_batch_item_t));
    if (!item) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }
    memset(item, 0, sizeof(heapstore_batch_item_t));
    item->type = HEAPSTORE_BATCH_ITEM_IPC_BUFFER;
    memcpy(&item->data.ipc_buffer, buffer, sizeof(heapstore_ipc_buffer_t));

    if (ctx->tail) {
        ctx->tail->next = item;
        ctx->tail = item;
    } else {
        ctx->head = ctx->tail = item;
    }
    ctx->count++;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_batch_add_span(
    heapstore_batch_context_t* ctx,
    const heapstore_span_t* span) {
    if (!ctx || !span) {
        return heapstore_ERR_INVALID_PARAM;
    }
    return heapstore_batch_add_trace(ctx, span->trace_id, span->span_id,
        span->parent_span_id, span->name, span->start_time_ns,
        span->end_time_ns, 0, span->attributes);
}

heapstore_error_t heapstore_batch_commit(heapstore_batch_context_t* ctx) {
    if (!ctx) {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_error_t result = heapstore_SUCCESS;
    heapstore_batch_item_t* item = ctx->head;

    while (item) {
        heapstore_batch_item_t* next = item->next;

        heapstore_error_t err = heapstore_batch_process_single_item(item);

        if (err != heapstore_SUCCESS && result == heapstore_SUCCESS) {
            result = err;
        }

        free(item);
        item = next;
    }

    ctx->head = ctx->tail = NULL;
    ctx->count = 0;

    return result;
}

/**
 * @brief 处理单个批量写入项目
 *
 * 根据项目类型调用相应的存储子系统 API
 *
 * @param item [in] 批量写入项目
 * @return heapstore_error_t 错误码
 */
static heapstore_error_t heapstore_batch_process_single_item(const heapstore_batch_item_t* item) {
    if (!item) {
        return heapstore_ERR_INVALID_PARAM;
    }

    switch (item->type) {
        case HEAPSTORE_BATCH_ITEM_LOG:
            return heapstore_batch_commit_log(&item->data.log);

        case HEAPSTORE_BATCH_ITEM_SPAN:
            return heapstore_batch_commit_span(&item->data.span);

        case HEAPSTORE_BATCH_ITEM_SESSION:
            return heapstore_registry_add_session(&item->data.session);

        case HEAPSTORE_BATCH_ITEM_AGENT:
            return heapstore_registry_add_agent(&item->data.agent);

        case HEAPSTORE_BATCH_ITEM_SKILL:
            return heapstore_registry_add_skill(&item->data.skill);

        case HEAPSTORE_BATCH_ITEM_MEMORY_POOL:
            return heapstore_memory_record_pool(&item->data.memory_pool);

        case HEAPSTORE_BATCH_ITEM_MEMORY_ALLOC:
            return heapstore_memory_record_allocation(&item->data.memory_alloc);

        case HEAPSTORE_BATCH_ITEM_IPC_CHANNEL:
            return heapstore_ipc_record_channel(&item->data.ipc_channel);

        case HEAPSTORE_BATCH_ITEM_IPC_BUFFER:
            return heapstore_ipc_record_buffer(&item->data.ipc_buffer);

        default:
            return heapstore_ERR_INVALID_PARAM;
    }
}

/**
 * @brief 处理日志类型的批量写入
 */
static heapstore_error_t heapstore_batch_commit_log(const heapstore_log_entry_t* log_entry) {
    if (!log_entry) {
        return heapstore_ERR_INVALID_PARAM;
    }

    return heapstore_log_write(
        log_entry->level,
        log_entry->service,
        log_entry->trace_id[0] ? log_entry->trace_id : NULL,
        NULL, 0, log_entry->message);
}

/**
 * @brief 处理 Span 类型的批量写入
 *
 * 包含单位转换（微秒→纳秒）和内存管理
 */
static heapstore_error_t heapstore_batch_commit_span(const heapstore_trace_entry_t* trace_entry) {
    if (!trace_entry) {
        return heapstore_ERR_INVALID_PARAM;
    }

    heapstore_span_t span_rec;
    memset(&span_rec, 0, sizeof(span_rec));

    /* 复制基本字段 */
    strncpy(span_rec.trace_id, trace_entry->trace_id, sizeof(span_rec.trace_id) - 1);
    strncpy(span_rec.span_id, trace_entry->span_id, sizeof(span_rec.span_id) - 1);

    /* 可选字段：parent_span_id */
    if (trace_entry->parent_span_id[0]) {
        strncpy(span_rec.parent_span_id, trace_entry->parent_span_id, sizeof(span_rec.parent_span_id) - 1);
    }

    /* 必填字段 */
    strncpy(span_rec.name, trace_entry->name, sizeof(span_rec.name) - 1);

    /* 单位转换：微秒 → 纳秒 */
    span_rec.start_time_ns = (uint64_t)trace_entry->start_time_us * 1000ULL;
    span_rec.end_time_ns = (uint64_t)trace_entry->end_time_us * 1000ULL;

    /* 状态转换：int → string */
    snprintf(span_rec.status, sizeof(span_rec.status), "%d", trace_entry->status);

    /* 属性处理：需要深拷贝 */
    if (trace_entry->attributes[0]) {
        span_rec.attributes = strdup(trace_entry->attributes);
        span_rec.attribute_count = 1;
    }

    /* 调用 trace 写入 API */
    heapstore_error_t err = heapstore_trace_write_span(&span_rec);

    /* 释放动态分配的属性内存 */
    if (span_rec.attributes) {
        free(span_rec.attributes);
        span_rec.attributes = NULL;
    }

    return err;
}

void heapstore_batch_rollback(heapstore_batch_context_t* ctx) {
    if (!ctx) {
        return;
    }

    heapstore_batch_item_t* item = ctx->head;
    while (item) {
        heapstore_batch_item_t* next = item->next;
        free(item);
        item = next;
    }

    ctx->head = ctx->tail = NULL;
    ctx->count = 0;
}

void heapstore_batch_context_destroy(heapstore_batch_context_t* ctx) {
    if (!ctx) {
        return;
    }

    heapstore_batch_rollback(ctx);
#ifdef _WIN32
    DeleteCriticalSection(&ctx->lock);
#else
    pthread_mutex_destroy(&ctx->lock);
#endif
    free(ctx);
}

size_t heapstore_batch_get_count(const heapstore_batch_context_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->count;
}

size_t heapstore_batch_get_capacity(const heapstore_batch_context_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->capacity;
}
