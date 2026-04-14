/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file service.c
 * @brief Gateway守护进程服务实现
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "gateway_service.h"
#include "svc_common.h"
#include "svc_logger.h"
#include "svc_config.h"

#include "gateway.h"
#include "syscalls.h"

#include <stdlib.h>
#include <string.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include <agentos/atomic_compat.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

/* ==================== 内部结构 ==================== */

/**
 * @brief 网关实例包装
 */
typedef struct {
    gateway_t* gateway;             /**< 网关实例 */
    gateway_daemon_type_t type;     /**< 网关类型 */
    bool running;                   /**< 运行状态 */
} gateway_instance_t;

/**
 * @brief 网关服务内部结构
 */
#ifdef _WIN32
/* Windows 平台原子类型兼容定义 */
typedef atomic_uint64_t atomic_uint_fast64_t;
#endif
struct gateway_service_s {
    gateway_service_config_t config;    /**< 服务配置 */
    agentos_svc_state_t state;          /**< 服务状态 */
    
    gateway_instance_t http;            /**< HTTP网关实例 */
    gateway_instance_t ws;              /**< WebSocket网关实例 */
    gateway_instance_t stdio;           /**< Stdio网关实例 */
    
    atomic_uint_fast64_t requests_total;    /**< 总请求数 */
    atomic_uint_fast64_t requests_failed;   /**< 失败请求数 */
};

/* ==================== 配置管理 ==================== */

/**
 * @brief 获取默认配置
 */
void gateway_service_get_default_config(gateway_service_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    config->name = "gateway_d";
    config->version = "1.0.0";
    
    config->http.type = GATEWAY_DAEMON_TYPE_HTTP;
    config->http.host = "0.0.0.0";
    config->http.port = 8080;
    config->http.enabled = true;
    config->http.max_request_size = 10 * 1024 * 1024;
    config->http.timeout_ms = 30000;
    
    config->ws.type = GATEWAY_DAEMON_TYPE_WS;
    config->ws.host = "0.0.0.0";
    config->ws.port = 8081;
    config->ws.enabled = false;
    config->ws.max_request_size = 10 * 1024 * 1024;
    config->ws.timeout_ms = 60000;
    
    config->stdio.type = GATEWAY_DAEMON_TYPE_STDIO;
    config->stdio.enabled = false;
    config->stdio.max_request_size = 1024 * 1024;
    config->stdio.timeout_ms = 0;
    
    config->enable_metrics = false;
    config->enable_tracing = false;
    config->shutdown_timeout_ms = 5000;
}

/**
 * @brief 从配置文件加载配置
 */
agentos_error_t gateway_service_load_config(
    gateway_service_config_t* config,
    const char* config_path
) {
    if (!config || !config_path) {
        return AGENTOS_EINVAL;
    }
    
    gateway_service_get_default_config(config);
    
    svc_config_t* cfg = NULL;
    agentos_error_t err = svc_config_load(&cfg, config_path);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }
    
    const char* str_val = NULL;
    int int_val = 0;
    
    str_val = svc_config_get_string(cfg, "gateway.http.host");
    if (str_val) config->http.host = strdup(str_val);
    
    int_val = svc_config_get_int(cfg, "gateway.http.port");
    if (int_val > 0) config->http.port = (uint16_t)int_val;
    
    int_val = svc_config_get_bool(cfg, "gateway.http.enabled");
    config->http.enabled = (int_val != 0);
    
    str_val = svc_config_get_string(cfg, "gateway.ws.host");
    if (str_val) config->ws.host = strdup(str_val);
    
    int_val = svc_config_get_int(cfg, "gateway.ws.port");
    if (int_val > 0) config->ws.port = (uint16_t)int_val;
    
    int_val = svc_config_get_bool(cfg, "gateway.ws.enabled");
    config->ws.enabled = (int_val != 0);
    
    int_val = svc_config_get_bool(cfg, "gateway.stdio.enabled");
    config->stdio.enabled = (int_val != 0);
    
    int_val = svc_config_get_bool(cfg, "gateway.metrics.enabled");
    config->enable_metrics = (int_val != 0);
    
    int_val = svc_config_get_bool(cfg, "gateway.tracing.enabled");
    config->enable_tracing = (int_val != 0);
    
    svc_config_destroy(cfg);
    
    return AGENTOS_SUCCESS;
}

/* ==================== 服务生命周期 ==================== */

/**
 * @brief 创建网关服务
 */
agentos_error_t gateway_service_create(
    gateway_service_t* service,
    const gateway_service_config_t* config
) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    gateway_service_t svc = calloc(1, sizeof(struct gateway_service_s));
    if (!svc) {
        return AGENTOS_ENOMEM;
    }
    
    if (config) {
        memcpy(&svc->config, config, sizeof(gateway_service_config_t));
    } else {
        gateway_service_get_default_config(&svc->config);
    }
    
    svc->state = AGENTOS_SVC_STATE_CREATED;
    
    atomic_init(&svc->requests_total, 0);
    atomic_init(&svc->requests_failed, 0);
    
    *service = svc;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁网关服务
 */
void gateway_service_destroy(gateway_service_t service) {
    if (!service) return;
    
    if (service->state != AGENTOS_SVC_STATE_STOPPED) {
        gateway_service_stop(service, true);
    }
    
    if (service->http.gateway) {
        gateway_destroy(service->http.gateway);
    }
    if (service->ws.gateway) {
        gateway_destroy(service->ws.gateway);
    }
    if (service->stdio.gateway) {
        gateway_destroy(service->stdio.gateway);
    }
    
    free(service);
}

