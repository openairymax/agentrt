#include "memory_compat.h"
/**
 * @file registry.c
 * @brief 工具注册表实现（内存哈希表）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include "platform.h"
#include "registry.h"
#include "svc_logger.h"

#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>

#define HASH_SIZE 256

typedef struct registry_entry {
    char *id;
    tool_metadata_t *meta;
    struct registry_entry *next;
} registry_entry_t;

struct tool_registry {
    registry_entry_t *buckets[HASH_SIZE];
    agentos_mutex_t lock;
};

static unsigned int hash(const char *id)
{
    unsigned int h = 5381;
    while (*id)
        h = (h << 5) + h + *id++;
    return h % HASH_SIZE;
}

static tool_metadata_t *dup_metadata(const tool_metadata_t *src)
{
    tool_metadata_t *dst = AGENTOS_CALLOC(1, sizeof(tool_metadata_t));
    if (!dst) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
        }
    dst->id = AGENTOS_STRDUP(src->id);
    dst->name = AGENTOS_STRDUP(src->name);
    dst->description = src->description ? AGENTOS_STRDUP(src->description) : NULL;
    dst->executable = AGENTOS_STRDUP(src->executable);
    dst->timeout_sec = src->timeout_sec;
    dst->cacheable = src->cacheable;
    dst->permission_rule = src->permission_rule ? AGENTOS_STRDUP(src->permission_rule) : NULL;
    if (src->param_count > 0) {
        dst->params = AGENTOS_CALLOC(src->param_count, sizeof(tool_param_t));
        if (!dst->params) {
            AGENTOS_FREE(dst->id);
            AGENTOS_FREE(dst->name);
            AGENTOS_FREE(dst->description);
            AGENTOS_FREE(dst->executable);
            AGENTOS_FREE(dst->permission_rule);
            AGENTOS_FREE(dst);
            AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
            return NULL;
        }
        for (size_t i = 0; i < src->param_count; ++i) {
            dst->params[i].name = AGENTOS_STRDUP(src->params[i].name);
            dst->params[i].schema = AGENTOS_STRDUP(src->params[i].schema);
        }
        dst->param_count = src->param_count;
    }
    return dst;
}

tool_registry_t *tool_registry_create(const tool_config_t *cfg)
{
    tool_registry_t *reg = AGENTOS_CALLOC(1, sizeof(tool_registry_t));
    if (!reg) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
        }
    agentos_mutex_init(&reg->lock);

    if (cfg && cfg->tools) {
        for (tool_def_t *def = cfg->tools; def->name; ++def) {
            tool_metadata_t meta = {
                .id = def->name,
                .name = def->name,
                .executable = def->executable,
                .timeout_sec = def->timeout_sec,
                .cacheable = def->cacheable,
                .permission_rule = def->permission_rule,
            };
            if (def->params) {
                size_t cnt = 0;
                while (def->params[cnt])
                    cnt++;
                if (cnt > 0) {
                    tool_param_t *params = AGENTOS_CALLOC(cnt, sizeof(tool_param_t));
                    if (params) {
                        for (size_t i = 0; i < cnt; ++i) {
                            params[i].name = AGENTOS_STRDUP(def->params[i]);
                            const char *pname = def->params[i];
                            size_t pname_len = pname ? strlen(pname) : 0;
                            char schema[128];
                            if (pname_len > 0) {
                                const char *type_hint = "string";
                                if (strstr(pname, "count") || strstr(pname, "num") ||
                                    strstr(pname, "size") || strstr(pname, "port") ||
                                    strstr(pname, "timeout") || strstr(pname, "limit")) {
                                    type_hint = "integer";
                                } else if (strstr(pname, "enable") || strstr(pname, "flag") ||
                                           strstr(pname, "verbose") || strstr(pname, "force")) {
                                    type_hint = "boolean";
                                } else if (strstr(pname, "rate") || strstr(pname, "ratio") ||
                                           strstr(pname, "threshold") || strstr(pname, "score")) {
                                    type_hint = "number";
                                }
                                snprintf(schema, sizeof(schema), "{\"type\":\"%s\"}", type_hint);
                            } else {
                                snprintf(schema, sizeof(schema), "{}");
                            }
                            params[i].schema = AGENTOS_STRDUP(schema);
                        }
                        meta.params = params;
                        meta.param_count = cnt;
                    }
                }
            }
            tool_registry_add(reg, &meta);
            if (meta.params) {
                for (size_t i = 0; i < meta.param_count; ++i) {
                    AGENTOS_FREE((void *)meta.params[i].name);
                    AGENTOS_FREE((void *)meta.params[i].schema);
                }
                AGENTOS_FREE((void *)meta.params);
            }
        }
    }
    return reg;
}

void tool_registry_destroy(tool_registry_t *reg)
{
    if (!reg)
        return;
    agentos_mutex_lock(&reg->lock);
    for (int i = 0; i < HASH_SIZE; ++i) {
        registry_entry_t *e = reg->buckets[i];
        while (e) {
            registry_entry_t *next = e->next;
            AGENTOS_FREE(e->id);
            tool_metadata_free(e->meta);
            AGENTOS_FREE(e);
            e = next;
        }
    }
    agentos_mutex_unlock(&reg->lock);
    agentos_mutex_destroy(&reg->lock);
    AGENTOS_FREE(reg);
}

int tool_registry_add(tool_registry_t *reg, const tool_metadata_t *meta)
{
    if (!reg || !meta || !meta->id)
        return AGENTOS_ERR_INVALID_PARAM;
    unsigned int idx = hash(meta->id);
    agentos_mutex_lock(&reg->lock);
    for (registry_entry_t *e = reg->buckets[idx]; e; e = e->next) {
        if (strcmp(e->id, meta->id) == 0) {
            agentos_mutex_unlock(&reg->lock);
            return AGENTOS_ERR_ALREADY_EXISTS;
        }
    }
    registry_entry_t *e = AGENTOS_CALLOC(1, sizeof(registry_entry_t));
    if (!e) {
        agentos_mutex_unlock(&reg->lock);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    e->id = AGENTOS_STRDUP(meta->id);
    e->meta = dup_metadata(meta);
    if (!e->id || !e->meta) {
        AGENTOS_FREE(e->id);
        AGENTOS_FREE(e->meta);
        AGENTOS_FREE(e);
        agentos_mutex_unlock(&reg->lock);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    e->next = reg->buckets[idx];
    reg->buckets[idx] = e;
    agentos_mutex_unlock(&reg->lock);
    return 0;
}

int tool_registry_remove(tool_registry_t *reg, const char *tool_id)
{
    if (!reg || !tool_id)
        return AGENTOS_ERR_INVALID_PARAM;
    unsigned int idx = hash(tool_id);
    agentos_mutex_lock(&reg->lock);
    registry_entry_t **p = &reg->buckets[idx];
    while (*p) {
        if (strcmp((*p)->id, tool_id) == 0) {
            registry_entry_t *victim = *p;
            *p = victim->next;
            AGENTOS_FREE(victim->id);
            tool_metadata_free(victim->meta);
            AGENTOS_FREE(victim);
            agentos_mutex_unlock(&reg->lock);
            return 0;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(&reg->lock);
    return AGENTOS_ERR_NOT_FOUND;
}

tool_metadata_t *tool_registry_get(tool_registry_t *reg, const char *tool_id)
{
    if (!reg || !tool_id) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
        }
    unsigned int idx = hash(tool_id);
    agentos_mutex_lock(&reg->lock);
    for (registry_entry_t *e = reg->buckets[idx]; e; e = e->next) {
        if (strcmp(e->id, tool_id) == 0) {
            tool_metadata_t *copy = dup_metadata(e->meta);
            agentos_mutex_unlock(&reg->lock);
            return copy;
        }
    }
    agentos_mutex_unlock(&reg->lock);
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "operation failed");
    return NULL;
}

char *tool_registry_list_json(tool_registry_t *reg)
{
    if (!reg)
        return AGENTOS_STRDUP("[]");
    agentos_mutex_lock(&reg->lock);
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < HASH_SIZE; ++i) {
        for (registry_entry_t *e = reg->buckets[i]; e; e = e->next) {
            cJSON *obj = cJSON_CreateObject();
            cJSON_AddStringToObject(obj, "id", e->meta->id);
            cJSON_AddStringToObject(obj, "name", e->meta->name);
            cJSON_AddStringToObject(obj, "description",
                                    e->meta->description ? e->meta->description : "");
            cJSON_AddNumberToObject(obj, "cacheable", e->meta->cacheable);
            cJSON_AddItemToArray(arr, obj);
        }
    }
    char *json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    agentos_mutex_unlock(&reg->lock);
    return json;
}
