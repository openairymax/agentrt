/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sync_types.h
 * @brief 同步原语内部类型定义
 *
 * 本文件定义所有同步原语的内部结构体，供各平台实现文件使用。
 * 不对外暴露，仅供sync模块内部使用。
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-05
 */

#ifndef AGENTOS_SYNC_TYPES_H
#define AGENTOS_SYNC_TYPES_H

#include "memory_compat.h"
#include "sync.h"
#include "sync_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sync_mutex {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    platform_mutex_t mutex;
};

struct sync_recursive_mutex {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    size_t recursive_count;
    uint64_t owner_thread;
    platform_recursive_mutex_t mutex;
};

struct sync_rwlock {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    size_t read_count;
    bool is_writer;
    platform_rwlock_t rwlock;
};

struct sync_spinlock {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    platform_spinlock_t lock;
};

struct sync_semaphore {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    unsigned int max_value;
    platform_semaphore_t semaphore;
};

struct sync_condition {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    platform_condition_t cond;
};

struct sync_barrier {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    unsigned int count;
    unsigned int current;
    unsigned int generation;
    platform_barrier_t barrier;
};

struct sync_event {
    sync_type_t type;
    bool initialized;
    const char *name;
    sync_stats_t stats;
    bool manual_reset;
    bool signaled;
    platform_event_t event;
};

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SYNC_TYPES_H */
