/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_config.c - Configuration Manager: Runtime Configuration Updates
 */

/**
 * @file cupolas_config.c
 * @brief Configuration Manager - Runtime configuration updates
 * @author Spharx AgentOS Team
 * @date 2024
 *
 * This module implements configuration management:
 * - Configuration validation and application
 * - Version control
 * - Observer pattern notifications
 * - Auto-reload mechanism
 */

#include "cupolas_config.h"
#include "yaml_minimal.h"
#include "utils/cupolas_utils.h"
#include "platform/platform.h"
#include "cupolas_metrics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#if cupolas_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#endif

#define MAX_CONFIG_DIR 512
#define MAX_ERROR_MSG 1024
#define MAX_WATCHERS 32

typedef struct config_entry {
    config_type_t type;
    config_status_t status;
    char file_path[MAX_CONFIG_DIR];
    config_version_t version;
    time_t last_modified;
    void* data;
} config_entry_t;

typedef struct config_watcher {
    int id;
    config_type_t type;
    config_observer_t callback;
    void* user_data;
    bool active;
} config_watcher_t;

struct cupolas_config {
    char config_dir[MAX_CONFIG_DIR];

    config_entry_t entries[CONFIG_TYPE_ALL + 1];
    config_watcher_t watchers[MAX_WATCHERS];
    int next_watcher_id;

    cupolas_rwlock_t lock;
    char last_error[MAX_ERROR_MSG];

    cupolas_thread_t monitor_thread;
    bool monitor_running;
};

static const char* config_type_names[] = {
    "permission_rules",
    "sanitizer_rules",
    "resource_limits",
    "log_level",
    "audit_policy",
    "all"
};

static const char* config_status_names[] = {
    "ok",
    "loading",
    "validating",
    "applied",
    "rollback",
    "error"
};

const char* cupolas_config_type_string(config_type_t type) {
    if (type >= 0 && type <= CONFIG_TYPE_ALL) {
        return config_type_names[type];
    }
    return "unknown";
}

const char* cupolas_config_status_string(config_status_t status) {
    if (status >= 0 && status <= CONFIG_STATUS_ERROR) {
        return config_status_names[status];
    }
    return "unknown";
}

cupolas_config_t* cupolas_config_create(const char* config_dir) {
    cupolas_config_t* cfg = (cupolas_config_t*)cupolas_mem_alloc(sizeof(cupolas_config_t));
    if (!cfg) {
        return NULL;
    }

    memset(cfg, 0, sizeof(cupolas_config_t));

    if (config_dir) {
        snprintf(cfg->config_dir, sizeof(cfg->config_dir), "%s", config_dir);
    } else {
#if cupolas_PLATFORM_WINDOWS
        snprintf(cfg->config_dir, sizeof(cfg->config_dir), "C:\\ProgramData\\cupolas\\conf");
#else
        snprintf(cfg->config_dir, sizeof(cfg->config_dir), "/etc/agentos/cupolas/conf");
#endif
    }

    cupolas_rwlock_init(&cfg->lock);

    for (int i = 0; i <= CONFIG_TYPE_ALL; i++) {
        cfg->entries[i].type = (config_type_t)i;
        cfg->entries[i].status = CONFIG_STATUS_OK;
    }

    cfg->next_watcher_id = 1;

    return cfg;
}

void cupolas_config_destroy(cupolas_config_t* cfg) {
    if (!cfg) return;

    cfg->monitor_running = false;

    for (int i = 0; i < (int)CONFIG_TYPE_ALL; i++) {
        if (cfg->entries[i].data) {
            yaml_destroy((yaml_document_t*)cfg->entries[i].data);
            cfg->entries[i].data = NULL;
        }
        free(cfg->entries[i].file_path);
        cfg->entries[i].file_path[0] = '\0';
    }

    cupolas_rwlock_destroy(&cfg->lock);

    free(cfg);
}

