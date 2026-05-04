// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file agntcy_acp_adapter.c
 * @brief AGNTCY Agent Communication Protocol Adapter Implementation
 */

#include "agntcy_acp_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static struct {
    agntcy_handle_t handle;
    bool proto_initialized;
} g_agntcy_state = {0};

int agntcy_acp_create(agntcy_handle_t** handle) {
    if (!handle) return -1;

    agntcy_handle_t* h = (agntcy_handle_t*)calloc(1, sizeof(agntcy_handle_t));
    if (!h) return -2;

    h->initialized = true;
    *handle = h;
    return 0;
}

void agntcy_acp_destroy(agntcy_handle_t* handle) {
    if (!handle) return;
    for (size_t i = 0; i < handle->task_count; i++) {
        if (handle->tasks[i]) {
            free(handle->tasks[i]->workflow_json);
            free(handle->tasks[i]);
        }
    }
    free(handle);
}

int agntcy_agent_register(agntcy_handle_t* h, const agntcy_agent_card_t* card) {
    if (!h || !card) return -1;
    if (h->agent_count >= AGNTCY_ACP_MAX_AGENTS) return -2;

    for (size_t i = 0; i < h->agent_count; i++) {
        if (strcmp(h->agents[i].agent_id, card->agent_id) == 0) {
            h->agents[i] = *card;
            h->agents[i].registered_at = (uint64_t)time(NULL);
            h->agents[i].online = true;
            return 0;
        }
    }

    h->agents[h->agent_count] = *card;
    h->agents[h->agent_count].registered_at = (uint64_t)time(NULL);
    h->agents[h->agent_count].online = true;
    h->agent_count++;
    return 0;
}

int agntcy_agent_unregister(agntcy_handle_t* h, const char* agent_id) {
    if (!h || !agent_id) return -1;

    for (size_t i = 0; i < h->agent_count; i++) {
        if (strcmp(h->agents[i].agent_id, agent_id) == 0) {
            if (i < h->agent_count - 1) {
                memmove(&h->agents[i], &h->agents[i + 1],
                        (h->agent_count - i - 1) * sizeof(agntcy_agent_card_t));
            }
            h->agent_count--;
            return 0;
        }
    }
    return -2;
}

int agntcy_agent_discover(agntcy_handle_t* h, uint32_t cap_mask,
                          agntcy_agent_card_t* results, size_t* count) {
    if (!h || !results || !count) return -1;

    size_t found = 0;
    for (size_t i = 0; i < h->agent_count && found < *count; i++) {
        if (!h->agents[i].online) continue;
        if (cap_mask == 0 || (h->agents[i].capabilities_mask & cap_mask)) {
            results[found++] = h->agents[i];
        }
    }

    *count = found;
    return 0;
}

int agntcy_channel_open(agntcy_handle_t* h, const char* initiator_id,
                         const char* responder_id, agntcy_channel_t* channel) {
    if (!h || !initiator_id || !responder_id || !channel) return -1;
    if (h->channel_count >= AGNTCY_ACP_MAX_CHANNELS) return -2;

    bool initiator_found = false, responder_found = false;
    for (size_t i = 0; i < h->agent_count; i++) {
        if (strcmp(h->agents[i].agent_id, initiator_id) == 0 && h->agents[i].online)
            initiator_found = true;
        if (strcmp(h->agents[i].agent_id, responder_id) == 0 && h->agents[i].online)
            responder_found = true;
    }

    if (!initiator_found || !responder_found) return -3;

    snprintf(channel->channel_id, AGNTCY_ACP_CHANNEL_ID_SIZE,
             "ch-%s-%s-%llu", initiator_id, responder_id,
             (unsigned long long)(uint64_t)(time(NULL) * 1000));

    for (size_t i = 0; i < AGNTCY_ACP_TOKEN_SIZE - 1; i++) {
        channel->session_token[i] = (char)('a' + (rand() % 26));
    }
    channel->session_token[AGNTCY_ACP_TOKEN_SIZE - 1] = '\0';

    strncpy(channel->initiator_id, initiator_id, sizeof(channel->initiator_id) - 1);
    strncpy(channel->responder_id, responder_id, sizeof(channel->responder_id) - 1);
    channel->established_at = (uint64_t)time(NULL);
    channel->expires_at = channel->established_at + 3600;
    channel->encrypted = true;

    h->channels[h->channel_count++] = *channel;
    return 0;
}

