/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file main.c
 * @brief Gateway守护进程主入口（遵循 daemon 模块统一规范）
 *
 * 规范遵循:
 * - ARCHITECTURAL_PRINCIPLES.md E-3 资源确定性(成对管理)
 * - ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性(platform.h)
 * - ARCHITECTURAL_PRINCIPLES.md E-5 命名语义化(SVC_LOG_*)
 * - ARCHITECTURAL_PRINCIPLES.md E-6 错误可追溯(AGENTOS_ERR_*)
 */

#include "gateway_service.h"
#include "svc_common.h"
#include "svc_logger.h"
#include "svc_config.h"
#include "platform.h"
#include "error.h"

#ifdef AGENTOS_HAS_PROTOCOLS
#include "unified_protocol.h"
#include "mcp_v1_adapter.h"
#include "a2a_v03_adapter.h"
#include "openai_enterprise_adapter.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* ==================== 全局状态 ==================== */

static gateway_service_t g_service = NULL;
static volatile int g_running = 1;
static agentos_mutex_t g_running_lock;

/* ==================== 信号处理 ==================== */

/**
 * @brief 信号处理函数（线程安全，使用互斥锁保护运行标志）
 */
static void signal_handler(int sig) {
    (void)sig;
    agentos_mutex_lock(&g_running_lock);
    g_running = 0;
    agentos_mutex_unlock(&g_running_lock);

    if (g_service) {
        gateway_service_stop(g_service, false);
    }
}

#ifdef _WIN32
/**
 * @brief Windows控制台事件处理函数
 */
static BOOL WINAPI console_handler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            signal_handler((int)fdwCtrlType);
            return TRUE;
        default:
            return FALSE;
    }
}
#endif

/* ==================== 帮助信息 ==================== */

static void print_usage(const char* prog) {
    printf("AgentOS Gateway Daemon\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  -c <config>   Configuration file path\n");
    printf("  -h <host>     HTTP gateway host (default: 0.0.0.0)\n");
    printf("  -p <port>     HTTP gateway port (default: 8080)\n");
    printf("  -w <port>     WebSocket gateway port (default: 8081)\n");
    printf("  -s            Enable stdio gateway\n");
    printf("  -d            Run as daemon (Unix only)\n");
    printf("  -v            Verbose output\n");
    printf("  --help        Show this help\n");
    printf("\nExamples:\n");
    printf("  %s -h 127.0.0.1 -p 8080\n", prog);
    printf("  %s -c /etc/agentos/gateway.conf\n", prog);
}

/* ==================== 参数解析 ==================== */

static int parse_args(int argc, char* argv[], gateway_service_config_t* config) {
    gateway_service_get_default_config(config);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        }
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            agentos_error_t err = gateway_service_load_config(config, argv[++i]);
            if (err != AGENTOS_SUCCESS) {
                SVC_LOG_ERROR("Failed to load config: %s", argv[i]);
                return -1;
            }
        }
        else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            config->http.host = argv[++i];
        }
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            config->http.port = (uint16_t)atoi(argv[++i]);
            config->http.enabled = true;
        }
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            config->ws.port = (uint16_t)atoi(argv[++i]);
            config->ws.enabled = true;
        }
        else if (strcmp(argv[i], "-s") == 0) {
            config->stdio.enabled = true;
        }
        else if (strcmp(argv[i], "-v") == 0) {
            config->enable_metrics = true;
        }
        else if (strcmp(argv[i], "-d") == 0) {
#ifndef _WIN32
            pid_t pid = fork();
            if (pid < 0) { SVC_LOG_ERROR("Failed to fork"); return -1; }
            if (pid > 0) exit(0);
            setsid(); umask(0); chdir("/");
            fclose(stdin); fclose(stdout); fclose(stderr);
            SVC_LOG_INFO("Gateway daemonized");
#else
            SVC_LOG_WARN("-d not supported on Windows");
#endif
        }
        else {
            SVC_LOG_ERROR("Unknown option: %s", argv[i]);
            return -1;
        }
    }
    return 0;
}

