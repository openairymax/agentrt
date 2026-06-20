/**
 * @file service.c
 * @brief P2.2: Plugin 服务实现 — 动态加载/卸载/生命周期管理
 *
 * 使用 dlopen/dlsym 加载动态库插件，管理插件生命周期。
 * 线程安全的注册表，支持 4 种插件类型。
 *
 * 插件入口点约定：
 *   - plugin_metadata_fn()  → 返回 plugin_metadata_t
 *   - plugin_init_fn()      → 初始化
 *   - plugin_destroy_fn()   → 销毁
 *   - plugin_start_fn()     → 启动
 *   - plugin_stop_fn()      → 停止
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "plugin_service.h"
#include "platform.h"
#include "svc_logger.h"
#include "sync_compat.h"
#include "memory_compat.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ==================== 常量 ==================== */

#define PLUGIN_MAX_COUNT     64     /**< 最大插件数 */
#define PLUGIN_NAME_MAX_LEN  64     /**< 插件名称最大长度 */

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 插件注册表节点
 */
typedef struct plugin_node {
    plugin_descriptor_t desc;        /**< 插件描述符 */
    plugin_stats_t stats;            /**< 插件统计 */
    struct timespec load_time;       /**< 加载时间 */
    struct plugin_node *next;       /**< 下一个节点 */
} plugin_node_t;

/**
 * @brief 全局插件注册表
 */
static struct {
    plugin_node_t *head;             /**< 链表头 */
    size_t count;                    /**< 插件总数 */
    sync_rwlock_t rwlock;            /**< 读写锁 */
    bool initialized;                /**< 是否已初始化 */
} g_plugin_registry;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 按名称查找插件节点
 * @return 节点指针，未找到返回 NULL
 */
static plugin_node_t *find_node(const char *name)
{
    if (!name) return NULL;
    plugin_node_t *node = g_plugin_registry.head;
    while (node) {
        if (strcmp(node->desc.metadata.name, name) == 0)
            return node;
        node = node->next;
    }
    return NULL;
}

/**
 * @brief 从动态库加载符号
 */
static void *load_symbol(void *handle, const char *name)
{
    void *sym = dlsym(handle, name);
    if (!sym) {
        SVC_LOG_ERROR("P2.2: PluginD: Symbol not found: %s (%s)", name, dlerror());
    }
    return sym;
}

/**
 * @brief 初始化插件注册表
 */
static int registry_init(void)
{
    if (g_plugin_registry.initialized)
        return 0;

    AGENTOS_MEMSET(&g_plugin_registry, 0, sizeof(g_plugin_registry));
    if (AGENTOS_RWLOCK_INIT(&g_plugin_registry.rwlock, NULL) != 0)
        return -1;

    g_plugin_registry.initialized = true;
    return 0;
}

/* ==================== 服务 API 实现 ==================== */

