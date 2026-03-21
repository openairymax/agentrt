/**
 * @file cache.c
 * @brief 工具结果缓存实现（LRU，复用 llm_d 的 cache 逻辑）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cache.h"
#include "tool_service.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <cjson/cJSON.h>

/* 直接复制 llm_d 的 cache 实现，略作调整 */
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
    pthread_mutex_t lock;
} cache_bucket_t;

struct tool_cache {
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

static void lru_remove(tool_cache_t* cache, cache_entry_t* e) {
    if (e->prev) e->prev->next = e->next;
    if (e->next) e->next->prev = e->prev;
    if (cache->lru_head == e) cache->lru_head = e->next;
    if (cache->lru_tail == e) cache->lru_tail = e->prev;
    e->prev = e->next = NULL;
}

static void lru_move_to_head(tool_cache_t* cache, cache_entry_t* e) {
    if (cache->lru_head == e) return;
    lru_remove(cache, e);
    e->next = cache->lru_head;
    if (cache->lru_head) cache->lru_head->prev = e;
    cache->lru_head = e;
    if (!cache->lru_tail) cache->lru_tail = e;
}

static void evict_lru(tool_cache_t* cache) {
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

tool_cache_t* tool_cache_create(size_t capacity, int ttl_sec) {
    tool_cache_t* cache = calloc(1, sizeof(tool_cache_t));
    if (!cache) return NULL;
    cache->capacity = capacity;
    cache->ttl_sec = ttl_sec;
    pthread_mutex_init(&cache->lru_lock, NULL);
    for (int i = 0; i < HASH_SIZE; ++i)
        pthread_mutex_init(&cache->buckets[i].lock, NULL);
    return cache;
}

void tool_cache_destroy(tool_cache_t* cache) {
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

int tool_cache_get(tool_cache_t* cache, const char* key, char** out_value) {
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
        tool_cache_put(cache, key, NULL);
        return 0;
    }

    pthread_mutex_lock(&cache->lru_lock);
    lru_move_to_head(cache, e);
    pthread_mutex_unlock(&cache->lru_lock);

    *out_value = strdup(e->value);
    return 1;
}

void tool_cache_put(tool_cache_t* cache, const char* key, const char* value) {
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

char* tool_cache_key(const char* tool_id, const char* params_json) {
    size_t len = strlen(tool_id) + strlen(params_json) + 2;
    char* key = malloc(len);
    if (!key) return NULL;
    sprintf(key, "%s|%s", tool_id, params_json);
    return key;
}

tool_result_t* tool_result_from_json(const char* json) {
    cJSON* root = cJSON_Parse(json);
    if (!root) return NULL;
    tool_result_t* res = calloc(1, sizeof(tool_result_t));
    if (!res) {
        cJSON_Delete(root);
        return NULL;
    }
    cJSON* success = cJSON_GetObjectItem(root, "success");
    if (cJSON_IsNumber(success)) res->success = success->valueint;
    cJSON* output = cJSON_GetObjectItem(root, "output");
    if (cJSON_IsString(output)) res->output = strdup(output->valuestring);
    cJSON* error = cJSON_GetObjectItem(root, "error");
    if (cJSON_IsString(error)) res->error = strdup(error->valuestring);
    cJSON* exit_code = cJSON_GetObjectItem(root, "exit_code");
    if (cJSON_IsNumber(exit_code)) res->exit_code = exit_code->valueint;
    cJSON_Delete(root);
    return res;
}

char* tool_result_to_json(const tool_result_t* res) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "success", res->success);
    if (res->output) cJSON_AddStringToObject(root, "output", res->output);
    if (res->error) cJSON_AddStringToObject(root, "error", res->error);
    cJSON_AddNumberToObject(root, "exit_code", res->exit_code);
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}