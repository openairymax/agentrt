#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file sched_svc_adapter.c
 * @brief 调度器服务适配器：将调度器服务适配到统一的AgentRT服务管理框架
 *
 * 使用 agentrt_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "scheduler_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    sched_service_t *sched_svc;
    sched_config_t sched_cfg;
    agentrt_svc_config_t common_cfg;
    bool owns_service;
    bool running;
} sched_adapter_ctx_t;

static sched_adapter_ctx_t *sched_get_ctx(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    return (sched_adapter_ctx_t *)agentrt_service_get_user_data(service);
}

static void sched_config_from_common(sched_config_t *sched_cfg,
                                     const agentrt_svc_config_t *common_cfg)
{
    __builtin_memset(sched_cfg, 0, sizeof(sched_config_t));
    sched_cfg->strategy = SCHED_STRATEGY_WEIGHTED;
    sched_cfg->health_check_interval_ms =
        (common_cfg && common_cfg->timeout_ms > 0) ? common_cfg->timeout_ms : 10000;
    sched_cfg->stats_report_interval_ms = 60000;
    sched_cfg->enable_ml_strategy = (common_cfg && common_cfg->enable_metrics);
    sched_cfg->ml_model_path = NULL;
    sched_cfg->max_agents =
        (common_cfg && common_cfg->max_concurrent > 0) ? common_cfg->max_concurrent : 100;
}

static agentrt_error_t sched_adapter_init(agentrt_service_t service,
                                          const agentrt_svc_config_t *config)
{
    if (!service)
        return AGENTRT_EINVAL;
    sched_adapter_ctx_t *ctx = sched_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    }

    if (!ctx->sched_svc) {
        sched_config_from_common(&ctx->sched_cfg, &ctx->common_cfg);
        int ret = sched_service_create(&ctx->sched_cfg, &ctx->sched_svc);
        if (ret != 0 || !ctx->sched_svc) {
            SVC_LOG_ERROR("调度器服务创建失败: %d", ret);
            return AGENTRT_ERR_UNKNOWN;
        }
        ctx->owns_service = true;
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t sched_adapter_start(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    sched_adapter_ctx_t *ctx = sched_get_ctx(service);
    if (!ctx || !ctx->sched_svc)
        return AGENTRT_ENOTINIT;
    if (ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = true;
    SVC_LOG_INFO("调度器服务适配器已启动");
    return AGENTRT_SUCCESS;
}

static agentrt_error_t sched_adapter_stop(agentrt_service_t service, bool force)
{
    if (!service)
        return AGENTRT_EINVAL;
    sched_adapter_ctx_t *ctx = sched_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;
    if (!ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = false;
    if (force) {
        if (ctx->sched_svc && ctx->owns_service) {
            sched_service_destroy(ctx->sched_svc);
            ctx->sched_svc = NULL;
            ctx->owns_service = false;
        }
        if (ctx->sched_cfg.ml_model_path) {
            AGENTRT_FREE((void *)ctx->sched_cfg.ml_model_path);
            ctx->sched_cfg.ml_model_path = NULL;
        }
        SVC_LOG_INFO("调度器服务适配器已强制停止");
    } else {
        SVC_LOG_INFO("调度器服务适配器已停止");
    }
    return AGENTRT_SUCCESS;
}

static void sched_adapter_destroy(agentrt_service_t service)
{
    if (!service)
        return;
    sched_adapter_ctx_t *ctx = sched_get_ctx(service);
    if (!ctx)
        return;

    if (ctx->sched_svc && ctx->owns_service) {
        sched_service_destroy(ctx->sched_svc);
        ctx->sched_svc = NULL;
    }

    if (ctx->sched_cfg.ml_model_path)
        AGENTRT_FREE((void *)ctx->sched_cfg.ml_model_path);

    agentrt_service_set_user_data(service, NULL);
    AGENTRT_FREE(ctx);
}

static agentrt_error_t sched_adapter_healthcheck(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    sched_adapter_ctx_t *ctx = sched_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;

    if (!ctx->sched_svc)
        return AGENTRT_ENOTINIT;

    bool health_status = false;
    int ret = sched_service_health_check(ctx->sched_svc, &health_status);
    if (ret != 0 || !health_status)
        return AGENTRT_ERR_UNKNOWN;

    return AGENTRT_SUCCESS;
}

static const agentrt_svc_interface_t sched_adapter_iface = {
    .init = sched_adapter_init,
    .start = sched_adapter_start,
    .stop = sched_adapter_stop,
    .destroy = sched_adapter_destroy,
    .healthcheck = sched_adapter_healthcheck,
};

agentrt_error_t sched_service_adapter_create(agentrt_service_t *out_service,
                                             const agentrt_svc_config_t *config)
{
    if (!out_service)
        return AGENTRT_EINVAL;

    sched_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(sched_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    } else {
        ctx->common_cfg.name = "sched_d";
        ctx->common_cfg.version = "0.1.0";
        ctx->common_cfg.enable_metrics = true;
    }

    ctx->owns_service = true;

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &sched_adapter_iface, &ctx->common_cfg);
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

agentrt_error_t sched_service_adapter_wrap(agentrt_service_t *out_service,
                                           sched_service_t *sched_svc,
                                           const agentrt_svc_config_t *config)
{
    if (!out_service || !sched_svc)
        return AGENTRT_EINVAL;

    sched_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(sched_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    ctx->sched_svc = sched_svc;
    ctx->owns_service = false;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    } else {
        ctx->common_cfg.name = "sched_d";
        ctx->common_cfg.version = "0.1.0";
    }

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &sched_adapter_iface, &ctx->common_cfg);
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

sched_service_t *sched_service_adapter_get_original(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    sched_adapter_ctx_t *ctx = sched_get_ctx(service);
    return ctx ? ctx->sched_svc : NULL;
}

agentrt_error_t sched_service_adapter_init(agentrt_service_t service)
{
    return sched_adapter_init(service, NULL);
}

agentrt_error_t sched_service_adapter_start(agentrt_service_t service)
{
    return sched_adapter_start(service);
}

agentrt_error_t sched_service_adapter_stop(agentrt_service_t service, bool force)
{
    return sched_adapter_stop(service, force);
}

void sched_service_adapter_destroy(agentrt_service_t service)
{
    sched_adapter_destroy(service);
}

agentrt_error_t sched_service_adapter_healthcheck(agentrt_service_t service)
{
    return sched_adapter_healthcheck(service);
}

const agentrt_svc_interface_t *sched_service_adapter_get_interface(void)
{
    return &sched_adapter_iface;
}
