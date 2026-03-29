/**
 * @file cache.c
 * @brief 检索缓存（LRU实现�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/retrieval.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>

typedef struct cache_node {
    char* key;                          /**< 查询键（查询文本的SHA1或其他哈希） */
    char** result_ids;                  /**< 缓存的检索结果ID数组 */
    size_t result_count;                /**< 结果数量 */
    uint64_t timestamp;                  /**< 最近使用时间戳（纳秒） */
    struct cache_node* next;
    struct cache_node* prev;
} cache_node_t;

struct agentos_retrieval_cache {
    cache_node_t* head;                  /**< LRU链表头（最近使用） */
    cache_node_t* tail;                  /**< LRU链表尾（最久未用） */
    size_t max_size;                     /**< 最大条目数 */
    // From data intelligence emerges. by spharx
    size_t current_size;                  /**< 当前条目�?*/
    agentos_mutex_t* lock;
};

agentos_error_t agentos_retrieval_cache_create(
    size_t max_size,
    agentos_retrieval_cache_t** out_cache) {

    if (!out_cache) return AGENTOS_EINVAL;

    agentos_retrieval_cache_t* cache = (agentos_retrieval_cache_t*)AGENTOS_CALLOC(1, sizeof(agentos_retrieval_cache_t));
    if (!cache) {
        AGENTOS_LOG_ERROR("Failed to allocate cache");
        return AGENTOS_ENOMEM;
    }

    cache->max_size = max_size;
    cache->lock = agentos_mutex_create();
    if (!cache->lock) {
        AGENTOS_FREE(cache);
        return AGENTOS_ENOMEM;
    }

    *out_cache = cache;
    return AGENTOS_SUCCESS;
}

void agentos_retrieval_cache_destroy(agentos_retrieval_cache_t* cache) {
    if (!cache) return;

    agentos_mutex_lock(cache->lock);
    cache_node_t* node = cache->head;
    while (node) {
        cache_node_t* next = node->next;
        AGENTOS_FREE(node->key);
        if (node->result_ids) {
            for (size_t i = 0; i < node->result_count; i++) {
                AGENTOS_FREE(node->result_ids[i]);
            }
            AGENTOS_FREE(node->result_ids);
        }
        AGENTOS_FREE(node);
        node = next;
    }
    agentos_mutex_unlock(cache->lock);
    agentos_mutex_destroy(cache->lock);
    AGENTOS_FREE(cache);
}

