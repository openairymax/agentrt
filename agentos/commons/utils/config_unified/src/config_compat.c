/**
 * @file config_compat.c
 * @brief 统一配置模块 - 向后兼容层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 为现有配置模块提供向后兼容接口，支持渐进式迁移。
 * 委托到 core_config 模块实现实际配置操作。
 */

#include "config_compat.h"
#include "core_config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static config_context_t* g_compat_ctx = NULL;
static config_compat_stats_t g_compat_stats = {0};
static bool g_compat_initialized = false;

#define MAX_CALLBACKS 16
#define MAX_SOURCES 8
#define MAX_ENV_LEN 64

static config_change_callback_t g_callbacks[MAX_CALLBACKS];
static void* g_callback_user_data[MAX_CALLBACKS];
static int g_callback_count = 0;

static char g_sources[MAX_SOURCES][128];
static int g_source_count = 0;

static char g_environment[MAX_ENV_LEN] = "default";

static config_context_t* _get_or_create_ctx(void) {
    if (!g_compat_ctx) {
        g_compat_ctx = config_context_create("compat_default");
    }
    return g_compat_ctx;
}

static void _ensure_manager_ctx(void** manager) {
    if (!manager) return;
    if (!*manager) {
        *manager = _get_or_create_ctx();
    }
}

int config_get_int(const char* key, int default_value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return config_value_get_int(val, default_value);
}

int64_t config_get_int64(const char* key, int64_t default_value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return config_value_get_int64(val, default_value);
}

double config_get_double(const char* key, double default_value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return config_value_get_double(val, default_value);
}

bool config_get_bool(const char* key, bool default_value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return config_value_get_bool(val, default_value);
}

const char* config_get_string(const char* key, const char* default_value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return config_value_get_string(val, default_value);
}

int config_set_int(const char* key, int value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_int(value);
    if (!val) return -1;
    config_error_t err = config_context_set(ctx, key, val);
    return err == CONFIG_SUCCESS ? 0 : -1;
}

int config_set_int64(const char* key, int64_t value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_int64(value);
    if (!val) return -1;
    config_error_t err = config_context_set(ctx, key, val);
    return err == CONFIG_SUCCESS ? 0 : -1;
}

int config_set_double(const char* key, double value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_double(value);
    if (!val) return -1;
    config_error_t err = config_context_set(ctx, key, val);
    return err == CONFIG_SUCCESS ? 0 : -1;
}

int config_set_bool(const char* key, bool value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_bool(value);
    if (!val) return -1;
    config_error_t err = config_context_set(ctx, key, val);
    return err == CONFIG_SUCCESS ? 0 : -1;
}

int config_set_string(const char* key, const char* value) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_string(value ? value : "");
    if (!val) return -1;
    config_error_t err = config_context_set(ctx, key, val);
    return err == CONFIG_SUCCESS ? 0 : -1;
}

int config_load_file(const char* file_path) {
    g_compat_stats.total_calls++;
    if (!file_path) return -1;
    FILE* f = fopen(file_path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || fsize > 10 * 1024 * 1024) { fclose(f); return -1; }
    char* buf = (char*)malloc(fsize + 1);
    if (!buf) { fclose(f); return -1; }
    size_t nread = fread(buf, 1, fsize, f);
    fclose(f);
    if (nread != (size_t)fsize) { free(buf); return -1; }
    buf[nread] = '\0';
    char* line = buf;
    while (line && *line) {
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') line++;
        if (*line == '#' || *line == ';' || *line == '\0') {
            line = strchr(line, '\n');
            if (line) line++;
            continue;
        }
        char* eq = strchr(line, '=');
        if (!eq) { line = strchr(line, '\n'); if (line) line++; continue; }
        *eq = '\0';
        char* key_end = eq - 1;
        while (key_end > line && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';
        char* val_start = eq + 1;
        while (*val_start == ' ' || *val_start == '\t') val_start++;
        char* val_end = val_start + strlen(val_start) - 1;
        while (val_end > val_start && (*val_end == '\n' || *val_end == '\r' || *val_end == ' ')) *val_end-- = '\0';
        config_set_string(line, val_start);
        line = strchr(eq + 1, '\n');
        if (line) line++;
    }
    free(buf);
    return 0;
}

int config_save_file(const char* file_path) {
    g_compat_stats.total_calls++;
    if (!file_path) return -1;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx) return -1;
    FILE* f = fopen(file_path, "w");
    if (!f) return -1;
    size_t count = config_context_count(ctx);
    for (size_t i = 0; i < count; i++) {
        const char* key = config_context_get_key_at(ctx, i);
        const config_value_t* val = config_context_get_value_at(ctx, i);
        if (!key || !val) continue;
        fprintf(f, "%s=", key);
        config_value_print(val, 0);
        fprintf(f, "\n");
    }
    fclose(f);
    return 0;
}

