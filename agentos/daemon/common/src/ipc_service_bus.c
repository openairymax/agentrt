// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ipc_service_bus.c
 * @brief IPC服务总线实现 - 守护进程间统一通信框架
 *
 * 实现守护进程间的高效通信抽象层，集成UnifiedProtocol协议栈，
 * 支持多协议消息传递、服务发现和负载均衡。
 *
 * @see ipc_service_bus.h
 */

#include "ipc_service_bus.h"
#include "svc_logger.h"
#include "platform.h"
#include "error.h"
#include "safe_string_utils.h"
#include "svc_common.h"
#include "ipc_client.h"

#include "include/memory_compat.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================== 内部常量 ==================== */

#define IPC_BUS_MAX_HANDLERS    16
#define IPC_BUS_MAX_EVENTS     32
#define IPC_BUS_MAX_PENDING    256
#define IPC_BUS_HASH_SEED      0x9e3779b9

/* ==================== 内部数据结构 ==================== */

typedef struct {
    ipc_bus_message_handler_t handler;
    void* user_data;
} message_handler_entry_t;

typedef struct {
    char event_name[64];
    ipc_bus_event_handler_t handler;
    void* user_data;
} event_handler_entry_t;

typedef struct {
    uint64_t msg_id;
    ipc_bus_message_t* response;
    volatile int completed;
    agentos_platform_mutex_t mutex;
    agentos_platform_cond_t cond;
} pending_request_t;

typedef struct ipc_bus_channel_s {
    char name[IPC_BUS_CHANNEL_NAME_LEN];
    ipc_bus_channel_config_t config;
    message_handler_entry_t handlers[IPC_BUS_MAX_HANDLERS];
    uint32_t handler_count;
    bool active;
    struct ipc_bus_channel_s* next;
} ipc_bus_channel_internal_t;

typedef struct ipc_service_bus_s {
    char name[IPC_BUS_SERVICE_ID_LEN];
    ipc_bus_channel_config_t default_config;
    ipc_bus_endpoint_t endpoints[IPC_BUS_MAX_SERVICES];
    uint32_t endpoint_count;
    ipc_bus_channel_internal_t* channels;
    uint32_t channel_count;
    event_handler_entry_t event_handlers[IPC_BUS_MAX_EVENTS];
    uint32_t event_handler_count;
    pending_request_t pending[IPC_BUS_MAX_PENDING];
    uint32_t pending_count;
    ipc_bus_stats_t stats;
    bool running;
    agentos_platform_mutex_t mutex;
    uint64_t next_msg_id;
} ipc_service_bus_internal_t;

static uint64_t g_bus_instance_count = 0;

/* ==================== 辅助函数 ==================== */

static uint64_t __attribute__((unused)) generate_msg_id(ipc_service_bus_internal_t* bus) {
    uint64_t id = bus->next_msg_id++;
    if (bus->next_msg_id == 0) bus->next_msg_id = 1;
    return id;
}

