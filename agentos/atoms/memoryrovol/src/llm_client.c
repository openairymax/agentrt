/**
 * @file llm_client.c
 * @brief LLM 客户端实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 实现与大语言模型的交互，支持：
 * - OpenAI API 兼容接口
 * - DeepSeek 等国产模型
 * - 本地模型服务
 */

#include "../include/llm_client.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <stdio.h>

/* CURL HTTP 客户端（libcurl 已通过CMake检测并定义 AGENTOS_HAS_CURL=1） */
#include <curl/curl.h>

/* JSON 解析（cJSON 已通过CMake检测并定义 AGENTOS_HAS_CJSON=1） */
#include <cjson/cJSON.h>

/* 双思考系统模块 (DS-004集成) */
#include "../../coreloopthree/src/cognition/thinking_chain.h"
#include "../../coreloopthree/src/cognition/metacognition.h"

/**
 * @brief 内存缓冲区（用于 HTTP 响应）
 */
typedef struct {
    char* data;
    size_t size;
} memory_buffer_t;

/**
 * @brief LLM 服务内部结构
 */
struct agentos_llm_service {
    agentos_llm_config_t config;    /**< 配置 */
    CURL* curl;                     /**< CURL 句柄 */
    int initialized;                /**< 初始化标志 */
};

/**
 * @brief HTTP 响应回调函数
 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    memory_buffer_t* mem = (memory_buffer_t*)userp;

    char* ptr = (char*)AGENTOS_REALLOC(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

/**
 * @brief 创建 LLM 服务
 */
