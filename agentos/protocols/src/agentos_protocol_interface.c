// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file agentos_protocol_interface.c
 * @brief AgentOS Protocol System Unified Interface Implementation
 *
 * 原位置: agentos/interfaces/src/
 * 迁移至: agentos/protocols/src/ (2026-04-19 interfaces删除重构)
 */

#include "agentos_protocol_interface.h"

#include "../core/router/include/protocol_router.h"
#include "error.h"
#include "logging.h"
#include "logging_compat.h"
#include "memory_compat.h"
#include "protocol_registry.h"
#include "types.h"

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static proto_adapter_entry_t *g_adapter_registry = NULL;
static size_t g_adapter_count = 0;

/* ============================================================================
 * I-L2: Standard Router Implementation
 * ============================================================================ */

typedef struct {
    protocol_router_handle_t handle;
    protocol_type_t default_protocol;
} proto_router_impl_t;

typedef struct {
    proto_router_impl_t impl;
    proto_router_iface_t iface;
} proto_router_full_t;

static proto_router_impl_t *router_get_impl(proto_router_iface_t *iface)
{
    if (!iface)
        return NULL;
    proto_router_full_t *full =
        (proto_router_full_t *)((char *)iface - offsetof(proto_router_full_t, iface));
    return &full->impl;
}

static int router_std_add_route(proto_router_iface_t *router, const char *source_pattern,
                                protocol_type_t source_proto, const char *target_endpoint,
                                protocol_type_t target_proto, int priority)
{
    proto_router_impl_t *impl = router_get_impl(router);
    if (!impl || !impl->handle)
        return AGENTOS_EINVAL;

    protocol_rule_t rule;
    AGENTOS_MEMSET(&rule, 0, sizeof(rule));
    rule.source_protocol = source_proto;
    rule.target_protocol = target_proto;
    rule.source_endpoint = source_pattern;
    rule.target_endpoint = target_endpoint;
    rule.priority = (uint32_t)priority;
    rule.transformer_context = NULL;

    return protocol_router_add_rule(impl->handle, &rule, NULL);
}

static int router_std_remove_route(proto_router_iface_t *router, const char *source_pattern)
{
    AGENTOS_CHECK(router != NULL, AGENTOS_EINVAL, "router is NULL");
    AGENTOS_CHECK(source_pattern != NULL, AGENTOS_EINVAL, "source_pattern is NULL");
    proto_router_impl_t *impl = router_get_impl(router);
    AGENTOS_CHECK(impl != NULL, AGENTOS_EINVAL, "impl is NULL");
    AGENTOS_CHECK(impl->handle != NULL, AGENTOS_EINVAL, "impl->handle is NULL");
    return AGENTOS_ERR_INVALID_PARAM;
}

static int router_std_route(proto_router_iface_t *router, const unified_message_t *message,
                            route_decision_t *decision)
{
    proto_router_impl_t *impl = router_get_impl(router);
    AGENTOS_CHECK(impl != NULL, AGENTOS_EINVAL, "impl is NULL");
    AGENTOS_CHECK(impl->handle != NULL, AGENTOS_EINVAL, "impl->handle is NULL");
    AGENTOS_CHECK(message != NULL, AGENTOS_EINVAL, "message is NULL");
    AGENTOS_CHECK(decision != NULL, AGENTOS_EINVAL, "decision is NULL");

    unified_message_t transformed;
    AGENTOS_MEMSET(&transformed, 0, sizeof(transformed));

    int result = protocol_router_route(impl->handle, message, &transformed);
    if (result == 0) {
        decision->adapter_name = NULL;
        decision->target_protocol = transformed.protocol_type;
        decision->confidence = 100;
        decision->needs_transformation = (message->protocol_type != transformed.protocol_type);
        decision->transformer_name = NULL;
    }
    return result;
}

static int router_std_transform(proto_router_iface_t *router, const unified_message_t *source,
                                unified_message_t *target, const char *transformer_name)
{
    proto_router_impl_t *impl = router_get_impl(router);
    if (!impl || !impl->handle || !source || !target)
        return AGENTOS_EINVAL;
    (void)transformer_name;

    return protocol_router_route(impl->handle, source, target);
}