static uint32_t compute_checksum(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

static ipc_bus_channel_internal_t* find_channel(
    ipc_service_bus_internal_t* bus,
    const char* name
) {
    ipc_bus_channel_internal_t* ch = bus->channels;
    while (ch) {
        if (strcmp(ch->name, name) == 0) return ch;
        ch = ch->next;
    }
    return NULL;
}

static int32_t find_endpoint_index(
    ipc_service_bus_internal_t* bus,
    const char* service_name
) {
    for (uint32_t i = 0; i < bus->endpoint_count; i++) {
        if (strcmp(bus->endpoints[i].service_name, service_name) == 0)
            return (int32_t)i;
    }
    return -1;
}

static void init_message_header(
    ipc_bus_message_header_t* header,
    ipc_bus_msg_type_t msg_type,
    ipc_bus_proto_t protocol,
    const char* source,
    const char* target
) {
    memset(header, 0, sizeof(ipc_bus_message_header_t));
    header->magic = IPC_BUS_MESSAGE_MAGIC;
    header->version = IPC_BUS_MESSAGE_VERSION;
    header->msg_type = msg_type;
    header->protocol = protocol;
    header->timestamp = agentos_platform_get_time_ms();
    if (source) safe_strcpy(header->source, source, IPC_BUS_SERVICE_ID_LEN);
    if (target) safe_strcpy(header->target, target, IPC_BUS_SERVICE_ID_LEN);
}

/* ==================== 公共API实现 ==================== */

AGENTOS_API ipc_service_bus_t ipc_service_bus_create(
    const char* bus_name,
    const ipc_bus_channel_config_t* config
) {
    if (!bus_name) return NULL;

    ipc_service_bus_internal_t* bus =
        (ipc_service_bus_internal_t*)AGENTOS_CALLOC(1, sizeof(ipc_service_bus_internal_t));
    if (!bus) return NULL;

    if (safe_strcpy(bus->name, bus_name, IPC_BUS_SERVICE_ID_LEN) != 0) {
        AGENTOS_FREE(bus);
        return NULL;
    }

    if (config) {
        memcpy(&bus->default_config, config, sizeof(ipc_bus_channel_config_t));
    } else {
        memset(&bus->default_config, 0, sizeof(ipc_bus_channel_config_t));
        safe_strcpy(bus->default_config.name, "default", IPC_BUS_CHANNEL_NAME_LEN);
        bus->default_config.default_protocol = IPC_BUS_PROTO_JSON_RPC;
        bus->default_config.timeout_ms = IPC_BUS_DEFAULT_TIMEOUT_MS;
        bus->default_config.max_retries = IPC_BUS_MAX_RETRIES;
        bus->default_config.buffer_size = IPC_BUS_MAX_MESSAGE_SIZE;
    }

    agentos_error_t err = agentos_platform_mutex_init(&bus->mutex);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(bus);
        return NULL;
    }

    bus->running = false;
    bus->next_msg_id = 1;
    g_bus_instance_count++;

    LOG_INFO("IPC service bus '%s' created", bus_name);
    return (ipc_service_bus_t)bus;
}

AGENTOS_API void ipc_service_bus_destroy(ipc_service_bus_t bus_handle) {
    if (!bus_handle) return;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    if (bus->running) {
        ipc_service_bus_stop(bus_handle);
    }

    ipc_bus_channel_internal_t* ch = bus->channels;
    while (ch) {
        ipc_bus_channel_internal_t* next = ch->next;
        AGENTOS_FREE(ch);
        ch = next;
    }

    for (uint32_t i = 0; i < bus->pending_count; i++) {
        if (bus->pending[i].response) {
            ipc_bus_message_free(bus->pending[i].response);
        }
    }

    agentos_platform_mutex_destroy(&bus->mutex);
    AGENTOS_FREE(bus);

    LOG_INFO("IPC service bus destroyed");
}

AGENTOS_API agentos_error_t ipc_service_bus_start(ipc_service_bus_t bus_handle) {
    if (!bus_handle) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);
    if (bus->running) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_SUCCESS;
    }

    bus->running = true;
    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_INFO("IPC service bus '%s' started", bus->name);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_stop(ipc_service_bus_t bus_handle) {
    if (!bus_handle) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);
    bus->running = false;
    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_INFO("IPC service bus '%s' stopped", bus->name);
    return AGENTOS_SUCCESS;
}

/* ==================== 通道管理 ==================== */

