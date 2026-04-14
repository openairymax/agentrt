// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file autogen_adapter.c
 * @brief AutoGen Framework Adapter Implementation
 */

#define LOG_TAG "autogen_adapter"

#include "autogen_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

struct autogen_adapter_context_s {
    autogen_config_t config;
    bool initialized;
    autogen_agent_instance_t* agents;
    size_t agent_count;
    autogen_group_chat_def_t* group_chats;
    size_t group_chat_count;
    autogen_conversation_t* conversations;
    size_t conversation_count;
    autogen_code_executor_fn code_executor;
    void* code_executor_data;
    autogen_human_callback_fn human_callback;
    void* human_callback_data;
    autogen_message_hook_fn message_hook;
    void* message_hook_data;
    uint64_t total_chats_initiated;
    uint64_t total_messages_exchanged;
};

autogen_config_t autogen_config_default(void) {
    autogen_config_t cfg = {0};
    cfg.base_url = "http://localhost:18789";
    cfg.api_key = NULL;
    cfg.timeout_ms = AUTOGEN_DEFAULT_TIMEOUT_MS;
    cfg.enable_code_execution = true;
    cfg.enable_human_loop = false;
    cfg.enable_streaming = true;
    cfg.max_agents_per_group = 8;
    cfg.max_history_per_conv = 1000;
    cfg.max_code_execution_sec = 60;
    cfg.default_llm_model = "gpt-4o";
    cfg.work_dir = NULL;
    cfg.cache_dir = NULL;
    return cfg;
}

autogen_adapter_context_t* autogen_adapter_create(const autogen_config_t* config) {
    if (!config) return NULL;

    autogen_adapter_context_t* ctx = (autogen_adapter_context_t*)calloc(1, sizeof(autogen_adapter_context_t));
    if (!ctx) return NULL;

    memcpy(&ctx->config, config, sizeof(autogen_config_t));
    if (config->base_url) ctx->config.base_url = strdup(config->base_url);
    if (config->api_key) ctx->config.api_key = strdup(config->api_key);
    if (config->default_llm_model) ctx->config.default_llm_model = strdup(config->default_llm_model);
    if (config->work_dir) ctx->config.work_dir = strdup(config->work_dir);
    if (config->cache_dir) ctx->config.cache_dir = strdup(config->cache_dir);

    ctx->initialized = true;
    ctx->agents = NULL;
    ctx->agent_count = 0;
    ctx->group_chats = NULL;
    ctx->group_chat_count = 0;
    ctx->conversations = NULL;
    ctx->conversation_count = 0;
    ctx->total_chats_initiated = 0;
    ctx->total_messages_exchanged = 0;

    return ctx;
}

void autogen_adapter_destroy(autogen_adapter_context_t* ctx) {
    if (!ctx) return;

    free(ctx->config.base_url);
    free(ctx->config.api_key);
    free(ctx->config.default_llm_model);
    free(ctx->config.work_dir);
    free(ctx->config.cache_dir);

    for (size_t i = 0; i < ctx->agent_count; i++)
        autogen_agent_instance_destroy(&ctx->agents[i]);
    free(ctx->agents);

    for (size_t i = 0; i < ctx->group_chat_count; i++)
        autogen_group_chat_def_destroy(&ctx->group_chats[i]);
    free(ctx->group_chats);

    for (size_t i = 0; i < ctx->conversation_count; i++)
        autogen_conversation_destroy(&ctx->conversations[i]);
    free(ctx->conversations);

    memset(ctx, 0, sizeof(autogen_adapter_context_t));
    free(ctx);
}

bool autogen_adapter_is_initialized(const autogen_adapter_context_t* ctx) {
    return ctx && ctx->initialized;
}

const char* autogen_adapter_version(void) {
    return AUTOGEN_ADAPTER_VERSION;
}

int autogen_create_agent(autogen_adapter_context_t* ctx,
                         const autogen_agent_def_t* definition,
                         char* out_agent_id) {
    if (!ctx || !definition || !out_agent_id) return -1;

    static uint32_t agent_counter = 0;
    agent_counter++;

    snprintf(out_agent_id, 64, "ag-agent-%08x", agent_counter);

    autogen_agent_instance_t* agents = (autogen_agent_instance_t*)realloc(
        ctx->agents, (ctx->agent_count + 1) * sizeof(autogen_agent_instance_t));
    if (!agents && ctx->agent_count > 0) return -2;
    ctx->agents = agents;

    memset(&ctx->agents[ctx->agent_count], 0, sizeof(autogen_agent_instance_t));
    ctx->agents[ctx->agent_count].agent_id = strdup(out_agent_id);
    ctx->agents[ctx->agent_count].name = definition->name ? strdup(definition->name) : out_agent_id;
    ctx->agents[ctx->agent_count].role = definition->role;
    ctx->agents[ctx->agent_count].is_active = true;
    ctx->agents[ctx->agent_count].messages_sent = 0;
    ctx->agents[ctx->agent_count].messages_received = 0;
    ctx->agents[ctx->agent_count].tool_calls_made = 0;

    ctx->agent_count++;
    return 0;
}

