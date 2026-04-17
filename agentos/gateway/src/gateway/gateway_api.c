/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_api.c
 * @brief 公共 API 包装函数实现
 *
 * 实现 include/gateway.h 中声明的公共 API 函数，
 * 将调用委托给各网关的内部创建函数。
 *
 * 架构说明：
 *   公共 API (gateway_http_create) --> 内部实现 (http_gateway_create)
 *   这样 agentos/daemon/gateway_d 等外部模块只需依赖公共头文件即可。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#define GATEWAY_API_IMPLEMENTATION
#include "gateway.h"
#include "http_gateway.h"
#include "ws_gateway.h"
#include "stdio_gateway.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 创建 HTTP 网关（公共 API 入口）
 *
 * 委托给 http_gateway_create() 内部实现。
 * 对外暴露统一的 gateway_http_create 符号。
 *
 * @param host 监听地址
 * @param port 监听端口
 * @return 网关句柄，失败返回 NULL
 */
gateway_t* gateway_http_create(const char* host, uint16_t port) {
    return http_gateway_create(host, port);
}

/**
 * @brief 创建 WebSocket 网关（公共 API 入口）
 *
 * 委托给 ws_gateway_create() 内部实现。
 */
gateway_t* gateway_ws_create(const char* host, uint16_t port) {
    return ws_gateway_create(host, port);
}

/**
 * @brief 创建 Stdio 网关（公共 API 入口）
 *
 * 委托给 stdio_gateway_create() 内部实现。
 */
gateway_t* gateway_stdio_create(void) {
    return stdio_gateway_create();
}

/* ==================== 公共 API 实现（非内联版本） ==================== */

#include <time.h>

static struct {
    uint64_t total_connections;
    uint64_t active_connections;
    time_t start_time;
    bool running;
} g_gateway_stats = {0, 0, 0, false};

void gateway_destroy(gateway_t* gw) {
    if (!gw) return;
    if (g_gateway_stats.running) {
        g_gateway_stats.running = false;
        g_gateway_stats.active_connections = 0;
    }
    free(gw);
}

agentos_error_t gateway_start(gateway_t* gw) {
    (void)gw;
    g_gateway_stats.start_time = time(NULL);
    g_gateway_stats.running = true;
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_stop(gateway_t* gw) {
    (void)gw;
    g_gateway_stats.running = false;
    g_gateway_stats.active_connections = 0;
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_get_stats(gateway_t* gw, char** out_json) {
    if (!gw || !out_json) return AGENTOS_EINVAL;

    double uptime_seconds = difftime(time(NULL), g_gateway_stats.start_time);

    cJSON* stats = cJSON_CreateObject();
    cJSON_AddStringToObject(stats, "status", g_gateway_stats.running ? "running" : "stopped");
    cJSON_AddNumberToObject(stats, "uptime_seconds", uptime_seconds);
    cJSON_AddNumberToObject(stats, "total_connections", (double)g_gateway_stats.total_connections);
    cJSON_AddNumberToObject(stats, "active_connections", (double)g_gateway_stats.active_connections);

    *out_json = cJSON_PrintUnformatted(stats);
    cJSON_Delete(stats);
    return AGENTOS_SUCCESS;
}
