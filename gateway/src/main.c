/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file main.c
 * @brief gateway 可执行入口
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "gateway.h"
#include "logger.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/* 全局服务器实例（用于信号处理） */
static gateway_server_t* g_server = NULL;

/* 命令行参数解析 */
static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n\n", prog);
    printf("AgentOS gateway Runtime - Gateway Layer\n\n");
    printf("Options:\n");
    printf("  -h, --http-host HOST     HTTP listen host (default: 127.0.0.1)\n");
    printf("  -p, --http-port PORT     HTTP listen port (default: 18789)\n");
    printf("  -w, --ws-host HOST       WebSocket listen host (default: 127.0.0.1)\n");
    printf("  -W, --ws-port PORT       WebSocket listen port (default: 18790)\n");
    printf("  -s, --no-stdio           Disable stdio gateway\n");
    printf("  -m, --max-sessions N     Maximum concurrent sessions (default: 1000)\n");
    printf("  -t, --timeout SEC        Session timeout in seconds (default: 3600)\n");
    printf("  -v, --version            Show version information\n");
    printf("  --help                   Show this help message\n");
}

static void print_version(void) {
    printf("AgentOS gateway Runtime v%s (ABI %u)\n",
        gateway_get_version(),
        gateway_get_abi_version());
    printf("Copyright (c) 2026 SPHARX. All Rights Reserved.\n");
}

/* 信号处理函数 */
static void signal_handler(int sig) {
    const char* sig_name = (sig == SIGINT) ? "SIGINT" : 
                           (sig == SIGTERM) ? "SIGTERM" : "UNKNOWN";
    
    AGENTOS_LOG_INFO("Received %s (%d), initiating graceful shutdown...", 
        sig_name, sig);
    
    if (g_server) {
        gateway_server_stop(g_server);
    }
}

/* 注册信号处理 */
static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    
    /* 忽略 SIGPIPE（防止写入已关闭的 socket） */
    signal(SIGPIPE, SIG_IGN);
}

int main(int argc, char** argv) {
    gateway_config_t manager;
    server_set_default_config(&manager);
    
    /* 命令行选项 */
    static struct option long_options[] = {
        {"http-host",    required_argument, 0, 'h'},
        {"http-port",    required_argument, 0, 'p'},
        {"ws-host",      required_argument, 0, 'w'},
        {"ws-port",      required_argument, 0, 'W'},
        {"no-stdio",     no_argument,       0, 's'},
        {"max-sessions", required_argument, 0, 'm'},
        {"timeout",      required_argument, 0, 't'},
        {"version",      no_argument,       0, 'v'},
        {"help",         no_argument,       0, 'H'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "h:p:w:W:sm:t:v", 
            long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                manager.http_host = optarg;
                break;
            case 'p':
                manager.http_port = (uint16_t)atoi(optarg);
                break;
            case 'w':
                manager.ws_host = optarg;
                break;
            case 'W':
                manager.ws_port = (uint16_t)atoi(optarg);
                break;
            case 's':
                manager.enable_stdio = 0;
                break;
            case 'm':
                manager.max_sessions = (uint32_t)atoi(optarg);
                break;
            case 't':
                manager.session_timeout_sec = (uint32_t)atoi(optarg);
                break;
            case 'v':
                print_version();
                return 0;
            case 'H':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    /* 初始化日志 */
    agentos_logger_init(NULL);
    AGENTOS_LOG_INFO("AgentOS gateway Runtime v%s starting...", 
        gateway_get_version());
    
    /* 注册信号处理 */
    setup_signal_handlers();
    
    /* 创建服务器实例 */
    agentos_error_t err = gateway_server_create(&manager, &g_server);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to create server: %s", 
            agentos_strerror(err));
        return 1;
    }
    
    AGENTOS_LOG_INFO("Server configuration:");
    AGENTOS_LOG_INFO("  HTTP:      %s:%u", manager.http_host, manager.http_port);
    AGENTOS_LOG_INFO("  WebSocket: %s:%u", manager.ws_host, manager.ws_port);
    AGENTOS_LOG_INFO("  Stdio:     %s", manager.enable_stdio ? "enabled" : "disabled");
    AGENTOS_LOG_INFO("  Sessions:  max=%u, timeout=%us", 
        manager.max_sessions, manager.session_timeout_sec);
    
    /* 启动服务器（阻塞） */
    err = gateway_server_start(g_server);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Server error: %s", agentos_strerror(err));
        gateway_server_destroy(g_server);
        return 1;
    }
    
    /* 清理 */
    gateway_server_destroy(g_server);
    g_server = NULL;
    
    AGENTOS_LOG_INFO("Server shutdown complete");
    agentos_logger_shutdown();
    
    return 0;
}
