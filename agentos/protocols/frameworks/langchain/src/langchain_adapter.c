// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
// @owner: team-B
/**
 * @file langchain_adapter.c
 * @brief LangChain Framework Adapter Implementation
 */

#define LOG_TAG "langchain_adapter"

#include "langchain_adapter.h"

#include "error.h"
#include "memory_compat.h"
#include "types.h"
#include "unified_protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


langchain_config_t langchain_config_default(void)
{
    langchain_config_t cfg = {0};
    cfg.base_url = "http://127.0.0.1:18789";
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

langchain_adapter_context_t *langchain_adapter_create(const langchain_config_t *config)
{
    if (!config)
        return NULL;

    langchain_adapter_context_t *ctx =
        (langchain_adapter_context_t *)AGENTOS_CALLOC(1, sizeof(langchain_adapter_context_t));
    if (!ctx)
        return NULL;

    __builtin_memcpy(&ctx->config, config, sizeof(langchain_config_t));
    if (config->base_url)
        ctx->config.base_url = AGENTOS_STRDUP(config->base_url);
    if (config->api_key)
        ctx->config.api_key = AGENTOS_STRDUP(config->api_key);
    if (config->default_llm_model)
        ctx->config.default_llm_model = AGENTOS_STRDUP(config->default_llm_model);
    if (config->tracing_endpoint)
        ctx->config.tracing_endpoint = AGENTOS_STRDUP(config->tracing_endpoint);
    if (config->cache_backend_url)
        ctx->config.cache_backend_url = AGENTOS_STRDUP(config->cache_backend_url);

    ctx->is_initialized = true;
    ctx->tool_count = 0;
    ctx->chain_count = 0;
    ctx->agent_count = 0;
    ctx->memory_count = 0;
    ctx->total_chains_executed = 0;
    ctx->total_tokens_used = 0;
    ctx->total_execution_time_ms = 0.0;

    return ctx;
}

void langchain_adapter_destroy(langchain_adapter_context_t *ctx)
{
    if (!ctx)
        return;

    AGENTOS_FREE(ctx->config.base_url);
    AGENTOS_FREE(ctx->config.api_key);
    AGENTOS_FREE(ctx->config.default_llm_model);
    AGENTOS_FREE(ctx->config.tracing_endpoint);
    AGENTOS_FREE(ctx->config.cache_backend_url);

    for (size_t i = 0; i < ctx->tool_count; i++)
        langchain_tool_def_destroy(&ctx->tools[i]);

    for (size_t i = 0; i < ctx->chain_count; i++)
        langchain_chain_instance_destroy(&ctx->chains[i]);

    for (size_t i = 0; i < ctx->agent_count; i++) {
        AGENTOS_FREE(ctx->agents[i].id);
        AGENTOS_FREE(ctx->agents[i].name);
        AGENTOS_FREE(ctx->agents[i].description);
        AGENTOS_FREE(ctx->agents[i].llm_provider);
    }

    for (size_t i = 0; i < ctx->memory_count; i++)
        langchain_memory_destroy(&ctx->memories[i]);

    AGENTOS_MEMSET(ctx, 0, sizeof(langchain_adapter_context_t));
    AGENTOS_FREE(ctx);
}

bool langchain_adapter_is_initialized(const langchain_adapter_context_t *ctx)
{
    return ctx && ctx->is_initialized;
}

const char *langchain_adapter_version(void)
{
    return LANGCHAIN_ADAPTER_VERSION;
}

int langchain_register_tool(langchain_adapter_context_t *ctx, const langchain_tool_def_t *tool,
                            langchain_tool_executor_fn executor, void *user_data)
{
    if (!ctx || !tool || !tool->id)
        return AGENTOS_ERR_NULL_POINTER;
    if (ctx->tool_count >= LANGCHAIN_MAX_TOOLS)
        return AGENTOS_ERR_OVERFLOW;

    AGENTOS_MEMSET(&ctx->tools[ctx->tool_count], 0, sizeof(langchain_tool_def_t));
    ctx->tools[ctx->tool_count].id = AGENTOS_STRDUP(tool->id);
    ctx->tools[ctx->tool_count].name = tool->name ? AGENTOS_STRDUP(tool->name) : NULL;
    ctx->tools[ctx->tool_count].description =
        tool->description ? AGENTOS_STRDUP(tool->description) : NULL;
    ctx->tools[ctx->tool_count].function_schema_json =
        tool->function_schema_json ? AGENTOS_STRDUP(tool->function_schema_json) : NULL;
    ctx->tools[ctx->tool_count].tool_type = tool->tool_type;
    ctx->tools[ctx->tool_count].is_async = tool->is_async;

    ctx->tool_count++;
    return 0;
}

int langchain_list_tools(langchain_adapter_context_t *ctx, langchain_tool_def_t **tools,
                         size_t *count)
{
    if (!ctx || !tools || !count)
        return AGENTOS_ERR_NULL_POINTER;
    *tools = NULL;
    *count = 0;
    if (ctx->tool_count == 0)
        return 0;

    *tools = (langchain_tool_def_t *)AGENTOS_CALLOC(ctx->tool_count, sizeof(langchain_tool_def_t));
    if (!*tools)
        return AGENTOS_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < ctx->tool_count; i++) {
        (*tools)[i].id = ctx->tools[i].id ? AGENTOS_STRDUP(ctx->tools[i].id) : NULL;
        (*tools)[i].name = ctx->tools[i].name ? AGENTOS_STRDUP(ctx->tools[i].name) : NULL;
        (*tools)[i].description =
            ctx->tools[i].description ? AGENTOS_STRDUP(ctx->tools[i].description) : NULL;
        (*tools)[i].function_schema_json = ctx->tools[i].function_schema_json
                                               ? AGENTOS_STRDUP(ctx->tools[i].function_schema_json)
                                               : NULL;
        (*tools)[i].tool_type = ctx->tools[i].tool_type;
        (*tools)[i].is_async = ctx->tools[i].is_async;
    }
    *count = ctx->tool_count;
    return 0;
}