static int router_std_batch_route(proto_router_iface_t *router, const unified_message_t *messages,
                                  size_t count, route_decision_t *decisions)
{
    proto_router_impl_t *impl = router_get_impl(router);
    if (!impl || !impl->handle || !messages || !decisions || count == 0)
        return AGENTOS_EINVAL;

    unified_message_t *transformed =
        (unified_message_t *)AGENTOS_CALLOC(count, sizeof(unified_message_t));
    if (!transformed)
        return AGENTOS_ENOMEM;

    int result = protocol_router_route_batch(impl->handle, messages, count, transformed);
    if (result >= 0) {
        for (int i = 0; i < result; i++) {
            decisions[i].adapter_name = NULL;
            decisions[i].target_protocol = transformed[i].protocol_type;
            decisions[i].confidence = 100;
            decisions[i].needs_transformation =
                (messages[i].protocol_type != transformed[i].protocol_type);
            decisions[i].transformer_name = NULL;
        }
    }
    AGENTOS_FREE(transformed);
    return result;
}

static int router_std_set_default_protocol(proto_router_iface_t *router, protocol_type_t proto)
{
    proto_router_impl_t *impl = router_get_impl(router);
    if (!impl)
        return AGENTOS_EINVAL;
    impl->default_protocol = proto;
    return 0;
}

static int router_std_list_routes(proto_router_iface_t *router, char **routes_json)
{
    proto_router_impl_t *impl = router_get_impl(router);
    if (!impl || !impl->handle || !routes_json)
        return AGENTOS_EINVAL;

    return protocol_router_get_stats(impl->handle, routes_json);
}

static int router_std_get_stats(proto_router_iface_t *router, char **stats_json)
{
    proto_router_impl_t *impl = router_get_impl(router);
    if (!impl || !impl->handle || !stats_json)
        return AGENTOS_EINVAL;

    return protocol_router_get_stats(impl->handle, stats_json);
}

proto_router_iface_t *proto_router_standard_create(void)
{
    proto_router_full_t *full =
        (proto_router_full_t *)AGENTOS_CALLOC(1, sizeof(proto_router_full_t));
    if (!full)
        return NULL;

    full->impl.handle = protocol_router_create(PROTOCOL_HTTP);
    if (!full->impl.handle) {
        AGENTOS_FREE(full);
        return NULL;
    }
    full->impl.default_protocol = PROTOCOL_HTTP;

    full->iface.add_route = router_std_add_route;
    full->iface.remove_route = router_std_remove_route;
    full->iface.route = router_std_route;
    full->iface.transform = router_std_transform;
    full->iface.batch_route = router_std_batch_route;
    full->iface.set_default_protocol = router_std_set_default_protocol;
    full->iface.list_routes = router_std_list_routes;
    full->iface.get_stats = router_std_get_stats;

    return &full->iface;
}

void proto_router_standard_destroy(proto_router_iface_t *router)
{
    if (!router)
        return;
    proto_router_impl_t *impl = router_get_impl(router);
    if (impl && impl->handle) {
        protocol_router_destroy(impl->handle);
    }
    proto_router_full_t *full =
        (proto_router_full_t *)((char *)router - offsetof(proto_router_full_t, iface));
    AGENTOS_FREE(full);
}

/* ============================================================================
 * I-L3: Standard Gateway Implementation
 * ============================================================================ */

typedef int (*gw_internal_request_cb)(const char *raw_request, size_t request_size,
                                      const char *content_type, char **response,
                                      size_t *response_size, char **response_content_type,
                                      void *user_data);

typedef struct {
    proto_gateway_request_cb public_cb;
    void *public_user_data;
} gw_request_adapter_ctx_t;

#define GW_MAX_PROTOCOLS 32

typedef struct {
    char name[64];
    const proto_adapter_vtable_t *vtable;
    gw_internal_request_cb raw_handler;
    void *raw_handler_data;
    gw_request_adapter_ctx_t *adapter_ctx;
    proto_gateway_event_cb event_callback;
    void *event_callback_data;
    uint64_t request_count;
    uint64_t error_count;
    uint64_t total_bytes;
} gw_protocol_entry_t;

typedef struct {
    gw_protocol_entry_t protocols[GW_MAX_PROTOCOLS];
    size_t protocol_count;
} proto_gateway_impl_t;

