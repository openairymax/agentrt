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

#include "sync.h"
#include "memory_compat.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <synchapi.h>
#else
    #include <pthread.h>
    #include <semaphore.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 互斥锁内部结构
 */
struct sync_mutex {
    sync_type_t type;
    bool initialized;
    const char* name;
    sync_stats_t stats;
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
};

/**
 * @brief 递归互斥锁内部结构
 */
struct sync_recursive_mutex {
    sync_type_t type;
    bool initialized;
    const char* name;
    sync_stats_t stats;
    size_t recursive_count;
#ifdef _WIN32
    DWORD owner_thread;
    CRITICAL_SECTION mutex;
#else
    pthread_t owner_thread;
    pthread_mutex_t mutex;
#endif
};

/**
 * @brief 读写锁内部结构
 */
struct sync_rwlock {
    sync_type_t type;
    bool initialized;
    const char* name;
    sync_stats_t stats;
    size_t read_count;
    bool is_writer;
#ifdef _WIN32
    SRWLOCK rwlock;
#else
    pthread_rwlock_t rwlock;
#endif
};

/**
 * @brief 自旋锁内部结构
 */
struct sync_spinlock {
    sync_type_t type;
    bool initialized;
    const char* name;
    sync_stats_t stats;
#ifdef _WIN32
    volatile LONG lock;
#else
    pthread_spinlock_t lock;
#endif
};

/**
 * @brief 信号量内部结构
 */
struct sync_semaphore {
    sync_type_t type;
    bool initialized;
    const char* name;
    sync_stats_t stats;
    unsigned int max_value;
#ifdef _WIN32
    HANDLE semaphore;
#else
    sem_t semaphore;
#endif
};

/**
 * @brief 条件变量内部结构
 */
struct sync_condition {
    sync_type_t type;
    bool initialized;
    const char* name;
    sync_stats_t stats;
#ifdef _WIN32
    CONDITION_VARIABLE cond;
#else
    pthread_cond_t cond;
#endif
};

/**
 * @brief 屏障内部结构
 */
struct sync_barrier {
    sync_type_t type;
    bool initialized;
    const char* name;
    sync_stats_t stats;
    unsigned int count;
    unsigned int current;
    unsigned int generation;
#ifdef _WIN32
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cond;
#else
    pthread_barrier_t barrier;
#endif
};

/**
 * @brief 事件内部结构
 */
struct sync_event {
    sync_type_t type;
    bool initialized;
    const char* name;
    sync_stats_t stats;
    bool manual_reset;
    bool signaled;
#ifdef _WIN32
    HANDLE event;
#else
    pthread_mutex_t mutex;
    pthread_cond_t cond;
#endif
};

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SYNC_TYPES_H */
