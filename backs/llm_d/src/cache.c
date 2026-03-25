/**
 * @file cache.c
 * @brief LRU 缓存实现（双链表 + 哈希表）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define HASH_SIZE 1024

typedef struct cache_entry {
    char* key;
    char* value;
    time_t timestamp;
    struct cache_entry* prev;
    struct cache_entry* next;
    struct cache_entry* hnext;
} cache_entry_t;

typedef struct cache_bucket {
    cache_entry_t* head;
    // From data intelligence emerges. by spharx
    pthread_mutex_t lock;
} cache_bucket_t;

struct cache {
    cache_bucket_t buckets[HASH_SIZE];
    cache_entry_t* lru_head;
    cache_entry_t* lru_tail;
    size_t capacity;
    size_t size;
    int ttl_sec;
    pthread_mutex_t lru_lock;
};

static unsigned int hash_key(const char* key) {
    unsigned int h = 5381;
    while (*key) h = (h << 5) + h + *key++;
    return h % HASH_SIZE;
}

static cache_entry_t* entry_create(const char* key, const char* value) {
    cache_entry_t* e = malloc(sizeof(cache_entry_t));
    if (!e) return NULL;
    e->key = strdup(key);
    e->value = strdup(value);
    e->timestamp = time(NULL);
    e->prev = e->next = e->hnext = NULL;
    return e;
}

static void entry_free(cache_entry_t* e) {
    if (!e) return;
    free(e->key);
    free(e->value);
    free(e);
}

static void lru_remove(cache_t* cache, cache_entry_t* e) {
    if (e->prev) e->prev->next = e->next;
    if (e->next) e->next->prev = e->prev;
    if (cache->lru_head == e) cache->lru_head = e->next;
    if (cache->lru_tail == e) cache->lru_tail = e->prev;
    e->prev = e->next = NULL;
}

static void lru_move_to_head(cache_t* cache, cache_entry_t* e) {
    if (cache->lru_head == e) return;
    lru_remove(cache, e);
    e->next = cache->lru_head;
    if (cache->lru_head) cache->lru_head->prev = e;
    cache->lru_head = e;
    if (!cache->lru_tail) cache->lru_tail = e;
}

static void evict_lru(cache_t* cache) {
    if (!cache->lru_tail) return;
    cache_entry_t* victim = cache->lru_tail;
    unsigned int idx = hash_key(victim->key);

    pthread_mutex_lock(&cache->buckets[idx].lock);
    cache_entry_t** p = &cache->buckets[idx].head;
    while (*p) {
        if (*p == victim) {
            *p = victim->hnext;
            break;
        }
        p = &(*p)->hnext;
    }
    pthread_mutex_unlock(&cache->buckets[idx].lock);

    lru_remove(cache, victim);
    entry_free(victim);
    cache->size--;
}

cache_t* cache_create(size_t capacity, int ttl_sec) {
    cache_t* cache = calloc(1, sizeof(cache_t));
    if (!cache) return NULL;
    cache->capacity = capacity;
    cache->ttl_sec = ttl_sec;
    pthread_mutex_init(&cache->lru_lock, NULL);
    for (int i = 0; i < HASH_SIZE; ++i)
        pthread_mutex_init(&cache->buckets[i].lock, NULL);
    return cache;
}

void cache_destroy(cache_t* cache) {
    if (!cache) return;
    for (int i = 0; i < HASH_SIZE; ++i) {
        pthread_mutex_lock(&cache->buckets[i].lock);
        cache_entry_t* e = cache->buckets[i].head;
        while (e) {
            cache_entry_t* next = e->hnext;
            entry_free(e);
            e = next;
        }
        pthread_mutex_unlock(&cache->buckets[i].lock);
        pthread_mutex_destroy(&cache->buckets[i].lock);
    }
    pthread_mutex_destroy(&cache->lru_lock);
    free(cache);
}

int cache_get(cache_t* cache, const char* key, char** out_value) {
    if (!cache || !key || !out_value) return -1;
    *out_value = NULL;

    unsigned int idx = hash_key(key);
    pthread_mutex_lock(&cache->buckets[idx].lock);
    cache_entry_t* e = cache->buckets[idx].head;
    while (e) {
        if (strcmp(e->key, key) == 0) break;
        e = e->hnext;
    }
    pthread_mutex_unlock(&cache->buckets[idx].lock);

    if (!e) return 0;

    if (cache->ttl_sec > 0 && (time(NULL) - e->timestamp) > cache->ttl_sec) {
        cache_put(cache, key, NULL);
        return 0;
    }

    pthread_mutex_lock(&cache->lru_lock);
    lru_move_to_head(cache, e);
    pthread_mutex_unlock(&cache->lru_lock);

    *out_value = strdup(e->value);
    return 1;
}

void cache_put(cache_t* cache, const char* key, const char* value) {
    if (!cache || !key) return;
    if (cache->capacity == 0) return;

    unsigned int idx = hash_key(key);
    pthread_mutex_lock(&cache->buckets[idx].lock);

    cache_entry_t** p = &cache->buckets[idx].head;
    while (*p) {
        if (strcmp((*p)->key, key) == 0) {
            cache_entry_t* e = *p;
            *p = e->hnext;
            pthread_mutex_unlock(&cache->buckets[idx].lock);

            pthread_mutex_lock(&cache->lru_lock);
            lru_remove(cache, e);
            cache->size--;
            pthread_mutex_unlock(&cache->lru_lock);

            entry_free(e);
            pthread_mutex_lock(&cache->buckets[idx].lock);
            break;
        }
        p = &(*p)->hnext;
    }

    if (!value) {
        pthread_mutex_unlock(&cache->buckets[idx].lock);
        return;
    }

    cache_entry_t* e = entry_create(key, value);
    if (!e) {
        pthread_mutex_unlock(&cache->buckets[idx].lock);
        return;
    }

    e->hnext = cache->buckets[idx].head;
    cache->buckets[idx].head = e;
    pthread_mutex_unlock(&cache->buckets[idx].lock);

    pthread_mutex_lock(&cache->lru_lock);
    e->next = cache->lru_head;
    if (cache->lru_head) cache->lru_head->prev = e;
    cache->lru_head = e;
    if (!cache->lru_tail) cache->lru_tail = e;
    cache->size++;
    pthread_mutex_unlock(&cache->lru_lock);

    if (cache->size > cache->capacity) {
        evict_lru(cache);
    }
}

void cache_clear(cache_t* cache) {
    if (!cache) return;
    for (int i = 0; i < HASH_SIZE; ++i) {
        pthread_mutex_lock(&cache->buckets[i].lock);
        cache_entry_t* e = cache->buckets[i].head;
        while (e) {
            cache_entry_t* next = e->hnext;
            entry_free(e);
            e = next;
        }
        cache->buckets[i].head = NULL;
        pthread_mutex_unlock(&cache->buckets[i].lock);
    }
    pthread_mutex_lock(&cache->lru_lock);
    cache->lru_head = cache->lru_tail = NULL;
    cache->size = 0;
    pthread_mutex_unlock(&cache->lru_lock);
}