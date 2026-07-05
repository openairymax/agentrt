#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file monit_svc_adapter.c
 * @brief 监控服务适配器：将监控服务适配到统一的AgentRT服务管理框架
 *
 * 使用 agentrt_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "monitor_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    void *monit_svc;
    monitor_config_t monit_cfg;
    agentrt_svc_config_t common_cfg;
    bool owns_service;
    bool running;
} monit_adapter_ctx_t;

static monit_adapter_ctx_t *monit_get_ctx(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    return (monit_adapter_ctx_t *)agentrt_service_get_user_data(service);
}

static void monit_config_from_common(monitor_config_t *monit_cfg,
                                     const agentrt_svc_config_t *common_cfg)
{
    __builtin_memset(monit_cfg, 0, sizeof(monitor_config_t));
    monit_cfg->metrics_collection_interval_ms = 5000;
    monit_cfg->health_check_interval_ms =
        (common_cfg && common_cfg->timeout_ms > 0) ? common_cfg->timeout_ms : 10000;
    monit_cfg->log_flush_interval_ms = 1000;
    monit_cfg->alert_check_interval_ms = 5000;
    monit_cfg->log_file_path = AGENTRT_STRDUP("./logs/monitor.log");
    monit_cfg->metrics_storage_path = AGENTRT_STRDUP("./metrics");
    monit_cfg->enable_tracing = (common_cfg && common_cfg->enable_tracing);
    monit_cfg->enable_alerting = true;
}

