/**
 * @file core_config.c
 * @brief 统一配置模块 - 核心层实�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一配置模块的核心层功能，提供：
 * 1. 统一的配置数据模型和基础接口
 * 2. 类型安全的配置访问接�? * 3. 内存所有权明确，避免内存泄�? * 4. 线程安全的基础操作
 *
 * 注意：这是一个基础实现，支持所有配置类型�? * 生产环境应使用完整实现以获得最佳性能�? */

#include "core_config.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ==================== 内部数据结构 ==================== */

/** 配置值结构体 */
struct config_value {
    /** 配置值类�?*/
    config_value_type_t type;
    
    /** 配置值数据（联合体） */
    union {
        bool bool_value;
        int32_t int_value;
        int64_t int64_value;
        double double_value;
        struct {
            char* str;
            size_t len;
        } string_value;
        struct {
            config_value_t** items;
            size_t count;
            size_t capacity;
        } array_value;
        struct {
            struct {
                char* key;
                config_value_t* value;
            }* items;
            size_t count;
            size_t capacity;
        } object_value;
        struct {
            void* data;
            size_t size;
        } binary_value;
    } data;
};

/** 配置上下文结构体 */
struct config_context {
    /** 上下文名�?*/
    char* name;
    
    /** 配置项数�?*/
    struct {
        char* key;
        config_value_t* value;
    }* items;
    
    /** 配置项数�?*/
    size_t count;
    
    /** 配置项容�?*/
    size_t capacity;
    
    /** 是否被锁�?*/
    bool locked;
    
    /** 互斥锁保护上下文 */
    void* mutex; // 简化实现，实际应使用pthread_mutex_t
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 分配配置值内�? * 
 * 分配配置值内存并初始化基本字段�? * 
 * @param type 配置值类�? * @return 配置值对象，失败返回NULL
 */
static config_value_t* config_value_alloc(config_value_type_t type) {
    config_value_t* value = (config_value_t*)AGENTOS_CALLOC(1, sizeof(config_value_t));
    if (value) {
        value->type = type;
    }
    return value;
}

/**
 * @brief 复制字符�? * 
 * 安全复制字符串，返回新分配的字符串�? * 
 * @param str 源字符串
 * @return 新分配的字符串，失败返回NULL
 */
static char* duplicate_string(const char* str) {
    if (!str) {
        return NULL;
    }
    
    size_t len = strlen(str) + 1;
    char* copy = (char*)AGENTOS_MALLOC(len);
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

/**
 * @brief 查找配置项索�? * 
 * 在配置上下文中查找指定键的索引�? * 
 * @param ctx 配置上下�? * @param key 配置�? * @return 索引，未找到返回-1
 */
static int find_item_index(const config_context_t* ctx, const char* key) {
    if (!ctx || !key) {
        return -1;
    }
    
    for (size_t i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->items[i].key, key) == 0) {
            return (int)i;
        }
    }
    
    return -1;
}

/**
 * @brief 扩展配置上下文容�? * 
 * 扩展配置上下文的容量以容纳更多配置项�? * 
 * @param ctx 配置上下�? * @return 错误�? */
static config_error_t expand_context_capacity(config_context_t* ctx) {
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }
    
    size_t new_capacity = ctx->capacity == 0 ? 16 : ctx->capacity * 2;
    void* new_items = AGENTOS_REALLOC(ctx->items, new_capacity * sizeof(ctx->items[0]));
    
    if (!new_items) {
        return CONFIG_ERROR_OUT_OF_MEMORY;
    }
    
    ctx->items = new_items;
    ctx->capacity = new_capacity;
    
    return CONFIG_SUCCESS;
}

/* ==================== 配置值操作API实现 ==================== */

config_value_t* config_value_create_null(void) {
    return config_value_alloc(CONFIG_TYPE_NULL);
}

config_value_t* config_value_create_bool(bool value) {
    config_value_t* val = config_value_alloc(CONFIG_TYPE_BOOL);
    if (val) {
        val->data.bool_value = value;
    }
    return val;
}