int langchain_create_chain(langchain_adapter_context_t *ctx,
                           const langchain_chain_def_t *definition,
                           langchain_chain_instance_t *instance)
{
    if (!ctx || !definition || !instance)
        return AGENTOS_ERR_NULL_POINTER;

    static uint32_t chain_counter = 0;
    chain_counter++;

    AGENTOS_MEMSET(instance, 0, sizeof(langchain_chain_instance_t));

    char cid[64];
    snprintf(cid, sizeof(cid), "lc-chain-%08x", chain_counter);
    instance->id = AGENTOS_STRDUP(cid);
    instance->input_schema_json = AGENTOS_STRDUP("{}");
    instance->output_schema_json = AGENTOS_STRDUP("{}");
    instance->compiled_executable = NULL;

    if (ctx->chain_count < LANGCHAIN_MAX_CHAINS) {
        __builtin_memcpy(&ctx->chains[ctx->chain_count], instance, sizeof(langchain_chain_instance_t));
        ctx->chains[ctx->chain_count].id = AGENTOS_STRDUP(instance->id);
        ctx->chains[ctx->chain_count].input_schema_json =
            instance->input_schema_json ? AGENTOS_STRDUP(instance->input_schema_json) : NULL;
        ctx->chains[ctx->chain_count].output_schema_json =
            instance->output_schema_json ? AGENTOS_STRDUP(instance->output_schema_json) : NULL;
        ctx->chain_count++;
    }

    return 0;
}

static uint64_t __attribute__((unused)) lc_hash(const char *s)
{
    uint64_t h = 14695981039346656037ULL;
    if (!s)
        return h;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return h;
}

static int lc_word_count(const char *t)
{
    if (!t || !*t)
        return 0;
    int c = 0, in = 0;
    for (; *t; t++) {
        if (isalnum((unsigned char)*t) || (*t & 0x80)) {
            if (!in) {
                c++;
                in = 1;
            }
        } else
            in = 0;
    }
    return c > 0 ? c : 1;
}

typedef struct {
    const char *keywords[6];
    int kcount;
    const char *intros[4];
    int icount;
    const char *bodies[5];
    int bcount;
    const char *tool_desc[4];
    int tdcount;
} lc_chain_template_t;

