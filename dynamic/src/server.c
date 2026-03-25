/**
 * @file server.c
 * @brief Dynamic 服务器核心控制器实现
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "server.h"
#include "gateway/http_gateway.h"
#include "gateway/ws_gateway.h"
#include "gateway/stdio_gateway.h"

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 全局服务器实例（用于信号处理） */
static dynamic_server_t* g_server_instance = NULL;
static pthread_mutex_t g_instance_lock = PTHREAD_MUTEX_INITIALIZER;

/* ========== 配置默认值 ========== */

void server_set_default_config(dynamic_config_t* cfg) {
    if (!cfg) return;
    
    memset(cfg, 0, sizeof(dynamic_config_t));
    
    cfg->http_host = "127.0.0.1";
    cfg->http_port = 18789;
    cfg->ws_host = "127.0.0.1";
    cfg->ws_port = 18790;
    cfg->enable_stdio = 1;
    cfg->max_sessions = 1000;
    cfg->session_timeout_sec = 3600;
    cfg->health_interval_sec = 30;
    cfg->metrics_path = "/metrics";
    cfg->max_request_size = 1048576;  /* 1MB */
    cfg->request_timeout_ms = 30000;  /* 30s */
}

agentos_error_t server_validate_config(const dynamic_config_t* cfg) {
    if (!cfg) return AGENTOS_EINVAL;
    
    /* 端口范围检查 */
    if (cfg->http_port == 0 || cfg->ws_port == 0) {
        return AGENTOS_EINVAL;
    }
    
    /* 会话限制检查 */
    if (cfg->max_sessions == 0 || cfg->max_sessions > 100000) {
        return AGENTOS_EINVAL;
    }
    
    /* 超时检查 */
    if (cfg->session_timeout_sec == 0 || cfg->session_timeout_sec > 86400) {
        return AGENTOS_EINVAL;
    }
    
    /* 请求大小检查 */
    if (cfg->max_request_size == 0 || cfg->max_request_size > 104857600) {
        return AGENTOS_EINVAL;
    }
    
    return AGENTOS_SUCCESS;
}

/* ========== 子系统初始化/销毁 ========== */

agentos_error_t server_init_subsystems(dynamic_server_t* server) {
    if (!server) return AGENTOS_EINVAL;
    
    /* 创建会话管理器 */
    server->session_mgr = session_manager_create(
        server->config.max_sessions,
        server->config.session_timeout_sec);
    if (!server->session_mgr) {
        AGENTOS_LOG_ERROR("Failed to create session manager");
        return AGENTOS_ENOMEM;
    }
    
    /* 创建健康检查器 */
    server->health = health_checker_create(server->config.health_interval_sec);
    if (!server->health) {
        AGENTOS_LOG_ERROR("Failed to create health checker");
        session_manager_destroy(server->session_mgr);
        server->session_mgr = NULL;
        return AGENTOS_ENOMEM;
    }
    
    /* 创建可观测性 */
    server->telemetry = telemetry_create();
    if (!server->telemetry) {
        AGENTOS_LOG_ERROR("Failed to create telemetry");
        health_checker_destroy(server->health);
        session_manager_destroy(server->session_mgr);
        server->health = NULL;
        server->session_mgr = NULL;
        return AGENTOS_ENOMEM;
    }
    
    return AGENTOS_SUCCESS;
}

void server_destroy_subsystems(dynamic_server_t* server) {
    if (!server) return;
    
    if (server->telemetry) {
        telemetry_destroy(server->telemetry);
        server->telemetry = NULL;
    }
    
    if (server->health) {
        health_checker_destroy(server->health);
        server->health = NULL;
    }
    
    if (server->session_mgr) {
        session_manager_destroy(server->session_mgr);
        server->session_mgr = NULL;
    }
}

/* ========== 网关管理 ========== */

