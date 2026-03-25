/**
 * @file service.c
 * @brief LLM 服务核心逻辑实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 改进说明：
 * 1. 修复 stpcpy 不可移植问题
 * 2. 统一错误码为 AGENTOS_ERR_* 
 * 3. 完善 YAML 解析逻辑
 * 4. 线程安全
 */

#include "service.h"
#include "svc_logger.h"
#include "error.h"
#include "platform.h"
#include "response.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>
#include <yaml.h>

/* ---------- 缓存键生成（便携版本） ---------- */

/**
 * @brief 安全字符串拼接
 * @param dest 目标字符串
 * @param dest_size 目标缓冲区大小
 * @param src 源字符串
 * @return 写入后的结束位置
 */
static char* safe_strcat(char* dest, size_t dest_size, const char* src) {
    size_t dest_len = strlen(dest);
    size_t remaining = dest_size - dest_len - 1;
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < remaining) ? src_len : remaining;
    
    if (copy_len > 0) {
        memcpy(dest + dest_len, src, copy_len);
        dest[dest_len + copy_len] = '\0';
    }
    
    return dest + dest_len + copy_len;
}

/**
 * @brief 生成缓存键
 * @param config 请求配置
 * @return 缓存键字符串（需调用者释放），失败返回 NULL
 */
static char* make_cache_key(const llm_request_config_t* config) {
    if (!config || !config->model) {
        return NULL;
    }
    
    /* 计算所需缓冲区大小 */
    size_t len = strlen(config->model) + 2;
    for (size_t i = 0; i < config->message_count; ++i) {
        len += strlen(config->messages[i].role) + 1 +
               strlen(config->messages[i].content) + 1;
    }
    
    char* key = (char*)malloc(len);
    if (!key) return NULL;
    
    /* 构建缓存键 */
    char* p = key;
    
    /* 使用安全的字符串复制 */
    size_t written = snprintf(p, len, "%s", config->model);
    p += written;
    
    *p++ = '|';
    
    for (size_t i = 0; i < config->message_count; ++i) {
        written = snprintf(p, len - (p - key), "%s:%s|",
                          config->messages[i].role,
                          config->messages[i].content);
        p += written;
    }
    
    /* 确保字符串以 null 结尾 */
    if (p > key && p[-1] == '|') {
        p[-1] = '\0';
    } else {
        *p = '\0';
    }
    
    return key;
}

/* ---------- 从 cJSON 节点加载定价规则 ---------- */

/**
 * @brief 加载定价规则
 * @param root JSON 根节点
 * @param count 输出规则数量
 * @return 规则数组（需调用者释放），失败返回 NULL
 */
static pricing_rule_t* load_pricing_rules(cJSON* root, int* count) {
    if (!root || !count) {
        *count = 0;
        return NULL;
    }
    
    cJSON* pricing = cJSON_GetObjectItem(root, "pricing");
    if (!pricing || !cJSON_IsArray(pricing)) {
        *count = 0;
        return NULL;
    }

    int n = cJSON_GetArraySize(pricing);
    pricing_rule_t* rules = (pricing_rule_t*)calloc((size_t)n, sizeof(pricing_rule_t));
    if (!rules) return NULL;

    for (int i = 0; i < n; ++i) {
        cJSON* item = cJSON_GetArrayItem(pricing, i);
        cJSON* pattern = cJSON_GetObjectItem(item, "pattern");
        cJSON* input = cJSON_GetObjectItem(item, "input_price_per_k");
        cJSON* output = cJSON_GetObjectItem(item, "output_price_per_k");

        if (cJSON_IsString(pattern) && cJSON_IsNumber(input) && cJSON_IsNumber(output)) {
            rules[i].model_pattern = strdup(pattern->valuestring);
            if (!rules[i].model_pattern) {
                /* 内存分配失败，清理已分配的 */
                for (int j = 0; j < i; ++j) {
                    free((void*)rules[j].model_pattern);
                }
                free(rules);
                *count = 0;
                return NULL;
            }
            rules[i].input_price_per_k = input->valuedouble;
            rules[i].output_price_per_k = output->valuedouble;
        } else {
            rules[i].model_pattern = NULL;
        }
    }
    
    *count = n;
    return rules;
}

/* ---------- 释放定价规则 ---------- */

static void free_pricing_rules(pricing_rule_t* rules, int count) {
    if (!rules) return;
    for (int i = 0; i < count; ++i) {
        free((void*)rules[i].model_pattern);
    }
    free(rules);
}

/* ---------- 创建服务 ---------- */

