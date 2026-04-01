/**
 * @file heapstore_integration.c
 * @brief heapstore 与 AgentOS 核心模块集成实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "heapstore_integration.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool g_integration_initialized = false;
static char g_root_path[512] = {0};

#ifdef _WIN32
static CRITICAL_SECTION g_integration_mutex;
#else
static pthread_mutex_t g_integration_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void integration_lock_init(void) {
#ifdef _WIN32
    InitializeCriticalSection(&g_integration_mutex);
#endif
}

static void integration_lock_cleanup(void) {
#ifdef _WIN32
    DeleteCriticalSection(&g_integration_mutex);
#endif
}

static void integration_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_integration_mutex);
#else
    pthread_mutex_lock(&g_integration_mutex);
#endif
}

static void integration_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_integration_mutex);
#else
    pthread_mutex_unlock(&g_integration_mutex);
#endif
}

agentos_error_t heapstore_integration_init(const char* root_path) {
    integration_lock();
    
    if (g_integration_initialized) {
        integration_unlock();
        return AGENTOS_SUCCESS;
    }
    
    heapstore_config_t config = {
        .root_path = root_path ? root_path : "heapstore",
        .max_log_size_mb = 100,
        .log_retention_days = 7,
        .trace_retention_days = 3,
        .enable_auto_cleanup = true,
        .enable_log_rotation = true,
        .enable_trace_export = true,
        .db_vacuum_interval_days = 7,
        .circuit_breaker_threshold = 5,
        .circuit_breaker_timeout_sec = 30
    };
    
    heapstore_error_t err = heapstore_init(&config);
    if (err != heapstore_SUCCESS) {
        integration_unlock();
        return AGENTOS_EIO;
    }
    
    if (root_path) {
        strncpy(g_root_path, root_path, sizeof(g_root_path) - 1);
        g_root_path[sizeof(g_root_path) - 1] = '\0';
    } else {
        strncpy(g_root_path, "heapstore", sizeof(g_root_path) - 1);
    }
    
    g_integration_initialized = true;
    integration_unlock();
    
    return AGENTOS_SUCCESS;
}

void heapstore_integration_shutdown(void) {
    integration_lock();
    
    if (!g_integration_initialized) {
        integration_unlock();
        return;
    }
    
    heapstore_shutdown();
    g_integration_initialized = false;
    g_root_path[0] = '\0';
    
    integration_unlock();
}

agentos_error_t heapstore_syscall_session_save(
    const char* session_id,
    const char* metadata,
    uint64_t created_ns,
    uint64_t last_active_ns) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!session_id) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_session_record_t record;
    memset(&record, 0, sizeof(record));
    strncpy(record.id, session_id, sizeof(record.id) - 1);
    if (metadata) {
        strncpy(record.user_id, metadata, sizeof(record.user_id) - 1);
    }
    record.created_at = created_ns;
    record.last_active_at = last_active_ns;
    record.ttl_seconds = 0;
    strncpy(record.status, "active", sizeof(record.status) - 1);
    
    heapstore_error_t err = heapstore_registry_add_session(&record);
    return (err == heapstore_SUCCESS) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t heapstore_syscall_session_load(
    const char* session_id,
    char** out_metadata,
    uint64_t* out_created_ns,
    uint64_t* out_last_active_ns) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!session_id || !out_metadata) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_session_record_t record;
    memset(&record, 0, sizeof(record));
    
    heapstore_error_t err = heapstore_registry_get_session(session_id, &record);
    if (err != heapstore_SUCCESS) {
        return (err == heapstore_ERR_NOT_FOUND) ? AGENTOS_ENOENT : AGENTOS_EIO;
    }
    
    if (out_metadata) {
        *out_metadata = strdup(record.user_id);
    }
    if (out_created_ns) {
        *out_created_ns = record.created_at;
    }
    if (out_last_active_ns) {
        *out_last_active_ns = record.last_active_at;
    }
    
    return AGENTOS_SUCCESS;
}

agentos_error_t heapstore_syscall_session_delete(const char* session_id) {
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!session_id) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_error_t err = heapstore_registry_remove_session(session_id);
    return (err == heapstore_SUCCESS) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t heapstore_syscall_session_list(
    char*** out_sessions,
    size_t* out_count) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!out_sessions || !out_count) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_session_iter_t iter;
    heapstore_error_t err = heapstore_registry_sessions_begin(&iter);
    if (err != heapstore_SUCCESS) {
        return AGENTOS_EIO;
    }
    
    size_t count = 0;
    size_t capacity = 16;
    char** sessions = (char**)malloc(capacity * sizeof(char*));
    if (!sessions) {
        return AGENTOS_ENOMEM;
    }
    
    while (heapstore_registry_sessions_next(&iter)) {
        if (count >= capacity) {
            capacity *= 2;
            char** new_sessions = (char**)realloc(sessions, capacity * sizeof(char*));
            if (!new_sessions) {
                for (size_t i = 0; i < count; i++) {
                    free(sessions[i]);
                }
                free(sessions);
                return AGENTOS_ENOMEM;
            }
            sessions = new_sessions;
        }
        sessions[count] = strdup(iter.current.session_id);
        if (!sessions[count]) {
            for (size_t i = 0; i < count; i++) {
                free(sessions[i]);
            }
            free(sessions);
            return AGENTOS_ENOMEM;
        }
        count++;
    }
    
    heapstore_registry_sessions_end(&iter);
    
    *out_sessions = sessions;
    *out_count = count;
    
    return AGENTOS_SUCCESS;
}

agentos_error_t heapstore_syscall_trace_save(
    const char* trace_id,
    const char* span_id,
    const char* parent_id,
    const char* name,
    int64_t start_time_us,
    int64_t end_time_us,
    int status,
    const char* events_json) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!trace_id || !span_id || !name) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_span_record_t record = {
        .trace_id = (char*)trace_id,
        .span_id = (char*)span_id,
        .parent_span_id = (char*)parent_id,
        .name = (char*)name,
        .start_time_us = start_time_us,
        .end_time_us = end_time_us,
        .status = status,
        .attributes = (char*)events_json
    };
    
    heapstore_error_t err = heapstore_trace_write_span(&record);
    return (err == heapstore_SUCCESS) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t heapstore_syscall_trace_export(char** out_traces) {
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!out_traces) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_trace_exporter_config_t config = {
        .format = heapstore_TRACE_EXPORT_JSON,
        .include_events = true
    };
    
    heapstore_error_t err = heapstore_trace_export(&config, out_traces);
    return (err == heapstore_SUCCESS) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t heapstore_memoryrovol_save(
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!data || len == 0 || !out_record_id) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_memory_pool_t pool;
    memset(&pool, 0, sizeof(pool));
    
    snprintf(pool.pool_id, "mem_raw_%llu", (unsigned long long)time(NULL));
    strncpy(pool.name, "memoryrovol_raw", sizeof(pool.name) - 1);
    pool.total_size = len;
    pool.used_size = len;
    pool.block_size = 0;
    pool.block_count = 0;
    pool.free_block_count = 0;
    pool.created_at = (uint64_t)time(NULL);
    strncpy(pool.status, "active", sizeof(pool.status) - 1);
    
    heapstore_error_t err = heapstore_memory_record_pool(&pool);
    if (err != heapstore_SUCCESS) {
        return AGENTOS_EIO;
    }
    
    *out_record_id = strdup(pool.pool_id);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t heapstore_memoryrovol_load(
    const char* record_id,
    void** out_data,
    size_t* out_len,
    char** out_metadata) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!record_id || !out_data || !out_len) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_memory_pool_t pool;
    memset(&pool, 0, sizeof(pool));
    
    heapstore_error_t err = heapstore_memory_get_pool(record_id, &pool);
    if (err != heapstore_SUCCESS) {
        return (err == heapstore_ERR_NOT_FOUND) ? AGENTOS_ENOENT : AGENTOS_EIO;
    }
    
    *out_data = malloc(pool.total_size);
    if (!*out_data) {
        if (pool.name) free(pool.name);
        return AGENTOS_ENOMEM;
    }
    memcpy(*out_data, &pool, sizeof(pool));
    *out_len = pool.total_size;
    
    if (out_metadata && pool.name) {
        *out_metadata = strdup(pool.name);
    }
    
    return AGENTOS_SUCCESS;
}

agentos_error_t heapstore_memoryrovol_delete(const char* record_id) {
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!record_id) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_error_t err = heapstore_memory_delete_pool(record_id);
    return (err == heapstore_SUCCESS) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t heapstore_ipc_channel_save(
    const char* channel_id,
    const char* state_json) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!channel_id || !state_json) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_channel_record_t record = {
        .channel_id = (char*)channel_id,
        .name = (char*)channel_id,
        .state = heapstore_CHANNEL_STATE_ACTIVE,
        .message_count = 0,
        .total_bytes = 0
    };
    
    heapstore_error_t err = heapstore_ipc_write_channel(&record);
    return (err == heapstore_SUCCESS) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t heapstore_ipc_channel_load(
    const char* channel_id,
    char** out_state) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!channel_id || !out_state) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_channel_record_t record;
    memset(&record, 0, sizeof(record));
    
    heapstore_error_t err = heapstore_ipc_get_channel(channel_id, &record);
    if (err != heapstore_SUCCESS) {
        return (err == heapstore_ERR_NOT_FOUND) ? AGENTOS_ENOENT : AGENTOS_EIO;
    }
    
    char buffer[512];
    snprintf(buffer, sizeof(buffer), 
        "{\"channel_id\":\"%s\",\"state\":%d,\"message_count\":%llu,\"total_bytes\":%llu}",
        record.channel_id ? record.channel_id : "",
        record.state,
        (unsigned long long)record.message_count,
        (unsigned long long)record.total_bytes);
    
    *out_state = strdup(buffer);
    
    if (record.channel_id) free(record.channel_id);
    if (record.name) free(record.name);
    
    return *out_state ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t heapstore_logging_write(
    const char* module,
    int level,
    const char* trace_id,
    const char* message,
    uint64_t timestamp_ns) {
    
    if (!g_integration_initialized) {
        return AGENTOS_ENOTINIT;
    }
    if (!module || !message) {
        return AGENTOS_EINVAL;
    }
    
    heapstore_log_level_t log_level;
    switch (level) {
        case 0: log_level = heapstore_LOG_DEBUG; break;
        case 1: log_level = heapstore_LOG_INFO; break;
        case 2: log_level = heapstore_LOG_WARN; break;
        case 3: log_level = heapstore_LOG_ERROR; break;
        default: log_level = heapstore_LOG_INFO; break;
    }
    
    heapstore_log_file_info_t info;
    memset(&info, 0, sizeof(info));
    
    heapstore_error_t err = heapstore_log_write(
        module, log_level, trace_id, message, &info);
    
    return (err == heapstore_SUCCESS) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t heapstore_integration_health_check(char** out_health_json) {
    if (!out_health_json) {
        return AGENTOS_EINVAL;
    }
    
    bool registry_ok = false, trace_ok = false, log_ok = false;
    bool ipc_ok = false, memory_ok = false;
    
    if (g_integration_initialized) {
        heapstore_health_check(&registry_ok, &trace_ok, &log_ok, &ipc_ok, &memory_ok);
    }
    
    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
        "{"
        "\"initialized\":%s,"
        "\"registry\":%s,"
        "\"trace\":%s,"
        "\"log\":%s,"
        "\"ipc\":%s,"
        "\"memory\":%s,"
        "\"overall\":%s"
        "}",
        g_integration_initialized ? "true" : "false",
        registry_ok ? "true" : "false",
        trace_ok ? "true" : "false",
        log_ok ? "true" : "false",
        ipc_ok ? "true" : "false",
        memory_ok ? "true" : "false",
        (g_integration_initialized && registry_ok && trace_ok && log_ok) ? "true" : "false");
    
    *out_health_json = strdup(buffer);
    return *out_health_json ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}

agentos_error_t heapstore_integration_get_stats(char** out_stats_json) {
    if (!out_stats_json) {
        return AGENTOS_EINVAL;
    }
    
    if (!g_integration_initialized) {
        *out_stats_json = strdup("{\"error\":\"not initialized\"}");
        return *out_stats_json ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    }
    
    heapstore_stats_t stats;
    heapstore_metrics_t metrics;
    
    heapstore_error_t err1 = heapstore_get_stats(&stats);
    heapstore_error_t err2 = heapstore_get_metrics(&metrics);
    
    if (err1 != heapstore_SUCCESS || err2 != heapstore_SUCCESS) {
        *out_stats_json = strdup("{\"error\":\"failed to get stats\"}");
        return *out_stats_json ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
    }
    
    char buffer[2048];
    snprintf(buffer, sizeof(buffer),
        "{"
        "\"disk_usage\":{"
        "\"total_bytes\":%llu,"
        "\"log_bytes\":%llu,"
        "\"registry_bytes\":%llu,"
        "\"trace_bytes\":%llu,"
        "\"ipc_bytes\":%llu,"
        "\"memory_bytes\":%llu"
        "},"
        "\"file_counts\":{"
        "\"log_files\":%u,"
        "\"trace_files\":%u"
        "},"
        "\"performance\":{"
        "\"total_operations\":%llu,"
        "\"failed_operations\":%llu,"
        "\"fast_path_ops\":%llu,"
        "\"slow_path_ops\":%llu,"
        "\"circuit_breaker_trips\":%llu,"
        "\"avg_operation_time_ns\":%.2f,"
        "\"peak_concurrent_ops\":%llu"
        "}"
        "}",
        (unsigned long long)stats.total_disk_usage_bytes,
        (unsigned long long)stats.log_usage_bytes,
        (unsigned long long)stats.registry_usage_bytes,
        (unsigned long long)stats.trace_usage_bytes,
        (unsigned long long)stats.ipc_usage_bytes,
        (unsigned long long)stats.memory_usage_bytes,
        stats.log_file_count,
        stats.trace_file_count,
        (unsigned long long)metrics.total_operations,
        (unsigned long long)metrics.failed_operations,
        (unsigned long long)metrics.fast_path_operations,
        (unsigned long long)metrics.slow_path_operations,
        (unsigned long long)metrics.circuit_breaker_trips,
        metrics.avg_operation_time_ns,
        (unsigned long long)metrics.peak_concurrent_ops);
    
    *out_stats_json = strdup(buffer);
    return *out_stats_json ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
}
