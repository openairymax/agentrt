/**
 * @file config_loader.c
 * @brief 配置加载器实?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "config_loader.h"

#include "logger.h"

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#include <string.h>

#define AGENTOS_MAX_CONFIG_SIZE (4 * 1024 * 1024)

/**
 * @brief 从文件加载配置内容
 * @param path 文件路径
 * @param out_json 输出文件内容字符串（需调用者释放）
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_config_load(const char *path, char **out_json)
{
    if (!path || !out_json)
        return AGENTOS_EINVAL;

    FILE *file = fopen(path, "rb");
    if (!file) {
        AGENTOS_LOG_ERROR("Failed to open manager file: %s", path);
        return AGENTOS_ENOENT;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0 || size > AGENTOS_MAX_CONFIG_SIZE) {
        fclose(file);
        return (size < 0) ? AGENTOS_EIO : AGENTOS_EINVAL;
    }

    char *buffer = (char *)AGENTOS_MALLOC((size_t)size + 1);
    if (!buffer) {
        fclose(file);
        return AGENTOS_ENOMEM;
    }

    size_t read = fread(buffer, 1, (size_t)size, file);
    if (read != (size_t)size) {
        AGENTOS_FREE(buffer);
        fclose(file);
        return AGENTOS_EIO;
    }
    buffer[read] = '\0';
    fclose(file);

    *out_json = buffer;
    return AGENTOS_SUCCESS;
}

/* ================================================================
 * C-L01: Manager → CoreLoopThree 连接线实现（P1.1）
 * ================================================================ */

#include "yaml_loader.h"
#include "memory_compat.h"

#include <sys/stat.h>

#define AGENTOS_DEFAULT_CONFIG_PATH  "./agentos.yaml"
#define AGENTOS_CONFIG_WATCH_INTERVAL_MS 1000
#define AGENTOS_MAX_RELOAD_CALLBACKS 32

/* ── 全局配置实例 ── */
static agentos_yaml_config_t g_global_config;
static bool g_config_loaded = false;
static char g_config_path[256] = AGENTOS_DEFAULT_CONFIG_PATH;

/* ── 热重载回调 ── */
static agentos_config_reload_cb_t g_reload_callbacks[AGENTOS_MAX_RELOAD_CALLBACKS];
static void *g_reload_userdata[AGENTOS_MAX_RELOAD_CALLBACKS];
static size_t g_reload_cb_count = 0;

/* ── 监听线程 ── */
#include <pthread.h>
static pthread_t g_watch_thread;
static volatile bool g_watch_running = false;

int agentos_config_load_yaml(const char *yaml_path,
                             agentos_yaml_config_t *config)
{
    if (!config) return -1;

    const char *path = yaml_path ? yaml_path : AGENTOS_DEFAULT_CONFIG_PATH;

    /* 保存路径用于热重载 */
    safe_strcpy(g_config_path, path, sizeof(g_config_path));

    int ret = agentos_yaml_load(path, config);
    if (ret != 0) {
        /* 加载失败，使用默认配置 */
        agentos_yaml_config_defaults(config);
        agentos_yaml_resolve_platform_paths(config);
        return 0; /* 返回成功但使用默认值 */
    }

    /* 平台路径映射（P1.12.4） */
    agentos_yaml_resolve_platform_paths(config);

    /* 验证配置 */
    if (agentos_yaml_validate(config) != 0) {
        agentos_yaml_config_defaults(config);
        agentos_yaml_resolve_platform_paths(config);
    }

    return 0;
}

const agentos_yaml_config_t *agentos_config_get_global(void)
{
    if (!g_config_loaded) {
        return NULL;
    }
    return &g_global_config;
}

int agentos_config_on_reload(agentos_config_reload_cb_t callback,
                             void *user_data)
{
    if (!callback) return -1;
    if (g_reload_cb_count >= AGENTOS_MAX_RELOAD_CALLBACKS) return -1;

    g_reload_callbacks[g_reload_cb_count] = callback;
    g_reload_userdata[g_reload_cb_count] = user_data;
    g_reload_cb_count++;
    return 0;
}

