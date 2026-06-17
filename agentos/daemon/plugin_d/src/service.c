/**
 * @file service.c
 * @brief Plugin 服务实现骨架
 *
 * @owner team-A
 */

#include "plugin_service.h"

#include <string.h>

/* ── 插件注册表 ── */

#define PLUGIN_MAX_LOADED 32

static plugin_descriptor_t g_plugins[PLUGIN_MAX_LOADED];
static size_t g_plugin_count = 0;

/* ── 服务 API 实现 ── */

int plugin_service_load(const char *library_path, const char *config_path,
                        const char **out_name) {
    if (!library_path) return -1;
    if (g_plugin_count >= PLUGIN_MAX_LOADED) return -1;

    /* TODO: Phase 2 实现 - dlopen/dlsym 加载动态库 */
    plugin_descriptor_t *desc = &g_plugins[g_plugin_count];
    memset(desc, 0, sizeof(*desc));
    strncpy(desc->library_path, library_path, sizeof(desc->library_path) - 1);
    if (config_path) {
        strncpy(desc->config_path, config_path, sizeof(desc->config_path) - 1);
    }
    desc->state = PLUGIN_STATE_LOADED;

    if (out_name) *out_name = desc->metadata.name;
    g_plugin_count++;
    return 0;
}

int plugin_service_unload(const char *name) {
    if (!name) return -1;

    for (size_t i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugins[i].metadata.name, name) == 0) {
            /* TODO: Phase 2 实现 - 调用 destroy, dlclose */
            g_plugins[i] = g_plugins[g_plugin_count - 1];
            g_plugin_count--;
            return 0;
        }
    }
    return -1;
}

int plugin_service_start(const char *name) {
    if (!name) return -1;

    for (size_t i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugins[i].metadata.name, name) == 0) {
            if (g_plugins[i].start) {
                return g_plugins[i].start(g_plugins[i].user_data);
            }
            g_plugins[i].state = PLUGIN_STATE_RUNNING;
            return 0;
        }
    }
    return -1;
}

int plugin_service_stop(const char *name) {
    if (!name) return -1;

    for (size_t i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugins[i].metadata.name, name) == 0) {
            if (g_plugins[i].stop) {
                return g_plugins[i].stop(g_plugins[i].user_data);
            }
            g_plugins[i].state = PLUGIN_STATE_LOADED;
            return 0;
        }
    }
    return -1;
}

int plugin_service_get_metadata(const char *name, plugin_metadata_t *metadata) {
    if (!name || !metadata) return -1;

    for (size_t i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugins[i].metadata.name, name) == 0) {
            *metadata = g_plugins[i].metadata;
            return 0;
        }
    }
    return -1;
}

plugin_state_t plugin_service_get_state(const char *name) {
    if (!name) return PLUGIN_STATE_UNLOADED;

    for (size_t i = 0; i < g_plugin_count; i++) {
        if (strcmp(g_plugins[i].metadata.name, name) == 0) {
            return g_plugins[i].state;
        }
    }
    return PLUGIN_STATE_UNLOADED;
}

int plugin_service_get_stats(const char *name, plugin_stats_t *stats) {
    (void)name;
    if (!stats) return -1;
    memset(stats, 0, sizeof(*stats));
    /* TODO: Phase 2 实现 - 收集实际统计数据 */
    return 0;
}

int plugin_service_list(char ***names, size_t *count, int type_filter) {
    if (!names || !count) return -1;

    size_t total = 0;
    for (size_t i = 0; i < g_plugin_count; i++) {
        if (type_filter < 0 || (int)g_plugins[i].metadata.type == type_filter) {
            total++;
        }
    }

    *count = total;
    if (total == 0) {
        *names = NULL;
        return 0;
    }

    /* TODO: Phase 2 实现 - 分配并填充名称数组 */
    *names = NULL;
    return 0;
}