agentos_error_t server_start_gateways(dynamic_server_t* server) {
    if (!server) return AGENTOS_EINVAL;
    
    agentos_error_t err = AGENTOS_SUCCESS;
    
    /* 创建并启动 HTTP 网关 */
    server->gateways[GATEWAY_TYPE_HTTP] = http_gateway_create(
        server->config.http_host,
        server->config.http_port,
        server);
    if (!server->gateways[GATEWAY_TYPE_HTTP]) {
        AGENTOS_LOG_ERROR("Failed to create HTTP gateway on %s:%u",
            server->config.http_host, server->config.http_port);
        return AGENTOS_ERROR;
    }
    
    err = gateway_start(server->gateways[GATEWAY_TYPE_HTTP]);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to start HTTP gateway");
        gateway_destroy(server->gateways[GATEWAY_TYPE_HTTP]);
        server->gateways[GATEWAY_TYPE_HTTP] = NULL;
        return err;
    }
    server->gateway_count++;
    AGENTOS_LOG_INFO("HTTP gateway started on %s:%u",
        server->config.http_host, server->config.http_port);
    
    /* 创建并启动 WebSocket 网关 */
    server->gateways[GATEWAY_TYPE_WEBSOCKET] = ws_gateway_create(
        server->config.ws_host,
        server->config.ws_port,
        server);
    if (!server->gateways[GATEWAY_TYPE_WEBSOCKET]) {
        AGENTOS_LOG_ERROR("Failed to create WebSocket gateway on %s:%u",
            server->config.ws_host, server->config.ws_port);
        /* HTTP 网关已启动，继续 */
    } else {
        err = gateway_start(server->gateways[GATEWAY_TYPE_WEBSOCKET]);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_ERROR("Failed to start WebSocket gateway");
            gateway_destroy(server->gateways[GATEWAY_TYPE_WEBSOCKET]);
            server->gateways[GATEWAY_TYPE_WEBSOCKET] = NULL;
        } else {
            server->gateway_count++;
            AGENTOS_LOG_INFO("WebSocket gateway started on %s:%u",
                server->config.ws_host, server->config.ws_port);
        }
    }
    
    /* 创建并启动 Stdio 网关（可选） */
    if (server->config.enable_stdio) {
        server->gateways[GATEWAY_TYPE_STDIO] = stdio_gateway_create(server);
        if (!server->gateways[GATEWAY_TYPE_STDIO]) {
            AGENTOS_LOG_WARN("Failed to create stdio gateway");
        } else {
            err = gateway_start(server->gateways[GATEWAY_TYPE_STDIO]);
            if (err != AGENTOS_SUCCESS) {
                AGENTOS_LOG_ERROR("Failed to start stdio gateway");
                gateway_destroy(server->gateways[GATEWAY_TYPE_STDIO]);
                server->gateways[GATEWAY_TYPE_STDIO] = NULL;
            } else {
                server->gateway_count++;
                AGENTOS_LOG_INFO("Stdio gateway started");
            }
        }
    }
    
    /* 至少需要一个网关 */
    if (server->gateway_count == 0) {
        AGENTOS_LOG_ERROR("No gateway could be started");
        return AGENTOS_ERROR;
    }
    
    return AGENTOS_SUCCESS;
}

void server_stop_gateways(dynamic_server_t* server) {
    if (!server) return;
    
    /* 按相反顺序停止网关 */
    for (int i = GATEWAY_TYPE_COUNT - 1; i >= 0; i--) {
        if (server->gateways[i]) {
            gateway_stop(server->gateways[i]);
        }
    }
    
    /* 销毁网关 */
    for (int i = 0; i < GATEWAY_TYPE_COUNT; i++) {
        if (server->gateways[i]) {
            gateway_destroy(server->gateways[i]);
            server->gateways[i] = NULL;
        }
    }
    
    server->gateway_count = 0;
}

/* ========== 统计信息生成 ========== */

agentos_error_t server_generate_stats_json(
    const dynamic_server_t* server,
    char** out_json) {
    
    if (!server || !out_json) return AGENTOS_EINVAL;
    
    uint64_t now_ns = agentos_time_monotonic_ns();
    uint64_t uptime_sec = (now_ns - server->start_time_ns) / 1000000000ULL;
    
    char* json = NULL;
    int len = asprintf(&json,
        "{"
        "\"state\":%d,"
        "\"uptime_sec\":%llu,"
        "\"gateways\":%zu,"
        "\"requests_total\":%llu,"
        "\"requests_failed\":%llu,"
        "\"bytes_received\":%llu,"
        "\"bytes_sent\":%llu,"
        "\"sessions_active\":%zu"
        "}",
        atomic_load(&server->state),
        (unsigned long long)uptime_sec,
        server->gateway_count,
        (unsigned long long)atomic_load(&server->requests_total),
        (unsigned long long)atomic_load(&server->requests_failed),
        (unsigned long long)atomic_load(&server->bytes_received),
        (unsigned long long)atomic_load(&server->bytes_sent),
        session_manager_count(server->session_mgr)
    );
    
    if (len < 0 || !json) {
        return AGENTOS_ENOMEM;
    }
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}

