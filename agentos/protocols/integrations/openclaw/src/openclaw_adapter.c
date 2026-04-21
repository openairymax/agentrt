// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file openclaw_adapter.c
 * @brief OpenClaw Platform Integration Adapter Implementation
 */

#define LOG_TAG "openclaw_adapter"

#include "openclaw_adapter.h"
#include "protocol_transformers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct openclaw_adapter_context_s {
    openclaw_config_t config;
    bool initialized;
    bool connected;
    openclaw_agent_card_t* registered_agents;
    size_t registered_agent_count;
    openclaw_session_t* active_sessions;
    size_t active_session_count;
    openclaw_tool_info_t* registered_tools;
    size_t registered_tool_count;
    openclaw_task_t* tracked_tasks;
    size_t tracked_task_count;
    openclaw_message_handler_t message_handler;
    void* message_handler_data;
    openclaw_task_handler_t task_handler;
    void* task_handler_data;
    openclaw_event_callback_t event_callback;
    void* event_callback_data;
    openclaw_status_callback_t status_callback;
    void* status_callback_data;
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t tasks_delegated;
    uint64_t tasks_completed;
    uint64_t connection_uptime_sec;
    uint64_t connect_timestamp;
    char last_error[256];
};

static openclaw_adapter_context_t* g_openclaw_instance = NULL;

openclaw_config_t openclaw_config_default(void) {
    openclaw_config_t cfg = {0};
    cfg.endpoint_url = "http://localhost:28080";
    cfg.api_key = NULL;
    cfg.organization_id = "default";
    cfg.cluster_id = "default";
    cfg.mode = OPENCLAW_MODE_STANDALONE;
    cfg.default_security_level = OPENCLAW_SECURITY_LEVEL_INTERNAL;
    cfg.heartbeat_interval_sec = OPENCLAW_HEARTBEAT_INTERVAL_SEC;
    cfg.request_timeout_ms = OPENCLAW_DEFAULT_TIMEOUT_MS;
    cfg.max_sessions = OPENCLAW_MAX_SESSIONS;
    cfg.max_context_kb = OPENCLAW_MAX_CONTEXT_KB;
    cfg.enable_multimodal = true;
    cfg.enable_tool_sharing = true;
    cfg.enable_audit_log = true;
    cfg.enable_metrics = true;
    cfg.custom_headers_json = NULL;
    cfg.reconnect_max_attempts = 5;
    cfg.reconnect_delay_ms = 2000;
    return cfg;
}

openclaw_adapter_context_t* openclaw_adapter_create(const openclaw_config_t* config) {
    if (!config) return NULL;

    openclaw_adapter_context_t* ctx = (openclaw_adapter_context_t*)calloc(1, sizeof(openclaw_adapter_context_t));
    if (!ctx) return NULL;

    memcpy(&ctx->config, config, sizeof(openclaw_config_t));

    if (config->endpoint_url)
        ctx->config.endpoint_url = strdup(config->endpoint_url);
    if (config->api_key)
        ctx->config.api_key = strdup(config->api_key);
    if (config->organization_id)
        ctx->config.organization_id = strdup(config->organization_id);
    if (config->cluster_id)
        ctx->config.cluster_id = strdup(config->cluster_id);
    if (config->custom_headers_json)
        ctx->config.custom_headers_json = strdup(config->custom_headers_json);

    ctx->initialized = true;
    ctx->connected = false;
    ctx->registered_agents = NULL;
    ctx->registered_agent_count = 0;
    ctx->active_sessions = NULL;
    ctx->active_session_count = 0;
    ctx->messages_sent = 0;
    ctx->messages_received = 0;
    ctx->tasks_delegated = 0;
    ctx->tasks_completed = 0;
    ctx->connection_uptime_sec = 0;

    g_openclaw_instance = ctx;
    return ctx;
}