AGENTOS_API ipc_bus_channel_t ipc_bus_channel_create(
    ipc_service_bus_t bus_handle,
    const ipc_bus_channel_config_t* config
) {
    if (!bus_handle || !config) return NULL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    if (bus->channel_count >= IPC_BUS_MAX_CHANNELS) {
        agentos_platform_mutex_unlock(&bus->mutex);
        LOG_ERROR("Cannot create channel: max channels reached");
        return NULL;
    }

    if (find_channel(bus, config->name)) {
        agentos_platform_mutex_unlock(&bus->mutex);
        LOG_ERROR("Channel '%s' already exists", config->name);
        return NULL;
    }

    ipc_bus_channel_internal_t* ch =
        (ipc_bus_channel_internal_t*)AGENTOS_CALLOC(1, sizeof(ipc_bus_channel_internal_t));
    if (!ch) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return NULL;
    }

    memcpy(&ch->config, config, sizeof(ipc_bus_channel_config_t));
    safe_strcpy(ch->name, config->name, IPC_BUS_CHANNEL_NAME_LEN);
    ch->active = true;
    ch->next = bus->channels;
    bus->channels = ch;
    bus->channel_count++;

    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_INFO("Channel '%s' created on bus '%s'", config->name, bus->name);
    return (ipc_bus_channel_t)ch;
}

AGENTOS_API void ipc_bus_channel_destroy(ipc_bus_channel_t channel) {
    if (!channel) return;

    ipc_bus_channel_internal_t* ch = (ipc_bus_channel_internal_t*)channel;
    ch->active = false;

    LOG_INFO("Channel '%s' destroyed", ch->name);
}

AGENTOS_API const char* ipc_bus_channel_get_name(ipc_bus_channel_t channel) {
    if (!channel) return NULL;
    ipc_bus_channel_internal_t* ch = (ipc_bus_channel_internal_t*)channel;
    return ch->name;
}

/* ==================== 消息发送 ==================== */

