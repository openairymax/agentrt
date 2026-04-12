/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_svc_adapter.h
 * @brief Gateway服务适配器头文件
 *
 * 提供网关服务与AgentOS统一服务管理框架的适配接口。
 * 通过本适配器，网关服务可以无缝集成到服务注册表、服务发现、
 * 统一生命周期管理等框架功能中。
 *
 * 设计原则：
 * 1. 接口契约化 - 提供标准化的服务接口
 * 2. 向后兼容 - 保持与现有网关服务的兼容性
 * 3. 最小侵入 - 对原始服务代码影响最小化
 * 4. 可扩展性 - 支持未来服务功能的扩展
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_DAEMON_GATEWAY_SVC_ADAPTER_H
#define AGENTOS_DAEMON_GATEWAY_SVC_ADAPTER_H

#include "gateway_service.h"
#include "svc_common.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 适配器创建与销毁 ==================== */

/**
 * @brief 创建网关服务适配器
 *
 * 创建一个新的网关服务适配器实例，该实例实现了agentos_service_t接口，
 * 可以通过统一的AgentOS服务管理框架进行管理。
 *
 * @param[out] out_service 输出服务句柄
 * @param[in] config 通用服务配置（可为NULL，使用默认配置）
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 否（需在单线程环境下调用）
 * @reentrant 是
 *
 * @example
 * @code
 * agentos_service_t svc = NULL;
 * agentos_svc_config_t config = {
 *     .name = "gateway_service",
 *     .version = "1.0.0",
 *     .capabilities = AGENTOS_SVC_CAP_ASYNC | AGENTOS_SVC_CAP_STREAMING,
 *     .max_concurrent = 1000,
 *     .timeout_ms = 30000,
 *     .auto_start = true,
 *     .enable_metrics = true
 * };
 *
 * agentos_error_t err = gateway_service_adapter_create(&svc, &config);
 * if (err == AGENTOS_SUCCESS) {
 *     // 使用agentos_service_*函数管理服务
 *     agentos_service_init(svc);
 *     agentos_service_start(svc);
 * }
 * @endcode
 */
AGENTOS_API agentos_error_t gateway_service_adapter_create(
    agentos_service_t* out_service,
    const agentos_svc_config_t* config
);

/**
 * @brief 将现有网关服务包装为适配器
 *
 * 将已存在的网关服务实例包装为适配器，使其能够集成到服务管理框架中。
 * 包装后，原始服务句柄将由适配器管理，不应再直接使用原始句柄。
 *
 * @param[out] out_service 输出服务句柄
 * @param[in] gateway_svc 原始网关服务句柄
 * @param[in] config 通用服务配置（可为NULL，使用默认配置）
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @warning 包装后，原始服务句柄的生命周期由适配器管理，
 *          不应再调用gateway_service_destroy等函数。
 *
 * @threadsafe 否
 * @reentrant 是
 */
AGENTOS_API agentos_error_t gateway_service_adapter_wrap(
    agentos_service_t* out_service,
    gateway_service_t gateway_svc,
    const agentos_svc_config_t* config
);

/**
 * @brief 获取原始网关服务句柄
 *
 * 用于需要直接访问网关服务特定功能的场景。
 * 获取的句柄不应被修改或销毁，应由适配器管理其生命周期。
 *
 * @param service 适配器服务句柄
 * @return 原始网关服务句柄，或NULL（如果无效）
 *
 * @threadsafe 是（前提是服务状态不变）
 * @reentrant 是
 */
AGENTOS_API gateway_service_t gateway_service_adapter_get_original(
    agentos_service_t service
);

/* ==================== 适配器生命周期管理 ==================== */

/**
 * @brief 适配器服务初始化
 *
 * 初始化适配器服务，创建底层网关服务实例。
 * 如果使用gateway_service_adapter_wrap创建适配器，此函数无操作。
 *
 * @param service 适配器服务句柄
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t gateway_service_adapter_init(agentos_service_t service);

/**
 * @brief 适配器服务启动
 *
 * 启动底层网关服务。
 *
 * @param service 适配器服务句柄
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t gateway_service_adapter_start(agentos_service_t service);

/**
 * @brief 适配器服务停止
 *
 * 停止底层网关服务。
 *
 * @param service 适配器服务句柄
 * @param force 是否强制停止
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 是（底层网关服务需支持并发停止）
 * @reentrant 否
 */