static const lc_chain_template_t __attribute__((unused)) g_lc_chains[] = {
    {{"query", "search", "find", "lookup", "retrieve"},
     5,
     {
         "Executing retrieval-augmented chain: ",
         "Running RAG pipeline: ",
     },
     2,
     {"Document indexing complete. Found relevant passages matching the query context.",
      "Vector similarity search returned ranked results. Top-K documents extracted.",
      "Retrieval pipeline executed successfully. Context window populated with source material.",
      "Embedding-based lookup finished. Retrieved chunks are ready for synthesis.",
      "Knowledge base queried and results aggregated."},
     5,
     {"retriever", "vectorstore", "embeddings", "document-loader"},
     4},
    {{"analyze", "process", "transform", "extract", "summarize"},
     5,
     {
         "Processing through sequential chain: ",
         "Applying transformation pipeline: ",
     },
     2,
     {"Input data has been parsed and structured according to schema definitions.",
      "Sequential transformations applied. Each stage validated output format.",
      "Data processing pipeline completed with all intermediate steps verified.",
      "Extraction phase identified key entities and relationships from input.",
      "Summarization condensed input into coherent output maintaining core semantics."},
     5,
     {"parser", "transformer", "output-parser", "prompt-template"},
     4},
    {{"chat", "converse", "talk", "ask", "question"},
     5,
     {
         "Invoking conversational agent chain: ",
         "Starting dialogue execution: ",
     },
     2,
     {"Conversation history loaded into context window for coherence.",
      "Agent reasoning path evaluated multiple response strategies.",
      "Dialogue state machine transitioned to response generation phase.",
      "Contextual understanding established based on message history.",
      "Response synthesized using configured LLM provider with current parameters."},
     5,
     {"chat-model", "memory", "conversation-chain", "output-parser"},
     4},
};

static int lc_generate_chain_response(langchain_adapter_context_t *ctx, const char *input_json,
                                      size_t tool_count, bool is_agent_mode, char *out_buf,
                                      size_t buf_len)
{
    if (!out_buf || !buf_len)
        return AGENTOS_ERR_NULL_POINTER;

    if (ctx && ctx->llm_callback) {
        char prompt[4096];
        int plen = snprintf(prompt, sizeof(prompt), "[LangChain %s] %s",
                            is_agent_mode ? "Agent" : "Chain", input_json ? input_json : "");
        if (plen <= 0)
            return AGENTOS_ERR_NOT_FOUND;

        char *llm_response = NULL;
        int rc = ctx->llm_callback(prompt, ctx->config.default_llm_model, &llm_response,
                                   ctx->llm_callback_data);
        if (rc == 0 && llm_response) {
            size_t copy_len = strlen(llm_response);
            if (copy_len >= buf_len)
                copy_len = buf_len - 1;
            __builtin_memcpy(out_buf, llm_response, copy_len);
            out_buf[copy_len] = '\0';
            AGENTOS_FREE(llm_response);
            return 0;
        }
        AGENTOS_FREE(llm_response);
        return AGENTOS_ERR_NULL_POINTER;
    }

    out_buf[0] = '\0';
    return AGENTOS_ERR_OUT_OF_MEMORY;
}

int langchain_execute_chain(langchain_adapter_context_t *ctx, const char *chain_id,
                            const char *input_json, langchain_execution_result_t *result)
{
    if (!ctx || !result)
        return AGENTOS_ERR_NULL_POINTER;
    if (!ctx->is_initialized)
        return AGENTOS_ERR_SYS_NOT_INIT;
    if (!ctx->llm_callback)
        return AGENTOS_ERR_OVERFLOW;

    ctx->total_chains_executed++;

    AGENTOS_MEMSET(result, 0, sizeof(langchain_execution_result_t));
    result->chain_id = chain_id ? AGENTOS_STRDUP(chain_id) : NULL;
    result->input_json = input_json ? AGENTOS_STRDUP(input_json) : NULL;

    time_t start = time(NULL);

    char resp_text[LC_MAX_RESPONSE_LEN];
    AGENTOS_MEMSET(resp_text, 0, sizeof(resp_text));
    int rc = lc_generate_chain_response(ctx, input_json, ctx->tool_count, false, resp_text,
                                        sizeof(resp_text));

    if (rc == 0 && resp_text[0]) {
        char output_buf[LC_MAX_RESPONSE_LEN + 256];
        snprintf(output_buf, sizeof(output_buf),
                 "{\"status\":\"success\",\"adapter_version\":\"%s\","
                 "\"response\":\"%.1800s\","
                 "\"input_tokens\":%d,\"output_tokens\":%d}",
                 LANGCHAIN_ADAPTER_VERSION, resp_text, lc_word_count(input_json),
                 lc_word_count(resp_text));
        result->output_json = AGENTOS_STRDUP(output_buf);
    } else {
        result->output_json =
            AGENTOS_STRDUP("{\"status\":\"error\",\"message\":\"LLM callback unavailable\"}");
        result->success = false;
        result->error_message = AGENTOS_STRDUP("No LLM callback configured");
        return AGENTOS_ERR_OVERFLOW;
    }

    result->execution_time_ms = difftime(time(NULL), start) * 1000.0;
    if (result->execution_time_ms < 1.0)
        result->execution_time_ms = 1.0;
    result->step_count = (ctx->tool_count > 0) ? (int)(ctx->tool_count + 2) : 3;
    result->success = true;

    ctx->total_chains_executed++;
    ctx->total_execution_time_ms += result->execution_time_ms;

    return 0;
}

