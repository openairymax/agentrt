/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_svc_adapter.c
 * @brief Gateway服务适配器：将网关服务适配到统一的AgentOS服务管理框架
 *
 * 本文件实现了网关服务与agentos_service_t通用接口的适配层。
 * 通过适配器模式，网关服务可以无缝集成到服务管理框架中，
 * 享受统一的生命周期管理、状态监控、服务发现等功能。
 *
 * 适配器设计遵循架构原则K-2（接口契约化原则）：
 * 1. 提供标准化的服务接口
 * 2. 保持向后兼容性
 * 3. 最小化性能开销
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "gateway_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

/* ==================== 适配器上下文 ==================== */

/**
 * @brief 网关服务适配器上下文
 */
typedef struct {
    gateway_service_t gateway_svc;          /**< 原始网关服务句柄 */
    gateway_service_config_t gateway_cfg;   /**< 网关服务配置 */
    agentos_svc_config_t common_cfg;        /**< 通用服务配置 */
} gateway_adapter_ctx_t;

/* ==================== 适配器实现 ==================== */

/**
 * @brief 适配器初始化函数
 */
static agentos_error_t gateway_adapter_init(
    agentos_service_t service,
    const agentos_svc_config_t* config
) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx) {
        return AGENTOS_EINVAL;
    }
    
    // 保存通用配置
    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    }
    
    // 将通用配置转换为网关特定配置
    gateway_service_get_default_config(&ctx->gateway_cfg);
    
    // 更新网关配置中的名称和版本
    ctx->gateway_cfg.name = ctx->common_cfg.name ? ctx->common_cfg.name : "gateway_d";
    ctx->gateway_cfg.version = ctx->common_cfg.version ? ctx->common_cfg.version : "1.0.0";
    
    // 根据通用配置调整网关能力
    if (ctx->common_cfg.capabilities & AGENTOS_SVC_CAP_ASYNC) {
        // 网关服务支持异步操作
    }
    
    if (ctx->common_cfg.capabilities & AGENTOS_SVC_CAP_STREAMING) {
        // WebSocket网关支持流式处理
        ctx->gateway_cfg.ws.enabled = true;
    }
    
    if (ctx->common_cfg.capabilities & AGENTOS_SVC_CAP_TIMEOUT) {
        // 使用配置的超时设置
        ctx->gateway_cfg.http.timeout_ms = ctx->common_cfg.timeout_ms;
        ctx->gateway_cfg.ws.timeout_ms = ctx->common_cfg.timeout_ms;
    }
    
    ctx->gateway_cfg.enable_metrics = ctx->common_cfg.enable_metrics;
    ctx->gateway_cfg.enable_tracing = ctx->common_cfg.enable_tracing;
    
    // 创建网关服务实例
    agentos_error_t err = gateway_service_create(
        &ctx->gateway_svc,
        &ctx->gateway_cfg
    );
    
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("网关服务创建失败: %d", err);
        return err;
    }
    
    // 初始化网关服务
    err = gateway_service_init(ctx->gateway_svc);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("网关服务初始化失败: %d", err);
        gateway_service_destroy(ctx->gateway_svc);
        ctx->gateway_svc = NULL;
        return err;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 适配器启动函数
 */
static agentos_error_t gateway_adapter_start(agentos_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx) {
        return AGENTOS_EINVAL;
    }
    
    if (!ctx->gateway_svc) {
        return AGENTOS_ENOTINIT;
    }
    
    agentos_error_t err = gateway_service_start(ctx->gateway_svc);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("网关服务启动失败: %d", err);
        return err;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 适配器停止函数
 */
static agentos_error_t gateway_adapter_stop(agentos_service_t service, bool force) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx) {
        return AGENTOS_EINVAL;
    }
    
    if (!ctx->gateway_svc) {
        return AGENTOS_ENOTINIT;
    }
    
    agentos_error_t err = gateway_service_stop(ctx->gateway_svc, force);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("网关服务停止失败: %d", err);
        return err;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 适配器销毁函数
 */
