/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file gateway_service.h
 * @brief Gateway守护进程服务接口
 *
 * gateway_d 是 AgentRT 的网关守护进程，负责：
 * 1. 管理 HTTP/WebSocket/Stdio 网关实例
 * 2. 提供统一的配置管理
 * 3. 实现服务生命周期管理
 * 4. 对接系统调用接口
 *
 * 架构定位：
 *   gateway_d/ → agentrt/gateway/ → agentrt/atoms/syscall/
 *       ↑           ↑
 *    守护进程    协议转换层
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_DAEMON_GATEWAY_SERVICE_H
#define AGENTRT_DAEMON_GATEWAY_SERVICE_H

#include "svc_common.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 网关类型 ==================== */

/**
 * @brief 网关类型枚举
 */
typedef enum {
    GATEWAY_DAEMON_TYPE_HTTP = 0, /**< HTTP网关 */
    GATEWAY_DAEMON_TYPE_WS,       /**< WebSocket网关 */
    GATEWAY_DAEMON_TYPE_STDIO     /**< Stdio网关 */
} gateway_daemon_type_t;

/* ==================== 网关配置 ==================== */

/**
 * @brief 单个网关配置
 */
typedef struct {
    gateway_daemon_type_t type; /**< 网关类型 */
    const char *host;           /**< 监听地址 */
    uint16_t port;              /**< 监听端口 */
    bool enabled;               /**< 是否启用 */
    size_t max_request_size;    /**< 最大请求大小 */
    uint32_t timeout_ms;        /**< 超时时间 */
} gateway_daemon_config_t;

/**
 * @brief 网关服务配置
 */
typedef struct {
    const char *name;    /**< 服务名称 */
    const char *version; /**< 服务版本 */

    gateway_daemon_config_t http;  /**< HTTP网关配置 */
    gateway_daemon_config_t ws;    /**< WebSocket网关配置 */
    gateway_daemon_config_t stdio; /**< Stdio网关配置 */

    bool enable_metrics;          /**< 启用指标收集 */
    bool enable_tracing;          /**< 启用追踪 */
    uint32_t shutdown_timeout_ms; /**< 关闭超时 */
} gateway_service_config_t;

/* ==================== 网关服务句柄 ==================== */

/**
 * @brief 网关服务句柄
 */
typedef struct gateway_service_s *gateway_service_t;

/* ==================== 服务生命周期 ==================== */

/**
 * @brief 创建网关服务
 * @param[out] service 服务句柄输出
 * @param[in] config 服务配置
 * @return AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t gateway_service_create(gateway_service_t *service,
                                                   const gateway_service_config_t *config);

/**
 * @brief 销毁网关服务
 * @param[in] service 服务句柄
 */
AGENTRT_API void gateway_service_destroy(gateway_service_t service);

/**
 * @brief 初始化网关服务
 * @param[in] service 服务句柄
 * @return AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t gateway_service_init(gateway_service_t service);

/**
 * @brief 启动网关服务
 * @param[in] service 服务句柄
 * @return AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t gateway_service_start(gateway_service_t service);

/**
 * @brief 停止网关服务
 * @param[in] service 服务句柄
 * @param[in] force 是否强制停止
 * @return AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t gateway_service_stop(gateway_service_t service, bool force);

/* ==================== 状态查询 ==================== */

/**
 * @brief 获取服务状态
 * @param[in] service 服务句柄
 * @return 服务状态
 */
AGENTRT_API agentrt_svc_state_t gateway_service_get_state(gateway_service_t service);

/**
 * @brief 检查服务是否运行中
 * @param[in] service 服务句柄
 * @return true 运行中
 */
AGENTRT_API bool gateway_service_is_running(gateway_service_t service);

/**
 * @brief 获取服务统计信息
 * @param[in] service 服务句柄
 * @param[out] stats 统计信息输出
 * @return AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t gateway_service_get_stats(gateway_service_t service,
                                                      agentrt_svc_stats_t *stats);

/**
 * @brief 执行健康检查
 * @param[in] service 服务句柄
 * @return AGENTRT_SUCCESS 健康
 */
AGENTRT_API agentrt_error_t gateway_service_healthcheck(gateway_service_t service);

/* ==================== 配置管理 ==================== */

/**
 * @brief 从配置文件加载配置
 * @param[out] config 配置输出
 * @param[in] config_path 配置文件路径
 * @return AGENTRT_SUCCESS 成功
 */
AGENTRT_API agentrt_error_t gateway_service_load_config(gateway_service_config_t *config,
                                                        const char *config_path);

/**
 * @brief 获取默认配置
 * @param[out] config 配置输出
 */
AGENTRT_API void gateway_service_get_default_config(gateway_service_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_GATEWAY_SERVICE_H */
