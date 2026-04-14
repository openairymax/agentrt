// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file config_manager.c
 * @brief 统一配置管理系统实现
 *
 * @see config_manager.h
 */

#include "config_manager.h"
#include "svc_logger.h"
#include "platform.h"
#include "safe_string_utils.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 内部常量 ==================== */

#define CM_MAX_VALIDATORS 16

/* ==================== 内部数据结构 ==================== */

typedef struct {
    char pattern[CM_MAX_KEY_LEN];
    cm_change_callback_t callback;
    void* user_data;
} cm_watcher_t;

typedef struct {
    char pattern[CM_MAX_KEY_LEN];
    cm_validator_t validator;
} cm_validator_entry_t;

static struct {
    cm_config_t config;
    cm_entry_t entries[CM_MAX_ENTRIES];
    uint32_t entry_count;
    cm_change_record_t history[CM_MAX_HISTORY];
    uint32_t history_count;
    uint32_t history_head;
    cm_watcher_t watchers[CM_MAX_WATCHERS];
    uint32_t watcher_count;
    cm_validator_entry_t validators[CM_MAX_VALIDATORS];
    uint32_t validator_count;
    uint64_t global_version;
    bool initialized;
    agentos_platform_mutex_t mutex;
} g_cm = {0};

/* ==================== 辅助函数 ==================== */

static cm_entry_t* find_entry(const char* key) {
    for (uint32_t i = 0; i < g_cm.entry_count; i++) {
        if (strcmp(g_cm.entries[i].key, key) == 0)
            return &g_cm.entries[i];
    }
    return NULL;
}

static bool pattern_matches(const char* pattern, const char* key) {
    if (!pattern || !pattern[0]) return true;
    if (strcmp(pattern, "*") == 0) return true;

    const char* star = strchr(pattern, '*');
    if (!star) return strcmp(pattern, key) == 0;

    size_t prefix_len = star - pattern;
    if (prefix_len > 0 && strncmp(pattern, key, prefix_len) != 0)
        return false;

    const char* suffix = star + 1;
    if (suffix && *suffix) {
        size_t key_len = strlen(key);
        size_t suffix_len = strlen(suffix);
        if (key_len < suffix_len) return false;
        return strcmp(key + key_len - suffix_len, suffix) == 0;
    }

    return true;
}

static void add_history(const char* key, const char* old_value,
                        const char* new_value, const char* source) {
    cm_change_record_t* rec = &g_cm.history[g_cm.history_head];
    memset(rec, 0, sizeof(cm_change_record_t));
    safe_strcpy(rec->key, key, CM_MAX_KEY_LEN);
    if (old_value) safe_strcpy(rec->old_value, old_value, CM_MAX_VALUE_LEN);
    if (new_value) safe_strcpy(rec->new_value, new_value, CM_MAX_VALUE_LEN);
    rec->timestamp = agentos_platform_get_time_ms();
    if (source) safe_strcpy(rec->source, source, sizeof(rec->source));

    g_cm.history_head = (g_cm.history_head + 1) % CM_MAX_HISTORY;
    if (g_cm.history_count < CM_MAX_HISTORY) g_cm.history_count++;
}

static void notify_watchers(const char* key, const char* old_value,
                            const char* new_value) {
    for (uint32_t i = 0; i < g_cm.watcher_count; i++) {
        if (pattern_matches(g_cm.watchers[i].pattern, key)) {
            g_cm.watchers[i].callback(key, old_value, new_value,
                                      g_cm.watchers[i].user_data);
        }
    }
}

static bool validate_value(const char* key, const char* value) {
    for (uint32_t i = 0; i < g_cm.validator_count; i++) {
        if (pattern_matches(g_cm.validators[i].pattern, key)) {
            char error_msg[256] = {0};
            if (!g_cm.validators[i].validator(key, value, error_msg, sizeof(error_msg))) {
                LOG_WARN("Config validation failed for '%s': %s", key, error_msg);
                return false;
            }
        }
    }
    return true;
}

