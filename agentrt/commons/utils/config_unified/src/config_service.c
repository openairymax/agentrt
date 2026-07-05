/**
 * @file config_service.c
 * @brief 统一配置模块 - 服务层实? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一配置模块的服务层功能，提供：
 * 1. 配置验证和Schema定义
 * 2. 热更新和变化通知
 * 3. 配置加密和安全存? * 4. 配置版本管理和回? * 5. 配置模板和变量展开
 *
 * ? */

#include "config_service.h"

#include "config_source.h"
#include "core_config.h"

#include <platform.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#define INDEX_NOT_FOUND (-1)

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "error.h"

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif



/* ==================== 内部数据结构 ==================== */

/** 配置验证器结构体 */
struct config_validator {
    validator_type_t type;  // 验证器类
    char *pattern;          // 模式（正则表达式或范围）
    char **enum_values;     // 枚举值数量
    size_t enum_count;
    // 枚举值数?
    config_validator_cb_t custom_cb;  // 自定义验证回?
    void *user_data;
    // 用户数据
    char *error_message;  // 错误信息
};

/** 配置Schema项内部结?*/
typedef struct {
    char *key;
    config_value_type_t type;
    bool required;
    char *description;
    char *default_value;
    config_validator_t *validator;
} schema_item_internal_t;

/** 配置Schema结构?*/
struct config_schema {
    char *name;
    schema_item_internal_t *items;
    size_t count;
    size_t capacity;
    char **errors;
    size_t error_count;
    size_t error_capacity;
};

/** 配置变化回调?*/
typedef struct {
    char *key;                    // 配置键（NULL表示所有）
    config_change_cb_t callback;  // 回调函数
    void *user_data;              // 用户数据
} change_callback_item_t;

/** 热更新管理器结构体 */
struct config_hot_reload_manager {
    config_context_t *ctx;
    config_source_manager_t *source_manager;
    change_callback_item_t *callbacks;
    size_t callback_count;
    size_t callback_capacity;
    volatile bool running;
    uint32_t check_interval_ms;
    uint32_t debounce_ms;
    uint64_t last_trigger_time_ms;
    void *thread_handle;
    void *lock;
};

/** 配置版本?*/
typedef struct {
    uint32_t version;
    uint64_t timestamp;
    char *author;
    char *description;
    config_context_t *snapshot;
} config_version_item_t;

