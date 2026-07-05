/**
 * @file llm_client.c
 * @brief LLM 客户端内置实现 — CoreLoopThree 本地适配层
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供 agentrt_llm_service 的内置实现。当 MemoryRovol 商业 LLM 仓库不可用时，
 * 作为 CoreLoopThree 的本地 LLM 客户端适配层。
 *
 * 设计：
 * - agentrt_llm_service_t 是一个轻量句柄，持有一个可选的 IPC adapter 指针
 * - 当 adapter 注入且已连接时，is_available() 返回 true，call() 桥接到 adapter
 * - 当 adapter 未注入时，is_available() 返回 false，call() 返回 ENOTSUP
 * - 调用方（如 reactive/reflective 规划器）在 is_available() 返回 false 时走降级路径
 *
 * 当前状态（P2.7）：
 * - create() 创建 service，adapter=NULL，available=0
 * - set_adapter() 注入 llm_svc_adapter_t，根据连接状态设置 available
 * - 调用方（reactive/reflective）传 NULL llm 或 available=0 的 service，走规则降级路径
 * - available=1 时，call()/complete()/dual_think() 通过 IPC 桥接到 llm_d
 *
 * BAN-35 合规：CoreLoopThree 使用内置 memory 子系统
 */

#include "llm_client.h"

#include "agentrt.h"
#include "llm_svc_adapter.h" /* P2.7: IPC adapter 桥接 */
#include "logger.h"
#include "memory_compat.h" /* P2.7: AGENTRT_MALLOC/CALLOC/FREE 宏定义 */
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 内部类型定义 ==================== */

/**
 * @brief LLM 服务句柄的内部实现
 *
 * P2.7: adapter 字段持有 llm_svc_adapter_t*（borrowed，由 loop.c 管理生命周期）。
 * set_adapter 注入 adapter 后，available 根据 adapter 连接状态设置。
 * call()/complete()/dual_think() 通过 llm_svc_adapter_complete 桥接到 llm_d。
 */
struct agentrt_llm_service {
    void *adapter;  /**< llm_svc_adapter_t*（void* 避免头文件循环依赖），NULL 表示未注入 */
    int available;  /**< 0=不可用（无 adapter 或未连接），1=可用（adapter 已连接） */
};

/* ==================== 生命周期函数 ==================== */

agentrt_error_t agentrt_llm_service_create(const agentrt_llm_config_t *config,
                                           agentrt_llm_service_t **out_service)
{
    if (!out_service)
        return AGENTRT_EINVAL;
    *out_service = NULL;

    /* config 可为 NULL：创建未配置的 service，后续通过 set_adapter 注入 */
    agentrt_llm_service_t *svc =
        (agentrt_llm_service_t *)AGENTRT_CALLOC(1, sizeof(agentrt_llm_service_t));
    if (!svc) {
        AGENTRT_LOG_ERROR("LLM service alloc failed (errno=ENOMEM)");
        return AGENTRT_ENOMEM;
    }

    svc->adapter = NULL;
    svc->available = 0;

    if (config && config->model_name) {
        AGENTRT_LOG_INFO("LLM service created (model=%s, builtin adapter not attached)",
                         config->model_name);
    } else {
        AGENTRT_LOG_INFO("LLM service created (no config, adapter not attached)");
    }

    *out_service = svc;
    return AGENTRT_SUCCESS;
}

void agentrt_llm_service_destroy(agentrt_llm_service_t *service)
{
    if (!service)
        return;
    /* adapter 由创建方（loop.c）管理，这里不销毁 */
    service->adapter = NULL;
    service->available = 0;
    AGENTRT_FREE(service);
}

/* ==================== P2.7: 便捷构造 ==================== */

agentrt_error_t agentrt_llm_service_create_default(agentrt_llm_service_t **out_service)
{
    return agentrt_llm_service_create(NULL, out_service);
}

/* ==================== 可用性检查 ==================== */