static proto_gateway_impl_t *g_gw_impl = NULL;

static void agentos_proto_gw_log(const proto_gateway_iface_t *gw, const char *operation)
{
    if (gw) {
        AGENTOS_LOG_DEBUG("[proto_gw:%p] %s", (const void *)gw, operation ? operation : "unknown");
    }
}

static int gw_request_adapter_trampoline(const char *raw_request, size_t request_size,
                                         const char *content_type, char **response,
                                         size_t *response_size, char **response_content_type,
                                         void *user_data)
{
    gw_request_adapter_ctx_t *ctx = (gw_request_adapter_ctx_t *)user_data;
    if (!ctx || !ctx->public_cb)
        return AGENTOS_EINVAL;

    char *response_json = NULL;
    char request_buf[4096];
    size_t copy_len =
        request_size < sizeof(request_buf) - 1 ? request_size : sizeof(request_buf) - 1;
    memcpy(request_buf, raw_request, copy_len);
    request_buf[copy_len] = '\0';

    int result =
        ctx->public_cb("unknown", "handle", request_buf, &response_json, ctx->public_user_data);
    if (result == 0 && response_json) {
        if (response)
            *response = response_json;
        if (response_size)
            *response_size = strlen(response_json);
        if (response_content_type)
            *response_content_type = AGENTOS_STRDUP("application/json");
    } else {
        AGENTOS_FREE(response_json);
    }
    (void)content_type;
    return result;
}

static gw_protocol_entry_t *gw_find_protocol(const char *name)
{
    if (!g_gw_impl || !name)
        return NULL;
    for (size_t i = 0; i < g_gw_impl->protocol_count; i++) {
        if (strcmp(g_gw_impl->protocols[i].name, name) == 0)
            return &g_gw_impl->protocols[i];
    }
    return NULL;
}

static int gw_std_register_protocol(proto_gateway_iface_t *gw, const char *name,
                                    const proto_adapter_vtable_t *adapter)
{
    if (!name || !adapter)
        return AGENTOS_EINVAL;
    if (!g_gw_impl)
        return AGENTOS_ERR_NOT_FOUND;
    if (g_gw_impl->protocol_count >= GW_MAX_PROTOCOLS)
        return AGENTOS_ERR_NULL_POINTER;

    gw_protocol_entry_t *existing = gw_find_protocol(name);
    if (existing) {
        existing->vtable = adapter;
        return 0;
    }

    gw_protocol_entry_t *entry = &g_gw_impl->protocols[g_gw_impl->protocol_count++];
    AGENTOS_MEMSET(entry, 0, sizeof(*entry));
    AGENTOS_STRNCPY_TERM(entry->name, name, sizeof(entry->name));
    entry->vtable = adapter;
    return 0;
}

static int gw_std_unregister_protocol(proto_gateway_iface_t *gw, const char *name)
{
    agentos_proto_gw_log(gw, "unregister_protocol");
    if (!name)
        return AGENTOS_EINVAL;
    if (!g_gw_impl)
        return AGENTOS_ERR_NOT_FOUND;

    for (size_t i = 0; i < g_gw_impl->protocol_count; i++) {
        if (strcmp(g_gw_impl->protocols[i].name, name) == 0) {
            AGENTOS_FREE(g_gw_impl->protocols[i].adapter_ctx);
            g_gw_impl->protocols[i].adapter_ctx = NULL;
            memmove(&g_gw_impl->protocols[i], &g_gw_impl->protocols[i + 1],
                    (g_gw_impl->protocol_count - i - 1) * sizeof(gw_protocol_entry_t));
            g_gw_impl->protocol_count--;
            AGENTOS_MEMSET(&g_gw_impl->protocols[g_gw_impl->protocol_count], 0,
                   sizeof(gw_protocol_entry_t));
            return 0;
        }
    }
    return AGENTOS_ERR_NOT_FOUND;
}

