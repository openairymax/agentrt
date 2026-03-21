/**
 * @file permission_cache.c
 * @brief 权限结果缓存实现（LRU + TTL）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "permission_cache.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static uint64_t hash_string(const char* s) {
    uint64_t h = 5381;
    int c;
    while ((c = *s++)) h = ((h << 5) + h) + c;
    return h;
}

static char* make_key(const char* agent, const char* action,
                      const char* resource, const char* context) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s:%s:%s:%llx",
             agent ? agent : "*",
             action ? action : "*",
             resource ? resource : "*",
             (unsigned long long)hash_string(context ? context : ""));
    return strdup(buf);
}

cache_manager_t* cache_manager_create(size_t capacity, uint32_t ttl_ms) {
    cache_manager_t* cm = (cache_manager_t*)calloc(1, sizeof(cache_manager_t));
    if (!cm) return NULL;
    cm->capacity = capacity;
    cm->ttl_ms = ttl_ms;
    pthread_mutex_init(&cm->lock, NULL);
    return cm;
}

static void free_entry(cache_entry_t* e) {
    if (!e) return;
    free(e->key);
    free(e);
}

static void move_to_head(cache_manager_t* cm, cache_entry_t* e) {
    if (e == cm->head) return;
    if (e->prev) e->prev->next = e->next;
    if (e->next) e->next->prev = e->prev;
    if (cm->tail == e) cm->tail = e->prev;
    e->next = cm->head;
    e->prev = NULL;
    if (cm->head) cm->head->prev = e;
    cm->head = e;
    if (!cm->tail) cm->tail = e;
}

static void evict_lru(cache_manager_t* cm) {
    if (!cm->tail) return;
    cache_entry_t* tail = cm->tail;
    cm->tail = tail->prev;
    if (cm->tail) cm->tail->next = NULL;
    free_entry(tail);
    cm->size--;
}

void cache_manager_destroy(cache_manager_t* cm) {
    if (!cm) return;
    pthread_mutex_lock(&cm->lock);
    cache_entry_t* e = cm->head;
    while (e) {
        cache_entry_t* next = e->next;
        free_entry(e);
        e = next;
    }
    pthread_mutex_unlock(&cm->lock);
    pthread_mutex_destroy(&cm->lock);
    free(cm);
}

int cache_manager_get(cache_manager_t* cm,
                      const char* agent_id,
                      const char* action,
                      const char* resource,
                      const char* context) {
    if (!cm) return -1;
    char* key = make_key(agent_id, action, resource, context);
    if (!key) return -1;

    pthread_mutex_lock(&cm->lock);
    cache_entry_t* e = cm->head;
    uint64_t now = current_time_ms();
    int found = -1;
    while (e) {
        if (strcmp(e->key, key) == 0) {
            if (cm->ttl_ms == 0 || now - e->timestamp_ms <= cm->ttl_ms) {
                found = e->result;
                move_to_head(cm, e);
            } else {
                // 过期，移除
                if (e->prev) e->prev->next = e->next;
                if (e->next) e->next->prev = e->prev;
                if (cm->head == e) cm->head = e->next;
                if (cm->tail == e) cm->tail = e->prev;
                free_entry(e);
                cm->size--;
            }
            break;
        }
        e = e->next;
    }
    pthread_mutex_unlock(&cm->lock);
    free(key);
    return found;
}

void cache_manager_put(cache_manager_t* cm,
                       const char* agent_id,
                       const char* action,
                       const char* resource,
                       const char* context,
                       int result) {
    if (!cm) return;
    char* key = make_key(agent_id, action, resource, context);
    if (!key) return;

    pthread_mutex_lock(&cm->lock);
    // 如果已存在，先删除
    cache_entry_t** p = &cm->head;
    while (*p) {
        if (strcmp((*p)->key, key) == 0) {
            cache_entry_t* victim = *p;
            *p = victim->next;
            if (victim->next) victim->next->prev = victim->prev;
            if (cm->tail == victim) cm->tail = victim->prev;
            free_entry(victim);
            cm->size--;
            break;
        }
        p = &(*p)->next;
    }

    cache_entry_t* e = (cache_entry_t*)malloc(sizeof(cache_entry_t));
    if (!e) {
        pthread_mutex_unlock(&cm->lock);
        free(key);
        return;
    }
    e->key = key;
    e->result = result;
    e->timestamp_ms = current_time_ms();
    e->prev = NULL;
    e->next = cm->head;
    if (cm->head) cm->head->prev = e;
    cm->head = e;
    if (!cm->tail) cm->tail = e;
    cm->size++;

    while (cm->capacity > 0 && cm->size > cm->capacity) {
        evict_lru(cm);
    }
    pthread_mutex_unlock(&cm->lock);
}

void cache_manager_clear(cache_manager_t* cm) {
    if (!cm) return;
    pthread_mutex_lock(&cm->lock);
    cache_entry_t* e = cm->head;
    while (e) {
        cache_entry_t* next = e->next;
        free_entry(e);
        e = next;
    }
    cm->head = cm->tail = NULL;
    cm->size = 0;
    pthread_mutex_unlock(&cm->lock);
}