/* ========== 公共 API 实现 ========== */

agentos_error_t dynamic_server_create(
    const dynamic_config_t* config,
    dynamic_server_t** out_server) {
    
    if (!out_server) return AGENTOS_EINVAL;
    
    /* 检查单例 */
    pthread_mutex_lock(&g_instance_lock);
    if (g_server_instance != NULL) {
        pthread_mutex_unlock(&g_instance_lock);
        return AGENTOS_EBUSY;
    }
    pthread_mutex_unlock(&g_instance_lock);
    
    /* 分配服务器实例 */
    dynamic_server_t* server = (dynamic_server_t*)calloc(1, sizeof(dynamic_server_t));
    if (!server) return AGENTOS_ENOMEM;
    
    /* 设置配置 */
    if (config) {
        server->config = *config;
    } else {
        server_set_default_config(&server->config);
    }
    
    /* 验证配置 */
    agentos_error_t err = server_validate_config(&server->config);
    if (err != AGENTOS_SUCCESS) {
        free(server);
        return err;
    }
    
    /* 初始化同步原语 */
    if (pthread_mutex_init(&server->state_lock, NULL) != 0) {
        free(server);
        return AGENTOS_ERROR;
    }
    
    if (pthread_cond_init(&server->state_cond, NULL) != 0) {
        pthread_mutex_destroy(&server->state_lock);
        free(server);
        return AGENTOS_ERROR;
    }
    
    /* 初始化原子变量 */
    atomic_init(&server->state, SERVER_STATE_IDLE);
    atomic_init(&server->shutdown_requested, 0);
    atomic_init(&server->requests_total, 0);
    atomic_init(&server->requests_failed, 0);
    atomic_init(&server->bytes_received, 0);
    atomic_init(&server->bytes_sent, 0);
    
    /* 初始化子系统 */
    err = server_init_subsystems(server);
    if (err != AGENTOS_SUCCESS) {
        pthread_mutex_destroy(&server->state_lock);
        pthread_cond_destroy(&server->state_cond);
        free(server);
        return err;
    }
    
    /* 注册全局实例 */
    pthread_mutex_lock(&g_instance_lock);
    g_server_instance = server;
    pthread_mutex_unlock(&g_instance_lock);
    
    *out_server = server;
    AGENTOS_LOG_INFO("Dynamic server instance created");
    return AGENTOS_SUCCESS;
}

agentos_error_t dynamic_server_start(dynamic_server_t* server) {
    if (!server) return AGENTOS_EINVAL;
    
    /* 检查状态 */
    int expected = SERVER_STATE_IDLE;
    if (!atomic_compare_exchange_strong(&server->state, &expected, SERVER_STATE_STARTING)) {
        if (expected == SERVER_STATE_RUNNING) {
            return AGENTOS_EBUSY;
        }
        return AGENTOS_EINVAL;
    }
    
    /* 记录启动时间 */
    server->start_time_ns = agentos_time_monotonic_ns();
    
    /* 启动网关 */
    agentos_error_t err = server_start_gateways(server);
    if (err != AGENTOS_SUCCESS) {
        atomic_store(&server->state, SERVER_STATE_STOPPED);
        return err;
    }
    
    /* 更新状态 */
    atomic_store(&server->state, SERVER_STATE_RUNNING);
    pthread_cond_broadcast(&server->state_cond);
    
    AGENTOS_LOG_INFO("Dynamic server started with %zu gateways", server->gateway_count);
    
    /* 等待停止信号 */
    pthread_mutex_lock(&server->state_lock);
    while (!atomic_load(&server->shutdown_requested)) {
        pthread_cond_wait(&server->state_cond, &server->state_lock);
    }
    pthread_mutex_unlock(&server->state_lock);
    
    /* 开始停止流程 */
    atomic_store(&server->state, SERVER_STATE_STOPPING);
    
    /* 停止网关 */
    server_stop_gateways(server);
    
    /* 更新状态 */
    atomic_store(&server->state, SERVER_STATE_STOPPED);
    pthread_cond_broadcast(&server->state_cond);
    
    AGENTOS_LOG_INFO("Dynamic server stopped");
    return AGENTOS_SUCCESS;
}