AGENTOS_API agentos_error_t ipc_service_bus_send(
    ipc_service_bus_t bus_handle,
    const char* target_service,
    const ipc_bus_message_t* message
) {
    if (!bus_handle || !target_service || !message) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    if (!bus->running) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ESTATE;
    }

    bus->stats.messages_sent++;
    bus->stats.bytes_sent += message->payload_size;

    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_DEBUG("Bus '%s': sent message to '%s' (type=%d, proto=%d, size=%zu)",
              bus->name, target_service,
              message->header.msg_type, message->header.protocol,
              message->payload_size);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_request(
    ipc_service_bus_t bus_handle,
    const char* target_service,
    const ipc_bus_message_t* request,
    ipc_bus_message_t* response,
    uint32_t timeout_ms
) {
    if (!bus_handle || !target_service || !request || !response)
        return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    if (!bus->running) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ESTATE;
    }

    if (bus->pending_count >= IPC_BUS_MAX_PENDING) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_EBUSY;
    }

    uint64_t start_time = agentos_platform_get_time_ms();

    pending_request_t* pending = &bus->pending[bus->pending_count];
    pending->msg_id = request->header.msg_id;
    pending->response = NULL;
    pending->completed = 0;
    bus->pending_count++;

    bus->stats.messages_sent++;
    bus->stats.bytes_sent += request->payload_size;

    agentos_platform_mutex_unlock(&bus->mutex);

    if (timeout_ms == 0) timeout_ms = bus->default_config.timeout_ms;

    const char* req_payload = (const char*)request->payload;
    char* resp_json = NULL;
    agentos_error_t svc_err = AGENTOS_SUCCESS;

    char rpc_method[256];
    snprintf(rpc_method, sizeof(rpc_method), "%s.handle", target_service);

    int rpc_err = svc_rpc_call(rpc_method,
        req_payload ? req_payload : "{}", &resp_json, timeout_ms);
    if (rpc_err != 0) {
        svc_err = AGENTOS_EIO;
    }

    agentos_platform_mutex_lock(&bus->mutex);

    if (svc_err == AGENTOS_SUCCESS && resp_json) {
        size_t resp_len = strlen(resp_json) + 1;
        pending->response = (ipc_bus_message_t*)calloc(1, sizeof(ipc_bus_message_t));
        if (pending->response) {
            pending->response->header.msg_type = IPC_BUS_MSG_RESPONSE;
            pending->response->header.protocol = request->header.protocol;
            snprintf(pending->response->header.target, sizeof(pending->response->header.target),
                     "%s", request->header.source);
            snprintf(pending->response->header.source, sizeof(pending->response->header.source),
                     "%s", target_service);
            pending->response->payload = resp_json;
            pending->response->payload_size = resp_len;
            pending->completed = 1;
        } else {
            free(resp_json);
            pending->completed = 0;
        }
    } else {
        size_t err_len = 128;
        char* err_payload = (char*)malloc(err_len);
        if (err_payload) {
            int elen = snprintf(err_payload, err_len,
                "{\"error\":{\"code\":%d,\"message\":\"service_call_failed\"}}", svc_err);
            pending->response = (ipc_bus_message_t*)calloc(1, sizeof(ipc_bus_message_t));
            if (pending->response) {
                pending->response->header.msg_type = IPC_BUS_MSG_RESPONSE;
                pending->response->header.protocol = request->header.protocol;
                pending->response->payload = err_payload;
                pending->response->payload_size = (size_t)(elen > 0 ? elen : 0) + 1;
                pending->completed = 1;
            } else {
                free(err_payload);
            }
        }
        if (resp_json) free(resp_json);
    }

    uint64_t elapsed = agentos_platform_get_time_ms() - start_time;
    if (elapsed >= (uint64_t)timeout_ms && !pending->completed) {
        bus->stats.timeouts++;
        bus->pending_count--;
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ETIMEDOUT;
    }

    if (pending->completed && pending->response) {
        memcpy(response, pending->response, sizeof(ipc_bus_message_t));
        ipc_bus_message_free(pending->response);
        pending->response = NULL;
    }

    bus->pending_count--;
    bus->stats.messages_received++;
    uint64_t latency = agentos_platform_get_time_ms() - start_time;
    bus->stats.avg_latency_us = bus->stats.avg_latency_us == 0
        ? latency * 1000
        : (bus->stats.avg_latency_us + latency * 1000) / 2;
    if (latency * 1000 > bus->stats.max_latency_us)
        bus->stats.max_latency_us = latency * 1000;

    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_DEBUG("Bus '%s': request to '%s' completed in %llums (completed=%d)",
              bus->name, target_service, (unsigned long long)latency, pending->completed);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_broadcast(
    ipc_service_bus_t bus_handle,
    const ipc_bus_message_t* message
) {
    if (!bus_handle || !message) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    if (!bus->running) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ESTATE;
    }

    uint32_t target_count = 0;
    for (uint32_t i = 0; i < bus->endpoint_count; i++) {
        if (bus->endpoints[i].healthy) target_count++;
    }

    bus->stats.messages_sent += target_count;
    bus->stats.bytes_sent += message->payload_size * target_count;

    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_DEBUG("Bus '%s': broadcast to %u endpoints", bus->name, target_count);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_notify(
    ipc_service_bus_t bus_handle,
    const char* target_service,
    const void* payload,
    size_t payload_size,
    ipc_bus_proto_t protocol
) {
    if (!bus_handle || !target_service || !payload) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    ipc_bus_message_t* msg = ipc_bus_message_create(
        IPC_BUS_MSG_NOTIFICATION, protocol, payload, payload_size);
    if (!msg) return AGENTOS_ENOMEM;

    init_message_header(&msg->header, IPC_BUS_MSG_NOTIFICATION, protocol,
                        bus->name, target_service);
    msg->header.payload_len = (uint32_t)payload_size;

    agentos_error_t err = ipc_service_bus_send(bus_handle, target_service, msg);
    ipc_bus_message_free(msg);

    return err;
}

/* ==================== 消息接收 ==================== */

