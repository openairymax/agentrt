/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file monit_svc_adapter.c
 * @brief 监控服务适配器：将监控服务适配到统一的AgentOS服务管理框架
 *
 * 使用 agentos_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "monitor_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    void* monit_svc;  // monitor_service_t* 但使用 void* 避免类型问题
    monitor_config_t monit_cfg;
    agentos_svc_config_t common_cfg;
    bool owns_service;
} monit_adapter_ctx_t;

static monit_adapter_ctx_t* monit_get_ctx(agentos_service_t service) {
    if (!service) return NULL;
    return (monit_adapter_ctx_t*)agentos_service_get_user_data(service);
}

static void monit_config_from_common(
    monitor_config_t* monit_cfg,
    const agentos_svc_config_t* common_cfg
) {
    memset(monit_cfg, 0, sizeof(monitor_config_t));
    monit_cfg->metrics_collection_interval_ms = 5000;
    monit_cfg->health_check_interval_ms = 10000;
    monit_cfg->log_flush_interval_ms = 1000;
    monit_cfg->alert_check_interval_ms = 5000;
    monit_cfg->log_file_path = strdup("./logs/monitor.log");
    monit_cfg->metrics_storage_path = strdup("./metrics");
    monit_cfg->enable_tracing = true;
    monit_cfg->enable_alerting = true;
}

static agentos_error_t monit_adapter_init(
    agentos_service_t service,
    const agentos_svc_config_t* config
) {
    if (!service) return AGENTOS_EINVAL;
    monit_adapter_ctx_t* ctx = monit_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    }

    if (!ctx->monit_svc) {
        monit_config_from_common(&ctx->monit_cfg, &ctx->common_cfg);
        int ret = monitor_service_create(&ctx->monit_cfg, &ctx->monit_svc);
        if (ret != 0 || !ctx->monit_svc) {
            SVC_LOG_ERROR("监控服务创建失败: %d", ret);
            return AGENTOS_ERR_UNKNOWN;
        }
        ctx->owns_service = true;
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t monit_adapter_start(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    monit_adapter_ctx_t* ctx = monit_get_ctx(service);
    if (!ctx || !ctx->monit_svc) return AGENTOS_ENOTINIT;
    SVC_LOG_INFO("监控服务适配器已启动");
    return AGENTOS_SUCCESS;
}

static agentos_error_t monit_adapter_stop(agentos_service_t service, bool force) {
    if (!service) return AGENTOS_EINVAL;
    monit_adapter_ctx_t* ctx = monit_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;
    SVC_LOG_INFO("监控服务适配器已停止");
    return AGENTOS_SUCCESS;
}

static void monit_adapter_destroy(agentos_service_t service) {
    if (!service) return;
    monit_adapter_ctx_t* ctx = monit_get_ctx(service);
    if (!ctx) return;

    if (ctx->monit_svc && ctx->owns_service) {
        monitor_service_destroy(ctx->monit_svc);
        ctx->monit_svc = NULL;
    }

    if (ctx->monit_cfg.log_file_path) free((void*)ctx->monit_cfg.log_file_path);
    if (ctx->monit_cfg.metrics_storage_path) free((void*)ctx->monit_cfg.metrics_storage_path);

    agentos_service_set_user_data(service, NULL);
    free(ctx);
}

static agentos_error_t monit_adapter_healthcheck(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    monit_adapter_ctx_t* ctx = monit_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;

    if (!ctx->monit_svc) return AGENTOS_ENOTINIT;

    health_check_result_t* result = NULL;
    int ret = monitor_service_health_check(
        ctx->monit_svc, "monitor_service", &result
    );

    if (ret != 0 || !result) return AGENTOS_ERR_UNKNOWN;

    agentos_error_t err = result->is_healthy ? AGENTOS_SUCCESS : AGENTOS_ERR_UNKNOWN;

    free(result->service_name);
    free(result->status_message);
    free(result);

    return err;
}

static const agentos_svc_interface_t monit_adapter_iface = {
    .init = monit_adapter_init,
    .start = monit_adapter_start,
    .stop = monit_adapter_stop,
    .destroy = monit_adapter_destroy,
    .healthcheck = monit_adapter_healthcheck,
};

agentos_error_t monit_service_adapter_create(
    agentos_service_t* out_service,
    const agentos_svc_config_t* config
) {
    if (!out_service) return AGENTOS_EINVAL;

    monit_adapter_ctx_t* ctx = calloc(1, sizeof(monit_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        ctx->common_cfg.name = "monit_d";
        ctx->common_cfg.version = "1.0.0";
        ctx->common_cfg.enable_metrics = true;
        ctx->common_cfg.enable_tracing = true;
    }

    ctx->owns_service = true;

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle, ctx->common_cfg.name, &monit_adapter_iface, &ctx->common_cfg
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

agentos_error_t monit_service_adapter_wrap(
    agentos_service_t* out_service,
    void* monit_svc,
    const agentos_svc_config_t* config
) {
    if (!out_service || !monit_svc) return AGENTOS_EINVAL;

    monit_adapter_ctx_t* ctx = calloc(1, sizeof(monit_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    ctx->monit_svc = monit_svc;
    ctx->owns_service = false;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        ctx->common_cfg.name = "monit_d";
        ctx->common_cfg.version = "1.0.0";
    }

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle, ctx->common_cfg.name, &monit_adapter_iface, &ctx->common_cfg
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

void* monit_service_adapter_get_original(agentos_service_t service) {
    if (!service) return NULL;
    monit_adapter_ctx_t* ctx = monit_get_ctx(service);
    return ctx ? ctx->monit_svc : NULL;
}

agentos_error_t monit_service_adapter_init(agentos_service_t service) {
    return monit_adapter_init(service, NULL);
}

agentos_error_t monit_service_adapter_start(agentos_service_t service) {
    return monit_adapter_start(service);
}

agentos_error_t monit_service_adapter_stop(agentos_service_t service, bool force) {
    return monit_adapter_stop(service, force);
}

void monit_service_adapter_destroy(agentos_service_t service) {
    monit_adapter_destroy(service);
}

agentos_error_t monit_service_adapter_healthcheck(agentos_service_t service) {
    return monit_adapter_healthcheck(service);
}

const agentos_svc_interface_t* monit_service_adapter_get_interface(void) {
    return &monit_adapter_iface;
}