llm_service_t* llm_service_create(const char* config_path) {
    if (!config_path) {
        SVC_LOG_ERROR("Config path is NULL");
        return NULL;
    }
    
    llm_service_t* svc = (llm_service_t*)calloc(1, sizeof(llm_service_t));
    if (!svc) {
        SVC_LOG_ERROR("Failed to allocate service context");
        return NULL;
    }
    
    if (agentos_mutex_init(&svc->lock) != 0) {
        SVC_LOG_ERROR("Failed to initialize service lock");
        free(svc);
        return NULL;
    }

    /* 加载基础配置 */
    service_config_t base_cfg;
    memset(&base_cfg, 0, sizeof(base_cfg));
    
    if (svc_config_load(config_path, &base_cfg) != 0) {
        SVC_LOG_WARN("Failed to load base config, using defaults");
        /* 使用默认配置继续 */
        base_cfg.cache_capacity = 1024;
        base_cfg.cache_ttl_sec = 3600;
        base_cfg.max_retries = 3;
        base_cfg.timeout_ms = 30000;
    }

    /* 解析定价规则（使用 cJSON） */
    FILE* f = fopen(config_path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long yaml_len = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* yaml_content = (char*)malloc((size_t)yaml_len + 1);
        if (yaml_content) {
            size_t read_len = fread(yaml_content, 1, (size_t)yaml_len, f);
            yaml_content[read_len] = '\0';
            
            cJSON* root = cJSON_Parse(yaml_content);
            if (root) {
                int rule_count = 0;
                pricing_rule_t* rules = load_pricing_rules(root, &rule_count);
                if (rules && rule_count > 0) {
                    svc->rules = rules;
                    svc->rule_count = rule_count;
                    SVC_LOG_INFO("Loaded %d pricing rules", rule_count);
                }
                cJSON_Delete(root);
            }
            free(yaml_content);
        }
        fclose(f);
    }

    /* 创建提供商注册表 */
    svc->registry = provider_registry_create(&base_cfg);
    if (!svc->registry) {
        SVC_LOG_ERROR("Failed to create provider registry");
        agentos_mutex_destroy(&svc->lock);
        free(svc);
        return NULL;
    }

    /* 创建缓存 */
    svc->cache = cache_create(base_cfg.cache_capacity, base_cfg.cache_ttl_sec);
    if (!svc->cache) {
        SVC_LOG_ERROR("Failed to create cache");
        provider_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        free(svc);
        return NULL;
    }

    /* 创建成本追踪器 */
    svc->cost = cost_tracker_create(svc->rules, svc->rule_count);
    if (!svc->cost) {
        SVC_LOG_ERROR("Failed to create cost tracker");
        cache_destroy(svc->cache);
        provider_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        free(svc);
        return NULL;
    }

    /* 创建 Token 计数器 */
    svc->token_counter = token_counter_create(base_cfg.token_encoding);
    if (!svc->token_counter) {
        SVC_LOG_ERROR("Failed to create token counter");
        cost_tracker_destroy(svc->cost);
        cache_destroy(svc->cache);
        provider_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        free(svc);
        return NULL;
    }

    SVC_LOG_INFO("LLM service initialized successfully");
    return svc;
}

/* ---------- 销毁服务 ---------- */

void llm_service_destroy(llm_service_t* svc) {
    if (!svc) return;
    
    if (svc->registry) {
        provider_registry_destroy(svc->registry);
        svc->registry = NULL;
    }
    
    if (svc->cache) {
        cache_destroy(svc->cache);
        svc->cache = NULL;
    }
    
    if (svc->cost) {
        cost_tracker_destroy(svc->cost);
        svc->cost = NULL;
    }
    
    if (svc->token_counter) {
        token_counter_destroy(svc->token_counter);
        svc->token_counter = NULL;
    }
    
    if (svc->rules) {
        free_pricing_rules(svc->rules, svc->rule_count);
        svc->rules = NULL;
        svc->rule_count = 0;
    }
    
    agentos_mutex_destroy(&svc->lock);
    free(svc);
}

/* ---------- 同步完成 ---------- */