/* ==================== 公共API实现 ==================== */

AGENTOS_API cm_config_t cm_create_default_config(void) {
    cm_config_t config;
    memset(&config, 0, sizeof(cm_config_t));
    safe_strcpy(config.base_path, "./config", sizeof(config.base_path));
    safe_strcpy(config.environment, "development", sizeof(config.environment));
    config.watch_interval_ms = 5000;
    config.max_history = CM_MAX_HISTORY;
    config.enable_hot_reload = true;
    config.enable_validation = true;
    config.enable_cross_process_sync = false;
    return config;
}

AGENTOS_API int cm_init(const cm_config_t* config) {
    if (g_cm.initialized) return 0;

    if (config) {
        memcpy(&g_cm.config, config, sizeof(cm_config_t));
    } else {
        g_cm.config = cm_create_default_config();
    }

    agentos_error_t err = agentos_platform_mutex_init(&g_cm.mutex);
    if (err != AGENTOS_SUCCESS) return -1;

    memset(g_cm.entries, 0, sizeof(g_cm.entries));
    g_cm.entry_count = 0;
    memset(g_cm.history, 0, sizeof(g_cm.history));
    g_cm.history_count = 0;
    g_cm.history_head = 0;
    g_cm.watcher_count = 0;
    g_cm.validator_count = 0;
    g_cm.global_version = 1;
    g_cm.initialized = true;

    cm_load_env("AGENTOS_", "env");

    LOG_INFO("Config manager initialized (env=%s, base_path=%s)",
             g_cm.config.environment, g_cm.config.base_path);
    return 0;
}

AGENTOS_API void cm_shutdown(void) {
    if (!g_cm.initialized) return;

    agentos_platform_mutex_lock(&g_cm.mutex);
    g_cm.initialized = false;
    agentos_platform_mutex_unlock(&g_cm.mutex);

    agentos_platform_mutex_destroy(&g_cm.mutex);

    LOG_INFO("Config manager shutdown");
}

/* ==================== 配置读写 ==================== */

AGENTOS_API const char* cm_get(const char* key, const char* default_value) {
    if (!key) return default_value;
    if (!g_cm.initialized) cm_init(NULL);

    agentos_platform_mutex_lock(&g_cm.mutex);

    cm_entry_t* entry = find_entry(key);
    const char* result = entry ? entry->value : default_value;

    agentos_platform_mutex_unlock(&g_cm.mutex);
    return result;
}

AGENTOS_API int64_t cm_get_int(const char* key, int64_t default_value) {
    const char* val = cm_get(key, NULL);
    if (!val) return default_value;
    return strtoll(val, NULL, 10);
}

AGENTOS_API double cm_get_double(const char* key, double default_value) {
    const char* val = cm_get(key, NULL);
    if (!val) return default_value;
    return strtod(val, NULL);
}

AGENTOS_API bool cm_get_bool(const char* key, bool default_value) {
    const char* val = cm_get(key, NULL);
    if (!val) return default_value;
    if (strcasecmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
        strcasecmp(val, "yes") == 0 || strcasecmp(val, "on") == 0)
        return true;
    if (strcasecmp(val, "false") == 0 || strcmp(val, "0") == 0 ||
        strcasecmp(val, "no") == 0 || strcasecmp(val, "off") == 0)
        return false;
    return default_value;
}

