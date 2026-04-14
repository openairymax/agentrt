// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file protocol_transformers.h
 * @brief Protocol Message Transformers (Complete Implementation)
 *
 * 实现 AgentOS 支持的所有协议之间的双向消息转换：
 * - JSON-RPC 2.0 ↔ MCP v1.0
 * - JSON-RPC 2.0 ↔ A2A v0.3
 * - JSON-RPC 2.0 ↔ OpenAI API
 * - JSON-RPC 2.0 ↔ OpenJiuwen
 *
 * 转换规则遵循 Capital_Specifications/agentos_contract/protocol_contract.md
 *
 * @since 2.0.0
 */

#ifndef AGENTOS_PROTOCOL_TRANSFORMERS_H
#define AGENTOS_PROTOCOL_TRANSFORMERS_H

#include "unified_protocol.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 转换上下文 — 携带协议特定的元数据辅助转换
 * ============================================================================ */

typedef struct {
    char source_protocol[32];
    char target_protocol[32];
    char agent_id[64];
    char session_id[64];
    char trace_id[64];
    int jsonrpc_id_counter;
} transform_context_t;

transform_context_t* transform_context_create(const char* src_proto,
                                                const char* tgt_proto);
void transform_context_destroy(transform_context_t* ctx);

/* ============================================================================
 * JSON-RPC → MCP 转换器
 * ============================================================================ */

/**
 * @brief 将 JSON-RPC tools/call 请求转换为 MCP tools/call 格式
 *
 * 映射规则:
 *   jsonrpc.method = "skill.execute" -> mcp.method = "tools/call"
 *   jsonrpc.params.name -> mcp.params.name
 *   jsonrpc.params.arguments -> mcp.params.arguments
 */
int transformer_jsonrpc_to_mcp_request(const unified_message_t* source,
                                       unified_message_t* target,
                                       void* context);

/**
 * @brief 将 MCP tools/call 响应转换为 JSON-RPC 格式
 *
 * 映射规则:
 *   mcp.result.content[] -> jsonrpc.result.output
 *   mcp.error -> jsonrpc.error
 */
int transformer_mcp_to_jsonrpc_response(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context);

/**
 * @brief 将 MCP tools/list 响应转换为 JSON-RPC skill.list 格式
 */
int transformer_mcp_tools_list_to_jsonrpc(const unified_message_t* source,
                                          unified_message_t* target,
                                          void* context);

/* ============================================================================
 * JSON-RPC → A2A 转换器
 * ============================================================================ */

/**
 * @brief 将 JSON-RPC task.submit 请求转换为 A2A task/delegate 格式
 */
int transformer_jsonrpc_to_a2a_task(const unified_message_t* source,
                                    unified_message_t* target,
                                    void* context);

/**
 * @brief 将 A2A task 响应转换为 JSON-RPC 格式
 */
int transformer_a2a_to_jsonrpc_response(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context);

/**
 * @brief 将 JSON-RPC agent.discover 请求转换为 A2A agent/discover 格式
 */
int transformer_jsonrpc_to_a2a_discover(const unified_message_t* source,
                                        unified_message_t* target,
                                        void* context);

/**
 * @brief 将 A2A agent card 列表转换为 JSON-RPC agent.list 格式
 */
int transformer_a2a_agents_to_jsonrpc(const unified_message_t* source,
                                      unified_message_t* target,
                                      void* context);

/* ============================================================================
 * JSON-RPC → OpenAI API 转换器
 * ============================================================================ */

/**
 * @brief 将 JSON-RPC llm.complete 请求转换为 OpenAI /v1/chat/completions 格式
 *
 * 映射规则:
 *   jsonrpc.params.messages -> openai.messages (role/content)
 *   jsonrpc.params.model -> openai.model
 *   jsonrpc.params.temperature -> openai.temperature
 *   jsonrpc.params.max_tokens -> openai.max_tokens
 *   jsonrpc.params.tools -> openai.tools/functions[]
 */
int transformer_jsonrpc_to_openai_chat(const unified_message_t* source,
                                      unified_message_t* target,
                                      void* context);

/**
 * @brief 将 OpenAI chat completions 响应转换为 JSON-RPC 格式
 *
 * 映射规则:
 *   openai.choices[0].message.content -> jsonrpc.result.content
 *   openai.choices[0].finish_reason -> jsonrpc.result.finish_reason
 *   openai.usage -> jsonrpc.result.usage
 */
int transformer_openai_chat_to_jsonrpc(const unified_message_t* source,
                                      unified_message_t* target,
                                      void* context);

/**
 * @brief 将 OpenAI streaming chunk 转换为 JSON-RPC notification
 */
int transformer_openai_stream_chunk_to_jsonrpc(const unified_message_t* source,
                                               unified_message_t* target,
                                               void* context);

/**
 * @brief 将 JSON-RPC embedding 请求转换为 OpenAI /v1/embeddings 格式
 */
int transformer_jsonrpc_to_openai_embedding(const unified_message_t* source,
                                           unified_message_t* target,
                                           void* context);

/* ============================================================================
 * JSON-RPC → OpenJiuwen 转换器
 * ============================================================================ */

/**
 * @brief 将 JSON-RPC 请求转换为 OpenJiuwen 二进制格式
 *
 * OpenJiuwen 使用自定义二进制协议:
 *   Header(24B) + Payload(variable) + CRC32(4B)
 */
int transformer_jsonrpc_to_openjiuwen(const unified_message_t* source,
                                     unified_message_t* target,
                                     void* context);

/**
 * @brief 将 OpenJiuwen 响应转换为 JSON-RPC 格式
 */
int transformer_openjiuwen_to_jsonrpc(const unified_message_t* source,
                                     unified_message_t* target,
                                     void* context);

/* ============================================================================
 * 通用工具函数
 * ============================================================================ */

/**
 * @brief 根据源和目标协议自动选择转换器
 *
 * 协议映射表:
 *   HTTP(JSON-RPC) + endpoint /mcp/*     -> MCP
 *   HTTP(JSON-RPC) + endpoint /a2a/*     -> A2A
 *   HTTP(JSON-RPC) + endpoint /v1/chat/* -> OpenAI
 *   HTTP(JSON-RPC) + endpoint /ojw/*     -> OpenJiuwen
 */
int protocol_auto_transform(const unified_message_t* source,
                           unified_message_t* target,
                           const char* target_protocol_name);

/**
 * @brief 验证转换后的消息完整性
 */
int protocol_validate_transformed(const unified_message_t* msg);

/**
 * @brief 获取所有已注册转换器的名称列表
 */
const char** protocol_list_transformers(size_t* count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PROTOCOL_TRANSFORMERS_H */
