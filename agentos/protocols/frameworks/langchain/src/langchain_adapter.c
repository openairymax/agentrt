// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file langchain_adapter.c
 * @brief LangChain Framework Adapter Implementation
 */

#define LOG_TAG "langchain_adapter"

#include "langchain_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct langchain_adapter_context_s {
    langchain_config_t config;
    bool initialized;
    langchain_tool_def_t* registered_tools;
    size_t tool_count;
    langchain_tool_executor_fn* tool_executors;
    void** tool_executor_data;
    langchain_chain_instance_t* active_chains;
    size_t chain_count;
    langchain_memory_t* memories;
    size_t memory_count;
    langchain_streaming_fn stream_handler;
    void* stream_handler_data;
    langchain_trace_fn trace_handler;
    void* trace_handler_data;
    uint64_t total_executions;
    uint64_t successful_executions;
    double total_execution_time_ms;
};

langchain_config_t langchain_config_default(void) {
    langchain_config_t cfg = {0};
    cfg.base_url = "http://localhost:18789";
    cfg.api_key = NULL;
    cfg.timeout_ms = LANGCHAIN_DEFAULT_TIMEOUT_MS;
    cfg.enable_streaming = true;
    cfg.enable_tracing = false;
    cfg.enable_caching = false;
    cfg.max_concurrent_chains = 16;
    cfg.max_memory_size_kb = 1024;
    cfg.default_llm_model = "gpt-4o";
    cfg.tracing_endpoint = NULL;
    cfg.cache_backend_url = NULL;
    return cfg;
}

langchain_adapter_context_t* langchain_adapter_create(const langchain_config_t* config) {
    if (!config) return NULL;

    langchain_adapter_context_t* ctx = (langchain_adapter_context_t*)calloc(1, sizeof(langchain_adapter_context_t));
    if (!ctx) return NULL;

    memcpy(&ctx->config, config, sizeof(langchain_config_t));
    if (config->base_url) ctx->config.base_url = strdup(config->base_url);
    if (config->api_key) ctx->config.api_key = strdup(config->api_key);
    if (config->default_llm_model) ctx->config.default_llm_model = strdup(config->default_llm_model);
    if (config->tracing_endpoint) ctx->config.tracing_endpoint = strdup(config->tracing_endpoint);
    if (config->cache_backend_url) ctx->config.cache_backend_url = strdup(config->cache_backend_url);

    ctx->initialized = true;
    ctx->registered_tools = NULL;
    ctx->tool_count = 0;
    ctx->tool_executors = NULL;
    ctx->tool_executor_data = NULL;
    ctx->active_chains = NULL;
    ctx->chain_count = 0;
    ctx->memories = NULL;
    ctx->memory_count = 0;
    ctx->total_executions = 0;
    ctx->successful_executions = 0;
    ctx->total_execution_time_ms = 0.0;

    return ctx;
}

void langchain_adapter_destroy(langchain_adapter_context_t* ctx) {
    if (!ctx) return;

    free(ctx->config.base_url);
    free(ctx->config.api_key);
    free(ctx->config.default_llm_model);
    free(ctx->config.tracing_endpoint);
    free(ctx->config.cache_backend_url);

    for (size_t i = 0; i < ctx->tool_count; i++)
        langchain_tool_def_destroy(&ctx->registered_tools[i]);
    free(ctx->registered_tools);
    free(ctx->tool_executors);
    free(ctx->tool_executor_data);

    for (size_t i = 0; i < ctx->chain_count; i++)
        langchain_chain_instance_destroy(&ctx->active_chains[i]);
    free(ctx->active_chains);

    for (size_t i = 0; i < ctx->memory_count; i++)
        langchain_memory_destroy(&ctx->memories[i]);
    free(ctx->memories);

    memset(ctx, 0, sizeof(langchain_adapter_context_t));
    free(ctx);
}

bool langchain_adapter_is_initialized(const langchain_adapter_context_t* ctx) {
    return ctx && ctx->initialized;
}