agentos_error_t agentos_llm_service_create(
    const agentos_llm_config_t* config,
    agentos_llm_service_t** out_service)
{
    if (!config || !out_service) {
        AGENTOS_LOG_ERROR("Invalid parameters to llm_service_create");
        return AGENTOS_EINVAL;
    }

    if (!config->api_key || !config->base_url) {
        AGENTOS_LOG_ERROR("API key and base URL are required");
        return AGENTOS_EINVAL;
    }

    agentos_llm_service_t* service = 
        (agentos_llm_service_t*)AGENTOS_CALLOC(1, sizeof(agentos_llm_service_t));
    if (!service) {
        return AGENTOS_ENOMEM;
    }

    /* 复制配置 */
    service->config.model_name = config->model_name ? AGENTOS_STRDUP(config->model_name) : AGENTOS_STRDUP("gpt-3.5-turbo");
    if (!service->config.model_name) {
        AGENTOS_FREE(service);
        return AGENTOS_ENOMEM;
    }
    service->config.api_key = AGENTOS_STRDUP(config->api_key);
    if (!service->config.api_key) {
        AGENTOS_FREE((void*)service->config.model_name);
        AGENTOS_FREE(service);
        return AGENTOS_ENOMEM;
    }
    service->config.base_url = AGENTOS_STRDUP(config->base_url);
    if (!service->config.base_url) {
        AGENTOS_FREE((void*)service->config.api_key);
        AGENTOS_FREE((void*)service->config.model_name);
        AGENTOS_FREE(service);
        return AGENTOS_ENOMEM;
    }
    service->config.timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 30000;
    service->config.temperature = config->temperature > 0 ? config->temperature : 0.7f;
    service->config.max_tokens = config->max_tokens > 0 ? config->max_tokens : 2048;

    /* 初始化 CURL（不支持降级模式，严格遵循SEC-017） */
    CURL* curl = curl_easy_init();
    if (!curl) {
        AGENTOS_LOG_ERROR("CURL initialization failed - LLM service cannot function without HTTP client");
        AGENTOS_FREE(service);
        return AGENTOS_ENOTSUP;
    }

    service->curl = curl;
    service->initialized = 1;
    *out_service = service;

    AGENTOS_LOG_INFO("LLM service created for model: %s", service->config.model_name);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 完整LLM调用接口
 */
agentos_error_t agentos_llm_complete(
    agentos_llm_service_t* service,
    const agentos_llm_request_t* request,
    agentos_llm_response_t** out_response)
{
    if (!service || !request || !out_response) {
        return AGENTOS_EINVAL;
    }

    if (!service->initialized) {
        return AGENTOS_ENOTINIT;
    }

    /* 严格模式：无CURL支持时返回明确错误，不返回假SUCCESS (SEC-017) */
    if (!service->curl) {
        AGENTOS_LOG_ERROR("LLM complete attempted but CURL not available - service in invalid state");
        return AGENTOS_ENOTINIT;
    }

    /* 使用现有的agentos_llm_service_call实现作为基础 */
    char* response_text = NULL;
    agentos_error_t err = agentos_llm_service_call(service, request->prompt, &response_text);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    agentos_llm_response_t* resp = (agentos_llm_response_t*)AGENTOS_CALLOC(1, sizeof(agentos_llm_response_t));
    if (!resp) {
        AGENTOS_FREE(response_text);
        return AGENTOS_ENOMEM;
    }

    resp->text = response_text;
    resp->usage_tokens = strlen(response_text) / 4; /* 近似估算：平均每个token 4个字符 */
    resp->total_tokens = resp->usage_tokens + 100;  /* 粗略估算总token数 */
    resp->finish_reason = 1;  /* 假设正常完成 */

    *out_response = resp;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 释放LLM响应
 */
void agentos_llm_response_free(agentos_llm_response_t* response) {
    if (!response) return;
    
    if (response->text) {
        AGENTOS_FREE(response->text);
    }
    AGENTOS_FREE(response);
}

/**
 * @brief 销毁 LLM 服务
 */
void agentos_llm_service_destroy(agentos_llm_service_t* service) {
    if (!service) return;

    if (service->curl) {
        curl_easy_cleanup(service->curl);
    }

    if (service->config.model_name) AGENTOS_FREE((void*)service->config.model_name);
    if (service->config.api_key) AGENTOS_FREE((void*)service->config.api_key);
    if (service->config.base_url) AGENTOS_FREE((void*)service->config.base_url);

    AGENTOS_FREE(service);
}

/**
 * @brief 调用 LLM 生成响应
 */
int agentos_llm_service_is_available(const agentos_llm_service_t* service) {
    if (!service) return 0;
    if (!service->initialized) return 0;
    if (!service->curl) return 0;
    return 1;
}

agentos_error_t agentos_llm_service_call(
    agentos_llm_service_t* service,
    const char* prompt,
    char** out_response)
{
    if (!service || !prompt || !out_response) {
        return AGENTOS_EINVAL;
    }

    if (!service->initialized) {
        return AGENTOS_ENOTINIT;
    }

    /* 严格模式：无CURL支持时返回明确错误 (SEC-017) */
    if (!service->curl) {
        AGENTOS_LOG_ERROR("LLM service call attempted but CURL not available");
        return AGENTOS_ENOTINIT;
    }

    /* 构建请求 URL */
    char url[512];
    snprintf(url, sizeof(url), "%s/chat/completions", service->config.base_url);

    /* 构建请求 JSON */
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        return AGENTOS_ENOMEM;
    }
    cJSON_AddStringToObject(root, "model", service->config.model_name);
    cJSON_AddNumberToObject(root, "temperature", service->config.temperature);
    cJSON_AddNumberToObject(root, "max_tokens", service->config.max_tokens);

    /* 构建消息数组 */
    cJSON* messages = cJSON_AddArrayToObject(root, "messages");
    cJSON* message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "role", "user");
    cJSON_AddStringToObject(message, "content", prompt);
    cJSON_AddItemToArray(messages, message);

    char* request_json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!request_json) {
        return AGENTOS_ENOMEM;
    }

    /* 准备 HTTP 请求 */
    curl_easy_reset(service->curl);
    curl_easy_setopt(service->curl, CURLOPT_URL, url);
    curl_easy_setopt(service->curl, CURLOPT_POSTFIELDS, request_json);

    /* 设置请求头 */
    struct curl_slist* headers = NULL;
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", service->config.api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(service->curl, CURLOPT_HTTPHEADER, headers);

    /* 设置超时 */
    curl_easy_setopt(service->curl, CURLOPT_TIMEOUT_MS, (long)service->config.timeout_ms);

    /* 准备响应缓冲区 */
    memory_buffer_t response = {0};
    curl_easy_setopt(service->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(service->curl, CURLOPT_WRITEDATA, (void*)&response);

    /* 执行请求 */
    CURLcode res = curl_easy_perform(service->curl);
    curl_slist_free_all(headers);
    AGENTOS_FREE(request_json);

    if (res != CURLE_OK) {
        AGENTOS_LOG_ERROR("LLM service call failed: %s", curl_easy_strerror(res));
        if (response.data) AGENTOS_FREE(response.data);
        return AGENTOS_EUNKNOWN;
    }

    /* 检查 HTTP 状态码 */
    long response_code = 0;
    curl_easy_getinfo(service->curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        AGENTOS_LOG_ERROR("LLM service returned error code: %ld", response_code);
        if (response.data) AGENTOS_FREE(response.data);
        return AGENTOS_EUNKNOWN;
    }

    /* 解析响应 JSON */
    cJSON* response_json = cJSON_Parse(response.data);
    AGENTOS_FREE(response.data);

    if (!response_json) {
        AGENTOS_LOG_ERROR("Failed to parse LLM response");
        return AGENTOS_EUNKNOWN;
    }

    /* 提取响应内容 */
    cJSON* choices = cJSON_GetObjectItemCaseSensitive(response_json, "choices");
    if (!choices || !cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        AGENTOS_LOG_ERROR("No choices in LLM response");
        cJSON_Delete(response_json);
        return AGENTOS_EUNKNOWN;
    }

    cJSON* first_choice = cJSON_GetArrayItem(choices, 0);
    cJSON* message_obj = cJSON_GetObjectItemCaseSensitive(first_choice, "message");
    if (!message_obj) {
        AGENTOS_LOG_ERROR("No message in LLM response");
        cJSON_Delete(response_json);
        return AGENTOS_EUNKNOWN;
    }

    cJSON* content = cJSON_GetObjectItemCaseSensitive(message_obj, "content");
    if (!content || !cJSON_IsString(content)) {
        AGENTOS_LOG_ERROR("No content in LLM response");
        cJSON_Delete(response_json);
        return AGENTOS_EUNKNOWN;
    }

    *out_response = AGENTOS_STRDUP(content->valuestring);
    cJSON_Delete(response_json);

    if (!*out_response) {
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * DS-004: 双思考流式批判循环实现
 * ============================================================================ */

/**
 * @brief S2真实生成器回调 —— 调用LLM服务生成内容
 *
 * 这是连接llm_client和thinking_chain/metacognition的桥梁函数。
 * 作为agentos_mc_apply_correction的corrector_fn参数使用。
 */
static agentos_error_t dual_think_s2_generator(
    const char* input, size_t input_len,
    char** output, size_t* output_len,
    void* user_data)
{
    if (!input || !output || !output_len || !user_data) {
        return AGENTOS_EINVAL;
    }

    agentos_llm_service_t* service = (agentos_llm_service_t*)user_data;
    if (!service->initialized || !service->curl) {
        return AGENTOS_ENOTINIT;
    }

    return agentos_llm_service_call(service, input, output);
}

/**
 * @brief 构建S1验证提示词（让LLM扮演验证者角色）
 */
static agentos_error_t build_s1_verification_prompt(
    agentos_llm_service_t* service,
    const char* original_prompt,
    const char* generated_content,
    const char* critique_context,
    char** out_prompt)
{
    if (!service || !original_prompt || !generated_content || !out_prompt) {
        return AGENTOS_EINVAL;
    }

    size_t buf_size = strlen(original_prompt) + strlen(generated_content) + 1024;
    if (critique_context) buf_size += strlen(critique_context);

    char* buf = (char*)AGENTOS_MALLOC(buf_size);
    if (!buf) return AGENTOS_ENOMEM;

    int written = snprintf(buf, buf_size,
        "You are a critical verifier (S1 role). Evaluate the following generated content "
        "for quality and accuracy.\n\n"
        "[Original Task]\n%s\n\n"
        "[Generated Content to Verify]\n%s\n\n"
        "%s"
        "Respond in JSON format:\n"
        "{\n"
        "  \"is_acceptable\": true/false,\n"
        "  \"confidence\": 0.0-1.0,\n"
        "  \"relevance_score\": 0.0-1.0,\n"
        "  \"accuracy_score\": 0.0-1.0,\n"
        "  \"completeness_score\": 0.0-1.0,\n"
        "  \"consistency_score\": 0.0-1.0,\n"
        "  \"clarity_score\": 0.0-1.0,\n"
        "  \"critique\": \"detailed criticism or empty if acceptable\",\n"
        "  \"suggested_fix\": \"specific improvement suggestion or empty\"\n"
        "}",
        original_prompt,
        generated_content,
        critique_context ? critique_context : "");

    if (written <= 0 || (size_t)written >= buf_size) {
        AGENTOS_FREE(buf);
        return AGENTOS_ENOMEM;
    }

    *out_prompt = buf;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 构建S2修正提示词（基于S1批判意见重新生成）
 */
static agentos_error_t build_s2_correction_prompt(
    const char* original_prompt,
    const char* previous_content,
    const char* critique,
    int correction_num,
    char** out_prompt)
{
    if (!original_prompt || !previous_content || !critique || !out_prompt) {
        return AGENTOS_EINVAL;
    }

    size_t buf_size = strlen(original_prompt) + strlen(previous_content) +
                       strlen(critique) + 512;
    char* buf = (char*)AGENTOS_MALLOC(buf_size);
    if (!buf) return AGENTOS_ENOMEM;

    int written = snprintf(buf, buf_size,
        "[Revision Request #%d]\n\n"
        "Your previous response was reviewed and needs improvement.\n\n"
        "[Original Task]\n%s\n\n"
        "[Your Previous Response]\n%s\n\n"
        "[Reviewer Critique]\n%s\n\n"
        "Please revise your response addressing all points in the critique. "
        "Maintain the same structure but improve quality based on feedback.",
        correction_num, original_prompt, previous_content, critique);

    if (written <= 0 || (size_t)written >= buf_size) {
        AGENTOS_FREE(buf);
        return AGENTOS_ENOMEM;
    }

    *out_prompt = buf;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 解析S1验证响应JSON
 */
static int parse_s1_verification_response(
    const char* response,
    float* out_confidence,
    float* out_scores,
    char** out_critique,
    char** out_suggested_fix)
{
    if (!response) return 0;

    cJSON* root = cJSON_Parse(response);
    if (!root) return 0;

    int is_acceptable = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root, "is_acceptable"));
    float confidence = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "confidence"));

    if (out_confidence) *out_confidence = confidence;

    if (out_scores) {
        out_scores[0] = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "relevance_score"));
        out_scores[1] = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "accuracy_score"));
        out_scores[2] = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "completeness_score"));
        out_scores[3] = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "consistency_score"));
        out_scores[4] = (float)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "clarity_score"));
    }

    if (out_critique) {
        cJSON* crit = cJSON_GetObjectItemCaseSensitive(root, "critique");
        *out_critique = crit && cJSON_IsString(crit) ? AGENTOS_STRDUP(crit->valuestring) : NULL;
    }

    if (out_suggested_fix) {
        cJSON* fix = cJSON_GetObjectItemCaseSensitive(root, "suggested_fix");
        *out_suggested_fix = fix && cJSON_IsString(fix) ? AGENTOS_STRDUP(fix->valuestring) : NULL;
    }

    cJSON_Delete(root);
    return is_acceptable;
}

