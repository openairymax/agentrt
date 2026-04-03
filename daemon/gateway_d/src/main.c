/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file main.c
 * @brief Gateway守护进程主入口
 *
 * gateway_d 是 AgentOS 的网关守护进程，负责管理多个网关实例
 * 并提供统一的服务入口。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "gateway_service.h"
#include "svc_common.h"
#include "svc_logger.h"
#include "svc_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

/* 全局服务句柄 */
static gateway_service_t g_service = NULL;

/* 运行标志 */
static volatile int g_running = 1;

/**
 * @brief 信号处理函数
 * @param sig 信号值
 */
static void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
    
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

/**
 * @brief 解析命令行参数
 * @param argc 参数数量
 * @param argv 参数数组
 * @param config 配置输出
 * @return 0 成功
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
                fprintf(stderr, "Failed to load config: %s\n", argv[i]);
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
                fprintf(stderr, "Failed to fork\n");
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
#endif
        }
        else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief 主函数
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 0 成功
 */
int main(int argc, char* argv[]) {
    gateway_service_config_t config;
    
    /* 解析命令行参数 */
    if (parse_args(argc, argv, &config) != 0) {
        return 1;
    }
    
    /* 设置信号处理 */
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, SIG_IGN);
#endif
    
    /* 创建服务 */
    agentos_error_t err = gateway_service_create(&g_service, &config);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to create gateway service: %d\n", err);
        return 1;
    }
    
    /* 初始化服务 */
    err = gateway_service_init(g_service);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to initialize gateway service: %d\n", err);
        gateway_service_destroy(g_service);
        return 1;
    }
    
    /* 启动服务 */
    err = gateway_service_start(g_service);
    if (err != AGENTOS_SUCCESS) {
        fprintf(stderr, "Failed to start gateway service: %d\n", err);
        gateway_service_destroy(g_service);
        return 1;
    }
    
    printf("AgentOS Gateway Daemon started\n");
    printf("  HTTP:    %s:%d %s\n", 
           config.http.host, config.http.port,
           config.http.enabled ? "[enabled]" : "[disabled]");
    printf("  WebSocket: %s:%d %s\n",
           config.ws.host, config.ws.port,
           config.ws.enabled ? "[enabled]" : "[disabled]");
    printf("  Stdio:   %s\n",
           config.stdio.enabled ? "[enabled]" : "[disabled]");
    
    /* 主循环 */
    while (g_running && gateway_service_is_running(g_service)) {
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
    }
    
    /* 停止服务 */
    printf("Shutting down...\n");
    gateway_service_stop(g_service, false);
    
    /* 清理 */
    gateway_service_destroy(g_service);
    
    printf("Gateway daemon stopped\n");
    return 0;
}