int plugin_service_load(const char *library_path, const char *config_path,
                        const char **out_name)
{
    if (!library_path) return -1;

    /* 惰性初始化注册表 */
    if (registry_init() != 0) return -1;

    /* 打开动态库 */
    void *handle = dlopen(library_path, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        SVC_LOG_ERROR("P2.2: PluginD: dlopen failed: %s (%s)", library_path, dlerror());
        return -1;
    }

    /* 加载元数据 */
    typedef const plugin_metadata_t *(*metadata_fn_t)(void);
    metadata_fn_t get_metadata = (metadata_fn_t)load_symbol(handle, "plugin_get_metadata");
    if (!get_metadata) {
        dlclose(handle);
        return -1;
    }

    const plugin_metadata_t *metadata = get_metadata();
    if (!metadata || !metadata->name[0]) {
        SVC_LOG_ERROR("P2.2: PluginD: Invalid plugin metadata");
        dlclose(handle);
        return -1;
    }

    /* 检查重名 */
    AGENTOS_RWLOCK_WRLOCK(&g_plugin_registry.rwlock);
    if (find_node(metadata->name)) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        SVC_LOG_WARN("P2.2: PluginD: Plugin already loaded: %s", metadata->name);
        dlclose(handle);
        return -1;
    }
    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);

    /* 加载入口点 */
    plugin_init_fn init_fn = (plugin_init_fn)load_symbol(handle, "plugin_init");
    plugin_destroy_fn destroy_fn = (plugin_destroy_fn)load_symbol(handle, "plugin_destroy");
    plugin_start_fn start_fn = (plugin_start_fn)load_symbol(handle, "plugin_start");
    plugin_stop_fn stop_fn = (plugin_stop_fn)load_symbol(handle, "plugin_stop");

    if (!init_fn || !destroy_fn) {
        SVC_LOG_ERROR("P2.2: PluginD: Missing required symbols (init/destroy) for %s",
                      metadata->name);
        dlclose(handle);
        return -1;
    }

    /* 分配插件节点 */
    plugin_node_t *node = (plugin_node_t *)AGENTOS_CALLOC(1, sizeof(plugin_node_t));
    if (!node) {
        dlclose(handle);
        return -1;
    }

    /* 填充描述符 */
    memcpy(&node->desc.metadata, metadata, sizeof(plugin_metadata_t));
    node->desc.init      = init_fn;
    node->desc.destroy   = destroy_fn;
    node->desc.start     = start_fn;
    node->desc.stop      = stop_fn;
    node->desc.handle    = handle;
    node->desc.user_data = NULL;
    node->desc.state     = PLUGIN_STATE_LOADED;

    AGENTOS_STRNCPY_TERM(node->desc.library_path, library_path,
                         sizeof(node->desc.library_path));
    if (config_path) {
        AGENTOS_STRNCPY_TERM(node->desc.config_path, config_path,
                             sizeof(node->desc.config_path));
    }

    /* 初始化插件 */
    clock_gettime(CLOCK_MONOTONIC, &node->load_time);
    void *user_data = NULL;
    int init_ret = init_fn(config_path, &user_data);
    if (init_ret != 0) {
        SVC_LOG_ERROR("P2.2: PluginD: Plugin init failed: %s (err=%d)",
                      metadata->name, init_ret);
        node->desc.state = PLUGIN_STATE_ERROR;
        node->stats.error_count++;
        /* 继续添加，允许用户重试 */
    } else {
        node->desc.user_data = user_data;
        node->desc.state = PLUGIN_STATE_INITIALIZED;
    }

    node->stats.load_count++;

    /* 插入注册表 */
    AGENTOS_RWLOCK_WRLOCK(&g_plugin_registry.rwlock);
    node->next = g_plugin_registry.head;
    g_plugin_registry.head = node;
    g_plugin_registry.count++;
    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);

    if (out_name) {
        *out_name = metadata->name;
    }

    SVC_LOG_INFO("P2.2: PluginD: Plugin loaded: %s v%s (type=%d, state=%d)",
               metadata->name, metadata->version, metadata->type, node->desc.state);

    return 0;
}

int plugin_service_unload(const char *name)
{
    if (!name) return -1;

    AGENTOS_RWLOCK_WRLOCK(&g_plugin_registry.rwlock);

    plugin_node_t **prev = &g_plugin_registry.head;
    while (*prev) {
        plugin_node_t *node = *prev;
        if (strcmp(node->desc.metadata.name, name) == 0) {
            /* 先停止 */
            if (node->desc.state == PLUGIN_STATE_RUNNING && node->desc.stop) {
                node->desc.stop(node->desc.user_data);
            }

            /* 销毁 */
            if (node->desc.destroy) {
                node->desc.destroy(node->desc.user_data);
            }

            /* 关闭动态库 */
            if (node->desc.handle) {
                dlclose(node->desc.handle);
            }

            *prev = node->next;
            AGENTOS_FREE(node);
            g_plugin_registry.count--;

            AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
            SVC_LOG_INFO("P2.2: PluginD: Plugin unloaded: %s", name);
            return 0;
        }
        prev = &(*prev)->next;
    }

    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
    return -1;
}