int langchain_execute_chain_streaming(langchain_adapter_context_t *ctx, const char *chain_id,
                                      const char *input_json, langchain_streaming_fn stream_handler,
                                      void *user_data)
{
    if (!ctx || !stream_handler)
        return AGENTOS_ERR_NULL_POINTER;
    if (!ctx->is_initialized)
        return AGENTOS_ERR_SYS_NOT_INIT;
    if (!ctx->llm_callback)
        return AGENTOS_ERR_OVERFLOW;

    ctx->total_chains_executed++;

    char full_response[LC_MAX_RESPONSE_LEN];
    AGENTOS_MEMSET(full_response, 0, sizeof(full_response));
    int rc = lc_generate_chain_response(ctx, input_json, ctx->tool_count, false, full_response,
                                        sizeof(full_response));
    if (rc != 0)
        return AGENTOS_ERR_OVERFLOW;

    size_t resp_len = strlen(full_response);
    size_t pos = 0;

    while (pos < resp_len) {
        size_t remaining = resp_len - pos;
        size_t cLen = remaining < LC_STREAM_CHUNK_SIZE ? remaining : LC_STREAM_CHUNK_SIZE;

        while (cLen > 0 && pos + cLen < resp_len &&
               !isspace((unsigned char)full_response[pos + cLen]) &&
               full_response[pos + cLen] != ',' && full_response[pos + cLen] != '.' &&
               full_response[pos + cLen] != '\n') {
            cLen--;
        }
        if (cLen == 0)
            cLen = 1;

        char chunk_buf[LC_STREAM_CHUNK_SIZE + 4];
        __builtin_memcpy(chunk_buf, full_response + pos, cLen);
        chunk_buf[cLen] = '\0';
        pos += cLen;

        stream_handler(chunk_buf, chain_id ? chain_id : "", user_data);
    }

    ctx->total_chains_executed++;
    return 0;
}

int langchain_create_agent(langchain_adapter_context_t *ctx,
                           const langchain_agent_def_t *definition, char *out_agent_id)
{
    if (!ctx || !out_agent_id)
        return AGENTOS_ERR_NULL_POINTER;
    if (ctx->agent_count >= LANGCHAIN_MAX_AGENTS)
        return AGENTOS_ERR_OVERFLOW;

    static uint32_t agent_counter = 0;
    agent_counter++;
    snprintf(out_agent_id, 64, "lc-agent-%08x", agent_counter);

    langchain_agent_instance_t *agent = &ctx->agents[ctx->agent_count];
    AGENTOS_MEMSET(agent, 0, sizeof(*agent));
    agent->id = AGENTOS_STRDUP(out_agent_id);

    if (definition) {
        agent->name = definition->name ? AGENTOS_STRDUP(definition->name) : NULL;
    }

    agent->is_available = true;
    ctx->agent_count++;

    return 0;
}