int config_has_key(const char* key) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return 0;
    return config_context_has(ctx, key) ? 1 : 0;
}

int config_remove_key(const char* key) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_error_t err = config_context_delete(ctx, key);
    return err == CONFIG_SUCCESS ? 0 : -1;
}

void config_clear_all(void) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (ctx) config_context_clear(ctx);
}

int config_register_callback(config_change_callback_t callback, void* user_data) {
    g_compat_stats.total_calls++;
    if (!callback || g_callback_count >= MAX_CALLBACKS) return -1;
    g_callbacks[g_callback_count] = callback;
    g_callback_user_data[g_callback_count] = user_data;
    g_callback_count++;
    return 0;
}

int config_unregister_callback(config_change_callback_t callback) {
    g_compat_stats.total_calls++;
    if (!callback) return -1;
    for (int i = 0; i < g_callback_count; i++) {
        if (g_callbacks[i] == callback) {
            g_callbacks[i] = g_callbacks[g_callback_count - 1];
            g_callback_user_data[i] = g_callback_user_data[g_callback_count - 1];
            g_callback_count--;
            return 0;
        }
    }
    return -1;
}

const char* config_get_last_error(void) {
    return "No error";
}

int config_init(void) {
    if (g_compat_initialized) return 0;
    g_compat_ctx = config_context_create("compat_global");
    g_compat_initialized = true;
    memset(&g_compat_stats, 0, sizeof(g_compat_stats));
    return 0;
}

void config_cleanup(void) {
    if (g_compat_ctx) {
        config_context_destroy(g_compat_ctx);
        g_compat_ctx = NULL;
    }
    g_compat_initialized = false;
}

int config_get_int_with_range(const char* key, int default_value, int min, int max) {
    int value = config_get_int(key, default_value);
    if (value < min) value = min;
    if (value > max) value = max;
    return value;
}

int config_get_string_with_maxlen(const char* key, const char* default_value,
                                 char* buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return -1;
    const char* value = config_get_string(key, default_value);
    if (!value) { buffer[0] = '\0'; return -1; }
    size_t len = strlen(value);
    if (len >= buffer_size) len = buffer_size - 1;
    memcpy(buffer, value, len);
    buffer[len] = '\0';
    return 0;
}

int config_get_array_size(const char* key) {
    g_compat_stats.total_calls++;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx || !key) return 0;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val || config_value_get_type(val) != CONFIG_TYPE_ARRAY) return 0;
    char count_key[256];
    snprintf(count_key, sizeof(count_key), "%s.__array_len", key);
    const config_value_t* count_val = config_context_get(ctx, count_key);
    if (count_val) return config_value_get_int(count_val, 0);
    return 0;
}

int config_get_array_item_int(const char* key, int index, int default_value) {
    g_compat_stats.total_calls++;
    if (index < 0) return default_value;
    char item_key[256];
    snprintf(item_key, sizeof(item_key), "%s.%d", key, index);
    return config_get_int(item_key, default_value);
}

const char* config_get_array_item_string(const char* key, int index, const char* default_value) {
    g_compat_stats.total_calls++;
    if (index < 0) return default_value;
    char item_key[256];
    snprintf(item_key, sizeof(item_key), "%s.%d", key, index);
    return config_get_string(item_key, default_value);
}

int config_set_array_item_int(const char* key, int index, int value) {
    g_compat_stats.total_calls++;
    if (index < 0) return -1;
    char item_key[256];
    snprintf(item_key, sizeof(item_key), "%s.%d", key, index);
    return config_set_int(item_key, value);
}

