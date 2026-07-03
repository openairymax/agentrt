/**
 * @file plugin_discovery.c
 * @brief P2.2.1: 插件发现实现 — 扫描目录 + 解析 manifest.yaml
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 扫描 ecosystem/plugins/ 目录下的每个子目录，
 * 查找并解析 manifest.yaml，提取插件元数据。
 */

#include "plugin_discovery.h"
#include "plugin_service.h"
#include "safe_string_utils.h"

#include "logger.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef _WIN32
#include <dirent.h>   /* opendir/readdir/closedir（仅 POSIX） */
#else
#include <windows.h>  /* FindFirstFile/FindNextFile/FindClose（Windows 目录扫描） */
#endif

/* ==================== 默认配置 ==================== */

#define DEFAULT_PLUGINS_DIR  "ecosystem/plugins/"
#define DEFAULT_SCAN_DEPTH   1

/* ==================== 内部状态 ==================== */

static struct {
    plugin_discovery_config_t config;
    char plugins_dir[PLUGIN_DISCOVERY_MAX_PATH];
    plugin_discovery_result_t *results;
    size_t count;
    bool initialized;
} g_discovery;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 简单 YAML 键值解析（不依赖第三方库）
 *
 * 仅解析 manifest.yaml 中的简单键值对。
 * 格式: key: value
 */
static char *parse_yaml_value(const char *line, const char *key)
{
    if (!line || !key) return NULL;

    /* 跳过前导空格 */
    while (*line == ' ' || *line == '\t') line++;

    size_t key_len = strlen(key);
    if (strncmp(line, key, key_len) != 0) return NULL;

    const char *rest = line + key_len;
    /* 跳过冒号和空格 */
    while (*rest == ' ' || *rest == ':') rest++;

    /* 复制值（去除尾部换行和空格） */
    size_t val_len = strlen(rest);
    while (val_len > 0 && (rest[val_len - 1] == '\n' ||
                           rest[val_len - 1] == '\r' ||
                           rest[val_len - 1] == ' ')) {
        val_len--;
    }

    if (val_len == 0) return NULL;

    char *result = (char *)AGENTOS_MALLOC(val_len + 1);
    if (!result) return NULL;

    AGENTOS_MEMCPY(result, rest, val_len);
    result[val_len] = '\0';
    return result;
}

/**
 * @brief 解析插件类型字符串
 */
static plugin_type_t parse_plugin_type(const char *type_str)
{
    if (!type_str) return PLUGIN_TYPE_TOOL_PROVIDER;

    if (strcmp(type_str, "tool_provider") == 0) return PLUGIN_TYPE_TOOL_PROVIDER;
    if (strcmp(type_str, "protocol_adapter") == 0) return PLUGIN_TYPE_PROTOCOL_ADAPTER;
    if (strcmp(type_str, "memory_provider") == 0) return PLUGIN_TYPE_MEMORY_PROVIDER;
    if (strcmp(type_str, "hook_extension") == 0) return PLUGIN_TYPE_HOOK_EXTENSION;

    return PLUGIN_TYPE_TOOL_PROVIDER;
}

/**
 * @brief 检查目录是否存在
 */
static bool dir_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/**
 * @brief 检查文件是否存在
 */
static bool file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

/* ==================== 生命周期实现 ==================== */

int plugin_discovery_init(const plugin_discovery_config_t *config)
{
    if (g_discovery.initialized) return 0;

    __builtin_memset(&g_discovery, 0, sizeof(g_discovery));

    if (config) {
        g_discovery.config = *config;
        if (config->plugins_dir) {
            safe_strcpy(g_discovery.plugins_dir, config->plugins_dir,
                        sizeof(g_discovery.plugins_dir));
        }
    } else {
        g_discovery.config.auto_load = false;
        g_discovery.config.fail_on_invalid = false;
        g_discovery.config.scan_depth = DEFAULT_SCAN_DEPTH;
    }

    if (g_discovery.plugins_dir[0] == '\0') {
        safe_strcpy(g_discovery.plugins_dir, DEFAULT_PLUGINS_DIR,
                    sizeof(g_discovery.plugins_dir));
    }

    g_discovery.initialized = true;

    AGENTOS_LOG_INFO("PluginDiscovery: initialized (dir=%s, auto_load=%d, "
                     "scan_depth=%u)",
                     g_discovery.plugins_dir,
                     g_discovery.config.auto_load,
                     g_discovery.config.scan_depth);
    return 0;
}

void plugin_discovery_destroy(void)
{
    if (g_discovery.results) {
        plugin_discovery_free_results(g_discovery.results, g_discovery.count);
        g_discovery.results = NULL;
        g_discovery.count = 0;
    }

    AGENTOS_LOG_INFO("PluginDiscovery: destroyed");
    g_discovery.initialized = false;
}