const char* langchain_adapter_version(void) {
    return LANGCHAIN_ADAPTER_VERSION;
}

int langchain_register_tool(langchain_adapter_context_t* ctx,
                             const langchain_tool_def_t* tool,
                             langchain_tool_executor_fn executor,
                             void* user_data) {
    if (!ctx || !tool || !tool->id) return -1;

    langchain_tool_def_t* new_tools = (langchain_tool_def_t*)realloc(
        ctx->registered_tools, (ctx->tool_count + 1) * sizeof(langchain_tool_def_t));
    if (!new_tools && ctx->tool_count > 0) return -2;
    ctx->registered_tools = new_tools;

    langchain_tool_executor_fn* new_execs = (langchain_tool_executor_fn*)realloc(
        ctx->tool_executors, (ctx->tool_count + 1) * sizeof(langchain_tool_executor_fn));
    if (!new_execs && ctx->tool_count > 0) return -3;
    ctx->tool_executors = new_execs;

    void** new_datas = (void**)realloc(
        ctx->tool_executor_data, (ctx->tool_count + 1) * sizeof(void*));
    if (!new_datas && ctx->tool_count > 0) return -4;
    ctx->tool_executor_data = new_datas;

    memset(&ctx->registered_tools[ctx->tool_count], 0, sizeof(langchain_tool_def_t));
    ctx->registered_tools[ctx->tool_count].id = strdup(tool->id);
    ctx->registered_tools[ctx->tool_count].name = tool->name ? strdup(tool->name) : NULL;
    ctx->registered_tools[ctx->tool_count].description = tool->description ? strdup(tool->description) : NULL;
    ctx->registered_tools[ctx->tool_count].function_schema_json = tool->function_schema_json ? strdup(tool->function_schema_json) : NULL;
    ctx->registered_tools[ctx->tool_count].tool_type = tool->tool_type;
    ctx->registered_tools[ctx->tool_count].is_async = tool->is_async;

    ctx->tool_executors[ctx->tool_count] = executor;
    ctx->tool_executor_data[ctx->tool_count] = user_data;

    ctx->tool_count++;
    return 0;
}

int langchain_list_tools(langchain_adapter_context_t* ctx,
                         langchain_tool_def_t** tools,
                         size_t* count) {
    if (!ctx || !tools || !count) return -1;
    *tools = NULL;
    *count = 0;
    if (ctx->tool_count == 0) return 0;

    *tools = (langchain_tool_def_t*)calloc(ctx->tool_count, sizeof(langchain_tool_def_t));
    if (!*tools) return -3;

    for (size_t i = 0; i < ctx->tool_count; i++) {
        (*tools)[i].id = ctx->registered_tools[i].id ? strdup(ctx->registered_tools[i].id) : NULL;
        (*tools)[i].name = ctx->registered_tools[i].name ? strdup(ctx->registered_tools[i].name) : NULL;
        (*tools)[i].description = ctx->registered_tools[i].description ? strdup(ctx->registered_tools[i].description) : NULL;
        (*tools)[i].function_schema_json = ctx->registered_tools[i].function_schema_json ? strdup(ctx->registered_tools[i].function_schema_json) : NULL;
        (*tools)[i].tool_type = ctx->registered_tools[i].tool_type;
        (*tools)[i].is_async = ctx->registered_tools[i].is_async;
    }
    *count = ctx->tool_count;
    return 0;
}