int config_set_array_item_string(const char* key, int index, const char* value) {
    g_compat_stats.total_calls++;
    if (index < 0) return -1;
    char item_key[256];
    snprintf(item_key, sizeof(item_key), "%s.%d", key, index);
    return config_set_string(item_key, value);
}

int config_add_source(const char* source_type, const char* source_config) {
    g_compat_stats.total_calls++;
    if (!source_type || !source_config) return -1;
    if (g_source_count >= MAX_SOURCES) return -1;
    snprintf(g_sources[g_source_count], sizeof(g_sources[g_source_count]),
             "%s:%s", source_type, source_config);
    g_source_count++;
    if (strcmp(source_type, "file") == 0) {
        return config_load_file(source_config);
    }
    return 0;
}

int config_remove_source(const char* source_type) {
    g_compat_stats.total_calls++;
    if (!source_type) return -1;
    for (int i = 0; i < g_source_count; i++) {
        if (strncmp(g_sources[i], source_type, strlen(source_type)) == 0 && g_sources[i][strlen(source_type)] == ':') {
            g_sources[i][0] = '\0';
            g_source_count--;
            if (i < g_source_count) {
                memcpy(g_sources[i], g_sources[g_source_count], sizeof(g_sources[i]));
                g_sources[g_source_count][0] = '\0';
            }
            return 0;
        }
    }
    return -1;
}

int config_reload_all_sources(void) {
    g_compat_stats.total_calls++;
    for (int i = 0; i < g_source_count; i++) {
        if (g_sources[i][0] == '\0') continue;
        if (strncmp(g_sources[i], "file:", 5) == 0) {
            config_load_file(g_sources[i] + 5);
        }
    }
    return 0;
}

int config_set_environment(const char* environment) {
    g_compat_stats.total_calls++;
    if (!environment) return -1;
    strncpy(g_environment, environment, sizeof(g_environment) - 1);
    g_environment[sizeof(g_environment) - 1] = '\0';
    return 0;
}

const char* config_get_current_environment(void) {
    return g_environment;
}

int config_load_environment_config(const char* environment) {
    g_compat_stats.total_calls++;
    if (!environment) return -1;
    char path[512];
    snprintf(path, sizeof(path), "%s.d/%s.conf", environment, environment);
    if (config_load_file(path) != 0) {
        snprintf(path, sizeof(path), "/etc/agentos/%s.conf", environment);
        config_load_file(path);
    }
    config_set_environment(environment);
    return 0;
}

int config_dump_to_file(const char* file_path, const char* format) {
    g_compat_stats.total_calls++;
    if (!file_path) return -1;
    config_context_t* ctx = _get_or_create_ctx();
    if (!ctx) return -1;
    FILE* f = fopen(file_path, "w");
    if (!f) return -1;
    size_t count = config_context_count(ctx);
    for (size_t i = 0; i < count; i++) {
        const char* key = config_context_get_key_at(ctx, i);
        const config_value_t* val = config_context_get_value_at(ctx, i);
        if (!key || !val) continue;
        if (format && strcmp(format, "json") == 0) {
            fprintf(f, "\"%s\": ", key);
            config_value_print(val, 0);
            if (i < count - 1) fprintf(f, ",");
            fprintf(f, "\n");
        } else {
            fprintf(f, "%s=", key);
            config_value_print(val, 0);
            fprintf(f, "\n");
        }
    }
    fclose(f);
    return 0;
}

int config_validate_schema(const char* schema_file) {
    g_compat_stats.total_calls++;
    if (!schema_file) return -1;
    FILE* f = fopen(schema_file, "r");
    if (!f) return -1;
    fclose(f);
    return 0;
}

static config_context_t* g_transaction_ctx = NULL;
static int g_transaction_depth = 0;

int config_begin_transaction(void) {
    if (!g_transaction_ctx) {
        g_transaction_ctx = _get_or_create_ctx();
        if (!g_transaction_ctx) return -1;
    }
    g_transaction_depth++;
    return 0;
}

int config_commit_transaction(void) {
    if (g_transaction_depth <= 0) return -1;
    g_transaction_depth--;
    if (g_transaction_depth == 0) {
        config_error_t err = config_save(g_transaction_ctx);
        if (err != CONFIG_SUCCESS) return -1;
        g_transaction_ctx = NULL;
    }
    return 0;
}

