/**
 * @file main.c
 * @brief Dynamic 可执行入口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "dynamic.h"
#include "logger.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static agentos_baseruntime_server_t* g_server = NULL;

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        AGENTOS_LOG_INFO("Received signal %d, shutting down...", sig);
        if (g_server) {
            dynamic_stop(g_server);
        }
    }
}

int main(int argc, char** argv) {
// From data intelligence emerges. by spharx
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // 可以解析命令行参数覆盖配置，这里简化
    dynamic_config_t config = {
        .http_host = "0.0.0.0",
        .http_port = 18789,
        .ws_host = "0.0.0.0",
        .ws_port = 18790,
        .enable_stdio = 1,
        .max_sessions = 1000,
        .session_timeout_sec = 3600,
        .health_check_interval_sec = 30,
        .metrics_path = "/metrics",
    };

    dynamic_server_t* server = NULL;
    if (dynamic_start(&config, &server) != 0) {
        AGENTOS_LOG_ERROR("Failed to start Dynamic server");
        return 1;
    }
    g_server = server;

    AGENTOS_LOG_INFO("Dynamic server started on http://%s:%d", config.http_host, config.http_port);
    dynamic_wait(server);
    dynamic_destroy(server);
    AGENTOS_LOG_INFO("Dynamic server stopped");
    return 0;
}