int agentrt_llm_service_is_available(const agentrt_llm_service_t *service)
{
    if (!service)
        return 0;
    if (!service->adapter)
        return 0;
    return service->available;
}

/* ==================== P2.7: IPC adapter 注入 ==================== */

agentrt_error_t agentrt_llm_service_set_adapter(agentrt_llm_service_t *service, void *adapter)
{
    if (!service)
        return AGENTRT_EINVAL;

    /* adapter 可为 NULL（卸载语义） */
    service->adapter = adapter;

    if (adapter) {
        /* 探测 adapter 连接状态：已连接才标记 available */
        llm_svc_adapter_t *llm_adapter = (llm_svc_adapter_t *)adapter;
        bool connected = llm_svc_adapter_is_connected(llm_adapter);
        service->available = connected ? 1 : 0;
        AGENTRT_LOG_INFO("LLM service: adapter injected (connected=%s, available=%d)",
                         connected ? "yes" : "no", service->available);
    } else {
        service->available = 0;
        AGENTRT_LOG_INFO("LLM service: adapter detached");
    }

    return AGENTRT_SUCCESS;
}

/* ==================== 调用接口 ==================== */

agentrt_error_t agentrt_llm_service_call(agentrt_llm_service_t *service, const char *prompt,
                                         char **out_response)
{
    if (!out_response)
        return AGENTRT_EINVAL;
    *out_response = NULL;

    if (!service || !prompt)
        return AGENTRT_EINVAL;

    if (!service->available || !service->adapter)
        return AGENTRT_ENOTSUP;

    /* P2.7: 桥接到 llm_svc_adapter_complete (IPC → llm_d) */
    llm_svc_adapter_t *adapter = (llm_svc_adapter_t *)service->adapter;

    llm_message_t msgs[1];
    msgs[0].role = "user";
    msgs[0].content = prompt;

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    cfg.model = NULL; /* 使用 provider 默认模型 */
    cfg.messages = msgs;
    cfg.message_count = 1;
    cfg.temperature = 0.3f;
    cfg.top_p = 1.0f;
    cfg.max_tokens = 4096;
    cfg.stream = 0;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(adapter, &cfg, &resp);
    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0) {
        AGENTRT_LOG_WARN("LLM service_call: IPC complete failed (ret=%d resp=%p)",
                         ret, (void *)resp);
        if (resp)
            llm_response_free(resp);
        return AGENTRT_ESERVICE;
    }

    const char *content = resp->choices[0].content;
    if (!content) {
        llm_response_free(resp);
        return AGENTRT_ESERVICE;
    }

    size_t len = strlen(content);
    char *text = (char *)AGENTRT_MALLOC(len + 1);
    if (!text) {
        llm_response_free(resp);
        return AGENTRT_ENOMEM;
    }
    __builtin_memcpy(text, content, len);
    text[len] = '\0';

    *out_response = text;
    llm_response_free(resp);
    return AGENTRT_SUCCESS;
}

/* ==================== 完整请求接口 ==================== */

