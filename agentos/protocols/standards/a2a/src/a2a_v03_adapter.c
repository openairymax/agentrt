// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file a2a_v03_adapter.c
 * @brief A2A v0.3.0 Protocol Adapter Implementation
 */

#include "a2a_v03_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct {
    a2a_agent_card_t card;
    bool active;
} a2a_agent_entry_t;

typedef struct {
    a2a_task_t task;
    bool active;
} a2a_task_entry_t;

struct a2a_v03_context_s {
    a2a_v03_config_t config;

    a2a_agent_entry_t* agents;
    size_t agent_count;
    size_t agent_capacity;

    a2a_task_entry_t* tasks;
    size_t task_count;
    size_t task_capacity;

    a2a_task_handler_t task_handler;
    void* task_handler_data;

    a2a_message_handler_t message_handler;
    void* message_handler_data;

    a2a_negotiation_handler_t negotiation_handler;
    void* negotiation_handler_data;

    a2a_notification_handler_t notification_handler;
    void* notification_handler_data;

    a2a_streaming_handler_t streaming_handler;
    void* streaming_handler_data;

    uint64_t task_counter;
    uint64_t session_counter;
};

static char* strdup_safe(const char* s) { return s ? strdup(s) : NULL; }

static uint64_t generate_id(void) {
    static uint64_t counter = 0;
    counter++;
    return (uint64_t)time(NULL) * 1000000 + counter;
}

a2a_v03_config_t a2a_v03_config_default(void) {
    a2a_v03_config_t config = {0};
    config.capabilities = A2A_CAP_TASK_EXECUTION | A2A_CAP_PUSH_NOTIFICATIONS | A2A_CAP_MULTI_TURN;
    config.default_timeout_ms = A2A_V03_DEFAULT_TIMEOUT_MS;
    config.max_agents = A2A_V03_MAX_AGENTS;
    config.max_tasks = A2A_V03_MAX_TASKS;
    config.max_message_size = A2A_V03_MAX_MESSAGE_SIZE;
    config.enable_negotiation = false;
    config.enable_streaming = false;
    config.enable_push_notifications = true;
    config.require_authentication = false;
    config.default_authentication = strdup_safe("none");
    return config;
}

a2a_v03_context_t* a2a_v03_context_create(const a2a_v03_config_t* config) {
    a2a_v03_context_t* ctx = calloc(1, sizeof(a2a_v03_context_t));
    if (!ctx) return NULL;

    if (config) {
        ctx->config = *config;
        ctx->config.default_authentication = strdup_safe(config->default_authentication);
    } else {
        ctx->config = a2a_v03_config_default();
    }

    ctx->agent_capacity = 32;
    ctx->agents = calloc(ctx->agent_capacity, sizeof(a2a_agent_entry_t));

    ctx->task_capacity = 128;
    ctx->tasks = calloc(ctx->task_capacity, sizeof(a2a_task_entry_t));

    ctx->task_counter = 0;
    ctx->session_counter = 0;

    return ctx;
}

void a2a_v03_context_destroy(a2a_v03_context_t* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < ctx->agent_count; i++) {
        a2a_agent_card_destroy(&ctx->agents[i].card);
    }
    free(ctx->agents);

    for (size_t i = 0; i < ctx->task_count; i++) {
        a2a_task_destroy(&ctx->tasks[i].task);
    }
    free(ctx->tasks);

    free(ctx->config.default_authentication);
    free(ctx);
}

