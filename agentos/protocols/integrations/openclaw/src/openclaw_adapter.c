// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
// @owner: team-B
/**
 * @file openclaw_adapter.c
 * @brief OpenClaw Platform Integration Adapter Implementation
 */

#define LOG_TAG "openclaw_adapter"

#include "openclaw_adapter.h"

#include "protocol_transformers.h"

#include <stdio.h>
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <pthread.h>
#endif
#include "memory_compat.h"
#include "types.h"

#include <errno.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close_socket(s) closesocket(s)
#define sock_errno WSAGetLastError()
#define SOCK_EINPROGRESS WSAEINPROGRESS
typedef SOCKET socket_fd_t;
#define INVALID_SOCK (-1)
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#define close_socket(s) close(s)
#define sock_errno errno
#define SOCK_EINPROGRESS EINPROGRESS
typedef int socket_fd_t;
#define INVALID_SOCK (-1)
#endif

#define OPENCLAW_SOCKET_TIMEOUT_MS 5000
#define OPENCLAW_RECV_BUFFER_SIZE 65536

struct openclaw_adapter_context_s {
    openclaw_config_t config;
    bool initialized;
    bool connected;
    openclaw_agent_card_t *registered_agents;
    size_t registered_agent_count;
    openclaw_session_t *active_sessions;
    size_t active_session_count;
    openclaw_tool_info_t *registered_tools;
    size_t registered_tool_count;
    openclaw_task_t *tracked_tasks;
    size_t tracked_task_count;
    openclaw_message_handler_t message_handler;
    void *message_handler_data;
    openclaw_task_handler_t task_handler;
    void *task_handler_data;
    openclaw_event_callback_t event_callback;
    void *event_callback_data;
    openclaw_status_callback_t status_callback;
    void *status_callback_data;
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t tasks_delegated;
    uint64_t tasks_completed;
    uint64_t connection_uptime_sec;
    uint64_t connect_timestamp;
    socket_fd_t sock_fd;
    char last_error[256];
};

static openclaw_adapter_context_t *g_openclaw_instance = NULL;

openclaw_config_t openclaw_config_default(void)
{
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

openclaw_adapter_context_t *openclaw_adapter_create(const openclaw_config_t *config)
{
    if (!config)
        return NULL;

    openclaw_adapter_context_t *ctx =
        (openclaw_adapter_context_t *)AGENTOS_CALLOC(1, sizeof(openclaw_adapter_context_t));
    if (!ctx)
        return NULL;

    __builtin_memcpy(&ctx->config, config, sizeof(openclaw_config_t));

    if (config->endpoint_url)
        ctx->config.endpoint_url = AGENTOS_STRDUP(config->endpoint_url);
    if (config->api_key)
        ctx->config.api_key = AGENTOS_STRDUP(config->api_key);
    if (config->organization_id)
        ctx->config.organization_id = AGENTOS_STRDUP(config->organization_id);
    if (config->cluster_id)
        ctx->config.cluster_id = AGENTOS_STRDUP(config->cluster_id);
    if (config->custom_headers_json)
        ctx->config.custom_headers_json = AGENTOS_STRDUP(config->custom_headers_json);

    ctx->initialized = true;
    ctx->connected = false;
    ctx->sock_fd = INVALID_SOCK;
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

void openclaw_adapter_destroy(openclaw_adapter_context_t *ctx)
{
    if (!ctx)
        return;

    if (ctx == g_openclaw_instance)
        g_openclaw_instance = NULL;

    if (ctx->connected)
        openclaw_disconnect(ctx);

    AGENTOS_FREE(ctx->config.endpoint_url);
    AGENTOS_FREE(ctx->config.api_key);
    AGENTOS_FREE(ctx->config.organization_id);
    AGENTOS_FREE(ctx->config.cluster_id);
    AGENTOS_FREE(ctx->config.custom_headers_json);

    for (size_t i = 0; i < ctx->registered_agent_count; i++)
        openclaw_agent_card_destroy(&ctx->registered_agents[i]);
    AGENTOS_FREE(ctx->registered_agents);

    for (size_t i = 0; i < ctx->active_session_count; i++)
        openclaw_session_destroy(&ctx->active_sessions[i]);
    AGENTOS_FREE(ctx->active_sessions);

    for (size_t i = 0; i < ctx->registered_tool_count; i++)
        openclaw_tool_info_destroy(&ctx->registered_tools[i]);
    AGENTOS_FREE(ctx->registered_tools);

    for (size_t i = 0; i < ctx->tracked_task_count; i++)
        openclaw_task_destroy(&ctx->tracked_tasks[i]);
    AGENTOS_FREE(ctx->tracked_tasks);

    AGENTOS_MEMSET(ctx, 0, sizeof(openclaw_adapter_context_t));
    AGENTOS_FREE(ctx);
}

bool openclaw_adapter_is_initialized(const openclaw_adapter_context_t *ctx)
{
    return ctx && ctx->initialized;
}

const char *openclaw_adapter_version(void)
{
    return OPENCLAW_ADAPTER_VERSION;
}

const char *openclaw_adapter_platform_version(void)
{
    return OPENCLAW_PLATFORM_VERSION;
}

static int openclaw_parse_endpoint(const char *endpoint_url, char *host, size_t host_size,
                                   int *port)
{
    if (!endpoint_url || !host || !port)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_parse_endpoint: parse error");
        return AGENTOS_ERR_UNKNOWN;
        }

    const char *url = endpoint_url;
    const char *host_start = url;

    if (strncmp(url, "http://", 7) == 0) {
        host_start = url + 7;
        *port = 80;
    } else if (strncmp(url, "https://", 8) == 0) {
        host_start = url + 8;
        *port = 443;
    } else {
        host_start = url;
        *port = 28080;
    }

    const char *colon = strchr(host_start, ':');
    const char *slash = strchr(host_start, '/');

    if (colon && (!slash || colon < slash)) {
        size_t host_len = (size_t)(colon - host_start);
        if (host_len >= host_size)
            host_len = host_size - 1;
        __builtin_memcpy(host, host_start, host_len);
        host[host_len] = '\0';
        *port = (int)strtol(colon + 1, NULL, 10);
    } else if (slash) {
        size_t host_len = (size_t)(slash - host_start);
        if (host_len >= host_size)
            host_len = host_size - 1;
        __builtin_memcpy(host, host_start, host_len);
        host[host_len] = '\0';
    } else {
        size_t host_len = strlen(host_start);
        if (host_len >= host_size)
            host_len = host_size - 1;
        __builtin_memcpy(host, host_start, host_len);
        host[host_len] = '\0';
    }

    return 0;
}

static int __attribute__((unused)) openclaw_socket_set_nonblocking(socket_fd_t fd)
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "ioctlsocket: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int openclaw_socket_connect(socket_fd_t fd, const char *host, int port, uint32_t timeout_ms)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp = NULL;

    AGENTOS_MEMSET(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    int ret = getaddrinfo(host, port_str, &hints, &result);
    if (ret != 0)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "snprintf: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    int connected = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if (connect(fd, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0) {
            connected = 0;
            break;
        }
        if (sock_errno == SOCK_EINPROGRESS) {
            connected = 1;
            break;
        }
    }

    freeaddrinfo(result);

    if (connected == -1)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    if (connected == 1) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;

        int poll_ret = poll(&pfd, 1, (int)timeout_ms);
        if (poll_ret <= 0)
            AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

        int so_error = 0;
        socklen_t len = sizeof(so_error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&so_error, &len) < 0)
            AGENTOS_ERROR(AGENTOS_ERR_OUT_OF_MEMORY, "out of memory");
        if (so_error != 0)
            AGENTOS_ERROR(AGENTOS_ERR_IO, "I/O error");
    }

    return 0;
}

