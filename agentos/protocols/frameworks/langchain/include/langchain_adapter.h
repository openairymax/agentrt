// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file langchain_adapter.h
 * @brief LangChain Framework Integration Adapter for AgentOS
 *
 * LangChain 框架适配器，实现AgentOS与LangChain生态的完整集成。
 *
 * LangChain核心概念映射:
 * - Chain → AgentOS Task Pipeline
 * - Agent → AgentOS Agent + Protocol Session
 * - Tool → AgentOS MCP/OpenAI tool interface
 * - LLM → AgentOS LLM Daemon via protocol
 * - Memory → AgentOS MemoryRovol (L1-L4)
 * - Retriever → AgentOS memory.search protocol
 *
 * 支持的LangChain组件:
 * 1. LCEL (LangChain Expression Language) 链式执行
 * 2. AgentExecutor 多步推理代理
 * 3. Tool Calling 原生工具调用
 * 4. RAG 检索增强生成
 * 5. ConversationBufferMemory 对话记忆
 * 6. StreamingIterator 流式输出
 *
 * @since 2.1.0
 */

#ifndef AGENTOS_LANGCHAIN_ADAPTER_H
#define AGENTOS_LANGCHAIN_ADAPTER_H

#include "unified_protocol.h"
#include "agentos_protocol_interface.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LANGCHAIN_ADAPTER_VERSION    "1.0.0"
#define LANGCHAIN_MAX_CHAINS          64
#define LANGCHAIN_MAX_TOOLS           128
#define LANGCHAIN_MAX_AGENTS          32
#define LANGCHAIN_MAX_MEMORY_ENTRIES  1024
#define LANGCHAIN_DEFAULT_TIMEOUT_MS  60000

/* Compatibility macros for .c file */
#define LC_MAX_RESPONSE_LEN           8192
#define LC_STREAM_CHUNK_SIZE          1024

typedef enum {
    LC_TYPE_LLM = 0,
    LC_TYPE_CHAT_MODEL,
    LC_TYPE_EMBEDDING_MODEL,
    LC_TYPE_TOOL,
    LC_TYPE_CHAIN,
    LC_TYPE_AGENT,
    LC_TYPE_MEMORY,
    LC_TYPE_RETRIEVER,
    LC_TYPE_OUTPUT_PARSER
} langchain_component_type_t;

typedef enum {
    LC_CHAIN_SEQUENTIAL = 0,
    LC_CHAIN_ROUTER,
    LC_CHAIN_MAP_REDUCE,
    LC_CHAIN_PARALLEL,
    LC_CHAIN_CONDITIONAL,
    LC_CHAIN_CUSTOM
} langchain_chain_type_t;

typedef enum {
    LC_AGENT_REACT = 0,
    LC_AGENT_PLAN_AND_EXECUTE,
    LC_AGENT_OPENAI_FUNCTIONS,
    LC_AGENT_STRUCTURED_CHAT,
    LC_AGENT_XML
} langchain_agent_type_t;

typedef enum {
    LC_MEM_BUFFER = 0,
    LC_MEM_SUMMARY,
    LC_MEM_WINDOW,
    LC_MEM_TOKEN,
    LC_MEM_ENTITY,
    LC_MEM_KG
} langchain_memory_type_t;

typedef struct {
    char* id;
    char* name;
    char* description;
    langchain_component_type_t type;
    bool is_configured;
    bool is_available;
} langchain_component_info_t;

typedef struct {
    char* id;
    char* name;
    langchain_chain_type_t type;
    char** step_ids;
    size_t step_count;
    bool is_compiled;
} langchain_chain_def_t;

typedef struct {
    char* id;
    char* name;
    langchain_chain_type_t type;
    char** step_ids;
    size_t step_count;
    bool is_compiled;
    char* input_schema_json;
    char* output_schema_json;
    void* compiled_executable;
} langchain_chain_instance_t;

typedef struct {
    char* id;
    char* name;
    langchain_agent_type_t type;
    char* llm_id;
    char** tool_ids;
    size_t tool_count;
    char* memory_id;
    int max_iterations;
    int max_execution_time_sec;
    bool handle_parsing_errors;
    bool verbose;
} langchain_agent_def_t;

typedef struct {
    char* id;
    char* name;
    char* description;
    char* function_schema_json;
    langchain_component_type_t tool_type;
    bool is_async;
} langchain_tool_def_t;

typedef struct {
    char* id;
    langchain_memory_type_t type;
    size_t max_entries;
    size_t current_entries;
    char** messages;
    size_t message_count;
    char* summary;
    uint64_t last_updated;
} langchain_memory_t;

typedef struct {
    char* chain_id;
    char* input_json;
    char* output_json;
    double execution_time_ms;
    int step_count;
    bool success;
    char* error_message;
    char** intermediate_results;
    size_t intermediate_count;
} langchain_execution_result_t;