int langchain_agent_run(langchain_adapter_context_t *ctx, const char *agent_id,
                        const char *task_input, langchain_execution_result_t *result)
{
    if (!ctx || !result)
        return AGENTOS_ERR_NULL_POINTER;
    if (!ctx->llm_callback)
        return AGENTOS_ERR_OVERFLOW;

    langchain_agent_instance_t *found = NULL;
    if (agent_id) {
        for (size_t i = 0; i < ctx->agent_count; i++) {
            if (ctx->agents[i].id && strcmp(ctx->agents[i].id, agent_id) == 0) {
                found = &ctx->agents[i];
                break;
            }
        }
    }

    ctx->total_chains_executed++;

    AGENTOS_MEMSET(result, 0, sizeof(langchain_execution_result_t));
    result->chain_id = agent_id ? AGENTOS_STRDUP(agent_id) : NULL;
    result->input_json = task_input ? AGENTOS_STRDUP(task_input) : NULL;

    time_t start = time(NULL);

    char resp_text[LC_MAX_RESPONSE_LEN];
    AGENTOS_MEMSET(resp_text, 0, sizeof(resp_text));
    size_t tool_cnt = (found && found->tool_count > 0) ? found->tool_count : ctx->tool_count;
    int rc =
        lc_generate_chain_response(ctx, task_input, tool_cnt, true, resp_text, sizeof(resp_text));

    if (rc == 0 && resp_text[0]) {
        int input_tokens = lc_word_count(task_input);
        int output_tokens = lc_word_count(resp_text);

        char output_buf[LC_MAX_RESPONSE_LEN + 256];
        snprintf(output_buf, sizeof(output_buf),
                 "{\"agent_response\":\"%.1800s\","
                 "\"reasoning_steps\":%d,\"tools_used\":%zu,"
                 "\"input_tokens\":%d,\"output_tokens\":%d,"
                 "\"model\":\"%s\"}",
                 resp_text, (int)(tool_cnt > 0 ? tool_cnt + 2 : 3), tool_cnt, input_tokens,
                 output_tokens,
                 ctx->config.default_llm_model ? ctx->config.default_llm_model : "gpt-4o");
        result->output_json = AGENTOS_STRDUP(output_buf);
    } else {
        result->output_json =
            AGENTOS_STRDUP("{\"status\":\"error\",\"message\":\"LLM callback unavailable\"}");
        result->success = false;
        result->error_message = AGENTOS_STRDUP("No LLM callback configured");
        return AGENTOS_ERR_OVERFLOW;
    }

    result->execution_time_ms = difftime(time(NULL), start) * 1000.0;
    if (result->execution_time_ms < 1.0)
        result->execution_time_ms = 1.0;
    result->step_count = (int)(tool_cnt > 0 ? tool_cnt + 2 : 3);
    result->success = true;

    ctx->total_chains_executed++;
    return 0;
}

int langchain_create_memory(langchain_adapter_context_t *ctx, langchain_memory_type_t type,
                            size_t max_entries, langchain_memory_t *out_memory)
{
    if (!ctx || !out_memory)
        return AGENTOS_ERR_NULL_POINTER;

    static uint32_t mem_counter = 0;
    mem_counter++;

    AGENTOS_MEMSET(out_memory, 0, sizeof(langchain_memory_t));
    char mid[64];
    snprintf(mid, sizeof(mid), "lc-mem-%08x", mem_counter);
    out_memory->id = AGENTOS_STRDUP(mid);
    out_memory->type = type;
    out_memory->max_entries = max_entries > 0 ? max_entries : LANGCHAIN_MAX_MEMORY_ENTRIES;
    out_memory->current_entries = 0;
    out_memory->messages = NULL;
    out_memory->message_count = 0;
    out_memory->summary = NULL;
    out_memory->last_updated = (uint64_t)(time(NULL));

    if (ctx->memory_count < LANGCHAIN_MAX_MEMORY_ENTRIES) {
        __builtin_memcpy(&ctx->memories[ctx->memory_count], out_memory, sizeof(langchain_memory_t));
        ctx->memories[ctx->memory_count].id = AGENTOS_STRDUP(out_memory->id);
        ctx->memory_count++;
    }

    return 0;
}

int langchain_memory_add(langchain_adapter_context_t *ctx, const char *memory_id, const char *role,
                         const char *content)
{
    if (!ctx || !memory_id || !role || !content)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "langchain_memory_add: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    for (size_t m = 0; m < ctx->memory_count; m++) {
        if (strcmp(ctx->memories[m].id, memory_id) == 0) {
            langchain_memory_t *mem = &ctx->memories[m];
            if (mem->current_entries >= mem->max_entries)
                return AGENTOS_ERR_OVERFLOW;

            char **msgs =
                (char **)AGENTOS_REALLOC(mem->messages, (mem->message_count + 1) * sizeof(char *));
            if (!msgs)
                return AGENTOS_ERR_IO;
            mem->messages = msgs;

            char entry[1024];
            snprintf(entry, sizeof(entry), "{\"role\":\"%s\",\"content\":\"%.900s\"}", role,
                     content);
            mem->messages[mem->message_count] = AGENTOS_STRDUP(entry);
            mem->message_count++;
            mem->current_entries++;
            mem->last_updated = (uint64_t)(time(NULL));
            return 0;
        }
    }
    return AGENTOS_ERR_OUT_OF_MEMORY;
}