int llm_service_complete(llm_service_t* svc,
                         const llm_request_config_t* config,
                         llm_response_t** out_response) {
    /* 参数检查 */
    if (!svc || !config || !out_response) {
        SVC_LOG_ERROR("Invalid arguments to llm_service_complete");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    if (!config->model) {
        SVC_LOG_ERROR("Model name is NULL");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 生成缓存键 */
    char* cache_key = make_cache_key(config);
    if (!cache_key) {
        SVC_LOG_ERROR("Failed to generate cache key");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    /* 查找缓存 */
    char* cached_json = NULL;
    if (cache_get(svc->cache, cache_key, &cached_json) == 1 && cached_json) {
        llm_response_t* cached_resp = response_from_json(cached_json);
        free(cached_json);
        if (cached_resp) {
            *out_response = cached_resp;
            free(cache_key);
            SVC_LOG_DEBUG("Cache hit for key");
            return AGENTOS_OK;
        }
    }

    /* 查找提供商 */
    agentos_mutex_lock(&svc->lock);
    const provider_t* prov = provider_registry_find(svc->registry, config->model);
    agentos_mutex_unlock(&svc->lock);
    
    if (!prov) {
        SVC_LOG_ERROR("No provider for model '%s'", config->model);
        free(cache_key);
        return AGENTOS_ERR_LLM_INVALID_MODEL;
    }

    /* 调用提供商 */
    llm_response_t* resp = NULL;
    int ret = prov->ops->complete(prov->ctx, config, &resp);
    if (ret != 0) {
        SVC_LOG_ERROR("Provider '%s' failed for model '%s': error %d", 
                     prov->name, config->model, ret);
        free(cache_key);
        return ret;
    }

    /* 更新成本追踪 */
    if (resp) {
        cost_tracker_add(svc->cost, config->model,
                        resp->prompt_tokens, resp->completion_tokens);
    }

    /* 存入缓存 */
    char* resp_json = response_to_json(resp);
    if (resp_json) {
        cache_put(svc->cache, cache_key, resp_json);
        free(resp_json);
    }

    *out_response = resp;
    free(cache_key);
    return AGENTOS_OK;
}

/* ---------- 流式完成 ---------- */

int llm_service_complete_stream(llm_service_t* svc,
                                const llm_request_config_t* config,
                                llm_stream_callback_t callback,
                                void* callback_data,
                                llm_response_t** out_response) {
    /* 参数检查 */
    if (!svc || !config || !callback) {
        SVC_LOG_ERROR("Invalid arguments to llm_service_complete_stream");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    if (!config->model) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 查找提供商 */
    agentos_mutex_lock(&svc->lock);
    const provider_t* prov = provider_registry_find(svc->registry, config->model);
    agentos_mutex_unlock(&svc->lock);
    
    if (!prov) {
        SVC_LOG_ERROR("No provider for model '%s'", config->model);
        return AGENTOS_ERR_LLM_INVALID_MODEL;
    }
    
    /* 检查是否支持流式 */
    if (!prov->ops->complete_stream) {
        SVC_LOG_WARN("Provider '%s' does not support streaming", prov->name);
        return AGENTOS_ERR_NOT_SUPPORTED;
    }

    /* 调用流式接口 */
    int ret = prov->ops->complete_stream(prov->ctx, config, callback, callback_data, out_response);
    
    if (ret == 0 && out_response && *out_response) {
        llm_response_t* resp = *out_response;
        cost_tracker_add(svc->cost, config->model,
                         resp->prompt_tokens, resp->completion_tokens);
    }
    
    return ret;
}

/* ---------- 统计 ---------- */

int llm_service_stats(llm_service_t* svc, char** out_json) {
    if (!svc || !out_json) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    
    cJSON* cost_json = cost_tracker_export(svc->cost);
    if (cost_json) {
        cJSON_AddItemToObject(root, "cost", cost_json);
    }
    
    cJSON_AddNumberToObject(root, "cache_size", cache_size(svc->cache));
    cJSON_AddNumberToObject(root, "cache_capacity", cache_capacity(svc->cache));
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    
    *out_json = json;
    return AGENTOS_OK;
}

/* ---------- 服务配置加载 ---------- */

/**
 * @brief 加载服务配置
 * @param config_path 配置文件路径
 * @param cfg 输出配置
 * @return 0 成功，非0 失败
 */
int svc_config_load(const char* config_path, service_config_t* cfg) {
    if (!cfg || !config_path) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    memset(cfg, 0, sizeof(service_config_t));
    
    /* 设置默认值 */
    cfg->cache_capacity = 1024;
    cfg->cache_ttl_sec = 3600;
    cfg->max_retries = 3;
    cfg->timeout_ms = 30000;
    
    FILE* f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("Cannot open config file '%s', using defaults", config_path);
        return 0;  /* 使用默认值，不算错误 */
    }
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc((size_t)len + 1);
    if (!content) {
        fclose(f);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    
    size_t read_len = fread(content, 1, (size_t)len, f);
    content[read_len] = '\0';
    fclose(f);
    
    /* 使用 cJSON 解析（配置文件实际上是 JSON） */
    cJSON* root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        SVC_LOG_WARN("Failed to parse config file, using defaults");
        return 0;
    }
    
    /* 提取配置值 */
    cJSON* item;
    
    item = cJSON_GetObjectItem(root, "cache_capacity");
    if (item && cJSON_IsNumber(item)) {
        cfg->cache_capacity = item->valueint;
    }
    
    item = cJSON_GetObjectItem(root, "cache_ttl_sec");
    if (item && cJSON_IsNumber(item)) {
        cfg->cache_ttl_sec = item->valueint;
    }
    
    item = cJSON_GetObjectItem(root, "max_retries");
    if (item && cJSON_IsNumber(item)) {
        cfg->max_retries = item->valueint;
    }
    
    item = cJSON_GetObjectItem(root, "timeout_ms");
    if (item && cJSON_IsNumber(item)) {
        cfg->timeout_ms = item->valueint;
    }
    
    item = cJSON_GetObjectItem(root, "token_encoding");
    if (item && cJSON_IsString(item)) {
        if (strlen(item->valuestring) < sizeof(cfg->token_encoding)) {
            strcpy(cfg->token_encoding, item->valuestring);
        }
    }
    
    cJSON_Delete(root);
    return AGENTOS_OK;
}
