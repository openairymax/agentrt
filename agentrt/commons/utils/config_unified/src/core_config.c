/**
 * @file core_config.c
 * @brief 统一配置模块 - 核心层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一配置模块的核心层功能，提供：
 * 1. 统一的配置数据模型和基础接口
 * 2. 类型安全的配置访问接口
 * 3. 内存所有权明确，避免内存泄漏
 * 4. 线程安全的基础操作
 */

#include "core_config.h"
#include "logging_compat.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"

#define INDEX_NOT_FOUND (-1)

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "error.h"



/* ==================== 内部数据结构 ==================== */

/** 配置值结构体 */
struct config_value {
    /** 配置值类?*/
    config_value_type_t type;

    /** 配置值数据（联合体） */
    union {
        bool bool_value;
        int32_t int_value;
        int64_t int64_value;
        double double_value;
        struct {
            char *str;
            size_t len;
        } string_value;
        struct {
            config_value_t **items;
            size_t count;
            size_t capacity;
        } array_value;
        struct {
            struct {
                char *key;
                config_value_t *value;
            } *items;
            size_t count;
            size_t capacity;
        } object_value;
        struct {
            void *data;
            size_t size;
        } binary_value;
    } data;
};

/** 配置上下文结构体 */
struct config_context {
    char *name;

    struct {
        char *key;
        config_value_t *value;
    } *items;

    size_t count;

    size_t capacity;

    bool locked;

    agentrt_mutex_t mutex;

    config_schema_t *schema;
    bool hot_reload_enabled;
    uint32_t reload_interval_ms;
    bool encryption_enabled;
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 分配配置值内? *
 * 分配配置值内存并初始化基本字段? *
 * @param type 配置值类? * @return 配置值对象，失败返回NULL
 */
static config_value_t *config_value_alloc(config_value_type_t type)
{
    config_value_t *value = (config_value_t *)AGENTRT_CALLOC(1, sizeof(config_value_t));
    if (value) {
        value->type = type;
    }
    return value;
}

/**
 * @brief 复制字符? *
 * 安全复制字符串，返回新分配的字符串? *
 * @param str 源字符串
 * @return 新分配的字符串，失败返回NULL
 */
static char *duplicate_string(const char *str)
{
    if (!str) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    size_t len = strlen(str) + 1;
    char *copy = (char *)AGENTRT_MALLOC(len);
    if (copy) {
        __builtin_memcpy(copy, str, len);
    }
    return copy;
}

/**
 * @brief 查找配置项索? *
 * 在配置上下文中查找指定键的索引? *
 * @param ctx 配置上下? * @param key 配置? * @return 索引，未找到返回-1
 */
static int find_item_index(const config_context_t *ctx, const char *key)
{
    if (!ctx || !key) {
        return INDEX_NOT_FOUND;
    }

    for (size_t i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->items[i].key, key) == 0) {
            return (int)i;
        }
    }

    return INDEX_NOT_FOUND;
}

/**
 * @brief 扩展配置上下文容? *
 * 扩展配置上下文的容量以容纳更多配置项? *
 * @param ctx 配置上下? * @return 错误? */
static config_error_t expand_context_capacity(config_context_t *ctx)
{
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    size_t new_capacity = ctx->capacity == 0 ? 16 : ctx->capacity * 2;
    void *new_items = AGENTRT_REALLOC(ctx->items, new_capacity * sizeof(ctx->items[0]));

    if (!new_items) {
        return CONFIG_ERROR_OUT_OF_MEMORY;
    }

    ctx->items = new_items;
    ctx->capacity = new_capacity;

    return CONFIG_SUCCESS;
}

/* ==================== 配置值操作API实现 ==================== */

config_value_t *config_value_create_null(void)
{
    return config_value_alloc(CONFIG_TYPE_NULL);
}

config_value_t *config_value_create_bool(bool value)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_BOOL);
    if (val) {
        val->data.bool_value = value;
    }
    return val;
}

config_value_t *config_value_create_int(int32_t value)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_INT);
    if (val) {
        val->data.int_value = value;
    }
    return val;
}

config_value_t *config_value_create_int64(int64_t value)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_INT64);
    if (val) {
        val->data.int64_value = value;
    }
    return val;
}