/**
 * @brief 初始化网关服务
 */
agentos_error_t gateway_service_init(gateway_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    if (service->state != AGENTOS_SVC_STATE_CREATED) {
        return AGENTOS_ESTATE;
    }
    
    service->state = AGENTOS_SVC_STATE_INITIALIZING;
    
    agentos_error_t err = AGENTOS_SUCCESS;
    
    if (service->config.http.enabled) {
        service->http.gateway = gateway_http_create(
            service->config.http.host,
            service->config.http.port
        );
        if (!service->http.gateway) {
            service->state = AGENTOS_SVC_STATE_ERROR;
            return AGENTOS_ENOMEM;
        }
        service->http.type = GATEWAY_DAEMON_TYPE_HTTP;
        service->http.running = false;
    }
    
    if (service->config.ws.enabled) {
        service->ws.gateway = gateway_ws_create(
            service->config.ws.host,
            service->config.ws.port
        );
        if (!service->ws.gateway) {
            service->state = AGENTOS_SVC_STATE_ERROR;
            return AGENTOS_ENOMEM;
        }
        service->ws.type = GATEWAY_DAEMON_TYPE_WS;
        service->ws.running = false;
    }
    
    if (service->config.stdio.enabled) {
        service->stdio.gateway = gateway_stdio_create();
        if (!service->stdio.gateway) {
            service->state = AGENTOS_SVC_STATE_ERROR;
            return AGENTOS_ENOMEM;
        }
        service->stdio.type = GATEWAY_DAEMON_TYPE_STDIO;
        service->stdio.running = false;
    }
    
    service->state = AGENTOS_SVC_STATE_READY;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 启动网关服务
 */
agentos_error_t gateway_service_start(gateway_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    if (service->state != AGENTOS_SVC_STATE_READY) {
        return AGENTOS_ESTATE;
    }
    
    agentos_error_t err = AGENTOS_SUCCESS;
    
    service->state = AGENTOS_SVC_STATE_RUNNING;
    
    if (service->http.gateway) {
        err = gateway_start(service->http.gateway);
        if (err == AGENTOS_SUCCESS) {
            service->http.running = true;
        } else {
            service->state = AGENTOS_SVC_STATE_ERROR;
            return err;
        }
    }
    
    if (service->ws.gateway) {
        err = gateway_start(service->ws.gateway);
        if (err == AGENTOS_SUCCESS) {
            service->ws.running = true;
        } else {
            service->state = AGENTOS_SVC_STATE_ERROR;
            return err;
        }
    }
    
    if (service->stdio.gateway) {
        err = gateway_start(service->stdio.gateway);
        if (err == AGENTOS_SUCCESS) {
            service->stdio.running = true;
        } else {
            service->state = AGENTOS_SVC_STATE_ERROR;
            return err;
        }
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 停止网关服务
 */
agentos_error_t gateway_service_stop(
    gateway_service_t service,
    bool force
) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    if (service->state != AGENTOS_SVC_STATE_RUNNING) {
        return AGENTOS_ESTATE;
    }
    
    service->state = AGENTOS_SVC_STATE_STOPPING;
    
    if (service->stdio.gateway && service->stdio.running) {
        gateway_stop(service->stdio.gateway);
        service->stdio.running = false;
    }
    
    if (service->ws.gateway && service->ws.running) {
        gateway_stop(service->ws.gateway);
        service->ws.running = false;
    }
    
    if (service->http.gateway && service->http.running) {
        gateway_stop(service->http.gateway);
        service->http.running = false;
    }
    
    service->state = AGENTOS_SVC_STATE_STOPPED;
    
    return AGENTOS_SUCCESS;
}

/* ==================== 状态查询 ==================== */

/**
 * @brief 获取服务状态
 */
agentos_svc_state_t gateway_service_get_state(gateway_service_t service) {
    if (!service) {
        return AGENTOS_SVC_STATE_NONE;
    }
    return service->state;
}

/**
 * @brief 检查服务是否运行中
 */
bool gateway_service_is_running(gateway_service_t service) {
    if (!service) {
        return false;
    }
    return service->state == AGENTOS_SVC_STATE_RUNNING;
}

/**
 * @brief 获取服务统计信息
 */
agentos_error_t gateway_service_get_stats(
    gateway_service_t service,
    agentos_svc_stats_t* stats
) {
    if (!service || !stats) {
        return AGENTOS_EINVAL;
    }
    
    memset(stats, 0, sizeof(*stats));
    
    stats->request_count = atomic_load(&service->requests_total);
    
    char* http_stats = NULL;
    if (service->http.gateway) {
        gateway_get_stats(service->http.gateway, &http_stats);
        if (http_stats) free(http_stats);
    }
    
    char* ws_stats = NULL;
    if (service->ws.gateway) {
        gateway_get_stats(service->ws.gateway, &ws_stats);
        if (ws_stats) free(ws_stats);
    }
    
    char* stdio_stats = NULL;
    if (service->stdio.gateway) {
        gateway_get_stats(service->stdio.gateway, &stdio_stats);
        if (stdio_stats) free(stdio_stats);
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 执行健康检查
 */
agentos_error_t gateway_service_healthcheck(gateway_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    if (service->state != AGENTOS_SVC_STATE_RUNNING) {
        return AGENTOS_ESTATE;
    }
    
    return AGENTOS_SUCCESS;
}