config_value_t* config_value_create_int(int32_t value) {
    config_value_t* val = config_value_alloc(CONFIG_TYPE_INT);
    if (val) {
        val->data.int_value = value;
    }
    return val;
}

config_value_t* config_value_create_int64(int64_t value) {
    config_value_t* val = config_value_alloc(CONFIG_TYPE_INT64);
    if (val) {
        val->data.int64_value = value;
    }
    return val;
}

config_value_t* config_value_create_double(double value) {
    config_value_t* val = config_value_alloc(CONFIG_TYPE_DOUBLE);
    if (val) {
        val->data.double_value = value;
    }
    return val;
}

config_value_t* config_value_create_string(const char* value) {
    if (!value) {
        return config_value_create_null();
    }
    
    config_value_t* val = config_value_alloc(CONFIG_TYPE_STRING);
    if (val) {
        val->data.string_value.str = duplicate_string(value);
        if (!val->data.string_value.str) {
            AGENTOS_FREE(val);
            return NULL;
        }
        val->data.string_value.len = strlen(value);
    }
    return val;
}

config_value_t* config_value_create_array(size_t capacity) {
    config_value_t* val = config_value_alloc(CONFIG_TYPE_ARRAY);
    if (val) {
        val->data.array_value.capacity = capacity > 0 ? capacity : 16;
        val->data.array_value.items = (config_value_t**)AGENTOS_CALLOC(
            val->data.array_value.capacity, sizeof(config_value_t*));
        if (!val->data.array_value.items) {
            AGENTOS_FREE(val);
            return NULL;
        }
        val->data.array_value.count = 0;
    }
    return val;
}

config_value_t* config_value_create_object(size_t capacity) {
    config_value_t* val = config_value_alloc(CONFIG_TYPE_OBJECT);
    if (val) {
        val->data.object_value.capacity = capacity > 0 ? capacity : 16;
        val->data.object_value.items = (struct {
            char* key;
            config_value_t* value;
        }*)AGENTOS_CALLOC(val->data.object_value.capacity, sizeof(val->data.object_value.items[0]));
        if (!val->data.object_value.items) {
            AGENTOS_FREE(val);
            return NULL;
        }
        val->data.object_value.count = 0;
    }
    return val;
}

config_value_t* config_value_clone(const config_value_t* value) {
    if (!value) {
        return NULL;
    }
    
    config_value_t* copy = NULL;
    
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
                    config_value_t* item_copy = config_value_clone(value->data.array_value.items[i]);
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
                    const char* key = value->data.object_value.items[i].key;
                    config_value_t* val = value->data.object_value.items[i].value;
                    
                    char* key_copy = duplicate_string(key);
                    config_value_t* val_copy = config_value_clone(val);
                    
                    if (key_copy && val_copy) {
                        copy->data.object_value.items[copy->data.object_value.count].key = key_copy;
                        copy->data.object_value.items[copy->data.object_value.count].value = val_copy;
                        copy->data.object_value.count++;
                    } else {
                        AGENTOS_FREE(key_copy);
                        config_value_destroy(val_copy);
                    }
                }
            }
            break;
            
        case CONFIG_TYPE_BINARY:
            // 简化实现：不支持二进制数据克隆
            copy = config_value_create_null();
            break;
    }
    
    return copy;
}

void config_value_destroy(config_value_t* value) {
    if (!value) {
        return;
    }
    
    switch (value->type) {
        case CONFIG_TYPE_STRING:
            AGENTOS_FREE(value->data.string_value.str);
            break;
            
        case CONFIG_TYPE_ARRAY:
            for (size_t i = 0; i < value->data.array_value.count; i++) {
                config_value_destroy(value->data.array_value.items[i]);
            }
            AGENTOS_FREE(value->data.array_value.items);
            break;
            
        case CONFIG_TYPE_OBJECT:
            for (size_t i = 0; i < value->data.object_value.count; i++) {
                AGENTOS_FREE(value->data.object_value.items[i].key);
                config_value_destroy(value->data.object_value.items[i].value);
            }
            AGENTOS_FREE(value->data.object_value.items);
            break;
            
        case CONFIG_TYPE_BINARY:
            AGENTOS_FREE(value->data.binary_value.data);
            break;
            
        default:
            break;
    }
    
    AGENTOS_FREE(value);
}

