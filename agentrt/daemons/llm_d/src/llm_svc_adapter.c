#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file llm_svc_adapter.c
 * @brief LLM服务适配器：将LLM服务适配到统一的AgentRT服务管理框架
 *
 * LLM服务的create接口接受config_path字符串而非配置结构体，
 * 本适配器将通用服务配置转换为LLM服务所需的配置路径格式。
 * 使用 agentrt_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "llm_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    llm_service_t *llm_svc;
    char *config_path;
    agentrt_svc_config_t common_cfg;
    bool owns_service;
    bool running;
} llm_adapter_ctx_t;

static llm_adapter_ctx_t *llm_get_ctx(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    return (llm_adapter_ctx_t *)agentrt_service_get_user_data(service);
}

static agentrt_error_t llm_adapter_init(agentrt_service_t service,
                                        const agentrt_svc_config_t *config)
{
    if (!service)
        return AGENTRT_EINVAL;

    llm_adapter_ctx_t *ctx = llm_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    }

    if (!ctx->llm_svc) {
        const char *path = ctx->config_path ? ctx->config_path : "llm_config.json";
        ctx->llm_svc = llm_service_create(path);
        if (!ctx->llm_svc) {
            SVC_LOG_ERROR("LLM服务创建失败");
            return AGENTRT_ERR_UNKNOWN;
        }
        ctx->owns_service = true;
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t llm_adapter_start(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    llm_adapter_ctx_t *ctx = llm_get_ctx(service);
    if (!ctx || !ctx->llm_svc)
        return AGENTRT_ENOTINIT;
    if (ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = true;
    SVC_LOG_INFO("LLM服务适配器已启动");
    return AGENTRT_SUCCESS;
}

static agentrt_error_t llm_adapter_stop(agentrt_service_t service, bool force)
{
    if (!service)
        return AGENTRT_EINVAL;
    llm_adapter_ctx_t *ctx = llm_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;
    if (!ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = false;
    if (force) {
        if (ctx->llm_svc && ctx->owns_service) {
            llm_service_destroy(ctx->llm_svc);
            ctx->llm_svc = NULL;
            ctx->owns_service = false;
        }
        if (ctx->config_path) {
            AGENTRT_FREE(ctx->config_path);
            ctx->config_path = NULL;
        }
        SVC_LOG_INFO("LLM服务适配器已强制停止");
    } else {
        SVC_LOG_INFO("LLM服务适配器已停止");
    }
    return AGENTRT_SUCCESS;
}

static void llm_adapter_destroy(agentrt_service_t service)
{
    if (!service)
        return;
    llm_adapter_ctx_t *ctx = llm_get_ctx(service);
    if (!ctx)
        return;

    if (ctx->llm_svc && ctx->owns_service) {
        llm_service_destroy(ctx->llm_svc);
        ctx->llm_svc = NULL;
    }

    if (ctx->config_path) {
        AGENTRT_FREE(ctx->config_path);
        ctx->config_path = NULL;
    }

    agentrt_service_set_user_data(service, NULL);
    AGENTRT_FREE(ctx);
}

static agentrt_error_t llm_adapter_healthcheck(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    llm_adapter_ctx_t *ctx = llm_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;
    if (!ctx->llm_svc)
        return AGENTRT_ENOTINIT;
    if (!ctx->running)
        return AGENTRT_ENOTINIT;
    char *stats_json = NULL;
    int ret = llm_service_stats(ctx->llm_svc, &stats_json);
    if (ret != 0) {
        SVC_LOG_WARN("LLM服务健康检查失败: %d", ret);
        return AGENTRT_ERR_UNKNOWN;
    }
    if (stats_json)
        AGENTRT_FREE(stats_json);
    return AGENTRT_SUCCESS;
}

static const agentrt_svc_interface_t llm_adapter_iface = {
    .init = llm_adapter_init,
    .start = llm_adapter_start,
    .stop = llm_adapter_stop,
    .destroy = llm_adapter_destroy,
    .healthcheck = llm_adapter_healthcheck,
};

agentrt_error_t llm_service_adapter_create(agentrt_service_t *out_service,
                                           const agentrt_svc_config_t *config)
{
    if (!out_service)
        return AGENTRT_EINVAL;

    llm_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(llm_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
        if (config->name) {
            size_t path_len = strlen(config->name) + strlen("_config.json") + 1;
            ctx->config_path = AGENTRT_CALLOC(1, path_len);
            if (ctx->config_path) {
                snprintf(ctx->config_path, path_len, "%s_config.json", config->name);
            }
        }
    } else {
        ctx->common_cfg.name = "llm_d";
        ctx->common_cfg.version = "0.1.0";
        ctx->common_cfg.capabilities = AGENTRT_SVC_CAP_ASYNC;
        ctx->common_cfg.enable_metrics = true;
    }

    ctx->owns_service = true;

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &llm_adapter_iface, &ctx->common_cfg);
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

agentrt_error_t llm_service_adapter_wrap(agentrt_service_t *out_service, llm_service_t *llm_svc,
                                         const agentrt_svc_config_t *config)
{
    if (!out_service || !llm_svc)
        return AGENTRT_EINVAL;

    llm_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(llm_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    ctx->llm_svc = llm_svc;
    ctx->owns_service = false;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    } else {
        ctx->common_cfg.name = "llm_d";
        ctx->common_cfg.version = "0.1.0";
    }

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &llm_adapter_iface, &ctx->common_cfg);
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

llm_service_t *llm_service_adapter_get_original(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    llm_adapter_ctx_t *ctx = llm_get_ctx(service);
    return ctx ? ctx->llm_svc : NULL;
}

agentrt_error_t llm_service_adapter_init(agentrt_service_t service)
{
    return llm_adapter_init(service, NULL);
}

agentrt_error_t llm_service_adapter_start(agentrt_service_t service)
{
    return llm_adapter_start(service);
}

agentrt_error_t llm_service_adapter_stop(agentrt_service_t service, bool force)
{
    return llm_adapter_stop(service, force);
}

void llm_service_adapter_destroy(agentrt_service_t service)
{
    llm_adapter_destroy(service);
}

agentrt_error_t llm_service_adapter_healthcheck(agentrt_service_t service)
{
    return llm_adapter_healthcheck(service);
}

const agentrt_svc_interface_t *llm_service_adapter_get_interface(void)
{
    return &llm_adapter_iface;
}
