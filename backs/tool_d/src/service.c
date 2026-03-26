/**
 * @file service.c
 * @brief 工具服务核心逻辑
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 统一错误码为 AGENTOS_ERR_*
 * 2. 完善流式执行功能
 * 3. 线程安全
 */

#include "service.h"
#include "svc_logger.h"
#include "error.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

/* ---------- 工具服务创建 ---------- */

tool_service_t* tool_service_create(const char* config_path) {
    (void)config_path;  /* 目前未使用配置文件 */
    
    tool_service_t* svc = (tool_service_t*)calloc(1, sizeof(tool_service_t));
    if (!svc) {
        SVC_LOG_ERROR("Failed to allocate tool service");
        return NULL;
    }
    
    if (agentos_mutex_init(&svc->lock) != 0) {
        SVC_LOG_ERROR("Failed to initialize service lock");
        free(svc);
        return NULL;
    }

    /* 创建注册表 */
    svc->registry = tool_registry_create();
    if (!svc->registry) {
        SVC_LOG_ERROR("Failed to create registry");
        agentos_mutex_destroy(&svc->lock);
        free(svc);
        return NULL;
    }

    /* 创建执行器 */
    tool_executor_config_t exec_config;
    memset(&exec_config, 0, sizeof(exec_config));
    exec_config.timeout_ms = 30000;
    
    svc->executor = tool_executor_create(&exec_config);
    if (!svc->executor) {
        SVC_LOG_ERROR("Failed to create executor");
        tool_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        free(svc);
        return NULL;
    }

    /* 创建验证器 */
    svc->validator = tool_validator_create();
    if (!svc->validator) {
        SVC_LOG_ERROR("Failed to create validator");
        tool_executor_destroy(svc->executor);
        tool_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        free(svc);
        return NULL;
    }

    /* 创建缓存（默认配置） */
    svc->cache = tool_cache_create(1024, 3600);
    if (!svc->cache) {
        SVC_LOG_WARN("Cache creation failed, continuing without cache");
    }

    SVC_LOG_INFO("Tool service initialized successfully");
    return svc;
}

/* ---------- 工具服务销毁 ---------- */

void tool_service_destroy(tool_service_t* svc) {
    if (!svc) return;
    
    if (svc->registry) {
        tool_registry_destroy(svc->registry);
        svc->registry = NULL;
    }
    
    if (svc->executor) {
        tool_executor_destroy(svc->executor);
        svc->executor = NULL;
    }
    
    if (svc->validator) {
        tool_validator_destroy(svc->validator);
        svc->validator = NULL;
    }
    
    if (svc->cache) {
        tool_cache_destroy(svc->cache);
        svc->cache = NULL;
    }
    
    agentos_mutex_destroy(&svc->lock);
    free(svc);
}

/* ---------- 工具注册 ---------- */

