/**
 * @file sanitizer_cache.c
 * @brief 净化结果缓存实现（LRU + TTL）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "sanitizer_cache.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

sanitizer_cache_t* sanitizer_cache_create(size_t capacity, uint32_t ttl_ms) {
    sanitizer_cache_t* cache = (sanitizer_cache_t*)calloc(1, sizeof(sanitizer_cache_t));
    if (!cache) return NULL;
    cache->capacity = capacity;
    cache->ttl_ms = ttl_ms;
    pthread_mutex_init(&cache->lock, NULL);
    return cache;
}

static void free_entry(cache_entry_t* e) {
    if (!e) return;
    free(e->key);
    free(e->cleaned);
    free(e);
}

static void move_to_head(sanitizer_cache_t* cache, cache_entry_t* e) {
    if (e == cache->head) return;
    // 从链表中移除
    if (e->prev) e->prev->next = e->next;
    if (e->next) e->next->prev = e->prev;
    if (cache->tail == e) cache->tail = e->prev;
    // 插入头部
    e->next = cache->head;
    e->prev = NULL;
    if (cache->head) cache->head->prev = e;
    cache->head = e;
    if (!cache->tail) cache->tail = e;
}

static void evict_lru(sanitizer_cache_t* cache) {
    if (!cache->tail) return;
    cache_entry_t* tail = cache->tail;
    cache->tail = tail->prev;
    if (cache->tail) cache->tail->next = NULL;
    free_entry(tail);
    cache->size--;
}

void sanitizer_cache_destroy(sanitizer_cache_t* cache) {
    if (!cache) return;
    pthread_mutex_lock(&cache->lock);
    cache_entry_t* e = cache->head;
    while (e) {
        cache_entry_t* next = e->next;
        free_entry(e);
        e = next;
    }
    pthread_mutex_unlock(&cache->lock);
    pthread_mutex_destroy(&cache->lock);
    free(cache);
}

int sanitizer_cache_get(sanitizer_cache_t* cache, const char* input,
                        char** out_cleaned, int* out_risk) {
    if (!cache || !input || !out_cleaned || !out_risk) return -1;
    pthread_mutex_lock(&cache->lock);
    cache_entry_t* e = cache->head;
    uint64_t now = current_time_ms();
    int found = 0;
    while (e) {
        if (strcmp(e->key, input) == 0) {
            if (cache->ttl_ms == 0 || now - e->timestamp_ms <= cache->ttl_ms) {
                *out_cleaned = strdup(e->cleaned);
                *out_risk = e->risk;
                if (*out_cleaned) found = 1;
                move_to_head(cache, e);
            } else {
                // 过期，移除
                if (e->prev) e->prev->next = e->next;
                if (e->next) e->next->prev = e->prev;
                if (cache->head == e) cache->head = e->next;
                if (cache->tail == e) cache->tail = e->prev;
                free_entry(e);
                cache->size--;
            }
            break;
        }
        e = e->next;
    }
    pthread_mutex_unlock(&cache->lock);
    return found ? 1 : 0;
}

void sanitizer_cache_put(sanitizer_cache_t* cache, const char* input,
                         const char* cleaned, int risk) {
    if (!cache || !input || !cleaned) return;
    pthread_mutex_lock(&cache->lock);

    // 如果已存在，先删除
    cache_entry_t** p = &cache->head;
    while (*p) {
        if (strcmp((*p)->key, input) == 0) {
            cache_entry_t* victim = *p;
            *p = victim->next;
            if (victim->next) victim->next->prev = victim->prev;
            if (cache->tail == victim) cache->tail = victim->prev;
            free_entry(victim);
            cache->size--;
            break;
        }
        p = &(*p)->next;
    }

    // 创建新条目
    cache_entry_t* e = (cache_entry_t*)malloc(sizeof(cache_entry_t));
    if (!e) {
        pthread_mutex_unlock(&cache->lock);
        return;
    }
    e->key = strdup(input);
    e->cleaned = strdup(cleaned);
    e->risk = risk;
    e->timestamp_ms = current_time_ms();
    if (!e->key || !e->cleaned) {
        free(e->key);
        free(e->cleaned);
        free(e);
        pthread_mutex_unlock(&cache->lock);
        return;
    }
    e->prev = NULL;
    e->next = cache->head;
    if (cache->head) cache->head->prev = e;
    cache->head = e;
    if (!cache->tail) cache->tail = e;
    cache->size++;

    // 如果超过容量，淘汰最久未用
    while (cache->capacity > 0 && cache->size > cache->capacity) {
        evict_lru(cache);
    }

    pthread_mutex_unlock(&cache->lock);
}