void openclaw_adapter_destroy(openclaw_adapter_context_t* ctx) {
    if (!ctx) return;

    if (ctx == g_openclaw_instance)
        g_openclaw_instance = NULL;

    if (ctx->connected)
        openclaw_disconnect(ctx);

    free(ctx->config.endpoint_url);
    free(ctx->config.api_key);
    free(ctx->config.organization_id);
    free(ctx->config.cluster_id);
    free(ctx->config.custom_headers_json);

    for (size_t i = 0; i < ctx->registered_agent_count; i++)
        openclaw_agent_card_destroy(&ctx->registered_agents[i]);
    free(ctx->registered_agents);

    for (size_t i = 0; i < ctx->active_session_count; i++)
        openclaw_session_destroy(&ctx->active_sessions[i]);
    free(ctx->active_sessions);

    for (size_t i = 0; i < ctx->registered_tool_count; i++)
        openclaw_tool_info_destroy(&ctx->registered_tools[i]);
    free(ctx->registered_tools);

    for (size_t i = 0; i < ctx->tracked_task_count; i++)
        openclaw_task_destroy(&ctx->tracked_tasks[i]);
    free(ctx->tracked_tasks);

    memset(ctx, 0, sizeof(openclaw_adapter_context_t));
    free(ctx);
}

bool openclaw_adapter_is_initialized(const openclaw_adapter_context_t* ctx) {
    return ctx && ctx->initialized;
}

const char* openclaw_adapter_version(void) {
    return OPENCLAW_ADAPTER_VERSION;
}

const char* openclaw_adapter_platform_version(void) {
    return OPENCLAW_PLATFORM_VERSION;
}

int openclaw_connect(openclaw_adapter_context_t* ctx) {
    if (!ctx || !ctx->initialized) return -1;
    if (ctx->connected) return 0;

    ctx->connected = true;
    ctx->connection_uptime_sec = 0;
    ctx->connect_timestamp = (uint64_t)time(NULL);
    snprintf(ctx->last_error, sizeof(ctx->last_error), "");
    return 0;
}

int openclaw_disconnect(openclaw_adapter_context_t* ctx) {
    if (!ctx || !ctx->connected) return -1;

    for (size_t i = 0; i < ctx->active_session_count; i++) {
        if (ctx->active_sessions[i].is_active)
            ctx->active_sessions[i].is_active = false;
    }

    ctx->connected = false;
    return 0;
}

bool openclaw_is_connected(const openclaw_adapter_context_t* ctx) {
    return ctx && ctx->connected;
}

int openclaw_register_agent(openclaw_adapter_context_t* ctx,
                             const openclaw_agent_card_t* card) {
    if (!ctx || !card || !card->agent_id) return -1;
    if (!ctx->connected) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Not connected to OpenClaw platform");
        return -2;
    }

    for (size_t i = 0; i < ctx->registered_agent_count; i++) {
        if (strcmp(ctx->registered_agents[i].agent_id, card->agent_id) == 0) {
            memcpy(&ctx->registered_agents[i], card, sizeof(openclaw_agent_card_t));
            if (card->agent_id) ctx->registered_agents[i].agent_id = strdup(card->agent_id);
            if (card->name) ctx->registered_agents[i].name = strdup(card->name);
            if (card->description) ctx->registered_agents[i].description = strdup(card->description);
            if (card->version) ctx->registered_agents[i].version = strdup(card->version);
            return 0;
        }
    }

    openclaw_agent_card_t* new_agents = (openclaw_agent_card_t*)realloc(
        ctx->registered_agents,
        (ctx->registered_agent_count + 1) * sizeof(openclaw_agent_card_t));
    if (!new_agents) return -3;

    ctx->registered_agents = new_agents;
    memset(&ctx->registered_agents[ctx->registered_agent_count], 0, sizeof(openclaw_agent_card_t));

    openclaw_agent_card_t* target = &ctx->registered_agents[ctx->registered_agent_count];
    target->agent_id = card->agent_id ? strdup(card->agent_id) : NULL;
    target->name = card->name ? strdup(card->name) : NULL;
    target->description = card->description ? strdup(card->description) : NULL;
    target->version = card->version ? strdup(card->version) : NULL;
    target->supported_modalities = card->supported_modalities;
    target->security_level = card->security_level;
    target->max_concurrent_tasks = card->max_concurrent_tasks > 0 ? card->max_concurrent_tasks : 8;
    target->is_active = true;
    target->created_at = (uint64_t)(time(NULL));
    target->last_heartbeat = target->created_at;

    ctx->registered_agent_count++;
    return 0;
}

