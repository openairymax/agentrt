/**
 * @file heapstore_core.c
 * @brief AgentOS 数据分区核心实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "heapstore.h"
#include "private.h"
#include "utils.h"

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

static bool s_initialized = false;
static char s_root_path[MAX_PATH_LEN];
static heapstore_config_t s_config;
static heapstore_path_type_t s_path_order[] = {
    heapstore_PATH_KERNEL,
    heapstore_PATH_LOGS,
    heapstore_PATH_REGISTRY,
    heapstore_PATH_SERVICES,
    heapstore_PATH_TRACES,
    heapstore_PATH_KERNEL_IPC,
    heapstore_PATH_KERNEL_MEMORY
};

static const char* s_path_names[] = {
    "kernel",
    "logs",
    "registry",
    "services",
    "traces",
    "kernel/ipc",
    "kernel/memory"
};

static const char* s_subpath_map[][MAX_SUBPATHS] = {
    {NULL},
    {"apps", "kernel", "services", NULL},
    {NULL},
    {"llm_d", "market_d", "tool_d", NULL},
    {"spans", NULL},
    {"channels", "buffers", NULL},
    {"pools", "allocations", "stats", "index", "meta", "patterns", "raw", NULL}
};

static const char* s_default_root = "heapstore";

static void set_default_config(void) {
    memset(&s_config, 0, sizeof(s_config));
    s_config.root_path = s_default_root;
    s_config.max_log_size_mb = 100;
    s_config.log_retention_days = 7;
    s_config.trace_retention_days = 3;
    s_config.enable_auto_cleanup = true;
    s_config.enable_log_rotation = true;
    s_config.enable_trace_export = true;
    s_config.db_vacuum_interval_days = 7;
}

heapstore_error_t heapstore_init(const heapstore_config_t* manager) {
    if (s_initialized) {
        return heapstore_ERR_ALREADY_INITIALIZED;
    }

    set_default_config();

    if (manager && manager->root_path) {
        s_config.root_path = manager->root_path;
        if (manager->max_log_size_mb > 0) {
            s_config.max_log_size_mb = manager->max_log_size_mb;
        }
        if (manager->log_retention_days > 0) {
            s_config.log_retention_days = manager->log_retention_days;
        }
        if (manager->trace_retention_days > 0) {
            s_config.trace_retention_days = manager->trace_retention_days;
        }
        s_config.enable_auto_cleanup = manager->enable_auto_cleanup;
        s_config.enable_log_rotation = manager->enable_log_rotation;
        s_config.enable_trace_export = manager->enable_trace_export;
        if (manager->db_vacuum_interval_days > 0) {
            s_config.db_vacuum_interval_days = manager->db_vacuum_interval_days;
        }
    }

    strncpy(s_root_path, s_config.root_path, sizeof(s_root_path) - 1);
    s_root_path[sizeof(s_root_path) - 1] = '\0';

    if (!heapstore_ensure_directory(s_root_path)) {
        return heapstore_ERR_DIR_CREATE_FAILED;
    }

    for (size_t i = 0; i < sizeof(s_path_order) / sizeof(s_path_order[0]); i++) {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", s_root_path, s_path_names[i]);

        if (!heapstore_ensure_directory(full_path)) {
            return heapstore_ERR_DIR_CREATE_FAILED;
        }

        size_t subpath_idx = (size_t)s_path_order[i];
        if (subpath_idx < sizeof(s_subpath_map) / sizeof(s_subpath_map[0])) {
            for (size_t j = 0; s_subpath_map[subpath_idx][j] != NULL; j++) {
                char sub_path[MAX_PATH_LEN];
                snprintf(sub_path, sizeof(sub_path), "%s/%s", full_path, s_subpath_map[subpath_idx][j]);
                if (!heapstore_ensure_directory(sub_path)) {
                    return heapstore_ERR_DIR_CREATE_FAILED;
                }
            }
        }
    }

    s_initialized = true;

    heapstore_registry_init();
    heapstore_trace_init();
    heapstore_ipc_init();
    heapstore_memory_init();
    heapstore_log_init();

    return heapstore_SUCCESS;
}

void heapstore_shutdown(void) {
    if (s_initialized) {
        heapstore_log_shutdown();
        heapstore_trace_shutdown();
        heapstore_ipc_shutdown();
        heapstore_memory_shutdown();
        heapstore_registry_shutdown();
        s_initialized = false;
    }
}

bool heapstore_is_initialized(void) {
    return s_initialized;
}

const char* heapstore_get_root(void) {
    return s_root_path;
}

const char* heapstore_get_path(heapstore_path_type_t type) {
    if (type < 0 || type >= heapstore_PATH_MAX) {
        return NULL;
    }
    return s_path_names[type];
}

heapstore_error_t heapstore_get_full_path(heapstore_path_type_t type, char* buffer, size_t buffer_size) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!buffer || buffer_size == 0) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (type < 0 || type >= heapstore_PATH_MAX) {
        return heapstore_ERR_INVALID_PARAM;
    }

    snprintf(buffer, buffer_size, "%s/%s", s_root_path, s_path_names[type]);
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_get_stats(heapstore_stats_t* stats) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!stats) {
        return heapstore_ERR_INVALID_PARAM;
    }

    memset(stats, 0, sizeof(*stats));

    for (size_t i = 0; i < sizeof(s_path_order) / sizeof(s_path_order[0]); i++) {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", s_root_path, s_path_names[i]);

        uint64_t dir_size = 0;
        uint32_t file_count = 0;

        switch (s_path_order[i]) {
            case heapstore_PATH_LOGS:
                stats->log_usage_bytes += dir_size;
                stats->log_file_count += file_count;
                break;
            case heapstore_PATH_REGISTRY:
                stats->registry_usage_bytes += dir_size;
                break;
            case heapstore_PATH_TRACES:
                stats->trace_usage_bytes += dir_size;
                stats->trace_file_count += file_count;
                break;
            case heapstore_PATH_KERNEL_IPC:
                stats->ipc_usage_bytes += dir_size;
                break;
            case heapstore_PATH_KERNEL_MEMORY:
                stats->memory_usage_bytes += dir_size;
                break;
            default:
                break;
        }
        stats->total_disk_usage_bytes += dir_size;
    }

    return heapstore_SUCCESS;
}

/**
 * @brief 计算文件年龄（天数）
 *
 * @note 此函数目前未被使用，保留作为未来日志清理功能的预留接口
 *       计划在 heapstore_log_cleanup 完整实现时使用
 *
 * @param filepath 文件路径
 * @return uint64_t 文件年龄（天数），如果文件不存在或获取失败返回 0
 */