/* ==================== 解析 manifest.yaml ==================== */

int plugin_discovery_parse_manifest(const char *yaml_path,
                                    const char *plugin_dir,
                                    plugin_discovery_result_t *out_result)
{
    if (!yaml_path || !out_result) return -1;

    __builtin_memset(out_result, 0, sizeof(*out_result));

    FILE *fp = fopen(yaml_path, "r");
    if (!fp) {
        AGENTOS_LOG_WARN("PluginDiscovery: cannot open manifest '%s'", yaml_path);
        out_result->valid = false;
        safe_strcpy(out_result->error_reason, "Cannot open manifest file",
                    sizeof(out_result->error_reason));
        return -1;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *value = NULL;

        /* 解析基础字段 */
        if ((value = parse_yaml_value(line, "name"))) {
            safe_strcpy(out_result->name, value, sizeof(out_result->name));
        } else if ((value = parse_yaml_value(line, "version"))) {
            safe_strcpy(out_result->version, value, sizeof(out_result->version));
        } else if ((value = parse_yaml_value(line, "author"))) {
            safe_strcpy(out_result->author, value, sizeof(out_result->author));
        } else if ((value = parse_yaml_value(line, "description"))) {
            safe_strcpy(out_result->description, value,
                        sizeof(out_result->description));
        } else if ((value = parse_yaml_value(line, "type"))) {
            out_result->type = parse_plugin_type(value);
        } else if ((value = parse_yaml_value(line, "api_version"))) {
            out_result->api_version = (uint32_t)atoi(value);
        } else if ((value = parse_yaml_value(line, "min_agentos_version"))) {
            out_result->min_agentos_version = (uint32_t)atoi(value);
        } else if ((value = parse_yaml_value(line, "library"))) {
            /* 构建完整路径 */
            if (plugin_dir) {
                snprintf(out_result->library_path,
                         sizeof(out_result->library_path),
                         "%s/%s", plugin_dir, value);
            } else {
                safe_strcpy(out_result->library_path, value,
                            sizeof(out_result->library_path));
            }
        } else if (strncmp(line, "  - ", 4) == 0) {
            /* 解析权限列表项 */
            char *perm = parse_yaml_value(line, "-");
            if (perm && out_result->permission_count < PLUGIN_DISCOVERY_MAX_PERMISSIONS) {
                safe_strcpy(out_result->permissions[out_result->permission_count],
                            perm, 64);
                out_result->permission_count++;
            }
        }

        if (value) AGENTOS_FREE(value);
    }

    fclose(fp);

    /* 验证必要字段 */
    if (out_result->name[0] == '\0') {
        out_result->valid = false;
        safe_strcpy(out_result->error_reason,
                    "Missing required field: name",
                    sizeof(out_result->error_reason));
        AGENTOS_LOG_WARN("PluginDiscovery: invalid manifest '%s': %s",
                         yaml_path, out_result->error_reason);
        return -1;
    }

    if (out_result->library_path[0] == '\0') {
        out_result->valid = false;
        safe_strcpy(out_result->error_reason,
                    "Missing required field: library",
                    sizeof(out_result->error_reason));
        return -1;
    }

    out_result->valid = true;

    AGENTOS_LOG_DEBUG("PluginDiscovery: parsed manifest '%s' → name=%s "
                      "type=%d version=%s perms=%u",
                      yaml_path, out_result->name,
                      out_result->type, out_result->version,
                      out_result->permission_count);
    return 0;
}

/* ==================== 扫描目录 ==================== */