int openclaw_discover_agents(openclaw_adapter_context_t* ctx,
                              const char* capability_filter,
                              openclaw_security_level_t min_level,
                              openclaw_agent_card_t** agents,
                              size_t* count) {
    if (!ctx || !agents || !count) return -1;
    *agents = NULL;
    *count = 0;

    if (!ctx->connected) return -2;

    size_t match_count = 0;
    for (size_t i = 0; i < ctx->registered_agent_count; i++) {
        const openclaw_agent_card_t* card = &ctx->registered_agents[i];
        if (card->is_active && card->security_level >= min_level) {
            match_count++;
        }
    }

    if (match_count == 0) return 0;

    *agents = (openclaw_agent_card_t*)calloc(match_count, sizeof(openclaw_agent_card_t));
    if (!*agents) return -3;

    size_t idx = 0;
    for (size_t i = 0; i < ctx->registered_agent_count && idx < match_count; i++) {
        const openclaw_agent_card_t* card = &ctx->registered_agents[i];
        if (card->is_active && card->security_level >= min_level) {
            (*agents)[idx] = *card;
            (*agents)[idx].agent_id = card->agent_id ? strdup(card->agent_id) : NULL;
            (*agents)[idx].name = card->name ? strdup(card->name) : NULL;
            (*agents)[idx].description = card->description ? strdup(card->description) : NULL;
            (*agents)[idx].version = card->version ? strdup(card->version) : NULL;
            idx++;
        }
    }

    *count = idx;
    return 0;
}

int openclaw_unregister_agent(openclaw_adapter_context_t* ctx,
                               const char* agent_id) {
    if (!ctx || !agent_id) return -1;

    for (size_t i = 0; i < ctx->registered_agent_count; i++) {
        if (strcmp(ctx->registered_agents[i].agent_id, agent_id) == 0) {
            openclaw_agent_card_destroy(&ctx->registered_agents[i]);
            if (i < ctx->registered_agent_count - 1) {
                memmove(&ctx->registered_agents[i],
                        &ctx->registered_agents[i + 1],
                        (ctx->registered_agent_count - i - 1) * sizeof(openclaw_agent_card_t));
            }
            ctx->registered_agent_count--;
            return 0;
        }
    }
    return -2;
}

int openclaw_register_tool(openclaw_adapter_context_t* ctx,
                            const openclaw_tool_info_t* tool) {
    if (!ctx || !tool || !tool->tool_id) return -1;
    if (!ctx->connected) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Not connected");
        return -2;
    }

    for (size_t i = 0; i < ctx->registered_tool_count; i++) {
        if (strcmp(ctx->registered_tools[i].tool_id, tool->tool_id) == 0) {
            openclaw_tool_info_destroy(&ctx->registered_tools[i]);
            ctx->registered_tools[i] = *tool;
            ctx->registered_tools[i].tool_id = tool->tool_id ? strdup(tool->tool_id) : NULL;
            ctx->registered_tools[i].name = tool->name ? strdup(tool->name) : NULL;
            ctx->registered_tools[i].description = tool->description ? strdup(tool->description) : NULL;
            ctx->registered_tools[i].input_schema_json = tool->input_schema_json ? strdup(tool->input_schema_json) : NULL;
            ctx->registered_tools[i].output_schema_json = tool->output_schema_json ? strdup(tool->output_schema_json) : NULL;
            return 0;
        }
    }

    openclaw_tool_info_t* new_tools = (openclaw_tool_info_t*)realloc(
        ctx->registered_tools,
        (ctx->registered_tool_count + 1) * sizeof(openclaw_tool_info_t));
    if (!new_tools) return -3;

    ctx->registered_tools = new_tools;
    memset(&ctx->registered_tools[ctx->registered_tool_count], 0, sizeof(openclaw_tool_info_t));

    openclaw_tool_info_t* target = &ctx->registered_tools[ctx->registered_tool_count];
    target->tool_id = tool->tool_id ? strdup(tool->tool_id) : NULL;
    target->name = tool->name ? strdup(tool->name) : NULL;
    target->description = tool->description ? strdup(tool->description) : NULL;
    target->input_schema_json = tool->input_schema_json ? strdup(tool->input_schema_json) : NULL;
    target->output_schema_json = tool->output_schema_json ? strdup(tool->output_schema_json) : NULL;

    ctx->registered_tool_count++;
    return 0;
}