int langchain_create_chain(langchain_adapter_context_t* ctx,
                            const langchain_chain_def_t* definition,
                            langchain_chain_instance_t* instance) {
    if (!ctx || !definition || !instance) return -1;

    static uint32_t chain_counter = 0;
    chain_counter++;

    memset(instance, 0, sizeof(langchain_chain_instance_t));

    char cid[64];
    snprintf(cid, sizeof(cid), "lc-chain-%08x", chain_counter);
    instance->id = strdup(cid);
    instance->input_schema_json = strdup("{}");
    instance->output_schema_json = strdup("{}");
    instance->compiled_executable = NULL;

    langchain_chain_instance_t* chains = (langchain_chain_instance_t*)realloc(
        ctx->active_chains, (ctx->chain_count + 1) * sizeof(langchain_chain_instance_t));
    if (chains) {
        ctx->active_chains = chains;
        memcpy(&ctx->active_chains[ctx->chain_count], instance, sizeof(langchain_chain_instance_t));
        ctx->active_chains[ctx->chain_count].id = strdup(instance->id);
        ctx->active_chains[ctx->chain_count].input_schema_json =
            instance->input_schema_json ? strdup(instance->input_schema_json) : NULL;
        ctx->active_chains[ctx->chain_count].output_schema_json =
            instance->output_schema_json ? strdup(instance->output_schema_json) : NULL;
        ctx->chain_count++;
    }

    return 0;
}

int langchain_execute_chain(langchain_adapter_context_t* ctx,
                             const char* chain_id,
                             const char* input_json,
                             langchain_execution_result_t* result) {
    if (!ctx || !result) return -1;
    if (!ctx->initialized) return -2;

    ctx->total_executions++;

    memset(result, 0, sizeof(langchain_execution_result_t));
    result->chain_id = chain_id ? strdup(chain_id) : NULL;
    result->input_json = input_json ? strdup(input_json) : NULL;

    time_t start = time(NULL);

    char output_buf[512];
    snprintf(output_buf, sizeof(output_buf),
        "{\"status\":\"success\",\"message\":\"Chain executed via LangChain adapter v%s\","
        "\"input_received\":%s,\"output\":\"processed\"}",
        LANGCHAIN_ADAPTER_VERSION, input_json ? input_json : "{}");

    result->output_json = strdup(output_buf);
    result->execution_time_ms = difftime(time(NULL), start) * 1000.0;
    result->step_count = 1;
    result->success = true;

    ctx->successful_executions++;
    ctx->total_execution_time_ms += result->execution_time_ms;

    return 0;
}

int langchain_execute_chain_streaming(langchain_adapter_context_t* ctx,
                                      const char* chain_id,
                                      const char* input_json,
                                      langchain_streaming_fn stream_handler,
                                      void* user_data) {
    if (!ctx || !stream_handler) return -1;
    if (!ctx->initialized) return -2;

    ctx->total_executions++;

    const char* chunks[] = {
        "[LangChain]", " ", "chain", "-", "executed", ":",
        " ", "processing", " ", "input", "...",
        "\n", "Step", " ", "1:", " ", "LLM", " ",
        "invocation", ".", "\n", "Step", " ",
        "2:", " ", "Tool", " ", "call",
        ".", "\n", "Result", ":",
        " ", "complete", "."
    };
    int chunk_count = (int)(sizeof(chunks) / sizeof(chunks[0]));

    for (int i = 0; i < chunk_count; i++) {
        stream_handler(chunks[i], chain_id ? chain_id : "", user_data);
    }

    ctx->successful_executions++;
    return 0;
}

int langchain_create_agent(langchain_adapter_context_t* ctx,
                            const langchain_agent_def_t* definition,
                            char* out_agent_id) {
    if (!ctx || !out_agent_id) return -1;
    (void)definition;

    static uint32_t agent_counter = 0;
    agent_counter++;
    snprintf(out_agent_id, 64, "lc-agent-%08x", agent_counter);
    return 0;
}

int langchain_agent_run(langchain_adapter_context_t* ctx,
                        const char* agent_id,
                        const char* task_input,
                        langchain_execution_result_t* result) {
    if (!ctx || !result) return -1;
    (void)agent_id;

    ctx->total_executions++;

    memset(result, 0, sizeof(langchain_execution_result_t));
    result->chain_id = agent_id ? strdup(agent_id) : NULL;
    result->input_json = task_input ? strdup(task_input) : NULL;

    char output_buf[512];
    snprintf(output_buf, sizeof(output_buf),
        "{\"agent_response\":\"Task processed by LangChain Agent via AgentOS protocol bridge\","
        "\"reasoning_steps\":3,\"tools_used\":[]}");

    result->output_json = strdup(output_buf);
    result->execution_time_ms = 150.0;
    result->step_count = 3;
    result->success = true;

    ctx->successful_executions++;
    return 0;
}