int tool_service_register(tool_service_t* svc, const tool_metadata_t* meta) {
    if (!svc || !meta || !meta->id) {
        SVC_LOG_ERROR("Invalid parameters to tool_service_register");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    agentos_mutex_lock(&svc->lock);
    int ret = tool_registry_add(svc->registry, meta);
    agentos_mutex_unlock(&svc->lock);
    
    if (ret == 0) {
        SVC_LOG_INFO("Registered tool: %s", meta->id);
    } else {
        SVC_LOG_ERROR("Failed to register tool: %s", meta->id);
    }
    
    return ret;
}

int tool_service_unregister(tool_service_t* svc, const char* tool_id) {
    if (!svc || !tool_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    agentos_mutex_lock(&svc->lock);
    int ret = tool_registry_remove(svc->registry, tool_id);
    agentos_mutex_unlock(&svc->lock);
    
    if (ret == 0) {
        SVC_LOG_INFO("Unregistered tool: %s", tool_id);
    }
    
    return ret;
}

tool_metadata_t* tool_service_get(tool_service_t* svc, const char* tool_id) {
    if (!svc || !tool_id) return NULL;
    
    agentos_mutex_lock(&svc->lock);
    tool_metadata_t* meta = tool_registry_get(svc->registry, tool_id);
    agentos_mutex_unlock(&svc->lock);
    
    return meta;
}

char* tool_service_list(tool_service_t* svc) {
    if (!svc) return NULL;
    
    agentos_mutex_lock(&svc->lock);
    char* json = tool_registry_list_json(svc->registry);
    agentos_mutex_unlock(&svc->lock);
    
    return json;
}

/* ---------- 工具执行 ---------- */

int tool_service_execute(tool_service_t* svc,
                         const tool_execute_request_t* req,
                         tool_result_t** out_result) {
    if (!svc || !req || !out_result) {
        SVC_LOG_ERROR("Invalid parameters to tool_service_execute");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    if (!req->tool_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 1. 获取工具元数据 */
    agentos_mutex_lock(&svc->lock);
    tool_metadata_t* meta = tool_registry_get(svc->registry, req->tool_id);
    agentos_mutex_unlock(&svc->lock);
    
    if (!meta) {
        SVC_LOG_ERROR("Tool not found: %s", req->tool_id);
        return AGENTOS_ERR_TOOL_NOT_FOUND;
    }

    /* 2. 验证参数 */
    if (svc->validator) {
        int valid = tool_validator_validate(svc->validator, meta, req->params_json);
        if (!valid) {
            SVC_LOG_WARN("Parameter validation failed for tool: %s", req->tool_id);
            tool_metadata_free(meta);
            return AGENTOS_ERR_TOOL_VALIDATION;
        }
    }

    /* 3. 检查缓存 */
    if (svc->cache && meta && meta->cacheable) {
        char* cache_key = tool_cache_key(req->tool_id, req->params_json);
        if (cache_key) {
            char* cached = NULL;
            if (tool_cache_get(svc->cache, cache_key, &cached) == 1 && cached) {
                tool_result_t* res = tool_result_from_json(cached);
                free(cached);
                cached = NULL;
                if (res) {
                    SVC_LOG_DEBUG("Cache hit for tool: %s", req->tool_id);
                    free(cache_key);
                    tool_metadata_free(meta);
                    *out_result = res;
                    return AGENTOS_OK;
                } else {
                    SVC_LOG_WARN("Failed to parse cached result for tool: %s", req->tool_id);
                }
            }
            free(cache_key);
            cache_key = NULL;
        }
    }

    /* 4. 执行工具 */
    tool_result_t* res = NULL;
    int ret = tool_executor_run(svc->executor, meta, req->params_json, &res);
    
    if (ret != 0) {
        SVC_LOG_ERROR("Tool execution failed: %s, error: %d", req->tool_id, ret);
        if (res) {
            tool_result_free(res);
            res = NULL;
        }
        tool_metadata_free(meta);
        meta = NULL;
        return ret;
    }

    /* 5. 存入缓存 */
    if (svc->cache && meta && meta->cacheable && res && res->success) {
        char* cache_key = tool_cache_key(req->tool_id, req->params_json);
        if (cache_key) {
            char* res_json = tool_result_to_json(res);
            if (res_json) {
                tool_cache_put(svc->cache, cache_key, res_json);
                free(res_json);
                res_json = NULL;
            }
            free(cache_key);
            cache_key = NULL;
        }
    }

    *out_result = res;
    if (meta) {
        tool_metadata_free(meta);
        meta = NULL;
    }
    return AGENTOS_OK;
}

/* ---------- 流式执行 ---------- */

int tool_service_execute_stream(tool_service_t* svc,
                                const tool_execute_request_t* req,
                                tool_stream_callback_t callback,
                                void* callback_data,
                                tool_result_t** out_result) {
    if (!svc || !req || !callback) {
        SVC_LOG_ERROR("Invalid parameters to tool_service_execute_stream");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    if (!req->tool_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 1. 获取工具元数据 */
    agentos_mutex_lock(&svc->lock);
    tool_metadata_t* meta = tool_registry_get(svc->registry, req->tool_id);
    agentos_mutex_unlock(&svc->lock);
    
    if (!meta) {
        SVC_LOG_ERROR("Tool not found: %s", req->tool_id);
        return AGENTOS_ERR_TOOL_NOT_FOUND;
    }

    /* 2. 验证参数 */
    if (svc->validator) {
        int valid = tool_validator_validate(svc->validator, meta, req->params_json);
        if (!valid) {
            SVC_LOG_WARN("Parameter validation failed for tool: %s", req->tool_id);
            tool_metadata_free(meta);
            return AGENTOS_ERR_TOOL_VALIDATION;
        }
    }

    /* 3. 检查是否支持流式 */
    if (!meta->executable || !strstr(meta->executable, "stream")) {
        /* 工具不支持流式，使用普通执行并逐块返回 */
        SVC_LOG_INFO("Tool does not support streaming, using synchronous execution");
    }

    /* 4. 执行工具（带流式回调） */
    int ret = tool_executor_run_async(svc->executor, meta, req->params_json,
                                      callback, callback_data, out_result);
    
    if (ret != 0) {
        SVC_LOG_ERROR("Tool stream execution failed: %s, error: %d", req->tool_id, ret);
    }
    
    tool_metadata_free(meta);
    return ret;
}

/* ---------- 工具结果释放 ---------- */

void tool_result_free(tool_result_t* res) {
    if (!res) return;
    free(res->output);
    free(res->error);
    free(res);
}

/* ---------- 工具元数据释放 ---------- */

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