agentrt_error_t agentrt_llm_complete(agentrt_llm_service_t *service,
                                     const agentrt_llm_request_t *request,
                                     agentrt_llm_response_t **out_response)
{
    if (!out_response)
        return AGENTRT_EINVAL;
    *out_response = NULL;

    if (!service || !request)
        return AGENTRT_EINVAL;

    if (!service->available || !service->adapter)
        return AGENTRT_ENOTSUP;

    /* P2.7: 桥接到 llm_svc_adapter_complete (IPC → llm_d) */
    llm_svc_adapter_t *adapter = (llm_svc_adapter_t *)service->adapter;

    /* 构造消息数组：可选 system prompt + user prompt */
    llm_message_t msgs[2];
    size_t msg_count = 0;
    if (request->system_prompt && request->system_prompt[0]) {
        msgs[msg_count].role = "system";
        msgs[msg_count].content = request->system_prompt;
        msg_count++;
    }
    msgs[msg_count].role = "user";
    msgs[msg_count].content = request->prompt;
    msg_count++;

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    cfg.model = request->model; /* NULL 时使用 provider 默认模型 */
    cfg.messages = msgs;
    cfg.message_count = msg_count;
    cfg.temperature = request->temperature > 0 ? request->temperature : 0.3f;
    cfg.top_p = 1.0f;
    cfg.max_tokens = request->max_tokens > 0 ? (int)request->max_tokens : 4096;
    cfg.stream = 0;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(adapter, &cfg, &resp);
    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0) {
        AGENTRT_LOG_WARN("LLM complete: IPC complete failed (ret=%d resp=%p)",
                         ret, (void *)resp);
        if (resp)
            llm_response_free(resp);
        return AGENTRT_ESERVICE;
    }

    const char *content = resp->choices[0].content;
    if (!content) {
        llm_response_free(resp);
        return AGENTRT_ESERVICE;
    }

    size_t len = strlen(content);
    char *text = (char *)AGENTRT_MALLOC(len + 1);
    if (!text) {
        llm_response_free(resp);
        return AGENTRT_ENOMEM;
    }
    __builtin_memcpy(text, content, len);
    text[len] = '\0';

    agentrt_llm_response_t *result =
        (agentrt_llm_response_t *)AGENTRT_CALLOC(1, sizeof(agentrt_llm_response_t));
    if (!result) {
        AGENTRT_FREE(text);
        llm_response_free(resp);
        return AGENTRT_ENOMEM;
    }
    result->text = text;
    result->usage_tokens = resp->completion_tokens;
    result->total_tokens = resp->total_tokens;
    result->finish_reason = 0; /* 0 = 正常完成 */

    *out_response = result;
    llm_response_free(resp);
    return AGENTRT_SUCCESS;
}

void agentrt_llm_response_free(agentrt_llm_response_t *response)
{
    if (!response)
        return;
    if (response->text)
        AGENTRT_FREE(response->text);
    AGENTRT_FREE(response);
}

/* ==================== 双思考接口 ==================== */
#ifndef MEMORYROVOL_OSS

/**
 * @brief 双思考配置的内部定义
 *
 * P2.7/P2.8: 双思考管线 — S2 生成 + S1 对抗式验证。
 */
struct agentrt_dual_think_config {
    int s1_verify_enabled;    /**< S1 验证是否启用（1=启用） */
    int s2_generate_enabled;  /**< S2 生成是否启用（1=启用） */
    float accept_threshold;   /**< S1/S2 一致性接受阈值 */
};

/**
 * @brief 双思考结果的内部定义
 */
struct agentrt_dual_think_result {
    char *s1_verify_output;    /**< S1 验证输出（需 free） */
    char *s2_generate_output;  /**< S2 生成输出（需 free） */
    int s1_s2_conflict;        /**< S1/S2 是否冲突（1=冲突） */
    float confidence;          /**< 整体置信度 */
};

static const agentrt_dual_think_config_t g_dual_think_default_config = {
    .s1_verify_enabled = 1,
    .s2_generate_enabled = 1,
    .accept_threshold = 0.7f,
};

/**
 * @brief 内部辅助：通过 adapter 调用 LLM 并返回文本
 *
 * 共用的 LLM 调用核心，system_prompt 可为 NULL。
 * 返回 AGENTRT_MALLOC 分配的文本，调用方负责释放。
 */