static int openclaw_socket_send(socket_fd_t fd, const void *data, size_t len, uint32_t timeout_ms)
{
    size_t total_sent = 0;

    while (total_sent < len) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;

        int poll_ret = poll(&pfd, 1, (int)timeout_ms);
        if (poll_ret <= 0)
            {
            agentos_error_push_ex(AGENTOS_ERR_TIMEOUT, __FILE__, __LINE__, __func__, "poll: timeout");
            return AGENTOS_ERR_TIMEOUT;
            }

        ssize_t sent = send(fd, (const char *)data + total_sent, len - total_sent, 0);
        if (sent < 0) {
            if (sock_errno == EINTR)
                continue;
            return AGENTOS_EINVAL;
        }
        total_sent += (size_t)sent;
    }

    return 0;
}

static int openclaw_socket_recv(socket_fd_t fd, char *buffer, size_t buffer_size, size_t *out_len,
                                uint32_t timeout_ms)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    int poll_ret = poll(&pfd, 1, (int)timeout_ms);
    if (poll_ret <= 0)
        {
        agentos_error_push_ex(AGENTOS_ERR_TIMEOUT, __FILE__, __LINE__, __func__, "poll: timeout");
        return AGENTOS_ERR_TIMEOUT;
        }

    ssize_t recvd = recv(fd, buffer, buffer_size - 1, 0);
    if (recvd < 0) {
        if (sock_errno == EINTR) {
            *out_len = 0;
            return 0;
        }
        return AGENTOS_EINVAL;
    }
    if (recvd == 0)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    buffer[recvd] = '\0';
    *out_len = (size_t)recvd;
    return 0;
}

static int openclaw_serialize_message(const openclaw_message_t *msg, char *buffer,
                                      size_t buffer_size)
{
    if (!msg || !buffer)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_serialize_message: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    const char *sender = msg->sender_id ? msg->sender_id : "";
    const char *receiver = msg->receiver_id ? msg->receiver_id : "";
    const char *session = msg->session_id ? msg->session_id : "";
    const char *mid = msg->message_id ? msg->message_id : "";
    const char *ctype = msg->content_type ? msg->content_type : "text/plain";
    const char *pload = msg->payload ? (const char *)msg->payload : "";

    return snprintf(buffer, buffer_size,
                    "{"
                    "\"message_id\":\"%s\","
                    "\"session_id\":\"%s\","
                    "\"sender_id\":\"%s\","
                    "\"receiver_id\":\"%s\","
                    "\"modality\":%u,"
                    "\"content_type\":\"%s\","
                    "\"payload\":\"%s\","
                    "\"payload_size\":%zu,"
                    "\"timestamp\":%llu,"
                    "\"priority\":%u"
                    "}",
                    mid, session, sender, receiver, (unsigned int)msg->modality, ctype, pload,
                    msg->payload_size, (unsigned long long)msg->timestamp,
                    (unsigned int)msg->priority);
}