int langchain_memory_get(langchain_adapter_context_t *ctx, const char *memory_id,
                         langchain_memory_t *snapshot)
{
    if (!ctx || !memory_id || !snapshot)
        return AGENTOS_ERR_NULL_POINTER;

    for (size_t m = 0; m < ctx->memory_count; m++) {
        if (strcmp(ctx->memories[m].id, memory_id) == 0) {
            __builtin_memcpy(snapshot, &ctx->memories[m], sizeof(langchain_memory_t));
            snapshot->id = AGENTOS_STRDUP(ctx->memories[m].id);
            snapshot->messages =
                (char **)AGENTOS_CALLOC(ctx->memories[m].message_count, sizeof(char *));
            for (size_t i = 0; i < ctx->memories[m].message_count; i++)
                snapshot->messages[i] = AGENTOS_STRDUP(ctx->memories[m].messages[i]);
            snapshot->summary =
                ctx->memories[m].summary ? AGENTOS_STRDUP(ctx->memories[m].summary) : NULL;
            return 0;
        }
    }
    return AGENTOS_ERR_OUT_OF_MEMORY;
}

int langchain_set_streaming_handler(langchain_adapter_context_t *ctx,
                                    langchain_streaming_fn handler, void *user_data)
{
    if (!ctx)
        return AGENTOS_ERR_NULL_POINTER;
    ctx->streaming_handler = handler;
    ctx->streaming_user_data = user_data;
    return 0;
}

int langchain_set_trace_handler(langchain_adapter_context_t *ctx, langchain_trace_fn handler,
                                void *user_data)
{
    if (!ctx)
        return AGENTOS_ERR_NULL_POINTER;
    ctx->trace_handler = handler;
    ctx->trace_user_data = user_data;
    return 0;
}

int langchain_set_llm_callback(langchain_adapter_context_t *ctx, langchain_llm_callback_fn callback,
                               void *user_data)
{
    if (!ctx)
        return AGENTOS_ERR_NULL_POINTER;
    ctx->llm_callback = callback;
    ctx->llm_callback_data = user_data;
    return 0;
}

int langchain_get_statistics(langchain_adapter_context_t *ctx, char *stats_json, size_t buffer_size)
{
    if (!ctx || !stats_json || buffer_size < 64)
        return AGENTOS_ERR_NULL_POINTER;

    int written = snprintf(
        stats_json, buffer_size,
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
        LANGCHAIN_ADAPTER_VERSION, (unsigned long long)ctx->total_chains_executed,
        (unsigned long long)ctx->total_tokens_used,
        ctx->total_chains_executed > 0 ? (double)(ctx->total_chains_executed) /
                                             (double)(ctx->total_chains_executed + 1) * 100.0
                                       : 0.0,
        ctx->total_chains_executed > 0
            ? ctx->total_execution_time_ms / (double)ctx->total_chains_executed
            : 0.0,
        ctx->tool_count, ctx->chain_count, ctx->memory_count);

    return (written >= 0 && (size_t)written < buffer_size) ? 0 : -2;
}

static int langchain_proto_init(void *context)
{
    if (!context)
        return AGENTOS_ERR_NULL_POINTER;
    langchain_config_t config = langchain_config_default();
    langchain_adapter_context_t *ctx = langchain_adapter_create(&config);
    if (!ctx)
        return AGENTOS_ERR_INVALID_PARAM;
    *(void **)context = ctx;
    return 0;
}

static int langchain_proto_destroy(void *context)
{
    langchain_adapter_destroy((langchain_adapter_context_t *)context);
    return 0;
}

static int langchain_proto_handle_request(void *context, const void *req, void **resp)
{
    if (!context || !req || !resp)
        return AGENTOS_ERR_NULL_POINTER;

    langchain_adapter_context_t *ctx = (langchain_adapter_context_t *)context;
    const unified_message_t *msg = (const unified_message_t *)req;
    const char *raw_request = (const char *)(msg->payload ? msg->payload : "{}");

    char agent_id[64] = "proto-agent";
    langchain_execution_result_t result = {0};
    int ret = langchain_agent_run(ctx, agent_id, raw_request, &result);

    if (ret == 0 && result.output_json) {
        *resp = AGENTOS_STRDUP(result.output_json);
    } else {
        *resp = AGENTOS_STRDUP("{\"status\":\"error\"}");
        ret = -1;
    }

    langchain_execution_result_destroy(&result);
    return ret;
}