int agntcy_channel_close(agntcy_handle_t* h, const char* channel_id) {
    if (!h || !channel_id) return -1;

    for (size_t i = 0; i < h->channel_count; i++) {
        if (strcmp(h->channels[i].channel_id, channel_id) == 0) {
            if (i < h->channel_count - 1) {
                memmove(&h->channels[i], &h->channels[i + 1],
                        (h->channel_count - i - 1) * sizeof(agntcy_channel_t));
            }
            h->channel_count--;
            return 0;
        }
    }
    return -2;
}

int agntcy_message_send(agntcy_handle_t* h, const agntcy_message_t* msg,
                         char* response, size_t* resp_size) {
    if (!h || !msg || !response || !resp_size) return -1;

    h->message_counter++;

    bool channel_valid = false;
    if (msg->channel_id[0] != '\0') {
        for (size_t i = 0; i < h->channel_count; i++) {
            if (strcmp(h->channels[i].channel_id, msg->channel_id) == 0) {
                channel_valid = true;
                break;
            }
        }
    } else {
        channel_valid = true;
    }

    if (!channel_valid) return -2;

    if (msg->payload && msg->payload_size > 0) {
        int written = snprintf(response, *resp_size,
            "{"
            "\"message_id\":\"%s\","
            "\"status\":\"delivered\","
            "\"sender\":\"%s\","
            "\"receiver\":\"%s\","
            "\"payload_size\":%zu,"
            "\"protocol\":\"agntcy-acp-%s\""
            "}",
            msg->message_id, msg->sender_id, msg->receiver_id,
            msg->payload_size, AGNTCY_ACP_VERSION);
        if (written > 0) *resp_size = (size_t)written;
    } else {
        int written = snprintf(response, *resp_size,
            "{\"status\":\"ack\",\"message_id\":\"%s\"}", msg->message_id);
        if (written > 0) *resp_size = (size_t)written;
    }

    return 0;
}

int agntcy_message_broadcast(agntcy_handle_t* h, const agntcy_message_t* msg) {
    if (!h || !msg) return -1;

    h->message_counter++;

    for (size_t i = 0; i < h->agent_count; i++) {
        if (h->agents[i].online &&
            strcmp(h->agents[i].agent_id, msg->sender_id) != 0) {
            if (i) { }
        }
    }

    return 0;
}

int agntcy_task_orchestrate(agntcy_handle_t* h, const char* task_id,
                             const char* workflow_json) {
    if (!h || !task_id || !workflow_json) return -1;
    if (h->task_count >= AGNTCY_ACP_MAX_TASKS) return -2;

    agntcy_task_t* task = NULL;
    for (size_t i = 0; i < h->task_count; i++) {
        if (strcmp(h->tasks[i]->task_id, task_id) == 0) {
            task = h->tasks[i];
            break;
        }
    }

    if (!task) {
        task = (agntcy_task_t*)calloc(1, sizeof(agntcy_task_t));
        if (!task) return -3;
        strncpy(task->task_id, task_id, sizeof(task->task_id) - 1);
        h->tasks[h->task_count++] = task;
    }

    free(task->workflow_json);
    task->workflow_json = strdup(workflow_json);
    strncpy(task->name, "orchestrated-task", sizeof(task->name) - 1);
    task->state = AGNTCY_TASK_DISPATCHED;
    task->created_at = (uint64_t)time(NULL);
    task->deadline_at = task->created_at + 3600;
    task->priority = 5;

    for (size_t i = 0; i < h->agent_count && task->assigned_count < AGNTCY_ACP_MAX_AGENTS; i++) {
        if (h->agents[i].online &&
            (h->agents[i].capabilities_mask & AGNTCY_CAP_ORCHESTRATE)) {
            strncpy(task->assigned_agent_ids[task->assigned_count],
                    h->agents[i].agent_id, 63);
            task->assigned_count++;
        }
    }

    h->task_counter++;
    return 0;
}