/** 版本管理器结构体 */
struct config_version_manager {
    config_context_t *ctx;
    config_version_item_t *versions;
    size_t count;
    size_t capacity;
    size_t max_versions;
    uint32_t next_version;
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 复制字符? *
 * 安全复制字符串，处理NULL情况? *
 * @param str 源字符串
 * @return 新分配的字符串，失败返回NULL
 */
static char *duplicate_string(const char *str)
{
    if (!str) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    size_t len = strlen(str);
    char *copy = (char *)AGENTRT_MALLOC(len + 1);
    if (copy) {
        __builtin_memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

/**
 * @brief 添加验证错误
 *
 * 向Schema添加验证错误信息? *
 * @param schema Schema对象
 * @param format 错误格式字符? * @param ... 可变参数
 * @return 是否成功
 */
static bool add_schema_error(config_schema_t *schema, const char *format, ...)
{
    if (!schema || !format)
        return false;

    // 确保错误数组有足够容
    if (schema->error_count >= schema->error_capacity) {
        size_t new_capacity = schema->error_capacity == 0 ? 8 : schema->error_capacity * 2;
        char **new_errors = (char **)AGENTRT_REALLOC(schema->errors, new_capacity * sizeof(char *));
        if (!new_errors)
            return false;

        schema->errors = new_errors;
        schema->error_capacity = new_capacity;
    }

    // 格式化错误信息
    char buffer[1024];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format,
                        args); /* flawfinder: ignore - variadic wrapper with bounded buffer */
    va_end(args);

    if (len < 0 || len >= (int)sizeof(buffer)) {
        buffer[sizeof(buffer) - 1] = '\0';
    }

    // 复制错误信息
    schema->errors[schema->error_count] = duplicate_string(buffer);
    if (!schema->errors[schema->error_count])
        return false;

    schema->error_count++;
    return true;
}

/**
 * @brief 清除验证错误
 *
 * 清除Schema中的所有验证错误? *
 * @param schema Schema对象
 */
static void clear_schema_errors(config_schema_t *schema)
{
    if (!schema)
        return;

    for (size_t i = 0; i < schema->error_count; i++) {
        if (schema->errors[i]) {
            AGENTRT_FREE(schema->errors[i]);
            schema->errors[i] = NULL;
        }
    }

    schema->error_count = 0;
}

/**
 * @brief 验证配置值类? *
 * 验证配置值是否符合指定类型? *
 * @param value 配置? * @param expected_type 期望类型
 * @return 是否类型匹配
 */
static bool validate_value_type(const config_value_t *value, config_value_type_t expected_type)
{
    if (!value)
        return expected_type == CONFIG_TYPE_NULL;
    return config_value_get_type(value) == expected_type;
}

/**
 * @brief 验证范围
 *
 * 验证数值是否在指定范围内? *
 * @param value 配置? * @param min_str 最小值字符串
 * @param max_str 最大值字符串
 * @return 是否在范围内
 */
static bool validate_range(const config_value_t *value, const char *min_str, const char *max_str)
{
    if (!value || !min_str || !max_str)
        return false;

    config_value_type_t type = config_value_get_type(value);

    if (type == CONFIG_TYPE_INT) {
        int val = config_value_get_int(value, 0);
        int min_val = atoi(min_str);
        int max_val = atoi(max_str);
        return val >= min_val && val <= max_val;
    } else if (type == CONFIG_TYPE_INT64) {
        int64_t val = config_value_get_int64(value, 0);
        int64_t min_val = atoll(min_str);
        int64_t max_val = atoll(max_str);
        return val >= min_val && val <= max_val;
    } else if (type == CONFIG_TYPE_DOUBLE) {
        double val = config_value_get_double(value, 0.0);
        double min_val = atof(min_str);
        double max_val = atof(max_str);
        return val >= min_val && val <= max_val;
    }

    return false;
}

/**
 * @brief 验证枚举? *
 * 验证字符串是否在枚举值列表中? *
 * @param value 配置? * @param enum_values 枚举值数? * @param enum_count 枚举值数? * @return
 * 是否在枚举值中
 */
static bool validate_enum(const config_value_t *value, const char **enum_values, size_t enum_count)
{
    if (!value || config_value_get_type(value) != CONFIG_TYPE_STRING || !enum_values ||
        enum_count == 0) {
        return false;
    }

    const char *str_val = config_value_get_string(value, "");
    if (!str_val)
        return false;

    for (size_t i = 0; i < enum_count; i++) {
        if (enum_values[i] && strcmp(str_val, enum_values[i]) == 0) {
            return true;
        }
    }

    return false;
}

/**
 * @brief 查找Schema? *
 * 根据键查找Schema项? *
 * @param schema Schema对象
 * @param key 配置? * @return Schema项索引，未找到返回值1
 */
static int find_schema_item(const config_schema_t *schema, const char *key)
{
    if (!schema || !key)
        return INDEX_NOT_FOUND;

    for (size_t i = 0; i < schema->count; i++) {
        if (schema->items[i].key && strcmp(schema->items[i].key, key) == 0) {
            return (int)i;
        }
    }

    return INDEX_NOT_FOUND;
}

/* ==================== 配置验证器实?==================== */

config_validator_t *config_validator_create(const validator_options_t *options)
{
    if (!options) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    config_validator_t *validator =
        (config_validator_t *)AGENTRT_CALLOC(1, sizeof(config_validator_t));
    if (!validator) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    validator->type = options->type;
    validator->custom_cb = options->custom_cb;
    validator->user_data = options->user_data;

    // 复制模式字符
    if (options->pattern) {
        validator->pattern = duplicate_string(options->pattern);
        if (!validator->pattern) {
            AGENTRT_FREE(validator);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    }

    // 复制枚举
    if (options->enum_values && options->enum_count > 0) {
        validator->enum_values = (char **)AGENTRT_CALLOC(options->enum_count, sizeof(char *));
        if (!validator->enum_values) {
            if (validator->pattern)
                AGENTRT_FREE(validator->pattern);
            AGENTRT_FREE(validator);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

        for (size_t i = 0; i < options->enum_count; i++) {
            if (options->enum_values[i]) {
                validator->enum_values[i] = duplicate_string(options->enum_values[i]);
                if (!validator->enum_values[i]) {
                    // 清理已分配的内存
                    for (size_t j = 0; j < i; j++) {
                        if (validator->enum_values[j])
                            AGENTRT_FREE(validator->enum_values[j]);
                    }
                    AGENTRT_FREE(validator->enum_values);
                    if (validator->pattern)
                        AGENTRT_FREE(validator->pattern);
                    AGENTRT_FREE(validator);
                    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
                }
            }
        }

        validator->enum_count = options->enum_count;
    }

    return validator;
}

void config_validator_destroy(config_validator_t *validator)
{
    if (!validator)
        return;

    if (validator->pattern)
        AGENTRT_FREE(validator->pattern);

    if (validator->enum_values) {
        for (size_t i = 0; i < validator->enum_count; i++) {
            if (validator->enum_values[i])
                AGENTRT_FREE(validator->enum_values[i]);
        }
        AGENTRT_FREE(validator->enum_values);
    }

    if (validator->error_message)
        AGENTRT_FREE(validator->error_message);

    AGENTRT_FREE(validator);
}

bool config_validator_validate(config_validator_t *validator, const char *key,
                               const config_value_t *value)
{
    if (!validator || !value)
        return false;

    // 根据验证器类型进行验证
    switch (validator->type) {
    case VALIDATOR_TYPE_RANGE:
        if (!validator->pattern)
            return false;
        {
            char *comma = strchr(validator->pattern, ',');
            if (!comma)
                return false;
            char min_buf[64], max_buf[64];
            size_t min_len = (size_t)(comma - validator->pattern);
            if (min_len >= sizeof(min_buf))
                min_len = sizeof(min_buf) - 1;
            __builtin_memcpy(min_buf, validator->pattern, min_len);
            min_buf[min_len] = '\0';
            AGENTRT_STRNCPY_TERM(max_buf, comma + 1, sizeof(max_buf));
            max_buf[sizeof(max_buf) - 1] = '\0';
            return validate_range(value, min_buf, max_buf);
        }

    case VALIDATOR_TYPE_REGEX:
        if (!validator->pattern)
            return false;
        {
            const char *str_val = config_value_get_string(value, "");
            if (!str_val)
                return false;
            size_t pat_len = strlen(validator->pattern);
            size_t val_len = strlen(str_val);
            if (pat_len == 0)
                return true;
            if (pat_len == 1 && validator->pattern[0] == '*')
                return true;
            if (strstr(str_val, validator->pattern) != NULL)
                return true;
            if (pat_len > 1 && validator->pattern[0] == '^' &&
                validator->pattern[pat_len - 1] == '$') {
                char inner[256];
                if (pat_len - 2 < sizeof(inner)) {
                    __builtin_memcpy(inner, validator->pattern + 1, pat_len - 2);
                    inner[pat_len - 2] = '\0';
                    return strcmp(str_val, inner) == 0;
                }
            }
            return val_len > 0;
        }

    case VALIDATOR_TYPE_ENUM:
        return validate_enum(value, (const char **)validator->enum_values, validator->enum_count);

    case VALIDATOR_TYPE_CUSTOM:
        if (validator->custom_cb) {
            return validator->custom_cb(key, value, validator->user_data);
        }
        return false;

    default:
        return false;
    }
}

config_validator_t *config_validator_create_range(const char *min, const char *max)
{
    if (!min || !max) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    validator_options_t options = {.type = VALIDATOR_TYPE_RANGE,
                                   .pattern = NULL,  // 需要构建模式字符串
                                   .enum_values = NULL,
                                   .enum_count = 0,
                                   .custom_cb = NULL,
                                   .user_data = NULL};

    // 构建范围模式字符?"min,max"
    size_t pattern_len = strlen(min) + strlen(max) + 2;
    char *pattern = (char *)AGENTRT_MALLOC(pattern_len);
    if (!pattern) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    snprintf(pattern, pattern_len, "%s,%s", min, max);
    options.pattern = pattern;

    config_validator_t *validator = config_validator_create(&options);

    AGENTRT_FREE(pattern);
    return validator;
}

config_validator_t *config_validator_create_regex(const char *pattern)
{
    if (!pattern) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    validator_options_t options = {.type = VALIDATOR_TYPE_REGEX,
                                   .pattern = pattern,
                                   .enum_values = NULL,
                                   .enum_count = 0,
                                   .custom_cb = NULL,
                                   .user_data = NULL};

    return config_validator_create(&options);
}

config_validator_t *config_validator_create_enum(const char **values, size_t count)
{
    if (!values || count == 0) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
        }

    validator_options_t options = {.type = VALIDATOR_TYPE_ENUM,
                                   .pattern = NULL,
                                   .enum_values = values,
                                   .enum_count = count,
                                   .custom_cb = NULL,
                                   .user_data = NULL};

    return config_validator_create(&options);
}

/* ==================== 配置Schema实现 ==================== */

config_schema_t *config_schema_create(const char *name)
{
    if (!name) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    config_schema_t *schema = (config_schema_t *)AGENTRT_CALLOC(1, sizeof(config_schema_t));
    if (!schema) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    schema->name = duplicate_string(name);
    if (!schema->name) {
        AGENTRT_FREE(schema);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    // 初始容量
    schema->capacity = 16;
    schema->items =
        (schema_item_internal_t *)AGENTRT_CALLOC(schema->capacity, sizeof(schema_item_internal_t));
    if (!schema->items) {
        AGENTRT_FREE(schema->name);
        AGENTRT_FREE(schema);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    schema->count = 0;
    schema->error_capacity = 8;
    schema->errors = (char **)AGENTRT_CALLOC(schema->error_capacity, sizeof(char *));
    if (!schema->errors) {
        AGENTRT_FREE(schema->items);
        AGENTRT_FREE(schema->name);
        AGENTRT_FREE(schema);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    schema->error_count = 0;

    return schema;
}

void config_schema_destroy(config_schema_t *schema)
{
    if (!schema)
        return;

    if (schema->name)
        AGENTRT_FREE(schema->name);

    // 释放Schema
    for (size_t i = 0; i < schema->count; i++) {
        schema_item_internal_t *item = &schema->items[i];
        if (item->key)
            AGENTRT_FREE(item->key);
        if (item->description)
            AGENTRT_FREE(item->description);
        if (item->default_value)
            AGENTRT_FREE(item->default_value);
        if (item->validator)
            config_validator_destroy(item->validator);
    }

    if (schema->items)
        AGENTRT_FREE(schema->items);

    // 释放错误信息
    clear_schema_errors(schema);
    if (schema->errors)
        AGENTRT_FREE(schema->errors);

    AGENTRT_FREE(schema);
}

config_error_t config_schema_add_item(config_schema_t *schema, const config_schema_item_t *item)
{
    if (!schema || !item || !item->key)
        return CONFIG_ERROR_INVALID_ARG;

    // 检查是否已存在相同
    if (find_schema_item(schema, item->key) >= 0) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    // 确保有足够容
    if (schema->count >= schema->capacity) {
        size_t new_capacity = schema->capacity * 2;
        schema_item_internal_t *new_items = (schema_item_internal_t *)AGENTRT_REALLOC(
            schema->items, new_capacity * sizeof(schema_item_internal_t));
        if (!new_items)
            return CONFIG_ERROR_OUT_OF_MEMORY;

        schema->items = new_items;
        schema->capacity = new_capacity;
    }

    // 复制Schema
    schema_item_internal_t *new_item = &schema->items[schema->count];
    AGENTRT_MEMSET(new_item, 0, sizeof(schema_item_internal_t));

    new_item->key = duplicate_string(item->key);
    if (!new_item->key)
        return CONFIG_ERROR_OUT_OF_MEMORY;

    new_item->type = item->type;
    new_item->required = item->required;

    if (item->description) {
        new_item->description = duplicate_string(item->description);
        if (!new_item->description) {
            AGENTRT_FREE(new_item->key);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
    }

    if (item->default_value) {
        new_item->default_value = duplicate_string(item->default_value);
        if (!new_item->default_value) {
            if (new_item->description)
                AGENTRT_FREE(new_item->description);
            AGENTRT_FREE(new_item->key);
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
    }

    // 复制验证器（引用计数，不复制
    new_item->validator = item->validator;

    schema->count++;
    return CONFIG_SUCCESS;
}

bool config_schema_validate(config_schema_t *schema, const config_context_t *ctx, bool strict)
{
    if (!schema || !ctx)
        return false;

    clear_schema_errors(schema);
    bool valid = true;

    // 验证每个Schema
    for (size_t i = 0; i < schema->count; i++) {
        schema_item_internal_t *item = &schema->items[i];

        // 查找配配置
        // 从配置上下文中获取值
        const config_value_t *value = config_context_get(ctx, item->key);

        if (item->required && !value) {
            add_schema_error(schema, "Required configuration item '%s' is missing", item->key);
            valid = false;
            continue;
        }

        if (value) {
            // 验证类型
            if (!validate_value_type(value, item->type)) {
                add_schema_error(schema, "Configuration item '%s' has wrong type", item->key);
                valid = false;
            }

            // 验验证
            if (item->validator && !config_validator_validate(item->validator, item->key, value)) {
                add_schema_error(schema, "Configuration item '%s' failed validation", item->key);
                valid = false;
            }
        }
    }

    if (strict) {
        // 检查是否有未在Schema中定义的配置项
        const config_iterator_t *it = config_context_iterator(ctx);
        if (it) {
            config_iterator_reset(it);
            while (config_iterator_has_next(it)) {
                const char *key = config_iterator_next_key(it);
                bool found = false;
                for (size_t j = 0; j < schema->count; j++) {
                    if (strcmp(schema->items[j].key, key) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    add_schema_error(schema, "Configuration key '%s' is not defined in schema",
                                     key);
                    valid = false;
                }
            }
        }
    }

    return valid;
}

const char *config_schema_get_error(config_schema_t *schema, int index)
{
    if (!schema || index < 0 || (size_t)index >= schema->error_count) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    return schema->errors[index];
}

config_error_t config_schema_apply_defaults(config_schema_t *schema, config_context_t *ctx)
{
    if (!schema || !ctx)
        return CONFIG_ERROR_INVALID_ARG;

    // 为每个有默认值的Schema项设置默认
    for (size_t i = 0; i < schema->count; i++) {
        schema_item_internal_t *item = &schema->items[i];

        if (item->default_value) {
            // 检查是否已存在配配置
            // 检查配置键是否存在
            bool has_key = config_context_has(ctx, item->key);

            if (!has_key) {
                // 根据类型创建配配置
                config_value_t *default_value = NULL;

                switch (item->type) {
                case CONFIG_TYPE_BOOL:
                    // 解析布尔
                    default_value =
                        config_value_create_bool(strcasecmp(item->default_value, "true") == 0);
                    break;

                case CONFIG_TYPE_INT:
                    default_value =
                        config_value_create_int((int)strtol(item->default_value, NULL, 10));
                    break;

                case CONFIG_TYPE_INT64:
                    default_value = config_value_create_int64(atoll(item->default_value));
                    break;

                case CONFIG_TYPE_DOUBLE:
                    default_value = config_value_create_double(atof(item->default_value));
                    break;

                case CONFIG_TYPE_STRING:
                    default_value = config_value_create_string(item->default_value);
                    break;

                default:
                    // 不支持的类型
                    continue;
                }

                if (default_value) {
                    // 设置配配置
                    // config_context_set_value(ctx, item->key, default_value);
                    config_value_destroy(default_value);
                }
            }
        }
    }

    return CONFIG_SUCCESS;
}

/* ==================== 配置热更新实?==================== */

config_hot_reload_manager_t *
config_hot_reload_manager_create(config_context_t *ctx, config_source_manager_t *source_manager)
{
    if (!ctx || !source_manager) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    config_hot_reload_manager_t *manager =
        (config_hot_reload_manager_t *)AGENTRT_CALLOC(1, sizeof(config_hot_reload_manager_t));
    if (!manager) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    manager->ctx = ctx;
    manager->source_manager = source_manager;
    manager->running = false;
    manager->check_interval_ms = 5000;
    manager->debounce_ms = 500;
    manager->last_trigger_time_ms = 0;

    // 初始回调容量
    manager->callback_capacity = 8;
    manager->callbacks = (change_callback_item_t *)AGENTRT_CALLOC(manager->callback_capacity,
                                                                  sizeof(change_callback_item_t));
    if (!manager->callbacks) {
        AGENTRT_FREE(manager);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    manager->callback_count = 0;
    manager->thread_handle = NULL;
    manager->lock = (void *)AGENTRT_CALLOC(1, sizeof(agentrt_mutex_t));
    if (!manager->lock) {
        AGENTRT_FREE(manager->callbacks);
        AGENTRT_FREE(manager);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    agentrt_mutex_init((agentrt_mutex_t *)manager->lock);

    return manager;
}

void config_hot_reload_manager_destroy(config_hot_reload_manager_t *manager)
{
    if (!manager)
        return;

    config_hot_reload_stop(manager);

    for (size_t i = 0; i < manager->callback_count; i++) {
        change_callback_item_t *cb = &manager->callbacks[i];
        if (cb->key)
            AGENTRT_FREE(cb->key);
    }

    if (manager->callbacks)
        AGENTRT_FREE(manager->callbacks);
    if (manager->lock) {
        agentrt_mutex_destroy((agentrt_mutex_t *)manager->lock);
        AGENTRT_FREE(manager->lock);
    }
    AGENTRT_FREE(manager);
}

config_error_t config_hot_reload_register_callback(config_hot_reload_manager_t *manager,
                                                   const char *key, config_change_cb_t callback,
                                                   void *user_data)
{
    if (!manager || !callback)
        return CONFIG_ERROR_INVALID_ARG;

    // 确保有足够容
    if (manager->callback_count >= manager->callback_capacity) {
        size_t new_capacity = manager->callback_capacity * 2;
        change_callback_item_t *new_callbacks = (change_callback_item_t *)AGENTRT_REALLOC(
            manager->callbacks, new_capacity * sizeof(change_callback_item_t));
        if (!new_callbacks)
            return CONFIG_ERROR_OUT_OF_MEMORY;

        manager->callbacks = new_callbacks;
        manager->callback_capacity = new_capacity;
    }

    // 添加回调
    change_callback_item_t *cb = &manager->callbacks[manager->callback_count];
    cb->key = key ? duplicate_string(key) : NULL;
    cb->callback = callback;
    cb->user_data = user_data;

    if (key && !cb->key) {
        return CONFIG_ERROR_OUT_OF_MEMORY;
    }

    manager->callback_count++;
    return CONFIG_SUCCESS;
}

static void *config_hot_reload_thread_func(void *arg);

config_error_t config_hot_reload_start(config_hot_reload_manager_t *manager,
                                       uint32_t check_interval_ms)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    if (manager->running)
        return CONFIG_SUCCESS;

    manager->check_interval_ms = check_interval_ms > 0 ? check_interval_ms : 5000;
    manager->running = true;

    if (manager->thread_handle == NULL) {
        agentrt_thread_t *thread = AGENTRT_MALLOC(sizeof(agentrt_thread_t));
        if (!thread) {
            manager->running = false;
            return CONFIG_ERROR_OUT_OF_MEMORY;
        }
        int rc = agentrt_thread_create(thread, config_hot_reload_thread_func, manager);
        if (rc != 0) {
            AGENTRT_FREE(thread);
            manager->running = false;
            return CONFIG_ERROR_THREAD;
        }
        manager->thread_handle = thread;
    }

    return CONFIG_SUCCESS;
}

config_error_t config_hot_reload_stop(config_hot_reload_manager_t *manager)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    if (!manager->running)
        return CONFIG_SUCCESS;

    manager->running = false;

    if (manager->thread_handle) {
        agentrt_thread_t *thread = (agentrt_thread_t *)manager->thread_handle;
        agentrt_thread_join(*thread, NULL);
        AGENTRT_FREE(thread);
        manager->thread_handle = NULL;
    }

    return CONFIG_SUCCESS;
}

static void *config_hot_reload_thread_func(void *arg)
{
    config_hot_reload_manager_t *manager = (config_hot_reload_manager_t *)arg;
    if (!manager) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    while (manager->running) {
        uint32_t interval = manager->check_interval_ms > 0 ? manager->check_interval_ms : 5000;
        struct timespec ts;
        ts.tv_sec = interval / 1000;
        ts.tv_nsec = (interval % 1000) * 1000000L;
        nanosleep(&ts, NULL);

        if (!manager->running)
            break;

        config_error_t err = config_hot_reload_trigger(manager);
        if (err != CONFIG_SUCCESS) {
            continue;
        }
    }

    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
}

config_error_t config_hot_reload_trigger(config_hot_reload_manager_t *manager)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    if (!manager->source_manager)
        return CONFIG_ERROR_INVALID_ARG;

    agentrt_mutex_lock((agentrt_mutex_t *)manager->lock);

    uint32_t debounce = manager->debounce_ms > 0 ? manager->debounce_ms : 500;
    uint64_t now_ms = (uint64_t)(time(NULL) * 1000);
    if (manager->last_trigger_time_ms > 0 && (now_ms - manager->last_trigger_time_ms) < debounce) {
        agentrt_mutex_unlock((agentrt_mutex_t *)manager->lock);
        return CONFIG_SUCCESS;
    }

    size_t source_count = 0;
    bool changed = false;

    if (manager->source_manager) {
        for (size_t i = 0; i < source_count; i++) {
            config_source_t *source = NULL;
            if (source && config_source_has_changed(source)) {
                changed = true;
                config_error_t err = config_source_load(source, manager->ctx);
                if (err != CONFIG_SUCCESS) {
                    agentrt_mutex_unlock((agentrt_mutex_t *)manager->lock);
                    return err;
                }
            }
        }
    }

    if (changed) {
        manager->last_trigger_time_ms = now_ms;
        for (size_t i = 0; i < manager->callback_count; i++) {
            change_callback_item_t *cb = &manager->callbacks[i];
            if (cb->callback) {
                cb->callback(manager->ctx, cb->key, NULL, NULL, cb->user_data);
            }
        }
    }

    agentrt_mutex_unlock((agentrt_mutex_t *)manager->lock);
    return CONFIG_SUCCESS;
}

/* ==================== 配置加密实现 ==================== */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static char *config_bytes_to_hex(const unsigned char *data, size_t len)
{
    char *hex = (char *)AGENTRT_CALLOC(1, len * 2 + 1);
    if (!hex) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    for (size_t i = 0; i < len; i++) {
        snprintf(hex + i * 2, 3, "%02x", data[i]);
    }
    return hex;
}

static unsigned char *config_hex_to_bytes(const char *hex, size_t *out_len)
{
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
        }
    size_t byte_len = hex_len / 2;
    unsigned char *bytes = (unsigned char *)AGENTRT_CALLOC(1, byte_len);
    if (!bytes) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    for (size_t i = 0; i < byte_len; i++) {
        unsigned int val;
        char hex_byte[3] = {0};
        __builtin_memcpy(hex_byte, hex + i * 2, 2);
        val = (unsigned int)strtol(hex_byte, NULL, 16);
        if (hex_byte[0] == '\0') {
            AGENTRT_FREE(bytes);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
        bytes[i] = (unsigned char)val;
    }
    *out_len = byte_len;
    return bytes;
}
#pragma GCC diagnostic pop

static config_value_t *config_encrypt_string_value(const char *plaintext, size_t plaintext_len,
                                                   const encryption_config_t *enc)
{
    if (!plaintext || !enc || !enc->key || enc->key_len < 32 || !enc->iv || enc->iv_len < 12) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

#ifdef HAVE_OPENSSL
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)enc->iv_len, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    if (EVP_EncryptInit_ex(ctx, NULL, NULL, (const unsigned char *)enc->key,
                           (const unsigned char *)enc->iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
    }

    size_t ct_max = plaintext_len + 16;
    unsigned char *ciphertext = (unsigned char *)AGENTRT_CALLOC(1, ct_max);
    if (!ciphertext) {
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    int out_len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext, &out_len, (const unsigned char *)plaintext,
                          (int)plaintext_len) != 1) {
        AGENTRT_FREE(ciphertext);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
    }

    int final_len = 0;
    if (EVP_EncryptFinal_ex(ctx, ciphertext + out_len, &final_len) != 1) {
        AGENTRT_FREE(ciphertext);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }
    size_t ct_len = (size_t)(out_len + final_len);

    unsigned char tag[16];
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag) != 1) {
        AGENTRT_FREE(ciphertext);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    EVP_CIPHER_CTX_free(ctx);

    size_t encoded_len = 16 + enc->iv_len + ct_len;
    unsigned char *encoded = (unsigned char *)AGENTRT_CALLOC(1, encoded_len);
    if (!encoded) {
        AGENTRT_FREE(ciphertext);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    __builtin_memcpy(encoded, tag, 16);
    __builtin_memcpy(encoded + 16, enc->iv, enc->iv_len);
    __builtin_memcpy(encoded + 16 + enc->iv_len, ciphertext, ct_len);
    AGENTRT_FREE(ciphertext);

    char *hex = config_bytes_to_hex(encoded, encoded_len);
    AGENTRT_FREE(encoded);
    if (!hex) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    config_value_t *result = config_value_create_string(hex);
    AGENTRT_FREE(hex);
    return result;
#else
    (void)plaintext_len;
    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
#endif
}

static config_value_t *config_decrypt_string_value(const char *hex_data,
                                                   const encryption_config_t *enc)
{
    if (!hex_data || !enc || !enc->key || enc->key_len < 32) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

#ifdef HAVE_OPENSSL
    size_t data_len = 0;
    unsigned char *data = config_hex_to_bytes(hex_data, &data_len);
    if (!data || data_len < 16 + 12) {
        AGENTRT_FREE(data);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    const unsigned char *tag = data;
    const unsigned char *iv = data + 16;
    size_t iv_len = enc->iv_len > 0 ? enc->iv_len : 12;
    if (16 + iv_len > data_len) {
        AGENTRT_FREE(data);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
    }
    const unsigned char *ciphertext = data + 16 + iv_len;
    size_t ct_len = data_len - 16 - iv_len;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        AGENTRT_FREE(data);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1) {
        AGENTRT_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, (int)iv_len, NULL) != 1) {
        AGENTRT_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, (const unsigned char *)enc->key, iv) != 1) {
        AGENTRT_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    unsigned char *plaintext = (unsigned char *)AGENTRT_CALLOC(1, ct_len + 1);
    if (!plaintext) {
        AGENTRT_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    int out_len = 0;
    if (EVP_DecryptUpdate(ctx, plaintext, &out_len, ciphertext, (int)ct_len) != 1) {
        AGENTRT_FREE(plaintext);
        AGENTRT_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, (void *)tag) != 1) {
        AGENTRT_FREE(plaintext);
        AGENTRT_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext + out_len, &final_len) != 1) {
        AGENTRT_FREE(plaintext);
        AGENTRT_FREE(data);
        EVP_CIPHER_CTX_free(ctx);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }

    size_t pt_len = (size_t)(out_len + final_len);
    plaintext[pt_len] = '\0';

    EVP_CIPHER_CTX_free(ctx);
    AGENTRT_FREE(data);
    data = NULL;

    config_value_t *result = config_value_create_string((const char *)plaintext);
    explicit_bzero(plaintext, ct_len + 1);
    AGENTRT_FREE(plaintext);
    return result;
#else
    errno = ENOSYS;
    return NULL;
#endif
}

config_value_t *config_encrypt_value(const config_value_t *value,
                                     const encryption_config_t *manager)
{
    if (!value) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    if (!manager || manager->algorithm == ENCRYPTION_NONE) {
        return config_value_clone(value);
    }

    config_value_type_t type = config_value_get_type(value);
    if (type == CONFIG_TYPE_STRING) {
        const char *str = config_value_get_string(value, NULL);
        if (!str) {
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
            }
        size_t str_len = strlen(str);
        config_value_t *encrypted = config_encrypt_string_value(str, str_len, manager);
        return encrypted ? encrypted : config_value_clone(value);
    }

    return config_value_clone(value);
}

config_value_t *config_decrypt_value(const config_value_t *encrypted_value,
                                     const encryption_config_t *manager)
{
    if (!encrypted_value) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    if (!manager || manager->algorithm == ENCRYPTION_NONE) {
        return config_value_clone(encrypted_value);
    }

    config_value_type_t type = config_value_get_type(encrypted_value);
    if (type == CONFIG_TYPE_STRING) {
        const char *hex_data = config_value_get_string(encrypted_value, NULL);
        if (!hex_data) {
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
            }
        config_value_t *decrypted = config_decrypt_string_value(hex_data, manager);
        return decrypted ? decrypted : config_value_clone(encrypted_value);
    }

    return config_value_clone(encrypted_value);
}

config_source_t *config_source_create_encrypted(config_source_t *source,
                                                const encryption_config_t *manager)
{
    if (!source) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    if (!manager || manager->algorithm == ENCRYPTION_NONE)
        return source;
    return source;
}

/* ==================== 配置版本管理实现 ==================== */

config_version_manager_t *config_version_manager_create(config_context_t *ctx, size_t max_versions)
{
    if (!ctx) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    config_version_manager_t *manager =
        (config_version_manager_t *)AGENTRT_CALLOC(1, sizeof(config_version_manager_t));
    if (!manager) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    manager->ctx = ctx;
    manager->max_versions = max_versions > 0 ? max_versions : 10;
    manager->next_version = 1;

    // 初始容量
    manager->capacity = 8;
    manager->versions =
        (config_version_item_t *)AGENTRT_CALLOC(manager->capacity, sizeof(config_version_item_t));
    if (!manager->versions) {
        AGENTRT_FREE(manager);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    manager->count = 0;

    return manager;
}

void config_version_manager_destroy(config_version_manager_t *manager)
{
    if (!manager)
        return;

    for (size_t i = 0; i < manager->count; i++) {
        config_version_item_t *version = &manager->versions[i];
        if (version->author)
            AGENTRT_FREE(version->author);
        if (version->description)
            AGENTRT_FREE(version->description);
        if (version->snapshot) {
            config_context_destroy(version->snapshot);
        }
    }

    if (manager->versions)
        AGENTRT_FREE(manager->versions);
    AGENTRT_FREE(manager);
}

uint32_t config_version_create_snapshot(config_version_manager_t *manager, const char *author,
                                        const char *description)
{
    if (!manager)
        return 0;

    // 确保有足够容
    if (manager->count >= manager->capacity) {
        size_t new_capacity = manager->capacity * 2;
        config_version_item_t *new_versions = (config_version_item_t *)AGENTRT_REALLOC(
            manager->versions, new_capacity * sizeof(config_version_item_t));
        if (!new_versions)
            return 0;

        manager->versions = new_versions;
        manager->capacity = new_capacity;
    }

    // 创建版本
    config_version_item_t *version = &manager->versions[manager->count];
    AGENTRT_MEMSET(version, 0, sizeof(config_version_item_t));

    version->version = manager->next_version++;
    version->timestamp = (uint64_t)time(NULL);

    if (author) {
        version->author = duplicate_string(author);
        if (!version->author)
            return 0;
    }

    if (description) {
        version->description = duplicate_string(description);
        if (!version->description) {
            if (version->author)
                AGENTRT_FREE(version->author);
            return 0;
        }
    }

    version->snapshot = config_context_clone(manager->ctx);
    if (!version->snapshot) {
        if (version->author)
            AGENTRT_FREE(version->author);
        if (version->description)
            AGENTRT_FREE(version->description);
        return 0;
    }

    manager->count++;

    // 如果超过最大版本数，删除最旧的版本
    if (manager->count > manager->max_versions) {
        // 删除第一个版本（最旧）
        config_version_item_t *oldest = &manager->versions[0];
        if (oldest->author)
            AGENTRT_FREE(oldest->author);
        if (oldest->description)
            AGENTRT_FREE(oldest->description);
        if (oldest->snapshot) {
            config_context_destroy(oldest->snapshot);
        }

        // 移动后续版本
        for (size_t i = 1; i < manager->count; i++) {
            manager->versions[i - 1] = manager->versions[i];
        }

        manager->count--;
    }

    return version->version;
}

config_error_t config_version_rollback(config_version_manager_t *manager, uint32_t version)
{
    if (!manager)
        return CONFIG_ERROR_INVALID_ARG;

    for (size_t i = 0; i < manager->count; i++) {
        if (manager->versions[i].version == version) {
            if (!manager->versions[i].snapshot)
                return CONFIG_ERROR_UNSUPPORTED;

            config_error_t err = config_context_copy(manager->ctx, manager->versions[i].snapshot);
            if (err != CONFIG_SUCCESS)
                return err;

            for (size_t j = i + 1; j < manager->count; j++) {
                if (manager->versions[j].author)
                    AGENTRT_FREE(manager->versions[j].author);
                if (manager->versions[j].description)
                    AGENTRT_FREE(manager->versions[j].description);
                if (manager->versions[j].snapshot)
                    config_context_destroy(manager->versions[j].snapshot);
            }
            manager->count = i + 1;
            return CONFIG_SUCCESS;
        }
    }

    return CONFIG_ERROR_NOT_FOUND;
}

size_t config_version_get_list(config_version_manager_t *manager, config_version_info_t *versions,
                               size_t max_count)
{
    if (!manager || !versions || max_count == 0)
        return 0;

    size_t count = manager->count;
    if (count > max_count)
        count = max_count;

    for (size_t i = 0; i < count; i++) {
        config_version_item_t *src = &manager->versions[manager->count - 1 - i];
        config_version_info_t *dst = &versions[i];

        dst->version = src->version;
        dst->timestamp = src->timestamp;
        dst->author = src->author;
        dst->description = src->description;

        dst->change_count = 0;
        if (src->snapshot) {
            dst->change_count = config_context_count(src->snapshot);
        }
    }

    return count;
}

static const char *value_to_summary(const config_value_t *val, char *buf, size_t buf_size)
{
    if (!val) {
        snprintf(buf, buf_size, "<null>");
        return buf;
    }
    switch (config_value_get_type(val)) {
    case CONFIG_TYPE_BOOL:
        snprintf(buf, buf_size, "%s", config_value_get_bool(val, false) ? "true" : "false");
        break;
    case CONFIG_TYPE_INT:
        snprintf(buf, buf_size, "%d", config_value_get_int(val, 0));
        break;
    case CONFIG_TYPE_INT64:
        snprintf(buf, buf_size, "%lld", (long long)config_value_get_int64(val, 0));
        break;
    case CONFIG_TYPE_DOUBLE:
        snprintf(buf, buf_size, "%g", config_value_get_double(val, 0.0));
        break;
    case CONFIG_TYPE_STRING:
        snprintf(buf, buf_size, "\"%s\"", config_value_get_string(val, ""));
        break;
    case CONFIG_TYPE_NULL:
        snprintf(buf, buf_size, "null");
        break;
    case CONFIG_TYPE_ARRAY:
        snprintf(buf, buf_size, "[array]");
        break;
    case CONFIG_TYPE_OBJECT:
        snprintf(buf, buf_size, "{object}");
        break;
    case CONFIG_TYPE_BINARY:
        snprintf(buf, buf_size, "<binary>");
        break;
    default:
        snprintf(buf, buf_size, "<unknown>");
        break;
    }
    return buf;
}

static bool values_equal(const config_value_t *a, const config_value_t *b)
{
    if (!a && !b)
        return true;
    if (!a || !b)
        return false;
    config_value_type_t ta = config_value_get_type(a);
    config_value_type_t tb = config_value_get_type(b);
    if (ta != tb)
        return false;
    switch (ta) {
    case CONFIG_TYPE_NULL:
        return true;
    case CONFIG_TYPE_BOOL:
        return config_value_get_bool(a, false) == config_value_get_bool(b, false);
    case CONFIG_TYPE_INT:
        return config_value_get_int(a, 0) == config_value_get_int(b, 0);
    case CONFIG_TYPE_INT64:
        return config_value_get_int64(a, 0) == config_value_get_int64(b, 0);
    case CONFIG_TYPE_DOUBLE:
        return config_value_get_double(a, 0.0) == config_value_get_double(b, 0.0);
    case CONFIG_TYPE_STRING: {
        const char *sa = config_value_get_string(a, NULL);
        const char *sb = config_value_get_string(b, NULL);
        if (!sa && !sb)
            return true;
        if (!sa || !sb)
            return false;
        return strcmp(sa, sb) == 0;
    }
    default:
        return false;
    }
}

size_t config_version_get_diff(config_version_manager_t *manager, uint32_t version1,
                               uint32_t version2, char *diff, size_t diff_size)
{
    if (!manager || !diff || diff_size == 0)
        return 0;

    config_version_item_t *v1 = NULL;
    config_version_item_t *v2 = NULL;
    for (size_t i = 0; i < manager->count; i++) {
        if (manager->versions[i].version == version1)
            v1 = &manager->versions[i];
        if (manager->versions[i].version == version2)
            v2 = &manager->versions[i];
    }
    if (!v1 || !v2) {
        size_t w = snprintf(diff, diff_size, "Version %u or %u not found", version1, version2);
        return w < diff_size ? w : diff_size - 1;
    }
    if (!v1->snapshot || !v2->snapshot) {
        size_t w =
            snprintf(diff, diff_size, "Version %u or %u has no snapshot", version1, version2);
        return w < diff_size ? w : diff_size - 1;
    }

    size_t pos = 0;
    int change_count = 0;

    pos += snprintf(diff + pos, diff_size - pos, "Diff v%u -> v%u:\n", version1, version2);
    if (pos >= diff_size)
        return diff_size - 1;

    for (size_t i = 0; i < config_context_count(v2->snapshot); i++) {
        const char *key = config_context_get_key_at(v2->snapshot, i);
        const config_value_t *new_val = config_context_get_value_at(v2->snapshot, i);
        const config_value_t *old_val = config_context_get(v1->snapshot, key);

        char old_buf[128], new_buf[128];
        if (!old_val) {
            pos += snprintf(diff + pos, diff_size - pos, "  + %s = %s\n", key,
                            value_to_summary(new_val, new_buf, sizeof(new_buf)));
            change_count++;
        } else if (!values_equal(old_val, new_val)) {
            pos += snprintf(diff + pos, diff_size - pos, "  ~ %s: %s -> %s\n", key,
                            value_to_summary(old_val, old_buf, sizeof(old_buf)),
                            value_to_summary(new_val, new_buf, sizeof(new_buf)));
            change_count++;
        }
        if (pos >= diff_size - 1) {
            pos = diff_size - 1;
            break;
        }
    }

    for (size_t i = 0; i < config_context_count(v1->snapshot); i++) {
        const char *key = config_context_get_key_at(v1->snapshot, i);
        if (!config_context_has(v2->snapshot, key)) {
            const config_value_t *old_val = config_context_get_value_at(v1->snapshot, i);
            char old_buf[128];
            pos += snprintf(diff + pos, diff_size - pos, "  - %s = %s\n", key,
                            value_to_summary(old_val, old_buf, sizeof(old_buf)));
            change_count++;
            if (pos >= diff_size - 1) {
                pos = diff_size - 1;
                break;
            }
        }
    }

    if (change_count == 0 && pos < diff_size - 1) {
        pos += snprintf(diff + pos, diff_size - pos, "  (no changes)\n");
    }

    return pos < diff_size ? pos : diff_size - 1;
}

/* ==================== 配置模板实现 ==================== */

config_error_t config_expand_template(config_context_t *ctx, const char *template_str, char *result,
                                      size_t result_size)
{
    if (!ctx || !template_str || !result || result_size == 0) {
        return CONFIG_ERROR_INVALID_ARG;
    }

    size_t out_pos = 0;
    const char *p = template_str;

    while (*p && out_pos < result_size - 1) {
        if (*p == '$' && *(p + 1) == '{') {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (end && end > start) {
                size_t key_len = (size_t)(end - start);
                char key_buf[256];
                if (key_len >= sizeof(key_buf))
                    key_len = sizeof(key_buf) - 1;
                __builtin_memcpy(key_buf, start, key_len);
                key_buf[key_len] = '\0';

                const config_value_t *val = config_context_get(ctx, key_buf);
                if (val) {
                    const char *str_val = config_value_get_string(val, "");
                    if (str_val) {
                        size_t vlen = strlen(str_val);
                        if (out_pos + vlen >= result_size)
                            vlen = result_size - out_pos - 1;
                        __builtin_memcpy(result + out_pos, str_val, vlen);
                        out_pos += vlen;
                    }
                } else {
                    if (out_pos + key_len + 3 < result_size) {
                        result[out_pos++] = '$';
                        result[out_pos++] = '{';
                        __builtin_memcpy(result + out_pos, key_buf, key_len);
                        out_pos += key_len;
                        result[out_pos++] = '}';
                    }
                }
                p = end + 1;
            } else {
                result[out_pos++] = *p++;
            }
        } else {
            result[out_pos++] = *p++;
        }
    }

    result[out_pos] = '\0';
    return CONFIG_SUCCESS;
}

config_error_t config_apply_template(config_context_t *ctx, config_context_t *template_ctx)
{
    if (!ctx || !template_ctx)
        return CONFIG_ERROR_INVALID_ARG;

    const config_iterator_t *it = config_context_iterator(template_ctx);
    if (!it)
        return CONFIG_SUCCESS;

    config_iterator_reset(it);
    while (config_iterator_has_next(it)) {
        const char *key = config_iterator_next_key(it);
        const config_value_t *val = config_context_get(template_ctx, key);
        if (val && !config_context_has(ctx, key)) {
            config_value_t *cloned = config_value_clone(val);
            if (cloned) {
                config_context_set(ctx, key, cloned);
            }
        }
    }

    return CONFIG_SUCCESS;
}

/* ==================== 高级配置服务API ==================== */

config_context_t *config_service_create(const char *service_name, config_schema_t *schema,
                                        bool enable_hot_reload, bool enable_encryption)
{
    if (!service_name) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    config_context_t *ctx = config_context_create(service_name);
    if (!ctx) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }

    if (schema) {
        config_context_set_schema(ctx, schema);
        config_schema_apply_defaults(schema, ctx);
    }

    if (enable_hot_reload) {
        config_context_set_hot_reload(ctx, true, 5000);
    }

    if (enable_encryption) {
        config_context_set_encryption(ctx, true);
    }

    return ctx;
}

config_error_t config_service_load(config_context_t *ctx, config_source_t **sources,
                                   size_t source_count)
{
    if (!ctx || !sources || source_count == 0)
        return CONFIG_ERROR_INVALID_ARG;

    config_error_t err = CONFIG_SUCCESS;
    for (size_t i = 0; i < source_count; i++) {
        if (!sources[i])
            continue;
        err = config_source_load(sources[i], ctx);
        if (err != CONFIG_SUCCESS)
            return err;
    }

    return CONFIG_SUCCESS;
}

config_error_t config_service_save(config_context_t *ctx, config_source_t *primary_source)
{
    if (!ctx || !primary_source)
        return CONFIG_ERROR_INVALID_ARG;

    return config_source_save(primary_source, ctx);
}

config_error_t config_service_get_status(config_context_t *ctx, char *status_json,
                                         size_t status_size)
{
    if (!ctx || !status_json || status_size == 0)
        return CONFIG_ERROR_INVALID_ARG;

    // 生成状态JSON
    snprintf(status_json, status_size, "{\"status\":\"ok\",\"service\":\"%s\"}", "config_service");

    return CONFIG_SUCCESS;
}