int a2a_v03_register_agent(a2a_v03_context_t* ctx, const a2a_agent_card_t* card) {
    if (!ctx || !card) return -1;
    if (ctx->agent_count >= ctx->config.max_agents) return -2;

    for (size_t i = 0; i < ctx->agent_count; i++) {
        if (ctx->agents[i].card.id && strcmp(ctx->agents[i].card.id, card->id) == 0) {
            return -3;
        }
    }

    if (ctx->agent_count >= ctx->agent_capacity) {
        size_t new_cap = ctx->agent_capacity * 2;
        a2a_agent_entry_t* new_agents = realloc(ctx->agents, new_cap * sizeof(a2a_agent_entry_t));
        if (!new_agents) return -4;
        ctx->agents = new_agents;
        ctx->agent_capacity = new_cap;
    }

    a2a_agent_entry_t* entry = &ctx->agents[ctx->agent_count];
    memset(entry, 0, sizeof(*entry));
    entry->card.id = strdup_safe(card->id);
    entry->card.name = strdup_safe(card->name);
    entry->card.description = strdup_safe(card->description);
    entry->card.url = strdup_safe(card->url);
    entry->card.version = strdup_safe(card->version);
    entry->card.capabilities = card->capabilities;
    entry->card.skill_count = card->skill_count;
    entry->card.provider_name = strdup_safe(card->provider_name);
    entry->card.provider_url = strdup_safe(card->provider_url);
    entry->card.documentation_url = strdup_safe(card->documentation_url);
    entry->card.authentication_schemes_json = strdup_safe(card->authentication_schemes_json);

    if (card->skills && card->skill_count > 0) {
        entry->card.skills = calloc(card->skill_count, sizeof(a2a_skill_t));
        for (size_t i = 0; i < card->skill_count; i++) {
            entry->card.skills[i].name = strdup_safe(card->skills[i].name);
            entry->card.skills[i].description = strdup_safe(card->skills[i].description);
            entry->card.skills[i].schema_json = strdup_safe(card->skills[i].schema_json);
        }
    }

    entry->active = true;
    ctx->agent_count++;
    return 0;
}

int a2a_v03_unregister_agent(a2a_v03_context_t* ctx, const char* agent_id) {
    if (!ctx || !agent_id) return -1;
    for (size_t i = 0; i < ctx->agent_count; i++) {
        if (ctx->agents[i].card.id && strcmp(ctx->agents[i].card.id, agent_id) == 0) {
            a2a_agent_card_destroy(&ctx->agents[i].card);
            memmove(&ctx->agents[i], &ctx->agents[i + 1], (ctx->agent_count - i - 1) * sizeof(a2a_agent_entry_t));
            ctx->agent_count--;
            return 0;
        }
    }
    return -2;
}

const a2a_agent_card_t* a2a_v03_get_agent_card(a2a_v03_context_t* ctx, const char* agent_id) {
    if (!ctx || !agent_id) return NULL;
    for (size_t i = 0; i < ctx->agent_count; i++) {
        if (ctx->agents[i].card.id && strcmp(ctx->agents[i].card.id, agent_id) == 0) {
            return &ctx->agents[i].card;
        }
    }
    return NULL;
}

int a2a_v03_discover_agents(a2a_v03_context_t* ctx,
                              const char* capability,
                              const char* skill_name,
                              a2a_agent_card_t*** results,
                              size_t* result_count) {
    if (!ctx || !results || !result_count) return -1;

    size_t count = 0;
    size_t max_results = ctx->agent_count;
    a2a_agent_card_t** found = calloc(max_results, sizeof(a2a_agent_card_t*));
    if (!found) return -3;

    for (size_t i = 0; i < ctx->agent_count; i++) {
        if (!ctx->agents[i].active) continue;

        bool match = true;
        if (capability) {
            match = false;
            for (size_t s = 0; s < ctx->agents[i].card.skill_count; s++) {
                if (ctx->agents[i].card.skills[s].name &&
                    strstr(ctx->agents[i].card.skills[s].name, capability)) {
                    match = true;
                    break;
                }
            }
        }
        if (skill_name) {
            bool skill_match = false;
            for (size_t s = 0; s < ctx->agents[i].card.skill_count; s++) {
                if (ctx->agents[i].card.skills[s].name &&
                    strcmp(ctx->agents[i].card.skills[s].name, skill_name) == 0) {
                    skill_match = true;
                    break;
                }
            }
            match = match && skill_match;
        }

        if (match && count < max_results) {
            found[count++] = &ctx->agents[i].card;
        }
    }

    *results = found;
    *result_count = count;
    return 0;
}

