#include "memory_compat.h"
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

#include "daemon_defaults.h"
#include "error.h"
#include "platform.h"
#include "response.h"
#include "router/llm_router.h"
#include "service.h"
#include "svc_logger.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_YAML
#include <yaml.h>
#endif

static int ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return 0;
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
static char *safe_strcat(char *dest, size_t dest_size, const char *src) __attribute__((unused));
static char *safe_strcat(char *dest, size_t dest_size, const char *src)
{
    size_t dest_len = strlen(dest);
    size_t remaining = dest_size - dest_len - 1;
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < remaining) ? src_len : remaining;

    if (copy_len > 0) {
        __builtin_memcpy(dest + dest_len, src, copy_len);
        dest[dest_len + copy_len] = '\0';
    }

    return dest + dest_len + copy_len;
}

/**
 * @brief 生成缓存键
 * @param manager 请求配置
 * @return 缓存键字符串（需调用者释放），失败返回 NULL
 */
static char *make_cache_key(const llm_request_config_t *manager)
{
    if (!manager || !manager->model) {
        SVC_LOG_ERROR("make_cache_key: NULL parameter (manager=%p, model=%p)", (const void *)manager, manager ? (const void *)manager->model : NULL);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    /* 计算所需缓冲区大小 */
    size_t len = strlen(manager->model) + 2;
    for (size_t i = 0; i < manager->message_count; ++i) {
        const char *role = manager->messages[i].role ? manager->messages[i].role : "";
        const char *content = manager->messages[i].content ? manager->messages[i].content : "";
        len += strlen(role) + 1 + strlen(content) + 1;
    }

    char *key = (char *)AGENTOS_MALLOC(len);
    if (!key) {
        SVC_LOG_ERROR("make_cache_key: malloc failed for cache key (len=%zu)", len);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    /* 构建缓存键 */
    char *p = key;

    /* 使用安全的字符串复制 */
    size_t pos = 0;
    size_t written = (size_t)snprintf(p, len, "%s", manager->model);
    if (written < len) {
        pos = written;
    } else {
        pos = len > 0 ? len - 1 : 0;
    }
    p[pos] = '|';
    pos++;

    for (size_t i = 0; i < manager->message_count; ++i) {
        const char *role = manager->messages[i].role ? manager->messages[i].role : "";
        const char *content = manager->messages[i].content ? manager->messages[i].content : "";
        size_t remaining = (pos < len) ? (len - pos) : 0;
        written = (size_t)snprintf(p + pos, remaining, "%s:%s|", role, content);
        if (written < remaining) {
            pos += written;
        } else {
            pos = len > 0 ? len - 1 : 0;
            break;
        }
    }

    /* 确保字符串以 null 结尾 */
    if (pos > 0 && key[pos - 1] == '|') {
        key[pos - 1] = '\0';
    } else if (pos < len) {
        key[pos] = '\0';
    } else {
        key[len - 1] = '\0';
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
static pricing_rule_t *load_pricing_rules(cJSON *root, int *count)
{
    if (!root || !count) {
        SVC_LOG_ERROR("load_pricing_rules: NULL parameter (root=%p, count=%p)", (const void *)root, (const void *)count);
        *count = 0;
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    cJSON *pricing = cJSON_GetObjectItem(root, "pricing");
    if (!pricing || !cJSON_IsArray(pricing)) {
        SVC_LOG_ERROR("load_pricing_rules: pricing array missing or not an array in config");
        *count = 0;
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    int n = cJSON_GetArraySize(pricing);
    pricing_rule_t *rules = (pricing_rule_t *)AGENTOS_CALLOC((size_t)n, sizeof(pricing_rule_t));
    if (!rules) {
        SVC_LOG_ERROR("load_pricing_rules: calloc failed for %d pricing rules", n);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    for (int i = 0; i < n; ++i) {
        cJSON *item = cJSON_GetArrayItem(pricing, i);
        cJSON *pattern = cJSON_GetObjectItem(item, "pattern");
        cJSON *input = cJSON_GetObjectItem(item, "input_price_per_k");
        cJSON *output = cJSON_GetObjectItem(item, "output_price_per_k");

        if (cJSON_IsString(pattern) && cJSON_IsNumber(input) && cJSON_IsNumber(output)) {
            rules[i].model_pattern = AGENTOS_STRDUP(pattern->valuestring);
            if (!rules[i].model_pattern) {
                /* 内存分配失败，清理已分配的 */
                SVC_LOG_ERROR("load_pricing_rules: strdup failed for model_pattern at index %d", i);
                for (int j = 0; j < i; ++j) {
                    AGENTOS_FREE((void *)rules[j].model_pattern);
                }
                AGENTOS_FREE(rules);
                *count = 0;
                AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
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

static void free_pricing_rules(pricing_rule_t *rules, int count)
{
    if (!rules)
        return;
    for (int i = 0; i < count; ++i) {
        AGENTOS_FREE((void *)rules[i].model_pattern);
    }
    AGENTOS_FREE(rules);
}

/* ---------- P3.16 (ACC-DT17): llm_router 端点注册辅助 ---------- */

/* 默认模型元数据 — 与 llm_router_init 的 cost_tracker 默认定价规则保持一致。
 * 用于在端点注册时填充 cost/latency/caps 字段；未来版本可通过配置文件覆盖。 */
typedef struct {
    const char *prefix;             /* 模型名前缀匹配（大小写敏感） */
    double cost_per_1k_input;
    double cost_per_1k_output;
    uint32_t avg_latency_ms;
    uint32_t capabilities;
} model_default_meta_t;

static const model_default_meta_t MODEL_DEFAULT_META[] = {
    {"gpt-4",    0.03,    0.06,    1200, LLM_CAP_CHAT | LLM_CAP_COMPLETION | LLM_CAP_STREAMING | LLM_CAP_FUNCTION_CALL},
    {"gpt-3.5",  0.001,   0.002,   1000, LLM_CAP_CHAT | LLM_CAP_COMPLETION | LLM_CAP_STREAMING},
    {"claude",   0.015,   0.075,   1100, LLM_CAP_CHAT | LLM_CAP_COMPLETION | LLM_CAP_STREAMING | LLM_CAP_FUNCTION_CALL},
    {"deepseek", 0.00014, 0.00028,  900, LLM_CAP_CHAT | LLM_CAP_COMPLETION | LLM_CAP_STREAMING | LLM_CAP_FUNCTION_CALL},
    {"gemini",   0.0005,  0.0015,  1000, LLM_CAP_CHAT | LLM_CAP_COMPLETION | LLM_CAP_STREAMING | LLM_CAP_VISION},
};

static const model_default_meta_t *lookup_model_meta(const char *model_name)
{
    if (!model_name)
        return NULL;
    for (size_t i = 0; i < sizeof(MODEL_DEFAULT_META) / sizeof(MODEL_DEFAULT_META[0]); i++) {
        size_t plen = strlen(MODEL_DEFAULT_META[i].prefix);
        if (strncmp(model_name, MODEL_DEFAULT_META[i].prefix, plen) == 0) {
            return &MODEL_DEFAULT_META[i];
        }
    }
    return NULL;
}

/* provider_registry_enumerate 回调：将每个 (provider, model) 注册为 router 端点 */
static int register_endpoint_cb(const char *provider_name, const char *model_name,
                                void *user_data)
{
    int *registered_count = (int *)user_data;

    llm_endpoint_t ep;
    __builtin_memset(&ep, 0, sizeof(ep));
    snprintf(ep.provider_name, sizeof(ep.provider_name), "%s", provider_name);
    snprintf(ep.model_name, sizeof(ep.model_name), "%s", model_name);
    /* endpoint/api_key_env 留空：实际凭证由 provider ctx 内部持有，路由器无需感知 */
    ep.enabled = true;
    ep.priority = 0;
    ep.context_window = 8192; /* 保守默认；未来可按模型精确化 */

    const model_default_meta_t *meta = lookup_model_meta(model_name);
    if (meta) {
        ep.cost_per_1k_input  = meta->cost_per_1k_input;
        ep.cost_per_1k_output = meta->cost_per_1k_output;
        ep.avg_latency_ms     = meta->avg_latency_ms;
        ep.capabilities       = meta->capabilities;
    } else {
        /* 未知模型：保守默认值，确保仍可被路由（非桩，真实可路由端点） */
        ep.cost_per_1k_input  = 0.001;
        ep.cost_per_1k_output = 0.002;
        ep.avg_latency_ms     = 1000;
        ep.capabilities       = LLM_CAP_CHAT | LLM_CAP_COMPLETION | LLM_CAP_STREAMING;
    }

    int rc = llm_router_register_endpoint(&ep);
    if (rc == 0) {
        (*registered_count)++;
    } else {
        SVC_LOG_WARN("C-L02: SVC: router endpoint register FAILED for %s/%s (rc=%d)",
                     provider_name, model_name, rc);
    }
    return 0; /* 继续枚举所有端点 */
}

/* 将 registry 中所有 provider/model 注册为 llm_router 端点 */
static void register_router_endpoints(llm_service_t *svc)
{
    int registered_count = 0;
    provider_registry_enumerate(svc->registry, register_endpoint_cb, &registered_count);
    SVC_LOG_INFO("C-L02: SVC: registered %d endpoints into llm_router", registered_count);
}

/* ---------- 创建服务 ---------- */

llm_service_t *llm_service_create(const char *config_path)
{
    llm_service_t *svc = (llm_service_t *)AGENTOS_CALLOC(1, sizeof(llm_service_t));
    if (!svc) {
        SVC_LOG_ERROR("C-L02: SVC: CREATE-FAIL allocate service context, STACK: llm_service_create");
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    if (agentos_mutex_init(&svc->lock) != 0) {
        SVC_LOG_ERROR("C-L02: SVC: CREATE-FAIL init lock, STACK: llm_service_create");
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "validation failed");
    }

    /* 加载基础配置 */
    service_config_t base_cfg;
    __builtin_memset(&base_cfg, 0, sizeof(base_cfg));
    base_cfg.llm_cache_capacity = AGENTOS_DEFAULT_CACHE_CAPACITY;
    base_cfg.llm_cache_ttl_sec = AGENTOS_DEFAULT_CACHE_TTL_SEC;
    base_cfg.max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
    base_cfg.timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS;

    /* 解析定价规则（使用 cJSON） */
    if (config_path) {
        FILE *f = fopen(config_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long yaml_len = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (yaml_len <= 0) {
                fclose(f);
            } else {
                char *yaml_content = (char *)AGENTOS_MALLOC((size_t)yaml_len + 1);
                if (yaml_content) {
                    size_t read_len = fread(yaml_content, 1, (size_t)yaml_len, f);
                    if (read_len != (size_t)yaml_len) {
                        AGENTOS_FREE(yaml_content);
                        yaml_content = NULL;
                    }
                    if (yaml_content) {
                        yaml_content[read_len] = '\0';

                        cJSON *root = cJSON_Parse(yaml_content);
                        if (root) {
                            int rule_count = 0;
                            pricing_rule_t *rules = load_pricing_rules(root, &rule_count);
                            if (rules && rule_count > 0) {
                                svc->rules = rules;
                                svc->rule_count = rule_count;
                                SVC_LOG_INFO("Loaded %d pricing rules", rule_count);
                            } else if (rules) {
                                AGENTOS_FREE(rules);
                            }
                            cJSON_Delete(root);
                        } else {
                            SVC_LOG_WARN("Failed to parse pricing rules from manager");
                        }
                    }
                    AGENTOS_FREE(yaml_content);
                } else {
                    SVC_LOG_ERROR("Failed to allocate memory for manager content");
                }
                fclose(f);
            }
        }
    }

    /* 创建提供商注册表 */
    svc->registry = provider_registry_create(&base_cfg);
    if (!svc->registry) {
        SVC_LOG_ERROR("C-L02: SVC: CREATE-FAIL registry, STACK: llm_service_create");
        agentos_mutex_destroy(&svc->lock);
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    /* 创建缓存 */
    svc->cache = llm_cache_create(base_cfg.llm_cache_capacity, base_cfg.llm_cache_ttl_sec);
    if (!svc->cache) {
        SVC_LOG_ERROR("C-L02: SVC: CREATE-FAIL cache, STACK: llm_service_create");
        provider_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    /* 创建成本追踪器 */
    svc->cost = cost_tracker_create((const pricing_rule_t *)svc->rules, (int)svc->rule_count);
    if (!svc->cost) {
        SVC_LOG_ERROR("C-L02: SVC: CREATE-FAIL cost_tracker, STACK: llm_service_create");
        llm_cache_destroy(svc->cache);
        provider_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    /* 创建 Token 计数器 */
    svc->token_counter = token_counter_create(base_cfg.token_encoding);
    if (!svc->token_counter) {
        SVC_LOG_ERROR("C-L02: SVC: CREATE-FAIL token_counter, STACK: llm_service_create");
        cost_tracker_destroy(svc->cost);
        llm_cache_destroy(svc->cache);
        provider_registry_destroy(svc->registry);
        agentos_mutex_destroy(&svc->lock);
        AGENTOS_FREE(svc);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    /* P3.16 (ACC-DT17): 初始化 llm_router 并将 registry 中的 provider/model
     * 注册为路由端点。路由器为全局单例（设计见 llm_router.c），init 幂等。
     * 失败非致命：complete 路径会回退到 find_provider 保持向后兼容。 */
    if (llm_router_init(NULL) != 0) {
        SVC_LOG_WARN("C-L02: SVC: llm_router_init failed — routing disabled, falling back to find_provider");
    } else {
        register_router_endpoints(svc);
    }

    SVC_LOG_INFO("C-L02: SVC: CREATE-OK pricing_rules=%d llm_cache_capacity=%zu cache_ttl=%u",
                 (int)svc->rule_count, base_cfg.llm_cache_capacity, base_cfg.llm_cache_ttl_sec);
    return svc;
}

/* ---------- 销毁服务 ---------- */

void llm_service_destroy(llm_service_t *svc)
{
    if (!svc)
        return;

    SVC_LOG_INFO("C-L02: SVC: DESTROY");

    if (svc->registry) {
        provider_registry_destroy(svc->registry);
        svc->registry = NULL;
    }

    if (svc->cache) {
        llm_cache_destroy(svc->cache);
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
        free_pricing_rules((pricing_rule_t *)svc->rules, (int)svc->rule_count);
        svc->rules = NULL;
        svc->rule_count = 0;
    }

    /* P3.16 (ACC-DT17): 销毁全局路由器单例。
     * 注意：router 为进程级全局单例，此处假设 llm_service 在 daemon 中为单实例
     * （与 create 路径的 llm_router_init 配对）。llm_router_init 幂等，重新创建
     * 服务时可再次初始化。每个测试为独立可执行进程，无跨实例全局状态污染。 */
    llm_router_destroy();

    agentos_mutex_destroy(&svc->lock);
    AGENTOS_FREE(svc);
}

/* ---------- 辅助函数（降低 llm_service_complete 复杂度） ---------- */

/**
 * @brief 复杂度评估等级（BAN-133 编码契约）
 */
typedef enum {
    LLM_COMPLEXITY_SIMPLE   = 0,
    LLM_COMPLEXITY_MODERATE = 1,
    LLM_COMPLEXITY_COMPLEX  = 2
} llm_complexity_level_t;

/**
 * @brief 基于输入文本评估复杂度（BAN-133: SIMPLE/MODERATE/COMPLEX）
 *
 * 评分规则:
 *   - 输入长度 >500 字符 +2分
 *   - 输入长度 >100 字符 +1分
 *   - 含架构/设计/系统级关键词 +1分
 *   - 含多步骤标志 +2分
 *   - 含代码生成标志 +1分
 *
 * 路由:
 *   - SIMPLE   (0-1分):  gpt-4o-mini
 *   - MODERATE (2-4分):  gpt-4o
 *   - COMPLEX  (5+分):   claude-sonnet
 */
static llm_complexity_level_t assess_complexity(const char *input)
{
    if (!input) return LLM_COMPLEXITY_SIMPLE;

    size_t len = strlen(input);
    int score = 0;

    /* 输入长度评分 */
    if (len > 500) score += 2;
    else if (len > 100) score += 1;

    /* 架构/设计/系统级关键词 */
    const char *complex_kw[] = {
        "architecture", "distributed", "system design", "scalability",
        "架构", "分布式", "系统设计", "高可用", "微服务", "重构"
    };
    for (size_t i = 0; i < sizeof(complex_kw) / sizeof(complex_kw[0]); i++) {
        if (strstr(input, complex_kw[i])) { score += 1; break; }
    }

    /* 多步骤标志 */
    const char *multi_step_kw[] = {
        "first", "then", "finally", "step 1", "step 2",
        "首先", "然后", "最后", "第一步", "第二步"
    };
    for (size_t i = 0; i < sizeof(multi_step_kw) / sizeof(multi_step_kw[0]); i++) {
        if (strstr(input, multi_step_kw[i])) { score += 2; break; }
    }

    /* 代码生成标志 */
    const char *code_kw[] = {
        "function", "algorithm", "implement", "write a",
        "函数", "算法", "实现", "编写", "代码"
    };
    for (size_t i = 0; i < sizeof(code_kw) / sizeof(code_kw[0]); i++) {
        if (strstr(input, code_kw[i])) { score += 1; break; }
    }

    if (score >= 5) return LLM_COMPLEXITY_COMPLEX;
    if (score >= 2) return LLM_COMPLEXITY_MODERATE;
    return LLM_COMPLEXITY_SIMPLE;
}

/**
 * @brief 根据复杂度选择默认模型（BAN-133 编码契约）
 */
static __attribute__((unused)) const char *route_by_complexity(llm_complexity_level_t level)
{
    switch (level) {
    case LLM_COMPLEXITY_SIMPLE:
        return "gpt-4o-mini";
    case LLM_COMPLEXITY_MODERATE:
        return "gpt-4o";
    case LLM_COMPLEXITY_COMPLEX:
        return "claude-sonnet";
    default:
        return "gpt-4o-mini";
    }
}

/**
 * @brief 记录路由决策审计日志（BAN-137 编码契约）
 */
static void log_routing_decision(const char *model, llm_complexity_level_t complexity,
                                 size_t input_len, const char *reason)
{
    const char *complexity_names[] = {"SIMPLE", "MODERATE", "COMPLEX"};
    SVC_LOG_INFO("[ROUTING] model=%s complexity=%s input_len=%zu reason=%s",
                 model ? model : "unknown",
                 complexity_names[complexity],
                 input_len,
                 reason ? reason : "default");
}

/**
 * @brief 从缓存获取响应
 */
static int get_cached_response(llm_service_t *svc, const char *cache_key,
                               llm_response_t **out_response)
{
    if (!svc || !cache_key || !out_response) {
        SVC_LOG_ERROR("get_cached_response: NULL parameter (svc=%p, cache_key=%p, out_response=%p)", (const void *)svc, (const void *)cache_key, (const void *)out_response);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char *cached_json = NULL;
    if (llm_cache_get(svc->cache, cache_key, &cached_json) == 1 && cached_json) {
        llm_response_t *cached_resp = response_from_json(cached_json);
        AGENTOS_FREE(cached_json);
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
static const provider_t *find_provider(llm_service_t *svc, const char *model)
{
    if (!svc || !model) {
        SVC_LOG_ERROR("find_provider: NULL parameter (svc=%p, model=%p)", (const void *)svc, (const void *)model);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    agentos_mutex_lock(&svc->lock);
    const provider_t *prov = provider_registry_find(svc->registry, model);
    agentos_mutex_unlock(&svc->lock);

    return prov;
}

/* ---------- P3.16 (ACC-DT17): 通过 llm_router 选择提供商 ----------
 *
 * 优先使用策略路由（默认 LLM_ROUTE_COST）选择最合适的端点，再用路由结果的
 * model_name 经 registry 解析为真实 provider_t*。路由失败（如未初始化、无符合
 * 能力的端点、registry 为空）时返回 NULL，由调用方回退到 find_provider(model)
 * 保持向后兼容。 */
static const provider_t *select_provider_via_router(llm_service_t *svc,
                                                    const llm_request_config_t *manager,
                                                    bool is_stream)
{
    if (!svc || !manager) {
        return NULL;
    }

    llm_route_request_t req;
    __builtin_memset(&req, 0, sizeof(req));

    /* 从首条消息提取 prompt 用于 token 估算（路由器内部用其估算成本） */
    if (manager->message_count > 0 && manager->messages &&
        manager->messages[0].content) {
        req.prompt = manager->messages[0].content;
        req.prompt_len = strlen(req.prompt);
    } else {
        req.prompt = "";
        req.prompt_len = 0;
    }

    /* 必需能力：CHAT + COMPLETION；流式额外要求 STREAMING */
    req.required_caps = LLM_CAP_CHAT | LLM_CAP_COMPLETION;
    if (is_stream) {
        req.required_caps |= LLM_CAP_STREAMING;
    }

    req.max_tokens = (manager->max_tokens > 0) ? (uint32_t)manager->max_tokens : 0;
    req.max_cost = 0;          /* 不限成本 */
    req.max_latency_ms = 0;    /* 不限延迟 */
    req.strategy = LLM_ROUTE_COST;
    req.preferred_provider[0] = '\0';  /* 自动选择，让策略生效 */

    llm_route_result_t result;
    __builtin_memset(&result, 0, sizeof(result));
    int rc = llm_router_route(&req, &result);
    if (rc != 0) {
        SVC_LOG_DEBUG("C-L02: SVC: router_route rc=%d — will fall back to find_provider(%s)",
                      rc, manager->model ? manager->model : "NULL");
        return NULL;
    }

    /* 用路由结果的 model_name 经 registry 解析为真实 provider。
     * 若路由选出的模型在 registry 中不存在（理论上不应发生，因端点源自 registry），
     * 返回 NULL 触发调用方回退。 */
    const provider_t *prov = find_provider(svc, result.model_name);
    if (prov) {
        SVC_LOG_INFO("C-L02: SVC: ROUTED provider=%s model=%s strategy=%d cost=%.6f latency=%u",
                     result.provider_name, result.model_name, (int)result.strategy_used,
                     result.estimated_cost, result.estimated_latency_ms);
    }
    return prov;
}

/**
 * @brief 存储响应到缓存
 */
static void cache_response(llm_service_t *svc, const char *cache_key, llm_response_t *resp)
{
    if (!svc || !cache_key || !resp) {
        return;
    }

    char *resp_json = response_to_json(resp);
    if (resp_json) {
        llm_cache_put(svc->cache, cache_key, resp_json);
        AGENTOS_FREE(resp_json);
        resp_json = NULL;
    }
}

/**
 * @brief 更新成本追踪
 */
static void update_cost_tracking(llm_service_t *svc, const char *model, const llm_response_t *resp)
{
    if (!svc || !model || !resp) {
        return;
    }

    cost_tracker_add(svc->cost, model, resp->prompt_tokens, resp->completion_tokens);
}

/* ---------- 同步完成（重构后：圈复杂度从 22 降至 9） ---------- */

int llm_service_complete(llm_service_t *svc, const llm_request_config_t *manager,
                         llm_response_t **out_response)
{
    /* 参数检查 */
    if (!svc || !manager || !out_response) {
        SVC_LOG_ERROR("C-L02: SVC: COMPLETE-FAIL invalid arguments, STACK: llm_service_complete");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!manager->model) {
        SVC_LOG_ERROR("C-L02: SVC: COMPLETE-FAIL model=NULL, STACK: llm_service_complete");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* 生成缓存键 */
    char *cache_key = make_cache_key(manager);
    if (!cache_key) {
        SVC_LOG_ERROR("C-L02: SVC: COMPLETE-FAIL cache_key alloc, STACK: llm_service_complete");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    /* 检查缓存 */
    llm_response_t *cached_resp = NULL;
    int cache_status = get_cached_response(svc, cache_key, &cached_resp);
    if (cache_status > 0 && cached_resp) {
        AGENTOS_FREE(cache_key);
        *out_response = cached_resp;
        return AGENTOS_OK;
    }

    /* P3.16 (ACC-DT17): 优先通过 llm_router 策略路由选择 provider；
     * 路由失败（未初始化/无符合能力端点/registry 为空）时回退到
     * find_provider(manager->model) 保持向后兼容。 */
    const provider_t *prov = select_provider_via_router(svc, manager, false);
    if (!prov) {
        SVC_LOG_INFO("C-L02: SVC: router miss — falling back to find_provider(model=%s)",
                     manager->model);
        prov = find_provider(svc, manager->model);
    }
    if (!prov) {
        SVC_LOG_ERROR("C-L02: SVC: COMPLETE-FAIL model=%s, error=INVALID_MODEL, STACK: llm_service_complete",
                      manager->model);
        AGENTOS_FREE(cache_key);
        cache_key = NULL;
        return AGENTOS_ERR_LLM_INVALID_MODEL;
    }

    /* 审计日志: 记录模型路由决策（BAN-137 编码契约） */
    {
        /* 从消息中提取输入文本用于复杂度评估 */
        const char *first_content = NULL;
        size_t input_len = 0;
        if (manager->message_count > 0 && manager->messages[0].content) {
            first_content = manager->messages[0].content;
            input_len = strlen(first_content);
        }
        llm_complexity_level_t complexity = assess_complexity(first_content);
        log_routing_decision(manager->model, complexity, input_len, "user_specified");
    }

    /* 调用提供商 */
    llm_response_t *resp = NULL;
    int ret = prov->ops->complete(prov->ctx, manager, &resp);
    if (ret != 0) {
        SVC_LOG_ERROR("C-L02: SVC: COMPLETE-FAIL model=%s, error=%d, STACK: llm_service_complete",
                      manager->model, ret);
        AGENTOS_FREE(cache_key);
        cache_key = NULL;
        return ret;
    }

    /* 更新成本追踪和缓存 */
    update_cost_tracking(svc, manager->model, resp);
    cache_response(svc, cache_key, resp);

    *out_response = resp;
    AGENTOS_FREE(cache_key);
    cache_key = NULL;
    return AGENTOS_OK;
}

/* ---------- 流式完成 ---------- */

int llm_service_complete_stream(llm_service_t *svc, const llm_request_config_t *manager,
                                llm_stream_callback_t callback, void *callback_data,
                                llm_response_t **out_response)
{
    /* 参数检查 */
    if (!svc || !manager || !callback) {
        SVC_LOG_ERROR("C-L02: SVC: STREAM-FAIL invalid arguments, STACK: llm_service_complete_stream");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (!manager->model) {
        SVC_LOG_ERROR("C-L02: SVC: STREAM-FAIL model=NULL, STACK: llm_service_complete_stream");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    /* P3.16 (ACC-DT17): 优先通过 llm_router 策略路由选择 provider；
     * 路由失败时回退到 find_provider(manager->model) 保持向后兼容。 */
    const provider_t *prov = select_provider_via_router(svc, manager, true);
    if (!prov) {
        SVC_LOG_INFO("C-L02: SVC: router miss (stream) — falling back to find_provider(model=%s)",
                     manager->model);
        prov = find_provider(svc, manager->model);
    }
    if (!prov) {
        SVC_LOG_ERROR("C-L02: SVC: STREAM-FAIL model=%s, error=INVALID_MODEL, STACK: llm_service_complete_stream",
                      manager->model);
        return AGENTOS_ERR_LLM_INVALID_MODEL;
    }

    /* 审计日志: 记录流式路由决策（BAN-137 编码契约） */
    {
        const char *first_content = NULL;
        size_t input_len = 0;
        if (manager->message_count > 0 && manager->messages[0].content) {
            first_content = manager->messages[0].content;
            input_len = strlen(first_content);
        }
        llm_complexity_level_t complexity = assess_complexity(first_content);
        log_routing_decision(manager->model, complexity, input_len, "stream_user_specified");
    }

    /* 检查是否支持流式 */
    if (!prov->ops->complete_stream) {
        SVC_LOG_ERROR("C-L02: SVC: STREAM-FAIL model=%s, error=NOT_SUPPORTED, STACK: llm_service_complete_stream",
                      manager->model);
        return AGENTOS_ERR_NOT_SUPPORTED;
    }

    /* 调用流式接口 */
    int ret = prov->ops->complete_stream(prov->ctx, manager, callback, callback_data, out_response);

    if (ret == 0 && out_response && *out_response) {
        llm_response_t *resp = *out_response;
        cost_tracker_add(svc->cost, manager->model, resp->prompt_tokens, resp->completion_tokens);
    }

    return ret;
}

/* ---------- 统计 ---------- */

int llm_service_stats(llm_service_t *svc, char **out_json)
{
    if (!svc || !out_json) {
        SVC_LOG_ERROR("llm_service_stats: NULL parameter (svc=%p, out_json=%p)", (const void *)svc, (const void *)out_json);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        SVC_LOG_ERROR("llm_service_stats: cJSON_CreateObject failed");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    cJSON *cost_json = cost_tracker_export(svc->cost);
    if (cost_json) {
        cJSON_AddItemToObject(root, "cost", cost_json);
    }

    cJSON_AddNumberToObject(root, "llm_cache_size", llm_cache_size(svc->cache));
    cJSON_AddNumberToObject(root, "llm_cache_capacity", llm_cache_capacity(svc->cache));

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json) {
        SVC_LOG_ERROR("llm_service_stats: cJSON_PrintUnformatted failed");
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
int svc_config_load(const char *config_path, service_config_t *cfg)
{
    if (!cfg || !config_path) {
        SVC_LOG_ERROR("C-L02: SVC: CONFIG-FAIL NULL parameter, STACK: svc_config_load");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (ends_with(config_path, ".yaml") || ends_with(config_path, ".yml")) {
#ifdef HAVE_YAML
        return svc_config_load_yaml(config_path, cfg);
#else
        SVC_LOG_WARN("C-L02: SVC: CONFIG-WARN YAML not compiled, STACK: svc_config_load");
        __builtin_memset(cfg, 0, sizeof(service_config_t));
        cfg->llm_cache_capacity = AGENTOS_DEFAULT_CACHE_CAPACITY;
        cfg->llm_cache_ttl_sec = AGENTOS_DEFAULT_CACHE_TTL_SEC;
        cfg->max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
        cfg->timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS;
        return 0;
#endif
    }

    __builtin_memset(cfg, 0, sizeof(service_config_t));

    cfg->llm_cache_capacity = AGENTOS_DEFAULT_CACHE_CAPACITY;
    cfg->llm_cache_ttl_sec = AGENTOS_DEFAULT_CACHE_TTL_SEC;
    cfg->max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
    cfg->timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS;

    FILE *f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("C-L02: SVC: CONFIG-WARN cannot open file, STACK: svc_config_load");
        return 0; /* 使用默认值，不算错误 */
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = (char *)AGENTOS_MALLOC((size_t)len + 1);
    if (!content) {
        SVC_LOG_ERROR("C-L02: SVC: CONFIG-FAIL malloc, STACK: svc_config_load");
        fclose(f);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    size_t read_len = fread(content, 1, (size_t)len, f);
    if (read_len != (size_t)len) {
        SVC_LOG_ERROR("C-L02: SVC: CONFIG-FAIL fread, STACK: svc_config_load");
        AGENTOS_FREE(content);
        fclose(f);
        return AGENTOS_ERR_IO;
    }
    content[read_len] = '\0';
    fclose(f);

    /* 使用 cJSON 解析（配置文件实际上是 JSON） */
    cJSON *root = cJSON_Parse(content);
    AGENTOS_FREE(content);

    if (!root) {
        SVC_LOG_WARN("C-L02: SVC: CONFIG-WARN parse failed, STACK: svc_config_load");
        return 0;
    }

    /* 提取配置值 */
    cJSON *item;

    item = cJSON_GetObjectItem(root, "llm_cache_capacity");
    if (item && cJSON_IsNumber(item)) {
        cfg->llm_cache_capacity = item->valueint;
    }

    item = cJSON_GetObjectItem(root, "llm_cache_ttl_sec");
    if (item && cJSON_IsNumber(item)) {
        cfg->llm_cache_ttl_sec = item->valueint;
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
            __builtin_memcpy((char *)cfg->token_encoding, item->valuestring, enc_len + 1);
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
    yaml_kv_t *pairs;
    size_t count;
    size_t capacity;
} yaml_map_t;

static void yaml_map_init(yaml_map_t *m)
{
    m->pairs = NULL;
    m->count = 0;
    m->capacity = 0;
}

static void yaml_map_add(yaml_map_t *m, const char *key, const char *value)
{
    if (!key || !value)
        return;
    if (m->count >= m->capacity) {
        size_t new_cap = m->capacity == 0 ? 16 : m->capacity * 2;
        yaml_kv_t *new_pairs = (yaml_kv_t *)AGENTOS_REALLOC(m->pairs, new_cap * sizeof(yaml_kv_t));
        if (!new_pairs)
            return;
        m->pairs = new_pairs;
        m->capacity = new_cap;
    }
    AGENTOS_STRNCPY_TERM(m->pairs[m->count].key, key, sizeof(m->pairs[m->count].key));
    AGENTOS_STRNCPY_TERM(m->pairs[m->count].value, value, sizeof(m->pairs[m->count].value));
    m->count++;
}

static const char *yaml_map_get(const yaml_map_t *m, const char *key)
{
    if (!m || !key) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }
    for (size_t i = 0; i < m->count; ++i) {
        if (strcmp(m->pairs[i].key, key) == 0)
            return m->pairs[i].value;
    }
    AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
}

static void yaml_map_free(yaml_map_t *m)
{
    AGENTOS_FREE(m->pairs);
    m->pairs = NULL;
    m->count = 0;
    m->capacity = 0;
}

int svc_config_load_yaml(const char *config_path, service_config_t *cfg)
{
    if (!cfg || !config_path)
        return AGENTOS_ERR_INVALID_PARAM;

    __builtin_memset(cfg, 0, sizeof(service_config_t));
    cfg->llm_cache_capacity = AGENTOS_DEFAULT_CACHE_CAPACITY;
    cfg->llm_cache_ttl_sec = AGENTOS_DEFAULT_CACHE_TTL_SEC;
    cfg->max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
    cfg->timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS;

    FILE *f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN cannot open YAML, STACK: svc_config_load_yaml");
        return 0;
    }

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN YAML parser init, STACK: svc_config_load_yaml");
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
            SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN YAML parse error, STACK: svc_config_load_yaml");
            break;
        }
        if (event.type == YAML_STREAM_END_EVENT) {
            done = 1;
        } else if (event.type == YAML_SCALAR_EVENT) {
            const char *val = (const char *)event.data.scalar.value;
            if (val && strcmp(val, "global") == 0) {
                in_global = 1;
            } else if (in_global && val) {
                char key_buf[128];
                AGENTOS_STRNCPY_TERM(key_buf, val, sizeof(key_buf));

                yaml_event_t val_event;
                if (yaml_parser_parse(&parser, &val_event)) {
                    if (val_event.type == YAML_SCALAR_EVENT) {
                        const char *v = (const char *)val_event.data.scalar.value;
                        if (v)
                            yaml_map_add(&current_map, key_buf, v);
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

    const char *val;
    if ((val = yaml_map_get(&current_map, "llm_cache_capacity"))) {
        cfg->llm_cache_capacity = (size_t)atol(val);
    }
    if ((val = yaml_map_get(&current_map, "llm_cache_ttl_sec"))) {
        cfg->llm_cache_ttl_sec = (uint32_t)atol(val);
    }
    if ((val = yaml_map_get(&current_map, "max_retries"))) {
        cfg->max_retries = (int)strtol(val, NULL, 10);
    }
    if ((val = yaml_map_get(&current_map, "timeout_ms"))) {
        cfg->timeout_ms = (uint32_t)atol(val);
    }
    if ((val = yaml_map_get(&current_map, "token_encoding"))) {
        size_t enc_len = strlen(val);
        if (enc_len < sizeof(cfg->token_encoding)) {
            __builtin_memcpy((char *)cfg->token_encoding, val, enc_len + 1);
        }
    }

    yaml_map_free(&current_map);
    SVC_LOG_INFO("C-L02: SVC: MODEL-CONFIG-OK YAML loaded, STACK: svc_config_load_yaml");
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

int svc_load_model_config_yaml(const char *config_path, provider_config_t **out_providers,
                               size_t *out_count)
{
    if (!config_path || !out_providers || !out_count)
        return AGENTOS_ERR_INVALID_PARAM;

    *out_providers = NULL;
    *out_count = 0;

    FILE *f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN cannot open model config, STACK: svc_load_model_config_yaml");
        return 0;
    }

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser)) {
        fclose(f);
        SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN YAML parser init, STACK: svc_load_model_config_yaml");
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
        if (!yaml_parser_parse(&parser, &event))
            break;
        if (event.type == YAML_STREAM_END_EVENT) {
            done = 1;
        } else if (event.type == YAML_SCALAR_EVENT) {
            const char *val = (const char *)event.data.scalar.value;
            if (val && strcmp(val, "models") == 0 && !in_models) {
                in_models = 1;
                yaml_event_delete(&event);
                continue;
            }
            if (in_models && val) {
                char key_buf[128];
                AGENTOS_STRNCPY_TERM(key_buf, val, sizeof(key_buf));

                yaml_event_t val_event;
                if (yaml_parser_parse(&parser, &val_event)) {
                    if (val_event.type == YAML_SCALAR_EVENT) {
                        const char *v = (const char *)val_event.data.scalar.value;
                        if (v)
                            yaml_map_add(&item_map, key_buf, v);
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
                const char *n = yaml_map_get(&item_map, "name");
                const char *p = yaml_map_get(&item_map, "provider");
                const char *e = yaml_map_get(&item_map, "api_key_env");
                const char *ep = yaml_map_get(&item_map, "endpoint");
                const char *t = yaml_map_get(&item_map, "timeout_sec");
                const char *r = yaml_map_get(&item_map, "max_retries");

                if (n && p) {
                    __builtin_memset(&models[model_count], 0, sizeof(model_entry_t));
                    AGENTOS_STRNCPY_TERM(models[model_count].name, n, sizeof(models[model_count].name));
                    AGENTOS_STRNCPY_TERM(models[model_count].provider, p, sizeof(models[model_count].provider));
                    if (e)
                        AGENTOS_STRNCPY_TERM(models[model_count].api_key_env, e, sizeof(models[model_count].api_key_env));
                    if (ep)
                        AGENTOS_STRNCPY_TERM(models[model_count].endpoint, ep, sizeof(models[model_count].endpoint));
                    if (t)
                        models[model_count].timeout_sec = (int)strtol(t, NULL, 10);
                    if (r)
                        models[model_count].max_retries = (int)strtol(r, NULL, 10);
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
        SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN no models found, STACK: svc_load_model_config_yaml");
        return 0;
    }

    typedef struct {
        char name[64];
        char api_key_env[128];
        char base_url[512];
        int timeout_sec;
        int max_retries;
        char *model_names[64];
        size_t model_count;
    } provider_agg_t;

    provider_agg_t provs[16];
    size_t prov_count = 0;

    for (size_t i = 0; i < model_count; ++i) {
        size_t j = 0;
        for (; j < prov_count; ++j) {
            if (strcmp(provs[j].name, models[i].provider) == 0)
                break;
        }
        if (j == prov_count) {
            if (prov_count >= 16)
                break;
            __builtin_memset(&provs[prov_count], 0, sizeof(provider_agg_t));
            AGENTOS_STRNCPY_TERM(provs[prov_count].name, models[i].provider, sizeof(provs[prov_count].name));
            if (models[i].api_key_env[0])
                AGENTOS_STRNCPY_TERM(provs[prov_count].api_key_env, models[i].api_key_env, sizeof(provs[prov_count].api_key_env));
            prov_count++;
        }
        if (provs[j].model_count < 64) {
            provs[j].model_names[provs[j].model_count++] = AGENTOS_STRDUP(models[i].name);
        }
        if (!provs[j].base_url[0] && models[i].endpoint[0]) {
            AGENTOS_STRNCPY_TERM(provs[j].base_url, models[i].endpoint, sizeof(provs[j].base_url));
        }
        if (models[i].timeout_sec > provs[j].timeout_sec)
            provs[j].timeout_sec = models[i].timeout_sec;
        if (models[i].max_retries > provs[j].max_retries)
            provs[j].max_retries = models[i].max_retries;
    }

    provider_config_t *result =
        (provider_config_t *)AGENTOS_CALLOC(prov_count + 1, sizeof(provider_config_t));
    if (!result) {
        for (size_t j = 0; j < prov_count; ++j) {
            for (size_t k = 0; k < provs[j].model_count; ++k)
                AGENTOS_FREE(provs[j].model_names[k]);
        }
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < prov_count; ++i) {
        result[i].name = AGENTOS_STRDUP(provs[i].name);
        if (provs[i].api_key_env[0]) {
            char env_prefix[8] = "env:";
            size_t env_key_len = strlen(provs[i].api_key_env);
            char *key_buf = (char *)AGENTOS_MALLOC(4 + env_key_len + 1);
            if (key_buf) {
                __builtin_memcpy(key_buf, env_prefix, 4);
                __builtin_memcpy(key_buf + 4, provs[i].api_key_env, env_key_len + 1);
                result[i].api_key = key_buf;
            }
        }
        if (provs[i].base_url[0])
            result[i].api_base = AGENTOS_STRDUP(provs[i].base_url);
        result[i].timeout_sec = (double)provs[i].timeout_sec;
        result[i].max_retries = provs[i].max_retries;
        if (provs[i].model_count > 0) {
            char **marr = (char **)AGENTOS_CALLOC(provs[i].model_count + 1, sizeof(char *));
            if (marr) {
                for (size_t k = 0; k < provs[i].model_count; ++k)
                    marr[k] = provs[i].model_names[k];
                marr[provs[i].model_count] = NULL;
            } else {
                for (size_t k = 0; k < provs[i].model_count; ++k)
                    AGENTOS_FREE(provs[i].model_names[k]);
            }
            result[i].models = marr;
        }
    }

    *out_providers = result;
    *out_count = prov_count;
    SVC_LOG_INFO("C-L02: SVC: MODEL-CONFIG-OK YAML providers=%zu, STACK: svc_load_model_config_yaml", prov_count);
    return AGENTOS_OK;
}

#endif /* HAVE_YAML */

static int svc_load_model_config_json(const char *config_path, provider_config_t **out_providers,
                                      size_t *out_count)
{
    if (!config_path || !out_providers || !out_count) {
        SVC_LOG_ERROR("C-L02: SVC: MODEL-CONFIG-FAIL NULL parameter, STACK: svc_load_model_config_json");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    *out_providers = NULL;
    *out_count = 0;

    FILE *f = fopen(config_path, "rb");
    if (!f) {
        SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN cannot open file, STACK: svc_load_model_config_json");
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = (char *)AGENTOS_MALLOC((size_t)len + 1);
    if (!content) {
        SVC_LOG_ERROR("C-L02: SVC: MODEL-CONFIG-FAIL malloc, STACK: svc_load_model_config_json");
        fclose(f);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    size_t read_len = fread(content, 1, (size_t)len, f);
    content[read_len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(content);
    AGENTOS_FREE(content);
    if (!root) {
        SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN parse failed, STACK: svc_load_model_config_json");
        return 0;
    }

    cJSON *providers_arr = cJSON_GetObjectItem(root, "providers");
    if (!providers_arr || !cJSON_IsArray(providers_arr)) {
        cJSON_Delete(root);
        SVC_LOG_WARN("C-L02: SVC: MODEL-CONFIG-WARN no providers, STACK: svc_load_model_config_json");
        return 0;
    }

    int n = cJSON_GetArraySize(providers_arr);
    if (n <= 0) {
        cJSON_Delete(root);
        return 0;
    }

    provider_config_t *result =
        (provider_config_t *)AGENTOS_CALLOC((size_t)n + 1, sizeof(provider_config_t));
    if (!result) {
        SVC_LOG_ERROR("C-L02: SVC: MODEL-CONFIG-FAIL calloc, STACK: svc_load_model_config_json");
        cJSON_Delete(root);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    size_t valid_count = 0;
    for (int i = 0; i < n; ++i) {
        cJSON *pitem = cJSON_GetArrayItem(providers_arr, i);
        cJSON *pname = cJSON_GetObjectItem(pitem, "name");
        cJSON *pkey_env = cJSON_GetObjectItem(pitem, "api_key_env");
        cJSON *pbase = cJSON_GetObjectItem(pitem, "base_url");
        cJSON *ptimeout = cJSON_GetObjectItem(pitem, "timeout_sec");
        cJSON *pretries = cJSON_GetObjectItem(pitem, "max_retries");
        cJSON *pmodels = cJSON_GetObjectItem(pitem, "models");

        if (!cJSON_IsString(pname))
            continue;

        provider_config_t *pcfg = &result[valid_count];
        pcfg->name = AGENTOS_STRDUP(pname->valuestring);

        if (cJSON_IsString(pkey_env) && pkey_env->valuestring[0]) {
            size_t env_len = strlen(pkey_env->valuestring);
            char *key_buf = (char *)AGENTOS_MALLOC(4 + env_len + 1);
            if (key_buf) {
                __builtin_memcpy(key_buf, "env:", 4);
                __builtin_memcpy(key_buf + 4, pkey_env->valuestring, env_len + 1);
                pcfg->api_key = key_buf;
            }
        }

        if (cJSON_IsString(pbase))
            pcfg->api_base = AGENTOS_STRDUP(pbase->valuestring);
        if (cJSON_IsNumber(ptimeout))
            pcfg->timeout_sec = ptimeout->valuedouble;
        if (cJSON_IsNumber(pretries))
            pcfg->max_retries = pretries->valueint;

        if (cJSON_IsArray(pmodels)) {
            int mcount = cJSON_GetArraySize(pmodels);
            char **marr = (char **)AGENTOS_CALLOC((size_t)mcount + 1, sizeof(char *));
            if (marr) {
                for (int j = 0; j < mcount; ++j) {
                    cJSON *mitem = cJSON_GetArrayItem(pmodels, j);
                    if (cJSON_IsString(mitem))
                        marr[j] = AGENTOS_STRDUP(mitem->valuestring);
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
    SVC_LOG_INFO("C-L02: SVC: MODEL-CONFIG-OK providers=%zu, STACK: svc_load_model_config_json", valid_count);
    return AGENTOS_OK;
}

int svc_load_model_config(const char *config_path, provider_config_t **out_providers,
                          size_t *out_count)
{
    if (!config_path || !out_providers || !out_count) {
        SVC_LOG_ERROR("C-L02: SVC: MODEL-CONFIG-FAIL NULL parameter, STACK: svc_load_model_config");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    if (ends_with(config_path, ".yaml") || ends_with(config_path, ".yml")) {
#ifdef HAVE_YAML
        return svc_load_model_config_yaml(config_path, out_providers, out_count);
#else
        SVC_LOG_ERROR("C-L02: SVC: MODEL-CONFIG-FAIL YAML not compiled, STACK: svc_load_model_config");
        return AGENTOS_ERR_NOT_SUPPORTED;
#endif
    }
    return svc_load_model_config_json(config_path, out_providers, out_count);
}

void llm_response_free(llm_response_t *resp)
{
    if (!resp)
        return;
    AGENTOS_FREE(resp->id);
    AGENTOS_FREE(resp->model);
    AGENTOS_FREE(resp->finish_reason);
    if (resp->choices) {
        for (size_t i = 0; i < resp->choice_count; i++) {
            AGENTOS_FREE((void *)resp->choices[i].role);
            AGENTOS_FREE((void *)resp->choices[i].content);
        }
        AGENTOS_FREE(resp->choices);
    }
    AGENTOS_FREE(resp);
}