AGENTOS_API agentos_error_t gateway_service_adapter_stop(
    agentos_service_t service,
    bool force
);

/**
 * @brief 适配器服务销毁
 *
 * 销毁适配器及其管理的底层网关服务。
 *
 * @param service 适配器服务句柄
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API void gateway_service_adapter_destroy(agentos_service_t service);

/**
 * @brief 适配器服务健康检查
 *
 * 执行底层网关服务的健康检查。
 *
 * @param service 适配器服务句柄
 * @return AGENTOS_SUCCESS 健康，其他值为不健康
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t gateway_service_adapter_healthcheck(agentos_service_t service);

/* ==================== 服务状态查询 ==================== */

/**
 * @brief 获取适配器服务状态
 *
 * 获取底层网关服务的当前状态。
 *
 * @param service 适配器服务句柄
 * @return 服务状态枚举值
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_svc_state_t gateway_service_adapter_get_state(agentos_service_t service);

/**
 * @brief 检查适配器服务是否运行中
 *
 * 检查底层网关服务是否处于运行状态。
 *
 * @param service 适配器服务句柄
 * @return true 运行中，false 未运行
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API bool gateway_service_adapter_is_running(agentos_service_t service);

/**
 * @brief 获取适配器服务统计信息
 *
 * 获取底层网关服务的统计信息。
 *
 * @param service 适配器服务句柄
 * @param stats 统计信息输出
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t gateway_service_adapter_get_stats(
    agentos_service_t service,
    agentos_svc_stats_t* stats
);

/* ==================== 工具函数 ==================== */

/**
 * @brief 创建网关服务适配器接口
 *
 * 返回网关服务的标准适配器接口，可用于agentos_service_create函数。
 * 此函数主要用于高级用法，通常使用gateway_service_adapter_create即可。
 *
 * @return 网关服务适配器接口结构体
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API const agentos_svc_interface_t* gateway_service_adapter_get_interface(void);

/**
 * @brief 检查是否支持特定网关类型
 *
 * 检查适配器是否支持特定的网关类型。
 *
 * @param service 适配器服务句柄
 * @param type 网关类型
 * @return true 支持，false 不支持
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API bool gateway_service_adapter_supports_type(
    agentos_service_t service,
    gateway_daemon_type_t type
);

/**
 * @brief 启用/禁用特定网关类型
 *
 * 动态启用或禁用特定的网关类型。
 * 只能在服务停止状态下调用。
 *
 * @param service 适配器服务句柄
 * @param type 网关类型
 * @param enabled 是否启用
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t gateway_service_adapter_set_type_enabled(
    agentos_service_t service,
    gateway_daemon_type_t type,
    bool enabled
);

/* ==================== 配置管理 ==================== */

/**
 * @brief 从配置文件创建网关服务适配器
 *
 * 从配置文件加载配置并创建网关服务适配器。
 *
 * @param[out] out_service 输出服务句柄
 * @param config_path 配置文件路径
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 否
 * @reentrant 是
 */
AGENTOS_API agentos_error_t gateway_service_adapter_create_from_config(
    agentos_service_t* out_service,
    const char* config_path
);

/**
 * @brief 重新加载适配器配置
 *
 * 从配置文件重新加载配置并更新适配器。
 * 只能在服务停止状态下调用。
 *
 * @param service 适配器服务句柄
 * @param config_path 配置文件路径
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API agentos_error_t gateway_service_adapter_reload_config(
    agentos_service_t service,
    const char* config_path
);

/* ==================== 适配器注册 ==================== */

/**
 * @brief 注册网关服务适配器到服务注册表
 *
 * 将网关服务适配器注册到全局服务注册表中，使其可以通过服务发现机制访问。
 *
 * @param service 适配器服务句柄
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t gateway_service_adapter_register(agentos_service_t service);

/**
 * @brief 从服务注册表注销网关服务适配器
 *
 * 从全局服务注册表中注销网关服务适配器。
 *
 * @param service 适配器服务句柄
 * @return AGENTOS_SUCCESS 成功，其他值为错误码
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t gateway_service_adapter_unregister(agentos_service_t service);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_GATEWAY_SVC_ADAPTER_H */