/**
 * @file service.c
 * @brief 工具服务核心逻辑
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "service.h"
#include "svc_logger.h"
#include "svc_error.h"
#include "utils/tool_errors.h"
#include <stdlib.h>
#include <string.h>

tool_service_t* tool_service_create(const char* config_path) {
    tool_service_t* svc = calloc(1, sizeof(tool_service_t));
    if (!svc) return NULL;
    pthread_mutex_init(&svc->lock, NULL);

    /* 加载配置 */
    svc->config = tool_config_load(config_path);
    if (!svc->config) {
        SVC_LOG_ERROR("Failed to load config");
        goto fail;
    }

// From data intelligence emerges. by spharx
    /* 创建注册表 */
    svc->registry = tool_registry_create(svc->config);
    if (!svc->registry) {
        SVC_LOG_ERROR("Failed to create registry");
        goto fail;
    }

    /* 创建执行器 */
    svc->executor = tool_executor_create(svc->config);
    if (!svc->executor) {
        SVC_LOG_ERROR("Failed to create executor");
        goto fail;
    }

    /* 创建验证器 */
    svc->validator = tool_validator_create();
    if (!svc->validator) {
        SVC_LOG_ERROR("Failed to create validator");
        goto fail;
    }

    /* 创建缓存（若配置启用） */
    if (svc->config->cache_capacity > 0) {
        svc->cache = tool_cache_create(svc->config->cache_capacity,
                                        svc->config->cache_ttl_sec);
        if (!svc->cache) {
            SVC_LOG_WARN("Cache creation failed, continuing without cache");
        }
    }

    SVC_LOG_INFO("Tool service initialized");
    return svc;

fail:
    if (svc->registry) tool_registry_destroy(svc->registry);
    if (svc->executor) tool_executor_destroy(svc->executor);
    if (svc->validator) tool_validator_destroy(svc->validator);
    if (svc->cache) tool_cache_destroy(svc->cache);
    if (svc->config) tool_config_free(svc->config);
    pthread_mutex_destroy(&svc->lock);
    free(svc);
    return NULL;
}

void tool_service_destroy(tool_service_t* svc) {
    if (!svc) return;
    tool_registry_destroy(svc->registry);
    tool_executor_destroy(svc->executor);
    tool_validator_destroy(svc->validator);
    tool_cache_destroy(svc->cache);
    tool_config_free(svc->config);
    pthread_mutex_destroy(&svc->lock);
    free(svc);
}

/* 注册工具 */
int tool_service_register(tool_service_t* svc, const tool_metadata_t* meta) {
    if (!svc || !meta || !meta->id) return ERR_INVALID_ARG;
    pthread_mutex_lock(&svc->lock);
    int ret = tool_registry_add(svc->registry, meta);
    pthread_mutex_unlock(&svc->lock);
    return ret;
}

int tool_service_unregister(tool_service_t* svc, const char* tool_id) {
    if (!svc || !tool_id) return ERR_INVALID_ARG;
    pthread_mutex_lock(&svc->lock);
    int ret = tool_registry_remove(svc->registry, tool_id);
    pthread_mutex_unlock(&svc->lock);
    return ret;
}

tool_metadata_t* tool_service_get(tool_service_t* svc, const char* tool_id) {
    if (!svc || !tool_id) return NULL;
    pthread_mutex_lock(&svc->lock);
    tool_metadata_t* meta = tool_registry_get(svc->registry, tool_id);
    pthread_mutex_unlock(&svc->lock);
    return meta;  /* 调用者需 free */
}

char* tool_service_list(tool_service_t* svc) {
    if (!svc) return NULL;
    pthread_mutex_lock(&svc->lock);
    char* json = tool_registry_list_json(svc->registry);
    pthread_mutex_unlock(&svc->lock);
    return json;
}

/* 工具执行 */
int tool_service_execute(tool_service_t* svc,
                         const tool_execute_request_t* req,
                         tool_result_t** out_result) {
    if (!svc || !req || !out_result) return ERR_INVALID_ARG;

    /* 1. 获取工具元数据 */
    pthread_mutex_lock(&svc->lock);
    tool_metadata_t* meta = tool_registry_get(svc->registry, req->tool_id);
    pthread_mutex_unlock(&svc->lock);
    if (!meta) return TOOL_ERR_NOT_FOUND;

    /* 2. 验证参数 */
    if (svc->validator) {
        int valid = tool_validator_validate(svc->validator, meta, req->params_json);
        if (!valid) {
            tool_metadata_free(meta);
            return TOOL_ERR_INVALID_PARAMS;
        }
    }

    /* 3. 检查缓存（若可缓存） */
    if (svc->cache && meta->cacheable) {
        char* cache_key = tool_cache_key(req->tool_id, req->params_json);
        char* cached = NULL;
        if (tool_cache_get(svc->cache, cache_key, &cached) == 1 && cached) {
            tool_result_t* res = tool_result_from_json(cached);
            free(cached);
            free(cache_key);
            if (res) {
                tool_metadata_free(meta);
                *out_result = res;
                return 0;
            }
        }
        free(cache_key);
    }

    /* 4. 执行工具（通过 executor） */
    tool_result_t* res = NULL;
    int ret = tool_executor_run(svc->executor, meta, req->params_json, &res);
    if (ret != 0) {
        tool_metadata_free(meta);
        return ret;
    }

    /* 5. 存入缓存 */
    if (svc->cache && meta->cacheable && res->success == 0) {
        char* cache_key = tool_cache_key(req->tool_id, req->params_json);
        char* res_json = tool_result_to_json(res);
        if (res_json) {
            tool_cache_put(svc->cache, cache_key, res_json);
            free(res_json);
        }
        free(cache_key);
    }

    *out_result = res;
    tool_metadata_free(meta);
    return 0;
}

/* 流式执行（简化：先实现非流式，流式后续完善） */
int tool_service_execute_stream(tool_service_t* svc,
                                const tool_execute_request_t* req,
                                tool_stream_callback_t callback,
                                tool_result_t** out_result) {
    (void)svc; (void)req; (void)callback; (void)out_result;
    SVC_LOG_ERROR("Streaming execution not implemented");
    return TOOL_ERR_NOT_IMPLEMENTED;
}

void tool_result_free(tool_result_t* res) {
    if (!res) return;
    free(res->output);
    free(res->error);
    free(res);
}

void tool_metadata_free(tool_metadata_t* meta) {
    if (!meta) return;
    free(meta->id);
    free(meta->name);
    free(meta->description);
    free(meta->executable);
    for (size_t i = 0; i < meta->param_count; ++i) {
        free((void*)meta->params[i].name);
        free((void*)meta->params[i].schema);
    }
    free(meta->params);
    free(meta->permission_rule);
    free(meta);
}