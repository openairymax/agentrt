/**
 * @file main.c
 * @brief Plugin 守护进程入口 — P2.2 完整实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 启动流程：
 *   1. 初始化 ServiceDiscovery 自动注册
 *   2. 初始化 IPC Bus 消息路由
 *   3. 初始化插件发现模块
 *   4. 初始化权限校验模块
 *   5. 扫描并自动加载所有插件
 *   6. 进入事件循环
 */

#include "plugin_service.h"
#include "plugin_discovery.h"
#include "plugin_permission.h"

#include "daemon_bootstrap_sd.h"
#include "daemon_bootstrap_ipc.h"
#include "logger.h"
#include "platform.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PLUGIN_D_SOCKET_PATH AGENTOS_RUNTIME_DIR "/plugin.sock"

/* ==================== 全局状态 ==================== */

static volatile bool g_running = true;
static daemon_bootstrap_sd_t *g_bsd = NULL;
static daemon_bootstrap_ipc_t *g_bipc = NULL;
static agentos_socket_t g_server_fd = AGENTOS_INVALID_SOCKET;

/* ==================== 信号处理 ==================== */

static void signal_handler(int sig)
{
    (void)sig;
    g_running = false;
    AGENTOS_LOG_INFO("Plugin_d: received shutdown signal");
}

/* ==================== 主入口 ==================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    AGENTOS_LOG_INFO("============================================");
    AGENTOS_LOG_INFO("Plugin_d: starting (P2.2 Plugin Management)");
    AGENTOS_LOG_INFO("============================================");

    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 创建 Unix Socket 服务器 */
    g_server_fd = agentos_socket_create_unix_server(PLUGIN_D_SOCKET_PATH);
    if (g_server_fd < 0) {
        AGENTOS_LOG_ERROR("Plugin_d: failed to create socket at %s", PLUGIN_D_SOCKET_PATH);
        return EXIT_FAILURE;
    }
    AGENTOS_LOG_INFO("Plugin_d: listening on %s (fd=%d)", PLUGIN_D_SOCKET_PATH, (int)g_server_fd);

    /* P1.7: ServiceDiscovery 自动注册 */
    g_bsd = daemon_bootstrap_sd_start(
        "plugin_d", "plugin", PLUGIN_D_SOCKET_PATH,
        0, "plugin,core", 0);
    if (!g_bsd) {
        AGENTOS_LOG_ERROR("Plugin_d: ServiceDiscovery bootstrap failed");
        return EXIT_FAILURE;
    }
    AGENTOS_LOG_INFO("Plugin_d: ServiceDiscovery registered");

    /* P1.8: IPC Bus 统一消息路由 */
    g_bipc = daemon_bootstrap_ipc_start(
        "plugin_d", "plugin", PLUGIN_D_SOCKET_PATH,
        0, IPC_BUS_PROTO_JSON_RPC);
    if (!g_bipc) {
        AGENTOS_LOG_ERROR("Plugin_d: IPC Bus bootstrap failed");
        daemon_bootstrap_sd_stop(g_bsd);
        return EXIT_FAILURE;
    }
    AGENTOS_LOG_INFO("Plugin_d: IPC Bus registered");

    /* P2.2.4: 初始化权限校验模块 */
    plugin_permission_config_t perm_cfg;
    __builtin_memset(&perm_cfg, 0, sizeof(perm_cfg));
    perm_cfg.enable_strict_mode = true;
    perm_cfg.enable_audit_log = true;
    perm_cfg.agent_id = "plugin_d";

    if (plugin_permission_init(&perm_cfg) != 0) {
        AGENTOS_LOG_WARN("Plugin_d: Permission module init failed");
    }

    /* P2.2.1: 初始化插件发现模块 */
    plugin_discovery_config_t disc_cfg;
    __builtin_memset(&disc_cfg, 0, sizeof(disc_cfg));
    disc_cfg.plugins_dir = "ecosystem/plugins/";
    disc_cfg.auto_load = false;
    disc_cfg.fail_on_invalid = false;
    disc_cfg.scan_depth = 1;

    if (plugin_discovery_init(&disc_cfg) != 0) {
        AGENTOS_LOG_ERROR("Plugin_d: Plugin discovery init failed");
    }

    /* P2.2.1: 扫描插件目录并显示发现的插件 */
    plugin_discovery_result_t *results = NULL;
    size_t count = 0;

    if (plugin_discovery_scan(&results, &count) == 0 && count > 0) {
        AGENTOS_LOG_INFO("Plugin_d: Found %zu plugins:", count);
        for (size_t i = 0; i < count; i++) {
            if (results[i].valid) {
                AGENTOS_LOG_INFO("  [%zu] %s v%s (type=%d, perms=%u)",
                                 i + 1, results[i].name,
                                 results[i].version, results[i].type,
                                 results[i].permission_count);
            }
        }

        /* P2.2.4: 权限校验后加载 */
        size_t loaded = 0;
        for (size_t i = 0; i < count; i++) {
            if (!results[i].valid) continue;

            /* 权限校验 */
            char denied[512] = {0};
            plugin_permission_result_t perm_result = plugin_permission_check(
                (const char (*)[64])results[i].permissions,
                results[i].permission_count,
                results[i].name,
                denied, sizeof(denied));

            if (perm_result != PLUGIN_PERM_ALLOWED) {
                AGENTOS_LOG_WARN("Plugin_d: Skipping '%s' — permission denied: %s",
                                 results[i].name, denied);
                continue;
            }

            /* 加载插件 */
            const char *out_name = NULL;
            int load_ret = plugin_service_load(
                results[i].library_path, NULL, &out_name);

            if (load_ret == 0) {
                /* 自动启动 */
                plugin_service_start(results[i].name);
                loaded++;
                AGENTOS_LOG_INFO("Plugin_d: Loaded and started '%s'",
                                 results[i].name);
            } else {
                AGENTOS_LOG_ERROR("Plugin_d: Failed to load '%s' from '%s'",
                                  results[i].name, results[i].library_path);
            }
        }

        AGENTOS_LOG_INFO("Plugin_d: Loaded %zu/%zu plugins", loaded, count);
        plugin_discovery_free_results(results, count);
    } else {
        AGENTOS_LOG_INFO("Plugin_d: No plugins found in '%s'",
                         disc_cfg.plugins_dir);
    }

    AGENTOS_LOG_INFO("Plugin_d: Entering event loop...");

    /* 事件循环 */
    while (g_running) {
        sleep(1);
    }

    AGENTOS_LOG_INFO("Plugin_d: Shutting down...");

    /* 清理 */
    plugin_discovery_destroy();
    plugin_permission_destroy();
    daemon_bootstrap_ipc_stop(g_bipc);
    daemon_bootstrap_sd_stop(g_bsd);
    if (g_server_fd >= 0) {
        agentos_socket_close(g_server_fd);
        g_server_fd = AGENTOS_INVALID_SOCKET;
    }

    AGENTOS_LOG_INFO("Plugin_d: Stopped");
    return EXIT_SUCCESS;
}