AGENTOS_API int cm_set(const char* key, const char* value, const char* source) {
    if (!key) return -1;
    if (!g_cm.initialized) cm_init(NULL);

    agentos_platform_mutex_lock(&g_cm.mutex);

    if (g_cm.config.enable_validation && !validate_value(key, value)) {
        agentos_platform_mutex_unlock(&g_cm.mutex);
        return -1;
    }

    cm_entry_t* entry = find_entry(key);
    if (entry) {
        char old_value[CM_MAX_VALUE_LEN];
        safe_strcpy(old_value, entry->value, CM_MAX_VALUE_LEN);

        if (value) {
            safe_strcpy(entry->value, value, CM_MAX_VALUE_LEN);
        } else {
            entry->value[0] = '\0';
            entry->type = CM_TYPE_NULL;
        }

        entry->version = ++g_cm.global_version;
        entry->last_modified = agentos_platform_get_time_ms();
        entry->is_overridden = true;
        if (source) safe_strcpy(entry->source, source, sizeof(entry->source));

        add_history(key, old_value, value, source);

        agentos_platform_mutex_unlock(&g_cm.mutex);
        notify_watchers(key, old_value, value);
        return 0;
    }

    if (g_cm.entry_count >= CM_MAX_ENTRIES) {
        agentos_platform_mutex_unlock(&g_cm.mutex);
        return -1;
    }

    entry = &g_cm.entries[g_cm.entry_count];
    memset(entry, 0, sizeof(cm_entry_t));
    safe_strcpy(entry->key, key, CM_MAX_KEY_LEN);
    if (value) safe_strcpy(entry->value, value, CM_MAX_VALUE_LEN);
    entry->type = CM_TYPE_STRING;
    safe_strcpy(entry->namespace_, "default", CM_MAX_NAMESPACE_LEN);
    entry->version = ++g_cm.global_version;
    entry->last_modified = agentos_platform_get_time_ms();
    entry->is_default = false;
    entry->is_overridden = true;
    if (source) safe_strcpy(entry->source, source, sizeof(entry->source));
    g_cm.entry_count++;

    add_history(key, "", value, source);

    agentos_platform_mutex_unlock(&g_cm.mutex);
    notify_watchers(key, "", value);
    return 0;
}

AGENTOS_API int cm_set_namespaced(const char* namespace_, const char* key,
                                  const char* value, const char* source) {
    if (!namespace_ || !key) return -1;

    char full_key[CM_MAX_KEY_LEN + CM_MAX_NAMESPACE_LEN + 2];
    snprintf(full_key, sizeof(full_key), "%s.%s", namespace_, key);
    return cm_set(full_key, value, source);
}

/* ==================== 配置加载 ==================== */

AGENTOS_API int cm_load_json(const char* path, const char* namespace_) {
    if (!path) return -1;
    if (!g_cm.initialized) cm_init(NULL);

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        LOG_WARN("Config file not found: %s", path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 1024 * 1024) {
        fclose(fp);
        return -1;
    }

    char* data = (char*)AGENTOS_MALLOC(file_size + 1);
    if (!data) {
        fclose(fp);
        return -1;
    }

    size_t bytes_read = fread(data, 1, file_size, fp);
    fclose(fp);
    data[bytes_read] = '\0';

    int count = 0;
    char* saveptr = NULL;
    char* line = strtok_r(data, "\n", &saveptr);

    while (line) {
        while (*line == ' ' || *line == '\t' || *line == '\r') line++;
        if (*line == '\0' || *line == '#' || *line == '/') {
            line = strtok_r(NULL, "\n", &saveptr);
            continue;
        }

        char* eq = strchr(line, '=');
        if (!eq) {
            char* colon = strchr(line, ':');
            if (colon && colon > line + 1 && *(colon - 1) != ':') eq = colon;
        }

        if (eq) {
            char k[CM_MAX_KEY_LEN] = {0};
            char v[CM_MAX_VALUE_LEN] = {0};

            size_t klen = eq - line;
            if (klen >= CM_MAX_KEY_LEN) klen = CM_MAX_KEY_LEN - 1;
            memcpy(k, line, klen);
            k[klen] = '\0';

            while (klen > 0 && (k[klen - 1] == ' ' || k[klen - 1] == '\t'))
                k[--klen] = '\0';

            const char* val_start = eq + 1;
            while (*val_start == ' ' || *val_start == '\t') val_start++;

            size_t vlen = strlen(val_start);
            while (vlen > 0 && (val_start[vlen - 1] == '\r' ||
                   val_start[vlen - 1] == ' ' || val_start[vlen - 1] == ',' ||
                   val_start[vlen - 1] == '"'))
                vlen--;
            if (vlen >= CM_MAX_VALUE_LEN) vlen = CM_MAX_VALUE_LEN - 1;
            memcpy(v, val_start, vlen);
            v[vlen] = '\0';

            if (namespace_ && namespace_[0]) {
                char full_key[CM_MAX_KEY_LEN + CM_MAX_NAMESPACE_LEN + 2];
                snprintf(full_key, sizeof(full_key), "%s.%s", namespace_, k);
                cm_set(full_key, v, path);
            } else {
                cm_set(k, v, path);
            }
            count++;
        }

        line = strtok_r(NULL, "\n", &saveptr);
    }

    AGENTOS_FREE(data);

    LOG_INFO("Loaded %d config entries from %s (namespace=%s)",
             count, path, namespace_ ? namespace_ : "default");
    return count;
}