/**
 * @brief 执行完整双思考推理会话
 */
agentos_error_t agentos_llm_dual_think(
    agentos_llm_service_t* service,
    const agentos_dual_think_config_t* config,
    const char* user_prompt,
    agentos_dual_think_result_t** out_result)
{
    if (!service || !user_prompt || !out_result) {
        return AGENTOS_EINVAL;
    }
    if (!service->initialized || !service->curl) {
        return AGENTOS_ENOTINIT;
    }

    /* 应用默认配置 */
    agentos_dual_think_config_t defaults = {
        .max_corrections = 3,
        .acceptance_threshold = 0.7f,
        .timeout_ms = 30000,
        .enable_context_window = 1,
        .context_window_tokens = 8192,
        .enable_working_memory = 1,
        .wm_capacity = 64,
        .system_prompt = NULL,
        .temperature_s2 = 0.7f,
        .temperature_s1 = 0.3f
    };
    if (!config) config = &defaults;

    /* 分配结果结构 */
    agentos_dual_think_result_t* result =
        (agentos_dual_think_result_t*)AGENTOS_CALLOC(1, sizeof(agentos_dual_think_result_t));
    if (!result) return AGENTOS_ENOMEM;

    result->total_steps = 0;
    result->total_corrections = 0;
    result->final_confidence = 0.0f;
    result->accepted = 0;
    result->final_output = NULL;
    result->chain_stats_json = NULL;

    /* 创建思考链路 */
    agentos_thinking_chain_t* chain = NULL;
    agentos_error_t err = agentos_tc_chain_create(
        user_prompt,
        config->enable_context_window ? config->context_window_tokens : 0,
        config->enable_working_memory ? config->wm_capacity : 0,
        &chain);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(result);
        return err;
    }

    /* 创建元认知引擎 */
    agentos_metacognition_t* meta = NULL;
    err = agentos_mc_create(&meta);
    if (err != AGENTOS_SUCCESS) {
        agentos_tc_chain_destroy(chain);
        AGENTOS_FREE(result);
        return err;
    }
    agentos_mc_set_chain(meta, chain);

    /* 启动思考链路 */
    agentos_tc_chain_start(chain);

    uint64_t start_ns = agentos_time_monotonic_ns();

    /* ========== Phase 2: 流式批判循环 ========== */

    /* Step 1: S2首次生成 */
    agentos_thinking_step_t* gen_step = NULL;
    err = agentos_tc_step_create(chain, TC_STEP_GENERATION,
                                 user_prompt, strlen(user_prompt),
                                 NULL, 0, &gen_step);
    if (err != AGENTOS_SUCCESS) goto cleanup_fail;

    char* current_content = NULL;
    size_t current_len = 0;

    /* 构建带系统提示的完整prompt（如果有） */
    char* full_prompt = NULL;
    const char* actual_prompt = user_prompt;
    if (config->system_prompt) {
        size_t plen = strlen(config->system_prompt) + strlen(user_prompt) + 16;
        full_prompt = (char*)AGENTOS_MALLOC(plen);
        if (full_prompt) {
            snprintf(full_prompt, plen, "%s\n\n%s", config->system_prompt, user_prompt);
            actual_prompt = full_prompt;
        }
    }

    err = agentos_llm_service_call(service, actual_prompt, &current_content);
    if (full_prompt) AGENTOS_FREE(full_prompt);

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Dual-think S2 initial generation failed: err=%d", (int)err);
        goto cleanup_fail;
    }
    current_len = strlen(current_content);

    /* 记录到Context Window */
    agentos_tc_context_window_append(chain->ctx_window, current_content, current_len);

    /* 标记步骤完成 */
    agentos_tc_step_complete(gen_step, current_content, current_len, 0.75f, "S2-generator");
    result->total_steps++;

    /* 存储到Working Memory */
    if (config->enable_working_memory) {
        agentos_tc_working_memory_store(chain->working_mem, "initial_output",
                                        current_content, current_len + 1,
                                        "text/plain", 1);
    }

    /* Step 2-N: S1验证 → [修正循环] */
    int correction_round = 0;
    float best_confidence = 0.0f;
    char* best_content = NULL;
    size_t best_len = 0;
    int accepted = 0;

    while (correction_round < config->max_corrections) {
        /* 构建S1验证提示 */
        char* s1_prompt = NULL;
        char* recent_ctx = NULL;
        size_t ctx_len = 0;

        agentos_tc_context_window_get_recent(chain->ctx_window, 500, &recent_ctx, &ctx_len);

        err = build_s1_verification_prompt(service, user_prompt, current_content,
                                           recent_ctx, &s1_prompt);
        if (recent_ctx) AGENTOS_FREE(recent_ctx);

        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Failed to build S1 prompt, accepting current content");
            accepted = 1;
            break;
        }

        /* 调用LLM进行S1验证 */
        char* s1_response = NULL;
        err = agentos_llm_service_call(service, s1_prompt, &s1_response);
        AGENTOS_FREE(s1_prompt);

        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("S1 verification LLM call failed: err=%d, accepting current", (int)err);
            accepted = 1;
            AGENTOS_FREE(s1_response);
            break;
        }

        /* 解析S1验证结果 */
        float confidence = 0.0f;
        float dim_scores[5] = {0};
        char* critique = NULL;
        char* suggested_fix = NULL;

        int is_valid = parse_s1_verification_response(s1_response, &confidence,
                                                       dim_scores, &critique, &suggested_fix);
        AGENTOS_FREE(s1_response);

        /* 创建验证步骤记录 */
        agentos_thinking_step_t* verify_step = NULL;
        agentos_tc_step_create(chain, TC_STEP_VERIFICATION,
                               user_prompt, strlen(user_prompt),
                               &gen_step->step_id, 1, &verify_step);

        int verify_valid = (is_valid || confidence >= config->acceptance_threshold) ? 1 : 0;
        agentos_tc_step_verify(verify_step, &verify_valid,
                               critique ? critique : "", critique ? strlen(critique) : 0);
        agentos_tc_step_complete(verify_step,
                                critique ? critique : "No critique (accepted)",
                                critique ? strlen(critique) : 18,
                                confidence, "S1-verifier");
        result->total_steps++;

        /* 同时运行元认知本地评估（作为交叉验证） */
        mc_evaluation_result_t mc_eval;
        char* mc_ctx = NULL;
        size_t mc_ctx_len = 0;
        agentos_tc_context_window_get_recent(chain->ctx_window, 300, &mc_ctx, &mc_ctx_len);
        agentos_mc_evaluate_step(meta, gen_step, mc_ctx, mc_ctx_len, &mc_eval);
        if (mc_ctx) AGENTOS_FREE(mc_ctx);

        /* 综合判断：LLM S1验证 + 元认知本地评估 */
        float combined_confidence = (confidence + mc_eval.overall_score) / 2.0f;
        if (combined_confidence > best_confidence) {
            best_confidence = combined_confidence;
            if (best_content) AGENTOS_FREE(best_content);
            best_content = AGENTOS_STRDUP(current_content);
            best_len = current_len;
        }

        /* 反馈到元认知校准器 */
        agentos_mc_feedback(meta, confidence, verify_valid);

        if (verify_valid && mc_eval.is_acceptable) {
            accepted = 1;
            result->final_confidence = combined_confidence;
            if (mc_eval.critique_text) AGENTOS_FREE(mc_eval.critique_text);
            if (critique) AGENTOS_FREE(critique);
            if (suggested_fix) AGENTOS_FREE(suggested_fix);
            break;
        }

        /* 未通过 → 准备修正 */
        correction_round++;
        result->total_corrections++;

        AGENTOS_LOG_INFO("Dual-think round %d: not accepted (conf=%.2f, mc=%.2f), correcting...",
                        correction_round, confidence, mc_eval.overall_score);

        if (correction_round >= config->max_corrections) {
            AGENTOS_LOG_WARN("Dual-think: max corrections (%d) reached, using best result",
                            config->max_corrections);
            if (mc_eval.critique_text) AGENTOS_FREE(mc_eval.critique_text);
            if (critique) AGENTOS_FREE(critique);
            if (suggested_fix) AGENTOS_FREE(suggested_fix);
            break;
        }

        /* 使用元认知纠错或LLM建议修复 */
        const char* fix_text = (suggested_fix && strlen(suggested_fix) > 5) ?
                                suggested_fix :
                                (mc_eval.critique_text ? mc_eval.critique_text : "Please improve quality.");

        /* 构建修正提示 */
        char* corr_prompt = NULL;
        err = build_s2_correction_prompt(user_prompt, current_content,
                                         fix_text, correction_round, &corr_prompt);
        if (mc_eval.critique_text) AGENTOS_FREE(mc_eval.critique_text);
        if (critique) AGENTOS_FREE(critique);
        if (suggested_fix) AGENTOS_FREE(suggested_fix);

        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Failed to build correction prompt, stopping");
            break;
        }

        /* 创建修正步骤 */
        agentos_thinking_step_t* corr_step = NULL;
        uint32_t corr_deps[] = {verify_step->step_id};
        agentos_tc_step_create(chain, TC_STEP_CORRECTION,
                               corr_prompt, strlen(corr_prompt),
                               corr_deps, 1, &corr_step);

        /* S2重新生成 */
        char* corrected_content = NULL;
        err = agentos_llm_service_call(service, corr_prompt, &corrected_content);
        AGENTOS_FREE(corr_prompt);

        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Correction generation failed: err=%d, keeping previous", (int)err);
            agentos_tc_step_complete(corr_step, "correction_failed", 16, 0.0f, "S2-corrector");
            result->total_steps++;
            break;
        }

        /* 更新当前内容 */
        AGENTOS_FREE(current_content);
        current_content = corrected_content;
        current_len = strlen(corrected_content);

        /* 追加到Context Window */
        agentos_tc_context_window_append(chain->ctx_window, corrected_content, current_len);

        agentos_tc_step_complete(corr_step, corrected_content, current_len,
                                0.60f, "S2-corrector");
        result->total_steps++;
    }

    /* ========== 输出结果 ========== */
    uint64_t end_ns = agentos_time_monotonic_ns();
    result->elapsed_ns = end_ns - start_ns;
    result->accepted = accepted;

    if (accepted && current_content) {
        result->final_output = AGENTOS_STRDUP(current_content);
        result->final_output_len = strlen(current_content);
    } else if (best_content) {
        result->final_output = best_content;
        best_content = NULL;
        result->final_output_len = best_len;
    } else {
        result->final_output = AGENTOS_STRDUP(current_content ? current_content : "(no output)");
        result->final_output_len = strlen(result->final_output);
    }

    /* 获取链路统计 */
    agentos_tc_chain_stats(chain, &result->chain_stats_json, &result->chain_stats_len);

    AGENTOS_LOG_INFO("Dual-think complete: steps=%u corrections=%u accepted=%d conf=%.2f elapsed=%.1fms",
                    result->total_steps, result->total_corrections, result->accepted,
                    result->final_confidence,
                    (double)(result->elapsed_ns / 1000000));

    AGENTOS_FREE(current_content);
    if (best_content) AGENTOS_FREE(best_content);
    agentos_mc_destroy(meta);
    agentos_tc_chain_stop(chain);
    agentos_tc_chain_destroy(chain);
    *out_result = result;
    return AGENTOS_SUCCESS;

