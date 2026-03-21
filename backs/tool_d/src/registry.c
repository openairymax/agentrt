/**
 * @file registry.c
 * @brief 工具注册表实现（内存哈希表）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "registry.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#define HASH_SIZE 256

typedef struct registry_entry {
    char* id;
    tool_metadata_t* meta;
    struct registry_entry* next;
} registry_entry_t;

struct tool_registry {
    registry_entry_t* buckets[HASH_SIZE];
    pthread_mutex_t lock;
};

static unsigned int hash(const char* id) {
    unsigned int h = 5381;
    while (*id) h = (h << 5) + h + *id++;
    return h % HASH_SIZE;
}

static tool_metadata_t* dup_metadata(const tool_metadata_t* src) {
    tool_metadata_t* dst = calloc(1, sizeof(tool_metadata_t));
    if (!dst) return NULL;
    dst->id = strdup(src->id);
    dst->name = strdup(src->name);
    dst->description = src->description ? strdup(src->description) : NULL;
    dst->executable = strdup(src->executable);
    dst->timeout_sec = src->timeout_sec;
    dst->cacheable = src->cacheable;
    dst->permission_rule = src->permission_rule ? strdup(src->permission_rule) : NULL;
    if (src->param_count > 0) {
        dst->params = calloc(src->param_count, sizeof(tool_param_t));
        if (!dst->params) {
            free(dst->id); free(dst->name); free(dst->description);
            free(dst->executable); free(dst->permission_rule); free(dst);
            return NULL;
        }
        for (size_t i = 0; i < src->param_count; ++i) {
            dst->params[i].name = strdup(src->params[i].name);
            dst->params[i].schema = strdup(src->params[i].schema);
        }
        dst->param_count = src->param_count;
    }
    return dst;
}

tool_registry_t* tool_registry_create(const tool_config_t* cfg) {
    tool_registry_t* reg = calloc(1, sizeof(tool_registry_t));
    if (!reg) return NULL;
    pthread_mutex_init(&reg->lock, NULL);

    /* 注册内置工具（如果有） */
    if (cfg->tools) {
        for (tool_def_t* def = cfg->tools; def->name; ++def) {
            tool_metadata_t meta = {
                .id = def->name,
                .name = def->name,
                .executable = def->executable,
                .timeout_sec = def->timeout_sec,
                .cacheable = def->cacheable,
                .permission_rule = def->permission_rule,
            };
            /* 解析参数模式（简化：将字符串数组视为参数名，schema 留空） */
            if (def->params) {
                size_t cnt = 0;
                while (def->params[cnt]) cnt++;
                if (cnt > 0) {
                    tool_param_t* params = calloc(cnt, sizeof(tool_param_t));
                    if (params) {
                        for (size_t i = 0; i < cnt; ++i) {
                            params[i].name = strdup(def->params[i]);
                            params[i].schema = strdup("{}"); /* 默认空 schema */
                        }
                        meta.params = params;
                        meta.param_count = cnt;
                    }
                }
            }
            tool_registry_add(reg, &meta);
            /* 释放临时分配的 params（注意 dup_metadata 会重新复制） */
            if (meta.params) {
                for (size_t i = 0; i < meta.param_count; ++i) {
                    free((void*)meta.params[i].name);
                    free((void*)meta.params[i].schema);
                }
                free((void*)meta.params);
            }
        }
    }
    return reg;
}

void tool_registry_destroy(tool_registry_t* reg) {
    if (!reg) return;
    for (int i = 0; i < HASH_SIZE; ++i) {
        registry_entry_t* e = reg->buckets[i];
        while (e) {
            registry_entry_t* next = e->next;
            free(e->id);
            tool_metadata_free(e->meta);
            free(e);
            e = next;
        }
    }
    pthread_mutex_destroy(&reg->lock);
    free(reg);
}

int tool_registry_add(tool_registry_t* reg, const tool_metadata_t* meta) {
    if (!reg || !meta || !meta->id) return -1;
    unsigned int idx = hash(meta->id);
    pthread_mutex_lock(&reg->lock);
    /* 检查是否已存在 */
    for (registry_entry_t* e = reg->buckets[idx]; e; e = e->next) {
        if (strcmp(e->id, meta->id) == 0) {
            pthread_mutex_unlock(&reg->lock);
            return -1; /* 已存在 */
        }
    }
    registry_entry_t* e = calloc(1, sizeof(registry_entry_t));
    if (!e) {
        pthread_mutex_unlock(&reg->lock);
        return -1;
    }
    e->id = strdup(meta->id);
    e->meta = dup_metadata(meta);
    if (!e->id || !e->meta) {
        free(e->id);
        free(e->meta);
        free(e);
        pthread_mutex_unlock(&reg->lock);
        return -1;
    }
    e->next = reg->buckets[idx];
    reg->buckets[idx] = e;
    pthread_mutex_unlock(&reg->lock);
    return 0;
}

int tool_registry_remove(tool_registry_t* reg, const char* tool_id) {
    if (!reg || !tool_id) return -1;
    unsigned int idx = hash(tool_id);
    pthread_mutex_lock(&reg->lock);
    registry_entry_t** p = &reg->buckets[idx];
    while (*p) {
        if (strcmp((*p)->id, tool_id) == 0) {
            registry_entry_t* victim = *p;
            *p = victim->next;
            free(victim->id);
            tool_metadata_free(victim->meta);
            free(victim);
            pthread_mutex_unlock(&reg->lock);
            return 0;
        }
        p = &(*p)->next;
    }
    pthread_mutex_unlock(&reg->lock);
    return -1;
}

tool_metadata_t* tool_registry_get(tool_registry_t* reg, const char* tool_id) {
    if (!reg || !tool_id) return NULL;
    unsigned int idx = hash(tool_id);
    pthread_mutex_lock(&reg->lock);
    for (registry_entry_t* e = reg->buckets[idx]; e; e = e->next) {
        if (strcmp(e->id, tool_id) == 0) {
            tool_metadata_t* copy = dup_metadata(e->meta);
            pthread_mutex_unlock(&reg->lock);
            return copy;
        }
    }
    pthread_mutex_unlock(&reg->lock);
    return NULL;
}

char* tool_registry_list_json(tool_registry_t* reg) {
    if (!reg) return strdup("[]");
    pthread_mutex_lock(&reg->lock);
    cJSON* arr = cJSON_CreateArray();
    for (int i = 0; i < HASH_SIZE; ++i) {
        for (registry_entry_t* e = reg->buckets[i]; e; e = e->next) {
            cJSON* obj = cJSON_CreateObject();
            cJSON_AddStringToObject(obj, "id", e->meta->id);
            cJSON_AddStringToObject(obj, "name", e->meta->name);
            cJSON_AddStringToObject(obj, "description", e->meta->description ? e->meta->description : "");
            cJSON_AddNumberToObject(obj, "cacheable", e->meta->cacheable);
            cJSON_AddItemToArray(arr, obj);
        }
    }
    char* json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    pthread_mutex_unlock(&reg->lock);
    return json;
}