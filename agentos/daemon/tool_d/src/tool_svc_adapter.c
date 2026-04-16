/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file tool_svc_adapter.c
 * @brief 工具服务适配器：将工具服务适配到统一的AgentOS服务管理框架
 *
 * 使用 agentos_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "tool_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    tool_service_t tool_svc;
    char* config_path;
    agentos_svc_config_t common_cfg;
    bool owns_service;
} tool_adapter_ctx_t;

static tool_adapter_ctx_t* tool_get_ctx(agentos_service_t service) {
    if (!service) return NULL;
    return (tool_adapter_ctx_t*)agentos_service_get_user_data(service);
}

static agentos_error_t tool_adapter_init(
    agentos_service_t service,
    const agentos_svc_config_t* config
) {
    if (!service) return AGENTOS_EINVAL;
    tool_adapter_ctx_t* ctx = tool_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;

    if (config) {
        memset(&ctx->common_cfg, 0, sizeof(agentos_svc_config_t));
        ctx->common_cfg.name = config->name ? strdup(config->name) : NULL;
        ctx->common_cfg.version = config->version ? strdup(config->version) : NULL;
        ctx->common_cfg.capabilities = config->capabilities;
        ctx->common_cfg.max_concurrent = config->max_concurrent;
        ctx->common_cfg.timeout_ms = config->timeout_ms;
        ctx->common_cfg.priority = config->priority;
        ctx->common_cfg.auto_start = config->auto_start;
        ctx->common_cfg.enable_metrics = config->enable_metrics;
        ctx->common_cfg.enable_tracing = config->enable_tracing;
    }

    if (!ctx->tool_svc) {
        const char* path = ctx->config_path ? ctx->config_path : "tool_config.json";
        ctx->tool_svc = tool_service_create(path);
        if (!ctx->tool_svc) {
            svc_logger_error("工具服务创建失败");
            return AGENTOS_ERROR;
        }
        ctx->owns_service = true;
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t tool_adapter_start(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    tool_adapter_ctx_t* ctx = tool_get_ctx(service);
    if (!ctx || !ctx->tool_svc) return AGENTOS_ENOTINIT;
    svc_logger_info("工具服务适配器已启动");
    return AGENTOS_SUCCESS;
}

static agentos_error_t tool_adapter_stop(agentos_service_t service, bool force) {
    if (!service) return AGENTOS_EINVAL;
    tool_adapter_ctx_t* ctx = tool_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;
    svc_logger_info("工具服务适配器已停止");
    return AGENTOS_SUCCESS;
}

static void tool_adapter_destroy(agentos_service_t service) {
    if (!service) return;
    tool_adapter_ctx_t* ctx = tool_get_ctx(service);
    if (!ctx) return;

    if (ctx->tool_svc && ctx->owns_service) {
        tool_service_destroy(ctx->tool_svc);
        ctx->tool_svc = NULL;
    }

    if (ctx->config_path) {
        free(ctx->config_path);
        ctx->config_path = NULL;
    }

    if (ctx->common_cfg.name && ctx->common_cfg.name[0] != '\0') {
        free((void*)ctx->common_cfg.name);
        ctx->common_cfg.name = NULL;
    }
    if (ctx->common_cfg.version && ctx->common_cfg.version[0] != '\0') {
        free((void*)ctx->common_cfg.version);
        ctx->common_cfg.version = NULL;
    }

    agentos_service_set_user_data(service, NULL);
    free(ctx);
}

static agentos_error_t tool_adapter_healthcheck(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    tool_adapter_ctx_t* ctx = tool_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;
    return ctx->tool_svc ? AGENTOS_SUCCESS : AGENTOS_ENOTINIT;
}

static const agentos_svc_interface_t tool_adapter_iface = {
    .init = tool_adapter_init,
    .start = tool_adapter_start,
    .stop = tool_adapter_stop,
    .destroy = tool_adapter_destroy,
    .healthcheck = tool_adapter_healthcheck,
};

agentos_error_t tool_service_adapter_create(
    agentos_service_t* out_service,
    const agentos_svc_config_t* config
) {
    if (!out_service) return AGENTOS_EINVAL;

    tool_adapter_ctx_t* ctx = calloc(1, sizeof(tool_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    if (config) {
        memset(&ctx->common_cfg, 0, sizeof(agentos_svc_config_t));
        ctx->common_cfg.name = config->name ? strdup(config->name) : NULL;
        ctx->common_cfg.version = config->version ? strdup(config->version) : NULL;
        ctx->common_cfg.capabilities = config->capabilities;
        ctx->common_cfg.max_concurrent = config->max_concurrent;
        ctx->common_cfg.timeout_ms = config->timeout_ms;
        ctx->common_cfg.priority = config->priority;
        ctx->common_cfg.auto_start = config->auto_start;
        ctx->common_cfg.enable_metrics = config->enable_metrics;
        ctx->common_cfg.enable_tracing = config->enable_tracing;
        if (config->name) {
            size_t path_len = strlen(config->name) + strlen("_config.json") + 1;
            ctx->config_path = calloc(1, path_len);
            if (ctx->config_path) {
                snprintf(ctx->config_path, path_len, "%s_config.json", config->name);
            }
        }
    } else {
        ctx->common_cfg.name = "tool_d";
        ctx->common_cfg.version = "1.0.0";
        ctx->common_cfg.capabilities = AGENTOS_SVC_CAP_ASYNC;
        ctx->common_cfg.enable_metrics = true;
    }

    ctx->owns_service = true;

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle, ctx->common_cfg.name, &tool_adapter_iface, &ctx->common_cfg
    );
    if (err != AGENTOS_SUCCESS) {
        free(ctx->config_path);
        free(ctx);
        return err;
    }

    err = agentos_service_set_user_data(svc_handle, ctx);
    if (err != AGENTOS_SUCCESS) {
        agentos_service_destroy(svc_handle);
        free(ctx->config_path);
        free(ctx);
        return err;
    }

    *out_service = svc_handle;
    return AGENTOS_SUCCESS;
}

agentos_error_t tool_service_adapter_wrap(
    agentos_service_t* out_service,
    tool_service_t tool_svc,
    const agentos_svc_config_t* config
) {
    if (!out_service || !tool_svc) return AGENTOS_EINVAL;

    tool_adapter_ctx_t* ctx = calloc(1, sizeof(tool_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    ctx->tool_svc = tool_svc;
    ctx->owns_service = false;

    if (config) {
        memset(&ctx->common_cfg, 0, sizeof(agentos_svc_config_t));
        ctx->common_cfg.name = config->name ? strdup(config->name) : NULL;
        ctx->common_cfg.version = config->version ? strdup(config->version) : NULL;
        ctx->common_cfg.capabilities = config->capabilities;
        ctx->common_cfg.max_concurrent = config->max_concurrent;
        ctx->common_cfg.timeout_ms = config->timeout_ms;
        ctx->common_cfg.priority = config->priority;
        ctx->common_cfg.auto_start = config->auto_start;
        ctx->common_cfg.enable_metrics = config->enable_metrics;
        ctx->common_cfg.enable_tracing = config->enable_tracing;
    } else {
        ctx->common_cfg.name = "tool_d";
        ctx->common_cfg.version = "1.0.0";
    }

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle, ctx->common_cfg.name, &tool_adapter_iface, &ctx->common_cfg
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

tool_service_t tool_service_adapter_get_original(agentos_service_t service) {
    if (!service) return NULL;
    tool_adapter_ctx_t* ctx = tool_get_ctx(service);
    return ctx ? ctx->tool_svc : NULL;
}

agentos_error_t tool_service_adapter_init(agentos_service_t service) {
    return tool_adapter_init(service, NULL);
}

agentos_error_t tool_service_adapter_start(agentos_service_t service) {
    return tool_adapter_start(service);
}

agentos_error_t tool_service_adapter_stop(agentos_service_t service, bool force) {
    return tool_adapter_stop(service, force);
}

void tool_service_adapter_destroy(agentos_service_t service) {
    tool_adapter_destroy(service);
}

agentos_error_t tool_service_adapter_healthcheck(agentos_service_t service) {
    return tool_adapter_healthcheck(service);
}

const agentos_svc_interface_t* tool_service_adapter_get_interface(void) {
    return &tool_adapter_iface;
}
