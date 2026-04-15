/**
 * @file cache.c
 * @brief 工具结果缓存实现（LRU�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cache.h"
#include <agentos/memory_common.h>
#include "tool_service.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cjson/cJSON.h>

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
    agentos_mutex_t lock;
} cache_bucket_t;

struct tool_cache {
    cache_bucket_t buckets[HASH_SIZE];
    cache_entry_t* lru_head;
    cache_entry_t* lru_tail;
    size_t capacity;
    size_t size;
    int ttl_sec;
    agentos_mutex_t lru_lock;
};

static unsigned int hash_key(const char* key) {
    unsigned int h = 5381;
    while (*key) h = (h << 5) + h + *key++;
    return h % HASH_SIZE;
}

static cache_entry_t* entry_create(const char* key, const char* value) {
    cache_entry_t* e = memory_safe_alloc(sizeof(cache_entry_t));
    if (!e) return NULL;
    e->key = memory_safe_strdup(key);
    e->value = memory_safe_strdup(value);
    e->timestamp = time(NULL);
    e->prev = e->next = e->hnext = NULL;
    return e;
}

static void entry_memory_safe_free(cache_entry_t* e) {
    if (!e) return;
    memory_safe_free(e->key);
    memory_safe_free(e->value);
    memory_safe_free(e);
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

    agentos_mutex_lock(&cache->buckets[idx].lock);
    cache_entry_t** p = &cache->buckets[idx].head;
    while (*p) {
        if (*p == victim) {
            *p = victim->hnext;
            break;
        }
        p = &(*p)->hnext;
    }
    agentos_mutex_unlock(&cache->buckets[idx].lock);

    lru_remove(cache, victim);
    entry_memory_safe_free(victim);
    cache->size--;
}

tool_cache_t* tool_cache_create(size_t capacity, int ttl_sec) {
    tool_cache_t* cache = calloc(1, sizeof(tool_cache_t));
    if (!cache) return NULL;
    cache->capacity = capacity;
    cache->ttl_sec = ttl_sec;
    agentos_mutex_init(&cache->lru_lock);
    for (int i = 0; i < HASH_SIZE; ++i)
        agentos_mutex_init(&cache->buckets[i].lock);
    return cache;
}

void tool_cache_destroy(tool_cache_t* cache) {
    if (!cache) return;
    for (int i = 0; i < HASH_SIZE; ++i) {
        agentos_mutex_lock(&cache->buckets[i].lock);
        cache_entry_t* e = cache->buckets[i].head;
        while (e) {
            cache_entry_t* next = e->hnext;
            entry_memory_safe_free(e);
            e = next;
        }
        agentos_mutex_unlock(&cache->buckets[i].lock);
        agentos_mutex_destroy(&cache->buckets[i].lock);
    }
    agentos_mutex_destroy(&cache->lru_lock);
    memory_safe_free(cache);
}

int tool_cache_get(tool_cache_t* cache, const char* key, char** out_value) {
    if (!cache || !key || !out_value) return -1;
    *out_value = NULL;

    unsigned int idx = hash_key(key);
    agentos_mutex_lock(&cache->buckets[idx].lock);
    cache_entry_t* e = cache->buckets[idx].head;
    while (e) {
        if (strcmp(e->key, key) == 0) break;
        e = e->hnext;
    }
    agentos_mutex_unlock(&cache->buckets[idx].lock);

    if (!e) return 0;

    if (cache->ttl_sec > 0 && (time(NULL) - e->timestamp) > cache->ttl_sec) {
        tool_cache_put(cache, key, NULL);
        return 0;
    }

    agentos_mutex_lock(&cache->lru_lock);
    lru_move_to_head(cache, e);
    agentos_mutex_unlock(&cache->lru_lock);

    *out_value = memory_safe_strdup(e->value);
    return 1;
}

void tool_cache_put(tool_cache_t* cache, const char* key, const char* value) {
    if (!cache || !key) return;
    if (cache->capacity == 0) return;

    unsigned int idx = hash_key(key);
    agentos_mutex_lock(&cache->buckets[idx].lock);

    cache_entry_t** p = &cache->buckets[idx].head;
    while (*p) {
        if (strcmp((*p)->key, key) == 0) {
            cache_entry_t* e = *p;
            *p = e->hnext;
            agentos_mutex_unlock(&cache->buckets[idx].lock);

            agentos_mutex_lock(&cache->lru_lock);
            lru_remove(cache, e);
            cache->size--;
            agentos_mutex_unlock(&cache->lru_lock);

            entry_memory_safe_free(e);
            agentos_mutex_lock(&cache->buckets[idx].lock);
            break;
        }
        p = &(*p)->hnext;
    }

    if (!value) {
        agentos_mutex_unlock(&cache->buckets[idx].lock);
        return;
    }

    cache_entry_t* e = entry_create(key, value);
    if (!e) {
        agentos_mutex_unlock(&cache->buckets[idx].lock);
        return;
    }

    e->hnext = cache->buckets[idx].head;
    cache->buckets[idx].head = e;
    agentos_mutex_unlock(&cache->buckets[idx].lock);

    agentos_mutex_lock(&cache->lru_lock);
    e->next = cache->lru_head;
    if (cache->lru_head) cache->lru_head->prev = e;
    cache->lru_head = e;
    if (!cache->lru_tail) cache->lru_tail = e;
    cache->size++;
    agentos_mutex_unlock(&cache->lru_lock);

    if (cache->size > cache->capacity) {
        evict_lru(cache);
    }
}

char* tool_cache_key(const char* tool_id, const char* params_json) {
    if (!tool_id || !params_json) return NULL;
    
    size_t tool_id_len = strlen(tool_id);
    size_t params_len = strlen(params_json);
    size_t len = tool_id_len + params_len + 2;
    
    char* key = memory_safe_alloc(len);
    if (!key) return NULL;
    
    snprintf(key, len, "%s|%s", tool_id, params_json);
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
    if (cJSON_IsString(output)) res->output = memory_safe_strdup(output->valuestring);
    cJSON* error = cJSON_GetObjectItem(root, "error");
    if (cJSON_IsString(error)) res->error = memory_safe_strdup(error->valuestring);
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