int openclaw_connect(openclaw_adapter_context_t *ctx)
{
    if (!ctx || !ctx->initialized)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_connect: not initialized");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (ctx->connected)
        return 0;

    char host[256] = {0};
    int port = 28080;

    openclaw_parse_endpoint(ctx->config.endpoint_url, host, sizeof(host), &port);

    socket_fd_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCK) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Failed to create socket: errno=%d",
                 sock_errno);
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    }

    int ret = openclaw_socket_connect(fd, host, port, ctx->config.request_timeout_ms);
    if (ret != 0) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Connect failed: %.80s:%d (ret=%d)",
                 host, port, ret);
        close_socket(fd);
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");
    }

    ctx->sock_fd = fd;
    ctx->connected = true;
    ctx->connection_uptime_sec = 0;
    ctx->connect_timestamp = (uint64_t)time(NULL);
    ctx->last_error[0] = '\0';
    return 0;
}

int openclaw_disconnect(openclaw_adapter_context_t *ctx)
{
    if (!ctx || !ctx->connected)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_disconnect: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }

    for (size_t i = 0; i < ctx->active_session_count; i++) {
        if (ctx->active_sessions[i].is_active)
            ctx->active_sessions[i].is_active = false;
    }

    if (ctx->sock_fd != INVALID_SOCK) {
        close_socket(ctx->sock_fd);
        ctx->sock_fd = INVALID_SOCK;
    }

    ctx->connected = false;
    return 0;
}

bool openclaw_is_connected(const openclaw_adapter_context_t *ctx)
{
    return ctx && ctx->connected;
}

int openclaw_register_agent(openclaw_adapter_context_t *ctx, const openclaw_agent_card_t *card)
{
    if (!ctx || !card || !card->agent_id)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_register_agent: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!ctx->connected) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Not connected to OpenClaw platform");
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    }

    for (size_t i = 0; i < ctx->registered_agent_count; i++) {
        if (strcmp(ctx->registered_agents[i].agent_id, card->agent_id) == 0) {
            __builtin_memcpy(&ctx->registered_agents[i], card, sizeof(openclaw_agent_card_t));
            if (card->agent_id)
                ctx->registered_agents[i].agent_id = AGENTOS_STRDUP(card->agent_id);
            if (card->name)
                ctx->registered_agents[i].name = AGENTOS_STRDUP(card->name);
            if (card->description)
                ctx->registered_agents[i].description = AGENTOS_STRDUP(card->description);
            if (card->version)
                ctx->registered_agents[i].version = AGENTOS_STRDUP(card->version);
            return 0;
        }
    }

    openclaw_agent_card_t *new_agents = (openclaw_agent_card_t *)AGENTOS_REALLOC(
        ctx->registered_agents, (ctx->registered_agent_count + 1) * sizeof(openclaw_agent_card_t));
    if (!new_agents)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

    ctx->registered_agents = new_agents;
    AGENTOS_MEMSET(&ctx->registered_agents[ctx->registered_agent_count], 0, sizeof(openclaw_agent_card_t));

    openclaw_agent_card_t *target = &ctx->registered_agents[ctx->registered_agent_count];
    target->agent_id = card->agent_id ? AGENTOS_STRDUP(card->agent_id) : NULL;
    target->name = card->name ? AGENTOS_STRDUP(card->name) : NULL;
    target->description = card->description ? AGENTOS_STRDUP(card->description) : NULL;
    target->version = card->version ? AGENTOS_STRDUP(card->version) : NULL;
    target->supported_modalities = card->supported_modalities;
    target->security_level = card->security_level;
    target->max_concurrent_tasks = card->max_concurrent_tasks > 0 ? card->max_concurrent_tasks : 8;
    target->is_active = true;
    target->created_at = (uint64_t)(time(NULL));
    target->last_heartbeat = target->created_at;

    ctx->registered_agent_count++;
    return 0;
}

int openclaw_discover_agents(openclaw_adapter_context_t *ctx, const char *capability_filter,
                             openclaw_security_level_t min_level, openclaw_agent_card_t **agents,
                             size_t *count)
{
    if (!ctx || !agents || !count)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_discover_agents: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    *agents = NULL;
    *count = 0;

    if (!ctx->connected)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    size_t match_count = 0;
    for (size_t i = 0; i < ctx->registered_agent_count; i++) {
        const openclaw_agent_card_t *card = &ctx->registered_agents[i];
        if (card->is_active && card->security_level >= min_level) {
            match_count++;
        }
    }

    if (match_count == 0)
        return 0;

    *agents = (openclaw_agent_card_t *)AGENTOS_CALLOC(match_count, sizeof(openclaw_agent_card_t));
    if (!*agents)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

    size_t idx = 0;
    for (size_t i = 0; i < ctx->registered_agent_count && idx < match_count; i++) {
        const openclaw_agent_card_t *card = &ctx->registered_agents[i];
        if (card->is_active && card->security_level >= min_level) {
            (*agents)[idx] = *card;
            (*agents)[idx].agent_id = card->agent_id ? AGENTOS_STRDUP(card->agent_id) : NULL;
            (*agents)[idx].name = card->name ? AGENTOS_STRDUP(card->name) : NULL;
            (*agents)[idx].description =
                card->description ? AGENTOS_STRDUP(card->description) : NULL;
            (*agents)[idx].version = card->version ? AGENTOS_STRDUP(card->version) : NULL;
            idx++;
        }
    }

    *count = idx;
    return 0;
}