int a2a_v03_create_task(a2a_v03_context_t* ctx,
                          const char* agent_id,
                          const char* description,
                          const char* input_json,
                          a2a_task_t** task) {
    if (!ctx || !agent_id || !task) return -1;

    const a2a_agent_card_t* agent = a2a_v03_get_agent_card(ctx, agent_id);
    if (!agent) return -2;

    if (ctx->task_count >= ctx->config.max_tasks) return -3;
    if (ctx->task_count >= ctx->task_capacity) {
        size_t new_cap = ctx->task_capacity * 2;
        a2a_task_entry_t* new_tasks = realloc(ctx->tasks, new_cap * sizeof(a2a_task_entry_t));
        if (!new_tasks) return -4;
        ctx->tasks = new_tasks;
        ctx->task_capacity = new_cap;
    }

    ctx->task_counter++;
    ctx->session_counter++;

    a2a_task_entry_t* entry = &ctx->tasks[ctx->task_count];
    memset(entry, 0, sizeof(*entry));

    char task_id[64];
    snprintf(task_id, sizeof(task_id), "task_%llu", (unsigned long long)ctx->task_counter);

    char session_id[64];
    snprintf(session_id, sizeof(session_id), "session_%llu", (unsigned long long)ctx->session_counter);

    entry->task.id = strdup_safe(task_id);
    entry->task.session_id = strdup_safe(session_id);
    entry->task.agent_id = strdup_safe(agent_id);
    entry->task.state = A2A_TASK_SUBMITTED;
    entry->task.description = strdup_safe(description);
    entry->task.input_json = strdup_safe(input_json);
    entry->task.output_json = NULL;
    entry->task.created_at = (uint64_t)time(NULL);
    entry->task.updated_at = entry->task.created_at;
    entry->task.progress = 0.0;
    entry->task.error_message = NULL;
    entry->task.assigned_agent = NULL;
    entry->active = true;

    *task = &entry->task;
    ctx->task_count++;

    if (ctx->task_handler) {
        a2a_task_state_t new_state = A2A_TASK_WORKING;
        char* output = NULL;
        int rc = ctx->task_handler(ctx, &entry->task, &new_state, &output, ctx->task_handler_data);
        if (rc == 0) {
            entry->task.state = new_state;
            if (output) {
                free(entry->task.output_json);
                entry->task.output_json = output;
            }
            entry->task.updated_at = (uint64_t)time(NULL);
        }
    }

    return 0;
}

int a2a_v03_update_task(a2a_v03_context_t* ctx,
                          const char* task_id,
                          a2a_task_state_t new_state,
                          const char* output_json,
                          double progress) {
    if (!ctx || !task_id) return -1;
    for (size_t i = 0; i < ctx->task_count; i++) {
        if (ctx->tasks[i].task.id && strcmp(ctx->tasks[i].task.id, task_id) == 0) {
            ctx->tasks[i].task.state = new_state;
            if (output_json) {
                free(ctx->tasks[i].task.output_json);
                ctx->tasks[i].task.output_json = strdup_safe(output_json);
            }
            ctx->tasks[i].task.progress = progress;
            ctx->tasks[i].task.updated_at = (uint64_t)time(NULL);
            return 0;
        }
    }
    return -2;
}

int a2a_v03_cancel_task(a2a_v03_context_t* ctx, const char* task_id, const char* reason) {
    if (!ctx || !task_id) return -1;
    for (size_t i = 0; i < ctx->task_count; i++) {
        if (ctx->tasks[i].task.id && strcmp(ctx->tasks[i].task.id, task_id) == 0) {
            ctx->tasks[i].task.state = A2A_TASK_CANCELED;
            free(ctx->tasks[i].task.error_message);
            ctx->tasks[i].task.error_message = strdup_safe(reason);
            ctx->tasks[i].task.updated_at = (uint64_t)time(NULL);
            return 0;
        }
    }
    return -2;
}

int a2a_v03_get_task(a2a_v03_context_t* ctx, const char* task_id, a2a_task_t** task) {
    if (!ctx || !task_id || !task) return -1;
    for (size_t i = 0; i < ctx->task_count; i++) {
        if (ctx->tasks[i].task.id && strcmp(ctx->tasks[i].task.id, task_id) == 0) {
            *task = &ctx->tasks[i].task;
            return 0;
        }
    }
    return -2;
}

int a2a_v03_send_message(a2a_v03_context_t* ctx,
                           const char* target_agent_id,
                           const a2a_message_t* message,
                           a2a_message_t** response,
                           size_t* response_count) {
    if (!ctx || !target_agent_id || !message) return -1;
    if (!ctx->message_handler) return -2;
    return ctx->message_handler(ctx, target_agent_id, message, response, response_count, ctx->message_handler_data);
}