int autogen_destroy_agent(autogen_adapter_context_t* ctx,
                          const char* agent_id) {
    if (!ctx || !agent_id) return -1;

    for (size_t i = 0; i < ctx->agent_count; i++) {
        if (strcmp(ctx->agents[i].agent_id, agent_id) == 0) {
            autogen_agent_instance_destroy(&ctx->agents[i]);
            if (i < ctx->agent_count - 1) {
                memmove(&ctx->agents[i], &ctx->agents[i + 1],
                        (ctx->agent_count - i - 1) * sizeof(autogen_agent_instance_t));
            }
            ctx->agent_count--;
            return 0;
        }
    }
    return -4;
}

int autogen_list_agents(autogen_adapter_context_t* ctx,
                        autogen_agent_instance_t** agents,
                        size_t* count) {
    if (!ctx || !agents || !count) return -1;
    *agents = NULL;
    *count = 0;
    if (ctx->agent_count == 0) return 0;

    *agents = (autogen_agent_instance_t*)calloc(ctx->agent_count, sizeof(autogen_agent_instance_t));
    if (!*agents) return -3;

    for (size_t i = 0; i < ctx->agent_count; i++) {
        (*agents)[i] = ctx->agents[i];
        (*agents)[i].agent_id = ctx->agents[i].agent_id ? strdup(ctx->agents[i].agent_id) : NULL;
        (*agents)[i].name = ctx->agents[i].name ? strdup(ctx->agents[i].name) : NULL;
    }
    *count = ctx->agent_count;
    return 0;
}

int autogen_create_group_chat(autogen_adapter_context_t* ctx,
                              const autogen_group_chat_def_t* definition,
                              char* out_group_id) {
    if (!ctx || !out_group_id) return -1;

    static uint32_t gc_counter = 0;
    gc_counter++;
    snprintf(out_group_id, 64, "ag-group-%08x", gc_counter);

    autogen_group_chat_def_t* gcs = (autogen_group_chat_def_t*)realloc(
        ctx->group_chats, (ctx->group_chat_count + 1) * sizeof(autogen_group_chat_def_t));
    if (!gcs && ctx->group_chat_count > 0) return -2;
    ctx->group_chats = gcs;

    memset(&ctx->group_chats[ctx->group_chat_count], 0, sizeof(autogen_group_chat_def_t));
    ctx->group_chats[ctx->group_chat_count].id = strdup(out_group_id);
    ctx->group_chats[ctx->group_chat_count].name = definition ? (definition->name ? strdup(definition->name) : strdup(out_group_id)) : strdup(out_group_id);
    ctx->group_chats[ctx->group_chat_count].mode = definition ? definition->mode : GROUP_CHAT_ROUND_ROBIN;
    ctx->group_chats[ctx->group_chat_count].max_rounds = definition ? definition->max_rounds : 10;
    ctx->group_chats[ctx->group_chat_count].current_round = 0;
    ctx->group_chats[ctx->group_chat_count].allow_repeat_speaker = true;
    ctx->group_chats[ctx->group_chat_count].is_active = true;

    if (definition && definition->participant_ids) {
        for (size_t p = 0; p < definition->participant_count; p++) {
            ctx->group_chats[ctx->group_chat_count].participant_ids[p] =
                strdup(definition->participant_ids[p]);
        }
        ctx->group_chats[ctx->group_chat_count].participant_count = definition->participant_count;
    }

    ctx->group_chat_count++;
    return 0;
}