int openclaw_unregister_agent(openclaw_adapter_context_t *ctx, const char *agent_id)
{
    if (!ctx || !agent_id)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_unregister_agent: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    for (size_t i = 0; i < ctx->registered_agent_count; i++) {
        if (strcmp(ctx->registered_agents[i].agent_id, agent_id) == 0) {
            openclaw_agent_card_destroy(&ctx->registered_agents[i]);
            if (i < ctx->registered_agent_count - 1) {
                __builtin_memmove(&ctx->registered_agents[i], &ctx->registered_agents[i + 1],
                        (ctx->registered_agent_count - i - 1) * sizeof(openclaw_agent_card_t));
            }
            ctx->registered_agent_count--;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int openclaw_register_tool(openclaw_adapter_context_t *ctx, const openclaw_tool_info_t *tool)
{
    if (!ctx || !tool || !tool->tool_id)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_register_tool: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!ctx->connected) {
        snprintf(ctx->last_error, sizeof(ctx->last_error), "Not connected");
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
    }

    for (size_t i = 0; i < ctx->registered_tool_count; i++) {
        if (strcmp(ctx->registered_tools[i].tool_id, tool->tool_id) == 0) {
            openclaw_tool_info_destroy(&ctx->registered_tools[i]);
            ctx->registered_tools[i] = *tool;
            ctx->registered_tools[i].tool_id = tool->tool_id ? AGENTOS_STRDUP(tool->tool_id) : NULL;
            ctx->registered_tools[i].name = tool->name ? AGENTOS_STRDUP(tool->name) : NULL;
            ctx->registered_tools[i].description =
                tool->description ? AGENTOS_STRDUP(tool->description) : NULL;
            ctx->registered_tools[i].input_schema_json =
                tool->input_schema_json ? AGENTOS_STRDUP(tool->input_schema_json) : NULL;
            ctx->registered_tools[i].output_schema_json =
                tool->output_schema_json ? AGENTOS_STRDUP(tool->output_schema_json) : NULL;
            return 0;
        }
    }

    openclaw_tool_info_t *new_tools = (openclaw_tool_info_t *)AGENTOS_REALLOC(
        ctx->registered_tools, (ctx->registered_tool_count + 1) * sizeof(openclaw_tool_info_t));
    if (!new_tools)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

    ctx->registered_tools = new_tools;
    AGENTOS_MEMSET(&ctx->registered_tools[ctx->registered_tool_count], 0, sizeof(openclaw_tool_info_t));

    openclaw_tool_info_t *target = &ctx->registered_tools[ctx->registered_tool_count];
    target->tool_id = tool->tool_id ? AGENTOS_STRDUP(tool->tool_id) : NULL;
    target->name = tool->name ? AGENTOS_STRDUP(tool->name) : NULL;
    target->description = tool->description ? AGENTOS_STRDUP(tool->description) : NULL;
    target->input_schema_json =
        tool->input_schema_json ? AGENTOS_STRDUP(tool->input_schema_json) : NULL;
    target->output_schema_json =
        tool->output_schema_json ? AGENTOS_STRDUP(tool->output_schema_json) : NULL;

    ctx->registered_tool_count++;
    return 0;
}

int openclaw_list_tools(openclaw_adapter_context_t *ctx, const char *agent_id,
                        openclaw_tool_info_t **tools, size_t *count)
{
    if (!ctx || !tools || !count)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_list_tools: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    *tools = NULL;
    *count = 0;

    if (!ctx->connected)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    size_t match_count = 0;
    for (size_t i = 0; i < ctx->registered_tool_count; i++) {
        if (!agent_id || (ctx->registered_tools[i].owner_agent_id &&
                          strcmp(ctx->registered_tools[i].owner_agent_id, agent_id) == 0)) {
            match_count++;
        }
    }

    if (match_count == 0)
        return 0;

    *tools = (openclaw_tool_info_t *)AGENTOS_CALLOC(match_count, sizeof(openclaw_tool_info_t));
    if (!*tools)
        AGENTOS_ERROR(AGENTOS_ERR_NULL_POINTER, "null pointer");

    size_t idx = 0;
    for (size_t i = 0; i < ctx->registered_tool_count && idx < match_count; i++) {
        if (!agent_id || (ctx->registered_tools[i].owner_agent_id &&
                          strcmp(ctx->registered_tools[i].owner_agent_id, agent_id) == 0)) {
            (*tools)[idx] = ctx->registered_tools[i];
            (*tools)[idx].tool_id = ctx->registered_tools[i].tool_id
                                        ? AGENTOS_STRDUP(ctx->registered_tools[i].tool_id)
                                        : NULL;
            (*tools)[idx].name = ctx->registered_tools[i].name
                                     ? AGENTOS_STRDUP(ctx->registered_tools[i].name)
                                     : NULL;
            (*tools)[idx].description = ctx->registered_tools[i].description
                                            ? AGENTOS_STRDUP(ctx->registered_tools[i].description)
                                            : NULL;
            idx++;
        }
    }

    *count = idx;
    return 0;
}

int openclaw_create_session(openclaw_adapter_context_t *ctx,
                            const openclaw_session_t *session_template,
                            openclaw_session_t *out_session)
{
    if (!ctx || !out_session)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_create_session: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!ctx->connected)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    static uint32_t session_counter = 0;
    session_counter++;

    AGENTOS_MEMSET(out_session, 0, sizeof(openclaw_session_t));

    char sid[64];
    snprintf(sid, sizeof(sid), "oc-session-%08x", session_counter);
    out_session->session_id = AGENTOS_STRDUP(sid);

    if (session_template) {
        out_session->agent_id =
            session_template->agent_id ? AGENTOS_STRDUP(session_template->agent_id) : NULL;
        out_session->modality = session_template->modality;
        out_session->security_level = session_template->security_level;
    } else {
        out_session->modality = OPENCLAW_MODALITY_TEXT;
        out_session->security_level = ctx->config.default_security_level;
    }

    out_session->created_at = (uint64_t)(time(NULL));
    out_session->last_activity = out_session->created_at;
    out_session->is_active = true;

    openclaw_session_t *new_sessions = (openclaw_session_t *)AGENTOS_REALLOC(
        ctx->active_sessions, (ctx->active_session_count + 1) * sizeof(openclaw_session_t));
    if (!new_sessions)
        return AGENTOS_ERR_OUT_OF_MEMORY;
    ctx->active_sessions = new_sessions;
    __builtin_memcpy(&ctx->active_sessions[ctx->active_session_count], out_session,
           sizeof(openclaw_session_t));
    ctx->active_sessions[ctx->active_session_count].session_id =
        AGENTOS_STRDUP(out_session->session_id);
    ctx->active_sessions[ctx->active_session_count].agent_id =
        out_session->agent_id ? AGENTOS_STRDUP(out_session->agent_id) : NULL;
    ctx->active_session_count++;

    return 0;
}

int openclaw_close_session(openclaw_adapter_context_t *ctx, const char *session_id)
{
    if (!ctx || !session_id)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_close_session: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    for (size_t i = 0; i < ctx->active_session_count; i++) {
        if (strcmp(ctx->active_sessions[i].session_id, session_id) == 0) {
            ctx->active_sessions[i].is_active = false;
            openclaw_session_destroy(&ctx->active_sessions[i]);
            if (i < ctx->active_session_count - 1) {
                __builtin_memmove(&ctx->active_sessions[i], &ctx->active_sessions[i + 1],
                        (ctx->active_session_count - i - 1) * sizeof(openclaw_session_t));
            }
            ctx->active_session_count--;
            return 0;
        }
    }
    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int openclaw_send_message(openclaw_adapter_context_t *ctx, const openclaw_message_t *msg,
                          openclaw_message_t *response)
{
    if (!ctx || !msg || !response)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_send_message: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!ctx->connected)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    ctx->messages_sent++;

    if (ctx->message_handler) {
        int ret = ctx->message_handler(msg, response, ctx->message_handler_data);
        ctx->messages_received++;
        return ret;
    }

    if (ctx->sock_fd != INVALID_SOCK) {
        char send_buf[OPENCLAW_RECV_BUFFER_SIZE] = {0};
        int ser_len = openclaw_serialize_message(msg, send_buf, sizeof(send_buf));
        if (ser_len > 0) {
            size_t send_len = (size_t)ser_len;
            char framed[OPENCLAW_RECV_BUFFER_SIZE + 8];
            int frame_len = snprintf(framed, sizeof(framed), "%06zx%s", send_len, send_buf);
            int send_ret =
                openclaw_socket_send(ctx->sock_fd, framed, (size_t)(frame_len > 0 ? frame_len : 0),
                                     ctx->config.request_timeout_ms);
            if (send_ret == 0) {
                char recv_buf[OPENCLAW_RECV_BUFFER_SIZE] = {0};
                size_t recv_len = 0;
                int recv_ret = openclaw_socket_recv(ctx->sock_fd, recv_buf, sizeof(recv_buf),
                                                    &recv_len, ctx->config.request_timeout_ms);
                if (recv_ret == 0 && recv_len >= 6) {
                    size_t payload_offset = 6;
                    size_t payload_len = recv_len - 6;
                    AGENTOS_MEMSET(response, 0, sizeof(openclaw_message_t));
                    response->message_id = msg->message_id ? AGENTOS_STRDUP(msg->message_id) : NULL;
                    response->session_id = msg->session_id ? AGENTOS_STRDUP(msg->session_id) : NULL;
                    response->sender_id =
                        msg->receiver_id ? AGENTOS_STRDUP(msg->receiver_id) : NULL;
                    response->receiver_id = msg->sender_id ? AGENTOS_STRDUP(msg->sender_id) : NULL;
                    response->modality = msg->modality;
                    response->timestamp = (uint64_t)(time(NULL));
                    if (payload_len > 0) {
                        response->payload = AGENTOS_MALLOC(payload_len + 1);
                        if (response->payload) {
                            __builtin_memcpy(response->payload, recv_buf + payload_offset, payload_len);
                            ((char *)response->payload)[payload_len] = '\0';
                            response->payload_size = payload_len;
                        }
                    }
                    ctx->messages_received++;
                    return 0;
                }
            }
        }
    }

    AGENTOS_MEMSET(response, 0, sizeof(openclaw_message_t));
    response->message_id = msg->message_id ? AGENTOS_STRDUP(msg->message_id) : NULL;
    response->session_id = msg->session_id ? AGENTOS_STRDUP(msg->session_id) : NULL;
    response->receiver_id = msg->sender_id ? AGENTOS_STRDUP(msg->sender_id) : NULL;
    response->sender_id = msg->receiver_id ? AGENTOS_STRDUP(msg->receiver_id) : NULL;
    response->modality = msg->modality;
    response->timestamp = (uint64_t)(time(NULL));

    ctx->messages_received++;
    return 0;
}

int openclaw_delegate_task(openclaw_adapter_context_t *ctx, const openclaw_task_t *task,
                           const char *target_agent_id, openclaw_task_t *result)
{
    if (!ctx || !task || !result)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_delegate_task: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!ctx->connected)
        AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");

    ctx->tasks_delegated++;

    if (ctx->task_handler) {
        int ret = ctx->task_handler(task, result, ctx->task_handler_data);
        if (ret == 0)
            ctx->tasks_completed++;
        return ret;
    }

    static uint32_t task_counter = 0;
    task_counter++;

    AGENTOS_MEMSET(result, 0, sizeof(openclaw_task_t));
    char tid[64];
    snprintf(tid, sizeof(tid), "oc-task-%08x", task_counter);
    result->task_id = AGENTOS_STRDUP(tid);
    result->session_id = task->session_id ? AGENTOS_STRDUP(task->session_id) : NULL;
    result->description = task->description ? AGENTOS_STRDUP(task->description) : NULL;
    result->input_data_json = task->input_data_json ? AGENTOS_STRDUP(task->input_data_json) : NULL;
    result->assigned_agent_id = target_agent_id ? AGENTOS_STRDUP(target_agent_id) : NULL;
    result->priority = task->priority > 0 ? task->priority : 5;
    result->state = OPENCLAW_AGENT_STATE_EXECUTING;
    result->progress = 0.0;
    result->created_at = (uint64_t)(time(NULL));

    ctx->tasks_completed++;
    result->state = OPENCLAW_AGENT_STATE_IDLE;
    result->progress = 1.0;
    result->completed_at = (uint64_t)(time(NULL));

    openclaw_task_t *new_tasks = (openclaw_task_t *)AGENTOS_REALLOC(
        ctx->tracked_tasks, (ctx->tracked_task_count + 1) * sizeof(openclaw_task_t));
    if (!new_tasks)
        return AGENTOS_ERR_OUT_OF_MEMORY;
    ctx->tracked_tasks = new_tasks;
    ctx->tracked_tasks[ctx->tracked_task_count] = *result;
    ctx->tracked_tasks[ctx->tracked_task_count].task_id =
        result->task_id ? AGENTOS_STRDUP(result->task_id) : NULL;
    ctx->tracked_tasks[ctx->tracked_task_count].session_id =
        result->session_id ? AGENTOS_STRDUP(result->session_id) : NULL;
    ctx->tracked_tasks[ctx->tracked_task_count].description =
        result->description ? AGENTOS_STRDUP(result->description) : NULL;
    ctx->tracked_tasks[ctx->tracked_task_count].assigned_agent_id =
        result->assigned_agent_id ? AGENTOS_STRDUP(result->assigned_agent_id) : NULL;
    ctx->tracked_task_count++;

    return 0;
}