typedef struct {
    char* base_url;
    char* api_key;
    uint32_t timeout_ms;
    bool enable_streaming;
    bool enable_tracing;
    bool enable_caching;
    int max_concurrent_chains;
    int max_memory_size_kb;
    char* default_llm_model;
    char* tracing_endpoint;
    char* cache_backend_url;
} langchain_config_t;

typedef struct {
    char* id;
    char* name;
    char* description;
    char* llm_provider;
    langchain_agent_type_t type;
    double temperature;
    int max_tokens;
    char* llm_id;
    char** tool_ids;
    size_t tool_count;
    char* memory_id;
    int max_iterations;
    int max_execution_time_sec;
    bool handle_parsing_errors;
    bool verbose;
    bool is_available;
} langchain_agent_instance_t;

typedef void (*langchain_streaming_fn)(const char* chunk_text,
                                       const char* source_step,
                                       void* user_data);

typedef void (*langchain_trace_fn)(const char* trace_event,
                                   const char* trace_data_json,
                                   void* user_data);

typedef struct langchain_adapter_context_s {
    langchain_config_t config;
    langchain_chain_instance_t chains[LANGCHAIN_MAX_CHAINS];
    size_t chain_count;
    langchain_tool_def_t tools[LANGCHAIN_MAX_TOOLS];
    size_t tool_count;
    langchain_agent_instance_t agents[LANGCHAIN_MAX_AGENTS];
    size_t agent_count;
    langchain_memory_t memories[LANGCHAIN_MAX_MEMORY_ENTRIES];
    size_t memory_count;
    langchain_streaming_fn streaming_handler;
    void* streaming_user_data;
    langchain_trace_fn trace_handler;
    void* trace_user_data;
    bool is_initialized;
    uint64_t total_chains_executed;
    uint64_t total_tokens_used;
    double total_execution_time_ms;
} langchain_adapter_context_t;

typedef int (*langchain_tool_executor_fn)(const char* tool_name,
                                           const char* input_json,
                                           char** output_json,
                                           void* user_data);

langchain_config_t langchain_config_default(void);

langchain_adapter_context_t* langchain_adapter_create(const langchain_config_t* config);
void langchain_adapter_destroy(langchain_adapter_context_t* ctx);

bool langchain_adapter_is_initialized(const langchain_adapter_context_t* ctx);
const char* langchain_adapter_version(void);

int langchain_register_tool(langchain_adapter_context_t* ctx,
                             const langchain_tool_def_t* tool,
                             langchain_tool_executor_fn executor,
                             void* user_data);

int langchain_list_tools(langchain_adapter_context_t* ctx,
                         langchain_tool_def_t** tools,
                         size_t* count);

int langchain_create_chain(langchain_adapter_context_t* ctx,
                            const langchain_chain_def_t* definition,
                            langchain_chain_instance_t* instance);

int langchain_execute_chain(langchain_adapter_context_t* ctx,
                             const char* chain_id,
                             const char* input_json,
                             langchain_execution_result_t* result);

int langchain_execute_chain_streaming(langchain_adapter_context_t* ctx,
                                      const char* chain_id,
                                      const char* input_json,
                                      langchain_streaming_fn stream_handler,
                                      void* user_data);

int langchain_create_agent(langchain_adapter_context_t* ctx,
                            const langchain_agent_def_t* definition,
                            char* out_agent_id);

int langchain_agent_run(langchain_adapter_context_t* ctx,
                        const char* agent_id,
                        const char* task_input,
                        langchain_execution_result_t* result);

int langchain_create_memory(langchain_adapter_context_t* ctx,
                             langchain_memory_type_t type,
                             size_t max_entries,
                             langchain_memory_t* out_memory);

int langchain_memory_add(langchain_adapter_context_t* ctx,
                         const char* memory_id,
                         const char* role,
                         const char* content);

int langchain_memory_get(langchain_adapter_context_t* ctx,
                         const char* memory_id,
                         langchain_memory_t* snapshot);

int langchain_set_streaming_handler(langchain_adapter_context_t* ctx,
                                    langchain_streaming_fn handler,
                                    void* user_data);

int langchain_set_trace_handler(langchain_adapter_context_t* ctx,
                                langchain_trace_fn handler,
                                void* user_data);

int langchain_get_statistics(langchain_adapter_context_t* ctx,
                             char* stats_json,
                             size_t buffer_size);

const proto_adapter_t* langchain_get_protocol_adapter(void);

void langchain_tool_def_destroy(langchain_tool_def_t* tool);
void langchain_chain_def_destroy(langchain_chain_def_t* chain);
void langchain_chain_instance_destroy(langchain_chain_instance_t* instance);
void langchain_agent_def_destroy(langchain_agent_def_t* agent);
void langchain_memory_destroy(langchain_memory_t* mem);
void langchain_execution_result_destroy(langchain_execution_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LANGCHAIN_ADAPTER_H */