int cupolas_config_load(cupolas_config_t* cfg, config_type_t type, const char* file_path) {
    if (!cfg) return -1;

    cupolas_rwlock_wrlock(&cfg->lock);

    if (type == CONFIG_TYPE_ALL) {
        int loaded = 0;
        for (config_type_t i = 0; i < CONFIG_TYPE_ALL; i++) {
            if (cupolas_config_load(cfg, (config_type_t)i, NULL) == 0) {
                loaded++;
            }
        }
        cupolas_rwlock_unlock(&cfg->lock);
        return loaded > 0 ? 0 : -1;
    }

    if (type >= CONFIG_TYPE_ALL || type < 0) {
        cupolas_rwlock_unlock(&cfg->lock);
        return -1;
    }

    config_entry_t* entry = &cfg->entries[type];
    entry->status = CONFIG_STATUS_LOADING;

    if (file_path) {
        snprintf(entry->file_path, sizeof(entry->file_path), "%s", file_path);
    } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(entry->file_path, sizeof(entry->file_path), "%s/%s.yaml",
                cfg->config_dir, config_type_names[type]);
#pragma GCC diagnostic pop
    }

#if cupolas_PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(entry->file_path, GetFileExInfoStandard, &attr)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error), "Configuration file not found: %s", entry->file_path);
        entry->status = CONFIG_STATUS_ERROR;
        cupolas_rwlock_unlock(&cfg->lock);
        return -1;
    }
#else
    struct stat st;
    if (stat(entry->file_path, &st) != 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error), "Configuration file not found: %s", entry->file_path);
        entry->status = CONFIG_STATUS_ERROR;
        cupolas_rwlock_unlock(&cfg->lock);
        return -1;
    }
#endif

    if (entry->data) {
        yaml_destroy((yaml_document_t*)entry->data);
        entry->data = NULL;
    }

    yaml_document_t* doc = yaml_create();
    if (!doc) {
        snprintf(cfg->last_error, sizeof(cfg->last_error), "Memory allocation failed");
        entry->status = CONFIG_STATUS_ERROR;
        cupolas_rwlock_unlock(&cfg->lock);
        return -1;
    }

    if (yaml_parse_file(doc, entry->file_path) != 0) {
        const char* err = yaml_get_error(doc);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "YAML parse error: %s", err ? err : "unknown");
        yaml_destroy(doc);
        entry->status = CONFIG_STATUS_ERROR;
        cupolas_rwlock_unlock(&cfg->lock);
        return -1;
    }

    entry->data = doc;

#if cupolas_PLATFORM_WINDOWS
    ULARGE_INTEGER ft;
    ft.LowPart = attr.ftLastWriteTime.dwLowDateTime;
    ft.HighPart = attr.ftLastWriteTime.dwHighDateTime;
    entry->last_modified = (time_t)(ft.QuadPart / 10000000 - 11644473600LL);
#else
    entry->last_modified = st.st_mtime;
#endif

    entry->version.major = 1;
    entry->version.minor = 0;
    entry->version.patch = 0;
    entry->version.timestamp_ns = cupolas_get_timestamp_ns();

    entry->status = CONFIG_STATUS_APPLIED;

    cupolas_rwlock_unlock(&cfg->lock);

    return 0;
}

int cupolas_config_reload(cupolas_config_t* cfg, config_type_t type) {
    if (!cfg) return -1;

    cupolas_rwlock_wrlock(&cfg->lock);

    if (type >= 0 && type < CONFIG_TYPE_ALL) {
        config_entry_t* entry = &cfg->entries[type];
        entry->status = CONFIG_STATUS_LOADING;

        if (entry->data) {
            yaml_destroy((yaml_document_t*)entry->data);
            entry->data = NULL;
        }

        yaml_document_t* doc = yaml_create();
        if (!doc) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Reload %s: memory allocation failed", config_type_names[type]);
            entry->status = CONFIG_STATUS_ERROR;
            cupolas_rwlock_unlock(&cfg->lock);
            return -1;
        }

        int ret = yaml_parse_file(doc, entry->file_path);
        if (ret != 0) {
            const char* err = yaml_get_error(doc);
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Reload %s: parse error: %s", config_type_names[type],
                     err ? err : "unknown");
            yaml_destroy(doc);
            entry->status = CONFIG_STATUS_ROLLBACK;
            cupolas_rwlock_unlock(&cfg->lock);
            return -1;
        }

        entry->data = doc;
        entry->version.patch++;
        entry->version.timestamp_ns = cupolas_get_timestamp_ns();
        entry->status = CONFIG_STATUS_APPLIED;

        CUPOLAS_LOG("Config reloaded: type=%s", config_type_names[type]);
    } else {
        int reloaded = 0;
        for (int i = 0; i < (int)CONFIG_TYPE_ALL; i++) {
            if (cupolas_config_reload(cfg, (config_type_t)i) == 0) reloaded++;
        }
        cupolas_rwlock_unlock(&cfg->lock);
        return reloaded > 0 ? 0 : -1;
    }

    cupolas_rwlock_unlock(&cfg->lock);
    return 0;
}