/* 简单的字符串哈希（djb2�?*/
static unsigned long hash_key(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

/* 将节点移动到链表头部 */
static void move_to_head(cache_node_t* node, agentos_retrieval_cache_t* cache) {
    if (node == cache->head) return;

    // 从链表中移除
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    if (cache->tail == node) cache->tail = node->prev;

    // 插入头部
    node->next = cache->head;
    node->prev = NULL;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
}

agentos_error_t agentos_retrieval_cache_get(
    agentos_retrieval_cache_t* cache,
    const char* query,
    char*** out_result_ids,
    size_t* out_count) {

    if (!cache || !query || !out_result_ids || !out_count) return AGENTOS_EINVAL;

    char key_buf[32];
    snprintf(key_buf, sizeof(key_buf), "%lx", hash_key(query));

    agentos_mutex_lock(cache->lock);
    cache_node_t* node = cache->head;
    while (node) {
        if (strcmp(node->key, key_buf) == 0) {
            move_to_head(node, cache);
            node->timestamp = agentos_time_monotonic_ns();

            // 复制结果
            char** ids = (char**)AGENTOS_MALLOC(node->result_count * sizeof(char*));
            if (!ids) {
                agentos_mutex_unlock(cache->lock);
                return AGENTOS_ENOMEM;
            }
            for (size_t i = 0; i < node->result_count; i++) {
                ids[i] = AGENTOS_STRDUP(node->result_ids[i]);
                if (!ids[i]) {
                    for (size_t j = 0; j < i; j++) AGENTOS_FREE(ids[j]);
                    AGENTOS_FREE(ids);
                    agentos_mutex_unlock(cache->lock);
                    return AGENTOS_ENOMEM;
                }
            }
            *out_result_ids = ids;
            *out_count = node->result_count;
            agentos_mutex_unlock(cache->lock);
            return AGENTOS_SUCCESS;
        }
        node = node->next;
    }
    agentos_mutex_unlock(cache->lock);
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_retrieval_cache_put(
    agentos_retrieval_cache_t* cache,
    const char* query,
    const char** result_ids,
    size_t result_count) {

    if (!cache || !query || !result_ids || result_count == 0) return AGENTOS_EINVAL;

    char key_buf[32];
    snprintf(key_buf, sizeof(key_buf), "%lx", hash_key(query));

    agentos_mutex_lock(cache->lock);

    // 如果已存在，更新并移动到头部
    cache_node_t* node = cache->head;
    while (node) {
        if (strcmp(node->key, key_buf) == 0) {
            // 释放旧结�?
            for (size_t i = 0; i < node->result_count; i++) AGENTOS_FREE(node->result_ids[i]);
            AGENTOS_FREE(node->result_ids);
            // 复制新结�?
            node->result_ids = (char**)AGENTOS_MALLOC(result_count * sizeof(char*));
            if (!node->result_ids) {
                agentos_mutex_unlock(cache->lock);
                return AGENTOS_ENOMEM;
            }
            for (size_t i = 0; i < result_count; i++) {
                node->result_ids[i] = AGENTOS_STRDUP(result_ids[i]);
                if (!node->result_ids[i]) {
                    for (size_t j = 0; j < i; j++) AGENTOS_FREE(node->result_ids[j]);
                    AGENTOS_FREE(node->result_ids);
                    node->result_ids = NULL;
                    node->result_count = 0;
                    agentos_mutex_unlock(cache->lock);
                    return AGENTOS_ENOMEM;
                }
            }
            node->result_count = result_count;
            node->timestamp = agentos_time_monotonic_ns();
            move_to_head(node, cache);
            agentos_mutex_unlock(cache->lock);
            return AGENTOS_SUCCESS;
        }
        node = node->next;
    }

    // 创建新节�?
    node = (cache_node_t*)AGENTOS_CALLOC(1, sizeof(cache_node_t));
    if (!node) {
        agentos_mutex_unlock(cache->lock);
        return AGENTOS_ENOMEM;
    }
    node->key = AGENTOS_STRDUP(key_buf);
    if (!node->key) {
        AGENTOS_FREE(node);
        agentos_mutex_unlock(cache->lock);
        return AGENTOS_ENOMEM;
    }
    node->result_ids = (char**)AGENTOS_MALLOC(result_count * sizeof(char*));
    if (!node->result_ids) {
        AGENTOS_FREE(node->key);
        AGENTOS_FREE(node);
        agentos_mutex_unlock(cache->lock);
        return AGENTOS_ENOMEM;
    }
    for (size_t i = 0; i < result_count; i++) {
        node->result_ids[i] = AGENTOS_STRDUP(result_ids[i]);
        if (!node->result_ids[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(node->result_ids[j]);
            AGENTOS_FREE(node->result_ids);
            AGENTOS_FREE(node->key);
            AGENTOS_FREE(node);
            agentos_mutex_unlock(cache->lock);
            return AGENTOS_ENOMEM;
        }
    }
    node->result_count = result_count;
    node->timestamp = agentos_time_monotonic_ns();

    // 插入头部
    node->next = cache->head;
    node->prev = NULL;
    if (cache->head) cache->head->prev = node;
    cache->head = node;
    if (!cache->tail) cache->tail = node;
    cache->current_size++;

    // 如果超过最大容量，移除尾部
    if (cache->current_size > cache->max_size) {
        cache_node_t* tail = cache->tail;
        if (tail) {
            cache->tail = tail->prev;
            if (cache->tail) cache->tail->next = NULL;
            AGENTOS_FREE(tail->key);
            for (size_t i = 0; i < tail->result_count; i++) AGENTOS_FREE(tail->result_ids[i]);
            AGENTOS_FREE(tail->result_ids);
            AGENTOS_FREE(tail);
            cache->current_size--;
        }
    }

    agentos_mutex_unlock(cache->lock);
    return AGENTOS_SUCCESS;
}