AGENTOS_API agentos_error_t ipc_service_bus_register_handler(
    ipc_service_bus_t bus_handle,
    ipc_bus_message_handler_t handler,
    void* user_data
) {
    if (!bus_handle || !handler) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    if (bus->channel_count == 0) {
        ipc_bus_channel_config_t config;
        memcpy(&config, &bus->default_config, sizeof(ipc_bus_channel_config_t));
        safe_strcpy(config.name, "default", IPC_BUS_CHANNEL_NAME_LEN);
        agentos_platform_mutex_unlock(&bus->mutex);

        ipc_bus_channel_t ch = ipc_bus_channel_create(bus_handle, &config);
        if (!ch) return AGENTOS_ENOMEM;

        agentos_platform_mutex_lock(&bus->mutex);
    }

    ipc_bus_channel_internal_t* ch = bus->channels;
    if (!ch || ch->handler_count >= IPC_BUS_MAX_HANDLERS) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ENOMEM;
    }

    ch->handlers[ch->handler_count].handler = handler;
    ch->handlers[ch->handler_count].user_data = user_data;
    ch->handler_count++;

    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_INFO("Message handler registered on bus '%s'", bus->name);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_unregister_handler(
    ipc_service_bus_t bus_handle,
    ipc_bus_message_handler_t handler
) {
    if (!bus_handle || !handler) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    ipc_bus_channel_internal_t* ch = bus->channels;
    while (ch) {
        for (uint32_t i = 0; i < ch->handler_count; i++) {
            if (ch->handlers[i].handler == handler) {
                if (i < ch->handler_count - 1) {
                    ch->handlers[i] = ch->handlers[ch->handler_count - 1];
                }
                ch->handler_count--;
                break;
            }
        }
        ch = ch->next;
    }

    agentos_platform_mutex_unlock(&bus->mutex);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_register_event_handler(
    ipc_service_bus_t bus_handle,
    const char* event_name,
    ipc_bus_event_handler_t handler,
    void* user_data
) {
    if (!bus_handle || !event_name || !handler) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    if (bus->event_handler_count >= IPC_BUS_MAX_EVENTS) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ENOMEM;
    }

    event_handler_entry_t* entry = &bus->event_handlers[bus->event_handler_count];
    safe_strcpy(entry->event_name, event_name, sizeof(entry->event_name));
    entry->handler = handler;
    entry->user_data = user_data;
    bus->event_handler_count++;

    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_INFO("Event handler registered for '%s' on bus '%s'", event_name, bus->name);
    return AGENTOS_SUCCESS;
}

/* ==================== 服务端点管理 ==================== */

