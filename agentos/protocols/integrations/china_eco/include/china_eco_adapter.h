// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file china_eco_adapter.h
 * @brief Chinese LLM Ecosystem Unified Compatibility Adapter for AgentOS
 *
 * 国内大模型生态统一兼容适配器，支持主流国内大模型服务商：
 * - DeepSeek — 深度求索（深度推理、代码生成）
 * - Qwen/通义千问 — 阿里云（多模态、长上下文）
 * - Wenxin/文心一言 — 百度（企业级、知识增强）
 * - ChatGLM/智谱 — 智谱AI（对话、代码、Agent）
 * - Spark/星火 — 讯飞（语音+文本多模态）
 * - Baichuan/百川 — 百川智能（中文优化）
 * - Doubao/豆包 — 字节跳动（高并发、低成本）
 * - Kimi/月之暗面 — Moonshot AI（长文本处理）
 * - MiniMax — MiniMax（多模态、语音合成）
 * - Volcengine/火山引擎 — 字节跳动云（企业级部署）
 *
 * 所有提供商均兼容 OpenAI API 格式，通过差异化配置实现统一接入。
 *
 * BAN-19 合规：无 curl 时 fail-closed，不使用 mock/模板生成假响应。
 *
 * @since 2.1.0
 * @see unified_protocol.h
 * @see agentos_protocol_interface.h
 */

#ifndef AGENTOS_CHINA_ECO_ADAPTER_H
#define AGENTOS_CHINA_ECO_ADAPTER_H

#include "unified_protocol.h"
#include "agentos_protocol_interface.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHINA_ECO_ADAPTER_VERSION    "1.0.0"
#define CHINA_ECO_MAX_PROVIDERS      16
#define CHINA_ECO_MAX_MODELS         64
#define CHINA_ECO_MAX_MESSAGES       128
#define CHINA_ECO_MAX_TOOLS          32
#define CHINA_ECO_MAX_RESPONSE_LEN   8192
#define CHINA_ECO_DEFAULT_TIMEOUT_MS 60000
#define CHINA_ECO_MAX_RETRIES        3

typedef enum {
    CHINA_PROVIDER_DEEPSEEK = 0,
    CHINA_PROVIDER_QWEN,
    CHINA_PROVIDER_WENXIN,
    CHINA_PROVIDER_CHATGLM,
    CHINA_PROVIDER_SPARK,
    CHINA_PROVIDER_BAICHUAN,
    CHINA_PROVIDER_DOUBAO,
    CHINA_PROVIDER_KIMI,
    CHINA_PROVIDER_MINIMAX,
    CHINA_PROVIDER_VOLCENGINE,
    CHINA_PROVIDER_CUSTOM
} china_provider_t;

typedef enum {
    CHINA_AUTH_BEARER = 0,
    CHINA_AUTH_API_KEY_HEADER,
    CHINA_AUTH_CUSTOM
} china_auth_type_t;

typedef struct {
    china_provider_t provider;
    const char* id;
    const char* display_name;
    const char* api_name;
    int max_context_tokens;
    int max_output_tokens;
    bool supports_streaming;
    bool supports_function_calling;
    bool supports_vision;
    bool is_default;
} china_model_info_t;

typedef struct {
    char* api_key;
    char* base_url;
    china_provider_t provider;
    china_auth_type_t auth_type;
    char* auth_header_name;
    char* default_model;
    int max_tokens;
    double temperature;
    double top_p;
    bool enable_streaming;
    bool enable_function_calling;
    uint32_t timeout_ms;
    int max_retries;
    char* organization;
} china_eco_config_t;

typedef struct {
    char* role;
    char* content;
} china_eco_message_t;

typedef struct {
    char* content;
    char* model;
    char* finish_reason;
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
} china_eco_response_t;

typedef struct {
    const char* chunk;
    size_t length;
    bool is_final;
} china_eco_stream_chunk_t;

typedef void (*china_eco_stream_handler_t)(const china_eco_stream_chunk_t* chunk,
                                            void* user_data);

typedef struct china_eco_context_s china_eco_context_t;

china_eco_config_t china_eco_config_default(china_provider_t provider);

china_eco_context_t* china_eco_context_create(const china_eco_config_t* config);
void china_eco_context_destroy(china_eco_context_t* ctx);

bool china_eco_is_initialized(const china_eco_context_t* ctx);
const char* china_eco_adapter_version(void);

int china_eco_chat_completion(china_eco_context_t* ctx,
                               const china_eco_message_t* messages,
                               size_t message_count,
                               const char* model,
                               double temperature,
                               int max_tokens,
                               china_eco_response_t* response);

int china_eco_chat_streaming(china_eco_context_t* ctx,
                               const china_eco_message_t* messages,
                               size_t message_count,
                               const char* model,
                               china_eco_stream_handler_t handler,
                               void* user_data);

int china_eco_embeddings(china_eco_context_t* ctx,
                          const char** inputs,
                          size_t input_count,
                          const char* model,
                          double** out_embeddings,
                          size_t* out_dim);

int china_eco_list_models(china_eco_context_t* ctx,
                            china_model_info_t** models,
                            size_t* count);

const char* china_eco_get_provider_name(china_provider_t provider);
const char* china_eco_get_provider_base_url(china_provider_t provider);
const char* china_eco_get_provider_default_model(china_provider_t provider);

int china_eco_route_request(china_eco_context_t* ctx,
                              const char* path,
                              const char* method,
                              const char* body_json,
                              char** response_json);

const proto_adapter_t* china_eco_get_protocol_adapter(void);

void china_eco_response_destroy(china_eco_response_t* resp);
void china_eco_message_destroy(china_eco_message_t* msg);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CHINA_ECO_ADAPTER_H */
