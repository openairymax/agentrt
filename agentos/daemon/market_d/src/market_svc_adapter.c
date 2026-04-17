/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file market_svc_adapter.c
 * @brief 市场服务适配器：将市场服务适配到统一的AgentOS服务管理框架
 *
 * 使用 agentos_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "market_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    market_service_t* market_svc;
    market_config_t market_cfg;
    agentos_svc_config_t common_cfg;
    bool owns_service;
} market_adapter_ctx_t;

static market_adapter_ctx_t* market_get_ctx(agentos_service_t service) {
    if (!service) return NULL;
    return (market_adapter_ctx_t*)agentos_service_get_user_data(service);
}

static void market_config_from_common(
    market_config_t* market_cfg,
    const agentos_svc_config_t* common_cfg
) {
    memset(market_cfg, 0, sizeof(market_config_t));
    market_cfg->sync_interval_ms = 30000;
    market_cfg->cache_ttl_ms = 300000;
    market_cfg->enable_remote_registry = true;
    market_cfg->enable_auto_update = false;
    market_cfg->registry_url = strdup("https://registry.agentos.io");
    market_cfg->storage_path = strdup("./market_data");
}

static agentos_error_t market_adapter_init(
    agentos_service_t service,
    const agentos_svc_config_t* config
) {
    if (!service) return AGENTOS_EINVAL;
    market_adapter_ctx_t* ctx = market_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    }

    if (!ctx->market_svc) {
        market_config_from_common(&ctx->market_cfg, &ctx->common_cfg);
        int ret = market_service_create(&ctx->market_cfg, &ctx->market_svc);
        if (ret != 0 || !ctx->market_svc) {
            SVC_LOG_ERROR("市场服务创建失败: %d", ret);
            return AGENTOS_ERR_UNKNOWN;
        }
        ctx->owns_service = true;
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t market_adapter_start(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    market_adapter_ctx_t* ctx = market_get_ctx(service);
    if (!ctx || !ctx->market_svc) return AGENTOS_ENOTINIT;
    SVC_LOG_INFO("市场服务适配器已启动");
    return AGENTOS_SUCCESS;
}

static agentos_error_t market_adapter_stop(agentos_service_t service, bool force) {
    if (!service) return AGENTOS_EINVAL;
    market_adapter_ctx_t* ctx = market_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;
    SVC_LOG_INFO("市场服务适配器已停止");
    return AGENTOS_SUCCESS;
}

static void market_adapter_destroy(agentos_service_t service) {
    if (!service) return;
    market_adapter_ctx_t* ctx = market_get_ctx(service);
    if (!ctx) return;

    if (ctx->market_svc && ctx->owns_service) {
        market_service_destroy(ctx->market_svc);
        ctx->market_svc = NULL;
    }

    if (ctx->market_cfg.registry_url) free((void*)ctx->market_cfg.registry_url);
    if (ctx->market_cfg.storage_path) free((void*)ctx->market_cfg.storage_path);

    agentos_service_set_user_data(service, NULL);
    free(ctx);
}

static agentos_error_t market_adapter_healthcheck(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    market_adapter_ctx_t* ctx = market_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;
    if (!ctx->market_svc) return AGENTOS_ENOTINIT;
    return AGENTOS_SUCCESS;
}

static const agentos_svc_interface_t market_adapter_iface = {
    .init = market_adapter_init,
    .start = market_adapter_start,
    .stop = market_adapter_stop,
    .destroy = market_adapter_destroy,
    .healthcheck = market_adapter_healthcheck,
};

agentos_error_t market_service_adapter_create(
    agentos_service_t* out_service,
    const agentos_svc_config_t* config
) {
    if (!out_service) return AGENTOS_EINVAL;

    market_adapter_ctx_t* ctx = calloc(1, sizeof(market_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        ctx->common_cfg.name = "market_d";
        ctx->common_cfg.version = "1.0.0";
        ctx->common_cfg.enable_metrics = true;
    }

    ctx->owns_service = true;

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle, ctx->common_cfg.name, &market_adapter_iface, &ctx->common_cfg
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

agentos_error_t market_service_adapter_wrap(
    agentos_service_t* out_service,
    market_service_t* market_svc,
    const agentos_svc_config_t* config
) {
    if (!out_service || !market_svc) return AGENTOS_EINVAL;

    market_adapter_ctx_t* ctx = calloc(1, sizeof(market_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    ctx->market_svc = market_svc;
    ctx->owns_service = false;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        ctx->common_cfg.name = "market_d";
        ctx->common_cfg.version = "1.0.0";
    }

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle, ctx->common_cfg.name, &market_adapter_iface, &ctx->common_cfg
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

market_service_t* market_service_adapter_get_original(agentos_service_t service) {
    if (!service) return NULL;
    market_adapter_ctx_t* ctx = market_get_ctx(service);
    return ctx ? ctx->market_svc : NULL;
}

agentos_error_t market_service_adapter_init(agentos_service_t service) {
    return market_adapter_init(service, NULL);
}

agentos_error_t market_service_adapter_start(agentos_service_t service) {
    return market_adapter_start(service);
}

agentos_error_t market_service_adapter_stop(agentos_service_t service, bool force) {
    return market_adapter_stop(service, force);
}

void market_service_adapter_destroy(agentos_service_t service) {
    market_adapter_destroy(service);
}

agentos_error_t market_service_adapter_healthcheck(agentos_service_t service) {
    return market_adapter_healthcheck(service);
}

const agentos_svc_interface_t* market_service_adapter_get_interface(void) {
    return &market_adapter_iface;
}
