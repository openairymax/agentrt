/**
 * @file partdata_core.c
 * @brief AgentOS 数据分区核心实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "partdata.h"
#include "private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#define stat _stat
#define S_ISDIR(m) (((m) & _S_IFDIR) == _S_IFDIR)
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/resource.h>
#endif

#define MAX_PATH_LEN 512
#define MAX_SUBPATHS 32

static bool g_initialized = false;
static char g_root_path[MAX_PATH_LEN];
static partdata_config_t g_config;
static partdata_path_type_t g_path_order[] = {
    PARTDATA_PATH_KERNEL,
    PARTDATA_PATH_LOGS,
    PARTDATA_PATH_REGISTRY,
    PARTDATA_PATH_SERVICES,
    PARTDATA_PATH_TRACES,
    PARTDATA_PATH_KERNEL_IPC,
    PARTDATA_PATH_KERNEL_MEMORY
};

static const char* g_path_names[] = {
    "kernel",
    "logs",
    "registry",
    "services",
    "traces",
    "kernel/ipc",
    "kernel/memory"
};

static const char* g_subpath_map[][MAX_SUBPATHS] = {
    {NULL},
    {"apps", "kernel", "services", NULL},
    {NULL},
    {"llm_d", "market_d", "tool_d", NULL},
    {"spans", NULL},
    {"channels", "buffers", NULL},
    {"pools", "allocations", "stats", "index", "meta", "patterns", "raw", NULL}
};

static const char* g_default_root = "partdata";

static bool ensure_dir_recursive(const char* path) {
    if (!path || strlen(path) == 0) {
        return false;
    }

    char path_copy[MAX_PATH_LEN];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    size_t len = strlen(path_copy);
    for (size_t i = 0; i < len; i++) {
        if (path_copy[i] == '\\' || path_copy[i] == '/') {
            if (i > 0 && path_copy[i - 1] != ':') {
                path_copy[i] = '\0';
                if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
                    if (!S_ISDIR(stat(path_copy, &(struct stat){0}).st_mode)) {
                        return false;
                    }
                }
                path_copy[i] = '/';
            }
        }
    }

    if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
        struct stat st;
        if (stat(path_copy, &st) != 0 || !S_ISDIR(st.st_mode)) {
            return false;
        }
    }

    return true;
}

static void set_default_config(void) {
    memset(&g_config, 0, sizeof(g_config));
    g_config.root_path = g_default_root;
    g_config.max_log_size_mb = 100;
    g_config.log_retention_days = 7;
    g_config.trace_retention_days = 3;
    g_config.enable_auto_cleanup = true;
    g_config.enable_log_rotation = true;
    g_config.enable_trace_export = true;
    g_config.db_vacuum_interval_days = 7;
}

partdata_error_t partdata_init(const partdata_config_t* config) {
    if (g_initialized) {
        return PARTDATA_ERR_ALREADY_INITIALIZED;
    }

    set_default_config();

    if (config && config->root_path) {
        g_config.root_path = config->root_path;
        if (config->max_log_size_mb > 0) {
            g_config.max_log_size_mb = config->max_log_size_mb;
        }
        if (config->log_retention_days > 0) {
            g_config.log_retention_days = config->log_retention_days;
        }
        if (config->trace_retention_days > 0) {
            g_config.trace_retention_days = config->trace_retention_days;
        }
        g_config.enable_auto_cleanup = config->enable_auto_cleanup;
        g_config.enable_log_rotation = config->enable_log_rotation;
        g_config.enable_trace_export = config->enable_trace_export;
        if (config->db_vacuum_interval_days > 0) {
            g_config.db_vacuum_interval_days = config->db_vacuum_interval_days;
        }
    }

    strncpy(g_root_path, g_config.root_path, sizeof(g_root_path) - 1);
    g_root_path[sizeof(g_root_path) - 1] = '\0';

    if (!ensure_dir_recursive(g_root_path)) {
        return PARTDATA_ERR_DIR_CREATE_FAILED;
    }

    for (size_t i = 0; i < sizeof(g_path_order) / sizeof(g_path_order[0]); i++) {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", g_root_path, g_path_names[i]);

        if (!ensure_dir_recursive(full_path)) {
            return PARTDATA_ERR_DIR_CREATE_FAILED;
        }

        size_t subpath_idx = (size_t)g_path_order[i];
        if (subpath_idx < sizeof(g_subpath_map) / sizeof(g_subpath_map[0])) {
            for (size_t j = 0; g_subpath_map[subpath_idx][j] != NULL; j++) {
                char sub_path[MAX_PATH_LEN];
                snprintf(sub_path, sizeof(sub_path), "%s/%s", full_path, g_subpath_map[subpath_idx][j]);
                if (!ensure_dir_recursive(sub_path)) {
                    return PARTDATA_ERR_DIR_CREATE_FAILED;
                }
            }
        }
    }

    g_initialized = true;

    partdata_registry_init();
    partdata_trace_init();
    partdata_ipc_init();
    partdata_memory_init();
    partdata_log_init();

    return PARTDATA_SUCCESS;
}

void partdata_shutdown(void) {
    if (g_initialized) {
        partdata_log_shutdown();
        partdata_trace_shutdown();
        partdata_ipc_shutdown();
        partdata_memory_shutdown();
        partdata_registry_shutdown();
        g_initialized = false;
    }
}

bool partdata_is_initialized(void) {
    return g_initialized;
}

const char* partdata_get_root(void) {
    return g_root_path;
}

const char* partdata_get_path(partdata_path_type_t type) {
    if (type < 0 || type >= PARTDATA_PATH_MAX) {
        return NULL;
    }
    return g_path_names[type];
}

partdata_error_t partdata_get_full_path(partdata_path_type_t type, char* buffer, size_t buffer_size) {
    if (!g_initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    if (!buffer || buffer_size == 0) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (type < 0 || type >= PARTDATA_PATH_MAX) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    snprintf(buffer, buffer_size, "%s/%s", g_root_path, g_path_names[type]);
    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_get_stats(partdata_stats_t* stats) {
    if (!g_initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    if (!stats) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(*stats));

    for (size_t i = 0; i < sizeof(g_path_order) / sizeof(g_path_order[0]); i++) {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", g_root_path, g_path_names[i]);

        uint64_t dir_size = 0;
        uint32_t file_count = 0;

        switch (g_path_order[i]) {
            case PARTDATA_PATH_LOGS:
                stats->log_usage_bytes += dir_size;
                stats->log_file_count += file_count;
                break;
            case PARTDATA_PATH_REGISTRY:
                stats->registry_usage_bytes += dir_size;
                break;
            case PARTDATA_PATH_TRACES:
                stats->trace_usage_bytes += dir_size;
                stats->trace_file_count += file_count;
                break;
            case PARTDATA_PATH_KERNEL_IPC:
                stats->ipc_usage_bytes += dir_size;
                break;
            case PARTDATA_PATH_KERNEL_MEMORY:
                stats->memory_usage_bytes += dir_size;
                break;
            default:
                break;
        }
        stats->total_disk_usage_bytes += dir_size;
    }

    return PARTDATA_SUCCESS;
}

static uint64_t get_file_age_days(const char* filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return 0;
    }

    time_t now = time(NULL);
    double seconds = difftime(now, st.st_mtime);
    return (uint64_t)(seconds / 86400.0);
}

partdata_error_t partdata_cleanup(bool dry_run, uint64_t* freed_bytes) {
    if (!g_initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    if (!g_config.enable_auto_cleanup) {
        return PARTDATA_SUCCESS;
    }

    if (freed_bytes) {
        *freed_bytes = 0;
    }

    char log_path[MAX_PATH_LEN];
    snprintf(log_path, sizeof(log_path), "%s/%s", g_root_path, g_path_names[PARTDATA_PATH_LOGS]);

    char trace_path[MAX_PATH_LEN];
    snprintf(trace_path, sizeof(trace_path), "%s/%s", g_root_path, g_path_names[PARTDATA_PATH_TRACES]);

    if (!dry_run && freed_bytes) {
        *freed_bytes = 0;
    }

    return PARTDATA_SUCCESS;
}

const char* partdata_strerror(partdata_error_t err) {
    switch (err) {
        case PARTDATA_SUCCESS:
            return "Success";
        case PARTDATA_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case PARTDATA_ERR_NOT_INITIALIZED:
            return "Partdata not initialized";
        case PARTDATA_ERR_ALREADY_INITIALIZED:
            return "Partdata already initialized";
        case PARTDATA_ERR_DIR_CREATE_FAILED:
            return "Failed to create directory";
        case PARTDATA_ERR_DIR_NOT_FOUND:
            return "Directory not found";
        case PARTDATA_ERR_PERMISSION_DENIED:
            return "Permission denied";
        case PARTDATA_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case PARTDATA_ERR_DB_INIT_FAILED:
            return "Database initialization failed";
        case PARTDATA_ERR_DB_QUERY_FAILED:
            return "Database query failed";
        case PARTDATA_ERR_FILE_OPEN_FAILED:
            return "Failed to open file";
        case PARTDATA_ERR_CONFIG_INVALID:
            return "Invalid configuration";
        case PARTDATA_ERR_INTERNAL:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

partdata_error_t partdata_reload_config(const partdata_config_t* config) {
    if (!g_initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }

    if (!config) {
        return PARTDATA_ERR_INVALID_PARAM;
    }

    if (config->max_log_size_mb > 0) {
        g_config.max_log_size_mb = config->max_log_size_mb;
    }
    if (config->log_retention_days > 0) {
        g_config.log_retention_days = config->log_retention_days;
    }
    if (config->trace_retention_days > 0) {
        g_config.trace_retention_days = config->trace_retention_days;
    }
    g_config.enable_auto_cleanup = config->enable_auto_cleanup;
    g_config.enable_log_rotation = config->enable_log_rotation;
    g_config.enable_trace_export = config->enable_trace_export;
    if (config->db_vacuum_interval_days > 0) {
        g_config.db_vacuum_interval_days = config->db_vacuum_interval_days;
    }

    return PARTDATA_SUCCESS;
}

partdata_error_t partdata_flush(void) {
    if (!g_initialized) {
        return PARTDATA_ERR_NOT_INITIALIZED;
    }
    return PARTDATA_SUCCESS;
}