int a2a_v03_negotiate(a2a_v03_context_t* ctx,
                        const a2a_negotiation_t* proposal,
                        a2a_negotiation_action_t* response_action,
                        char** response_terms) {
    if (!ctx || !proposal) return -1;
    if (!ctx->negotiation_handler) return -2;
    if (!(ctx->config.capabilities & A2A_CAP_NEGOTIATION)) return -3;
    return ctx->negotiation_handler(ctx, proposal, response_action, response_terms, ctx->negotiation_handler_data);
}

int a2a_v03_subscribe_notifications(a2a_v03_context_t* ctx,
                                      a2a_notification_handler_t handler,
                                      void* user_data) {
    if (!ctx) return -1;
    ctx->notification_handler = handler;
    ctx->notification_handler_data = user_data;
    return 0;
}

int a2a_v03_unsubscribe_notifications(a2a_v03_context_t* ctx) {
    if (!ctx) return -1;
    ctx->notification_handler = NULL;
    ctx->notification_handler_data = NULL;
    return 0;
}

int a2a_v03_send_notification(a2a_v03_context_t* ctx, const a2a_notification_t* notification) {
    if (!ctx || !notification) return -1;
    if (ctx->notification_handler) {
        ctx->notification_handler(ctx, notification, ctx->notification_handler_data);
    }
    return 0;
}

int a2a_v03_stream_task_update(a2a_v03_context_t* ctx,
                                 const char* task_id,
                                 double progress,
                                 const char* chunk_json,
                                 bool is_final) {
    if (!ctx || !task_id) return -1;
    if (ctx->streaming_handler) {
        ctx->streaming_handler(ctx, task_id, progress, chunk_json, is_final, ctx->streaming_handler_data);
    }
    return 0;
}

int a2a_v03_set_task_handler(a2a_v03_context_t* ctx, a2a_task_handler_t handler, void* user_data) {
    if (!ctx) return -1;
    ctx->task_handler = handler;
    ctx->task_handler_data = user_data;
    return 0;
}

int a2a_v03_set_message_handler(a2a_v03_context_t* ctx, a2a_message_handler_t handler, void* user_data) {
    if (!ctx) return -1;
    ctx->message_handler = handler;
    ctx->message_handler_data = user_data;
    return 0;
}

int a2a_v03_set_negotiation_handler(a2a_v03_context_t* ctx, a2a_negotiation_handler_t handler, void* user_data) {
    if (!ctx) return -1;
    ctx->negotiation_handler = handler;
    ctx->negotiation_handler_data = user_data;
    if (handler) ctx->config.capabilities |= A2A_CAP_NEGOTIATION;
    return 0;
}

int a2a_v03_set_streaming_handler(a2a_v03_context_t* ctx, a2a_streaming_handler_t handler, void* user_data) {
    if (!ctx) return -1;
    ctx->streaming_handler = handler;
    ctx->streaming_handler_data = user_data;
    if (handler) ctx->config.capabilities |= A2A_CAP_STREAMING;
    return 0;
}