static int gw_std_detect_protocol(proto_gateway_iface_t *gw, const char *data, size_t len,
                                  char **detected)
{
    agentos_proto_gw_log(gw, "detect_protocol");
    if (!data || !len || !detected)
        return AGENTOS_EINVAL;

    const char *result = NULL;
    if (len >= 4 && memcmp(data, "\x4F\x4A\x57\x4D", 4) == 0) {
        result = "openjiuwen";
    } else if (len > 0 && data[0] == '{') {
        bool has_jsonrpc = (strstr(data, "\"jsonrpc\"") != NULL);
        bool has_mcp = (strstr(data, "\"method\"") != NULL && strstr(data, "\"jsonrpc\"") == NULL);
        bool has_claude =
            (strstr(data, "\"model\"") != NULL && strstr(data, "\"anthropic\"") != NULL);
        bool has_openai =
            (strstr(data, "\"model\"") != NULL && strstr(data, "\"messages\"") != NULL);
        bool has_a2a = (strstr(data, "\"agentUrl\"") != NULL || strstr(data, "\"task\"") != NULL);

        if (has_jsonrpc)
            result = "jsonrpc";
        else if (has_mcp)
            result = "mcp";
        else if (has_a2a)
            result = "a2a";
        else if (has_claude)
            result = "claude";
        else if (has_openai)
            result = "openai";
        else
            result = "jsonrpc";
    } else {
        result = "unknown";
    }

    *detected = AGENTOS_STRDUP(result);
    return (*detected) ? 0 : AGENTOS_ENOMEM;
}

static int gw_std_handle_request(proto_gateway_iface_t *gw, const char *raw_request,
                                 size_t request_size, const char *content_type, char **response,
                                 size_t *response_size, char **response_content_type)
{
    if (!raw_request)
        return AGENTOS_EINVAL;

    char *detected_proto = NULL;
    int detect_result = gw_std_detect_protocol(gw, raw_request, request_size, &detected_proto);
    if (detect_result != 0 || !detected_proto) {
        if (response)
            *response = AGENTOS_STRDUP("{\"error\":\"Protocol detection failed\"}");
        if (response_size)
            *response_size = response ? strlen(*response) : 0;
        if (response_content_type)
            *response_content_type = AGENTOS_STRDUP("application/json");
        AGENTOS_FREE(detected_proto);
        return AGENTOS_ERR_IO;
    }

    gw_protocol_entry_t *entry = gw_find_protocol(detected_proto);
    AGENTOS_FREE(detected_proto);

    if (entry && entry->raw_handler) {
        int result =
            entry->raw_handler(raw_request, request_size, content_type, response, response_size,
                               response_content_type, entry->raw_handler_data);
        entry->request_count++;
        entry->total_bytes += request_size;
        if (result != 0)
            entry->error_count++;
        return result;
    }

    if (response)
        *response = AGENTOS_STRDUP("{\"error\":\"No handler for protocol\"}");
    if (response_size)
        *response_size = response ? strlen(*response) : 0;
    if (response_content_type)
        *response_content_type = AGENTOS_STRDUP("application/json");
    return AGENTOS_ERR_NOT_FOUND;
}

static int gw_std_set_request_handler(proto_gateway_iface_t *gw, proto_gateway_request_cb handler,
                                      void *user_data)
{
    if (!handler)
        return AGENTOS_EINVAL;
    if (!g_gw_impl)
        return AGENTOS_ERR_NOT_FOUND;

    for (size_t i = 0; i < g_gw_impl->protocol_count; i++) {
        gw_protocol_entry_t *entry = &g_gw_impl->protocols[i];

        if (!entry->adapter_ctx) {
            entry->adapter_ctx =
                (gw_request_adapter_ctx_t *)AGENTOS_CALLOC(1, sizeof(gw_request_adapter_ctx_t));
            if (!entry->adapter_ctx)
                return AGENTOS_ERR_OUT_OF_MEMORY;
        }

        entry->adapter_ctx->public_cb = handler;
        entry->adapter_ctx->public_user_data = user_data;
        entry->raw_handler = gw_request_adapter_trampoline;
        entry->raw_handler_data = entry->adapter_ctx;
    }
    return 0;
}

static int gw_std_set_event_callback(proto_gateway_iface_t *gw, proto_gateway_event_cb callback,
                                     void *user_data)
{
    agentos_proto_gw_log(gw, "set_event_callback");
    if (!callback)
        return AGENTOS_EINVAL;
    if (!g_gw_impl)
        return AGENTOS_ERR_NOT_FOUND;

    for (size_t i = 0; i < g_gw_impl->protocol_count; i++) {
        g_gw_impl->protocols[i].event_callback = callback;
        g_gw_impl->protocols[i].event_callback_data = user_data;
    }
    return 0;
}

