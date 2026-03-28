/**
 * @file svc_cache.c
 * @brief 通用 LRU 缓存实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 提供 llm_d 和 tool_d 共用的缓存实现
 * 2. 代码复用率：从 2 处复制变为 1 处实现
 * 3. 便于后续统一优化和维护
 */

#include "svc_cache.h"
#include "platform.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------- 链表节点 ---------- */

typedef struct cache_node {
    char* key;
    void* value;
    size_t value_size;
    time_t expire_at;
    struct cache_node* prev;
    struct cache_node* next;
} cache_node_t;

/* ---------- 缓存结构 ---------- */

struct svc_cache {
    cache_node_t** slots;
    size_t capacity;
    size_t size;
    int ttl_sec;
    void (*value_free_fn)(void*);
    agentos_mutex_t lock;
};

/* ---------- 辅助函数 ---------- */

static unsigned int hash_key(const char* key, size_t capacity) {
    unsigned int hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % capacity;
}

static void remove_node(cache_node_t* node) {
    if (!node) return;
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
}

static void add_to_front(cache_node_t** head, cache_node_t* node) {
    if (!head || !node) return;
    node->next = *head;
    node->prev = NULL;
    if (*head) (*head)->prev = node;
    *head = node;
}

/* ---------- 公共接口实现 ---------- */

svc_cache_t* svc_cache_create(const svc_cache_config_t* manager) {
    if (!manager || manager->capacity == 0) {
        return NULL;
    }

    svc_cache_t* cache = (svc_cache_t*)calloc(1, sizeof(svc_cache_t));
    if (!cache) {
        return NULL;
    }

    cache->slots = (cache_node_t**)calloc(manager->capacity, sizeof(cache_node_t*));
    if (!cache->slots) {
        free(cache);
        return NULL;
    }

    if (agentos_mutex_init(&cache->lock) != 0) {
        free(cache->slots);
        free(cache);
        return NULL;
    }

    cache->capacity = manager->capacity;
    cache->size = 0;
    cache->ttl_sec = manager->ttl_sec > 0 ? manager->ttl_sec : 3600;
    cache->value_free_fn = manager->value_free_fn;

    return cache;
}

void svc_cache_destroy(svc_cache_t* cache) {
    if (!cache) return;

    agentos_mutex_lock(&cache->lock);

    for (size_t i = 0; i < cache->capacity; ++i) {
        cache_node_t* node = cache->slots[i];
        while (node) {
            cache_node_t* next = node->next;
            free(node->key);
            if (node->value && cache->value_free_fn) {
                cache->value_free_fn(node->value);
            } else {
                free(node->value);
            }
            free(node);
            node = next;
        }
    }

    free(cache->slots);
    agentos_mutex_unlock(&cache->lock);
    agentos_mutex_destroy(&cache->lock);
    free(cache);
}

int svc_cache_get(svc_cache_t* cache, const char* key, void** out_value) {
    if (!cache || !key || !out_value) {
        return -1;
    }

    *out_value = NULL;

    agentos_mutex_lock(&cache->lock);

    unsigned int idx = hash_key(key, cache->capacity);
    cache_node_t* node = cache->slots[idx];

    while (node) {
        if (strcmp(node->key, key) == 0) {
            if (cache->ttl_sec > 0 && node->expire_at > 0) {
                if (time(NULL) > node->expire_at) {
                    remove_node(node);
                    free(node->key);
                    if (node->value && cache->value_free_fn) {
                        cache->value_free_fn(node->value);
                    } else {
                        free(node->value);
                    }
                    free(node);
                    cache->size--;
                    agentos_mutex_unlock(&cache->lock);
                    return 0;
                }
            }

            cache_node_t** head = &cache->slots[idx];
            while (*head && *head != node) head = &(*head)->next;
            if (*head == node) {
                remove_node(node);
                add_to_front(&cache->slots[idx], node);
            }

            *out_value = node->value;
            agentos_mutex_unlock(&cache->lock);
            return 1;
        }
        node = node->next;
    }

    agentos_mutex_unlock(&cache->lock);
    return 0;
}

int svc_cache_put(svc_cache_t* cache, const char* key, const void* value, size_t value_size) {
    if (!cache || !key || !value) {
        return -1;
    }

    agentos_mutex_lock(&cache->lock);

    unsigned int idx = hash_key(key, cache->capacity);
    cache_node_t** head = &cache->slots[idx];
    cache_node_t* node = *head;

    while (node) {
        if (strcmp(node->key, key) == 0) {
            if (node->value && cache->value_free_fn) {
                cache->value_free_fn(node->value);
            } else {
                free(node->value);
            }
            node->value = malloc(value_size);
            if (!node->value) {
                agentos_mutex_unlock(&cache->lock);
                return -1;
            }
            memcpy(node->value, value, value_size);
            node->value_size = value_size;
            if (cache->ttl_sec > 0) {
                node->expire_at = time(NULL) + cache->ttl_sec;
            }
            remove_node(node);
            add_to_front(head, node);
            agentos_mutex_unlock(&cache->lock);
            return 0;
        }
        node = node->next;
    }

    node = (cache_node_t*)calloc(1, sizeof(cache_node_t));
    if (!node) {
        agentos_mutex_unlock(&cache->lock);
        return -1;
    }

    node->key = strdup(key);
    if (!node->key) {
        free(node);
        agentos_mutex_unlock(&cache->lock);
        return -1;
    }

    node->value = malloc(value_size);
    if (!node->value) {
        free(node->key);
        free(node);
        agentos_mutex_unlock(&cache->lock);
        return -1;
    }
    memcpy(node->value, value, value_size);
    node->value_size = value_size;

    if (cache->ttl_sec > 0) {
        node->expire_at = time(NULL) + cache->ttl_sec;
    }

    add_to_front(head, node);
    cache->size++;

    if (cache->size > cache->capacity) {
        cache_node_t** tail = head;
        while (*tail && (*tail)->next) {
            tail = &(*tail)->next;
        }
        if (*tail) {
            cache_node_t* victim = *tail;
            remove_node(victim);
            free(victim->key);
            if (victim->value && cache->value_free_fn) {
                cache->value_free_fn(victim->value);
            } else {
                free(victim->value);
            }
            free(victim);
            cache->size--;
        }
    }

    agentos_mutex_unlock(&cache->lock);
    return 0;
}

void svc_cache_clear(svc_cache_t* cache) {
    if (!cache) return;

    agentos_mutex_lock(&cache->lock);

    for (size_t i = 0; i < cache->capacity; ++i) {
        cache_node_t* node = cache->slots[i];
        while (node) {
            cache_node_t* next = node->next;
            free(node->key);
            if (node->value && cache->value_free_fn) {
                cache->value_free_fn(node->value);
            } else {
                free(node->value);
            }
            free(node);
            node = next;
        }
        cache->slots[i] = NULL;
    }
    cache->size = 0;

    agentos_mutex_unlock(&cache->lock);
}

size_t svc_cache_size(svc_cache_t* cache) {
    if (!cache) return 0;
    agentos_mutex_lock(&cache->lock);
    size_t size = cache->size;
    agentos_mutex_unlock(&cache->lock);
    return size;
}

bool svc_cache_is_empty(svc_cache_t* cache) {
    return svc_cache_size(cache) == 0;
}