cleanup_fail:
    AGENTOS_FREE(current_content);
    if (best_content) AGENTOS_FREE(best_content);
    agentos_mc_destroy(meta);
    agentos_tc_chain_stop(chain);
    agentos_tc_chain_destroy(chain);
    result->final_output = AGENTOS_STRDUP("(dual-think failed)");
    result->final_output_len = 17;
    *out_result = result;
    return err;
}

void agentos_llm_dual_result_free(agentos_dual_think_result_t* result) {
    if (!result) return;
    if (result->final_output) AGENTOS_FREE(result->final_output);
    if (result->chain_stats_json) AGENTOS_FREE(result->chain_stats_json);
    AGENTOS_FREE(result);
}

agentos_error_t agentos_llm_dual_think_simple(
    agentos_llm_service_t* service,
    const char* user_prompt,
    char** out_response)
{
    if (!service || !user_prompt || !out_response) return AGENTOS_EINVAL;

    agentos_dual_think_result_t* result = NULL;
    agentos_error_t err = agentos_llm_dual_think(service, NULL, user_prompt, &result);
    if (err != AGENTOS_SUCCESS) return err;

    if (result && result->final_output) {
        *out_response = result->final_output;
        result->final_output = NULL;
    } else {
        *out_response = AGENTOS_STRDUP("");
    }

    agentos_llm_dual_result_free(result);
    return AGENTOS_SUCCESS;
}