AGENTOS_API int cm_load_env(const char* prefix, const char* namespace_) {
    if (!prefix) return 0;
    if (!g_cm.initialized) cm_init(NULL);

    int count = 0;

#ifdef _WIN32
    char env_var[256];
    char search_prefix[256];
    snprintf(search_prefix, sizeof(search_prefix), "%s*", prefix);

    HANDLE hEnv = GetEnvironmentStrings();
    if (hEnv) {
        char* env = (char*)hEnv;
        while (*env) {
            if (strncmp(env, prefix, strlen(prefix)) == 0) {
                char* eq = strchr(env, '=');
                if (eq) {
                    size_t klen = eq - env - strlen(prefix);
                    char key[CM_MAX_KEY_LEN];
                    safe_strcpy(key, env + strlen(prefix), (uint32_t)(klen + 1 > CM_MAX_KEY_LEN ? CM_MAX_KEY_LEN : klen + 1));
                    for (char* p = key; *p; p++) {
                        if (*p == '_') *p = '.';
                        else if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
                    }
                    cm_set_namespaced(namespace_ ? namespace_ : "env", key, eq + 1, "env");
                    count++;
                }
            }
            env += strlen(env) + 1;
        }
        FreeEnvironmentStrings(hEnv);
    }
#else
    extern char** environ;
    for (char** env = environ; *env; env++) {
        if (strncmp(*env, prefix, strlen(prefix)) == 0) {
            char* eq = strchr(*env, '=');
            if (eq) {
                size_t klen = eq - *env - strlen(prefix);
                char key[CM_MAX_KEY_LEN];
                safe_strcpy(key, *env + strlen(prefix), (uint32_t)(klen + 1 > CM_MAX_KEY_LEN ? CM_MAX_KEY_LEN : klen + 1));
                for (char* p = key; *p; p++) {
                    if (*p == '_') *p = '.';
                    else if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
                }
                cm_set_namespaced(namespace_ ? namespace_ : "env", key, eq + 1, "env");
                count++;
            }
        }
    }
#endif

    LOG_DEBUG("Loaded %d config entries from environment (prefix=%s)", count, prefix);
    return count;
}

AGENTOS_API int cm_load_args(int argc, char** argv) {
    if (!argv) return 0;
    int count = 0;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--", 2) == 0) {
            char* eq = strchr(argv[i] + 2, '=');
            if (eq) {
                char key[CM_MAX_KEY_LEN];
                size_t klen = eq - argv[i] - 2;
                if (klen >= CM_MAX_KEY_LEN) klen = CM_MAX_KEY_LEN - 1;
                memcpy(key, argv[i] + 2, klen);
                key[klen] = '\0';
                cm_set(key, eq + 1, "cli");
                count++;
            } else if (i + 1 < argc && argv[i + 1][0] != '-') {
                cm_set(argv[i] + 2, argv[i + 1], "cli");
                i++;
                count++;
            }
        }
    }

    return count;
}