int cupolas_config_validate(cupolas_config_t* cfg, config_type_t type,
                        config_validation_result_t* result) {
    if (!cfg || !result) return -1;

    cupolas_rwlock_rdlock(&cfg->lock);

    memset(result, 0, sizeof(config_validation_result_t));

    if (type >= 0 && type < CONFIG_TYPE_ALL) {
        config_entry_t* entry = &cfg->entries[type];
        yaml_document_t* doc = (yaml_document_t*)entry->data;

        if (!doc || !doc->root) {
            static const char* err = "No configuration loaded";
            result->valid = false;
            result->errors = &err;
            result->error_count = 1;
            cupolas_rwlock_unlock(&cfg->lock);
            return -1;
        }

        struct yaml_node* root = yaml_root(doc);
        if (root && root->type == YAML_NODE_MAPPING) {
            size_t sz = yaml_size(root);
            if (sz == 0) {
                static const char* warn = "Configuration mapping is empty";
                result->warnings = &warn;
                result->warning_count = 1;
            }
        }

        result->valid = true;

        switch (type) {
            case CONFIG_TYPE_RESOURCE_LIMITS: {
                struct yaml_node* max_mem = yaml_get(root, "max_memory_mb");
                struct yaml_node* max_cpu = yaml_get(root, "max_cpu_percent");
                if (max_mem) {
                    double v = yaml_as_double(max_mem, -1.0);
                    if (v <= 0) {
                        static const char* e = "max_memory_mb must be positive";
                        result->errors = &e; result->error_count = 1;
                        result->valid = false;
                    }
                }
                if (max_cpu) {
                    double v = yaml_as_double(max_cpu, -1.0);
                    if (v <= 0 || v > 100) {
                        static const char* e = "max_cpu_percent must be in (0,100]";
                        result->errors = &e; result->error_count = 1;
                        result->valid = false;
                    }
                }
                break;
            }
            case CONFIG_TYPE_LOG_LEVEL: {
                struct yaml_node* level = yaml_get(root, "level");
                if (level) {
                    const char* lv = yaml_as_string(level, "");
                    if (strcmp(lv, "") != 0 &&
                        strcmp(lv, "debug") != 0 && strcmp(lv, "info") != 0 &&
                        strcmp(lv, "warn") != 0 && strcmp(lv, "error") != 0 &&
                        strcmp(lv, "fatal") != 0) {
                        static const char* e = "Invalid log level";
                        result->errors = &e; result->error_count = 1;
                        result->valid = false;
                    }
                }
                break;
            }
            default:
                break;
        }
    } else {
        result->valid = false;
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return result->valid ? 0 : -1;
}

int cupolas_config_apply(cupolas_config_t* cfg, config_type_t type) {
    if (!cfg) return -1;

    cupolas_rwlock_wrlock(&cfg->lock);

    if (type >= 0 && type < CONFIG_TYPE_ALL) {
        config_entry_t* entry = &cfg->entries[type];
        entry->status = CONFIG_STATUS_APPLIED;
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return 0;
}

int cupolas_config_rollback(cupolas_config_t* cfg, config_type_t type) {
    if (!cfg) return -1;

    cupolas_rwlock_wrlock(&cfg->lock);

    if (type >= 0 && type < CONFIG_TYPE_ALL) {
        config_entry_t* entry = &cfg->entries[type];
        config_status_t prev_status = entry->status;
        entry->status = CONFIG_STATUS_ROLLBACK;
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                "Rolled back %s (from %s)", config_type_names[type],
                config_status_names[prev_status]);
        if (entry->version.major > 0) {
            entry->version.patch = (entry->version.patch > 0) ? entry->version.patch - 1 : 0;
        }
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return 0;
}

int cupolas_config_get_version(cupolas_config_t* cfg, config_type_t type,
                            config_version_t* version) {
    if (!cfg || !version) return -1;

    cupolas_rwlock_rdlock(&cfg->lock);

    if (type >= 0 && type < CONFIG_TYPE_ALL) {
        config_entry_t* entry = &cfg->entries[type];
        memcpy(version, &entry->version, sizeof(config_version_t));
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return 0;
}

int cupolas_config_watch(cupolas_config_t* cfg, config_type_t type,
                      config_observer_t callback, void* user_data) {
    if (!cfg || !callback) return -1;

    cupolas_rwlock_wrlock(&cfg->lock);

    int watcher_id = -1;
    for (int i = 0; i < MAX_WATCHERS; i++) {
        if (!cfg->watchers[i].active) {
            cfg->watchers[i].id = cfg->next_watcher_id++;
            cfg->watchers[i].type = type;
            cfg->watchers[i].callback = callback;
            cfg->watchers[i].user_data = user_data;
            cfg->watchers[i].active = true;
            watcher_id = cfg->watchers[i].id;
            break;
        }
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return watcher_id;
}

int cupolas_config_unwatch(cupolas_config_t* cfg, int watcher_id) {
    if (!cfg) return -1;

    cupolas_rwlock_wrlock(&cfg->lock);

    for (int i = 0; i < MAX_WATCHERS; i++) {
        if (cfg->watchers[i].id == watcher_id) {
            cfg->watchers[i].active = false;
            cupolas_rwlock_unlock(&cfg->lock);
            return 0;
        }
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return -1;
}

config_status_t cupolas_config_get_status(cupolas_config_t* cfg, config_type_t type) {
    if (!cfg) return CONFIG_STATUS_ERROR;

    cupolas_rwlock_rdlock(&cfg->lock);

    config_status_t status = CONFIG_STATUS_OK;
    if (type >= 0 && type < CONFIG_TYPE_ALL) {
        status = cfg->entries[type].status;
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return status;
}

int cupolas_config_set_auto_reload(cupolas_config_t* cfg, config_type_t type,
                                uint32_t interval_ms) {
    if (!cfg) return -1;

    cupolas_rwlock_wrlock(&cfg->lock);

    if (interval_ms > 0) {
        cfg->monitor_running = true;
    } else {
        cfg->monitor_running = false;
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return 0;
}

int cupolas_config_check_reload(cupolas_config_t* cfg, config_type_t type) {
    if (!cfg) return 0;

    cupolas_rwlock_wrlock(&cfg->lock);

    int changed = 0;
    for (config_type_t i = 0; i < CONFIG_TYPE_ALL; i++) {
        if (type != CONFIG_TYPE_ALL && type != i) {
            continue;
        }

        config_entry_t* entry = &cfg->entries[i];

#if cupolas_PLATFORM_WINDOWS
        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExA(entry->file_path, GetFileExInfoStandard, &attr)) {
            ULARGE_INTEGER ft;
            ft.LowPart = attr.ftLastWriteTime.dwLowDateTime;
            ft.HighPart = attr.ftLastWriteTime.dwHighDateTime;
            time_t current_modified = (time_t)(ft.QuadPart / 10000000 - 11644473600LL);

            if (current_modified > entry->last_modified) {
                entry->status = CONFIG_STATUS_LOADING;
                entry->last_modified = current_modified;
                entry->status = CONFIG_STATUS_APPLIED;
                changed++;
            }
        }
#else
        struct stat st;
        if (stat(entry->file_path, &st) == 0) {
            if (st.st_mtime > entry->last_modified) {
                entry->status = CONFIG_STATUS_LOADING;
                entry->last_modified = st.st_mtime;
                entry->status = CONFIG_STATUS_APPLIED;
                changed++;
            }
        }
#endif
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return changed;
}

const char* cupolas_config_get_last_error(cupolas_config_t* cfg) {
    if (!cfg) return NULL;

    cupolas_rwlock_rdlock(&cfg->lock);
    const char* error = cfg->last_error[0] ? cfg->last_error : NULL;
    cupolas_rwlock_unlock(&cfg->lock);

    return error;
}

const char* cupolas_config_get_config_dir(cupolas_config_t* cfg) {
    if (!cfg) return NULL;
    return cfg->config_dir;
}

size_t cupolas_config_export_json(cupolas_config_t* cfg, config_type_t type,
                               char* buffer, size_t size) {
    if (!cfg || !buffer || size == 0) return 0;

    cupolas_rwlock_rdlock(&cfg->lock);

    size_t offset = snprintf(buffer, size, "{\"configs\":[");

    for (config_type_t i = 0; i < CONFIG_TYPE_ALL; i++) {
        if (type != CONFIG_TYPE_ALL && type != i) {
            continue;
        }

        if (i > 0) {
            offset += snprintf(buffer + offset, size - offset, ",");
        }

        config_entry_t* entry = &cfg->entries[i];
        offset += snprintf(buffer + offset, size - offset,
                          "{\"type\":\"%s\",\"status\":\"%s\",\"version\":\"%u.%u.%u\"}",
                          config_type_names[i],
                          config_status_names[entry->status],
                          entry->version.major,
                          entry->version.minor,
                          entry->version.patch);
    }

    offset += snprintf(buffer + offset, size - offset, "]}");

    cupolas_rwlock_unlock(&cfg->lock);

    return offset;
}

size_t cupolas_config_export_yaml(cupolas_config_t* cfg, config_type_t type,
                               char* buffer, size_t size) {
    if (!cfg || !buffer || size == 0) return 0;

    cupolas_rwlock_rdlock(&cfg->lock);

    buffer[0] = '\0';
    size_t offset = 0;

    for (config_type_t i = 0; i < CONFIG_TYPE_ALL; i++) {
        if (type != CONFIG_TYPE_ALL && type != i) continue;

        config_entry_t* entry = &cfg->entries[i];
        yaml_document_t* doc = (yaml_document_t*)entry->data;

        offset += snprintf(buffer + offset, size > offset ? size - offset : 0,
                          "# %s configuration\n"
                          "# status: %s\n"
                          "# version: %u.%u.%u\n"
                          "# file: %s\n",
                          config_type_names[i],
                          config_status_names[entry->status],
                          entry->version.major,
                          entry->version.minor,
                          entry->version.patch,
                          entry->file_path[0] ? entry->file_path : "(none)");

        if (doc && doc->root) {
            if (offset < size - 1)
                yaml_dump(doc->root, buffer, size, 0);
            offset = strlen(buffer);
            if (offset < size - 1) {
                buffer[offset++] = '\n';
                buffer[offset] = '\0';
            }
        } else {
            const char* not_loaded = "  # (not loaded)\n";
            size_t nl_len = strlen(not_loaded);
            if (offset + nl_len + 1 < size) {
                memcpy(buffer + offset, not_loaded, nl_len);
                offset += nl_len;
                buffer[offset] = '\0';
            }
        }

        if (i < (config_type_t)(CONFIG_TYPE_ALL - 1)) {
            if (offset < size - 2) {
                buffer[offset++] = '\n'; buffer[offset] = '\0';
            }
        }
    }

    cupolas_rwlock_unlock(&cfg->lock);
    return offset;
}

int cupolas_config_reload_all(cupolas_config_t* cfg) {
    return cupolas_config_check_reload(cfg, CONFIG_TYPE_ALL);
}

bool cupolas_config_validate_all(cupolas_config_t* cfg) {
    if (!cfg) return false;

    cupolas_rwlock_rdlock(&cfg->lock);

    bool all_valid = true;
    for (config_type_t i = 0; i < CONFIG_TYPE_ALL; i++) {
        if (cfg->entries[i].status != CONFIG_STATUS_APPLIED) {
            all_valid = false;
            break;
        }
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return all_valid;
}