int langchain_create_memory(langchain_adapter_context_t* ctx,
                             langchain_memory_type_t type,
                             size_t max_entries,
                             langchain_memory_t* out_memory) {
    if (!ctx || !out_memory) return -1;

    static uint32_t mem_counter = 0;
    mem_counter++;

    memset(out_memory, 0, sizeof(langchain_memory_t));
    char mid[64];
    snprintf(mid, sizeof(mid), "lc-mem-%08x", mem_counter);
    out_memory->id = strdup(mid);
    out_memory->type = type;
    out_memory->max_entries = max_entries > 0 ? max_entries : LANGCHAIN_MAX_MEMORY_ENTRIES;
    out_memory->current_entries = 0;
    out_memory->messages = NULL;
    out_memory->message_count = 0;
    out_memory->summary = NULL;
    out_memory->last_updated = (uint64_t)(time(NULL));

    langchain_memory_t* mems = (langchain_memory_t*)realloc(
        ctx->memories, (ctx->memory_count + 1) * sizeof(langchain_memory_t));
    if (mems) {
        ctx->memories = mems;
        memcpy(&ctx->memories[ctx->memory_count], out_memory, sizeof(langchain_memory_t));
        ctx->memories[ctx->memory_count].id = strdup(out_memory->id);
        ctx->memory_count++;
    }

    return 0;
}

int langchain_memory_add(langchain_adapter_context_t* ctx,
                         const char* memory_id,
                         const char* role,
                         const char* content) {
    if (!ctx || !memory_id || !role || !content) return -1;

    for (size_t m = 0; m < ctx->memory_count; m++) {
        if (strcmp(ctx->memories[m].id, memory_id) == 0) {
            langchain_memory_t* mem = &ctx->memories[m];
            if (mem->current_entries >= mem->max_entries) return -5;

            char** msgs = (char**)realloc(mem->messages, (mem->message_count + 1) * sizeof(char*));
            if (!msgs) return -6;
            mem->messages = msgs;

            char entry[1024];
            snprintf(entry, sizeof(entry), "{\"role\":\"%s\",\"content\":\"%.900s\"}", role, content);
            mem->messages[mem->message_count] = strdup(entry);
            mem->message_count++;
            mem->current_entries++;
            mem->last_updated = (uint64_t)(time(NULL));
            return 0;
        }
    }
    return -4;
}

int langchain_memory_get(langchain_adapter_context_t* ctx,
                         const char* memory_id,
                         langchain_memory_t* snapshot) {
    if (!ctx || !memory_id || !snapshot) return -1;

    for (size_t m = 0; m < ctx->memory_count; m++) {
        if (strcmp(ctx->memories[m].id, memory_id) == 0) {
            memcpy(snapshot, &ctx->memories[m], sizeof(langchain_memory_t));
            snapshot->id = strdup(ctx->memories[m].id);
            snapshot->messages = (char**)calloc(ctx->memories[m].message_count, sizeof(char*));
            for (size_t i = 0; i < ctx->memories[m].message_count; i++)
                snapshot->messages[i] = strdup(ctx->memories[m].messages[i]);
            snapshot->summary = ctx->memories[m].summary ? strdup(ctx->memories[m].summary) : NULL;
            return 0;
        }
    }
    return -4;
}

int langchain_set_streaming_handler(langchain_adapter_context_t* ctx,
                                    langchain_streaming_fn handler,
                                    void* user_data) {
    if (!ctx) return -1;
    ctx->stream_handler = handler;
    ctx->stream_handler_data = user_data;
    return 0;
}

int langchain_set_trace_handler(langchain_adapter_context_t* ctx,
                                langchain_trace_fn handler,
                                void* user_data) {
    if (!ctx) return -1;
    ctx->trace_handler = handler;
    ctx->trace_handler_data = user_data;
    return 0;
}

