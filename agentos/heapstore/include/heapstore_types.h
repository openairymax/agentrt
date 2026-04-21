/**
 * @file heapstore_types.h
 * @brief AgentOS heapstore 共享类型定义（打破循环依赖）
 *
 * 本文件集中定义所有子模块间共享的结构体类型，
 * 避免循环包含导致的类型可见性问题。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AGENTOS_HEAPSTORE_TYPES_H
#define AGENTOS_HEAPSTORE_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 注册表类型 ========== */

typedef struct heapstore_agent_record {
    char id[128];
    char name[256];
    char type[64];
    char version[32];
    char status[32];
    char config_path[512];
    uint64_t created_at;
    uint64_t updated_at;
} heapstore_agent_record_t;

typedef struct heapstore_skill_record {
    char id[128];
    char name[256];
    char version[32];
    char library_path[512];
    char manifest_path[512];
    uint64_t installed_at;
} heapstore_skill_record_t;

typedef struct heapstore_session_record {
    char id[128];
    char user_id[128];
    uint64_t created_at;
    uint64_t last_active_at;
    uint32_t ttl_seconds;
    char status[32];
} heapstore_session_record_t;

/* ========== 内存管理类型 ========== */

typedef struct heapstore_memory_pool {
    char pool_id[128];
    char name[256];
    size_t total_size;
    size_t used_size;
    size_t block_size;
    uint32_t block_count;
    uint32_t free_block_count;
    uint64_t created_at;
    char status[32];
} heapstore_memory_pool_t;

typedef struct heapstore_memory_allocation {
    char allocation_id[128];
    char pool_id[128];
    size_t size;
    uint64_t address;
    uint64_t allocated_at;
    uint64_t freed_at;
    char status[32];
} heapstore_memory_allocation_t;

/* ========== IPC 类型 ========== */

typedef struct heapstore_ipc_channel {
    char channel_id[128];
    char name[256];
    char type[32];
    char status[32];
    uint64_t created_at;
    uint64_t last_activity_at;
    size_t buffer_size;
    size_t current_usage;
} heapstore_ipc_channel_t;

typedef struct heapstore_ipc_buffer {
    char buffer_id[128];
    char channel_id[128];
    size_t size;
    size_t used;
    uint64_t created_at;
    char status[32];
} heapstore_ipc_buffer_t;

/* ========== Trace/Span 类型 ========== */

typedef struct heapstore_span {
    char trace_id[64];
    char span_id[32];
    char parent_span_id[32];
    char name[128];
    char kind[64];
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    char service_name[64];
    char status[32];
    void* attributes;
    size_t attribute_count;
} heapstore_span_t;

typedef struct heapstore_trace_entry {
    char trace_id[64];
    char span_id[32];
    char parent_span_id[32];
    char name[128];
    char kind[64];
    uint64_t start_time_us;
    uint64_t end_time_us;
    int status;
    char attributes[256];
} heapstore_trace_entry_t;

/* ========== 日志类型 ========== */

typedef struct heapstore_log_entry {
    int level;
    char service[64];
    char trace_id[64];
    char message[1024];
    time_t timestamp;
} heapstore_log_entry_t;

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_HEAPSTORE_TYPES_H */
