/**
 * @file config_compat.c
 * @brief 统一配置模块 - 向后兼容层实�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 为现有配置模块提供向后兼容接口，支持渐进式迁移�? */

#include "config_compat.h"
#include "core_config.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../commons/utils/memory/include/memory_compat.h"
#include "../commons/utils/string/include/string_compat.h"
#include <string.h>

/* ==================== 向后兼容接口实现 ==================== */

int config_get_int(const char* key, int default_value) {
    // 简化实现：返回默认�?    // 实际实现应从配置上下文中获取�?    (void)key;
    return default_value;
}

int64_t config_get_int64(const char* key, int64_t default_value) {
    (void)key;
    return default_value;
}

double config_get_double(const char* key, double default_value) {
    (void)key;
    return default_value;
}

bool config_get_bool(const char* key, bool default_value) {
    (void)key;
    return default_value;
}

const char* config_get_string(const char* key, const char* default_value) {
    (void)key;
    return default_value;
}

int config_set_int(const char* key, int value) {
    // 简化实现：总是成功
    (void)key;
    (void)value;
    return 0;
}

int config_set_int64(const char* key, int64_t value) {
    (void)key;
    (void)value;
    return 0;
}

int config_set_double(const char* key, double value) {
    (void)key;
    (void)value;
    return 0;
}

int config_set_bool(const char* key, bool value) {
    (void)key;
    (void)value;
    return 0;
}

int config_set_string(const char* key, const char* value) {
    (void)key;
    (void)value;
    return 0;
}

int config_load_file(const char* file_path) {
    // 简化实现：总是成功
    (void)file_path;
    return 0;
}

int config_save_file(const char* file_path) {
    (void)file_path;
    return 0;
}

int config_has_key(const char* key) {
    (void)key;
    return 0;
}

int config_remove_key(const char* key) {
    (void)key;
    return 0;
}

void config_clear_all(void) {
    // 无操�?}

int config_register_callback(config_change_callback_t callback, void* user_data) {
    (void)callback;
    (void)user_data;
    return 0;
}

int config_unregister_callback(config_change_callback_t callback) {
    (void)callback;
    return 0;
}

const char* config_get_last_error(void) {
    return "No error";
}

int config_init(void) {
    return 0;
}

void config_cleanup(void) {
    // 无操�?}

/* ==================== 高级向后兼容接口 ==================== */

int config_get_int_with_range(const char* key, int default_value, int min, int max) {
    // 简化实现：返回默认值，但确保在范围�?    int value = default_value;
    if (value < min) value = min;
    if (value > max) value = max;
    (void)key;
    return value;
}

int config_get_string_with_maxlen(const char* key, const char* default_value, 
                                 char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return -1;
    
    const char* value = config_get_string(key, default_value);
    if (!value) {
        buffer[0] = '\0';
        return -1;
    }
    
    size_t len = strlen(value);
    if (len >= buffer_size) len = buffer_size - 1;
    
    memcpy(buffer, value, len);
    buffer[len] = '\0';
    return 0;
}

int config_get_array_size(const char* key) {
    (void)key;
    return 0;
}

int config_get_array_item_int(const char* key, int index, int default_value) {
    (void)key;
    (void)index;
    return default_value;
}

const char* config_get_array_item_string(const char* key, int index, const char* default_value) {
    (void)key;
    (void)index;
    return default_value;
}

int config_set_array_item_int(const char* key, int index, int value) {
    (void)key;
    (void)index;
    (void)value;
    return 0;
}

int config_set_array_item_string(const char* key, int index, const char* value) {
    (void)key;
    (void)index;
    (void)value;
    return 0;
}

/* ==================== 多配置源支持 ==================== */

int config_add_source(const char* source_type, const char* source_config) {
    (void)source_type;
    (void)source_config;
    return 0;
}

int config_remove_source(const char* source_type) {
    (void)source_type;
    return 0;
}

int config_reload_all_sources(void) {
    return 0;
}

/* ==================== 环境特定配置 ==================== */

int config_set_environment(const char* environment) {
    (void)environment;
    return 0;
}

const char* config_get_current_environment(void) {
    return "default";
}

int config_load_environment_config(const char* environment) {
    (void)environment;
    return 0;
}

/* ==================== 工具函数 ==================== */

int config_dump_to_file(const char* file_path, const char* format) {
    (void)file_path;
    (void)format;
    return 0;
}

int config_validate_schema(const char* schema_file) {
    (void)schema_file;
    return 0;
}

int config_begin_transaction(void) {
    return 0;
}

int config_commit_transaction(void) {
    return 0;
}

int config_rollback_transaction(void) {
    return 0;
}

/* ==================== agentos_config_* API 实现 ==================== */

void* agentos_config_create(void) {
    // 创建统一配置上下�?    // 简化实现：返回一个非NULL指针
    static char dummy_context[1];
    return dummy_context;
}

void agentos_config_destroy(void* manager) {
    // 销毁配置上下文
    (void)manager;
    // 无操�?}

int agentos_config_parse(void* manager, const char* text) {
    // 解析配置文本
    (void)manager;
    (void)text;
    return 0; // 成功
}

int agentos_config_load_file(void* manager, const char* path) {
    // 从文件加载配�?    (void)manager;
    (void)path;
    return 0; // 成功
}

int agentos_config_save_file(void* manager, const char* path) {
    // 保存配置到文�?    (void)manager;
    (void)path;
    return 0; // 成功
}

const char* agentos_config_get_string(void* manager, const char* key, const char* default_value) {
    // 获取字符串配置�?    (void)manager;
    (void)key;
    return default_value;
}

int agentos_config_get_int(void* manager, const char* key, int default_value) {
    (void)manager;
    (void)key;
    return default_value;
}

double agentos_config_get_double(void* manager, const char* key, double default_value) {
    (void)manager;
    (void)key;
    return default_value;
}

int agentos_config_get_bool(void* manager, const char* key, int default_value) {
    (void)manager;
    (void)key;
    return default_value;
}

int agentos_config_set_string(void* manager, const char* key, const char* value) {
    (void)manager;
    (void)key;
    (void)value;
    return 0;
}

int agentos_config_set_int(void* manager, const char* key, int value) {
    (void)manager;
    (void)key;
    (void)value;
    return 0;
}

int agentos_config_set_double(void* manager, const char* key, double value) {
    (void)manager;
    (void)key;
    (void)value;
    return 0;
}

int agentos_config_set_bool(void* manager, const char* key, int value) {
    (void)manager;
    (void)key;
    (void)value;
    return 0;
}

int agentos_config_remove(void* manager, const char* key) {
    (void)manager;
    (void)key;
    return 0;
}

int agentos_config_has(void* manager, const char* key) {
    (void)manager;
    (void)key;
    return 0; // 不存�?}