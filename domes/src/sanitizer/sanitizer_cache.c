/**
 * @file sanitizer_cache.c
 * @brief 净化器缓存实现
 * @author Spharx
 * @date 2024
 */

#include "sanitizer_cache.h"
#include <stdlib.h>
#include <string.h>

#define CACHE_TTL_MS 60000

typedef struct cache_entry {
    char* key;
    char* value;
    uint64_t timestamp_ms;
    struct cache_entry* next;
} cache_entry_t;

struct sanitizer_cache {
    cache_entry_t** buckets;
    size_t bucket_count;
    size_t size;
    size_t capacity;
    domes_mutex_t lock;
};

static uint32_t hash_key(const char* key) {
    uint32_t hash = 5381;
    while (*key) {
        hash = ((hash << 5) + hash) + (unsigned char)*key++;
    }
    return hash;
}

static char* build_key(const char* input, sanitize_level_t level) {
    size_t len = strlen(input) + 16;
    char* key = (char*)domes_mem_alloc(len);
    if (!key) return NULL;
    snprintf(key, len, "%d:%s", (int)level, input);
    return key;
}

sanitizer_cache_t* sanitizer_cache_create(size_t capacity) {
    sanitizer_cache_t* cache = (sanitizer_cache_t*)domes_mem_alloc(sizeof(sanitizer_cache_t));
    if (!cache) return NULL;
    
    memset(cache, 0, sizeof(sanitizer_cache_t));
    cache->capacity = capacity;
    cache->bucket_count = capacity > 0 ? capacity / 4 : 64;
    if (cache->bucket_count < 16) cache->bucket_count = 16;
    
    cache->buckets = (cache_entry_t**)domes_mem_alloc(cache->bucket_count * sizeof(cache_entry_t*));
    if (!cache->buckets) {
        domes_mem_free(cache);
        return NULL;
    }
    memset(cache->buckets, 0, cache->bucket_count * sizeof(cache_entry_t*));
    
    if (domes_mutex_init(&cache->lock) != DOMES_OK) {
        domes_mem_free(cache->buckets);
        domes_mem_free(cache);
        return NULL;
    }
    
    return cache;
}

void sanitizer_cache_destroy(sanitizer_cache_t* cache) {
    if (!cache) return;
    
    domes_mutex_lock(&cache->lock);
    
    for (size_t i = 0; i < cache->bucket_count; i++) {
        cache_entry_t* entry = cache->buckets[i];
        while (entry) {
            cache_entry_t* next = entry->next;
            domes_mem_free(entry->key);
            domes_mem_free(entry->value);
            domes_mem_free(entry);
            entry = next;
        }
    }
    
    domes_mem_free(cache->buckets);
    
    domes_mutex_unlock(&cache->lock);
    domes_mutex_destroy(&cache->lock);
    domes_mem_free(cache);
}

char* sanitizer_cache_get(sanitizer_cache_t* cache, const char* input, sanitize_level_t level) {
    if (!cache || !input) return NULL;
    
    char* key = build_key(input, level);
    if (!key) return NULL;
    
    domes_mutex_lock(&cache->lock);
    
    uint32_t hash = hash_key(key);
    size_t idx = hash % cache->bucket_count;
    
    cache_entry_t* entry = cache->buckets[idx];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            uint64_t now = domes_time_ms();
            if (now - entry->timestamp_ms > CACHE_TTL_MS) {
                domes_mutex_unlock(&cache->lock);
                domes_mem_free(key);
                return NULL;
            }
            char* result = domes_strdup(entry->value);
            domes_mutex_unlock(&cache->lock);
            domes_mem_free(key);
            return result;
        }
        entry = entry->next;
    }
    
    domes_mutex_unlock(&cache->lock);
    domes_mem_free(key);
    return NULL;
}

void sanitizer_cache_put(sanitizer_cache_t* cache, const char* input, const char* output, sanitize_level_t level) {
    if (!cache || !input || !output) return;
    
    char* key = build_key(input, level);
    if (!key) return;
    
    domes_mutex_lock(&cache->lock);
    
    if (cache->size >= cache->capacity) {
        domes_mutex_unlock(&cache->lock);
        domes_mem_free(key);
        return;
    }
    
    uint32_t hash = hash_key(key);
    size_t idx = hash % cache->bucket_count;
    
    cache_entry_t* entry = (cache_entry_t*)domes_mem_alloc(sizeof(cache_entry_t));
    if (!entry) {
        domes_mutex_unlock(&cache->lock);
        domes_mem_free(key);
        return;
    }
    
    entry->key = key;
    entry->value = domes_strdup(output);
    entry->timestamp_ms = domes_time_ms();
    entry->next = cache->buckets[idx];
    cache->buckets[idx] = entry;
    cache->size++;
    
    domes_mutex_unlock(&cache->lock);
}

void sanitizer_cache_clear(sanitizer_cache_t* cache) {
    if (!cache) return;
    
    domes_mutex_lock(&cache->lock);
    
    for (size_t i = 0; i < cache->bucket_count; i++) {
        cache_entry_t* entry = cache->buckets[i];
        while (entry) {
            cache_entry_t* next = entry->next;
            domes_mem_free(entry->key);
            domes_mem_free(entry->value);
            domes_mem_free(entry);
            entry = next;
        }
        cache->buckets[i] = NULL;
    }
    cache->size = 0;
    
    domes_mutex_unlock(&cache->lock);
}