int openclaw_list_tools(openclaw_adapter_context_t* ctx,
                        const char* agent_id,
                        openclaw_tool_info_t** tools,
                        size_t* count) {
    if (!ctx || !tools || !count) return -1;
    *tools = NULL;
    *count = 0;

    if (!ctx->connected) return -2;

    size_t match_count = 0;
    for (size_t i = 0; i < ctx->registered_tool_count; i++) {
        if (!agent_id || (ctx->registered_tools[i].owner_agent_id &&
            strcmp(ctx->registered_tools[i].owner_agent_id, agent_id) == 0)) {
            match_count++;
        }
    }

    if (match_count == 0) return 0;

    *tools = (openclaw_tool_info_t*)calloc(match_count, sizeof(openclaw_tool_info_t));
    if (!*tools) return -3;

    size_t idx = 0;
    for (size_t i = 0; i < ctx->registered_tool_count && idx < match_count; i++) {
        if (!agent_id || (ctx->registered_tools[i].owner_agent_id &&
            strcmp(ctx->registered_tools[i].owner_agent_id, agent_id) == 0)) {
            (*tools)[idx] = ctx->registered_tools[i];
            (*tools)[idx].tool_id = ctx->registered_tools[i].tool_id ? strdup(ctx->registered_tools[i].tool_id) : NULL;
            (*tools)[idx].name = ctx->registered_tools[i].name ? strdup(ctx->registered_tools[i].name) : NULL;
            (*tools)[idx].description = ctx->registered_tools[i].description ? strdup(ctx->registered_tools[i].description) : NULL;
            idx++;
        }
    }

    *count = idx;
    return 0;
}

int openclaw_create_session(openclaw_adapter_context_t* ctx,
                             const openclaw_session_t* session_template,
                             openclaw_session_t* out_session) {
    if (!ctx || !out_session) return -1;
    if (!ctx->connected) return -2;

    static uint32_t session_counter = 0;
    session_counter++;

    memset(out_session, 0, sizeof(openclaw_session_t));

    char sid[64];
    snprintf(sid, sizeof(sid), "oc-session-%08x", session_counter);
    out_session->session_id = strdup(sid);

    if (session_template) {
        out_session->agent_id = session_template->agent_id ? strdup(session_template->agent_id) : NULL;
        out_session->modality = session_template->modality;
        out_session->security_level = session_template->security_level;
    } else {
        out_session->modality = OPENCLAW_MODALITY_TEXT;
        out_session->security_level = ctx->config.default_security_level;
    }

    out_session->created_at = (uint64_t)(time(NULL));
    out_session->last_activity = out_session->created_at;
    out_session->is_active = true;

    openclaw_session_t* new_sessions = (openclaw_session_t*)realloc(
        ctx->active_sessions,
        (ctx->active_session_count + 1) * sizeof(openclaw_session_t));
    if (new_sessions) {
        ctx->active_sessions = new_sessions;
        memcpy(&ctx->active_sessions[ctx->active_session_count],
               out_session, sizeof(openclaw_session_t));
        ctx->active_sessions[ctx->active_session_count].session_id = strdup(out_session->session_id);
        ctx->active_sessions[ctx->active_session_count].agent_id =
            out_session->agent_id ? strdup(out_session->agent_id) : NULL;
        ctx->active_session_count++;
    }

    return 0;
}