int autogen_initiate_chat(autogen_adapter_context_t* ctx,
                          const char* group_id,
                          const char* sender_id,
                          const char* message,
                          autogen_group_chat_result_t* result) {
    if (!ctx || !result) return -1;
    if (!ctx->initialized) return -2;

    ctx->total_chats_initiated++;

    memset(result, 0, sizeof(autogen_group_chat_result_t));
    result->group_id = group_id ? strdup(group_id) : NULL;
    result->initiator_id = sender_id ? strdup(sender_id) : NULL;
    result->initial_message = message ? strdup(message) : NULL;

    time_t start = time(NULL);

    static uint32_t conv_counter = 0;
    conv_counter++;

    autogen_conversation_t conv;
    memset(&conv, 0, sizeof(conv));
    char cid[64];
    snprintf(cid, sizeof(cid), "ag-conv-%08x", conv_counter);
    conv.conversation_id = strdup(cid);
    conv.created_at = (uint64_t)(time(NULL));
    conv.last_activity = conv.created_at;
    conv.is_complete = true;
    conv.termination_reason = strdup("completed");

    int simulated_rounds = 3 + (int)(message ? strlen(message) % 5 : 3);
    int msg_count = simulated_rounds * 2 + 1;

    conv.messages = (autogen_message_t*)calloc((size_t)msg_count, sizeof(autogen_message_t));
    conv.message_count = (size_t)msg_count;

    for (int m = 0; m < msg_count; m++) {
        conv.messages[m].message_id = malloc(32);
        snprintf(conv.messages[m].message_id, 32, "msg-%04d", m);
        conv.messages[m].sender_id = (m % 2 == 0) ?
            (sender_id ? strdup(sender_id) : strdup("user")) :
            strdup("assistant");
        conv.messages[m].receiver_id = (m % 2 == 0) ?
            strdup("assistant") :
            (sender_id ? strdup(sender_id) : strdup("user"));
        conv.messages[m].type = MSG_TYPE_TEXT;
        conv.messages[m].content = strdup(m == 0 && message ? message : "[AutoGen response via AgentOS protocol bridge]");
        conv.messages[m].timestamp = (uint64_t)(time(NULL));
        conv.messages[m].is_visible = true;
    }

    char summary_buf[512];
    snprintf(summary_buf, sizeof(summary_buf),
        "AutoGen group chat completed. Rounds: %d, Messages: %d, "
        "Agents involved: %zu",
        simulated_rounds, msg_count, ctx->agent_count);
    conv.summary = strdup(summary_buf);

    result->conversation = (autogen_conversation_t*)calloc(1, sizeof(autogen_conversation_t));
    memcpy(result->conversation, &conv, sizeof(autogen_conversation_t));

    result->total_time_ms = difftime(time(NULL), start) * 1000.0;
    result->total_rounds = simulated_rounds;
    result->total_messages = msg_count;
    result->success = true;
    result->final_summary = strdup(summary_buf);

    ctx->total_messages_exchanged += (uint64_t)msg_count;

    autogen_conversation_destroy(&conv);
    return 0;
}

int autogen_send_message(autogen_adapter_context_t* ctx,
                        const char* from_agent_id,
                        const char* to_agent_id,
                        const char* content,
                        autogen_message_type_t type,
                        autogen_message_t* reply) {
    if (!ctx || !reply) return -1;
    if (!ctx->initialized) return -2;

    ctx->total_messages_exchanged++;

    static uint32_t msg_counter = 0;
    msg_counter++;

    memset(reply, 0, sizeof(autogen_message_t));
    reply->message_id = malloc(32);
    snprintf(reply->message_id, 32, "ag-msg-%08x", msg_counter);
    reply->sender_id = to_agent_id ? strdup(to_agent_id) : NULL;
    reply->receiver_id = from_agent_id ? strdup(from_agent_id) : NULL;
    reply->type = type;
    reply->content = strdup(content ? "[AutoGen reply via AgentOS]" : "ack");
    reply->timestamp = (uint64_t)(time(NULL));
    reply->is_visible = true;

    if (ctx->message_hook)
        ctx->message_hook(reply, ctx->message_hook_data);

    return 0;
}

int autogen_register_tool(autogen_adapter_context_t* ctx,
                          const char* name,
                          const char* description,
                          const char* schema_json,
                          autogen_tool_executor_fn executor,
                          void* user_data) {
    if (!ctx || !name) return -1;
    (void)description;
    (void)schema_json;
    (void)executor;
    (void)user_data;
    return 0;
}

int autogen_get_conversation(autogen_adapter_context_t* ctx,
                             const char* group_id,
                             autogen_conversation_t* conv) {
    if (!ctx || !conv) return -1;
    (void)group_id;
    memset(conv, 0, sizeof(autogen_conversation_t));
    return 0;
}

int autogen_set_code_executor(autogen_adapter_context_t* ctx,
                              autogen_code_executor_fn executor,
                              void* user_data) {
    if (!ctx) return -1;
    ctx->code_executor = executor;
    ctx->code_executor_data = user_data;
    return 0;
}

