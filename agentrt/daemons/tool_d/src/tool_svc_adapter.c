#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file tool_svc_adapter.c
 * @brief 工具服务适配器：将工具服务适配到统一的AgentRT服务管理框架
 *
 * 使用 agentrt_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "svc_common.h"
#include "svc_logger.h"
#include "tool_service.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    tool_service_t *tool_svc;
    char *config_path;
    agentrt_svc_config_t common_cfg;
    bool owns_service;
    bool running;
} tool_adapter_ctx_t;

static tool_adapter_ctx_t *tool_get_ctx(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    return (tool_adapter_ctx_t *)agentrt_service_get_user_data(service);
}

static agentrt_error_t tool_adapter_init(agentrt_service_t service,
                                         const agentrt_svc_config_t *config)
{
    if (!service)
        return AGENTRT_EINVAL;
    tool_adapter_ctx_t *ctx = tool_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;

    if (config) {
        __builtin_memset(&ctx->common_cfg, 0, sizeof(agentrt_svc_config_t));
        ctx->common_cfg.name = config->name ? AGENTRT_STRDUP(config->name) : NULL;
        ctx->common_cfg.version = config->version ? AGENTRT_STRDUP(config->version) : NULL;
        ctx->common_cfg.capabilities = config->capabilities;
        ctx->common_cfg.max_concurrent = config->max_concurrent;
        ctx->common_cfg.timeout_ms = config->timeout_ms;
        ctx->common_cfg.priority = config->priority;
        ctx->common_cfg.auto_start = config->auto_start;
        ctx->common_cfg.enable_metrics = config->enable_metrics;
        ctx->common_cfg.enable_tracing = config->enable_tracing;
    }

    if (!ctx->tool_svc) {
        const char *path = ctx->config_path ? ctx->config_path : "tool_config.json";
        ctx->tool_svc = tool_service_create(path);
        if (!ctx->tool_svc) {
            SVC_LOG_ERROR("工具服务创建失败");
            return AGENTRT_ERR_UNKNOWN;
        }
        ctx->owns_service = true;
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t tool_adapter_start(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    tool_adapter_ctx_t *ctx = tool_get_ctx(service);
    if (!ctx || !ctx->tool_svc)
        return AGENTRT_ENOTINIT;
    if (ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = true;
    SVC_LOG_INFO("工具服务适配器已启动");
    return AGENTRT_SUCCESS;
}

static agentrt_error_t tool_adapter_stop(agentrt_service_t service, bool force)
{
    if (!service)
        return AGENTRT_EINVAL;
    tool_adapter_ctx_t *ctx = tool_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;
    if (!ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = false;
    if (force) {
        if (ctx->tool_svc && ctx->owns_service) {
            tool_service_destroy(ctx->tool_svc);
            ctx->tool_svc = NULL;
            ctx->owns_service = false;
        }
        if (ctx->config_path) {
            AGENTRT_FREE(ctx->config_path);
            ctx->config_path = NULL;
        }
        SVC_LOG_INFO("工具服务适配器已强制停止");
    } else {
        SVC_LOG_INFO("工具服务适配器已停止");
    }
    return AGENTRT_SUCCESS;
}

static void tool_adapter_destroy(agentrt_service_t service)
{
    if (!service)
        return;
    tool_adapter_ctx_t *ctx = tool_get_ctx(service);
    if (!ctx)
        return;

    if (ctx->tool_svc && ctx->owns_service) {
        tool_service_destroy(ctx->tool_svc);
        ctx->tool_svc = NULL;
    }

    if (ctx->config_path) {
        AGENTRT_FREE(ctx->config_path);
        ctx->config_path = NULL;
    }

    if (ctx->common_cfg.name && ctx->common_cfg.name[0] != '\0') {
        AGENTRT_FREE((void *)ctx->common_cfg.name);
        ctx->common_cfg.name = NULL;
    }
    if (ctx->common_cfg.version && ctx->common_cfg.version[0] != '\0') {
        AGENTRT_FREE((void *)ctx->common_cfg.version);
        ctx->common_cfg.version = NULL;
    }

    agentrt_service_set_user_data(service, NULL);
    AGENTRT_FREE(ctx);
}

static agentrt_error_t tool_adapter_healthcheck(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    tool_adapter_ctx_t *ctx = tool_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;
    if (!ctx->tool_svc)
        return AGENTRT_ENOTINIT;
    if (!ctx->running)
        return AGENTRT_ENOTINIT;
    char *list_json = tool_service_list(ctx->tool_svc);
    if (!list_json) {
        SVC_LOG_WARN("工具服务健康检查失败: 无法获取工具列表");
        return AGENTRT_ERR_UNKNOWN;
    }
    AGENTRT_FREE(list_json);
    return AGENTRT_SUCCESS;
}

static const agentrt_svc_interface_t tool_adapter_iface = {
    .init = tool_adapter_init,
    .start = tool_adapter_start,
    .stop = tool_adapter_stop,
    .destroy = tool_adapter_destroy,
    .healthcheck = tool_adapter_healthcheck,
};

agentrt_error_t tool_service_adapter_create(agentrt_service_t *out_service,
                                            const agentrt_svc_config_t *config)
{
    if (!out_service)
        return AGENTRT_EINVAL;

    tool_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(tool_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    if (config) {
        __builtin_memset(&ctx->common_cfg, 0, sizeof(agentrt_svc_config_t));
        ctx->common_cfg.name = config->name ? AGENTRT_STRDUP(config->name) : NULL;
        ctx->common_cfg.version = config->version ? AGENTRT_STRDUP(config->version) : NULL;
        ctx->common_cfg.capabilities = config->capabilities;
        ctx->common_cfg.max_concurrent = config->max_concurrent;
        ctx->common_cfg.timeout_ms = config->timeout_ms;
        ctx->common_cfg.priority = config->priority;
        ctx->common_cfg.auto_start = config->auto_start;
        ctx->common_cfg.enable_metrics = config->enable_metrics;
        ctx->common_cfg.enable_tracing = config->enable_tracing;
        if (config->name) {
            size_t path_len = strlen(config->name) + strlen("_config.json") + 1;
            ctx->config_path = AGENTRT_CALLOC(1, path_len);
            if (ctx->config_path) {
                snprintf(ctx->config_path, path_len, "%s_config.json", config->name);
            }
        }
    } else {
        ctx->common_cfg.name = "tool_d";
        ctx->common_cfg.version = "0.1.0";
        ctx->common_cfg.capabilities = AGENTRT_SVC_CAP_ASYNC;
        ctx->common_cfg.enable_metrics = true;
    }

    ctx->owns_service = true;

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &tool_adapter_iface, &ctx->common_cfg);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_FREE(ctx->config_path);
        AGENTRT_FREE(ctx);
        return err;
    }

    err = agentrt_service_set_user_data(svc_handle, ctx);
    if (err != AGENTRT_SUCCESS) {
        agentrt_service_destroy(svc_handle);
        AGENTRT_FREE(ctx->config_path);
        AGENTRT_FREE(ctx);
        return err;
    }

    *out_service = svc_handle;
    return AGENTRT_SUCCESS;
}

agentrt_error_t tool_service_adapter_wrap(agentrt_service_t *out_service, tool_service_t *tool_svc,
                                          const agentrt_svc_config_t *config)
{
    if (!out_service || !tool_svc)
        return AGENTRT_EINVAL;

    tool_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(tool_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    ctx->tool_svc = tool_svc;
    ctx->owns_service = false;

    if (config) {
        __builtin_memset(&ctx->common_cfg, 0, sizeof(agentrt_svc_config_t));
        ctx->common_cfg.name = config->name ? AGENTRT_STRDUP(config->name) : NULL;
        ctx->common_cfg.version = config->version ? AGENTRT_STRDUP(config->version) : NULL;
        ctx->common_cfg.capabilities = config->capabilities;
        ctx->common_cfg.max_concurrent = config->max_concurrent;
        ctx->common_cfg.timeout_ms = config->timeout_ms;
        ctx->common_cfg.priority = config->priority;
        ctx->common_cfg.auto_start = config->auto_start;
        ctx->common_cfg.enable_metrics = config->enable_metrics;
        ctx->common_cfg.enable_tracing = config->enable_tracing;
    } else {
        ctx->common_cfg.name = "tool_d";
        ctx->common_cfg.version = "0.1.0";
    }

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &tool_adapter_iface, &ctx->common_cfg);
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

tool_service_t *tool_service_adapter_get_original(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    tool_adapter_ctx_t *ctx = tool_get_ctx(service);
    return ctx ? ctx->tool_svc : NULL;
}

agentrt_error_t tool_service_adapter_init(agentrt_service_t service)
{
    return tool_adapter_init(service, NULL);
}

agentrt_error_t tool_service_adapter_start(agentrt_service_t service)
{
    return tool_adapter_start(service);
}

agentrt_error_t tool_service_adapter_stop(agentrt_service_t service, bool force)
{
    return tool_adapter_stop(service, force);
}

void tool_service_adapter_destroy(agentrt_service_t service)
{
    tool_adapter_destroy(service);
}

agentrt_error_t tool_service_adapter_healthcheck(agentrt_service_t service)
{
    return tool_adapter_healthcheck(service);
}

const agentrt_svc_interface_t *tool_service_adapter_get_interface(void)
{
    return &tool_adapter_iface;
}