int openclaw_query_task(openclaw_adapter_context_t *ctx, const char *task_id,
                        openclaw_task_t *result)
{
    if (!ctx || !task_id || !result)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_query_task: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    AGENTOS_MEMSET(result, 0, sizeof(openclaw_task_t));

    for (size_t i = 0; i < ctx->tracked_task_count; i++) {
        if (ctx->tracked_tasks[i].task_id && strcmp(ctx->tracked_tasks[i].task_id, task_id) == 0) {
            *result = ctx->tracked_tasks[i];
            result->task_id = ctx->tracked_tasks[i].task_id
                                  ? AGENTOS_STRDUP(ctx->tracked_tasks[i].task_id)
                                  : NULL;
            result->session_id = ctx->tracked_tasks[i].session_id
                                     ? AGENTOS_STRDUP(ctx->tracked_tasks[i].session_id)
                                     : NULL;
            result->description = ctx->tracked_tasks[i].description
                                      ? AGENTOS_STRDUP(ctx->tracked_tasks[i].description)
                                      : NULL;
            result->assigned_agent_id =
                ctx->tracked_tasks[i].assigned_agent_id
                    ? AGENTOS_STRDUP(ctx->tracked_tasks[i].assigned_agent_id)
                    : NULL;
            result->input_data_json = ctx->tracked_tasks[i].input_data_json
                                          ? AGENTOS_STRDUP(ctx->tracked_tasks[i].input_data_json)
                                          : NULL;
            result->result_json = ctx->tracked_tasks[i].result_json
                                      ? AGENTOS_STRDUP(ctx->tracked_tasks[i].result_json)
                                      : NULL;
            result->error_message = ctx->tracked_tasks[i].error_message
                                        ? AGENTOS_STRDUP(ctx->tracked_tasks[i].error_message)
                                        : NULL;
            return 0;
        }
    }

    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int openclaw_cancel_task(openclaw_adapter_context_t *ctx, const char *task_id)
{
    if (!ctx || !task_id)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_cancel_task: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    for (size_t i = 0; i < ctx->tracked_task_count; i++) {
        if (ctx->tracked_tasks[i].task_id && strcmp(ctx->tracked_tasks[i].task_id, task_id) == 0) {
            ctx->tracked_tasks[i].state = OPENCLAW_AGENT_STATE_ERROR;
            if (ctx->event_callback) {
                ctx->event_callback("task_cancelled", task_id, ctx->event_callback_data);
            }
            return 0;
        }
    }

    AGENTOS_ERROR(AGENTOS_ERR_INVALID_PARAM, "invalid parameter");
}

int openclaw_get_cluster_status(openclaw_adapter_context_t *ctx, openclaw_cluster_status_t *status)
{
    if (!ctx || !status)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_get_cluster_status: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    AGENTOS_MEMSET(status, 0, sizeof(openclaw_cluster_status_t));
    status->node_id = "agentos-node-001";
    status->cluster_name = ctx->config.cluster_id ? ctx->config.cluster_id : "default";
    status->total_nodes = 1;
    status->active_nodes = 1;
    status->total_agents = (uint64_t)ctx->registered_agent_count;
    status->active_sessions = (uint64_t)ctx->active_session_count;
    status->messages_processed = ctx->messages_sent + ctx->messages_received;
    status->tasks_completed = ctx->tasks_completed;
    status->uptime_seconds = ctx->connection_uptime_sec;
    status->cpu_usage_pct =
        (double)(ctx->active_session_count * 2.5 + ctx->registered_agent_count * 0.5);
    status->memory_usage_mb =
        (double)(ctx->tracked_task_count * 8.0 + ctx->registered_tool_count * 0.5 + 32.0);
    status->disk_usage_pct =
        (double)(ctx->registered_agent_count * 0.1 + ctx->registered_tool_count * 0.05);

    return 0;
}

int openclaw_set_message_handler(openclaw_adapter_context_t *ctx,
                                 openclaw_message_handler_t handler, void *user_data)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_set_message_handler: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->message_handler = handler;
    ctx->message_handler_data = user_data;
    return 0;
}