int plugin_service_start(const char *name)
{
    if (!name) return -1;

    AGENTOS_RWLOCK_WRLOCK(&g_plugin_registry.rwlock);
    plugin_node_t *node = find_node(name);
    if (!node) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        return -1;
    }

    if (node->desc.state == PLUGIN_STATE_RUNNING) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        return 0;  /* 已在运行 */
    }

    if (node->desc.state == PLUGIN_STATE_ERROR) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        return -1;
    }

    if (node->desc.start) {
        int ret = node->desc.start(node->desc.user_data);
        if (ret != 0) {
            node->desc.state = PLUGIN_STATE_ERROR;
            node->stats.error_count++;
            AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
            SVC_LOG_ERROR("P2.2: PluginD: Plugin start failed: %s (err=%d)", name, ret);
            return -1;
        }
    }

    node->desc.state = PLUGIN_STATE_RUNNING;
    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);

    SVC_LOG_INFO("P2.2: PluginD: Plugin started: %s", name);
    return 0;
}

int plugin_service_stop(const char *name)
{
    if (!name) return -1;

    AGENTOS_RWLOCK_WRLOCK(&g_plugin_registry.rwlock);
    plugin_node_t *node = find_node(name);
    if (!node) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        return -1;
    }

    if (node->desc.state != PLUGIN_STATE_RUNNING) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        return 0;
    }

    if (node->desc.stop) {
        node->desc.stop(node->desc.user_data);
    }

    node->desc.state = PLUGIN_STATE_INITIALIZED;

    /* 更新运行时间统计 */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    node->stats.uptime_ns +=
        (uint64_t)(now.tv_sec - node->load_time.tv_sec) * 1000000000ULL
        + (uint64_t)(now.tv_nsec - node->load_time.tv_nsec);

    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);

    SVC_LOG_INFO("P2.2: PluginD: Plugin stopped: %s", name);
    return 0;
}

int plugin_service_get_metadata(const char *name, plugin_metadata_t *metadata)
{
    if (!name || !metadata) return -1;

    AGENTOS_RWLOCK_RDLOCK(&g_plugin_registry.rwlock);
    plugin_node_t *node = find_node(name);
    if (!node) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        return -1;
    }

    memcpy(metadata, &node->desc.metadata, sizeof(plugin_metadata_t));
    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
    return 0;
}

plugin_state_t plugin_service_get_state(const char *name)
{
    if (!name) return PLUGIN_STATE_UNLOADED;

    AGENTOS_RWLOCK_RDLOCK(&g_plugin_registry.rwlock);
    plugin_node_t *node = find_node(name);
    plugin_state_t state = node ? node->desc.state : PLUGIN_STATE_UNLOADED;
    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
    return state;
}

int plugin_service_get_stats(const char *name, plugin_stats_t *stats)
{
    if (!name || !stats) return -1;

    AGENTOS_RWLOCK_RDLOCK(&g_plugin_registry.rwlock);
    plugin_node_t *node = find_node(name);
    if (!node) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        return -1;
    }

    memcpy(stats, &node->stats, sizeof(plugin_stats_t));

    /* 如果正在运行，更新 uptime */
    if (node->desc.state == PLUGIN_STATE_RUNNING) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        stats->uptime_ns +=
            (uint64_t)(now.tv_sec - node->load_time.tv_sec) * 1000000000ULL
            + (uint64_t)(now.tv_nsec - node->load_time.tv_nsec);
    }

    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
    return 0;
}

int plugin_service_list(char ***names, size_t *count, int type_filter)
{
    if (!names || !count) return -1;

    AGENTOS_RWLOCK_RDLOCK(&g_plugin_registry.rwlock);

    /* 先计数 */
    size_t total = 0;
    plugin_node_t *node = g_plugin_registry.head;
    while (node) {
        if (type_filter < 0 || (int)node->desc.metadata.type == type_filter) {
            total++;
        }
        node = node->next;
    }

    /* 分配名称数组 */
    char **name_array = (char **)AGENTOS_CALLOC(total, sizeof(char *));
    if (!name_array && total > 0) {
        AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);
        return -1;
    }

    /* 填充名称 */
    size_t idx = 0;
    node = g_plugin_registry.head;
    while (node && idx < total) {
        if (type_filter < 0 || (int)node->desc.metadata.type == type_filter) {
            name_array[idx] = AGENTOS_STRDUP(node->desc.metadata.name);
            idx++;
        }
        node = node->next;
    }

    AGENTOS_RWLOCK_UNLOCK(&g_plugin_registry.rwlock);

    *names = name_array;
    *count = total;
    return 0;
}