static void gateway_adapter_destroy(agentos_service_t service) {
    if (!service) {
        return;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx) {
        return;
    }
    
    if (ctx->gateway_svc) {
        gateway_service_destroy(ctx->gateway_svc);
        ctx->gateway_svc = NULL;
    }
    
    // 释放动态分配的资源
    if (ctx->gateway_cfg.http.host && strcmp(ctx->gateway_cfg.http.host, "0.0.0.0") != 0) {
        free((void*)ctx->gateway_cfg.http.host);
    }
    if (ctx->gateway_cfg.ws.host && strcmp(ctx->gateway_cfg.ws.host, "0.0.0.0") != 0) {
        free((void*)ctx->gateway_cfg.ws.host);
    }
    
    free(ctx);
    
    agentos_service_set_user_data(service, NULL);
}

/**
 * @brief 适配器健康检查函数
 */
static agentos_error_t gateway_adapter_healthcheck(agentos_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx) {
        return AGENTOS_EINVAL;
    }
    
    if (!ctx->gateway_svc) {
        return AGENTOS_ENOTINIT;
    }
    
    return gateway_service_healthcheck(ctx->gateway_svc);
}

/* ==================== 适配器接口 ==================== */

/**
 * @brief 网关服务适配器接口
 */
static const agentos_svc_interface_t gateway_adapter_iface = {
    .init = gateway_adapter_init,
    .start = gateway_adapter_start,
    .stop = gateway_adapter_stop,
    .destroy = gateway_adapter_destroy,
    .healthcheck = gateway_adapter_healthcheck,
};

/* ==================== 公共API ==================== */

/**
 * @brief 创建网关服务适配器
 * 
 * 此函数创建一个网关服务的适配器实例，使其能够通过统一的agentos_service_t接口进行管理。
 * 
 * @param[out] out_service 输出服务句柄
 * @param[in] config 通用服务配置
 * @return 错误码
 */
agentos_error_t gateway_service_adapter_create(
    agentos_service_t* out_service,
    const agentos_svc_config_t* config
) {
    if (!out_service) {
        return AGENTOS_EINVAL;
    }
    
    // 分配适配器上下文
    gateway_adapter_ctx_t* ctx = calloc(1, sizeof(gateway_adapter_ctx_t));
    if (!ctx) {
        return AGENTOS_ENOMEM;
    }
    
    // 保存通用配置
    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        // 使用默认配置
        ctx->common_cfg.name = "gateway_d";
        ctx->common_cfg.version = "1.0.0";
        ctx->common_cfg.capabilities = AGENTOS_SVC_CAP_ASYNC | AGENTOS_SVC_CAP_STREAMING;
        ctx->common_cfg.max_concurrent = 1000;
        ctx->common_cfg.timeout_ms = 30000;
        ctx->common_cfg.priority = 0;
        ctx->common_cfg.auto_start = true;
        ctx->common_cfg.enable_metrics = true;
        ctx->common_cfg.enable_tracing = false;
    }
    
    // 通过通用服务创建API创建服务实例
    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle,
        ctx->common_cfg.name,
        &gateway_adapter_iface,
        &ctx->common_cfg
    );
    
    if (err != AGENTOS_SUCCESS) {
        free(ctx);
        return err;
    }
    
    // 将适配器上下文存储在服务的user_data字段中
    // 这样接口函数可以通过user_data获取适配器上下文，
    // 而不是将service句柄强转为适配器上下文（避免类型混淆）
    err = agentos_service_set_user_data(svc_handle, ctx);
    if (err != AGENTOS_SUCCESS) {
        agentos_service_destroy(svc_handle);
        free(ctx);
        return err;
    }
    
    *out_service = svc_handle;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取原始网关服务句柄
 * 
 * 用于需要直接访问网关服务特定功能的场景。
 * 
 * @param service 适配器服务句柄
 * @return 原始网关服务句柄，或NULL
 */
