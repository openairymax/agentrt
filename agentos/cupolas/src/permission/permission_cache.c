/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * permission_cache.c - Permission Cache Implementation: Hash-based LRU Cache
 */

/**
 * @file permission_cache.c
 * @brief Permission Cache Implementation - High-performance LRU cache based on hash table
 * @author Spharx
 * @date 2024
 */

#include "permission_cache.h"
#include <stdlib.h>
#include <string.h>

#define DEFAULT_BUCKET_COUNT 64
#define MAX_BUCKET_COUNT 4096
#define LOAD_FACTOR_THRESHOLD 0.75

static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static char* build_cache_key(const char* agent_id, const char* action,
                              const char* resource, const char* context) {
    size_t agent_len = agent_id ? strlen(agent_id) : 0;
    size_t action_len = action ? strlen(action) : 0;
    size_t resource_len = resource ? strlen(resource) : 0;
    size_t context_len = context ? strlen(context) : 0;
    
    size_t total_len = agent_len + 1 + action_len + 1 + resource_len + 1 + context_len + 1;
    char* key = (char*)cupolas_mem_alloc(total_len);
    if (!key) return NULL;
    
    char* p = key;
    if (agent_id) {
        memcpy(p, agent_id, agent_len);
        p += agent_len;
    }
    *p++ = ':';
    if (action) {
        memcpy(p, action, action_len);
        p += action_len;
    }
    *p++ = ':';
    if (resource) {
        memcpy(p, resource, resource_len);
        p += resource_len;
    }
    *p++ = ':';
    if (context) {
        memcpy(p, context, context_len);
        p += context_len;
    }
    *p = '\0';
    
    return key;
}

static size_t next_power_of_two(size_t n) {
    size_t power = 1;
    while (power < n) {
        power *= 2;
    }
    return power;
}

cache_manager_t* cache_manager_create(size_t capacity, uint32_t ttl_ms) {
    if (capacity == 0) {
        capacity = 1024;
    }
    
    cache_manager_t* cm = (cache_manager_t*)cupolas_mem_alloc(sizeof(cache_manager_t));
    if (!cm) return NULL;
    
    memset(cm, 0, sizeof(cache_manager_t));
    
    size_t bucket_count = next_power_of_two(capacity / 4);
    if (bucket_count < DEFAULT_BUCKET_COUNT) {
        bucket_count = DEFAULT_BUCKET_COUNT;
    }
    if (bucket_count > MAX_BUCKET_COUNT) {
        bucket_count = MAX_BUCKET_COUNT;
    }
    
    cm->buckets = (cache_entry_t**)cupolas_mem_alloc(bucket_count * sizeof(cache_entry_t*));
    if (!cm->buckets) {
        cupolas_mem_free(cm);
        return NULL;
    }
    memset(cm->buckets, 0, bucket_count * sizeof(cache_entry_t*));
    
    cm->bucket_count = bucket_count;
    cm->capacity = capacity;
    cm->ttl_ms = ttl_ms;
    
    if (cupolas_mutex_init(&cm->lock) != cupolas_OK) {
        cupolas_mem_free(cm->buckets);
        cupolas_mem_free(cm);
        return NULL;
    }
    
    return cm;
}

void cache_manager_destroy(cache_manager_t* cm) {
    if (!cm) return;
    
    cupolas_mutex_lock(&cm->lock);
    
    cache_entry_t* entry = cm->head;
    while (entry) {
        cache_entry_t* next = entry->next;
        cupolas_mem_free(entry->key);
        cupolas_mem_free(entry);
        entry = next;
    }
    
    cupolas_mem_free(cm->buckets);
    
    cupolas_mutex_unlock(&cm->lock);
    cupolas_mutex_destroy(&cm->lock);
    cupolas_mem_free(cm);
}

static void move_to_head(cache_manager_t* cm, cache_entry_t* entry) {
    if (entry == cm->head) return;
    
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry == cm->tail) {
        cm->tail = entry->prev;
    }
    
    entry->prev = NULL;
    entry->next = cm->head;
    if (cm->head) {
        cm->head->prev = entry;
    }
    cm->head = entry;
    
    if (!cm->tail) {
        cm->tail = entry;
    }
}

static void remove_entry(cache_manager_t* cm, cache_entry_t* entry) {
    size_t bucket_idx = entry->hash & (cm->bucket_count - 1);
    cache_entry_t** pp = &cm->buckets[bucket_idx];
    
    while (*pp) {
        if (*pp == entry) {
            *pp = entry->hnext;
            break;
        }
        pp = &(*pp)->hnext;
    }
    
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        cm->head = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        cm->tail = entry->prev;
    }
    
    cupolas_mem_free(entry->key);
    cupolas_mem_free(entry);
    cm->size--;
}