int openclaw_close_session(openclaw_adapter_context_t* ctx,
                           const char* session_id) {
    if (!ctx || !session_id) return -1;

    for (size_t i = 0; i < ctx->active_session_count; i++) {
        if (strcmp(ctx->active_sessions[i].session_id, session_id) == 0) {
            ctx->active_sessions[i].is_active = false;
            openclaw_session_destroy(&ctx->active_sessions[i]);
            if (i < ctx->active_session_count - 1) {
                memmove(&ctx->active_sessions[i],
                        &ctx->active_sessions[i + 1],
                        (ctx->active_session_count - i - 1) * sizeof(openclaw_session_t));
            }
            ctx->active_session_count--;
            return 0;
        }
    }
    return -2;
}

int openclaw_send_message(openclaw_adapter_context_t* ctx,
                          const openclaw_message_t* msg,
                          openclaw_message_t* response) {
    if (!ctx || !msg || !response) return -1;
    if (!ctx->connected) return -2;

    ctx->messages_sent++;

    if (ctx->message_handler) {
        int ret = ctx->message_handler(msg, response, ctx->message_handler_data);
        ctx->messages_received++;
        return ret;
    }

    memset(response, 0, sizeof(openclaw_message_t));
    response->message_id = msg->message_id ? strdup(msg->message_id) : NULL;
    response->session_id = msg->session_id ? strdup(msg->session_id) : NULL;
    response->receiver_id = msg->sender_id ? strdup(msg->sender_id) : NULL;
    response->sender_id = msg->receiver_id ? strdup(msg->receiver_id) : NULL;
    response->modality = msg->modality;
    response->timestamp = (uint64_t)(time(NULL));

    ctx->messages_received++;
    return 0;
}

int openclaw_delegate_task(openclaw_adapter_context_t* ctx,
                            const openclaw_task_t* task,
                            const char* target_agent_id,
                            openclaw_task_t* result) {
    if (!ctx || !task || !result) return -1;
    if (!ctx->connected) return -2;

    ctx->tasks_delegated++;

    if (ctx->task_handler) {
        int ret = ctx->task_handler(task, result, ctx->task_handler_data);
        if (ret == 0) ctx->tasks_completed++;
        return ret;
    }

    static uint32_t task_counter = 0;
    task_counter++;

    memset(result, 0, sizeof(openclaw_task_t));
    char tid[64];
    snprintf(tid, sizeof(tid), "oc-task-%08x", task_counter);
    result->task_id = strdup(tid);
    result->session_id = task->session_id ? strdup(task->session_id) : NULL;
    result->description = task->description ? strdup(task->description) : NULL;
    result->input_data_json = task->input_data_json ? strdup(task->input_data_json) : NULL;
    result->assigned_agent_id = target_agent_id ? strdup(target_agent_id) : NULL;
    result->priority = task->priority > 0 ? task->priority : 5;
    result->state = OPENCLAW_AGENT_STATE_EXECUTING;
    result->progress = 0.0;
    result->created_at = (uint64_t)(time(NULL));

    ctx->tasks_completed++;
    result->state = OPENCLAW_AGENT_STATE_IDLE;
    result->progress = 1.0;
    result->completed_at = (uint64_t)(time(NULL));

    openclaw_task_t* new_tasks = (openclaw_task_t*)realloc(
        ctx->tracked_tasks,
        (ctx->tracked_task_count + 1) * sizeof(openclaw_task_t));
    if (new_tasks) {
        ctx->tracked_tasks = new_tasks;
        ctx->tracked_tasks[ctx->tracked_task_count] = *result;
        ctx->tracked_tasks[ctx->tracked_task_count].task_id = result->task_id ? strdup(result->task_id) : NULL;
        ctx->tracked_tasks[ctx->tracked_task_count].session_id = result->session_id ? strdup(result->session_id) : NULL;
        ctx->tracked_tasks[ctx->tracked_task_count].description = result->description ? strdup(result->description) : NULL;
        ctx->tracked_tasks[ctx->tracked_task_count].assigned_agent_id = result->assigned_agent_id ? strdup(result->assigned_agent_id) : NULL;
        ctx->tracked_task_count++;
    }

    return 0;
}