void dynamic_server_stop(dynamic_server_t* server) {
    if (!server) return;
    
    int state = atomic_load(&server->state);
    if (state != SERVER_STATE_RUNNING) {
        return;
    }
    
    AGENTOS_LOG_INFO("Stop requested");
    
    atomic_store(&server->shutdown_requested, 1);
    
    pthread_mutex_lock(&server->state_lock);
    pthread_cond_broadcast(&server->state_cond);
    pthread_mutex_unlock(&server->state_lock);
}

agentos_error_t dynamic_server_wait(
    dynamic_server_t* server,
    uint32_t timeout_ms) {
    
    if (!server) return AGENTOS_EINVAL;
    
    int state = atomic_load(&server->state);
    
    /* 如果未启动，立即返回 */
    if (state == SERVER_STATE_IDLE) {
        return AGENTOS_SUCCESS;
    }
    
    /* 如果已停止，立即返回 */
    if (state == SERVER_STATE_STOPPED) {
        return AGENTOS_SUCCESS;
    }
    
    /* 等待停止 */
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    
    if (timeout_ms > 0) {
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
    }
    
    pthread_mutex_lock(&server->state_lock);
    
    while (atomic_load(&server->state) == SERVER_STATE_RUNNING) {
        if (timeout_ms > 0) {
            int ret = pthread_cond_timedwait(&server->state_cond, &server->state_lock, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&server->state_lock);
                return AGENTOS_ETIMEDOUT;
            }
        } else {
            pthread_cond_wait(&server->state_cond, &server->state_lock);
        }
    }
    
    pthread_mutex_unlock(&server->state_lock);
    return AGENTOS_SUCCESS;
}

void dynamic_server_destroy(dynamic_server_t* server) {
    if (!server) return;
    
    /* 确保服务器已停止 */
    int state = atomic_load(&server->state);
    if (state == SERVER_STATE_RUNNING) {
        dynamic_server_stop(server);
        dynamic_server_wait(server, 5000);
    }
    
    /* 清除全局实例 */
    pthread_mutex_lock(&g_instance_lock);
    if (g_server_instance == server) {
        g_server_instance = NULL;
    }
    pthread_mutex_unlock(&g_instance_lock);
    
    /* 销毁子系统 */
    server_destroy_subsystems(server);
    
    /* 销毁同步原语 */
    pthread_mutex_destroy(&server->state_lock);
    pthread_cond_destroy(&server->state_cond);
    
    free(server);
    AGENTOS_LOG_INFO("Dynamic server instance destroyed");
}

agentos_error_t dynamic_server_get_health(
    dynamic_server_t* server,
    char** out_json) {
    
    if (!server || !out_json) return AGENTOS_EINVAL;
    
    int state = atomic_load(&server->state);
    const char* status = (state == SERVER_STATE_RUNNING) ? "healthy" : "unhealthy";
    
    char* json = NULL;
    int len = asprintf(&json,
        "{\"status\":\"%s\",\"state\":%d,\"gateways\":%zu}",
        status, state, server->gateway_count);
    
    if (len < 0 || !json) {
        return AGENTOS_ENOMEM;
    }
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}

agentos_error_t dynamic_server_get_metrics(
    dynamic_server_t* server,
    char** out_metrics) {
    
    if (!server || !out_metrics) return AGENTOS_EINVAL;
    
    return telemetry_export_metrics(server->telemetry, out_metrics);
}

agentos_error_t dynamic_server_get_stats(
    dynamic_server_t* server,
    char** out_stats) {
    
    if (!server || !out_stats) return AGENTOS_EINVAL;
    
    return server_generate_stats_json(server, out_stats);
}

/* ========== 单例便捷 API ========== */

agentos_error_t dynamic_run(const dynamic_config_t* config) {
    dynamic_server_t* server = NULL;
    agentos_error_t err = dynamic_server_create(config, &server);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }
    
    err = dynamic_server_start(server);
    dynamic_server_destroy(server);
    return err;
}

const char* dynamic_get_version(void) {
    return DYNAMIC_API_VERSION_MAJOR "." DYNAMIC_API_VERSION_MINOR "." DYNAMIC_API_VERSION_PATCH;
}

uint32_t dynamic_get_abi_version(void) {
    return DYNAMIC_ABI_VERSION;
}