static int gw_std_list_protocols(proto_gateway_iface_t *gw, char **protocols_json)
{
    if (!protocols_json)
        return AGENTOS_EINVAL;

    size_t buf_size = 256;
    if (g_gw_impl && g_gw_impl->protocol_count > 0) {
        buf_size += g_gw_impl->protocol_count * 64;
    } else {
        buf_size += 256;
    }

    char *buf = (char *)AGENTOS_MALLOC(buf_size);
    if (!buf)
        return AGENTOS_ENOMEM;

    size_t offset = snprintf(buf, buf_size, "{\"protocols\":[");
    if (g_gw_impl && g_gw_impl->protocol_count > 0) {
        for (size_t i = 0; i < g_gw_impl->protocol_count; i++) {
            if (i > 0)
                offset += snprintf(buf + offset, buf_size - offset, ",");
            offset +=
                snprintf(buf + offset, buf_size - offset, "\"%s\"", g_gw_impl->protocols[i].name);
        }
    } else {
        offset +=
            snprintf(buf + offset, buf_size - offset, "\"jsonrpc\",\"mcp\",\"a2a\",\"openai\"");
    }
    offset += snprintf(buf + offset, buf_size - offset, "],\"count\":%zu}",
                       g_gw_impl ? g_gw_impl->protocol_count : 0);

    *protocols_json = buf;
    return 0;
}

static int gw_std_get_protocol_stats(proto_gateway_iface_t *gw, const char *name,
                                     proto_stats_t *stats)
{
    agentos_proto_gw_log(gw, "get_protocol_stats");
    if (!stats)
        return AGENTOS_EINVAL;
    AGENTOS_MEMSET(stats, 0, sizeof(*stats));

    if (!g_gw_impl)
        return AGENTOS_ERR_NOT_FOUND;

    if (name) {
        gw_protocol_entry_t *entry = gw_find_protocol(name);
        if (!entry)
            return AGENTOS_ERR_NULL_POINTER;
        stats->messages_sent = entry->request_count;
        stats->errors_total = entry->error_count;
        stats->bytes_sent = entry->total_bytes / 2;
        stats->bytes_received = entry->total_bytes - stats->bytes_sent;
    } else {
        for (size_t i = 0; i < g_gw_impl->protocol_count; i++) {
            stats->messages_sent += g_gw_impl->protocols[i].request_count;
            stats->errors_total += g_gw_impl->protocols[i].error_count;
            uint64_t half_bytes = g_gw_impl->protocols[i].total_bytes / 2;
            stats->bytes_sent += half_bytes;
            stats->bytes_received += g_gw_impl->protocols[i].total_bytes - half_bytes;
        }
    }
    return 0;
}

proto_gateway_iface_t *proto_gateway_standard_create(void)
{
    g_gw_impl = (proto_gateway_impl_t *)AGENTOS_CALLOC(1, sizeof(proto_gateway_impl_t));
    if (!g_gw_impl)
        return NULL;

    proto_gateway_iface_t *iface =
        (proto_gateway_iface_t *)AGENTOS_CALLOC(1, sizeof(proto_gateway_iface_t));
    if (!iface) {
        AGENTOS_FREE(g_gw_impl);
        g_gw_impl = NULL;
        return NULL;
    }

    iface->register_protocol = gw_std_register_protocol;
    iface->unregister_protocol = gw_std_unregister_protocol;
    iface->handle_request = gw_std_handle_request;
    iface->detect_protocol = gw_std_detect_protocol;
    iface->set_request_handler = gw_std_set_request_handler;
    iface->set_event_callback = gw_std_set_event_callback;
    iface->list_protocols = gw_std_list_protocols;
    iface->get_protocol_stats = gw_std_get_protocol_stats;

    return iface;
}

void proto_gateway_standard_destroy(proto_gateway_iface_t *gw)
{
    if (!gw)
        return;
    agentos_proto_gw_log(gw, "destroy");
    if (g_gw_impl) {
        for (size_t i = 0; i < g_gw_impl->protocol_count; i++) {
            AGENTOS_FREE(g_gw_impl->protocols[i].adapter_ctx);
            g_gw_impl->protocols[i].adapter_ctx = NULL;
        }
        AGENTOS_FREE(g_gw_impl);
        g_gw_impl = NULL;
    }
    AGENTOS_FREE(gw);
}