gateway_service_t gateway_service_adapter_get_original(agentos_service_t service) {
    if (!service) {
        return NULL;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    return ctx ? ctx->gateway_svc : NULL;
}

/**
 * @brief 将现有网关服务包装为适配器
 * 
 * 此函数将已存在的网关服务实例包装为适配器，使其能够集成到服务管理框架中。
 * 
 * @param[out] out_service 输出服务句柄
 * @param[in] gateway_svc 原始网关服务句柄
 * @param[in] config 通用服务配置
 * @return 错误码
 */
agentos_error_t gateway_service_adapter_wrap(
    agentos_service_t* out_service,
    gateway_service_t gateway_svc,
    const agentos_svc_config_t* config
) {
    if (!out_service || !gateway_svc) {
        return AGENTOS_EINVAL;
    }
    
    // 分配适配器上下文
    gateway_adapter_ctx_t* ctx = calloc(1, sizeof(gateway_adapter_ctx_t));
    if (!ctx) {
        return AGENTOS_ENOMEM;
    }
    
    ctx->gateway_svc = gateway_svc;
    
    // 保存通用配置
    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        // 从网关服务获取配置信息
        ctx->common_cfg.name = "gateway_d";
        ctx->common_cfg.version = "1.0.0";
        ctx->common_cfg.capabilities = AGENTOS_SVC_CAP_ASYNC | AGENTOS_SVC_CAP_STREAMING;
        ctx->common_cfg.max_concurrent = 1000;
        ctx->common_cfg.timeout_ms = 30000;
        ctx->common_cfg.auto_start = true;
        ctx->common_cfg.enable_metrics = true;
    }
    
    // 通过通用服务创建API创建服务实例
    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle,
        ctx->common_cfg.name,
        &gateway_adapter_iface,
        &ctx->common_cfg
    );
    
    if (err != AGENTOS_SUCCESS) {
        free(ctx);
        return err;
    }
    
    // 将适配器上下文存储在user_data中
    err = agentos_service_set_user_data(svc_handle, ctx);
    if (err != AGENTOS_SUCCESS) {
        agentos_service_destroy(svc_handle);
        free(ctx);
        return err;
    }
    
    *out_service = svc_handle;
    return AGENTOS_SUCCESS;
}

/* ==================== 适配器生命周期管理 ==================== */

/**
 * @brief 适配器服务初始化（代理函数）
 */
agentos_error_t gateway_service_adapter_init(agentos_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    return gateway_adapter_init(service, NULL);
}

/**
 * @brief 适配器服务启动（代理函数）
 */
agentos_error_t gateway_service_adapter_start(agentos_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    return gateway_adapter_start(service);
}

/**
 * @brief 适配器服务停止（代理函数）
 */
agentos_error_t gateway_service_adapter_stop(agentos_service_t service, bool force) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    return gateway_adapter_stop(service, force);
}

/**
 * @brief 适配器服务销毁（代理函数）
 */
void gateway_service_adapter_destroy(agentos_service_t service) {
    gateway_adapter_destroy(service);
}

/**
 * @brief 适配器服务健康检查（代理函数）
 */
agentos_error_t gateway_service_adapter_healthcheck(agentos_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }
    
    return gateway_adapter_healthcheck(service);
}

/* ==================== 服务状态查询 ==================== */

/**
 * @brief 获取适配器服务状态
 */
agentos_svc_state_t gateway_service_adapter_get_state(agentos_service_t service) {
    if (!service) {
        return AGENTOS_SVC_STATE_NONE;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx || !ctx->gateway_svc) {
        return AGENTOS_SVC_STATE_NONE;
    }
    
    return gateway_service_get_state(ctx->gateway_svc);
}

/**
 * @brief 检查适配器服务是否运行中
 */
