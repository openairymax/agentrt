/**
 * @file heapstore_memory.c
 * @brief AgentOS 数据分区内存管理数据存储实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "heapstore_memory.h"
#include "private.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#define heapstore_MEMORY_MAX_POOLS 64
#define heapstore_MEMORY_MAX_ALLOCATIONS 10000
#define heapstore_MEMORY_MAX_PATH 512

static bool s_initialized = false;
static pthread_mutex_t s_memory_lock = PTHREAD_MUTEX_INITIALIZER;
static heapstore_memory_pool_t s_pools[heapstore_MEMORY_MAX_POOLS];
static size_t s_pool_count = 0;
static heapstore_memory_allocation_t s_allocations[heapstore_MEMORY_MAX_ALLOCATIONS];
static size_t s_allocation_count = 0;
static char s_memory_path[heapstore_MEMORY_MAX_PATH] = {0};

heapstore_error_t heapstore_memory_init(void) {
    if (s_initialized) {
        return heapstore_SUCCESS;
    }

    const char* base_path = "agentos/heapstore/kernel/memory";
    strncpy(s_memory_path, base_path, sizeof(s_memory_path) - 1);
    s_memory_path[sizeof(s_memory_path) - 1] = '\0';

    heapstore_error_t err;
    err = heapstore_ensure_directory(s_memory_path);
    if (err != heapstore_SUCCESS) return err;

    char pools_path[heapstore_MEMORY_MAX_PATH];
    snprintf(pools_path, sizeof(pools_path), "%s/pools", s_memory_path);
    err = heapstore_ensure_directory(pools_path);
    if (err != heapstore_SUCCESS) return err;

    char allocations_path[heapstore_MEMORY_MAX_PATH];
    snprintf(allocations_path, sizeof(allocations_path), "%s/allocations", s_memory_path);
    err = heapstore_ensure_directory(allocations_path);
    if (err != heapstore_SUCCESS) return err;

    char stats_path[heapstore_MEMORY_MAX_PATH];
    snprintf(stats_path, sizeof(stats_path), "%s/stats", s_memory_path);
    err = heapstore_ensure_directory(stats_path);
    if (err != heapstore_SUCCESS) return err;

    memset(s_pools, 0, sizeof(s_pools));
    memset(s_allocations, 0, sizeof(s_allocations));
    s_pool_count = 0;
    s_allocation_count = 0;

    s_initialized = true;

    return heapstore_SUCCESS;
}

void heapstore_memory_shutdown(void) {
    if (!s_initialized) {
        return;
    }

    pthread_mutex_lock(&s_memory_lock);

    memset(s_pools, 0, sizeof(s_pools));
    memset(s_allocations, 0, sizeof(s_allocations));
    s_pool_count = 0;
    s_allocation_count = 0;

    s_initialized = false;
    pthread_mutex_unlock(&s_memory_lock);
}

heapstore_error_t heapstore_memory_record_pool(const heapstore_memory_pool_t* pool) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!pool) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_memory_lock);

    if (s_pool_count >= heapstore_MEMORY_MAX_POOLS) {
        pthread_mutex_unlock(&s_memory_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < s_pool_count; i++) {
        if (strcmp(s_pools[i].pool_id, pool->pool_id) == 0) {
            memcpy(&s_pools[i], pool, sizeof(heapstore_memory_pool_t));
            pthread_mutex_unlock(&s_memory_lock);
            return heapstore_SUCCESS;
        }
    }

    memcpy(&s_pools[s_pool_count], pool, sizeof(heapstore_memory_pool_t));
    s_pool_count++;

    pthread_mutex_unlock(&s_memory_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_memory_get_pool(const char* pool_id, heapstore_memory_pool_t* pool) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!pool_id || !pool) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_memory_lock);

    for (size_t i = 0; i < s_pool_count; i++) {
        if (strcmp(s_pools[i].pool_id, pool_id) == 0) {
            memcpy(pool, &s_pools[i], sizeof(heapstore_memory_pool_t));
            pthread_mutex_unlock(&s_memory_lock);
            return heapstore_SUCCESS;
        }
    }

    pthread_mutex_unlock(&s_memory_lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_memory_update_pool_usage(const char* pool_id, size_t used_size, uint32_t free_block_count) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!pool_id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_memory_lock);

    for (size_t i = 0; i < s_pool_count; i++) {
        if (strcmp(s_pools[i].pool_id, pool_id) == 0) {
            s_pools[i].used_size = used_size;
            s_pools[i].free_block_count = free_block_count;
            pthread_mutex_unlock(&s_memory_lock);
            return heapstore_SUCCESS;
        }
    }

    pthread_mutex_unlock(&s_memory_lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_memory_record_allocation(const heapstore_memory_allocation_t* allocation) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!allocation) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_memory_lock);

    if (s_allocation_count >= heapstore_MEMORY_MAX_ALLOCATIONS) {
        pthread_mutex_unlock(&s_memory_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < s_allocation_count; i++) {
        if (strcmp(s_allocations[i].allocation_id, allocation->allocation_id) == 0) {
            memcpy(&s_allocations[i], allocation, sizeof(heapstore_memory_allocation_t));
            pthread_mutex_unlock(&s_memory_lock);
            return heapstore_SUCCESS;
        }
    }

    memcpy(&s_allocations[s_allocation_count], allocation, sizeof(heapstore_memory_allocation_t));
    s_allocation_count++;

    pthread_mutex_unlock(&s_memory_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_memory_get_allocation(const char* allocation_id, heapstore_memory_allocation_t* allocation) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!allocation_id || !allocation) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_memory_lock);

    for (size_t i = 0; i < s_allocation_count; i++) {
        if (strcmp(s_allocations[i].allocation_id, allocation_id) == 0) {
            memcpy(allocation, &s_allocations[i], sizeof(heapstore_memory_allocation_t));
            pthread_mutex_unlock(&s_memory_lock);
            return heapstore_SUCCESS;
        }
    }

    pthread_mutex_unlock(&s_memory_lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_memory_free_allocation(const char* allocation_id) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!allocation_id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_memory_lock);

    for (size_t i = 0; i < s_allocation_count; i++) {
        if (strcmp(s_allocations[i].allocation_id, allocation_id) == 0) {
            s_allocations[i].freed_at = (uint64_t)time(NULL);
            strncpy(s_allocations[i].status, "freed", sizeof(s_allocations[i].status) - 1);
            pthread_mutex_unlock(&s_memory_lock);
            return heapstore_SUCCESS;
        }
    }

    pthread_mutex_unlock(&s_memory_lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_memory_get_stats(uint32_t* pool_count, uint32_t* total_allocations, uint64_t* total_size) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_memory_lock);

    if (pool_count) {
        *pool_count = (uint32_t)s_pool_count;
    }
    if (total_allocations) {
        *total_allocations = (uint32_t)s_allocation_count;
    }
    if (total_size) {
        uint64_t size = 0;
        for (size_t i = 0; i < s_pool_count; i++) {
            size += s_pools[i].total_size;
        }
        *total_size = size;
    }

    pthread_mutex_unlock(&s_memory_lock);

    return heapstore_SUCCESS;
}

bool heapstore_memory_is_healthy(void) {
    return s_initialized;
}