static agentrt_error_t llm_call_adapter(llm_svc_adapter_t *adapter,
                                        const char *system_prompt,
                                        const char *user_prompt,
                                        char **out_text)
{
    if (!adapter || !user_prompt || !out_text)
        return AGENTRT_EINVAL;
    *out_text = NULL;

    llm_message_t msgs[2];
    size_t msg_count = 0;
    if (system_prompt && system_prompt[0]) {
        msgs[msg_count].role = "system";
        msgs[msg_count].content = system_prompt;
        msg_count++;
    }
    msgs[msg_count].role = "user";
    msgs[msg_count].content = user_prompt;
    msg_count++;

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    cfg.model = NULL;
    cfg.messages = msgs;
    cfg.message_count = msg_count;
    cfg.temperature = 0.3f;
    cfg.top_p = 1.0f;
    cfg.max_tokens = 4096;
    cfg.stream = 0;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(adapter, &cfg, &resp);
    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0) {
        if (resp)
            llm_response_free(resp);
        return AGENTRT_ESERVICE;
    }

    const char *content = resp->choices[0].content;
    if (!content) {
        llm_response_free(resp);
        return AGENTRT_ESERVICE;
    }

    size_t len = strlen(content);
    char *text = (char *)AGENTRT_MALLOC(len + 1);
    if (!text) {
        llm_response_free(resp);
        return AGENTRT_ENOMEM;
    }
    __builtin_memcpy(text, content, len);
    text[len] = '\0';

    *out_text = text;
    llm_response_free(resp);
    return AGENTRT_SUCCESS;
}

/**
 * @brief P2.7/P2.8: S2 生成 system prompt — 主思考生成
 */
static const char DT_S2_SYSTEM_PROMPT[] =
    "You are the S2 (slow) generator in a dual-thinking system. "
    "Generate a thorough, logically rigorous response to the user's request. "
    "Ensure your reasoning is correct, evidence-based, and stays on topic.";

/**
 * @brief P2.7/P2.8: S1 验证 system prompt — 对抗式验证（与 tc3 一致）
 */
static const char DT_S1_VERIFY_SYSTEM_PROMPT[] =
    "You are the S1 (fast) adversarial critic in a dual-thinking system. "
    "Find bugs, flaws, and deviations in the response. "
    "Reply with ONLY a JSON object: "
    "{\"score\": <float 0.0-1.0>, \"acceptable\": <true|false>, \"critique\": \"<bugs found>\"}. "
    "Score guide: 0.9-1.0=no bugs, 0.7-0.89=minor issues, 0.5-0.69=several bugs, "
    "0.3-0.49=major flaws, below 0.3=critical defects.";

