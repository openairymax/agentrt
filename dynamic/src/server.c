/**
 * @file server.c
 * @brief Dynamic 核心控制器
 */

#include "server.h"
#include "gateway/http_gateway.h"
#include "gateway/ws_gateway.h"
#include "gateway/stdio_gateway.h"
#include "agentos.h"
#include "logger.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 全局实例（供信号处理）
struct dynamic_server* g_server = NULL;

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        AGENTOS_LOG_INFO("Received signal %d, initiating shutdown...", sig);
        dynamic_stop();
    }
}

static void set_default_config(dynamic_config_t* cfg) {
    cfg->http_host = "127.0.0.1";
    cfg->http_port = 18789;
    cfg->ws_host = "127.0.0.1";
    cfg->ws_port = 18790;
    cfg->enable_stdio = 1;
    cfg->max_sessions = 1000;
    cfg->session_timeout_sec = 3600;
    cfg->health_interval_sec = 30;
    cfg->metrics_path = "/metrics";
}

int dynamic_start(const dynamic_config_t* user_cfg) {
    if (g_server) {
        AGENTOS_LOG_ERROR("Dynamic already running");
        return -1;
    }
    struct dynamic_server* server = (struct dynamic_server*)calloc(1, sizeof(struct dynamic_server));
    if (!server) return -1;

    // 合并配置
    if (user_cfg) {
        server->config = *user_cfg;
    } else {
        set_default_config(&server->config);
    }

    // 初始化锁和条件变量
    pthread_mutex_init(&server->lock, NULL);
    pthread_cond_init(&server->cond, NULL);
    server->running = 1;

    // 创建会话管理器
    server->session_mgr = session_manager_create(server->config.max_sessions,
                                                 server->config.session_timeout_sec);
    if (!server->session_mgr) goto fail;

    // 创建健康检查器
    server->health = health_checker_create(server->config.health_interval_sec);
    if (!server->health) goto fail;

    // 创建可观测性
    server->telemetry = telemetry_create();
    if (!server->telemetry) goto fail;

    // 分配网关数组（最多3个）
    server->gateways = (gateway_t**)calloc(3, sizeof(gateway_t*));
    if (!server->gateways) goto fail;

    // 创建 HTTP 网关
    gateway_t* http = http_gateway_create(server->config.http_host, server->config.http_port);
    if (!http) {
        AGENTOS_LOG_ERROR("Failed to create HTTP gateway");
        goto fail_gateways;
    }
    server->gateways[server->gateway_count++] = http;

    // 创建 WebSocket 网关
    gateway_t* ws = ws_gateway_create(server->config.ws_host, server->config.ws_port);
    if (!ws) {
        AGENTOS_LOG_ERROR("Failed to create WebSocket gateway");
        goto fail_gateways;
    }
    server->gateways[server->gateway_count++] = ws;

    // 创建 stdio 网关（如果需要）
    if (server->config.enable_stdio) {
        gateway_t* stdio = stdio_gateway_create();
        if (!stdio) {
            AGENTOS_LOG_ERROR("Failed to create stdio gateway");
            goto fail_gateways;
        }
        server->gateways[server->gateway_count++] = stdio;
    }

    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    g_server = server;

    // 启动所有网关
    for (size_t i = 0; i < server->gateway_count; i++) {
        if (gateway_start(server->gateways[i]) != 0) {
            AGENTOS_LOG_ERROR("Failed to start gateway %zu", i);
            // 继续尝试其他网关，但标记为失败？这里选择继续，但启动失败可能影响服务
        }
    }

    // 等待停止信号
    pthread_mutex_lock(&server->lock);
    while (server->running) {
        pthread_cond_wait(&server->cond, &server->lock);
    }
    pthread_mutex_unlock(&server->lock);

    // 停止所有网关
    for (size_t i = 0; i < server->gateway_count; i++) {
        gateway_stop(server->gateways[i]);
    }

    // 销毁所有网关
    for (size_t i = 0; i < server->gateway_count; i++) {
        gateway_destroy(server->gateways[i]);
    }
    free(server->gateways);

    // 销毁其他组件
    if (server->session_mgr) session_manager_destroy(server->session_mgr);
    if (server->health) health_checker_destroy(server->health);
    if (server->telemetry) telemetry_destroy(server->telemetry);

    pthread_mutex_destroy(&server->lock);
    pthread_cond_destroy(&server->cond);
    free(server);
    g_server = NULL;
    return 0;

fail_gateways:
    // 销毁已创建的网关
    for (size_t i = 0; i < server->gateway_count; i++) {
        gateway_destroy(server->gateways[i]);
    }
    free(server->gateways);
    server->gateways = NULL;

fail:
    if (server->session_mgr) session_manager_destroy(server->session_mgr);
    if (server->health) health_checker_destroy(server->health);
    if (server->telemetry) telemetry_destroy(server->telemetry);
    pthread_mutex_destroy(&server->lock);
    pthread_cond_destroy(&server->cond);
    free(server);
    return -1;
}

void dynamic_stop(void) {
    if (!g_server) return;
    pthread_mutex_lock(&g_server->lock);
    g_server->running = 0;
    pthread_cond_signal(&g_server->cond);
    pthread_mutex_unlock(&g_server->lock);
}

void dynamic_wait(void) {
    // 已经阻塞在 dynamic_start 中，无需额外操作
}