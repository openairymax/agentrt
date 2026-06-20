/**
 * @file main.c
 * @brief Hook 守护进程入口
 *
 * @owner team-A
 */

#include "hook_service.h"
#include "daemon_bootstrap_sd.h"
#include "daemon_bootstrap_ipc.h"
#include "logging.h"
#include "platform.h"
#include "svc_logger.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define HOOK_D_SOCKET_PATH AGENTOS_RUNTIME_DIR "/hook.sock"

static volatile int g_running = 1;
static agentos_socket_t g_server_fd = AGENTOS_INVALID_SOCKET;
static daemon_bootstrap_sd_t *g_bsd = NULL;
static daemon_bootstrap_ipc_t *g_bipc = NULL;

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

static void svc_log_toggle_handler(int sig)
{
    (void)sig;
    static int debug_mode = 0;
    debug_mode = !debug_mode;
    log_set_module_level("*", debug_mode ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, svc_log_toggle_handler);

    agentos_log_init(NULL);
    atexit(log_cleanup);

    SVC_LOG_INFO("hook_d: starting");

    /* 创建 Unix Socket 服务器 */
    g_server_fd = agentos_socket_create_unix_server(HOOK_D_SOCKET_PATH);
    if (g_server_fd < 0) {
        SVC_LOG_ERROR("P2.1: HookD: failed to create socket at %s (errno=%d: %s)",
                      HOOK_D_SOCKET_PATH, errno, strerror(errno));
        return 1;
    }
    SVC_LOG_INFO("P2.1: HookD: listening on %s (fd=%d)", HOOK_D_SOCKET_PATH, (int)g_server_fd);

    g_bsd = daemon_bootstrap_sd_start("hook_d", "hook", HOOK_D_SOCKET_PATH,
                                      0, "hook,core", 0);
    if (!g_bsd) {
        SVC_LOG_WARN("P2.1: HookD: SD bootstrap failed, continuing");
    }
    g_bipc = daemon_bootstrap_ipc_start("hook_d", "hook", HOOK_D_SOCKET_PATH,
                                        0, IPC_BUS_PROTO_JSON_RPC);
    if (!g_bipc) {
        SVC_LOG_WARN("P2.1: HookD: IPC bootstrap failed, continuing");
    }

    SVC_LOG_INFO("P2.1: HookD: running (sd=%s ipc=%s), waiting for shutdown signal",
                 g_bsd ? "ok" : "no", g_bipc ? "ok" : "no");

    /* 等待关闭信号 */
    while (g_running) {
        sleep(1);
    }

    daemon_bootstrap_ipc_stop(g_bipc);
    daemon_bootstrap_sd_stop(g_bsd);
    if (g_server_fd >= 0) {
        agentos_socket_close(g_server_fd);
        g_server_fd = AGENTOS_INVALID_SOCKET;
    }
    SVC_LOG_INFO("P2.1: HookD: shutting down (sd=%s ipc=%s)",
                 g_bsd ? "stopped" : "n/a", g_bipc ? "stopped" : "n/a");
    log_cleanup();
    return EXIT_SUCCESS;
}