/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file main.c
 * @brief Gateway守护进程主入口（遵循 daemon 模块统一规范）
 *
 * gateway_d 是 AgentOS 的网关守护进程，负责管理多个网关实例
 * 并提供统一的服务入口。
 *
 * 规范遵循:
 * - ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性
 * - C_coding_style_guide.md 命名/可见性/错误处理规范
 * - protocol_contract.md JSON-RPC 2.0 协议
 * - logging_format.md 结构化日志 (SVC_LOG_* 宏)
 */

#include "gateway_service.h"
#include "svc_common.h"
#include "svc_logger.h"
#include "svc_config.h"
#include "platform.h"
#include "error.h"

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
 * @brief 信号处理函数
 * @param sig 信号值
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
 * @brief Windows控制台处理函数
 * @param fdwCtrlType 控制信号类型
 * @return TRUE 已处理
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

/**
 * @brief 打印使用说明
 * @param prog 程序名
 */
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
    printf("\n");
    printf("Examples:\n");
    printf("  %s -h 127.0.0.1 -p 8080\n", prog);
    printf("  %s -c /etc/agentos/gateway.conf\n", prog);
    printf("  %s -s  # Enable stdio mode for CLI\n", prog);
}

/* ==================== 参数解析 ==================== */

/**
 * @brief 解析命令行参数
 * @param argc 参数数量
 * @param argv 参数数组
 * @param config 配置输出
 * @return 0 成功，非0 失败
 */
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
            /* Unix daemon mode */
            pid_t pid = fork();
            if (pid < 0) {
                SVC_LOG_ERROR("Failed to fork daemon process");
                return -1;
            }
            if (pid > 0) {
                exit(0);
            }
            setsid();
            umask(0);
            chdir("/");
            fclose(stdin);
            fclose(stdout);
            fclose(stderr);
            SVC_LOG_INFO("Gateway daemonized successfully");
#else
            SVC_LOG_WARN("-d (daemon mode) is not supported on Windows");
#endif
        }
        else {
            SVC_LOG_ERROR("Unknown option: %s", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }

    return 0;
}

/* ==================== 主函数 ==================== */

int main(int argc, char* argv[]) {
    gateway_service_config_t config;

    /* 初始化平台层 */
    agentos_socket_init();
    agentos_mutex_init(&g_running_lock);

    /* 设置信号处理 */
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
#endif

    /* 解析命令行参数 */
    if (parse_args(argc, argv, &config) != 0) {
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    SVC_LOG_INFO("Gateway service starting...");

    /* 创建服务 */
    agentos_error_t err = gateway_service_create(&g_service, &config);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to create gateway service (error=%d)", err);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    /* 初始化服务 */
    err = gateway_service_init(g_service);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to initialize gateway service (error=%d)", err);
        gateway_service_destroy(g_service);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
    }

    /* 启动服务 */
    err = gateway_service_start(g_service);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to start gateway service (error=%d)", err);
        gateway_service_destroy(g_service);
        agentos_mutex_destroy(&g_running_lock);
        agentos_socket_cleanup();
        return 1;
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

    /* 主循环 */
    while (g_running && gateway_service_is_running(g_service)) {
        agentos_sleep_ms(1000);
    }

    /* 停止服务 */
    SVC_LOG_INFO("Gateway service shutting down...");
    gateway_service_stop(g_service, false);

    /* 清理 */
    gateway_service_destroy(g_service);

    agentos_mutex_destroy(&g_running_lock);
    agentos_socket_cleanup();

    SVC_LOG_INFO("Gateway daemon stopped");
    return 0;
}
