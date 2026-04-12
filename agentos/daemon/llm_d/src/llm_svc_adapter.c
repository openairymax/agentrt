/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file llm_svc_adapter.c
 * @brief LLM服务适配器：将LLM服务适配到统一的AgentOS服务管理框架
 *
 * LLM服务的create接口接受config_path字符串而非配置结构体，
 * 本适配器将通用服务配置转换为LLM服务所需的配置路径格式。
 * 使用 agentos_service_set/get_user_data 存取适配器上下文，
 * 避免类型强转导致的类型安全问题。
 */

#include "llm_service.h"
#include "svc_common.h"
#include "svc_logger.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    llm_service_t llm_svc;
    char* config_path;
    agentos_svc_config_t common_cfg;
    bool owns_service;
} llm_adapter_ctx_t;

static llm_adapter_ctx_t* llm_get_ctx(agentos_service_t service) {
    if (!service) return NULL;
    return (llm_adapter_ctx_t*)agentos_service_get_user_data(service);
}

static agentos_error_t llm_adapter_init(
    agentos_service_t service,
    const agentos_svc_config_t* config
) {
    if (!service) return AGENTOS_EINVAL;

    llm_adapter_ctx_t* ctx = llm_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    }

    if (!ctx->llm_svc) {
        const char* path = ctx->config_path ? ctx->config_path : "llm_config.json";
        ctx->llm_svc = llm_service_create(path);
        if (!ctx->llm_svc) {
            svc_logger_error("LLM服务创建失败");
            return AGENTOS_ERROR;
        }
        ctx->owns_service = true;
    }

    return AGENTOS_SUCCESS;
}

static agentos_error_t llm_adapter_start(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    llm_adapter_ctx_t* ctx = llm_get_ctx(service);
    if (!ctx || !ctx->llm_svc) return AGENTOS_ENOTINIT;
    svc_logger_info("LLM服务适配器已启动");
    return AGENTOS_SUCCESS;
}

static agentos_error_t llm_adapter_stop(agentos_service_t service, bool force) {
    if (!service) return AGENTOS_EINVAL;
    llm_adapter_ctx_t* ctx = llm_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;
    svc_logger_info("LLM服务适配器已停止");
    return AGENTOS_SUCCESS;
}

static void llm_adapter_destroy(agentos_service_t service) {
    if (!service) return;
    llm_adapter_ctx_t* ctx = llm_get_ctx(service);
    if (!ctx) return;

    if (ctx->llm_svc && ctx->owns_service) {
        llm_service_destroy(ctx->llm_svc);
        ctx->llm_svc = NULL;
    }

    if (ctx->config_path) {
        free(ctx->config_path);
        ctx->config_path = NULL;
    }

    agentos_service_set_user_data(service, NULL);
    free(ctx);
}

static agentos_error_t llm_adapter_healthcheck(agentos_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    llm_adapter_ctx_t* ctx = llm_get_ctx(service);
    if (!ctx) return AGENTOS_EINVAL;
    return ctx->llm_svc ? AGENTOS_SUCCESS : AGENTOS_ENOTINIT;
}

static const agentos_svc_interface_t llm_adapter_iface = {
    .init = llm_adapter_init,
    .start = llm_adapter_start,
    .stop = llm_adapter_stop,
    .destroy = llm_adapter_destroy,
    .healthcheck = llm_adapter_healthcheck,
};

agentos_error_t llm_service_adapter_create(
    agentos_service_t* out_service,
    const agentos_svc_config_t* config
) {
    if (!out_service) return AGENTOS_EINVAL;

    llm_adapter_ctx_t* ctx = calloc(1, sizeof(llm_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
        if (config->name) {
            size_t path_len = strlen(config->name) + strlen("_config.json") + 1;
            ctx->config_path = calloc(1, path_len);
            if (ctx->config_path) {
                snprintf(ctx->config_path, path_len, "%s_config.json", config->name);
            }
        }
    } else {
        ctx->common_cfg.name = "llm_d";
        ctx->common_cfg.version = "1.0.0";
        ctx->common_cfg.capabilities = AGENTOS_SVC_CAP_ASYNC;
        ctx->common_cfg.enable_metrics = true;
    }

    ctx->owns_service = true;

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle,
        ctx->common_cfg.name,
        &llm_adapter_iface,
        &ctx->common_cfg
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

agentos_error_t llm_service_adapter_wrap(
    agentos_service_t* out_service,
    llm_service_t llm_svc,
    const agentos_svc_config_t* config
) {
    if (!out_service || !llm_svc) return AGENTOS_EINVAL;

    llm_adapter_ctx_t* ctx = calloc(1, sizeof(llm_adapter_ctx_t));
    if (!ctx) return AGENTOS_ENOMEM;

    ctx->llm_svc = llm_svc;
    ctx->owns_service = false;

    if (config) {
        memcpy(&ctx->common_cfg, config, sizeof(agentos_svc_config_t));
    } else {
        ctx->common_cfg.name = "llm_d";
        ctx->common_cfg.version = "1.0.0";
    }

    agentos_service_t svc_handle = NULL;
    agentos_error_t err = agentos_service_create(
        &svc_handle,
        ctx->common_cfg.name,
        &llm_adapter_iface,
        &ctx->common_cfg
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

llm_service_t llm_service_adapter_get_original(agentos_service_t service) {
    if (!service) return NULL;
    llm_adapter_ctx_t* ctx = llm_get_ctx(service);
    return ctx ? ctx->llm_svc : NULL;
}

agentos_error_t llm_service_adapter_init(agentos_service_t service) {
    return llm_adapter_init(service, NULL);
}

agentos_error_t llm_service_adapter_start(agentos_service_t service) {
    return llm_adapter_start(service);
}

agentos_error_t llm_service_adapter_stop(agentos_service_t service, bool force) {
    return llm_adapter_stop(service, force);
}

void llm_service_adapter_destroy(agentos_service_t service) {
    llm_adapter_destroy(service);
}

agentos_error_t llm_service_adapter_healthcheck(agentos_service_t service) {
    return llm_adapter_healthcheck(service);
}

const agentos_svc_interface_t* llm_service_adapter_get_interface(void) {
    return &llm_adapter_iface;
}
