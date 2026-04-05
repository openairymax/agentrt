/**
 * @file lru_cache.c
 * @brief LRU 缓存管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "lru_cache.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static uint64_t hash_string(const char* str) {
    if (!str) return 0;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static void evict_lru_entry(lru_cache_t* cache) {
    if (!cache || !cache->tail) return;

    cache_entry_t* to_evict = cache->tail;

    if (to_evict->prev) {
        to_evict->prev->next = NULL;
    } else {
        cache->head = NULL;
    }
    cache->tail = to_evict->prev;

    uint64_t hash = hash_string(to_evict->key);
    size_t index = hash % cache->hash_table_size;
    cache_entry_t** pp = &cache->hash_table[index];
    while (*pp) {
        if (*pp == to_evict) {
            *pp = to_evict->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }

    cache->current_size -= to_evict->value_size;
    cache->evict_count++;

    if (to_evict->key) AGENTOS_FREE(to_evict->key);
    if (to_evict->value) AGENTOS_FREE(to_evict->value);
    AGENTOS_FREE(to_evict);
}

lru_cache_t* lru_cache_create(const char* name, uint64_t max_size, size_t hash_size) {
    lru_cache_t* cache = (lru_cache_t*)AGENTOS_CALLOC(1, sizeof(lru_cache_t));
    if (!cache) return NULL;

    cache->cache_name = name ? AGENTOS_STRDUP(name) : NULL;
    cache->max_size = max_size;
    cache->current_size = 0;
    cache->entry_count = 0;
    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->evict_count = 0;
    cache->hash_table_size = hash_size > 0 ? hash_size : 1024;
    cache->default_ttl_seconds = DEFAULT_CACHE_TTL_SECONDS;

    cache->hash_table = (cache_entry_t**)AGENTOS_CALLOC(cache->hash_table_size, sizeof(cache_entry_t*));
    if (!cache->hash_table) {
        if (cache->cache_name) AGENTOS_FREE(cache->cache_name);
        AGENTOS_FREE(cache);
        return NULL;
    }

    cache->head = NULL;
    cache->tail = NULL;

    cache->lock = agentos_mutex_create();
    if (!cache->lock) {
        AGENTOS_FREE(cache->hash_table);
        if (cache->cache_name) AGENTOS_FREE(cache->cache_name);
        AGENTOS_FREE(cache);
        return NULL;
    }

    return cache;
}

void lru_cache_destroy(lru_cache_t* cache) {
    if (!cache) return;

    agentos_mutex_lock(cache->lock);

    cache_entry_t* entry = cache->head;
    while (entry) {
        cache_entry_t* next = entry->next;
        if (entry->key) AGENTOS_FREE(entry->key);
        if (entry->value) AGENTOS_FREE(entry->value);
        AGENTOS_FREE(entry);
        entry = next;
    }

    AGENTOS_FREE(cache->hash_table);
    agentos_mutex_unlock(cache->lock);
    agentos_mutex_destroy(cache->lock);

    if (cache->cache_name) AGENTOS_FREE(cache->cache_name);
    AGENTOS_FREE(cache);
}

void* lru_cache_get(lru_cache_t* cache, const char* key, size_t* out_size) {
    if (!cache || !key) return NULL;

    agentos_mutex_lock(cache->lock);

    uint64_t hash = hash_string(key);
    size_t index = hash % cache->hash_table_size;
    cache_entry_t* entry = cache->hash_table[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (entry->expire_time_ns > 0 && get_timestamp_ns() > entry->expire_time_ns) {
                break;
            }

            entry->access_count++;
            entry->last_access_ns = get_timestamp_ns();
            cache->hit_count++;

            if (entry != cache->head) {
                if (entry->prev) entry->prev->next = entry->next;
                if (entry->next) entry->next->prev = entry->prev;
                if (entry == cache->tail) cache->tail = entry->prev;

                entry->prev = NULL;
                entry->next = cache->head;
                if (cache->head) cache->head->prev = entry;
                cache->head = entry;
            }

            if (out_size) *out_size = entry->value_size;
            void* result = entry->value;
            agentos_mutex_unlock(cache->lock);
            return result;
        }
        entry = entry->hash_next;
    }

    cache->miss_count++;
    agentos_mutex_unlock(cache->lock);
    return NULL;
}

int lru_cache_put(lru_cache_t* cache, const char* key, const void* value, size_t size, uint32_t ttl_seconds) {
    if (!cache || !key || !value) return -1;

    agentos_mutex_lock(cache->lock);

    while (cache->current_size + size > cache->max_size && cache->tail) {
        evict_lru_entry(cache);
    }

    uint64_t hash = hash_string(key);
    size_t index = hash % cache->hash_table_size;

    cache_entry_t* entry = cache->hash_table[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (entry->value) AGENTOS_FREE(entry->value);
            entry->value = AGENTOS_MALLOC(size);
            if (!entry->value) {
                agentos_mutex_unlock(cache->lock);
                return -1;
            }
            memcpy(entry->value, value, size);
            entry->value_size = size;
            entry->access_count++;
            entry->last_access_ns = get_timestamp_ns();

            if (ttl_seconds > 0) {
                entry->expire_time_ns = get_timestamp_ns() + (uint64_t)ttl_seconds * 1000000000ULL;
            } else {
                entry->expire_time_ns = 0;
            }

            agentos_mutex_unlock(cache->lock);
            return 0;
        }
        entry = entry->hash_next;
    }

    cache_entry_t* new_entry = (cache_entry_t*)AGENTOS_CALLOC(1, sizeof(cache_entry_t));
    if (!new_entry) {
        agentos_mutex_unlock(cache->lock);
        return -1;
    }

    new_entry->key = AGENTOS_STRDUP(key);
    new_entry->value = AGENTOS_MALLOC(size);
    if (!new_entry->value) {
        AGENTOS_FREE(new_entry->key);
        AGENTOS_FREE(new_entry);
        agentos_mutex_unlock(cache->lock);
        return -1;
    }

    memcpy(new_entry->value, value, size);
    new_entry->value_size = size;
    new_entry->create_time_ns = get_timestamp_ns();
    new_entry->last_access_ns = new_entry->create_time_ns;
    new_entry->access_count = 1;

    if (ttl_seconds > 0) {
        new_entry->expire_time_ns = new_entry->create_time_ns + (uint64_t)ttl_seconds * 1000000000ULL;
    } else {
        new_entry->expire_time_ns = 0;
    }

    new_entry->hash_next = cache->hash_table[index];
    cache->hash_table[index] = new_entry;

    new_entry->next = cache->head;
    new_entry->prev = NULL;
    if (cache->head) {
        cache->head->prev = new_entry;
    }
    cache->head = new_entry;

    if (!cache->tail) {
        cache->tail = new_entry;
    }

    cache->current_size += size;
    cache->entry_count++;

    agentos_mutex_unlock(cache->lock);
    return 0;
}

int lru_cache_remove(lru_cache_t* cache, const char* key) {
    if (!cache || !key) return -1;

    agentos_mutex_lock(cache->lock);

    uint64_t hash = hash_string(key);
    size_t index = hash % cache->hash_table_size;

    cache_entry_t** pp = &cache->hash_table[index];
    while (*pp) {
        if (strcmp((*pp)->key, key) == 0) {
            cache_entry_t* entry = *pp;
            *pp = entry->hash_next;

            if (entry->prev) {
                entry->prev->next = entry->next;
            } else {
                cache->head = entry->next;
            }
            if (entry->next) {
                entry->next->prev = entry->prev;
            } else {
                cache->tail = entry->prev;
            }

            cache->current_size -= entry->value_size;
            cache->entry_count--;

            if (entry->key) AGENTOS_FREE(entry->key);
            if (entry->value) AGENTOS_FREE(entry->value);
            AGENTOS_FREE(entry);

            agentos_mutex_unlock(cache->lock);
            return 0;
        }
        pp = &(*pp)->hash_next;
    }

    agentos_mutex_unlock(cache->lock);
    return -1;
}

void lru_cache_clear(lru_cache_t* cache) {
    if (!cache) return;

    agentos_mutex_lock(cache->lock);

    cache_entry_t* entry = cache->head;
    while (entry) {
        cache_entry_t* next = entry->next;
        if (entry->key) AGENTOS_FREE(entry->key);
        if (entry->value) AGENTOS_FREE(entry->value);
        AGENTOS_FREE(entry);
        entry = next;
    }

    memset(cache->hash_table, 0, cache->hash_table_size * sizeof(cache_entry_t*));
    cache->head = NULL;
    cache->tail = NULL;
    cache->current_size = 0;
    cache->entry_count = 0;

    agentos_mutex_unlock(cache->lock);
}

uint64_t lru_cache_get_hit_count(lru_cache_t* cache) {
    if (!cache) return 0;
    return cache->hit_count;
}

uint64_t lru_cache_get_miss_count(lru_cache_t* cache) {
    if (!cache) return 0;
    return cache->miss_count;
}

double lru_cache_get_hit_rate(lru_cache_t* cache) {
    if (!cache) return 0.0;
    uint64_t total = cache->hit_count + cache->miss_count;
    if (total == 0) return 0.0;
    return (double)cache->hit_count / total;
}

uint64_t lru_cache_get_evict_count(lru_cache_t* cache) {
    if (!cache) return 0;
    return cache->evict_count;
}