/* ==================== 配置监视与热更新 ==================== */

AGENTOS_API int cm_watch(const char* key_pattern, cm_change_callback_t callback,
                         void* user_data) {
    if (!callback) return -1;
    if (!g_cm.initialized) cm_init(NULL);

    agentos_platform_mutex_lock(&g_cm.mutex);

    if (g_cm.watcher_count >= CM_MAX_WATCHERS) {
        agentos_platform_mutex_unlock(&g_cm.mutex);
        return -1;
    }

    cm_watcher_t* w = &g_cm.watchers[g_cm.watcher_count];
    if (key_pattern) safe_strcpy(w->pattern, key_pattern, CM_MAX_KEY_LEN);
    else w->pattern[0] = '\0';
    w->callback = callback;
    w->user_data = user_data;
    g_cm.watcher_count++;

    agentos_platform_mutex_unlock(&g_cm.mutex);

    return 0;
}

AGENTOS_API int cm_unwatch(const char* key_pattern, cm_change_callback_t callback) {
    if (!callback) return -1;

    agentos_platform_mutex_lock(&g_cm.mutex);

    for (uint32_t i = 0; i < g_cm.watcher_count; i++) {
        if (g_cm.watchers[i].callback == callback &&
            (!key_pattern || strcmp(g_cm.watchers[i].pattern, key_pattern) == 0)) {
            if (i < g_cm.watcher_count - 1) {
                g_cm.watchers[i] = g_cm.watchers[g_cm.watcher_count - 1];
            }
            memset(&g_cm.watchers[g_cm.watcher_count - 1], 0, sizeof(cm_watcher_t));
            g_cm.watcher_count--;
            break;
        }
    }

    agentos_platform_mutex_unlock(&g_cm.mutex);
    return 0;
}

AGENTOS_API int cm_reload(void) {
    if (!g_cm.initialized) return -1;

    LOG_INFO("Config reload triggered");
    return cm_load_json(
        g_cm.config.base_path[0] ? g_cm.config.base_path : "./config",
        NULL);
}

/* ==================== 配置校验 ==================== */

AGENTOS_API int cm_register_validator(const char* key_pattern, cm_validator_t validator) {
    if (!validator) return -1;

    agentos_platform_mutex_lock(&g_cm.mutex);

    if (g_cm.validator_count >= CM_MAX_VALIDATORS) {
        agentos_platform_mutex_unlock(&g_cm.mutex);
        return -1;
    }

    cm_validator_entry_t* v = &g_cm.validators[g_cm.validator_count];
    if (key_pattern) safe_strcpy(v->pattern, key_pattern, CM_MAX_KEY_LEN);
    else v->pattern[0] = '\0';
    v->validator = validator;
    g_cm.validator_count++;

    agentos_platform_mutex_unlock(&g_cm.mutex);
    return 0;
}

AGENTOS_API int cm_validate_all(void) {
    int failures = 0;

    agentos_platform_mutex_lock(&g_cm.mutex);

    for (uint32_t i = 0; i < g_cm.entry_count; i++) {
        if (!validate_value(g_cm.entries[i].key, g_cm.entries[i].value)) {
            failures++;
        }
    }

    agentos_platform_mutex_unlock(&g_cm.mutex);
    return failures;
}

/* ==================== 版本控制 ==================== */

AGENTOS_API int cm_get_history(const char* key, cm_change_record_t* records,
                               uint32_t max_count, uint32_t* found_count) {
    if (!records || !found_count) return -1;

    agentos_platform_mutex_lock(&g_cm.mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < g_cm.history_count && count < max_count; i++) {
        uint32_t idx = (g_cm.history_head + CM_MAX_HISTORY - 1 - i) % CM_MAX_HISTORY;
        cm_change_record_t* rec = &g_cm.history[idx];

        if (key && strcmp(rec->key, key) != 0) continue;

        memcpy(&records[count], rec, sizeof(cm_change_record_t));
        count++;
    }

    *found_count = count;
    agentos_platform_mutex_unlock(&g_cm.mutex);
    return 0;
}

