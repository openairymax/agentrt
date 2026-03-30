/**
 * @file heapstore_memory.c
 * @brief AgentOS 数据分区内存管理数据存储实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "heapstore_memory.h"
#include "private.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

typedef struct memory_pool_node {
    heapstore_memory_pool_t pool;
    struct memory_pool_node* next;
} memory_pool_node_t;

typedef struct memory_allocation_node {
    heapstore_memory_allocation_t allocation;
    struct memory_allocation_node* next;
} memory_allocation_node_t;

typedef struct {
    memory_pool_node_t* pools;
    memory_allocation_node_t* allocations;
    size_t pool_count;
    size_t allocation_count;
    pthread_mutex_t lock;
    bool initialized;
} memory_data_t;

static memory_data_t g_memory_data = {0};

heapstore_error_t heapstore_memory_init(void) {
    if (g_memory_data.initialized) {
        return heapstore_ERR_ALREADY_INITIALIZED;
    }

    memset(&g_memory_data, 0, sizeof(g_memory_data));
    pthread_mutex_init(&g_memory_data.lock, NULL);
    g_memory_data.initialized = true;

    heapstore_ensure_directory("heapstore/kernel/memory/pools");
    heapstore_ensure_directory("heapstore/kernel/memory/stats");
    heapstore_ensure_directory("heapstore/kernel/memory/allocations");

    return heapstore_SUCCESS;
}

void heapstore_memory_shutdown(void) {
    if (!g_memory_data.initialized) {
        return;
    }

    pthread_mutex_lock(&g_memory_data.lock);

    memory_pool_node_t* pool = g_memory_data.pools;
    while (pool) {
        memory_pool_node_t* next = pool->next;
        free(pool);
        pool = next;
    }

    memory_allocation_node_t* alloc = g_memory_data.allocations;
    while (alloc) {
        memory_allocation_node_t* next = alloc->next;
        free(alloc);
        alloc = next;
    }

    g_memory_data.pools = NULL;
    g_memory_data.allocations = NULL;
    g_memory_data.pool_count = 0;
    g_memory_data.allocation_count = 0;

    pthread_mutex_unlock(&g_memory_data.lock);
    pthread_mutex_destroy(&g_memory_data.lock);

    g_memory_data.initialized = false;
}

heapstore_error_t heapstore_memory_record_pool(const heapstore_memory_pool_t* pool) {
    if (!pool || !pool->pool_id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!g_memory_data.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_memory_data.lock);

    memory_pool_node_t* node = (memory_pool_node_t*)malloc(sizeof(memory_pool_node_t));
    if (!node) {
        pthread_mutex_unlock(&g_memory_data.lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&node->pool, pool, sizeof(heapstore_memory_pool_t));
    node->next = g_memory_data.pools;
    g_memory_data.pools = node;
    g_memory_data.pool_count++;

    pthread_mutex_unlock(&g_memory_data.lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_memory_get_pool(const char* pool_id, heapstore_memory_pool_t* pool) {
    if (!pool_id || !pool) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!g_memory_data.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_memory_data.lock);

    memory_pool_node_t* node = g_memory_data.pools;
    while (node) {
        if (strcmp(node->pool.pool_id, pool_id) == 0) {
            memcpy(pool, &node->pool, sizeof(heapstore_memory_pool_t));
            pthread_mutex_unlock(&g_memory_data.lock);
            return heapstore_SUCCESS;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&g_memory_data.lock);

    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_memory_update_pool_usage(const char* pool_id, size_t used_size, uint32_t free_block_count) {
    if (!pool_id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!g_memory_data.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_memory_data.lock);

    memory_pool_node_t* node = g_memory_data.pools;
    while (node) {
        if (strcmp(node->pool.pool_id, pool_id) == 0) {
            node->pool.used_size = used_size;
            node->pool.free_block_count = free_block_count;
            pthread_mutex_unlock(&g_memory_data.lock);
            return heapstore_SUCCESS;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&g_memory_data.lock);

    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_memory_record_allocation(const heapstore_memory_allocation_t* allocation) {
    if (!allocation || !allocation->allocation_id[0]) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!g_memory_data.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_memory_data.lock);

    memory_allocation_node_t* node = (memory_allocation_node_t*)malloc(sizeof(memory_allocation_node_t));
    if (!node) {
        pthread_mutex_unlock(&g_memory_data.lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    memcpy(&node->allocation, allocation, sizeof(heapstore_memory_allocation_t));
    node->next = g_memory_data.allocations;
    g_memory_data.allocations = node;
    g_memory_data.allocation_count++;

    pthread_mutex_unlock(&g_memory_data.lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_memory_get_allocation(const char* allocation_id, heapstore_memory_allocation_t* allocation) {
    if (!allocation_id || !allocation) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!g_memory_data.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_memory_data.lock);

    memory_allocation_node_t* node = g_memory_data.allocations;
    while (node) {
        if (strcmp(node->allocation.allocation_id, allocation_id) == 0) {
            memcpy(allocation, &node->allocation, sizeof(heapstore_memory_allocation_t));
            pthread_mutex_unlock(&g_memory_data.lock);
            return heapstore_SUCCESS;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&g_memory_data.lock);

    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_memory_free_allocation(const char* allocation_id) {
    if (!allocation_id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!g_memory_data.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_memory_data.lock);

    memory_allocation_node_t* node = g_memory_data.allocations;
    while (node) {
        if (strcmp(node->allocation.allocation_id, allocation_id) == 0) {
            node->allocation.freed_at = (uint64_t)time(NULL);
            snprintf(node->allocation.status, sizeof(node->allocation.status), "freed");
            pthread_mutex_unlock(&g_memory_data.lock);
            return heapstore_SUCCESS;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&g_memory_data.lock);

    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_memory_get_stats(uint32_t* pool_count, uint32_t* total_allocations, uint64_t* total_size) {
    if (!g_memory_data.initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_memory_data.lock);

    if (pool_count) {
        *pool_count = (uint32_t)g_memory_data.pool_count;
    }

    if (total_allocations) {
        *total_allocations = (uint32_t)g_memory_data.allocation_count;
    }

    if (total_size) {
        *total_size = 0;
        memory_pool_node_t* node = g_memory_data.pools;
        while (node) {
            *total_size += node->pool.total_size;
            node = node->next;
        }
    }

    pthread_mutex_unlock(&g_memory_data.lock);

    return heapstore_SUCCESS;
}

