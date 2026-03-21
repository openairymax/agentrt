/**
 * @file service.c
 * @brief 服务核心逻辑实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "service.h"
#include "svc_logger.h"
#include "svc_error.h"
#include "svc_config.h"
#include "response.h"
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

/* ---------- 缓存键生成 ---------- */
static char* make_cache_key(const llm_request_config_t* config) {
    size_t len = strlen(config->model) + 2;
    for (size_t i = 0; i < config->message_count; ++i) {
        len += strlen(config->messages[i].role) + 1 +
               strlen(config->messages[i].content) + 1;
    }
    char* key = malloc(len);
    if (!key) return NULL;

// From data intelligence emerges. by spharx
    char* p = key;
    p = stpcpy(p, config->model);
    *p++ = '|';
    for (size_t i = 0; i < config->message_count; ++i) {
        p = stpcpy(p, config->messages[i].role);
        *p++ = ':';
        p = stpcpy(p, config->messages[i].content);
        *p++ = '|';
    }
    *(--p) = '\0';
    return key;
}

/* ---------- 从 YAML 节点加载定价规则 ---------- */
static pricing_rule_t* load_pricing_rules(cJSON* root, int* count) {
    cJSON* pricing = cJSON_GetObjectItem(root, "pricing");
    if (!pricing || !cJSON_IsArray(pricing)) {
        *count = 0;
        return NULL;
    }

    int n = cJSON_GetArraySize(pricing);
    pricing_rule_t* rules = calloc(n, sizeof(pricing_rule_t));
    if (!rules) return NULL;

    for (int i = 0; i < n; ++i) {
        cJSON* item = cJSON_GetArrayItem(pricing, i);
        cJSON* pattern = cJSON_GetObjectItem(item, "pattern");
        cJSON* input = cJSON_GetObjectItem(item, "input_price_per_k");
        cJSON* output = cJSON_GetObjectItem(item, "output_price_per_k");

        if (cJSON_IsString(pattern) && cJSON_IsNumber(input) && cJSON_IsNumber(output)) {
            rules[i].model_pattern = strdup(pattern->valuestring);
            rules[i].input_price_per_k = input->valuedouble;
            rules[i].output_price_per_k = output->valuedouble;
        } else {
            /* 格式错误，跳过 */
            rules[i].model_pattern = NULL;
        }
    }
    *count = n;
    return rules;
}

/* ---------- 创建服务 ---------- */
llm_service_t* llm_service_create(const char* config_path) {
    llm_service_t* svc = calloc(1, sizeof(llm_service_t));
    if (!svc) return NULL;
    pthread_mutex_init(&svc->lock, NULL);

    /* 加载基础配置 */
    service_config_t base_cfg;
    if (svc_config_load(config_path, &base_cfg) != 0) {
        SVC_LOG_ERROR("Failed to load base config");
        goto fail;
    }

    /* 额外解析定价规则（因为 svc_config 未包含） */
    FILE* f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_ERROR("Cannot open config for pricing");
        svc_config_free(&base_cfg);
        goto fail;
    }
    char* yaml_content = NULL;
    size_t yaml_len = 0;
    /* 简单读取整个文件（生产级应用应使用 YAML 解析器，这里简化为示例） */
    fseek(f, 0, SEEK_END);
    yaml_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    yaml_content = malloc(yaml_len + 1);
    fread(yaml_content, 1, yaml_len, f);
    yaml_content[yaml_len] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(yaml_content);
    free(yaml_content);
    if (!root) {
        SVC_LOG_ERROR("Failed to parse YAML for pricing");
        svc_config_free(&base_cfg);
        goto fail;
    }

    int rule_count = 0;
    pricing_rule_t* rules = load_pricing_rules(root, &rule_count);
    cJSON_Delete(root);

    svc->registry = provider_registry_create(&base_cfg);
    if (!svc->registry) {
        SVC_LOG_ERROR("Failed to create provider registry");
        svc_config_free(&base_cfg);
        free(rules);
        goto fail;
    }

    svc->cache = cache_create(base_cfg.cache_capacity, base_cfg.cache_ttl_sec);
    if (!svc->cache) {
        SVC_LOG_ERROR("Failed to create cache");
        provider_registry_destroy(svc->registry);
        svc_config_free(&base_cfg);
        free(rules);
        goto fail;
    }

    svc->cost = cost_tracker_create(rules, rule_count);
    if (!svc->cost) {
        SVC_LOG_ERROR("Failed to create cost tracker");
        cache_destroy(svc->cache);
        provider_registry_destroy(svc->registry);
        svc_config_free(&base_cfg);
        free(rules);
        goto fail;
    }

    svc->token_counter = token_counter_create(base_cfg.token_encoding);
    if (!svc->token_counter) {
        SVC_LOG_ERROR("Failed to create token counter");
        cost_tracker_destroy(svc->cost);
        cache_destroy(svc->cache);
        provider_registry_destroy(svc->registry);
        svc_config_free(&base_cfg);
        free(rules);
        goto fail;
    }

    svc_config_free(&base_cfg);
    free(rules);
    SVC_LOG_INFO("LLM service initialized");
    return svc;