AGENTOS_API int cm_rollback(const char* key, uint64_t version) {
    if (!key) return -1;

    agentos_platform_mutex_lock(&g_cm.mutex);

    for (uint32_t i = 0; i < g_cm.history_count; i++) {
        uint32_t idx = (g_cm.history_head + CM_MAX_HISTORY - 1 - i) % CM_MAX_HISTORY;
        cm_change_record_t* rec = &g_cm.history[idx];

        if (strcmp(rec->key, key) == 0) {
            if (version == 0 || true) {
                agentos_platform_mutex_unlock(&g_cm.mutex);
                return cm_set(key, rec->old_value, "rollback");
            }
        }
    }

    agentos_platform_mutex_unlock(&g_cm.mutex);
    return -1;
}

/* ==================== 环境差异化 ==================== */

AGENTOS_API const char* cm_get_environment(void) {
    if (!g_cm.initialized) return "unknown";
    return g_cm.config.environment;
}

AGENTOS_API int cm_set_environment(const char* env) {
    if (!env) return -1;

    agentos_platform_mutex_lock(&g_cm.mutex);
    safe_strcpy(g_cm.config.environment, env, sizeof(g_cm.config.environment));
    agentos_platform_mutex_unlock(&g_cm.mutex);

    cm_load_environment_config(env);

    LOG_INFO("Environment set to: %s", env);
    return 0;
}

AGENTOS_API int cm_load_environment_config(const char* env) {
    if (!env) return -1;

    char path[CM_MAX_PATH_LEN];
    snprintf(path, sizeof(path), "%s/%s.json",
             g_cm.config.base_path[0] ? g_cm.config.base_path : "./config", env);

    int count = cm_load_json(path, env);
    if (count < 0) {
        snprintf(path, sizeof(path), "%s/%s.yaml",
                 g_cm.config.base_path[0] ? g_cm.config.base_path : "./config", env);
        count = cm_load_json(path, env);
    }

    return count > 0 ? count : 0;
}

/* ==================== 导出 ==================== */

AGENTOS_API char* cm_export_json(const char* namespace_) {
    if (!g_cm.initialized) return NULL;

    agentos_platform_mutex_lock(&g_cm.mutex);

    size_t buf_size = 8192;
    char* buf = (char*)AGENTOS_MALLOC(buf_size);
    if (!buf) {
        agentos_platform_mutex_unlock(&g_cm.mutex);
        return NULL;
    }
    size_t pos = 0;

    pos += snprintf(buf + pos, buf_size - pos, "{\"environment\":\"%s\",\"version\":%llu,\"entries\":{",
                    g_cm.config.environment, (unsigned long long)g_cm.global_version);

    uint32_t exported = 0;
    for (uint32_t i = 0; i < g_cm.entry_count; i++) {
        cm_entry_t* e = &g_cm.entries[i];

        if (namespace_ && namespace_[0] &&
            strncmp(e->key, namespace_, strlen(namespace_)) != 0)
            continue;

        if (exported > 0) pos += snprintf(buf + pos, buf_size - pos, ",");
        pos += snprintf(buf + pos, buf_size - pos, "\"%s\":{\"value\":\"%s\",\"type\":%d,\"version\":%llu,\"source\":\"%s\"}",
                        e->key, e->value, e->type, (unsigned long long)e->version, e->source);
        exported++;
    }

    pos += snprintf(buf + pos, buf_size - pos, "}}");

    agentos_platform_mutex_unlock(&g_cm.mutex);
    return buf;
}

AGENTOS_API uint32_t cm_entry_count(void) {
    return g_cm.entry_count;
}