AGENTOS_API agentos_error_t ipc_service_bus_register_endpoint(
    ipc_service_bus_t bus_handle,
    const ipc_bus_endpoint_t* endpoint
) {
    if (!bus_handle || !endpoint) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    int32_t idx = find_endpoint_index(bus, endpoint->service_name);
    if (idx >= 0) {
        memcpy(&bus->endpoints[idx], endpoint, sizeof(ipc_bus_endpoint_t));
        bus->endpoints[idx].last_heartbeat = agentos_platform_get_time_ms();
        agentos_platform_mutex_unlock(&bus->mutex);
        LOG_INFO("Endpoint '%s' updated on bus '%s'",
                 endpoint->service_name, bus->name);
        return AGENTOS_SUCCESS;
    }

    if (bus->endpoint_count >= IPC_BUS_MAX_SERVICES) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ENOMEM;
    }

    memcpy(&bus->endpoints[bus->endpoint_count], endpoint, sizeof(ipc_bus_endpoint_t));
    bus->endpoints[bus->endpoint_count].last_heartbeat = agentos_platform_get_time_ms();
    bus->endpoint_count++;
    bus->stats.active_endpoints = bus->endpoint_count;

    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_INFO("Endpoint '%s' registered on bus '%s' (endpoint=%s)",
             endpoint->service_name, bus->name, endpoint->endpoint);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_unregister_endpoint(
    ipc_service_bus_t bus_handle,
    const char* service_name
) {
    if (!bus_handle || !service_name) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    int32_t idx = find_endpoint_index(bus, service_name);
    if (idx < 0) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ENOENT;
    }

    if ((uint32_t)idx < bus->endpoint_count - 1) {
        bus->endpoints[idx] = bus->endpoints[bus->endpoint_count - 1];
    }
    memset(&bus->endpoints[bus->endpoint_count - 1], 0, sizeof(ipc_bus_endpoint_t));
    bus->endpoint_count--;
    bus->stats.active_endpoints = bus->endpoint_count;

    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_INFO("Endpoint '%s' unregistered from bus '%s'", service_name, bus->name);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_discover(
    ipc_service_bus_t bus_handle,
    const char* service_name,
    ipc_bus_proto_t protocol,
    ipc_bus_endpoint_t* endpoints,
    uint32_t max_count,
    uint32_t* found_count
) {
    if (!bus_handle || !endpoints || !found_count) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < bus->endpoint_count && count < max_count; i++) {
        ipc_bus_endpoint_t* ep = &bus->endpoints[i];

        if (service_name && service_name[0] &&
            strcmp(ep->service_name, service_name) != 0)
            continue;

        if (protocol != IPC_BUS_PROTO_AUTO) {
            bool proto_match = false;
            for (uint32_t p = 0; p < ep->protocol_count; p++) {
                if (ep->supported_protocols[p] == protocol) {
                    proto_match = true;
                    break;
                }
            }
            if (!proto_match) continue;
        }

        memcpy(&endpoints[count], ep, sizeof(ipc_bus_endpoint_t));
        count++;
    }

    *found_count = count;
    agentos_platform_mutex_unlock(&bus->mutex);

    LOG_DEBUG("Service discovery: found %u endpoints (name=%s, proto=%d)",
              count, service_name ? service_name : "*", protocol);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_select_endpoint(
    ipc_service_bus_t bus_handle,
    const char* service_name,
    ipc_bus_proto_t protocol,
    ipc_bus_endpoint_t* endpoint
) {
    if (!bus_handle || !service_name || !endpoint) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    ipc_bus_endpoint_t* best = NULL;
    uint32_t best_load = UINT32_MAX;

    for (uint32_t i = 0; i < bus->endpoint_count; i++) {
        ipc_bus_endpoint_t* ep = &bus->endpoints[i];

        if (strcmp(ep->service_name, service_name) != 0) continue;
        if (!ep->healthy) continue;

        if (protocol != IPC_BUS_PROTO_AUTO) {
            bool proto_match = false;
            for (uint32_t p = 0; p < ep->protocol_count; p++) {
                if (ep->supported_protocols[p] == protocol) {
                    proto_match = true;
                    break;
                }
            }
            if (!proto_match) continue;
        }

        uint32_t load = ep->max_connections > 0
            ? ep->active_connections * 100 / ep->max_connections
            : 0;
        if (ep->weight > 0) load = load / ep->weight;

        if (load < best_load) {
            best_load = load;
            best = ep;
        }
    }

    if (!best) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ENOENT;
    }

    memcpy(endpoint, best, sizeof(ipc_bus_endpoint_t));
    agentos_platform_mutex_unlock(&bus->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_update_endpoint_health(
    ipc_service_bus_t bus_handle,
    const char* service_name,
    bool healthy
) {
    if (!bus_handle || !service_name) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);

    int32_t idx = find_endpoint_index(bus, service_name);
    if (idx < 0) {
        agentos_platform_mutex_unlock(&bus->mutex);
        return AGENTOS_ENOENT;
    }

    bool was_healthy = bus->endpoints[idx].healthy;
    bus->endpoints[idx].healthy = healthy;
    bus->endpoints[idx].last_heartbeat = agentos_platform_get_time_ms();

    agentos_platform_mutex_unlock(&bus->mutex);

    if (was_healthy && !healthy) {
        LOG_WARN("Endpoint '%s' became unhealthy on bus '%s'",
                 service_name, bus->name);
    } else if (!was_healthy && healthy) {
        LOG_INFO("Endpoint '%s' recovered on bus '%s'",
                 service_name, bus->name);
    }

    return AGENTOS_SUCCESS;
}

/* ==================== 消息辅助函数 ==================== */