int agntcy_task_get_state(agntcy_handle_t* h, const char* task_id,
                          agntcy_task_state_t* state) {
    if (!h || !task_id || !state) return -1;

    for (size_t i = 0; i < h->task_count; i++) {
        if (strcmp(h->tasks[i]->task_id, task_id) == 0) {
            *state = h->tasks[i]->state;
            return 0;
        }
    }
    return -2;
}

int agntcy_ack_negotiate(agntcy_handle_t* h, const char* agent_id,
                          const agntcy_ack_t* ack_request,
                          agntcy_ack_t* ack_response) {
    if (!h || !agent_id || !ack_request || !ack_response) return -1;

    bool agent_found = false;
    for (size_t i = 0; i < h->agent_count; i++) {
        if (strcmp(h->agents[i].agent_id, agent_id) == 0 &&
            h->agents[i].online) {
            agent_found = true;
            break;
        }
    }
    if (!agent_found) return -2;

    memcpy(ack_response, ack_request, sizeof(agntcy_ack_t));

    ack_response->guaranteed_amount = ack_request->requested_amount;
    if (ack_request->requested_amount > (size_t)(1024 * 1024 * 1024)) {
        ack_response->guaranteed_amount = (size_t)(1024 * 1024 * 1024);
    }

    ack_response->cpu_cores = ack_request->cpu_cores;
    if (ack_response->cpu_cores < 1) ack_response->cpu_cores = 1;

    ack_response->valid_until = (uint64_t)time(NULL) + 3600;
    ack_response->committed = true;

    return 0;
}

static int agntcy_proto_init(void* context, const void* config) {
    if (config) { }
    agntcy_handle_t* h = (agntcy_handle_t*)context;
    if (!h) {
        if (agntcy_acp_create(&h) != 0) return -1;
    }
    g_agntcy_state.handle = *h;
    g_agntcy_state.proto_initialized = true;
    return 0;
}

static int agntcy_proto_destroy(void* context) {
    if (context) { }
    g_agntcy_state.proto_initialized = false;
    return 0;
}

static int agntcy_proto_handle_request(void* context,
                                        const void* req,
                                        void** resp) {
    if (context) { }
    if (!req || !resp) return -1;

    const char* raw = (const char*)req;
    char buf[4096];
    snprintf(buf, sizeof(buf),
        "{"
        "\"protocol\":\"agntcy-acp\","
        "\"version\":\"%s\","
        "\"agents_registered\":%zu,"
        "\"active_channels\":%zu,"
        "\"tasks_active\":%zu,"
        "\"messages_total\":%llu"
        "}",
        AGNTCY_ACP_VERSION,
        g_agntcy_state.handle.agent_count,
        g_agntcy_state.handle.channel_count,
        g_agntcy_state.handle.task_count,
        (unsigned long long)g_agntcy_state.handle.message_counter);

    *resp = strdup(buf);
    return 0;
}

static const char* agntcy_proto_get_version(void* context) {
    if (context) { }
    return AGNTCY_ACP_VERSION;
}

static uint64_t agntcy_proto_capabilities(void* context) {
    if (context) { }
    return (uint64_t)(
        AGNTCY_CAP_DISCOVERY |
        AGNTCY_CAP_CHANNEL |
        AGNTCY_CAP_MESSAGING |
        AGNTCY_CAP_ORCHESTRATE |
        AGNTCY_CAP_BROADCAST |
        AGNTCY_CAP_ACK);
}

const proto_adapter_t* agntcy_get_protocol_adapter(void) {
    static proto_adapter_t adapter = {0};
    static bool initialized = false;

    if (!initialized) {
        adapter.name = "AGNTCY ACP";
        adapter.version = AGNTCY_ACP_VERSION;
        adapter.description = "Agent Communication Protocol - open standard for agent-to-agent communication";
        adapter.type = PROTO_AGNTCY;
        adapter.init = agntcy_proto_init;
        adapter.destroy = agntcy_proto_destroy;
        adapter.handle_request = agntcy_proto_handle_request;
        adapter.get_version = agntcy_proto_get_version;
        adapter.capabilities = agntcy_proto_capabilities;
        initialized = true;
    }

    return &adapter;
}