int openclaw_set_task_handler(openclaw_adapter_context_t *ctx, openclaw_task_handler_t handler,
                              void *user_data)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_set_task_handler: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->task_handler = handler;
    ctx->task_handler_data = user_data;
    return 0;
}

int openclaw_set_event_callback(openclaw_adapter_context_t *ctx, openclaw_event_callback_t callback,
                                void *user_data)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_set_event_callback: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->event_callback = callback;
    ctx->event_callback_data = user_data;
    return 0;
}

int openclaw_set_status_callback(openclaw_adapter_context_t *ctx,
                                 openclaw_status_callback_t callback, void *user_data)
{
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_set_status_callback: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    ctx->status_callback = callback;
    ctx->status_callback_data = user_data;
    return 0;
}

int openclaw_send_heartbeat(openclaw_adapter_context_t *ctx)
{
    if (!ctx || !ctx->connected)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_send_heartbeat: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }

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

int openclaw_get_statistics(openclaw_adapter_context_t *ctx, char *stats_json, size_t buffer_size)
{
    if (!ctx || !stats_json || buffer_size < 64)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_get_statistics: failed");
        return AGENTOS_ERR_UNKNOWN;
        }

    openclaw_cluster_status_t status;
    openclaw_get_cluster_status(ctx, &status);

    int written = snprintf(
        stats_json, buffer_size,
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
        OPENCLAW_ADAPTER_VERSION, OPENCLAW_PLATFORM_VERSION,
        ctx->config.mode == OPENCLAW_MODE_STANDALONE  ? "standalone"
        : ctx->config.mode == OPENCLAW_MODE_CLUSTERED ? "clustered"
        : ctx->config.mode == OPENCLAW_MODE_HYBRID    ? "hybrid"
                                                      : "embedded",
        ctx->connected ? "true" : "false", ctx->registered_agent_count, ctx->active_session_count,
        (unsigned long long)ctx->messages_sent, (unsigned long long)ctx->messages_received,
        (unsigned long long)ctx->tasks_delegated, (unsigned long long)ctx->tasks_completed,
        (unsigned long long)ctx->connection_uptime_sec, status.node_id ? status.node_id : "",
        status.cluster_name ? status.cluster_name : "", (unsigned long long)status.total_agents,
        (unsigned long long)status.active_sessions);

    openclaw_cluster_status_destroy(&status);

    return (written >= 0 && (size_t)written < buffer_size) ? 0 : -2;
}