static void config_reload_notify(const agentos_yaml_config_t *old_config,
                                 const agentos_yaml_config_t *new_config)
{
    for (size_t i = 0; i < g_reload_cb_count; i++) {
        if (g_reload_callbacks[i]) {
            g_reload_callbacks[i](old_config, new_config,
                                  g_reload_userdata[i]);
        }
    }
}

static void *config_watch_thread(void *arg)
{
    (void)arg;
    char path[256];
    safe_strcpy(path, g_config_path, sizeof(path));

    time_t last_mtime = 0;

    /* 获取初始 mtime */
    struct stat st;
    if (stat(path, &st) == 0) {
        last_mtime = st.st_mtime;
    }

    while (g_watch_running) {
        /* 轮询文件变更 */
        if (stat(path, &st) == 0) {
            if (st.st_mtime != last_mtime) {
                last_mtime = st.st_mtime;

                /* 文件已变更，触发重载 */
                agentos_yaml_config_t new_config;
                if (agentos_yaml_load(path, &new_config) == 0 &&
                    agentos_yaml_validate(&new_config) == 0) {

                    /* 保存旧配置 */
                    agentos_yaml_config_t old_config = g_global_config;
                    g_global_config = new_config;

                    /* 通知回调 */
                    config_reload_notify(&old_config, &new_config);
                }
            }
        }

        /* 等待下一次检查 */
        /* 简化实现：使用 sleep，生产环境应使用 inotify */
        sleep((AGENTOS_CONFIG_WATCH_INTERVAL_MS + 500) / 1000);
        if (AGENTOS_CONFIG_WATCH_INTERVAL_MS < 1000) {
            usleep(AGENTOS_CONFIG_WATCH_INTERVAL_MS * 1000);
        }
    }

    return NULL;
}

int agentos_config_watch_start(const char *yaml_path, uint32_t interval_ms)
{
    if (g_watch_running) return -1;

    const char *path = yaml_path ? yaml_path : AGENTOS_DEFAULT_CONFIG_PATH;
    safe_strcpy(g_config_path, path, sizeof(g_config_path));

    g_watch_running = true;

    /* TODO: Phase 3 实现 - 使用 inotify（Linux）/kqueue（macOS）
     * 当前使用轮询模式，适合 Phase 1 快速验证 */
    if (pthread_create(&g_watch_thread, NULL, config_watch_thread, NULL) != 0) {
        g_watch_running = false;
        return -1;
    }

    return 0;
}

void agentos_config_watch_stop(void)
{
    g_watch_running = false;
    /* TODO: Phase 3 实现 - pthread_join 等待线程退出 */
}

int agentos_config_reload(const char *yaml_path)
{
    const char *path = yaml_path ? yaml_path : g_config_path;

    agentos_yaml_config_t new_config;
    if (agentos_yaml_load(path, &new_config) != 0) {
        return -1;
    }

    if (agentos_yaml_validate(&new_config) != 0) {
        return -1;
    }

    agentos_yaml_config_t old_config = g_global_config;
    g_global_config = new_config;
    g_config_loaded = true;

    config_reload_notify(&old_config, &new_config);
    return 0;
}

/* ── 模块初始化 ── */

/**
 * @brief CoreLoopThree 配置模块初始化（在 agentos_init() 之后调用）
 * @return 0 成功，非0失败
 */
int agentos_config_init(const char *yaml_path)
{
    const char *path = yaml_path ? yaml_path : AGENTOS_DEFAULT_CONFIG_PATH;

    agentos_yaml_config_defaults(&g_global_config);

    if (agentos_yaml_load(path, &g_global_config) == 0) {
        g_config_loaded = true;
    } else {
        /* 文件不存在时使用默认配置 */
        g_config_loaded = true;
    }

    /* 平台路径映射（P1.12.4） */
    agentos_yaml_resolve_platform_paths(&g_global_config);

    return 0;
}