int openclaw_query_task(openclaw_adapter_context_t* ctx,
                        const char* task_id,
                        openclaw_task_t* result) {
    if (!ctx || !task_id || !result) return -1;
    memset(result, 0, sizeof(openclaw_task_t));

    for (size_t i = 0; i < ctx->tracked_task_count; i++) {
        if (ctx->tracked_tasks[i].task_id &&
            strcmp(ctx->tracked_tasks[i].task_id, task_id) == 0) {
            *result = ctx->tracked_tasks[i];
            result->task_id = ctx->tracked_tasks[i].task_id ? strdup(ctx->tracked_tasks[i].task_id) : NULL;
            result->session_id = ctx->tracked_tasks[i].session_id ? strdup(ctx->tracked_tasks[i].session_id) : NULL;
            result->description = ctx->tracked_tasks[i].description ? strdup(ctx->tracked_tasks[i].description) : NULL;
            result->assigned_agent_id = ctx->tracked_tasks[i].assigned_agent_id ? strdup(ctx->tracked_tasks[i].assigned_agent_id) : NULL;
            result->input_data_json = ctx->tracked_tasks[i].input_data_json ? strdup(ctx->tracked_tasks[i].input_data_json) : NULL;
            result->result_json = ctx->tracked_tasks[i].result_json ? strdup(ctx->tracked_tasks[i].result_json) : NULL;
            result->error_message = ctx->tracked_tasks[i].error_message ? strdup(ctx->tracked_tasks[i].error_message) : NULL;
            return 0;
        }
    }

    return -2;
}

int openclaw_cancel_task(openclaw_adapter_context_t* ctx,
                         const char* task_id) {
    if (!ctx || !task_id) return -1;

    for (size_t i = 0; i < ctx->tracked_task_count; i++) {
        if (ctx->tracked_tasks[i].task_id &&
            strcmp(ctx->tracked_tasks[i].task_id, task_id) == 0) {
            ctx->tracked_tasks[i].state = OPENCLAW_AGENT_STATE_ERROR;
            if (ctx->event_callback) {
                ctx->event_callback("task_cancelled", task_id, ctx->event_callback_data);
            }
            return 0;
        }
    }

    return -2;
}

int openclaw_get_cluster_status(openclaw_adapter_context_t* ctx,
                                openclaw_cluster_status_t* status) {
    if (!ctx || !status) return -1;

    memset(status, 0, sizeof(openclaw_cluster_status_t));
    status->node_id = "agentos-node-001";
    status->cluster_name = ctx->config.cluster_id ? ctx->config.cluster_id : "default";
    status->total_nodes = 1;
    status->active_nodes = 1;
    status->total_agents = (uint64_t)ctx->registered_agent_count;
    status->active_sessions = (uint64_t)ctx->active_session_count;
    status->messages_processed = ctx->messages_sent + ctx->messages_received;
    status->tasks_completed = ctx->tasks_completed;
    status->uptime_seconds = ctx->connection_uptime_sec;
    status->cpu_usage_pct = (double)(ctx->active_session_count * 2.5 + ctx->registered_agent_count * 0.5);
    status->memory_usage_mb = (double)(ctx->tracked_task_count * 8.0 + ctx->registered_tool_count * 0.5 + 32.0);
    status->disk_usage_pct = (double)(ctx->registered_agent_count * 0.1 + ctx->registered_tool_count * 0.05);

    return 0;
}