/* ==================== 主函数 ==================== */

int main(int argc, char* argv[]) {
    gateway_service_config_t config;

    /* E-3 资源确定性: 初始化与清理成对 */
    agentos_socket_init();
    agentos_mutex_init(&g_running_lock);

    /* 跨平台信号处理 */
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
#endif

    if (parse_args(argc, argv, &config) != 0) {
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    SVC_LOG_INFO("Gateway service starting...");

    agentos_error_t err = gateway_service_create(&g_service, &config);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to create service (err=%d)", err);
        goto cleanup;
    }

    err = gateway_service_init(g_service);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to init service (err=%d)", err);
        goto cleanup_service;
    }

    /* Initialize UnifiedProtocol stack for multi-protocol support */
#ifdef AGENTOS_HAS_PROTOCOLS
    const protocol_adapter_t* mcp_adapter = mcp_v1_get_adapter();
    if (mcp_adapter) {
        if (mcp_adapter->init(mcp_adapter->context) == 0) {
            SVC_LOG_INFO("MCP v1.0 adapter initialized (version=%s, caps=0x%x)",
                        mcp_adapter->version ? mcp_adapter->version : "unknown",
                        mcp_adapter->capabilities ? mcp_adapter->capabilities(mcp_adapter->context) : 0);
        } else {
            SVC_LOG_WARN("Failed to initialize MCP v1.0 adapter");
        }
    } else {
        SVC_LOG_WARN("MCP v1.0 adapter not available");
    }
#endif

    err = gateway_service_start(g_service);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to start service (err=%d)", err);
        goto cleanup_service;
    }

    SVC_LOG_INFO("AgentOS Gateway Daemon started");
    SVC_LOG_INFO("  HTTP:     %s:%d %s",
                config.http.host, config.http.port,
                config.http.enabled ? "[enabled]" : "[disabled]");
    SVC_LOG_INFO("  WebSocket: %s:%d %s",
                config.ws.host, config.ws.port,
                config.ws.enabled ? "[enabled]" : "[disabled]");
    SVC_LOG_INFO("  Stdio:    %s",
                config.stdio.enabled ? "[enabled]" : "[disabled]");

    /* 主事件循环：信号驱动 + 周期性健康检查 */
    int loop_count = 0;
    const int HEALTH_CHECK_INTERVAL = 30;

    while (g_running) {
        if (!gateway_service_is_running(g_service)) {
            SVC_LOG_WARN("Gateway service stopped unexpectedly");
            break;
        }

        agentos_sleep_ms(1000);
        loop_count++;

        if (config.enable_metrics && (loop_count % HEALTH_CHECK_INTERVAL == 0)) {
            agentos_svc_stats_t stats;
            if (gateway_service_get_stats(g_service, &stats) == AGENTOS_SUCCESS) {
                SVC_LOG_INFO("Health Check [interval=%ds] "
                            "| concurrent=%u | total_req=%llu "
                            "| errors=%llu | avg_time=%.1fms",
                            HEALTH_CHECK_INTERVAL,
                            stats.current_concurrent,
                            (unsigned long long)stats.request_count,
                            (unsigned long long)stats.error_count,
                            stats.avg_time_ms);
            }
        }
    }

    SVC_LOG_INFO("Gateway shutting down...");
    gateway_service_stop(g_service, false);

    /* Cleanup protocol stack */
#ifdef AGENTOS_HAS_PROTOCOLS
    {
        const protocol_adapter_t* mcp_adapter = mcp_v1_get_adapter();
        if (mcp_adapter && mcp_adapter->destroy) {
            mcp_adapter->destroy(mcp_adapter->context);
            SVC_LOG_INFO("MCP adapter destroyed");
        }
    }
#endif

cleanup_service:
    gateway_service_destroy(g_service);
cleanup:
    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Gateway daemon stopped");
    return 0;
}
