#include "memory_compat.h"
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

#include "daemon_defaults.h"
#include "daemon_security.h"
#include "error.h"
#include "executor.h"
#include "platform.h"
#include "service.h"
#include "svc_logger.h"
#include "tool_approval.h"

#include <stdlib.h>
#include <string.h>

/* ---------- 工具服务创建 ---------- */

tool_service_t *tool_service_create(const char *config_path __attribute__((unused)))
{

    tool_service_t *svc = (tool_service_t *)AGENTOS_CALLOC(1, sizeof(tool_service_t));
    if (!svc) {
        SVC_LOG_ERROR("Failed to allocate tool service");
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    if (agentos_mutex_init(&svc->lock) != 0) {
        SVC_LOG_ERROR("Failed to initialize service lock");
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "validation failed");
    }

    /* 创建注册表 */
    svc->registry = tool_registry_create(NULL);
    if (!svc->registry) {
        SVC_LOG_ERROR("Failed to create registry");
        agentos_mutex_destroy(&svc->lock);
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    /* 创建执行器 */
    tool_executor_config_t exec_config;
    __builtin_memset(&exec_config, 0, sizeof(exec_config));
    exec_config.timeout_sec = AGENTOS_DEFAULT_TIMEOUT_SEC;

    svc->executor = tool_executor_create_ex(&exec_config);
    if (!svc->executor) {
        SVC_LOG_ERROR("Failed to create executor");
        tool_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    /* P3.17 (ACC-DT18): 默认启用工具审批（enable_approval=true）。
     * 创建 approval_ctx 并注入 executor，使所有工具执行必须通过 Cupolas 安全穹顶审批。
     * executor.c 已改为 fail-closed：未注入 approval_ctx 时拒绝执行。
     * daemon_security 采用 fail-closed ACL：无 ACL 条目 = 拒绝。
     * 部署时需通过 daemon_security_add_acl_rule() 注册授权的工具。*/
    daemon_security_init(NULL, NULL);
    tool_approval_config_t approval_cfg;
    __builtin_memset(&approval_cfg, 0, sizeof(approval_cfg));
    approval_cfg.agent_id = "tool_d";
    approval_cfg.enable_safety_guard_chain = true;
    approval_cfg.enable_audit_logging = true;
    approval_cfg.permission_rules = NULL;
    tool_approval_ctx_t *approval_ctx = tool_approval_create(&approval_cfg);
    if (approval_ctx) {
        tool_executor_set_approval_ctx(svc->executor, approval_ctx);
        SVC_LOG_INFO("C-L05: Default tool approval context attached (enable_approval=true)");
    } else {
        SVC_LOG_ERROR("C-L05: Failed to create default approval context — "
                      "executor will fail-closed on all tool executions");
    }

    /* 创建验证器 */
    svc->validator = tool_validator_create();
    if (!svc->validator) {
        SVC_LOG_ERROR("Failed to create validator");
        tool_executor_destroy(svc->executor);
        tool_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
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

void tool_service_destroy(tool_service_t *svc)
{
    if (!svc)
        return;

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
    AGENTOS_FREE(svc);
}

/* ---------- 工具注册 ---------- */

int tool_service_register(tool_service_t *svc, const tool_metadata_t *meta)
{
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

int tool_service_unregister(tool_service_t *svc, const char *tool_id)
{
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

tool_metadata_t *tool_service_get(tool_service_t *svc, const char *tool_id)
{
    if (!svc || !tool_id) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        }

    agentos_mutex_lock(&svc->lock);
    tool_metadata_t *meta = tool_registry_get(svc->registry, tool_id);
    agentos_mutex_unlock(&svc->lock);

    return meta;
}

char *tool_service_list(tool_service_t *svc)
{
    if (!svc) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        }

    agentos_mutex_lock(&svc->lock);
    char *json = tool_registry_list_json(svc->registry);
    agentos_mutex_unlock(&svc->lock);

    return json;
}

/* ---------- 辅助函数（降低 tool_service_execute 复杂度） ---------- */

/**
 * @brief 获取工具元数据
 */
static tool_metadata_t *get_tool_metadata(tool_service_t *svc, const char *tool_id)
{
    if (!svc || !tool_id) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    agentos_mutex_lock(&svc->lock);
    tool_metadata_t *meta = tool_registry_get(svc->registry, tool_id);
    agentos_mutex_unlock(&svc->lock);

    return meta;
}

/**
 * @brief 验证工具参数
 */
static int validate_tool_params(tool_service_t *svc, tool_metadata_t *meta, const char *tool_id,
                                const char *params_json)
{
    if (!svc || !meta || !tool_id) {
        return AGENTOS_EINVAL;
    }

    if (svc->validator) {
        int valid = tool_validator_validate(svc->validator, meta, params_json);
        if (!valid) {
            SVC_LOG_WARN("Parameter validation failed for tool: %s", tool_id);
            return 0;
        }
    }
    return 1;
}

/**
 * @brief 获取缓存的工具结果
 */
static tool_result_t *get_cached_result(tool_service_t *svc, tool_metadata_t *meta,
                                        const char *tool_id, const char *params_json)
{
    if (!svc || !meta || !tool_id || !params_json) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    if (!meta->cacheable) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    char *cache_key = tool_cache_key(tool_id, params_json);
    if (!cache_key) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    char *cached = NULL;
    if (tool_cache_get(svc->cache, cache_key, &cached) == 1 && cached) {
        tool_result_t *res = tool_result_from_json(cached);
        AGENTOS_FREE(cached);
        cached = NULL;
        if (res) {
            SVC_LOG_DEBUG("Cache hit for tool: %s", tool_id);
            AGENTOS_FREE(cache_key);
            return res;
        }
        SVC_LOG_WARN("Failed to parse cached result for tool: %s", tool_id);
    }

    AGENTOS_FREE(cache_key);
    cache_key = NULL;
    AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "operation failed");
}

/**
 * @brief 缓存工具结果
 */
static void cache_tool_result(tool_service_t *svc, tool_metadata_t *meta, const char *tool_id,
                              const char *params_json, tool_result_t *res)
{
    if (!svc || !meta || !tool_id || !params_json || !res || !res->success) {
        return;
    }

    if (!meta->cacheable) {
        return;
    }

    char *cache_key = tool_cache_key(tool_id, params_json);
    if (!cache_key) {
        return;
    }

    char *res_json = tool_result_to_json(res);
    if (res_json) {
        tool_cache_put(svc->cache, cache_key, res_json);
        AGENTOS_FREE(res_json);
        res_json = NULL;
    }

    AGENTOS_FREE(cache_key);
    cache_key = NULL;
}

/**
 * @brief 执行工具
 */
static int do_execute_tool(tool_service_t *svc, tool_metadata_t *meta, const char *params_json,
                           tool_result_t **out_result)
{
    if (!svc || !meta || !out_result) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    tool_result_t *res = NULL;
    int ret = tool_executor_run(svc->executor, meta, params_json, &res);

    if (ret != 0) {
        SVC_LOG_ERROR("Tool execution failed, error: %d", ret);
        if (res) {
            tool_result_free(res);
            res = NULL;
        }
        return ret;
    }

    *out_result = res;
    return AGENTOS_OK;
}

/* ---------- 工具执行（重构后：圈复杂度从 18 降至 8） ---------- */

int tool_service_execute(tool_service_t *svc, const tool_execute_request_t *req,
                         tool_result_t **out_result)
{
    if (!svc || !req || !out_result) {
        SVC_LOG_ERROR("Invalid parameters to tool_service_execute");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!req->tool_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 1. 获取工具元数据 */
    tool_metadata_t *meta = get_tool_metadata(svc, req->tool_id);
    if (!meta) {
        SVC_LOG_ERROR("Tool not found: %s", req->tool_id);
        return AGENTOS_ERROR_TOOL_NOT_FOUND;
    }

    /* 2. 验证参数 */
    int valid = validate_tool_params(svc, meta, req->tool_id, req->params_json);
    if (valid <= 0) {
        tool_metadata_free(meta);
        return AGENTOS_ERROR_TOOL_VALIDATION;
    }

    /* 3. 检查缓存 */
    tool_result_t *cached_result = get_cached_result(svc, meta, req->tool_id, req->params_json);
    if (cached_result) {
        tool_metadata_free(meta);
        *out_result = cached_result;
        return AGENTOS_OK;
    }

    /* 4. 执行工具 */
    tool_result_t *res = NULL;
    int ret = do_execute_tool(svc, meta, req->params_json, &res);
    if (ret != 0) {
        tool_metadata_free(meta);
        meta = NULL;
        return ret;
    }

    /* 5. 存入缓存 */
    cache_tool_result(svc, meta, req->tool_id, req->params_json, res);

    *out_result = res;
    if (meta) {
        tool_metadata_free(meta);
        meta = NULL;
    }
    return AGENTOS_OK;
}

/* ---------- 流式执行 ---------- */

int tool_service_execute_stream(tool_service_t *svc, const tool_execute_request_t *req,
                                tool_stream_callback_t callback, void *callback_data,
                                tool_result_t **out_result)
{
    if (!svc || !req || !callback) {
        SVC_LOG_ERROR("Invalid parameters to tool_service_execute_stream");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!req->tool_id) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 1. 获取工具元数据 */
    agentos_mutex_lock(&svc->lock);
    tool_metadata_t *meta = tool_registry_get(svc->registry, req->tool_id);
    agentos_mutex_unlock(&svc->lock);

    if (!meta) {
        SVC_LOG_ERROR("Tool not found: %s", req->tool_id);
        return AGENTOS_ERROR_TOOL_NOT_FOUND;
    }

    /* 2. 验证参数 */
    if (svc->validator) {
        int valid = tool_validator_validate(svc->validator, meta, req->params_json);
        if (!valid) {
            SVC_LOG_WARN("Parameter validation failed for tool: %s", req->tool_id);
            tool_metadata_free(meta);
            return AGENTOS_ERROR_TOOL_VALIDATION;
        }
    }

    /* 3. 检查是否支持流式 */
    if (!meta->executable || !strstr(meta->executable, "stream")) {
        /* 工具不支持流式，使用普通执行并逐块返回 */
        SVC_LOG_INFO("Tool does not support streaming, using synchronous execution");
    }

    /* 4. 执行工具（带流式回调） */
    tool_result_t *res = NULL;
    int ret = tool_executor_run(svc->executor, meta, req->params_json, &res);

    if (ret == 0 && res) {
        if (callback) {
            if (res->output) {
                callback(res->output, 0, callback_data);
            }
            if (res->error) {
                callback(res->error, 1, callback_data);
            }
        }
        if (out_result) {
            *out_result = res;
        }
    } else {
        if (out_result) {
            *out_result = res;
        }
    }

    if (ret != 0) {
        SVC_LOG_ERROR("Tool stream execution failed: %s, error: %d", req->tool_id, ret);
    }

    tool_metadata_free(meta);
    return ret;
}

/* ---------- 工具结果释放 ---------- */

void tool_result_free(tool_result_t *res)
{
    if (!res)
        return;
    AGENTOS_FREE(res->output);
    AGENTOS_FREE(res->error);
    AGENTOS_FREE(res);
}

/* ---------- 工具元数据释放 ---------- */

void tool_metadata_free(tool_metadata_t *meta)
{
    if (!meta)
        return;
    AGENTOS_FREE(meta->id);
    AGENTOS_FREE(meta->name);
    AGENTOS_FREE(meta->description);
    AGENTOS_FREE(meta->executable);

    for (size_t i = 0; i < meta->param_count; ++i) {
        AGENTOS_FREE((void *)meta->params[i].name);
        AGENTOS_FREE((void *)meta->params[i].schema);
    }
    AGENTOS_FREE(meta->params);

    AGENTOS_FREE(meta->permission_rule);
    AGENTOS_FREE(meta);
}