int openclaw_set_message_handler(openclaw_adapter_context_t* ctx,
                                 openclaw_message_handler_t handler,
                                 void* user_data) {
    if (!ctx) return -1;
    ctx->message_handler = handler;
    ctx->message_handler_data = user_data;
    return 0;
}

int openclaw_set_task_handler(openclaw_adapter_context_t* ctx,
                               openclaw_task_handler_t handler,
                               void* user_data) {
    if (!ctx) return -1;
    ctx->task_handler = handler;
    ctx->task_handler_data = user_data;
    return 0;
}

int openclaw_set_event_callback(openclaw_adapter_context_t* ctx,
                                 openclaw_event_callback_t callback,
                                 void* user_data) {
    if (!ctx) return -1;
    ctx->event_callback = callback;
    ctx->event_callback_data = user_data;
    return 0;
}

int openclaw_set_status_callback(openclaw_adapter_context_t* ctx,
                                  openclaw_status_callback_t callback,
                                  void* user_data) {
    if (!ctx) return -1;
    ctx->status_callback = callback;
    ctx->status_callback_data = user_data;
    return 0;
}

int openclaw_send_heartbeat(openclaw_adapter_context_t* ctx) {
    if (!ctx || !ctx->connected) return -1;

    ctx->connection_uptime_sec += ctx->config.heartbeat_interval_sec;

    for (size_t i = 0; i < ctx->registered_agent_count; i++) {
        if (ctx->registered_agents[i].is_active) {
            ctx->registered_agents[i].last_heartbeat = (uint64_t)(time(NULL));
        }
    }

    if (ctx->status_callback) {
        openclaw_cluster_status_t status;
        openclaw_get_cluster_status(ctx, &status);
        ctx->status_callback(&status, ctx->status_callback_data);
    }

    return 0;
}

int openclaw_get_statistics(openclaw_adapter_context_t* ctx,
                            char* stats_json,
                            size_t buffer_size) {
    if (!ctx || !stats_json || buffer_size < 64) return -1;

    openclaw_cluster_status_t status;
    openclaw_get_cluster_status(ctx, &status);

    int written = snprintf(stats_json, buffer_size,
        "{"
        "\"adapter_version\":\"%s\","
        "\"platform_version\":\"%s\","
        "\"mode\":\"%s\","
        "\"connected\":%s,"
        "\"registered_agents\":%zu,"
        "\"active_sessions\":%zu,"
        "\"messages_sent\":%llu,"
        "\"messages_received\":%llu,"
        "\"tasks_delegated\":%llu,"
        "\"tasks_completed\":%llu,"
        "\"uptime_seconds\":%llu,"
        "\"cluster\":{"
            "\"node_id\":\"%s\","
            "\"cluster_name\":\"%s\","
            "\"total_agents\":%llu,"
            "\"active_sessions\":%llu"
        "}"
        "}",
        OPENCLAW_ADAPTER_VERSION,
        OPENCLAW_PLATFORM_VERSION,
        ctx->config.mode == OPENCLAW_MODE_STANDALONE ? "standalone" :
         ctx->config.mode == OPENCLAW_MODE_CLUSTERED ? "clustered" :
         ctx->config.mode == OPENCLAW_MODE_HYBRID ? "hybrid" : "embedded",
        ctx->connected ? "true" : "false",
        ctx->registered_agent_count,
        ctx->active_session_count,
        (unsigned long long)ctx->messages_sent,
        (unsigned long long)ctx->messages_received,
        (unsigned long long)ctx->tasks_delegated,
        (unsigned long long)ctx->tasks_completed,
        (unsigned long long)ctx->connection_uptime_sec,
        status.node_id ? status.node_id : "",
        status.cluster_name ? status.cluster_name : "",
        (unsigned long long)status.total_agents,
        (unsigned long long)status.active_sessions
    );

    openclaw_cluster_status_destroy(&status);

    return (written >= 0 && (size_t)written < buffer_size) ? 0 : -2;
}

