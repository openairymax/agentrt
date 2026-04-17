/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file sched_svc_adapter.c
 * @brief 调度器服务适配器：将调度器服务适配到统一的AgentOS服务管理框架
 *
 * 使用 agentos_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "scheduler_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    sched_service_t* sched_svc;
    sched_config_t sched_cfg;
    agentos_svc_config_t common_cfg;
    bool owns_service;
} sched_adapter_ctx_t;

static sched_adapter_ctx_t* sched_get_ctx(agentos_service_t service) {
    if (!service) return NULL;
    return (sched_adapter_ctx_t*)agentos_service_get_user_data(service);
}

static void sched_config_from_common(
    sched_config_t* sched_cfg,
    const agentos_svc_config_t* common_cfg
) {
    memset(sched_cfg, 0, sizeof(sched_config_t));
    sched_cfg->strategy = SCHED_STRATEGY_WEIGHTED;
    sched_cfg->health_check_interval_ms = 10000;
    sched_cfg->stats_report_interval_ms = 60000;
    sched_cfg->enable_ml_strategy = false;
    sched_cfg->ml_model_path = NULL;
    sched_cfg->max_agents = 100;
}

static agentos_error_t sched_adapter_init(
    agentos_service_t service,
    const agentos_svc_config_t* config
) {
    if (!service) return AGENTOS_EINVAL;
    sched_adapter_ctx_t* ctx = sched_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    }

    if (!ctx->sched_svc) {
        sched_config_from_common(&ctx->sched_cfg, &ctx->common_cfg);
        int ret = sched_service_create(&ctx->sched_cfg, &ctx->sched_svc);
        if (ret != 0 || !ctx->sched_svc) {
            SVC_LOG_ERROR("调度器服务创建失败: %d", ret);
            return AGENTOS_ERR_UNKNOWN;
        }
        ctx->owns_service = true;
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t sched_adapter_start(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    sched_adapter_ctx_t* ctx = sched_get_ctx(service);
    if (!ctx || !ctx->sched_svc) return AGENTOS_ENOTINIT;
    SVC_LOG_INFO("调度器服务适配器已启动");
    return AGENTOS_SUCCESS;
}

static agentos_error_t sched_adapter_stop(agentos_service_t service, bool force) {
    if (!service) return AGENTOS_EINVAL;
    sched_adapter_ctx_t* ctx = sched_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;
    SVC_LOG_INFO("调度器服务适配器已停止");
    return AGENTOS_SUCCESS;
}

static void sched_adapter_destroy(agentos_service_t service) {
    if (!service) return;
    sched_adapter_ctx_t* ctx = sched_get_ctx(service);
    if (!ctx) return;

    if (ctx->sched_svc && ctx->owns_service) {
        sched_service_destroy(ctx->sched_svc);
        ctx->sched_svc = NULL;
    }

    if (ctx->sched_cfg.ml_model_path) free((void*)ctx->sched_cfg.ml_model_path);

    agentos_service_set_user_data(service, NULL);
    free(ctx);
}

static agentos_error_t sched_adapter_healthcheck(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    sched_adapter_ctx_t* ctx = sched_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;

    if (!ctx->sched_svc) return AGENTOS_ENOTINIT;

    bool health_status = false;
    int ret = sched_service_health_check(ctx->sched_svc, &health_status);
    if (ret != 0 || !health_status) return AGENTOS_ERR_UNKNOWN;

    return AGENTOS_SUCCESS;
}

static const agentos_svc_interface_t sched_adapter_iface = {
    .init = sched_adapter_init,
    .start = sched_adapter_start,
    .stop = sched_adapter_stop,
    .destroy = sched_adapter_destroy,
    .healthcheck = sched_adapter_healthcheck,
};

agentos_error_t sched_service_adapter_create(
    agentos_service_t* out_service,
    const agentos_svc_config_t* config
) {
    if (!out_service) return AGENTOS_EINVAL;

    sched_adapter_ctx_t* ctx = calloc(1, sizeof(sched_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        ctx->common_cfg.name = "sched_d";
        ctx->common_cfg.version = "1.0.0";
        ctx->common_cfg.enable_metrics = true;
    }

    ctx->owns_service = true;

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle, ctx->common_cfg.name, &sched_adapter_iface, &ctx->common_cfg
    );
    if (err != AGENTOS_SUCCESS) {
        free(ctx);
        return err;
    }

    err = agentos_service_set_user_data(svc_handle, ctx);
    if (err != AGENTOS_SUCCESS) {
        agentos_service_destroy(svc_handle);
        free(ctx);
        return err;
    }

    *out_service = svc_handle;
    return AGENTOS_SUCCESS;
}

agentos_error_t sched_service_adapter_wrap(
    agentos_service_t* out_service,
    sched_service_t* sched_svc,
    const agentos_svc_config_t* config
) {
    if (!out_service || !sched_svc) return AGENTOS_EINVAL;

    sched_adapter_ctx_t* ctx = calloc(1, sizeof(sched_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    ctx->sched_svc = sched_svc;
    ctx->owns_service = false;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        ctx->common_cfg.name = "sched_d";
        ctx->common_cfg.version = "1.0.0";
    }

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle, ctx->common_cfg.name, &sched_adapter_iface, &ctx->common_cfg
    );
    if (err != AGENTOS_SUCCESS) {
        free(ctx);
        return err;
    }

    err = agentos_service_set_user_data(svc_handle, ctx);
    if (err != AGENTOS_SUCCESS) {
        agentos_service_destroy(svc_handle);
        free(ctx);
        return err;
    }

    *out_service = svc_handle;
    return AGENTOS_SUCCESS;
}

sched_service_t* sched_service_adapter_get_original(agentos_service_t service) {
    if (!service) return NULL;
    sched_adapter_ctx_t* ctx = sched_get_ctx(service);
    return ctx ? ctx->sched_svc : NULL;
}

agentos_error_t sched_service_adapter_init(agentos_service_t service) {
    return sched_adapter_init(service, NULL);
}

agentos_error_t sched_service_adapter_start(agentos_service_t service) {
    return sched_adapter_start(service);
}

agentos_error_t sched_service_adapter_stop(agentos_service_t service, bool force) {
    return sched_adapter_stop(service, force);
}

void sched_service_adapter_destroy(agentos_service_t service) {
    sched_adapter_destroy(service);
}

agentos_error_t sched_service_adapter_healthcheck(agentos_service_t service) {
    return sched_adapter_healthcheck(service);
}

const agentos_svc_interface_t* sched_service_adapter_get_interface(void) {
    return &sched_adapter_iface;
}
