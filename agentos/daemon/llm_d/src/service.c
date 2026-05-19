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
#include "daemon_defaults.h"
#include "svc_logger.h"
#include "error.h"
#include "platform.h"
#include "response.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>
#ifdef HAVE_YAML
#include <yaml.h>
#endif

static int ends_with(const char* str, const char* suffix) {
    if (!str || !suffix) return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/* ---------- 缓存键生成（便携版本） ---------- */

/**
 * @brief 安全字符串拼接
 * @param dest 目标字符串
 * @param dest_size 目标缓冲区大小
 * @param src 源字符串
 * @return 写入后的结束位置
 */
static char* safe_strcat(char* dest, size_t dest_size, const char* src) __attribute__((unused));
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
 * @param manager 请求配置
 * @return 缓存键字符串（需调用者释放），失败返回 NULL
 */
static char* make_cache_key(const llm_request_config_t* manager) {
    if (!manager || !manager->model) {
        return NULL;
    }
    
    /* 计算所需缓冲区大小 */
    size_t len = strlen(manager->model) + 2;
    for (size_t i = 0; i < manager->message_count; ++i) {
        const char* role = manager->messages[i].role ? manager->messages[i].role : "";
        const char* content = manager->messages[i].content ? manager->messages[i].content : "";
        len += strlen(role) + 1 +
               strlen(content) + 1;
    }
    
    char* key = (char*)malloc(len);
    if (!key) return NULL;
    
    /* 构建缓存键 */
    char* p = key;
    
    /* 使用安全的字符串复制 */
    size_t written = snprintf(p, len, "%s", manager->model);
    p += written;
    
    *p++ = '|';
    
    for (size_t i = 0; i < manager->message_count; ++i) {
        const char* role = manager->messages[i].role ? manager->messages[i].role : "";
        const char* content = manager->messages[i].content ? manager->messages[i].content : "";
        written = snprintf(p, len - (p - key), "%s:%s|",
                          role, content);
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
    base_cfg.cache_capacity = AGENTOS_DEFAULT_CACHE_CAPACITY;
    base_cfg.cache_ttl_sec = AGENTOS_DEFAULT_CACHE_TTL_SEC;
    base_cfg.max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
    base_cfg.timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS;

    /* 解析定价规则（使用 cJSON） */
    if (config_path) {
        FILE* f = fopen(config_path, "rb");
        if (f) {
        fseek(f, 0, SEEK_END);
        long yaml_len = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        if (yaml_len <= 0) {
            fclose(f);
            return svc;
        }

        char* yaml_content = (char*)malloc((size_t)yaml_len + 1);
        if (yaml_content) {
            size_t read_len = fread(yaml_content, 1, (size_t)yaml_len, f);
            if (read_len != (size_t)yaml_len) { free(yaml_content); yaml_content = NULL; }
            if (yaml_content) {
            yaml_content[read_len] = '\0';
            
            cJSON* root = cJSON_Parse(yaml_content);
            if (root) {
                int rule_count = 0;
                pricing_rule_t* rules = load_pricing_rules(root, &rule_count);
                if (rules && rule_count > 0) {
                    svc->rules = (void**)rules;
                    svc->rule_count = rule_count;
                    SVC_LOG_INFO("Loaded %d pricing rules", rule_count);
                } else if (rules) {
                    free(rules);
                }
                cJSON_Delete(root);
            } else {
                SVC_LOG_WARN("Failed to parse pricing rules from manager");
            }
            }
            free(yaml_content);
        } else {
            SVC_LOG_ERROR("Failed to allocate memory for manager content");
        }
        fclose(f);
        }
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
    svc->cost = cost_tracker_create((const pricing_rule_t*)svc->rules, (int)svc->rule_count);
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
        free_pricing_rules((pricing_rule_t*)svc->rules, (int)svc->rule_count);
        svc->rules = NULL;
        svc->rule_count = 0;
    }
    
    agentos_mutex_destroy(&svc->lock);
    free(svc);
}

/* ---------- 辅助函数（降低 llm_service_complete 复杂度） ---------- */

/**
 * @brief 从缓存获取响应
 */
static int get_cached_response(llm_service_t* svc,
                               const char* cache_key,
                               llm_response_t** out_response) {
    if (!svc || !cache_key || !out_response) {
        return -1;
    }

    char* cached_json = NULL;
    if (cache_get(svc->cache, cache_key, &cached_json) == 1 && cached_json) {
        llm_response_t* cached_resp = response_from_json(cached_json);
        free(cached_json);
        cached_json = NULL;

        if (cached_resp) {
            *out_response = cached_resp;
            SVC_LOG_DEBUG("Cache hit for key");
            return 1;
        }
        SVC_LOG_WARN("Failed to parse cached response, fetching fresh data");
    }

    return 0;
}

/**
 * @brief 查找提供商
 */
static const provider_t* find_provider(llm_service_t* svc, const char* model) {
    if (!svc || !model) {
        return NULL;
    }

    agentos_mutex_lock(&svc->lock);
    const provider_t* prov = provider_registry_find(svc->registry, model);
    agentos_mutex_unlock(&svc->lock);

    return prov;
}

/**
 * @brief 存储响应到缓存
 */
static void cache_response(llm_service_t* svc, const char* cache_key, llm_response_t* resp) {
    if (!svc || !cache_key || !resp) {
        return;
    }

    char* resp_json = response_to_json(resp);
    if (resp_json) {
        cache_put(svc->cache, cache_key, resp_json);
        free(resp_json);
        resp_json = NULL;
    }
}

/**
 * @brief 更新成本追踪
 */
static void update_cost_tracking(llm_service_t* svc, const char* model, const llm_response_t* resp) {
    if (!svc || !model || !resp) {
        return;
    }

    cost_tracker_add(svc->cost, model, resp->prompt_tokens, resp->completion_tokens);
}

/* ---------- 同步完成（重构后：圈复杂度从 22 降至 9） ---------- */

int llm_service_complete(llm_service_t* svc,
                         const llm_request_config_t* manager,
                         llm_response_t** out_response) {
    /* 参数检查 */
    if (!svc || !manager || !out_response) {
        SVC_LOG_ERROR("Invalid arguments to llm_service_complete");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!manager->model) {
        SVC_LOG_ERROR("Model name is NULL");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 生成缓存键 */
    char* cache_key = make_cache_key(manager);
    if (!cache_key) {
        SVC_LOG_ERROR("Failed to generate cache key");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    /* 检查缓存 */
    llm_response_t* cached_resp = NULL;
    int cache_status = get_cached_response(svc, cache_key, &cached_resp);
    if (cache_status > 0 && cached_resp) {
        free(cache_key);
        *out_response = cached_resp;
        return AGENTOS_OK;
    }

    /* 查找提供商 */
    const provider_t* prov = find_provider(svc, manager->model);
    if (!prov) {
        SVC_LOG_ERROR("No provider for model '%s'", manager->model);
        free(cache_key);
        cache_key = NULL;
        return AGENTOS_ERR_LLM_INVALID_MODEL;
    }

    /* 调用提供商 */
    llm_response_t* resp = NULL;
    int ret = prov->ops->complete(prov->ctx, manager, &resp);
    if (ret != 0) {
        SVC_LOG_ERROR("Provider '%s' failed for model '%s': error %d",
                     prov->name, manager->model, ret);
        free(cache_key);
        cache_key = NULL;
        return ret;
    }

    /* 更新成本追踪和缓存 */
    update_cost_tracking(svc, manager->model, resp);
    cache_response(svc, cache_key, resp);

    *out_response = resp;
    free(cache_key);
    cache_key = NULL;
    return AGENTOS_OK;
}

/* ---------- 流式完成 ---------- */

int llm_service_complete_stream(llm_service_t* svc,
                                const llm_request_config_t* manager,
                                llm_stream_callback_t callback,
                                void* callback_data,
                                llm_response_t** out_response) {
    /* 参数检查 */
    if (!svc || !manager || !callback) {
        SVC_LOG_ERROR("Invalid arguments to llm_service_complete_stream");
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    if (!manager->model) {
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 查找提供商 */
    agentos_mutex_lock(&svc->lock);
    const provider_t* prov = provider_registry_find(svc->registry, manager->model);
    agentos_mutex_unlock(&svc->lock);
    
    if (!prov) {
        SVC_LOG_ERROR("No provider for model '%s'", manager->model);
        return AGENTOS_ERR_LLM_INVALID_MODEL;
    }
    
    /* 检查是否支持流式 */
    if (!prov->ops->complete_stream) {
        SVC_LOG_WARN("Provider '%s' does not support streaming", prov->name);
        return AGENTOS_ERR_NOT_SUPPORTED;
    }

    /* 调用流式接口 */
    int ret = prov->ops->complete_stream(prov->ctx, manager, callback, callback_data, out_response);
    
    if (ret == 0 && out_response && *out_response) {
        llm_response_t* resp = *out_response;
        cost_tracker_add(svc->cost, manager->model,
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

    if (ends_with(config_path, ".yaml") || ends_with(config_path, ".yml")) {
#ifdef HAVE_YAML
        return svc_config_load_yaml(config_path, cfg);
#else
        SVC_LOG_WARN("YAML support not compiled, cannot load '%s'", config_path);
        memset(cfg, 0, sizeof(service_config_t));
        cfg->cache_capacity = AGENTOS_DEFAULT_CACHE_CAPACITY;
        cfg->cache_ttl_sec = AGENTOS_DEFAULT_CACHE_TTL_SEC;
        cfg->max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
        cfg->timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS;
        return 0;
#endif
    }

    memset(cfg, 0, sizeof(service_config_t));
    
    cfg->cache_capacity = AGENTOS_DEFAULT_CACHE_CAPACITY;
    cfg->cache_ttl_sec = AGENTOS_DEFAULT_CACHE_TTL_SEC;
    cfg->max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
    cfg->timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS;
    
    FILE* f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("Cannot open manager file '%s', using defaults", config_path);
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
    if (read_len != (size_t)len) { free(content); fclose(f); return AGENTOS_ERR_IO; }
    content[read_len] = '\0';
    fclose(f);
    
    /* 使用 cJSON 解析（配置文件实际上是 JSON） */
    cJSON* root = cJSON_Parse(content);
    free(content);
    
    if (!root) {
        SVC_LOG_WARN("Failed to parse manager file, using defaults");
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
        size_t enc_len = strlen(item->valuestring);
        if (enc_len < sizeof(cfg->token_encoding)) {
            memcpy((char*)cfg->token_encoding, item->valuestring, enc_len + 1);
        }
    }
    
    cJSON_Delete(root);
    return AGENTOS_OK;
}

#ifdef HAVE_YAML

typedef struct {
    char key[128];
    char value[512];
} yaml_kv_t;

typedef struct {
    yaml_kv_t* pairs;
    size_t count;
    size_t capacity;
} yaml_map_t;

static void yaml_map_init(yaml_map_t* m) {
    m->pairs = NULL;
    m->count = 0;
    m->capacity = 0;
}

static void yaml_map_add(yaml_map_t* m, const char* key, const char* value) {
    if (!key || !value) return;
    if (m->count >= m->capacity) {
        size_t new_cap = m->capacity == 0 ? 16 : m->capacity * 2;
        yaml_kv_t* new_pairs = (yaml_kv_t*)realloc(m->pairs, new_cap * sizeof(yaml_kv_t));
        if (!new_pairs) return;
        m->pairs = new_pairs;
        m->capacity = new_cap;
    }
    strncpy(m->pairs[m->count].key, key, sizeof(m->pairs[m->count].key) - 1);
    m->pairs[m->count].key[sizeof(m->pairs[m->count].key) - 1] = '\0';
    strncpy(m->pairs[m->count].value, value, sizeof(m->pairs[m->count].value) - 1);
    m->pairs[m->count].value[sizeof(m->pairs[m->count].value) - 1] = '\0';
    m->count++;
}

static const char* yaml_map_get(const yaml_map_t* m, const char* key) {
    if (!m || !key) return NULL;
    for (size_t i = 0; i < m->count; ++i) {
        if (strcmp(m->pairs[i].key, key) == 0) return m->pairs[i].value;
    }
    return NULL;
}

static void yaml_map_free(yaml_map_t* m) {
    free(m->pairs);
    m->pairs = NULL;
    m->count = 0;
    m->capacity = 0;
}

int svc_config_load_yaml(const char* config_path, service_config_t* cfg) {
    if (!cfg || !config_path) return AGENTOS_ERR_INVALID_PARAM;

    memset(cfg, 0, sizeof(service_config_t));
    cfg->cache_capacity = AGENTOS_DEFAULT_CACHE_CAPACITY;
    cfg->cache_ttl_sec = AGENTOS_DEFAULT_CACHE_TTL_SEC;
    cfg->max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
    cfg->timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS;

    FILE* f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("Cannot open YAML config '%s', using defaults", config_path);
        return 0;
    }

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        SVC_LOG_WARN("Failed to init YAML parser");
        return 0;
    }
    yaml_parser_set_input_file(&parser, f);

    yaml_event_t event;
    yaml_map_t current_map;
    yaml_map_init(&current_map);
    int in_global = 0;
    int done = 0;

    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            SVC_LOG_WARN("YAML parse error in '%s'", config_path);
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            done = 1;
        } else if (event.type == YAML_SCALAR_EVENT) {
            const char* val = (const char*)event.data.scalar.value;
            if (val && strcmp(val, "global") == 0) {
                in_global = 1;
            } else if (in_global && val) {
                char key_buf[128];
                strncpy(key_buf, val, sizeof(key_buf) - 1);
                key_buf[sizeof(key_buf) - 1] = '\0';

                yaml_event_t val_event;
                if (yaml_parser_parse(&parser, &val_event)) {
                    if (val_event.type == YAML_SCALAR_EVENT) {
                        const char* v = (const char*)val_event.data.scalar.value;
                        if (v) yaml_map_add(&current_map, key_buf, v);
                    }
                    yaml_event_delete(&val_event);
                }
            }
        } else if (event.type == YAML_MAPPING_END_EVENT) {
            in_global = 0;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(f);

    const char* val;
    if ((val = yaml_map_get(&current_map, "cache_capacity"))) {
        cfg->cache_capacity = (size_t)atol(val);
    }
    if ((val = yaml_map_get(&current_map, "cache_ttl_sec"))) {
        cfg->cache_ttl_sec = (uint32_t)atol(val);
    }
    if ((val = yaml_map_get(&current_map, "max_retries"))) {
        cfg->max_retries = atoi(val);
    }
    if ((val = yaml_map_get(&current_map, "timeout_ms"))) {
        cfg->timeout_ms = (uint32_t)atol(val);
    }
    if ((val = yaml_map_get(&current_map, "token_encoding"))) {
        size_t enc_len = strlen(val);
        if (enc_len < sizeof(cfg->token_encoding)) {
            memcpy((char*)cfg->token_encoding, val, enc_len + 1);
        }
    }

    yaml_map_free(&current_map);
    SVC_LOG_INFO("Loaded YAML config from '%s'", config_path);
    return AGENTOS_OK;
}

typedef struct {
    char name[128];
    char provider[64];
    char api_key_env[128];
    char endpoint[512];
    int timeout_sec;
    int max_retries;
} model_entry_t;

int svc_load_model_config_yaml(const char* config_path, provider_config_t** out_providers, size_t* out_count) {
    if (!config_path || !out_providers || !out_count) return AGENTOS_ERR_INVALID_PARAM;

    *out_providers = NULL;
    *out_count = 0;

    FILE* f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("Cannot open model config '%s'", config_path);
        return 0;
    }

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        SVC_LOG_WARN("Failed to init YAML parser for model config");
        return 0;
    }
    yaml_parser_set_input_file(&parser, f);

    yaml_event_t event;
    model_entry_t models[64];
    size_t model_count = 0;
    int in_models = 0;
    int in_model_item = 0;
    yaml_map_t item_map;
    yaml_map_init(&item_map);
    int done = 0;

    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) break;
        if (event.type == YAML_STREAM_END_EVENT) {
            done = 1;
        } else if (event.type == YAML_SCALAR_EVENT) {
            const char* val = (const char*)event.data.scalar.value;
            if (val && strcmp(val, "models") == 0 && !in_models) {
                in_models = 1;
                continue;
            }
            if (in_models && val) {
                char key_buf[128];
                strncpy(key_buf, val, sizeof(key_buf) - 1);
                key_buf[sizeof(key_buf) - 1] = '\0';

                yaml_event_t val_event;
                if (yaml_parser_parse(&parser, &val_event)) {
                    if (val_event.type == YAML_SCALAR_EVENT) {
                        const char* v = (const char*)val_event.data.scalar.value;
                        if (v) yaml_map_add(&item_map, key_buf, v);
                    }
                    yaml_event_delete(&val_event);
                }
            }
        } else if (event.type == YAML_MAPPING_START_EVENT && in_models) {
            in_model_item++;
            if (in_model_item == 1) {
                yaml_map_free(&item_map);
                yaml_map_init(&item_map);
            }
        } else if (event.type == YAML_MAPPING_END_EVENT && in_models) {
            in_model_item--;
            if (in_model_item == 0 && model_count < 64) {
                const char* n = yaml_map_get(&item_map, "name");
                const char* p = yaml_map_get(&item_map, "provider");
                const char* e = yaml_map_get(&item_map, "api_key_env");
                const char* ep = yaml_map_get(&item_map, "endpoint");
                const char* t = yaml_map_get(&item_map, "timeout_sec");
                const char* r = yaml_map_get(&item_map, "max_retries");

                if (n && p) {
                    memset(&models[model_count], 0, sizeof(model_entry_t));
                    strncpy(models[model_count].name, n, sizeof(models[model_count].name) - 1);
                    strncpy(models[model_count].provider, p, sizeof(models[model_count].provider) - 1);
                    if (e) strncpy(models[model_count].api_key_env, e, sizeof(models[model_count].api_key_env) - 1);
                    if (ep) strncpy(models[model_count].endpoint, ep, sizeof(models[model_count].endpoint) - 1);
                    if (t) models[model_count].timeout_sec = atoi(t);
                    if (r) models[model_count].max_retries = atoi(r);
                    model_count++;
                }
            }
        } else if (event.type == YAML_SEQUENCE_END_EVENT && in_models) {
            in_models = 0;
        }
        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(f);
    yaml_map_free(&item_map);

    if (model_count == 0) {
        SVC_LOG_WARN("No models found in '%s'", config_path);
        return 0;
    }

    typedef struct {
        char name[64];
        char api_key_env[128];
        char base_url[512];
        int timeout_sec;
        int max_retries;
        char* model_names[64];
        size_t model_count;
    } provider_agg_t;

    provider_agg_t provs[16];
    size_t prov_count = 0;

    for (size_t i = 0; i < model_count; ++i) {
        size_t j = 0;
        for (; j < prov_count; ++j) {
            if (strcmp(provs[j].name, models[i].provider) == 0) break;
        }
        if (j == prov_count) {
            if (prov_count >= 16) break;
            memset(&provs[prov_count], 0, sizeof(provider_agg_t));
            strncpy(provs[prov_count].name, models[i].provider, sizeof(provs[prov_count].name) - 1);
            if (models[i].api_key_env[0])
                strncpy(provs[prov_count].api_key_env, models[i].api_key_env, sizeof(provs[prov_count].api_key_env) - 1);
            prov_count++;
        }
        if (provs[j].model_count < 64) {
            provs[j].model_names[provs[j].model_count++] = strdup(models[i].name);
        }
        if (!provs[j].base_url[0] && models[i].endpoint[0]) {
            strncpy(provs[j].base_url, models[i].endpoint, sizeof(provs[j].base_url) - 1);
        }
        if (models[i].timeout_sec > provs[j].timeout_sec)
            provs[j].timeout_sec = models[i].timeout_sec;
        if (models[i].max_retries > provs[j].max_retries)
            provs[j].max_retries = models[i].max_retries;
    }

    provider_config_t* result = (provider_config_t*)calloc(prov_count + 1, sizeof(provider_config_t));
    if (!result) return AGENTOS_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < prov_count; ++i) {
        result[i].name = strdup(provs[i].name);
        if (provs[i].api_key_env[0]) {
            char env_prefix[8] = "env:";
            size_t env_key_len = strlen(provs[i].api_key_env);
            char* key_buf = (char*)malloc(4 + env_key_len + 1);
            if (key_buf) {
                memcpy(key_buf, env_prefix, 4);
                memcpy(key_buf + 4, provs[i].api_key_env, env_key_len + 1);
                result[i].api_key = key_buf;
            }
        }
        if (provs[i].base_url[0]) result[i].api_base = strdup(provs[i].base_url);
        result[i].timeout_sec = (double)provs[i].timeout_sec;
        result[i].max_retries = provs[i].max_retries;
        if (provs[i].model_count > 0) {
            char** marr = (char**)calloc(provs[i].model_count + 1, sizeof(char*));
            if (marr) {
                for (size_t k = 0; k < provs[i].model_count; ++k)
                    marr[k] = provs[i].model_names[k];
                marr[provs[i].model_count] = NULL;
            }
            result[i].models = marr;
        }
    }

    *out_providers = result;
    *out_count = prov_count;
    SVC_LOG_INFO("Loaded %zu providers from YAML model config '%s'", prov_count, config_path);
    return AGENTOS_OK;
}

#endif /* HAVE_YAML */

static int svc_load_model_config_json(const char* config_path, provider_config_t** out_providers, size_t* out_count) {
    if (!config_path || !out_providers || !out_count) return AGENTOS_ERR_INVALID_PARAM;

    *out_providers = NULL;
    *out_count = 0;

    FILE* f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("Cannot open model config '%s'", config_path);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = (char*)malloc((size_t)len + 1);
    if (!content) { fclose(f); return AGENTOS_ERR_OUT_OF_MEMORY; }

    size_t read_len = fread(content, 1, (size_t)len, f);
    content[read_len] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(content);
    free(content);
    if (!root) {
        SVC_LOG_WARN("Failed to parse model config JSON");
        return 0;
    }

    cJSON* providers_arr = cJSON_GetObjectItem(root, "providers");
    if (!providers_arr || !cJSON_IsArray(providers_arr)) {
        cJSON_Delete(root);
        SVC_LOG_WARN("No 'providers' array in model config");
        return 0;
    }

    int n = cJSON_GetArraySize(providers_arr);
    if (n <= 0) { cJSON_Delete(root); return 0; }

    provider_config_t* result = (provider_config_t*)calloc((size_t)n + 1, sizeof(provider_config_t));
    if (!result) { cJSON_Delete(root); return AGENTOS_ERR_OUT_OF_MEMORY; }

    size_t valid_count = 0;
    for (int i = 0; i < n; ++i) {
        cJSON* pitem = cJSON_GetArrayItem(providers_arr, i);
        cJSON* pname = cJSON_GetObjectItem(pitem, "name");
        cJSON* pkey_env = cJSON_GetObjectItem(pitem, "api_key_env");
        cJSON* pbase = cJSON_GetObjectItem(pitem, "base_url");
        cJSON* ptimeout = cJSON_GetObjectItem(pitem, "timeout_sec");
        cJSON* pretries = cJSON_GetObjectItem(pitem, "max_retries");
        cJSON* pmodels = cJSON_GetObjectItem(pitem, "models");

        if (!cJSON_IsString(pname)) continue;

        provider_config_t* pcfg = &result[valid_count];
        pcfg->name = strdup(pname->valuestring);

        if (cJSON_IsString(pkey_env) && pkey_env->valuestring[0]) {
            size_t env_len = strlen(pkey_env->valuestring);
            char* key_buf = (char*)malloc(4 + env_len + 1);
            if (key_buf) {
                memcpy(key_buf, "env:", 4);
                memcpy(key_buf + 4, pkey_env->valuestring, env_len + 1);
                pcfg->api_key = key_buf;
            }
        }

        if (cJSON_IsString(pbase)) pcfg->api_base = strdup(pbase->valuestring);
        if (cJSON_IsNumber(ptimeout)) pcfg->timeout_sec = ptimeout->valuedouble;
        if (cJSON_IsNumber(pretries)) pcfg->max_retries = pretries->valueint;

        if (cJSON_IsArray(pmodels)) {
            int mcount = cJSON_GetArraySize(pmodels);
            char** marr = (char**)calloc((size_t)mcount + 1, sizeof(char*));
            if (marr) {
                for (int j = 0; j < mcount; ++j) {
                    cJSON* mitem = cJSON_GetArrayItem(pmodels, j);
                    if (cJSON_IsString(mitem)) marr[j] = strdup(mitem->valuestring);
                }
                marr[mcount] = NULL;
                pcfg->models = marr;
            }
        }
        valid_count++;
    }

    cJSON_Delete(root);
    *out_providers = result;
    *out_count = valid_count;
    SVC_LOG_INFO("Loaded %zu providers from JSON model config '%s'", valid_count, config_path);
    return AGENTOS_OK;
}

int svc_load_model_config(const char* config_path, provider_config_t** out_providers, size_t* out_count) {
    if (!config_path || !out_providers || !out_count) return AGENTOS_ERR_INVALID_PARAM;

    if (ends_with(config_path, ".yaml") || ends_with(config_path, ".yml")) {
#ifdef HAVE_YAML
        return svc_load_model_config_yaml(config_path, out_providers, out_count);
#else
        SVC_LOG_ERROR("YAML support not compiled, cannot load '%s'", config_path);
        return AGENTOS_ERR_NOT_SUPPORTED;
#endif
    }
    return svc_load_model_config_json(config_path, out_providers, out_count);
}

void llm_response_free(llm_response_t* resp) {
    if (!resp) return;
    free(resp->id);
    free(resp->model);
    free(resp->finish_reason);
    if (resp->choices) {
        for (size_t i = 0; i < resp->choice_count; i++) {
            free((void*)resp->choices[i].role);
            free((void*)resp->choices[i].content);
        }
        free(resp->choices);
    }
    free(resp);
}
