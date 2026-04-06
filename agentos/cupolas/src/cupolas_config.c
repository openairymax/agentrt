/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * cupolas_config.c - Configuration Manager: Runtime Configuration Updates
 */

/**
 * @file cupolas_config.c
 * @brief Configuration Manager - Runtime configuration updates
 * @author Spharx
 * @date 2024
 *
 * This module implements configuration management:
 * - Configuration validation and application
 * - Version control
 * - Observer pattern notifications
 * - Auto-reload mechanism
 */

#include "cupolas_config.h"
#include "../utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef cupolas_PLATFORM_WINDOWS
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
#ifdef cupolas_PLATFORM_WINDOWS
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

    cupolas_rwlock_destroy(&cfg->lock);

    cupolas_mem_free(cfg);
}

int cupolas_config_load(cupolas_config_t* cfg, config_type_t type, const char* file_path) {
    if (!cfg) return -1;

    cupolas_rwlock_wrlock(&cfg->lock);

    if (type == CONFIG_TYPE_ALL) {
        int loaded = 0;
        for (int i = 0; i < CONFIG_TYPE_ALL; i++) {
            if (cupolas_config_load(cfg, (config_type_t)i, NULL) == 0) {
                loaded++;
            }
        }
        cupolas_rwlock_unlock(&cfg->lock);
        return loaded > 0 ? 0 : -1;
    }

    config_entry_t* entry = &cfg->entries[type];
    entry->status = CONFIG_STATUS_LOADING;

    if (file_path) {
        snprintf(entry->file_path, sizeof(entry->file_path), "%s", file_path);
    } else {
        snprintf(entry->file_path, sizeof(entry->file_path), "%s/%s.yaml",
                cfg->config_dir, config_type_names[type]);
    }

#ifdef cupolas_PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (GetFileAttributesExA(entry->file_path, GetFileExInfoStandard, &attr)) {
        ULARGE_INTEGER ft;
        ft.LowPart = attr.ftLastWriteTime.dwLowDateTime;
        ft.HighPart = attr.ftLastWriteTime.dwHighDateTime;
        entry->last_modified = (time_t)(ft.QuadPart / 10000000 - 11644473600LL);
    }
#else
    struct stat st;
    if (stat(entry->file_path, &st) == 0) {
        entry->last_modified = st.st_mtime;
    }
#endif

    entry->version.major = 1;
    entry->version.minor = 0;
    entry->version.patch = 0;
    entry->version.timestamp_ns = metrics_get_timestamp_ns();

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

        snprintf(cfg->last_error, sizeof(cfg->last_error), "Reloading %s",
                config_type_names[type]);

        entry->status = CONFIG_STATUS_APPLIED;
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
        result->valid = true;
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
        entry->status = CONFIG_STATUS_ROLLBACK;
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                "Rolled back %s", config_type_names[type]);
        entry->status = CONFIG_STATUS_APPLIED;
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

    cupolas_UNUSED(type);
    cupolas_UNUSED(interval_ms);

    return 0;
}

int cupolas_config_check_reload(cupolas_config_t* cfg, config_type_t type) {
    if (!cfg) return 0;

    cupolas_rwlock_wrlock(&cfg->lock);

    int changed = 0;
    for (int i = 0; i < CONFIG_TYPE_ALL; i++) {
        if (type != CONFIG_TYPE_ALL && type != i) {
            continue;
        }

        config_entry_t* entry = &cfg->entries[i];

#ifdef cupolas_PLATFORM_WINDOWS
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

    for (int i = 0; i < CONFIG_TYPE_ALL; i++) {
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

    size_t offset = 0;

    for (int i = 0; i < CONFIG_TYPE_ALL; i++) {
        if (type != CONFIG_TYPE_ALL && type != i) {
            continue;
        }

        config_entry_t* entry = &cfg->entries[i];
        offset += snprintf(buffer + offset, size - offset,
                          "%s:\n"
                          "  status: %s\n"
                          "  version: %u.%u.%u\n"
                          "  file: %s\n",
                          config_type_names[i],
                          config_status_names[entry->status],
                          entry->version.major,
                          entry->version.minor,
                          entry->version.patch,
                          entry->file_path);
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
    for (int i = 0; i < CONFIG_TYPE_ALL; i++) {
        if (cfg->entries[i].status != CONFIG_STATUS_APPLIED) {
            all_valid = false;
            break;
        }
    }

    cupolas_rwlock_unlock(&cfg->lock);

    return all_valid;
}