static int openclaw_proto_init(void** context) {
    openclaw_config_t config = openclaw_config_default();
    openclaw_adapter_context_t* ctx = openclaw_adapter_create(&config);
    if (!ctx) return -1;
    *context = ctx;
    return 0;
}

static void openclaw_proto_destroy(void* context) {
    openclaw_adapter_destroy((openclaw_adapter_context_t*)context);
}

static int openclaw_proto_handle_request(void* context,
                                          const char* raw_request,
                                          size_t request_size,
                                          const char* content_type,
                                          char** response,
                                          size_t* response_size,
                                          char** response_content_type) {
    if (!context || !raw_request) return -1;
    openclaw_adapter_context_t* ctx = (openclaw_adapter_context_t*)context;

    (void)request_size;
    (void)content_type;

    openclaw_message_t msg = {0};
    msg.message_id = "proto-req";
    msg.payload = (void*)raw_request;
    msg.payload_size = request_size;
    msg.modality = OPENCLAW_MODALITY_TEXT;
    msg.timestamp = (uint64_t)time(NULL);

    openclaw_message_t resp = {0};
    int ret = openclaw_send_message(ctx, &msg, &resp);

    if (ret == 0 && resp.payload) {
        *response = strdup((const char*)resp.payload);
        *response_size = resp.payload_size;
    } else {
        *response = strdup("{\"status\":\"error\",\"message\":\"Request processing failed\"}");
        *response_size = strlen(*response);
    }

    if (response_content_type) *response_content_type = strdup("application/json");

    openclaw_message_destroy(&resp);
    return ret;
}

const proto_adapter_t* openclaw_get_protocol_adapter(void) {
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "OpenClaw";
        adapter.version = OPENCLAW_ADAPTER_VERSION;
        adapter.description = "OpenClaw Platform Integration Adapter - offline private AI Agent platform with multimodal capabilities";
        adapter.init = openclaw_proto_init;
        adapter.destroy = openclaw_proto_destroy;
        adapter.handle_request = openclaw_proto_handle_request;
        adapter.get_version = openclaw_adapter_version;
        adapter.capabilities = PROTO_CAP_MULTIMODAL | PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING | PROTO_CAP_AGENT_DISCOVERY;
        initialized = true;
    }

    return &adapter;
}

void openclaw_agent_card_destroy(openclaw_agent_card_t* card) {
    if (!card) return;
    free(card->agent_id);
    free(card->name);
    free(card->description);
    free(card->version);
    memset(card, 0, sizeof(openclaw_agent_card_t));
}

void openclaw_tool_info_destroy(openclaw_tool_info_t* tool) {
    if (!tool) return;
    free(tool->tool_id);
    free(tool->name);
    free(tool->description);
    free(tool->input_schema_json);
    free(tool->output_schema_json);
    memset(tool, 0, sizeof(openclaw_tool_info_t));
}

void openclaw_session_destroy(openclaw_session_t* session) {
    if (!session) return;
    free(session->session_id);
    free(session->agent_id);
    free(session->parent_session_id);
    memset(session, 0, sizeof(openclaw_session_t));
}

void openclaw_message_destroy(openclaw_message_t* msg) {
    if (!msg) return;
    free(msg->message_id);
    free(msg->session_id);
    free(msg->sender_id);
    free(msg->receiver_id);
    free(msg->content_type);
    free(msg->payload);
    memset(msg, 0, sizeof(openclaw_message_t));
}

void openclaw_task_destroy(openclaw_task_t* task) {
    if (!task) return;
    free(task->task_id);
    free(task->session_id);
    free(task->description);
    free(task->input_data_json);
    free(task->assigned_agent_id);
    free(task->result_json);
    free(task->error_message);
    memset(task, 0, sizeof(openclaw_task_t));
}

void openclaw_cluster_status_destroy(openclaw_cluster_status_t* status) {
    if (!status) return;
    free(status->node_id);
    free(status->cluster_name);
    memset(status, 0, sizeof(openclaw_cluster_status_t));
}