static uint64_t get_file_age_days(const char* filepath) {
    struct stat st;
    if (stat(filepath, &st) != 0) {
        return 0;
    }

    time_t now = time(NULL);
    double seconds = difftime(now, st.st_mtime);
    return (uint64_t)(seconds / 86400.0);
}

heapstore_error_t heapstore_cleanup(bool dry_run, uint64_t* freed_bytes) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!s_config.enable_auto_cleanup) {
        return heapstore_SUCCESS;
    }

    if (freed_bytes) {
        *freed_bytes = 0;
    }

    char log_path[MAX_PATH_LEN];
    snprintf(log_path, sizeof(log_path), "%s/%s", s_root_path, s_path_names[heapstore_PATH_LOGS]);

    char trace_path[MAX_PATH_LEN];
    snprintf(trace_path, sizeof(trace_path), "%s/%s", s_root_path, s_path_names[heapstore_PATH_TRACES]);

    if (!dry_run && freed_bytes) {
        *freed_bytes = 0;
    }

    return heapstore_SUCCESS;
}

const char* heapstore_strerror(heapstore_error_t err) {
    switch (err) {
        case heapstore_SUCCESS:
            return "Success";
        case heapstore_ERR_INVALID_PARAM:
            return "Invalid parameter";
        case heapstore_ERR_NOT_INITIALIZED:
            return "heapstore not initialized";
        case heapstore_ERR_ALREADY_INITIALIZED:
            return "heapstore already initialized";
        case heapstore_ERR_DIR_CREATE_FAILED:
            return "Failed to create directory";
        case heapstore_ERR_DIR_NOT_FOUND:
            return "Directory not found";
        case heapstore_ERR_PERMISSION_DENIED:
            return "Permission denied";
        case heapstore_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case heapstore_ERR_DB_INIT_FAILED:
            return "Database initialization failed";
        case heapstore_ERR_DB_QUERY_FAILED:
            return "Database query failed";
        case heapstore_ERR_FILE_OPEN_FAILED:
            return "Failed to open file";
        case heapstore_ERR_CONFIG_INVALID:
            return "Invalid configuration";
        case heapstore_ERR_INTERNAL:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

heapstore_error_t heapstore_reload_config(const heapstore_config_t* manager) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!manager) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (manager->max_log_size_mb > 0) {
        s_config.max_log_size_mb = manager->max_log_size_mb;
    }
    if (manager->log_retention_days > 0) {
        s_config.log_retention_days = manager->log_retention_days;
    }
    if (manager->trace_retention_days > 0) {
        s_config.trace_retention_days = manager->trace_retention_days;
    }
    s_config.enable_auto_cleanup = manager->enable_auto_cleanup;
    s_config.enable_log_rotation = manager->enable_log_rotation;
    s_config.enable_trace_export = manager->enable_trace_export;
    if (manager->db_vacuum_interval_days > 0) {
        s_config.db_vacuum_interval_days = manager->db_vacuum_interval_days;
    }

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_flush(void) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_health_check(bool* registry_ok, 
                                       bool* trace_ok, 
                                       bool* log_ok, 
                                       bool* ipc_ok, 
                                       bool* memory_ok) {
    bool all_healthy = true;
    
    if (registry_ok) {
        *registry_ok = s_initialized;
        if (!*registry_ok) {
            all_healthy = false;
        }
    }
    
    if (trace_ok) {
        *trace_ok = s_initialized;
        if (!*trace_ok) {
            all_healthy = false;
        }
    }
    
    if (log_ok) {
        *log_ok = s_initialized;
        if (!*log_ok) {
            all_healthy = false;
        }
    }
    
    if (ipc_ok) {
        *ipc_ok = s_initialized;
        if (!*ipc_ok) {
            all_healthy = false;
        }
    }
    
    if (memory_ok) {
        *memory_ok = s_initialized;
        if (!*memory_ok) {
            all_healthy = false;
        }
    }
    
    return all_healthy ? heapstore_SUCCESS : heapstore_ERR_NOT_INITIALIZED;
}