config_value_t *config_value_create_double(double value)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_DOUBLE);
    if (val) {
        val->data.double_value = value;
    }
    return val;
}

config_value_t *config_value_create_string(const char *value)
{
    if (!value) {
        return config_value_create_null();
    }

    config_value_t *val = config_value_alloc(CONFIG_TYPE_STRING);
    if (val) {
        val->data.string_value.str = duplicate_string(value);
        if (!val->data.string_value.str) {
            AGENTRT_FREE(val);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
        val->data.string_value.len = strlen(value);
    }
    return val;
}

config_value_t *config_value_create_array(size_t capacity)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_ARRAY);
    if (val) {
        val->data.array_value.capacity = capacity > 0 ? capacity : 16;
        val->data.array_value.items = (config_value_t **)AGENTRT_CALLOC(
            val->data.array_value.capacity, sizeof(config_value_t *));
        if (!val->data.array_value.items) {
            AGENTRT_FREE(val);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
        val->data.array_value.count = 0;
    }
    return val;
}

config_value_t *config_value_create_object(size_t capacity)
{
    config_value_t *val = config_value_alloc(CONFIG_TYPE_OBJECT);
    if (val) {
        val->data.object_value.capacity = capacity > 0 ? capacity : 16;
        val->data.object_value.items = AGENTRT_CALLOC(val->data.object_value.capacity,
                                                      sizeof(val->data.object_value.items[0]));
        if (!val->data.object_value.items) {
            AGENTRT_FREE(val);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
        val->data.object_value.count = 0;
    }
    return val;
}

config_value_t *config_value_clone(const config_value_t *value)
{
    if (!value) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    config_value_t *copy = NULL;

    switch (value->type) {
    case CONFIG_TYPE_NULL:
        copy = config_value_create_null();
        break;

    case CONFIG_TYPE_BOOL:
        copy = config_value_create_bool(value->data.bool_value);
        break;

    case CONFIG_TYPE_INT:
        copy = config_value_create_int(value->data.int_value);
        break;

    case CONFIG_TYPE_INT64:
        copy = config_value_create_int64(value->data.int64_value);
        break;

    case CONFIG_TYPE_DOUBLE:
        copy = config_value_create_double(value->data.double_value);
        break;

    case CONFIG_TYPE_STRING:
        copy = config_value_create_string(value->data.string_value.str);
        break;

    case CONFIG_TYPE_ARRAY:
        copy = config_value_create_array(value->data.array_value.capacity);
        if (copy) {
            for (size_t i = 0; i < value->data.array_value.count; i++) {
                config_value_t *item_copy = config_value_clone(value->data.array_value.items[i]);
                if (item_copy) {
                    copy->data.array_value.items[copy->data.array_value.count++] = item_copy;
                }
            }
        }
        break;

    case CONFIG_TYPE_OBJECT:
        copy = config_value_create_object(value->data.object_value.capacity);
        if (copy) {
            for (size_t i = 0; i < value->data.object_value.count; i++) {
                const char *key = value->data.object_value.items[i].key;
                config_value_t *val = value->data.object_value.items[i].value;

                char *key_copy = duplicate_string(key);
                config_value_t *val_copy = config_value_clone(val);

                if (key_copy && val_copy) {
                    copy->data.object_value.items[copy->data.object_value.count].key = key_copy;
                    copy->data.object_value.items[copy->data.object_value.count].value = val_copy;
                    copy->data.object_value.count++;
                } else {
                    AGENTRT_FREE(key_copy);
                    config_value_destroy(val_copy);
                }
            }
        }
        break;

    case CONFIG_TYPE_BINARY:
        if (value->data.binary_value.data && value->data.binary_value.size > 0) {
            copy = config_value_alloc(CONFIG_TYPE_BINARY);
            if (copy) {
                copy->data.binary_value.data = AGENTRT_MALLOC(value->data.binary_value.size);
                if (copy->data.binary_value.data) {
                    __builtin_memcpy(copy->data.binary_value.data, value->data.binary_value.data,
                           value->data.binary_value.size);
                    copy->data.binary_value.size = value->data.binary_value.size;
                } else {
                    AGENTRT_FREE(copy);
                    copy = NULL;
                }
            }
        } else {
            copy = config_value_alloc(CONFIG_TYPE_BINARY);
            if (copy) {
                copy->data.binary_value.data = NULL;
                copy->data.binary_value.size = 0;
            }
        }
        break;
    }

    return copy;
}

void config_value_destroy(config_value_t *value)
{
    if (!value) {
        return;
    }

    switch (value->type) {
    case CONFIG_TYPE_STRING:
        AGENTRT_FREE(value->data.string_value.str);
        break;

    case CONFIG_TYPE_ARRAY:
        for (size_t i = 0; i < value->data.array_value.count; i++) {
            config_value_destroy(value->data.array_value.items[i]);
        }
        AGENTRT_FREE(value->data.array_value.items);
        break;

    case CONFIG_TYPE_OBJECT:
        for (size_t i = 0; i < value->data.object_value.count; i++) {
            AGENTRT_FREE(value->data.object_value.items[i].key);
            config_value_destroy(value->data.object_value.items[i].value);
        }
        AGENTRT_FREE(value->data.object_value.items);
        break;

    case CONFIG_TYPE_BINARY:
        AGENTRT_FREE(value->data.binary_value.data);
        break;

    default:
        break;
    }

    AGENTRT_FREE(value);
}

config_error_t config_value_array_append(config_value_t *array, config_value_t *item)
{
    if (!array || !item)
        return CONFIG_ERROR_INVALID_ARG;
    if (array->type != CONFIG_TYPE_ARRAY)
        return CONFIG_ERROR_TYPE_MISMATCH;

    if (array->data.array_value.count >= array->data.array_value.capacity) {
        size_t new_cap = array->data.array_value.capacity * 2;
        config_value_t **new_items = (config_value_t **)AGENTRT_REALLOC(
            array->data.array_value.items, new_cap * sizeof(config_value_t *));
        if (!new_items)
            return CONFIG_ERROR_OUT_OF_MEMORY;
        array->data.array_value.items = new_items;
        array->data.array_value.capacity = new_cap;
    }

    array->data.array_value.items[array->data.array_value.count++] = item;
    return CONFIG_SUCCESS;
}

config_value_type_t config_value_get_type(const config_value_t *value)
{
    return value ? value->type : CONFIG_TYPE_NULL;
}

bool config_value_get_bool(const config_value_t *value, bool default_value)
{
    if (!value || value->type != CONFIG_TYPE_BOOL) {
        return default_value;
    }
    return value->data.bool_value;
}

int32_t config_value_get_int(const config_value_t *value, int32_t default_value)
{
    if (!value) {
        return default_value;
    }

    switch (value->type) {
    case CONFIG_TYPE_INT:
        return value->data.int_value;
    case CONFIG_TYPE_INT64:
        return (int32_t)value->data.int64_value;
    case CONFIG_TYPE_DOUBLE:
        return (int32_t)value->data.double_value;
    case CONFIG_TYPE_STRING:
        // 尝试解析字符串
        return (int32_t)strtol(value->data.string_value.str, NULL, 10);
    default:
        return default_value;
    }
}

int64_t config_value_get_int64(const config_value_t *value, int64_t default_value)
{
    if (!value) {
        return default_value;
    }

    switch (value->type) {
    case CONFIG_TYPE_INT:
        return (int64_t)value->data.int_value;
    case CONFIG_TYPE_INT64:
        return value->data.int64_value;
    case CONFIG_TYPE_DOUBLE:
        return (int64_t)value->data.double_value;
    case CONFIG_TYPE_STRING:
        return strtoll(value->data.string_value.str, NULL, 10);
    default:
        return default_value;
    }
}

double config_value_get_double(const config_value_t *value, double default_value)
{
    if (!value) {
        return default_value;
    }

    switch (value->type) {
    case CONFIG_TYPE_INT:
        return (double)value->data.int_value;
    case CONFIG_TYPE_INT64:
        return (double)value->data.int64_value;
    case CONFIG_TYPE_DOUBLE:
        return value->data.double_value;
    case CONFIG_TYPE_STRING:
        return strtod(value->data.string_value.str, NULL);
    default:
        return default_value;
    }
}

const char *config_value_get_string(const config_value_t *value, const char *default_value)
{
    if (!value || value->type != CONFIG_TYPE_STRING) {
        return default_value;
    }
    return value->data.string_value.str ? value->data.string_value.str : default_value;
}

/* ==================== 配置上下文操作API实现 ==================== */

config_context_t *config_context_create(const char *name)
{
    config_context_t *ctx = (config_context_t *)AGENTRT_CALLOC(1, sizeof(config_context_t));
    if (!ctx) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    if (name) {
        ctx->name = duplicate_string(name);
    } else {
        ctx->name = duplicate_string("default");
    }

    if (!ctx->name) {
        AGENTRT_FREE(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    ctx->capacity = 16;
    ctx->items = AGENTRT_CALLOC(ctx->capacity, sizeof(ctx->items[0]));

    if (!ctx->items) {
        AGENTRT_FREE(ctx->name);
        AGENTRT_FREE(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    ctx->count = 0;
    ctx->locked = false;
    agentrt_mutex_init(&ctx->mutex);

    return ctx;
}

void config_context_destroy(config_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    agentrt_mutex_destroy(&ctx->mutex);

    for (size_t i = 0; i < ctx->count; i++) {
        AGENTRT_FREE(ctx->items[i].key);
        config_value_destroy(ctx->items[i].value);
    }

    AGENTRT_FREE(ctx->items);
    AGENTRT_FREE(ctx->name);
    AGENTRT_FREE(ctx);
}

config_error_t config_context_set(config_context_t *ctx, const char *key, config_value_t *value)
{
    if (!ctx || !key || !value || ctx->locked) {
        if (value) {
            config_value_destroy(value);
        }
        return CONFIG_ERROR_INVALID_ARG;
    }

    /* v0.1.1 修复（并发安全 Bug E）：config_context_set 缺少 mutex 保护。
     * 此前 config_context_get/delete/foreach 等操作均持有 mutex，唯独 set 未加锁，
     * 导致多线程并发调用 config_source_load 写入同一 context 时，
     * find_item_index + config_value_destroy + 赋值之间存在 TOCTOU 竞争，
     * 产生 double-free（ASAN 检测到 config_value_destroy 对同一指针被两个线程
     * 同时调用）。修复：对 set 操作加 mutex，与 get/delete 保持一致。 */
    agentrt_mutex_lock(&ctx->mutex);

    // 查找现有项
    int index = find_item_index(ctx, key);

    if (index >= 0) {
        // 替换现有项
        config_value_destroy(ctx->items[index].value);
        ctx->items[index].value = value;
        agentrt_mutex_unlock(&ctx->mutex);
        return CONFIG_SUCCESS;
    } else {
        // 添加新项
        if (ctx->count >= ctx->capacity) {
            config_error_t err = expand_context_capacity(ctx);
            if (err != CONFIG_SUCCESS) {
                config_value_destroy(value);
                agentrt_mutex_unlock(&ctx->mutex);
                return err;
            }
        }

        char *key_copy = duplicate_string(key);
        if (!key_copy) {
            config_value_destroy(value);
            agentrt_mutex_unlock(&ctx->mutex);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }

        ctx->items[ctx->count].key = key_copy;
        ctx->items[ctx->count].value = value;
        ctx->count++;

        agentrt_mutex_unlock(&ctx->mutex);
        return CONFIG_SUCCESS;
    }
}

const config_value_t *config_context_get(const config_context_t *ctx, const char *key)
{
    if (!ctx || !key) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    agentrt_mutex_lock((agentrt_mutex_t *)&ctx->mutex);
    int index = find_item_index(ctx, key);
    const config_value_t *result = index >= 0 ? ctx->items[index].value : NULL;
    agentrt_mutex_unlock((agentrt_mutex_t *)&ctx->mutex);
    return result;
}

config_error_t config_context_delete(config_context_t *ctx, const char *key)
{
    if (!ctx || !key) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    if (ctx->locked) {
        return CONFIG_ERROR_LOCKED;
    }

    agentrt_mutex_lock(&ctx->mutex);

    int index = find_item_index(ctx, key);
    if (index < 0) {
        agentrt_mutex_unlock(&ctx->mutex);
        return CONFIG_ERROR_NOT_FOUND;
    }

    AGENTRT_FREE(ctx->items[index].key);
    config_value_destroy(ctx->items[index].value);

    for (size_t i = index + 1; i < ctx->count; i++) {
        ctx->items[i - 1] = ctx->items[i];
    }

    ctx->count--;
    agentrt_mutex_unlock(&ctx->mutex);
    return CONFIG_SUCCESS;
}

bool config_context_has(const config_context_t *ctx, const char *key)
{
    if (!ctx || !key)
        return false;
    agentrt_mutex_lock((agentrt_mutex_t *)&ctx->mutex);
    bool result = find_item_index(ctx, key) >= 0;
    agentrt_mutex_unlock((agentrt_mutex_t *)&ctx->mutex);
    return result;
}

void config_context_clear(config_context_t *ctx)
{
    if (!ctx)
        return;

    if (ctx->locked)
        return;

    agentrt_mutex_lock(&ctx->mutex);

    for (size_t i = 0; i < ctx->count; i++) {
        AGENTRT_FREE(ctx->items[i].key);
        config_value_destroy(ctx->items[i].value);
    }

    ctx->count = 0;
    agentrt_mutex_unlock(&ctx->mutex);
}

size_t config_context_count(const config_context_t *ctx)
{
    if (!ctx)
        return 0;
    agentrt_mutex_lock((agentrt_mutex_t *)&ctx->mutex);
    size_t result = ctx->count;
    agentrt_mutex_unlock((agentrt_mutex_t *)&ctx->mutex);
    return result;
}

config_error_t config_context_lock(config_context_t *ctx)
{
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    ctx->locked = true;
    return CONFIG_SUCCESS;
}

config_error_t config_context_unlock(config_context_t *ctx)
{
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    ctx->locked = false;
    return CONFIG_SUCCESS;
}

config_context_t *config_context_clone(const config_context_t *ctx)
{
    if (!ctx) return NULL;

    config_context_t *clone = config_context_create(ctx->name);
    if (!clone) return NULL;

    for (size_t i = 0; i < ctx->count; i++) {
        char *key_copy = duplicate_string(ctx->items[i].key);
        if (!key_copy) {
            config_context_destroy(clone);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
        config_value_t *val_copy = config_value_clone(ctx->items[i].value);
        if (!val_copy) {
            AGENTRT_FREE(key_copy);
            config_context_destroy(clone);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
        if (clone->count >= clone->capacity) {
            config_error_t err = expand_context_capacity(clone);
            if (err != CONFIG_SUCCESS) {
                AGENTRT_FREE(key_copy);
                config_value_destroy(val_copy);
                config_context_destroy(clone);
                AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
            }
        }
        clone->items[clone->count].key = key_copy;
        clone->items[clone->count].value = val_copy;
        clone->count++;
    }

    clone->locked = ctx->locked;
    return clone;
}

config_error_t config_context_copy(config_context_t *dst, const config_context_t *src)
{
    if (!dst || !src)
        return CONFIG_ERROR_INVALID_ARG;
    if (dst->locked)
        return CONFIG_ERROR_LOCKED;

    config_context_clear(dst);

    for (size_t i = 0; i < src->count; i++) {
        char *key_copy = duplicate_string(src->items[i].key);
        if (!key_copy)
            return CONFIG_ERROR_OUT_OF_MEMORY;
        config_value_t *val_copy = config_value_clone(src->items[i].value);
        if (!val_copy) {
            AGENTRT_FREE(key_copy);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
        if (dst->count >= dst->capacity) {
            config_error_t err = expand_context_capacity(dst);
            if (err != CONFIG_SUCCESS) {
                AGENTRT_FREE(key_copy);
                config_value_destroy(val_copy);
                return err;
            }
        }
        dst->items[dst->count].key = key_copy;
        dst->items[dst->count].value = val_copy;
        dst->count++;
    }

    return CONFIG_SUCCESS;
}

const char *config_context_get_key_at(const config_context_t *ctx, size_t index)
{
    if (!ctx || index >= ctx->count) return NULL;
    return ctx->items[index].key;
}

const config_value_t *config_context_get_value_at(const config_context_t *ctx, size_t index)
{
    if (!ctx || index >= ctx->count) return NULL;
    return ctx->items[index].value;
}

struct config_iterator {
    const config_context_t *ctx;
    size_t pos;
};

const config_iterator_t *config_context_iterator(const config_context_t *ctx)
{
    if (!ctx) return NULL;
    config_iterator_t *it = (config_iterator_t *)AGENTRT_CALLOC(1, sizeof(config_iterator_t));
    if (!it) return NULL;
    it->ctx = ctx;
    it->pos = 0;
    return it;
}

void config_iterator_reset(const config_iterator_t *it)
{
    if (!it)
        return;
    ((config_iterator_t *)it)->pos = 0;
}

bool config_iterator_has_next(const config_iterator_t *it)
{
    if (!it || !it->ctx)
        return false;
    return it->pos < it->ctx->count;
}

const char *config_iterator_next_key(const config_iterator_t *it)
{
    if (!it || !it->ctx || it->pos >= it->ctx->count) return NULL;
    const char *key = it->ctx->items[it->pos].key;
    ((config_iterator_t *)it)->pos++;
    return key;
}

void config_context_set_schema(config_context_t *ctx, config_schema_t *schema)
{
    if (!ctx)
        return;
    ctx->schema = schema;
}

void config_context_set_hot_reload(config_context_t *ctx, bool enabled, uint32_t interval_ms)
{
    if (!ctx)
        return;
    ctx->hot_reload_enabled = enabled;
    ctx->reload_interval_ms = interval_ms;
}

void config_context_set_encryption(config_context_t *ctx, bool enabled)
{
    if (!ctx)
        return;
    ctx->encryption_enabled = enabled;
}

/* ==================== 工具函数实现 ==================== */

const char *config_error_to_string(config_error_t error)
{
    static const char *error_strings[] = {"Success",       "Invalid argument",
                                          "Not found",     "Type mismatch",
                                          "Out of memory", "I/O error",
                                          "Parse error",   "Validation failed",
                                          "Config locked", "Unsupported operation"};

    if (error >= 0 && error < sizeof(error_strings) / sizeof(error_strings[0])) {
        return error_strings[error];
    }

    return "Unknown error";
}

const char *config_type_to_string(config_value_type_t type)
{
    static const char *type_strings[] = {"Null",   "Boolean", "Int32",  "Int64", "Double",
                                         "String", "Array",   "Object", "Binary"};

    if (type >= 0 && type < sizeof(type_strings) / sizeof(type_strings[0])) {
        return type_strings[type];
    }

    return "未知类型";
}

void config_value_print(const config_value_t *value, int indent)
{
    if (!value) {
        AGENTRT_LOG_DEBUG("%*s(null)", indent, "");
        return;
    }

    switch (value->type) {
    case CONFIG_TYPE_NULL:
        AGENTRT_LOG_DEBUG("%*snull", indent, "");
        break;

    case CONFIG_TYPE_BOOL:
        AGENTRT_LOG_DEBUG("%*s%s", indent, "", value->data.bool_value ? "true" : "false");
        break;

    case CONFIG_TYPE_INT:
        AGENTRT_LOG_DEBUG("%*s%d", indent, "", value->data.int_value);
        break;

    case CONFIG_TYPE_INT64:
        AGENTRT_LOG_DEBUG("%*s%lld", indent, "", (long long)value->data.int64_value);
        break;

    case CONFIG_TYPE_DOUBLE:
        AGENTRT_LOG_DEBUG("%*s%g", indent, "", value->data.double_value);
        break;

    case CONFIG_TYPE_STRING:
        AGENTRT_LOG_DEBUG("%*s\"%s\"", indent, "", value->data.string_value.str);
        break;

    case CONFIG_TYPE_ARRAY:
        AGENTRT_LOG_DEBUG("%*s[", indent, "");
        for (size_t i = 0; i < value->data.array_value.count; i++) {
            config_value_print(value->data.array_value.items[i], indent + 2);
        }
        AGENTRT_LOG_DEBUG("%*s]", indent, "");
        break;

    case CONFIG_TYPE_OBJECT:
        AGENTRT_LOG_DEBUG("%*s{", indent, "");
        for (size_t i = 0; i < value->data.object_value.count; i++) {
            AGENTRT_LOG_DEBUG("%*s\"%s\": ", indent + 2, "", value->data.object_value.items[i].key);
            config_value_print(value->data.object_value.items[i].value, 0);
        }
        AGENTRT_LOG_DEBUG("%*s}", indent, "");
        break;

    case CONFIG_TYPE_BINARY:
        AGENTRT_LOG_DEBUG("%*s<binary data, size=%zu>", indent, "", value->data.binary_value.size);
        break;
    }
}
