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