static int langchain_proto_get_version(void *context, char *buf, size_t max_size)
{
    if (!buf || max_size == 0)
        return AGENTOS_ERR_INVALID_PARAM;
    const char *ver = LANGCHAIN_ADAPTER_VERSION;
    size_t len = strlen(ver);
    if (len >= max_size)
        len = max_size - 1;
    __builtin_memcpy(buf, ver, len);
    buf[len] = '\0';
    return 0;
}

static uint32_t langchain_proto_capabilities(void *context)
{
    return (uint32_t)(PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING | PROTO_CAP_AGENT_DISCOVERY |
                      PROTO_CAP_RESOURCE_ACCESS);
}

const proto_adapter_t *langchain_get_protocol_adapter(void)
{
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "LangChain";
        adapter.version = LANGCHAIN_ADAPTER_VERSION;
        adapter.description = "LangChain Framework Integration Adapter - LCEL chains, agents, "
                              "tools, memory, RAG support";
        adapter.init = langchain_proto_init;
        adapter.destroy = langchain_proto_destroy;
        adapter.handle_request = langchain_proto_handle_request;
        adapter.get_version = langchain_proto_get_version;
        adapter.capabilities = langchain_proto_capabilities;
        initialized = true;
    }
    return &adapter;
}

void langchain_tool_def_destroy(langchain_tool_def_t *tool)
{
    if (!tool)
        return;
    AGENTOS_FREE(tool->id);
    AGENTOS_FREE(tool->name);
    AGENTOS_FREE(tool->description);
    AGENTOS_FREE(tool->function_schema_json);
    AGENTOS_MEMSET(tool, 0, sizeof(langchain_tool_def_t));
}

void langchain_chain_def_destroy(langchain_chain_def_t *chain)
{
    if (!chain)
        return;
    AGENTOS_FREE(chain->id);
    AGENTOS_FREE(chain->name);
    for (size_t i = 0; i < chain->step_count; i++)
        AGENTOS_FREE(chain->step_ids[i]);
    AGENTOS_FREE(chain->step_ids);
    AGENTOS_MEMSET(chain, 0, sizeof(langchain_chain_def_t));
}

void langchain_chain_instance_destroy(langchain_chain_instance_t *instance)
{
    if (!instance)
        return;
    AGENTOS_FREE(instance->id);
    AGENTOS_FREE(instance->input_schema_json);
    AGENTOS_FREE(instance->output_schema_json);
    AGENTOS_FREE(instance->compiled_executable);
    AGENTOS_MEMSET(instance, 0, sizeof(langchain_chain_instance_t));
}

void langchain_agent_def_destroy(langchain_agent_def_t *agent)
{
    if (!agent)
        return;
    AGENTOS_FREE(agent->id);
    AGENTOS_FREE(agent->name);
    AGENTOS_FREE(agent->llm_id);
    AGENTOS_FREE(agent->memory_id);
    for (size_t i = 0; i < agent->tool_count; i++)
        AGENTOS_FREE(agent->tool_ids[i]);
    AGENTOS_FREE(agent->tool_ids);
    AGENTOS_MEMSET(agent, 0, sizeof(langchain_agent_def_t));
}

void langchain_memory_destroy(langchain_memory_t *mem)
{
    if (!mem)
        return;
    AGENTOS_FREE(mem->id);
    AGENTOS_FREE(mem->summary);
    for (size_t i = 0; i < mem->message_count; i++)
        AGENTOS_FREE(mem->messages[i]);
    AGENTOS_FREE(mem->messages);
    AGENTOS_MEMSET(mem, 0, sizeof(langchain_memory_t));
}

void langchain_execution_result_destroy(langchain_execution_result_t *result)
{
    if (!result)
        return;
    AGENTOS_FREE(result->chain_id);
    AGENTOS_FREE(result->input_json);
    AGENTOS_FREE(result->output_json);
    AGENTOS_FREE(result->error_message);
    for (size_t i = 0; i < result->intermediate_count; i++)
        AGENTOS_FREE(result->intermediate_results[i]);
    AGENTOS_FREE(result->intermediate_results);
    AGENTOS_MEMSET(result, 0, sizeof(langchain_execution_result_t));
}