int config_rollback_transaction(void) {
    if (g_transaction_depth <= 0) return -1;
    g_transaction_depth--;
    if (g_transaction_depth == 0) {
        config_error_t err = config_reload(g_transaction_ctx);
        if (err != CONFIG_SUCCESS) return -1;
        g_transaction_ctx = NULL;
    }
    return 0;
}

void* agentos_config_create(void) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = config_context_create("agentos_compat");
    return ctx;
}

void agentos_config_destroy(void* manager) {
    if (manager) {
        config_context_destroy((config_context_t*)manager);
    }
}

int agentos_config_parse(void* manager, const char* text) {
    g_compat_stats.agentos_config_calls++;
    if (!text) return -1;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    char* text_copy = strdup(text);
    if (!text_copy) return -1;
    char* line = text_copy;
    while (line && *line) {
        while (*line == ' ' || *line == '\t' || *line == '\n' || *line == '\r') line++;
        if (*line == '#' || *line == ';' || *line == '\0') {
            line = strchr(line, '\n');
            if (line) line++;
            continue;
        }
        char* eq = strchr(line, '=');
        if (!eq) { line = strchr(line, '\n'); if (line) line++; continue; }
        *eq = '\0';
        char* key_end = eq - 1;
        while (key_end > line && (*key_end == ' ' || *key_end == '\t')) *key_end-- = '\0';
        char* val_start = eq + 1;
        while (*val_start == ' ' || *val_start == '\t') val_start++;
        char* val_end = val_start + strlen(val_start) - 1;
        while (val_end > val_start && (*val_end == '\n' || *val_end == '\r' || *val_end == ' ')) *val_end-- = '\0';
        config_value_t* val = config_value_create_string(val_start);
        if (val) config_context_set(ctx, line, val);
        line = strchr(eq + 1, '\n');
        if (line) line++;
    }
    free(text_copy);
    return 0;
}

int agentos_config_load_file(void* manager, const char* path) {
    g_compat_stats.agentos_config_calls++;
    return config_load_file(path);
}

int agentos_config_save_file(void* manager, const char* path) {
    g_compat_stats.agentos_config_calls++;
    return config_save_file(path);
}

const char* agentos_config_get_string(void* manager, const char* key, const char* default_value) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return config_value_get_string(val, default_value);
}

int agentos_config_get_int(void* manager, const char* key, int default_value) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return config_value_get_int(val, default_value);
}

double agentos_config_get_double(void* manager, const char* key, double default_value) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return config_value_get_double(val, default_value);
}

int agentos_config_get_bool(void* manager, const char* key, int default_value) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return default_value;
    const config_value_t* val = config_context_get(ctx, key);
    if (!val) return default_value;
    return (int)config_value_get_bool(val, (bool)default_value);
}

int agentos_config_set_string(void* manager, const char* key, const char* value) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_string(value ? value : "");
    if (!val) return -1;
    return config_context_set(ctx, key, val) == CONFIG_SUCCESS ? 0 : -1;
}

int agentos_config_set_int(void* manager, const char* key, int value) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_int(value);
    if (!val) return -1;
    return config_context_set(ctx, key, val) == CONFIG_SUCCESS ? 0 : -1;
}

int agentos_config_set_double(void* manager, const char* key, double value) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_double(value);
    if (!val) return -1;
    return config_context_set(ctx, key, val) == CONFIG_SUCCESS ? 0 : -1;
}

int agentos_config_set_bool(void* manager, const char* key, int value) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    config_value_t* val = config_value_create_bool((bool)value);
    if (!val) return -1;
    return config_context_set(ctx, key, val) == CONFIG_SUCCESS ? 0 : -1;
}

int agentos_config_remove(void* manager, const char* key) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return -1;
    return config_context_delete(ctx, key) == CONFIG_SUCCESS ? 0 : -1;
}

int agentos_config_has(void* manager, const char* key) {
    g_compat_stats.agentos_config_calls++;
    config_context_t* ctx = (config_context_t*)manager;
    if (!ctx) ctx = _get_or_create_ctx();
    if (!ctx || !key) return 0;
    return config_context_has(ctx, key) ? 1 : 0;
}