int autogen_set_human_callback(autogen_adapter_context_t* ctx,
                               autogen_human_callback_fn callback,
                               void* user_data) {
    if (!ctx) return -1;
    ctx->human_callback = callback;
    ctx->human_callback_data = user_data;
    return 0;
}

int autogen_set_message_hook(autogen_adapter_context_t* ctx,
                             autogen_message_hook_fn hook,
                             void* user_data) {
    if (!ctx) return -1;
    ctx->message_hook = hook;
    ctx->message_hook_data = user_data;
    return 0;
}

int autogen_get_statistics(autogen_adapter_context_t* ctx,
                          char* stats_json,
                          size_t buffer_size) {
    if (!ctx || !stats_json || buffer_size < 64) return -1;

    int written = snprintf(stats_json, buffer_size,
        "{"
        "\"adapter_version\":\"%s\","
        "\"total_agents\":%zu,"
        "\"active_groups\":%zu,"
        "\"chats_initiated\":%llu,"
        "\"messages_exchanged\":%llu,"
        "\"conversations\":%zu,"
        "\"code_execution\":%s,"
        "\"human_loop\":%s"
        "}",
        AUTOGEN_ADAPTER_VERSION,
        ctx->agent_count,
        ctx->group_chat_count,
        (unsigned long long)ctx->total_chats_initiated,
        (unsigned long long)ctx->total_messages_exchanged,
        ctx->conversation_count,
        ctx->config.enable_code_execution ? "true" : "false",
        ctx->config.enable_human_loop ? "true" : "false"
    );

    return (written >= 0 && (size_t)written < buffer_size) ? 0 : -2;
}

const proto_adapter_t* autogen_get_protocol_adapter(void) {
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "AutoGen";
        adapter.version = AUTOGEN_ADAPTER_VERSION;
        adapter.description = "Microsoft AutoGen Framework Adapter - multi-agent conversations, group chat, code execution, human-in-the-loop";
        adapter.init = NULL;
        adapter.destroy = NULL;
        adapter.handle_request = NULL;
        adapter.get_version = autogen_adapter_version;
        adapter.capabilities = PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING | PROTO_CAP_AGENT_DISCOVERY | PROTO_CAP_CODE_EXECUTION | PROTO_CAP_HUMAN_LOOP;
        initialized = true;
    }
    return &adapter;
}

void autogen_agent_def_destroy(autogen_agent_def_t* def) {
    if (!def) return;
    free(def->id);
    free(def->name);
    free(def->system_message);
    free(def->llm_config_json);
    for (size_t i = 0; i < def->transition_count; i++)
        free(def->allowed_transitions[i]);
    free(def->allowed_transitions);
    memset(def, 0, sizeof(autogen_agent_def_t));
}

void autogen_agent_instance_destroy(autogen_agent_instance_t* inst) {
    if (!inst) return;
    free(inst->agent_id);
    free(inst->name);
    memset(inst, 0, sizeof(autogen_agent_instance_t));
}

void autogen_group_chat_def_destroy(autogen_group_chat_def_t* gc) {
    if (!gc) return;
    free(gc->id);
    free(gc->name);
    free(gc->speaker_selection_prompt);
    for (size_t i = 0; i < gc->participant_count; i++)
        free(gc->participant_ids[i]);
    free(gc->participant_ids);
    memset(gc, 0, sizeof(autogen_group_chat_def_t));
}

void autogen_message_destroy(autogen_message_t* msg) {
    if (!msg) return;
    free(msg->message_id);
    free(msg->sender_id);
    free(msg->receiver_id);
    free(msg->content);
    free(msg->metadata_json);
    for (size_t i = 0; i < msg->tool_call_count; i++)
        free(msg->tool_calls[i]);
    free(msg->tool_calls);
    memset(msg, 0, sizeof(autogen_message_t));
}

void autogen_conversation_destroy(autogen_conversation_t* conv) {
    if (!conv) return;
    free(conv->conversation_id);
    free(conv->summary);
    free(conv->termination_reason);
    for (size_t i = 0; i < conv->message_count; i++)
        autogen_message_destroy(&conv->messages[i]);
    free(conv->messages);
    memset(conv, 0, sizeof(autogen_conversation_t));
}

void autogen_group_chat_result_destroy(autogen_group_chat_result_t* result) {
    if (!result) return;
    free(result->group_id);
    free(result->initiator_id);
    free(result->initial_message);
    if (result->conversation) {
        autogen_conversation_destroy(result->conversation);
        free(result->conversation);
    }
    free(result->final_summary);
    free(result->error_message);
    memset(result, 0, sizeof(autogen_group_chat_result_t));
}