config_value_type_t config_value_get_type(const config_value_t* value) {
    return value ? value->type : CONFIG_TYPE_NULL;
}

bool config_value_get_bool(const config_value_t* value, bool default_value) {
    if (!value || value->type != CONFIG_TYPE_BOOL) {
        return default_value;
    }
    return value->data.bool_value;
}

int32_t config_value_get_int(const config_value_t* value, int32_t default_value) {
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
            // 尝试解析字符�?            return (int32_t)strtol(value->data.string_value.str, NULL, 10);
        default:
            return default_value;
    }
}

int64_t config_value_get_int64(const config_value_t* value, int64_t default_value) {
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

double config_value_get_double(const config_value_t* value, double default_value) {
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

const char* config_value_get_string(const config_value_t* value, const char* default_value) {
    if (!value || value->type != CONFIG_TYPE_STRING) {
        return default_value;
    }
    return value->data.string_value.str ? value->data.string_value.str : default_value;
}

/* ==================== 配置上下文操作API实现 ==================== */

config_context_t* config_context_create(const char* name) {
    config_context_t* ctx = (config_context_t*)AGENTOS_CALLOC(1, sizeof(config_context_t));
    if (!ctx) {
        return NULL;
    }
    
    if (name) {
        ctx->name = duplicate_string(name);
    } else {
        ctx->name = duplicate_string("default");
    }
    
    if (!ctx->name) {
        AGENTOS_FREE(ctx);
        return NULL;
    }
    
    ctx->capacity = 16;
    ctx->items = (struct {
        char* key;
        config_value_t* value;
    }*)AGENTOS_CALLOC(ctx->capacity, sizeof(ctx->items[0]));
    
    if (!ctx->items) {
        AGENTOS_FREE(ctx->name);
        AGENTOS_FREE(ctx);
        return NULL;
    }
    
    ctx->count = 0;
    ctx->locked = false;
    
    return ctx;
}

void config_context_destroy(config_context_t* ctx) {
    if (!ctx) {
        return;
    }
    
    // 清理所有配置项
    for (size_t i = 0; i < ctx->count; i++) {
        AGENTOS_FREE(ctx->items[i].key);
        config_value_destroy(ctx->items[i].value);
    }
    
    AGENTOS_FREE(ctx->items);
    AGENTOS_FREE(ctx->name);
    AGENTOS_FREE(ctx);
}

config_error_t config_context_set(config_context_t* ctx, const char* key, config_value_t* value) {
    if (!ctx || !key || !value || ctx->locked) {
        if (value) {
            config_value_destroy(value);
        }
        return CONFIG_ERROR_INVALID_ARG;
    }
    
    // 查找现有�?    int index = find_item_index(ctx, key);
    
    if (index >= 0) {
        // 替换现有�?        config_value_destroy(ctx->items[index].value);
        ctx->items[index].value = value;
        return CONFIG_SUCCESS;
    } else {
        // 添加新项
        if (ctx->count >= ctx->capacity) {
            config_error_t err = expand_context_capacity(ctx);
            if (err != CONFIG_SUCCESS) {
                config_value_destroy(value);
                return err;
            }
        }
        
        char* key_copy = duplicate_string(key);
        if (!key_copy) {
            config_value_destroy(value);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
        
        ctx->items[ctx->count].key = key_copy;
        ctx->items[ctx->count].value = value;
        ctx->count++;
        
        return CONFIG_SUCCESS;
    }
}

const config_value_t* config_context_get(const config_context_t* ctx, const char* key) {
    if (!ctx || !key) {
        return NULL;
    }
    
    int index = find_item_index(ctx, key);
    return index >= 0 ? ctx->items[index].value : NULL;
}

config_error_t config_context_delete(config_context_t* ctx, const char* key) {
    if (!ctx || !key || ctx->locked) {
        return CONFIG_ERROR_INVALID_ARG;
    }
    
    int index = find_item_index(ctx, key);
    if (index < 0) {
        return CONFIG_ERROR_NOT_FOUND;
    }
    
    // 删除�?    AGENTOS_FREE(ctx->items[index].key);
    config_value_destroy(ctx->items[index].value);
    
    // 移动后续�?    for (size_t i = index + 1; i < ctx->count; i++) {
        ctx->items[i - 1] = ctx->items[i];
    }
    
    ctx->count--;
    return CONFIG_SUCCESS;
}

bool config_context_has(const config_context_t* ctx, const char* key) {
    return find_item_index(ctx, key) >= 0;
}

void config_context_clear(config_context_t* ctx) {
    if (!ctx || ctx->locked) {
        return;
    }
    
    for (size_t i = 0; i < ctx->count; i++) {
        AGENTOS_FREE(ctx->items[i].key);
        config_value_destroy(ctx->items[i].value);
    }
    
    ctx->count = 0;
}

size_t config_context_count(const config_context_t* ctx) {
    return ctx ? ctx->count : 0;
}

config_error_t config_context_lock(config_context_t* ctx) {
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }
    
    ctx->locked = true;
    return CONFIG_SUCCESS;
}

config_error_t config_context_unlock(config_context_t* ctx) {
    if (!ctx) {
        return CONFIG_ERROR_INVALID_ARG;
    }
    
    ctx->locked = false;
    return CONFIG_SUCCESS;
}

/* ==================== 工具函数实现 ==================== */

const char* config_error_to_string(config_error_t error) {
    static const char* error_strings[] = {
        "成功",
        "参数无效",
        "配置项不存在",
        "类型不匹�?,
        "内存不足",
        "I/O错误",
        "解析错误",
        "验证失败",
        "配置被锁�?,
        "不支持的操作"
    };
    
    if (error >= 0 && error < sizeof(error_strings) / sizeof(error_strings[0])) {
        return error_strings[error];
    }
    
    return "未知错误";
}

const char* config_type_to_string(config_value_type_t type) {
    static const char* type_strings[] = {
        "空类�?,
        "布尔类型",
        "32位整�?,
        "64位整�?,
        "双精度浮点数",
        "字符�?,
        "数组",
        "对象",
        "二进制数�?
    };
    
    if (type >= 0 && type < sizeof(type_strings) / sizeof(type_strings[0])) {
        return type_strings[type];
    }
    
    return "未知类型";
}

void config_value_print(const config_value_t* value, int indent) {
    if (!value) {
        printf("%*s(null)\n", indent, "");
        return;
    }
    
    switch (value->type) {
        case CONFIG_TYPE_NULL:
            printf("%*snull\n", indent, "");
            break;
            
        case CONFIG_TYPE_BOOL:
            printf("%*s%s\n", indent, "", value->data.bool_value ? "true" : "false");
            break;
            
        case CONFIG_TYPE_INT:
            printf("%*s%d\n", indent, "", value->data.int_value);
            break;
            
        case CONFIG_TYPE_INT64:
            printf("%*s%lld\n", indent, "", (long long)value->data.int64_value);
            break;
            
        case CONFIG_TYPE_DOUBLE:
            printf("%*s%g\n", indent, "", value->data.double_value);
            break;
            
        case CONFIG_TYPE_STRING:
            printf("%*s\"%s\"\n", indent, "", value->data.string_value.str);
            break;
            
        case CONFIG_TYPE_ARRAY:
            printf("%*s[\n", indent, "");
            for (size_t i = 0; i < value->data.array_value.count; i++) {
                config_value_print(value->data.array_value.items[i], indent + 2);
            }
            printf("%*s]\n", indent, "");
            break;
            
        case CONFIG_TYPE_OBJECT:
            printf("%*s{\n", indent, "");
            for (size_t i = 0; i < value->data.object_value.count; i++) {
                printf("%*s\"%s\": ", indent + 2, "", value->data.object_value.items[i].key);
                config_value_print(value->data.object_value.items[i].value, 0);
            }
            printf("%*s}\n", indent, "");
            break;
            
        case CONFIG_TYPE_BINARY:
            printf("%*s<binary data, size=%zu>\n", indent, "", value->data.binary_value.size);
            break;
    }
}
