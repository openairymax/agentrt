#include "memory_compat.h"
#include "error.h"
/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file market_svc_adapter.c
 * @brief 市场服务适配器：将市场服务适配到统一的AgentRT服务管理框架
 *
 * 使用 agentrt_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "market_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    market_service_t *market_svc;
    market_config_t market_cfg;
    agentrt_svc_config_t common_cfg;
    bool owns_service;
    bool running;
} market_adapter_ctx_t;

static market_adapter_ctx_t *market_get_ctx(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    return (market_adapter_ctx_t *)agentrt_service_get_user_data(service);
}

static void market_config_from_common(market_config_t *market_cfg,
                                      const agentrt_svc_config_t *common_cfg)
{
    __builtin_memset(market_cfg, 0, sizeof(market_config_t));
    market_cfg->sync_interval_ms =
        (common_cfg && common_cfg->timeout_ms > 0) ? common_cfg->timeout_ms : 30000;
    market_cfg->cache_ttl_ms = 300000;
    market_cfg->enable_remote_registry = true;
    market_cfg->enable_auto_update = false;
    market_cfg->registry_url = AGENTRT_STRDUP("https://registry.agentos.io");
    market_cfg->storage_path = AGENTRT_STRDUP("./market_data");
}

static agentrt_error_t market_adapter_init(agentrt_service_t service,
                                           const agentrt_svc_config_t *config)
{
    if (!service)
        return AGENTRT_EINVAL;
    market_adapter_ctx_t *ctx = market_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    }

    if (!ctx->market_svc) {
        market_config_from_common(&ctx->market_cfg, &ctx->common_cfg);
        int ret = market_service_create(&ctx->market_cfg, &ctx->market_svc);
        if (ret != 0 || !ctx->market_svc) {
            SVC_LOG_ERROR("市场服务创建失败: %d", ret);
            return AGENTRT_ERR_UNKNOWN;
        }
        ctx->owns_service = true;
    }

    return AGENTRT_SUCCESS;
}

static agentrt_error_t market_adapter_start(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    market_adapter_ctx_t *ctx = market_get_ctx(service);
    if (!ctx || !ctx->market_svc)
        return AGENTRT_ENOTINIT;
    if (ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = true;
    SVC_LOG_INFO("市场服务适配器已启动");
    return AGENTRT_SUCCESS;
}

static agentrt_error_t market_adapter_stop(agentrt_service_t service, bool force)
{
    if (!service)
        return AGENTRT_EINVAL;
    market_adapter_ctx_t *ctx = market_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;
    if (!ctx->running)
        return AGENTRT_SUCCESS;
    ctx->running = false;
    if (force) {
        if (ctx->market_svc && ctx->owns_service) {
            market_service_destroy(ctx->market_svc);
            ctx->market_svc = NULL;
            ctx->owns_service = false;
        }
        if (ctx->market_cfg.registry_url) {
            AGENTRT_FREE((void *)ctx->market_cfg.registry_url);
            ctx->market_cfg.registry_url = NULL;
        }
        if (ctx->market_cfg.storage_path) {
            AGENTRT_FREE((void *)ctx->market_cfg.storage_path);
            ctx->market_cfg.storage_path = NULL;
        }
        SVC_LOG_INFO("市场服务适配器已强制停止");
    } else {
        SVC_LOG_INFO("市场服务适配器已停止");
    }
    return AGENTRT_SUCCESS;
}

static void market_adapter_destroy(agentrt_service_t service)
{
    if (!service)
        return;
    market_adapter_ctx_t *ctx = market_get_ctx(service);
    if (!ctx)
        return;

    if (ctx->market_svc && ctx->owns_service) {
        market_service_destroy(ctx->market_svc);
        ctx->market_svc = NULL;
    }

    if (ctx->market_cfg.registry_url)
        AGENTRT_FREE((void *)ctx->market_cfg.registry_url);
    if (ctx->market_cfg.storage_path)
        AGENTRT_FREE((void *)ctx->market_cfg.storage_path);

    agentrt_service_set_user_data(service, NULL);
    AGENTRT_FREE(ctx);
}

static agentrt_error_t market_adapter_healthcheck(agentrt_service_t service)
{
    if (!service)
        return AGENTRT_EINVAL;
    market_adapter_ctx_t *ctx = market_get_ctx(service);
    if (!ctx)
        return AGENTRT_EINVAL;
    if (!ctx->market_svc)
        return AGENTRT_ENOTINIT;
    if (!ctx->running)
        return AGENTRT_ENOTINIT;
    agent_info_t **agents = NULL;
    size_t count = 0;
    search_params_t params = {0};
    int ret = market_service_search_agents(ctx->market_svc, &params, &agents, &count);
    if (ret != 0) {
        SVC_LOG_WARN("市场服务健康检查失败: %d", ret);
        return AGENTRT_ERR_UNKNOWN;
    }
    if (agents)
        AGENTRT_FREE(agents);
    return AGENTRT_SUCCESS;
}

static const agentrt_svc_interface_t market_adapter_iface = {
    .init = market_adapter_init,
    .start = market_adapter_start,
    .stop = market_adapter_stop,
    .destroy = market_adapter_destroy,
    .healthcheck = market_adapter_healthcheck,
};

agentrt_error_t market_service_adapter_create(agentrt_service_t *out_service,
                                              const agentrt_svc_config_t *config)
{
    if (!out_service)
        return AGENTRT_EINVAL;

    market_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(market_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    } else {
        ctx->common_cfg.name = "market_d";
        ctx->common_cfg.version = "0.1.0";
        ctx->common_cfg.enable_metrics = true;
    }

    ctx->owns_service = true;

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &market_adapter_iface, &ctx->common_cfg);
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

agentrt_error_t market_service_adapter_wrap(agentrt_service_t *out_service,
                                            market_service_t *market_svc,
                                            const agentrt_svc_config_t *config)
{
    if (!out_service || !market_svc)
        return AGENTRT_EINVAL;

    market_adapter_ctx_t *ctx = AGENTRT_CALLOC(1, sizeof(market_adapter_ctx_t));
    if (!ctx)
        return AGENTRT_ENOMEM;

    ctx->market_svc = market_svc;
    ctx->owns_service = false;

    if (config) {
        __builtin_memcpy(&ctx->common_cfg, config, sizeof(agentrt_svc_config_t));
    } else {
        ctx->common_cfg.name = "market_d";
        ctx->common_cfg.version = "0.1.0";
    }

    agentrt_service_t svc_handle = NULL;
    agentrt_error_t err = agentrt_service_create(&svc_handle, ctx->common_cfg.name,
                                                 &market_adapter_iface, &ctx->common_cfg);
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

market_service_t *market_service_adapter_get_original(agentrt_service_t service)
{
    if (!service) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    market_adapter_ctx_t *ctx = market_get_ctx(service);
    return ctx ? ctx->market_svc : NULL;
}

agentrt_error_t market_service_adapter_init(agentrt_service_t service)
{
    return market_adapter_init(service, NULL);
}

agentrt_error_t market_service_adapter_start(agentrt_service_t service)
{
    return market_adapter_start(service);
}

agentrt_error_t market_service_adapter_stop(agentrt_service_t service, bool force)
{
    return market_adapter_stop(service, force);
}

void market_service_adapter_destroy(agentrt_service_t service)
{
    market_adapter_destroy(service);
}

agentrt_error_t market_service_adapter_healthcheck(agentrt_service_t service)
{
    return market_adapter_healthcheck(service);
}

const agentrt_svc_interface_t *market_service_adapter_get_interface(void)
{
    return &market_adapter_iface;
}