int a2a_v03_route_request(a2a_v03_context_t* ctx,
                            const char* method,
                            const char* params_json,
                            char** response_json) {
    if (!ctx || !method || !response_json) return -1;

    if (strcmp(method, "agent/discover") == 0 || strcmp(method, "agents/discover") == 0) {
        a2a_agent_card_t** results = NULL;
        size_t count = 0;
        int rc = a2a_v03_discover_agents(ctx, NULL, NULL, &results, &count);
        if (rc != 0) { free(results); return rc; }

        size_t buf_size = 4096 + count * 512;
        char* buf = malloc(buf_size);
        if (!buf) { free(results); return -3; }

        size_t offset = snprintf(buf, buf_size, "{\"agents\":[");
        for (size_t i = 0; i < count; i++) {
            if (i > 0) offset += snprintf(buf + offset, buf_size - offset, ",");
            offset += snprintf(buf + offset, buf_size - offset,
                "{\"id\":\"%s\",\"name\":\"%s\",\"capabilities\":%u}",
                results[i]->id ? results[i]->id : "",
                results[i]->name ? results[i]->name : "",
                results[i]->capabilities);
        }
        offset += snprintf(buf + offset, buf_size - offset, "]}");
        *response_json = buf;
        free(results);
        return 0;
    }

    if (strcmp(method, "task/create") == 0 || strcmp(method, "tasks/create") == 0) {
        a2a_task_t* task = NULL;
        int rc = a2a_v03_create_task(ctx, "default", "A2A Task", params_json, &task);
        if (rc != 0 || !task) {
            *response_json = strdup("{\"error\":{\"code\":-32000,\"message\":\"Task creation failed\"}}");
            return rc;
        }
        size_t len = snprintf(NULL, 0,
            "{\"id\":\"%s\",\"status\":\"%s\",\"progress\":%.1f}",
            task->id, "submitted", task->progress);
        char* resp = malloc(len + 1);
        if (resp) snprintf(resp, len + 1,
            "{\"id\":\"%s\",\"status\":\"%s\",\"progress\":%.1f}",
            task->id, "submitted", task->progress);
        *response_json = resp;
        return 0;
    }

    if (strcmp(method, "task/get") == 0 || strcmp(method, "tasks/get") == 0) {
        a2a_task_t* task = NULL;
        if (a2a_v03_get_task(ctx, "latest", &task) == 0 && task) {
            const char* state_str = "submitted";
            switch (task->state) {
                case A2A_TASK_WORKING: state_str = "working"; break;
                case A2A_TASK_COMPLETED: state_str = "completed"; break;
                case A2A_TASK_CANCELED: state_str = "canceled"; break;
                case A2A_TASK_FAILED: state_str = "failed"; break;
                default: break;
            }
            size_t len = snprintf(NULL, 0,
                "{\"id\":\"%s\",\"status\":\"%s\",\"progress\":%.1f}",
                task->id, state_str, task->progress);
            char* resp = malloc(len + 1);
            if (resp) snprintf(resp, len + 1,
                "{\"id\":\"%s\",\"status\":\"%s\",\"progress\":%.1f}",
                task->id, state_str, task->progress);
            *response_json = resp;
            return 0;
        }
        *response_json = strdup("{\"error\":{\"code\":-32001,\"message\":\"Task not found\"}}");
        return -2;
    }

    *response_json = strdup("{\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
    return -2;
}

static protocol_adapter_t a2a_v03_adapter_instance = {
    .type = PROTOCOL_CUSTOM,
    .init = NULL,
    .destroy = NULL,
    .encode = NULL,
    .decode = NULL,
    .connect = NULL,
    .disconnect = NULL,
    .is_connected = NULL,
    .get_stats = NULL
};

const protocol_adapter_t* a2a_v03_get_adapter(void) {
    return &a2a_v03_adapter_instance;
}

size_t a2a_v03_get_agent_count(a2a_v03_context_t* ctx) { return ctx ? ctx->agent_count : 0; }
size_t a2a_v03_get_task_count(a2a_v03_context_t* ctx) { return ctx ? ctx->task_count : 0; }
uint32_t a2a_v03_get_capabilities(a2a_v03_context_t* ctx) { return ctx ? ctx->config.capabilities : 0; }

void a2a_agent_card_destroy(a2a_agent_card_t* card) {
    if (!card) return;
    free(card->id); free(card->name); free(card->description);
    free(card->url); free(card->version);
    free(card->provider_name); free(card->provider_url);
    free(card->documentation_url); free(card->authentication_schemes_json);
    if (card->skills) {
        for (size_t i = 0; i < card->skill_count; i++) {
            free(card->skills[i].name);
            free(card->skills[i].description);
            free(card->skills[i].schema_json);
        }
        free(card->skills);
    }
}

void a2a_task_destroy(a2a_task_t* task) {
    if (!task) return;
    free(task->id); free(task->session_id); free(task->agent_id);
    free(task->description); free(task->input_json); free(task->output_json);
    free(task->error_message);
}

void a2a_message_destroy(a2a_message_t* msg) {
    if (!msg) return;
    free(msg->role); free(msg->content_json); free(msg->mime_type);
    free(msg->file_name); free(msg->file_data);
}

void a2a_negotiation_destroy(a2a_negotiation_t* neg) {
    if (!neg) return;
    free(neg->task_id); free(neg->agent_id);
    free(neg->terms_json); free(neg->counter_proposal_json); free(neg->reason);
}