agentrt_error_t agentrt_llm_dual_think(agentrt_llm_service_t *service,
                                       const agentrt_dual_think_config_t *config,
                                       const char *user_prompt,
                                       agentrt_dual_think_result_t **out_result)
{
    if (!out_result)
        return AGENTRT_EINVAL;
    *out_result = NULL;

    if (!service || !user_prompt)
        return AGENTRT_EINVAL;

    if (!service->available || !service->adapter)
        return AGENTRT_ENOTSUP;

    const agentrt_dual_think_config_t *cfg = config ? config : &g_dual_think_default_config;
    llm_svc_adapter_t *adapter = (llm_svc_adapter_t *)service->adapter;

    /* 分配结果结构 */
    agentrt_dual_think_result_t *result =
        (agentrt_dual_think_result_t *)AGENTRT_CALLOC(1, sizeof(agentrt_dual_think_result_t));
    if (!result)
        return AGENTRT_ENOMEM;
    result->confidence = 0.0f;
    result->s1_s2_conflict = 0;

    /* Phase 1: S2 生成 */
    if (cfg->s2_generate_enabled) {
        agentrt_error_t err =
            llm_call_adapter(adapter, DT_S2_SYSTEM_PROMPT, user_prompt, &result->s2_generate_output);
        if (err != AGENTRT_SUCCESS) {
            AGENTRT_LOG_WARN("dual_think: S2 generate failed (err=%d)", (int)err);
            agentrt_llm_dual_result_free(result);
            return err;
        }
    } else {
        /* S2 禁用：复制 user_prompt 作为占位输出 */
        size_t pl = strlen(user_prompt);
        result->s2_generate_output = (char *)AGENTRT_MALLOC(pl + 1);
        if (!result->s2_generate_output) {
            agentrt_llm_dual_result_free(result);
            return AGENTRT_ENOMEM;
        }
        __builtin_memcpy(result->s2_generate_output, user_prompt, pl);
        result->s2_generate_output[pl] = '\0';
    }

    /* Phase 2: S1 对抗式验证 */
    if (cfg->s1_verify_enabled && result->s2_generate_output) {
        /* 构造验证 prompt：原始任务 + S2 输出 */
        size_t s2_len = strlen(result->s2_generate_output);
        size_t prompt_len = strlen(user_prompt) + s2_len + 128;
        char *verify_prompt = (char *)AGENTRT_MALLOC(prompt_len);
        if (!verify_prompt) {
            agentrt_llm_dual_result_free(result);
            return AGENTRT_ENOMEM;
        }
        int wn = snprintf(verify_prompt, prompt_len,
                          "Original task: %s\n\nResponse to verify:\n%s\n\n"
                          "Critique this response. Find bugs and deviations.",
                          user_prompt, result->s2_generate_output);
        if (wn <= 0 || (size_t)wn >= prompt_len) {
            AGENTRT_FREE(verify_prompt);
            agentrt_llm_dual_result_free(result);
            return AGENTRT_EINVAL;
        }

        agentrt_error_t err = llm_call_adapter(adapter, DT_S1_VERIFY_SYSTEM_PROMPT,
                                               verify_prompt, &result->s1_verify_output);
        AGENTRT_FREE(verify_prompt);
        if (err != AGENTRT_SUCCESS) {
            AGENTRT_LOG_WARN("dual_think: S1 verify failed (err=%d)", (int)err);
            /* S1 失败不致命，继续返回 S2 结果（confidence 降低） */
            result->confidence = 0.4f;
            result->s1_s2_conflict = 0;
            *out_result = result;
            return AGENTRT_SUCCESS;
        }

        /* 简单启发式：从 S1 输出中检测 "acceptable" 字段或关键词估计置信度 */
        const char *s1 = result->s1_verify_output;
        if (strstr(s1, "\"acceptable\":true") || strstr(s1, "\"acceptable\": true")) {
            /* S1 判定可接受 — 检查 score 提取 */
            result->s1_s2_conflict = 0;
            result->confidence = 0.85f;
        } else if (strstr(s1, "\"acceptable\":false") || strstr(s1, "\"acceptable\": false")) {
            /* S1 判定不可接受 — 冲突 */
            result->s1_s2_conflict = 1;
            result->confidence = 0.3f;
        } else {
            /* 无法解析 — 中等置信度 */
            result->confidence = 0.5f;
            result->s1_s2_conflict = 0;
        }
    } else {
        /* S1 禁用 — 仅 S2 输出，无冲突 */
        result->confidence = 0.6f;
        result->s1_s2_conflict = 0;
    }

    *out_result = result;
    return AGENTRT_SUCCESS;
}

void agentrt_llm_dual_result_free(agentrt_dual_think_result_t *result)
{
    if (!result)
        return;
    if (result->s1_verify_output)
        AGENTRT_FREE(result->s1_verify_output);
    if (result->s2_generate_output)
        AGENTRT_FREE(result->s2_generate_output);
    AGENTRT_FREE(result);
}

agentrt_error_t agentrt_llm_dual_think_simple(agentrt_llm_service_t *service,
                                              const char *user_prompt, char **out_response)
{
    if (!out_response)
        return AGENTRT_EINVAL;
    *out_response = NULL;

    if (!service || !user_prompt)
        return AGENTRT_EINVAL;

    if (!service->available || !service->adapter)
        return AGENTRT_ENOTSUP;

    /* P2.7: 简化版双思考 = 单次 LLM 调用（仅 S2 生成路径） */
    llm_svc_adapter_t *adapter = (llm_svc_adapter_t *)service->adapter;
    return llm_call_adapter(adapter, DT_S2_SYSTEM_PROMPT, user_prompt, out_response);
}

const agentrt_dual_think_config_t *agentrt_dual_think_config_default(void)
{
    return &g_dual_think_default_config;
}

#endif /* !MEMORYROVOL_OSS */