AGENTOS_API ipc_bus_message_t* ipc_bus_message_create(
    ipc_bus_msg_type_t msg_type,
    ipc_bus_proto_t protocol,
    const void* payload,
    size_t payload_size
) {
    ipc_bus_message_t* msg = (ipc_bus_message_t*)AGENTOS_CALLOC(1, sizeof(ipc_bus_message_t));
    if (!msg) return NULL;

    init_message_header(&msg->header, msg_type, protocol, NULL, NULL);
    msg->header.msg_id = (uint64_t)agentos_platform_get_time_ms();
    msg->header.payload_len = (uint32_t)payload_size;

    if (payload && payload_size > 0) {
        msg->payload = AGENTOS_CALLOC(1, payload_size);
        if (!msg->payload) {
            AGENTOS_FREE(msg);
            return NULL;
        }
        memcpy(msg->payload, payload, payload_size);
        msg->payload_size = payload_size;
        msg->header.checksum = compute_checksum(payload, payload_size);
    }

    return msg;
}

AGENTOS_API void ipc_bus_message_free(ipc_bus_message_t* message) {
    if (!message) return;
    if (message->payload) {
        AGENTOS_FREE(message->payload);
        message->payload = NULL;
    }
    AGENTOS_FREE(message);
}

AGENTOS_API ipc_bus_message_t* ipc_bus_message_clone(
    const ipc_bus_message_t* message
) {
    if (!message) return NULL;

    ipc_bus_message_t* clone = ipc_bus_message_create(
        message->header.msg_type,
        message->header.protocol,
        message->payload,
        message->payload_size
    );
    if (!clone) return NULL;

    clone->header = message->header;
    return clone;
}

AGENTOS_API const char* ipc_bus_proto_to_string(ipc_bus_proto_t proto) {
    static const char* proto_strings[] = {
        "JSON-RPC",
        "MCP",
        "A2A",
        "OpenAI",
        "AUTO"
    };

    if (proto < 0 || proto > IPC_BUS_PROTO_AUTO) return "UNKNOWN";
    return proto_strings[proto];
}

AGENTOS_API ipc_bus_proto_t ipc_bus_proto_from_string(const char* str) {
    if (!str) return IPC_BUS_PROTO_AUTO;

    if (strcasecmp(str, "JSON-RPC") == 0 || strcasecmp(str, "jsonrpc") == 0)
        return IPC_BUS_PROTO_JSON_RPC;
    if (strcasecmp(str, "MCP") == 0) return IPC_BUS_PROTO_MCP;
    if (strcasecmp(str, "A2A") == 0) return IPC_BUS_PROTO_A2A;
    if (strcasecmp(str, "OpenAI") == 0 || strcasecmp(str, "openai") == 0)
        return IPC_BUS_PROTO_OPENAI;

    return IPC_BUS_PROTO_AUTO;
}

/* ==================== 统计与诊断 ==================== */

AGENTOS_API agentos_error_t ipc_service_bus_get_stats(
    ipc_service_bus_t bus_handle,
    ipc_bus_stats_t* stats
) {
    if (!bus_handle || !stats) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);
    memcpy(stats, &bus->stats, sizeof(ipc_bus_stats_t));
    stats->active_channels = bus->channel_count;
    stats->active_endpoints = bus->endpoint_count;
    agentos_platform_mutex_unlock(&bus->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t ipc_service_bus_reset_stats(ipc_service_bus_t bus_handle) {
    if (!bus_handle) return AGENTOS_EINVAL;

    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;

    agentos_platform_mutex_lock(&bus->mutex);
    memset(&bus->stats, 0, sizeof(ipc_bus_stats_t));
    agentos_platform_mutex_unlock(&bus->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API const char* ipc_service_bus_get_name(ipc_service_bus_t bus_handle) {
    if (!bus_handle) return NULL;
    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;
    return bus->name;
}

AGENTOS_API bool ipc_service_bus_is_running(ipc_service_bus_t bus_handle) {
    if (!bus_handle) return false;
    ipc_service_bus_internal_t* bus = (ipc_service_bus_internal_t*)bus_handle;
    return bus->running;
}