static int openclaw_proto_init(void *context)
{
    openclaw_config_t config = openclaw_config_default();
    openclaw_adapter_context_t *ctx = openclaw_adapter_create(&config);
    if (!ctx)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_proto_init: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    *(void **)context = ctx;
    return 0;
}

static int openclaw_proto_destroy(void *context)
{
    if (context) {
        openclaw_adapter_destroy((openclaw_adapter_context_t *)context);
    }
    return 0;
}

static int openclaw_proto_handle_request(void *context, const void *req, void **resp)
{
    if (!context || !req)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_proto_handle_request: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    openclaw_adapter_context_t *ctx = (openclaw_adapter_context_t *)context;

    const char *raw_request = (const char *)req;
    openclaw_message_t msg = {0};
    msg.message_id = "proto-req";
    msg.payload = (void *)raw_request;
    msg.payload_size = raw_request ? strlen(raw_request) : 0;
    msg.modality = OPENCLAW_MODALITY_TEXT;
    msg.timestamp = (uint64_t)(time(NULL));

    openclaw_message_t response = {0};
    int ret = openclaw_send_message(ctx, &msg, &response);

    if (resp) {
        if (ret == 0 && response.payload && response.payload_size > 0) {
            *resp = AGENTOS_MALLOC(response.payload_size + 1);
            if (*resp) {
                __builtin_memcpy(*resp, response.payload, response.payload_size);
                ((char *)*resp)[response.payload_size] = '\0';
            }
        } else if (ret == 0) {
            char stats_buf[2048] = {0};
            openclaw_get_statistics(ctx, stats_buf, sizeof(stats_buf));
            *resp = AGENTOS_STRDUP(stats_buf);
        } else {
            char err_buf[512];
            int err_len = snprintf(
                err_buf, sizeof(err_buf),
                "{\"error\":\"Request processing failed\",\"code\":%d,\"adapter_version\":\"%s\"}",
                ret, OPENCLAW_ADAPTER_VERSION);
            if (err_len > 0 && (size_t)err_len < sizeof(err_buf)) {
                *resp = AGENTOS_STRDUP(err_buf);
            } else {
                *resp = NULL;
            }
        }
    }

    openclaw_message_destroy(&msg);
    openclaw_message_destroy(&response);
    return ret;
}