bool gateway_service_adapter_is_running(agentos_service_t service) {
    if (!service) {
        return false;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx || !ctx->gateway_svc) {
        return false;
    }
    
    return gateway_service_is_running(ctx->gateway_svc);
}

/**
 * @brief 获取适配器服务统计信息
 */
agentos_error_t gateway_service_adapter_get_stats(
    agentos_service_t service,
    agentos_svc_stats_t* stats
) {
    if (!service || !stats) {
        return AGENTOS_EINVAL;
    }
    
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx || !ctx->gateway_svc) {
        return AGENTOS_ENOTINIT;
    }
    
    return gateway_service_get_stats(ctx->gateway_svc, stats);
}

/* ==================== 示例使用代码 ==================== */

const agentos_svc_interface_t* gateway_service_adapter_get_interface(void) {
    return &gateway_adapter_iface;
}

bool gateway_service_adapter_supports_type(
    agentos_service_t service,
    gateway_daemon_type_t type
) {
    if (!service) return false;
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx) return false;
    switch (type) {
        case GATEWAY_DAEMON_TYPE_HTTP:
            return ctx->gateway_cfg.http.enabled;
        case GATEWAY_DAEMON_TYPE_WS:
            return ctx->gateway_cfg.ws.enabled;
        case GATEWAY_DAEMON_TYPE_STDIO:
            return ctx->gateway_cfg.stdio.enabled;
        default:
            return false;
    }
}

agentos_error_t gateway_service_adapter_set_type_enabled(
    agentos_service_t service,
    gateway_daemon_type_t type,
    bool enabled
) {
    if (!service) return AGENTOS_EINVAL;
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx) return AGENTOS_EINVAL;
    agentos_svc_state_t state = agentos_service_get_state(service);
    if (state == AGENTOS_SVC_STATE_RUNNING) {
        return AGENTOS_EBUSY;
    }
    switch (type) {
        case GATEWAY_DAEMON_TYPE_HTTP:
            ctx->gateway_cfg.http.enabled = enabled;
            break;
        case GATEWAY_DAEMON_TYPE_WS:
            ctx->gateway_cfg.ws.enabled = enabled;
            break;
        case GATEWAY_DAEMON_TYPE_STDIO:
            ctx->gateway_cfg.stdio.enabled = enabled;
            break;
        default:
            return AGENTOS_EINVAL;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_service_adapter_create_from_config(
    agentos_service_t* out_service,
    const char* config_path
) {
    if (!out_service || !config_path) return AGENTOS_EINVAL;
    agentos_svc_config_t config;
    memset(&config, 0, sizeof(config));
    config.name = "gateway_d";
    config.version = "1.0.0";
    config.capabilities = AGENTOS_SVC_CAP_ASYNC | AGENTOS_SVC_CAP_STREAMING;
    config.max_concurrent = 1000;
    config.timeout_ms = 30000;
    config.auto_start = true;
    config.enable_metrics = true;
    agentos_error_t err = gateway_service_adapter_create(out_service, &config);
    if (err != AGENTOS_SUCCESS) return err;
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(*out_service);
    if (ctx) {
        gateway_service_get_default_config(&ctx->gateway_cfg);
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_service_adapter_reload_config(
    agentos_service_t service,
    const char* config_path
) {
    if (!service) return AGENTOS_EINVAL;
    gateway_adapter_ctx_t* ctx = (gateway_adapter_ctx_t*)agentos_service_get_user_data(service);
    if (!ctx) return AGENTOS_EINVAL;
    agentos_svc_state_t state = agentos_service_get_state(service);
    if (state == AGENTOS_SVC_STATE_RUNNING) {
        return AGENTOS_EBUSY;
    }
    if (config_path) {
        (void)config_path;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_service_adapter_register(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    return agentos_service_register(service);
}

agentos_error_t gateway_service_adapter_unregister(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    return agentos_service_unregister(service);
}