static agentrt_error_t monit_adapter_init(agentrt_service_t service,
                                          const agentrt_svc_config_t *config)
{
    if (!service)
        return AGENTRT_EINVAL;
    monit_adapter_ctx_t *ctx = monit_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    }

    if (!ctx->monit_svc) {
        monit_config_from_common(&ctx->monit_cfg, &ctx->common_cfg);
        int ret = monitor_service_create(&ctx->monit_cfg, (monitor_service_t **)&ctx->monit_svc);
        if (ret != 0 || !ctx->monit_svc) {
            SVC_LOG_ERROR("监控服务创建失败: %d", ret);
            return AGENTRT_ERR_UNKNOWN;
        }
        ctx->owns_service = true;
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t monit_adapter_start(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    monit_adapter_ctx_t *ctx = monit_get_ctx(service);
    if (!ctx || !ctx->monit_svc)
        return AGENTRT_ENOTINIT;
    if (ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = true;
    SVC_LOG_INFO("监控服务适配器已启动");
    return AGENTRT_SUCCESS;
}

static agentrt_error_t monit_adapter_stop(agentrt_service_t service, bool force)
{
    if (!service)
        return AGENTRT_EINVAL;
    monit_adapter_ctx_t *ctx = monit_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;
    if (!ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = false;
    if (force) {
        if (ctx->monit_svc && ctx->owns_service) {
            monitor_service_destroy(ctx->monit_svc);
            ctx->monit_svc = NULL;
            ctx->owns_service = false;
        }
        if (ctx->monit_cfg.log_file_path) {
            AGENTRT_FREE((void *)ctx->monit_cfg.log_file_path);
            ctx->monit_cfg.log_file_path = NULL;
        }
        if (ctx->monit_cfg.metrics_storage_path) {
            AGENTRT_FREE((void *)ctx->monit_cfg.metrics_storage_path);
            ctx->monit_cfg.metrics_storage_path = NULL;
        }
        SVC_LOG_INFO("监控服务适配器已强制停止");
    } else {
        SVC_LOG_INFO("监控服务适配器已停止");
    }
    return AGENTRT_SUCCESS;
}

static void monit_adapter_destroy(agentrt_service_t service)
{
    if (!service)
        return;
    monit_adapter_ctx_t *ctx = monit_get_ctx(service);
    if (!ctx)
        return;

    if (ctx->monit_svc && ctx->owns_service) {
        monitor_service_destroy(ctx->monit_svc);
        ctx->monit_svc = NULL;
    }

    if (ctx->monit_cfg.log_file_path)
        AGENTRT_FREE((void *)ctx->monit_cfg.log_file_path);
    if (ctx->monit_cfg.metrics_storage_path)
        AGENTRT_FREE((void *)ctx->monit_cfg.metrics_storage_path);

    agentrt_service_set_user_data(service, NULL);
    AGENTRT_FREE(ctx);
}

static agentrt_error_t monit_adapter_healthcheck(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    monit_adapter_ctx_t *ctx = monit_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;

    if (!ctx->monit_svc)
        return AGENTRT_ENOTINIT;

    health_check_result_t *result = NULL;
    int ret = monitor_service_health_check(ctx->monit_svc, "monitor_service", &result);

    if (ret != 0 || !result)
        return AGENTRT_ERR_UNKNOWN;

    agentrt_error_t err = result->is_healthy ? AGENTRT_SUCCESS : AGENTRT_ERR_UNKNOWN;

    AGENTRT_FREE(result->service_name);
    AGENTRT_FREE(result->status_message);
    AGENTRT_FREE(result);

    return err;
}

static const agentrt_svc_interface_t monit_adapter_iface = {
    .init = monit_adapter_init,
    .start = monit_adapter_start,
    .stop = monit_adapter_stop,
    .destroy = monit_adapter_destroy,
    .healthcheck = monit_adapter_healthcheck,
};

agentrt_error_t monit_service_adapter_create(agentrt_service_t *out_service,
                                             const agentrt_svc_config_t *config)
{
    if (!out_service)
        return AGENTRT_EINVAL;

    monit_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(monit_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    } else {
        ctx->common_cfg.name = "monit_d";
        ctx->common_cfg.version = "0.1.0";
        ctx->common_cfg.enable_metrics = true;
        ctx->common_cfg.enable_tracing = true;
    }

    ctx->owns_service = true;

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &monit_adapter_iface, &ctx->common_cfg);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_FREE(ctx);
        return err;
    }

    err = agentrt_service_set_user_data(svc_handle, ctx);
    if (err != AGENTRT_SUCCESS) {
        agentrt_service_destroy(svc_handle);
        AGENTRT_FREE(ctx);
        return err;
    }

    *out_service = svc_handle;
    return AGENTRT_SUCCESS;
}

agentrt_error_t monit_service_adapter_wrap(agentrt_service_t *out_service, void *monit_svc,
                                           const agentrt_svc_config_t *config)
{
    if (!out_service || !monit_svc)
        return AGENTRT_EINVAL;

    monit_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(monit_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    ctx->monit_svc = monit_svc;
    ctx->owns_service = false;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    } else {
        ctx->common_cfg.name = "monit_d";
        ctx->common_cfg.version = "0.1.0";
    }

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &monit_adapter_iface, &ctx->common_cfg);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_FREE(ctx);
        return err;
    }

    err = agentrt_service_set_user_data(svc_handle, ctx);
    if (err != AGENTRT_SUCCESS) {
        agentrt_service_destroy(svc_handle);
        AGENTRT_FREE(ctx);
        return err;
    }

    *out_service = svc_handle;
    return AGENTRT_SUCCESS;
}

void *monit_service_adapter_get_original(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    monit_adapter_ctx_t *ctx = monit_get_ctx(service);
    return ctx ? ctx->monit_svc : NULL;
}

agentrt_error_t monit_service_adapter_init(agentrt_service_t service)
{
    return monit_adapter_init(service, NULL);
}

agentrt_error_t monit_service_adapter_start(agentrt_service_t service)
{
    return monit_adapter_start(service);
}

agentrt_error_t monit_service_adapter_stop(agentrt_service_t service, bool force)
{
    return monit_adapter_stop(service, force);
}

void monit_service_adapter_destroy(agentrt_service_t service)
{
    monit_adapter_destroy(service);
}

agentrt_error_t monit_service_adapter_healthcheck(agentrt_service_t service)
{
    return monit_adapter_healthcheck(service);
}

const agentrt_svc_interface_t *monit_service_adapter_get_interface(void)
{
    return &monit_adapter_iface;
}
