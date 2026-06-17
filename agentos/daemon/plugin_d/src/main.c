/**
 * @file main.c
 * @brief Plugin 守护进程入口
 *
 * @owner team-A
 */

#include "plugin_service.h"
#include "daemon_bootstrap_sd.h"
#include "daemon_bootstrap_ipc.h"

#include <stdio.h>
#include <stdlib.h>

static daemon_bootstrap_sd_t *g_bsd = NULL;
static daemon_bootstrap_ipc_t *g_bipc = NULL;

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* TODO: Phase 2 实现 - 读取 agentos.yaml, 初始化 IPC, 注册服务 */
    fprintf(stderr, "[plugin_d] Plugin daemon started (stub)\n");

    g_bsd = daemon_bootstrap_sd_start("plugin_d", "plugin", AGENTOS_RUNTIME_DIR "/plugin.sock",
                                      0, "plugin,core", 0);
    g_bipc = daemon_bootstrap_ipc_start("plugin_d", "plugin", AGENTOS_RUNTIME_DIR "/plugin.sock",
                                        0, IPC_BUS_PROTO_JSON_RPC);

    /* TODO: 进入事件循环 */
    /* agentos_event_loop_run(); */

    daemon_bootstrap_ipc_stop(g_bipc);
    daemon_bootstrap_sd_stop(g_bsd);
    return EXIT_SUCCESS;
}