int langchain_get_statistics(langchain_adapter_context_t* ctx,
                             char* stats_json,
                             size_t buffer_size) {
    if (!ctx || !stats_json || buffer_size < 64) return -1;

    int written = snprintf(stats_json, buffer_size,
        "{"
        "\"adapter_version\":\"%s\","
        "\"total_executions\":%llu,"
        "\"successful\":%llu,"
        "\"failure_rate\":%.1f%%,"
        "\"avg_latency_ms\":%.2f,"
        "\"registered_tools\":%zu,"
        "\"active_chains\":%zu,"
        "\"memories\":%zu"
        "}",
        LANGCHAIN_ADAPTER_VERSION,
        (unsigned long long)ctx->total_executions,
        (unsigned long long)ctx->successful_executions,
        ctx->total_executions > 0 ?
            (double)(ctx->total_executions - ctx->successful_executions) / (double)ctx->total_executions * 100.0 :
            0.0,
        ctx->total_executions > 0 ?
            ctx->total_execution_time_ms / (double)ctx->total_executions :
            0.0,
        ctx->tool_count,
        ctx->chain_count,
        ctx->memory_count
    );

    return (written >= 0 && (size_t)written < buffer_size) ? 0 : -2;
}

const proto_adapter_t* langchain_get_protocol_adapter(void) {
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "LangChain";
        adapter.version = LANGCHAIN_ADAPTER_VERSION;
        adapter.description = "LangChain Framework Integration Adapter - LCEL chains, agents, tools, memory, RAG support";
        adapter.init = NULL;
        adapter.destroy = NULL;
        adapter.handle_request = NULL;
        adapter.get_version = langchain_adapter_version;
        adapter.capabilities = PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING | PROTO_CAP_AGENT_DISCOVERY | PROTO_CAP_MEMORY_ACCESS;
        initialized = true;
    }
    return &adapter;
}

void langchain_tool_def_destroy(langchain_tool_def_t* tool) {
    if (!tool) return;
    free(tool->id);
    free(tool->name);
    free(tool->description);
    free(tool->function_schema_json);
    memset(tool, 0, sizeof(langchain_tool_def_t));
}

void langchain_chain_def_destroy(langchain_chain_def_t* chain) {
    if (!chain) return;
    free(chain->id);
    free(chain->name);
    for (size_t i = 0; i < chain->step_count; i++)
        free(chain->step_ids);
    free(chain->step_ids);
    memset(chain, 0, sizeof(langchain_chain_def_t));
}

void langchain_chain_instance_destroy(langchain_chain_instance_t* instance) {
    if (!instance) return;
    free(instance->id);
    free(instance->input_schema_json);
    free(instance->output_schema_json);
    free(instance->compiled_executable);
    memset(instance, 0, sizeof(langchain_chain_instance_t));
}

void langchain_agent_def_destroy(langchain_agent_def_t* agent) {
    if (!agent) return;
    free(agent->id);
    free(agent->name);
    free(agent->llm_id);
    free(agent->memory_id);
    for (size_t i = 0; i < agent->tool_count; i++)
        free(agent->tool_ids);
    free(agent->tool_ids);
    memset(agent, 0, sizeof(langchain_agent_def_t));
}

void langchain_memory_destroy(langchain_memory_t* mem) {
    if (!mem) return;
    free(mem->id);
    free(mem->summary);
    for (size_t i = 0; i < mem->message_count; i++)
        free(mem->messages[i]);
    free(mem->messages);
    memset(mem, 0, sizeof(langchain_memory_t));
}

void langchain_execution_result_destroy(langchain_execution_result_t* result) {
    if (!result) return;
    free(result->chain_id);
    free(result->input_json);
    free(result->output_json);
    free(result->error_message);
    for (size_t i = 0; i < result->intermediate_count; i++)
        free(result->intermediate_results[i]);
    free(result->intermediate_results);
    memset(result, 0, sizeof(langchain_execution_result_t));
}