static cache_entry_t* find_entry(cache_manager_t* cm, uint32_t hash, const char* key) {
    size_t bucket_idx = hash & (cm->bucket_count - 1);
    cache_entry_t* entry = cm->buckets[bucket_idx];
    
    while (entry) {
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            return entry;
        }
        entry = entry->hnext;
    }
    
    return NULL;
}

int cache_manager_get(cache_manager_t* cm,
                      const char* agent_id,
                      const char* action,
                      const char* resource,
                      const char* context) {
    if (!cm) return -1;
    
    char* key = build_cache_key(agent_id, action, resource, context);
    if (!key) return -1;
    
    uint32_t hash = hash_string(key);
    
    cupolas_mutex_lock(&cm->lock);
    
    cache_entry_t* entry = find_entry(cm, hash, key);
    
    if (entry) {
        if (cm->ttl_ms > 0) {
            uint64_t now = cupolas_time_ms();
            if (now - entry->timestamp_ms > cm->ttl_ms) {
                remove_entry(cm, entry);
                cupolas_atomic_add64(&cm->miss_count, 1);
                cupolas_mutex_unlock(&cm->lock);
                cupolas_mem_free(key);
                return -1;
            }
        }
        
        move_to_head(cm, entry);
        int result = entry->result;
        cupolas_atomic_add64(&cm->hit_count, 1);
        cupolas_mutex_unlock(&cm->lock);
        cupolas_mem_free(key);
        return result;
    }
    
    cupolas_atomic_add64(&cm->miss_count, 1);
    cupolas_mutex_unlock(&cm->lock);
    cupolas_mem_free(key);
    return -1;
}

void cache_manager_put(cache_manager_t* cm,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context,
                       int result) {
    if (!cm) return;
    
    char* key = build_cache_key(agent_id, action, resource, context);
    if (!key) return;
    
    uint32_t hash = hash_string(key);
    
    cupolas_mutex_lock(&cm->lock);
    
    cache_entry_t* entry = find_entry(cm, hash, key);
    
    if (entry) {
        entry->result = result;
        entry->timestamp_ms = cupolas_time_ms();
        move_to_head(cm, entry);
        cupolas_mutex_unlock(&cm->lock);
        cupolas_mem_free(key);
        return;
    }
    
    while (cm->size >= cm->capacity && cm->tail) {
        remove_entry(cm, cm->tail);
    }
    
    entry = (cache_entry_t*)cupolas_mem_alloc(sizeof(cache_entry_t));
    if (!entry) {
        cupolas_mutex_unlock(&cm->lock);
        cupolas_mem_free(key);
        return;
    }
    
    entry->key = key;
    entry->result = result;
    entry->timestamp_ms = cupolas_time_ms();
    entry->hash = hash;
    entry->prev = NULL;
    entry->next = cm->head;
    entry->hnext = NULL;
    
    if (cm->head) {
        cm->head->prev = entry;
    }
    cm->head = entry;
    if (!cm->tail) {
        cm->tail = entry;
    }
    
    size_t bucket_idx = hash & (cm->bucket_count - 1);
    entry->hnext = cm->buckets[bucket_idx];
    cm->buckets[bucket_idx] = entry;
    
    cm->size++;
    
    cupolas_mutex_unlock(&cm->lock);
}

void cache_manager_clear(cache_manager_t* cm) {
    if (!cm) return;
    
    cupolas_mutex_lock(&cm->lock);
    
    cache_entry_t* entry = cm->head;
    while (entry) {
        cache_entry_t* next = entry->next;
        cupolas_mem_free(entry->key);
        cupolas_mem_free(entry);
        entry = next;
    }
    
    memset(cm->buckets, 0, cm->bucket_count * sizeof(cache_entry_t*));
    cm->head = NULL;
    cm->tail = NULL;
    cm->size = 0;
    
    cupolas_mutex_unlock(&cm->lock);
}

void cache_manager_stats(cache_manager_t* cm, uint64_t* hit_count, uint64_t* miss_count) {
    if (!cm) {
        if (hit_count) *hit_count = 0;
        if (miss_count) *miss_count = 0;
        return;
    }
    
    if (hit_count) *hit_count = cupolas_atomic_load64(&cm->hit_count);
    if (miss_count) *miss_count = cupolas_atomic_load64(&cm->miss_count);
}