fail:
    pthread_mutex_destroy(&svc->lock);
    free(svc);
    return NULL;
}

void llm_service_destroy(llm_service_t* svc) {
    if (!svc) return;
    provider_registry_destroy(svc->registry);
    cache_destroy(svc->cache);
    cost_tracker_destroy(svc->cost);
    token_counter_destroy(svc->token_counter);
    pthread_mutex_destroy(&svc->lock);
    free(svc);
}

/* ---------- 同步完成 ---------- */
int llm_service_complete(llm_service_t* svc,
                         const llm_request_config_t* config,
                         llm_response_t** out_response) {
    if (!svc || !config || !out_response) return ERR_INVALID_ARG;

    char* cache_key = make_cache_key(config);
    if (!cache_key) return ERR_NOMEM;

    /* 查缓存 */
    char* cached_json = NULL;
    if (cache_get(svc->cache, cache_key, &cached_json) == 1 && cached_json) {
        llm_response_t* cached_resp = response_from_json(cached_json);
        free(cached_json);
        if (cached_resp) {
            *out_response = cached_resp;
            free(cache_key);
            SVC_LOG_DEBUG("Cache hit for %s", cache_key);
            return 0;
        }
    }

    /* 找提供商 */
    pthread_mutex_lock(&svc->lock);
    const provider_t* prov = provider_registry_find(svc->registry, config->model);
    pthread_mutex_unlock(&svc->lock);
    if (!prov) {
        SVC_LOG_ERROR("No provider for model %s", config->model);
        free(cache_key);
        return ERR_NOT_FOUND;
    }

    llm_response_t* resp = NULL;
    int ret = prov->ops->complete(prov->ctx, config, &resp);
    if (ret != 0) {
        SVC_LOG_ERROR("Provider %s failed for model %s", prov->name, config->model);
        free(cache_key);
        return ret;
    }

    /* 更新成本 */
    cost_tracker_add(svc->cost, config->model,
                     resp->prompt_tokens, resp->completion_tokens);

    /* 存入缓存 */
    char* resp_json = response_to_json(resp);
    if (resp_json) {
        cache_put(svc->cache, cache_key, resp_json);
        free(resp_json);
    }

    *out_response = resp;
    free(cache_key);
    return 0;
}

/* ---------- 流式完成 ---------- */
int llm_service_complete_stream(llm_service_t* svc,
                                const llm_request_config_t* config,
                                llm_stream_callback_t callback,
                                llm_response_t** out_response) {
    if (!svc || !config || !callback) return ERR_INVALID_ARG;

    pthread_mutex_lock(&svc->lock);
    const provider_t* prov = provider_registry_find(svc->registry, config->model);
    pthread_mutex_unlock(&svc->lock);
    if (!prov) {
        SVC_LOG_ERROR("No provider for model %s", config->model);
        return ERR_NOT_FOUND;
    }

    int ret = prov->ops->complete_stream(prov->ctx, config, callback, out_response);
    if (ret == 0 && out_response && *out_response) {
        llm_response_t* resp = *out_response;
        cost_tracker_add(svc->cost, config->model,
                         resp->prompt_tokens, resp->completion_tokens);
    }
    return ret;
}

/* ---------- 统计 ---------- */
int llm_service_stats(llm_service_t* svc, char** out_json) {
    if (!svc || !out_json) return ERR_INVALID_ARG;
    cJSON* root = cJSON_CreateObject();
    cJSON* cost_json = cost_tracker_export(svc->cost);
    if (cost_json) cJSON_AddItemToObject(root, "cost", cost_json);
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return ERR_NOMEM;
    *out_json = json;
    return 0;
}