static int openclaw_proto_get_version(void *context, char *buf, size_t max_size)
{
    if (!buf || max_size == 0)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "openclaw_proto_get_version: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    const char *ver = openclaw_adapter_version();
    size_t len = strlen(ver);
    if (len >= max_size)
        len = max_size - 1;
    __builtin_memcpy(buf, ver, len);
    buf[len] = '\0';
    return 0;
}

static uint32_t openclaw_proto_capabilities(void *context)
{
    return (uint32_t)(PROTO_CAP_MULTIMODAL | PROTO_CAP_STREAMING | PROTO_CAP_TOOL_CALLING |
                      PROTO_CAP_AGENT_DISCOVERY);
}

static proto_adapter_t g_openclaw_adapter = {0};
static pthread_once_t g_openclaw_adapter_once = PTHREAD_ONCE_INIT;

static void openclaw_adapter_init_once(void)
{
    g_openclaw_adapter.name = "OpenClaw";
    g_openclaw_adapter.version = OPENCLAW_ADAPTER_VERSION;
    g_openclaw_adapter.description = "OpenClaw Platform Integration Adapter - offline private AI "
                                     "Agent platform with multimodal capabilities";
    g_openclaw_adapter.type = PROTO_OPENCLAW;
    g_openclaw_adapter.init = openclaw_proto_init;
    g_openclaw_adapter.destroy = openclaw_proto_destroy;
    g_openclaw_adapter.handle_request = openclaw_proto_handle_request;
    g_openclaw_adapter.get_version = openclaw_proto_get_version;
    g_openclaw_adapter.capabilities = openclaw_proto_capabilities;
}

const proto_adapter_t *openclaw_get_protocol_adapter(void)
{
    pthread_once(&g_openclaw_adapter_once, openclaw_adapter_init_once);
    return &g_openclaw_adapter;
}

void openclaw_agent_card_destroy(openclaw_agent_card_t *card)
{
    if (!card)
        return;
    AGENTOS_FREE(card->agent_id);
    AGENTOS_FREE(card->name);
    AGENTOS_FREE(card->description);
    AGENTOS_FREE(card->version);
    AGENTOS_MEMSET(card, 0, sizeof(openclaw_agent_card_t));
}

void openclaw_tool_info_destroy(openclaw_tool_info_t *tool)
{
    if (!tool)
        return;
    AGENTOS_FREE(tool->tool_id);
    AGENTOS_FREE(tool->name);
    AGENTOS_FREE(tool->description);
    AGENTOS_FREE(tool->input_schema_json);
    AGENTOS_FREE(tool->output_schema_json);
    AGENTOS_MEMSET(tool, 0, sizeof(openclaw_tool_info_t));
}

void openclaw_session_destroy(openclaw_session_t *session)
{
    if (!session)
        return;
    AGENTOS_FREE(session->session_id);
    AGENTOS_FREE(session->agent_id);
    AGENTOS_FREE(session->parent_session_id);
    AGENTOS_MEMSET(session, 0, sizeof(openclaw_session_t));
}

void openclaw_message_destroy(openclaw_message_t *msg)
{
    if (!msg)
        return;
    AGENTOS_FREE(msg->message_id);
    AGENTOS_FREE(msg->session_id);
    AGENTOS_FREE(msg->sender_id);
    AGENTOS_FREE(msg->receiver_id);
    AGENTOS_FREE(msg->content_type);
    AGENTOS_FREE(msg->payload);
    AGENTOS_MEMSET(msg, 0, sizeof(openclaw_message_t));
}

void openclaw_task_destroy(openclaw_task_t *task)
{
    if (!task)
        return;
    AGENTOS_FREE(task->task_id);
    AGENTOS_FREE(task->session_id);
    AGENTOS_FREE(task->description);
    AGENTOS_FREE(task->input_data_json);
    AGENTOS_FREE(task->assigned_agent_id);
    AGENTOS_FREE(task->result_json);
    AGENTOS_FREE(task->error_message);
    AGENTOS_MEMSET(task, 0, sizeof(openclaw_task_t));
}

void openclaw_cluster_status_destroy(openclaw_cluster_status_t *status)
{
    if (!status)
        return;
    AGENTOS_FREE(status->node_id);
    AGENTOS_FREE(status->cluster_name);
    AGENTOS_MEMSET(status, 0, sizeof(openclaw_cluster_status_t));
}