/* Global Registration & Discovery API Implementation */
int proto_interface_register_builtins(void)
{
    static bool registered = false;
    if (registered)
        return 0;

    protocol_registry_t *registry = proto_registry_create();
    if (!registry)
        return AGENTOS_EINVAL;

    int count = proto_registry_initialize_builtins(registry);
    if (count > 0) {
        proto_registry_entry_t *entries = NULL;
        size_t total = proto_registry_list_active(registry, &entries);

        for (size_t i = 0; i < total; i++) {
            proto_adapter_entry_t *entry =
                (proto_adapter_entry_t *)AGENTOS_CALLOC(1, sizeof(proto_adapter_entry_t));
            if (!entry)
                continue;

            entry->name = AGENTOS_STRDUP(entries[i].name);
            entry->version = AGENTOS_STRDUP(entries[i].version);
            entry->description = AGENTOS_STRDUP(entries[i].description);
            entry->type = entries[i].type;
            entry->capabilities = entries[i].capabilities;
            entry->is_builtin = true;
            entry->vtable = NULL;
            entry->next = g_adapter_registry;
            g_adapter_registry = entry;
            g_adapter_count++;
        }
        if (entries)
            AGENTOS_FREE(entries);
    }

    registered = true;
    return count;
}

const proto_adapter_entry_t *proto_interface_find(const char *name)
{
    if (!name)
        return NULL;
    proto_adapter_entry_t *entry = g_adapter_registry;
    while (entry) {
        if (strcmp(entry->name, name) == 0)
            return entry;
        entry = entry->next;
    }
    return NULL;
}

int proto_interface_list_all(char **json_output)
{
    if (!json_output)
        return AGENTOS_EINVAL;

    size_t buf_size = 512 + g_adapter_count * 256;
    char *buf = AGENTOS_MALLOC(buf_size);
    if (!buf)
        return AGENTOS_ERR_OUT_OF_MEMORY;

    size_t offset = snprintf(buf, buf_size, "{\"adapters\":[");
    proto_adapter_entry_t *entry = g_adapter_registry;
    while (entry) {
        if (entry != g_adapter_registry)
            offset += snprintf(buf + offset, buf_size - offset, ",");
        offset += snprintf(buf + offset, buf_size - offset, "\"%s\"",
                           entry->name ? entry->name : "unknown");
        entry = entry->next;
    }
    offset += snprintf(buf + offset, buf_size - offset, "],\"count\":%zu}", g_adapter_count);

    *json_output = buf;
    return 0;
}

const char *proto_interface_type_name(protocol_type_t type)
{
    switch (type) {
    case PROTOCOL_HTTP:
        return "HTTP";
    case PROTOCOL_CUSTOM:
        return "Custom";
    default:
        if (type == PROTOCOL_WEBSOCKET)
            return "WebSocket";
        if (type == PROTOCOL_STDIO)
            return "Stdio";
        if (type == PROTOCOL_IPC)
            return "IPC";
        return "Unknown";
    }
}

protocol_type_t proto_interface_parse_type(const char *name)
{
    if (!name)
        return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "http") == 0 || strcasecmp(name, "jsonrpc") == 0)
        return PROTOCOL_HTTP;
    if (strcasecmp(name, "websocket") == 0 || strcasecmp(name, "ws") == 0)
        return PROTOCOL_WEBSOCKET;
    if (strcasecmp(name, "stdio") == 0)
        return PROTOCOL_STDIO;
    if (strcasecmp(name, "ipc") == 0)
        return PROTOCOL_IPC;
    if (strcasecmp(name, "mcp") == 0 || strcasecmp(name, "mcp_v1") == 0)
        return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "a2a") == 0 || strcasecmp(name, "a2a_v03") == 0)
        return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "openai") == 0)
        return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "openjiuwen") == 0)
        return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "openclaw") == 0)
        return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "claude") == 0)
        return PROTOCOL_CUSTOM;
    return PROTOCOL_CUSTOM;
}