int plugin_discovery_scan(plugin_discovery_result_t **out_results,
                          size_t *out_count)
{
    if (!out_results || !out_count) return -1;

    *out_results = NULL;
    *out_count = 0;

    if (!g_discovery.initialized) {
        AGENTOS_LOG_WARN("PluginDiscovery: not initialized");
        return -1;
    }

    const char *plugins_dir = g_discovery.plugins_dir;

    if (!dir_exists(plugins_dir)) {
        AGENTOS_LOG_INFO("PluginDiscovery: plugins dir not found '%s', "
                         "skipping scan", plugins_dir);
        return 0;
    }

    AGENTOS_LOG_INFO("PluginDiscovery: scanning '%s'...", plugins_dir);

    /* 跨平台目录遍历初始化：
     * POSIX: opendir/readdir/closedir
     * Windows: FindFirstFile/FindNextFile/FindClose */
#ifndef _WIN32
    DIR *dir = opendir(plugins_dir);
    if (!dir) {
        AGENTOS_LOG_ERROR("PluginDiscovery: cannot open dir '%s'", plugins_dir);
        return -1;
    }
#else
    char win_pattern[PLUGIN_DISCOVERY_MAX_PATH];
    snprintf(win_pattern, sizeof(win_pattern), "%s\\*", plugins_dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(win_pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        AGENTOS_LOG_ERROR("PluginDiscovery: cannot open dir '%s'", plugins_dir);
        return -1;
    }
#endif

    /* 先分配结果数组（最大数量） */
    plugin_discovery_result_t *results = (plugin_discovery_result_t *)
        AGENTOS_CALLOC(PLUGIN_DISCOVERY_MAX_PLUGINS,
                       sizeof(plugin_discovery_result_t));
    if (!results) {
#ifndef _WIN32
        closedir(dir);
#else
        FindClose(hFind);
#endif
        return -1;
    }

    size_t found = 0;
    int first_entry = 1; /* Windows: FindFirstFile 已获取第一个条目，首次迭代不调 FindNextFile */

    while (found < PLUGIN_DISCOVERY_MAX_PLUGINS) {
        const char *d_name;
#ifndef _WIN32
        struct dirent *entry = readdir(dir);
        if (!entry) break;
        d_name = entry->d_name;
#else
        if (!first_entry) {
            if (!FindNextFileA(hFind, &fd)) break;
        }
        first_entry = 0;
        d_name = fd.cFileName;
#endif
        /* 跳过 . 和 .. */
        if (d_name[0] == '.') continue;

        /* 构建插件目录路径 */
        char plugin_dir_path[PLUGIN_DISCOVERY_MAX_PATH];
        snprintf(plugin_dir_path, sizeof(plugin_dir_path),
                 "%s/%s", plugins_dir, d_name);

        /* 检查是否为目录 */
        struct stat st;
        if (stat(plugin_dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }

        /* 检查 manifest.yaml */
        char manifest_path[PLUGIN_DISCOVERY_MAX_PATH];
        snprintf(manifest_path, sizeof(manifest_path),
                 "%s/manifest.yaml", plugin_dir_path);

        if (!file_exists(manifest_path)) {
            AGENTOS_LOG_DEBUG("PluginDiscovery: skipping '%s' (no manifest.yaml)",
                              d_name);
            continue;
        }

        /* 解析 manifest */
        plugin_discovery_result_t *result = &results[found];
        int ret = plugin_discovery_parse_manifest(
            manifest_path, plugin_dir_path, result);

        if (ret == 0 && result->valid) {
            found++;
            AGENTOS_LOG_INFO("PluginDiscovery: found plugin '%s' v%s (type=%d)",
                             result->name, result->version, result->type);
        } else if (g_discovery.config.fail_on_invalid) {
            AGENTOS_LOG_ERROR("PluginDiscovery: invalid plugin '%s' in '%s'",
                              d_name, plugin_dir_path);
        }
    }

#ifndef _WIN32
    closedir(dir);
#else
    FindClose(hFind);
#endif

    *out_results = results;
    *out_count = found;

    /* 缓存结果 */
    if (g_discovery.results) {
        plugin_discovery_free_results(g_discovery.results, g_discovery.count);
    }
    g_discovery.results = results;
    g_discovery.count = found;

    AGENTOS_LOG_INFO("PluginDiscovery: scan complete (%zu plugins found)",
                     found);
    return 0;
}

/* ==================== 自动加载 ==================== */

int plugin_discovery_auto_load(void)
{
    if (!g_discovery.initialized) {
        plugin_discovery_init(NULL);
    }

    plugin_discovery_result_t *results = NULL;
    size_t count = 0;

    int ret = plugin_discovery_scan(&results, &count);
    if (ret != 0 || count == 0) {
        AGENTOS_LOG_INFO("PluginDiscovery: no plugins to auto-load");
        return 0;
    }

    size_t loaded = 0;
    size_t failed = 0;

    for (size_t i = 0; i < count; i++) {
        if (!results[i].valid) continue;

        AGENTOS_LOG_INFO("PluginDiscovery: auto-loading '%s' from '%s'",
                         results[i].name, results[i].library_path);

        const char *out_name = NULL;
        int load_ret = plugin_service_load(
            results[i].library_path, NULL, &out_name);

        if (load_ret == 0) {
            loaded++;
            /* 自动启动插件 */
            plugin_service_start(results[i].name);
        } else {
            failed++;
            AGENTOS_LOG_WARN("PluginDiscovery: auto-load failed for '%s'",
                             results[i].name);
        }
    }

    AGENTOS_LOG_INFO("PluginDiscovery: auto-load complete "
                     "(loaded=%zu, failed=%zu)",
                     loaded, failed);

    plugin_discovery_free_results(results, count);
    return (failed > 0) ? -1 : 0;
}

/* ==================== 查询 ==================== */

size_t plugin_discovery_count(void)
{
    return g_discovery.count;
}

/* ==================== 内存管理 ==================== */

void plugin_discovery_free_results(plugin_discovery_result_t *results,
                                   size_t count)
{
    (void)count;
    if (results) {
        AGENTOS_FREE(results);
    }
}