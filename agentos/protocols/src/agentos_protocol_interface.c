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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static proto_adapter_entry_t* g_adapter_registry = NULL;
static size_t g_adapter_count = 0;

/* I-L2: Standard Router Implementation */
struct proto_router_iface_s {
    void* internal_router;
};

proto_router_iface_t* proto_router_standard_create(void) {
    proto_router_iface_t* iface = calloc(1, sizeof(proto_router_iface_t));
    if (!iface) return NULL;

    iface->internal_router = protocol_router_create(PROTOCOL_HTTP);
    if (!iface->internal_router) {
        free(iface);
        return NULL;
    }
    return iface;
}

void proto_router_standard_destroy(proto_router_iface_t* router) {
    if (!router) return;
    if (router->internal_router) {
        protocol_router_destroy(router->internal_router);
    }
    free(router);
}

/* I-L3: Standard Gateway Implementation */
typedef struct {
    char name[64];
    const proto_adapter_vtable_t* vtable;
    proto_gateway_request_cb request_handler;
    void* request_handler_data;
    proto_gateway_event_cb event_callback;
    void* event_callback_data;
    uint64_t request_count;
    uint64_t error_count;
    uint64_t total_bytes;
} gw_protocol_entry_t;

#define GW_MAX_PROTOCOLS 32

struct proto_gateway_iface_s {
    gw_protocol_entry_t protocols[GW_MAX_PROTOCOLS];
    size_t protocol_count;
};

static gw_protocol_entry_t* gw_find_protocol(proto_gateway_iface_t* gw, const char* name) {
    if (!gw || !name) return NULL;
    for (size_t i = 0; i < gw->protocol_count; i++) {
        if (strcmp(gw->protocols[i].name, name) == 0) return &gw->protocols[i];
    }
    return NULL;
}

static int gw_std_register_protocol(proto_gateway_iface_t* gw,
                                     const char* name,
                                     const proto_adapter_vtable_t* adapter) {
    if (!gw || !name || !adapter) return -1;
    if (gw->protocol_count >= GW_MAX_PROTOCOLS) return -2;

    gw_protocol_entry_t* existing = gw_find_protocol(gw, name);
    if (existing) {
        existing->vtable = adapter;
        return 0;
    }

    gw_protocol_entry_t* entry = &gw->protocols[gw->protocol_count++];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->vtable = adapter;
    return 0;
}

static int gw_std_unregister_protocol(proto_gateway_iface_t* gw, const char* name) {
    if (!gw || !name) return -1;

    for (size_t i = 0; i < gw->protocol_count; i++) {
        if (strcmp(gw->protocols[i].name, name) == 0) {
            memmove(&gw->protocols[i], &gw->protocols[i + 1],
                    (gw->protocol_count - i - 1) * sizeof(gw_protocol_entry_t));
            gw->protocol_count--;
            memset(&gw->protocols[gw->protocol_count], 0, sizeof(gw_protocol_entry_t));
            return 0;
        }
    }
    return -2;
}

static int gw_std_handle_request(proto_gateway_iface_t* gw,
                                  const char* raw_request,
                                  size_t request_size,
                                  const char* content_type,
                                  char** response,
                                  size_t* response_size,
                                  char** response_content_type) {
    if (!gw || !raw_request) return -1;

    char* detected_proto = NULL;
    int detect_result = gw_std_detect_protocol(gw, raw_request, request_size, &detected_proto);
    if (detect_result != 0 || !detected_proto) {
        if (response) *response = strdup("{\"error\":\"Protocol detection failed\"}");
        if (response_size) *response_size = response ? strlen(*response) : 0;
        if (response_content_type) *response_content_type = strdup("application/json");
        free(detected_proto);
        return -2;
    }

    gw_protocol_entry_t* entry = gw_find_protocol(gw, detected_proto);
    free(detected_proto);

    if (entry && entry->request_handler) {
        int result = entry->request_handler(
            raw_request, request_size, content_type,
            response, response_size, response_content_type,
            entry->request_handler_data);
        entry->request_count++;
        entry->total_bytes += request_size;
        if (result != 0) entry->error_count++;
        return result;
    }

    if (entry && entry->vtable && entry->vtable->handle_request) {
        int result = entry->vtable->handle_request(
            raw_request, request_size, content_type,
            response, response_size, response_content_type);
        entry->request_count++;
        entry->total_bytes += request_size;
        return result;
    }

    if (response) *response = strdup("{\"error\":\"No handler for protocol\"}");
    if (response_size) *response_size = response ? strlen(*response) : 0;
    if (response_content_type) *response_content_type = strdup("application/json");
    return -3;
}

static int gw_std_detect_protocol(proto_gateway_iface_t* gw, const char* data, size_t len, char** detected) {
    (void)gw;
    if (!data || !len || !detected) return -1;

    const char* result = NULL;
    if (len >= 4 && memcmp(data, "\x4F\x4A\x57\x4D", 4) == 0) {
        result = "openjiuwen";
    } else if (len > 0 && data[0] == '{') {
        bool has_jsonrpc = (strstr(data, "\"jsonrpc\"") != NULL);
        bool has_mcp = (strstr(data, "\"method\"") != NULL && strstr(data, "\"jsonrpc\"") == NULL);
        bool has_claude = (strstr(data, "\"model\"") != NULL && strstr(data, "\"anthropic\"") != NULL);
        bool has_openai = (strstr(data, "\"model\"") != NULL && strstr(data, "\"messages\"") != NULL);
        bool has_a2a = (strstr(data, "\"agentUrl\"") != NULL || strstr(data, "\"task\"") != NULL);

        if (has_jsonrpc)      result = "jsonrpc";
        else if (has_mcp)     result = "mcp";
        else if (has_a2a)     result = "a2a";
        else if (has_claude)  result = "claude";
        else if (has_openai)  result = "openai";
        else                  result = "jsonrpc";
    } else {
        result = "unknown";
    }

    *detected = strdup(result);
    return (*detected) ? 0 : -1;
}

static int gw_std_set_request_handler(proto_gateway_iface_t* gw,
                                      proto_gateway_request_cb handler,
                                      void* user_data) {
    if (!gw || !handler) return -1;

    for (size_t i = 0; i < gw->protocol_count; i++) {
        gw->protocols[i].request_handler = handler;
        gw->protocols[i].request_handler_data = user_data;
    }
    return 0;
}

static int gw_std_set_event_callback(proto_gateway_iface_t* gw,
                                     proto_gateway_event_cb callback,
                                     void* user_data) {
    if (!gw || !callback) return -1;

    for (size_t i = 0; i < gw->protocol_count; i++) {
        gw->protocols[i].event_callback = callback;
        gw->protocols[i].event_callback_data = user_data;
    }
    return 0;
}

static int gw_std_list_protocols(proto_gateway_iface_t* gw, char** protocols_json) {
    (void)gw;
    if (protocols_json)
        *protocols_json = strdup(
            "{\"protocols\":["
            "\"jsonrpc\",\"mcp\",\"a2a\",\"openai\","
            "\"openjiuwen\",\"openclaw\",\"claude\""
            "]}");
    return 0;
}

static int gw_std_get_protocol_stats(proto_gateway_iface_t* gw,
                                     const char* name,
                                     proto_stats_t* stats) {
    if (!gw || !stats) return -1;
    memset(stats, 0, sizeof(*stats));

    if (name) {
        gw_protocol_entry_t* entry = gw_find_protocol(gw, name);
        if (!entry) return -2;
        stats->request_count = entry->request_count;
        stats->error_count = entry->error_count;
        stats->total_bytes = entry->total_bytes;
    } else {
        for (size_t i = 0; i < gw->protocol_count; i++) {
            stats->request_count += gw->protocols[i].request_count;
            stats->error_count += gw->protocols[i].error_count;
            stats->total_bytes += gw->protocols[i].total_bytes;
        }
    }
    return 0;
}

proto_gateway_iface_t* proto_gateway_standard_create(void) {
    proto_gateway_iface_t* iface = calloc(1, sizeof(proto_gateway_iface_t));
    if (!iface) return NULL;

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

void proto_gateway_standard_destroy(proto_gateway_iface_t* gw) {
    if (!gw) return;
    free(gw);
}

/* Global Registration & Discovery API Implementation */
int proto_interface_register_builtins(void) {
    static bool registered = false;
    if (registered) return 0;

    protocol_registry_t* registry = proto_registry_create();
    if (!registry) return -1;

    int count = proto_registry_initialize_builtins(registry);
    if (count > 0) {
        proto_registry_entry_t* entries = NULL;
        size_t total = proto_registry_list_active(registry, &entries);

        for (size_t i = 0; i < total; i++) {
            proto_adapter_entry_t* entry = (proto_adapter_entry_t*)calloc(1, sizeof(proto_adapter_entry_t));
            if (!entry) continue;

            entry->name = strdup(entries[i].name);
            entry->version = strdup(entries[i].version);
            entry->description = strdup(entries[i].description);
            entry->type = entries[i].type;
            entry->capabilities = entries[i].capabilities;
            entry->is_builtin = true;
            entry->vtable = NULL;
            entry->next = g_adapter_registry;
            g_adapter_registry = entry;
            g_adapter_count++;
        }
        if (entries) free(entries);
    }

    registered = true;
    return count;
}

const proto_adapter_entry_t* proto_interface_find(const char* name) {
    if (!name) return NULL;
    proto_adapter_entry_t* entry = g_adapter_registry;
    while (entry) {
        if (strcmp(entry->name, name) == 0) return entry;
        entry = entry->next;
    }
    return NULL;
}

int proto_interface_list_all(char** json_output) {
    if (!json_output) return -1;

    size_t buf_size = 512 + g_adapter_count * 256;
    char* buf = malloc(buf_size);
    if (!buf) return -2;

    size_t offset = snprintf(buf, buf_size, "{\"adapters\":[");
    proto_adapter_entry_t* entry = g_adapter_registry;
    while (entry) {
        if (entry != g_adapter_registry) offset += snprintf(buf + offset, buf_size - offset, ",");
        offset += snprintf(buf + offset, buf_size - offset,
            "\"%s\"", entry->name ? entry->name : "unknown");
        entry = entry->next;
    }
    offset += snprintf(buf + offset, buf_size - offset, "],\"count\":%zu}", g_adapter_count);

    *json_output = buf;
    return 0;
}

const char* proto_interface_type_name(protocol_type_t type) {
    switch (type) {
        case PROTOCOL_HTTP:       return "HTTP";
        case PROTOCOL_WEBSOCKET:  return "WebSocket";
        case PROTOCOL_STDIO:      return "Stdio";
        case PROTOCOL_IPC:        return "IPC";
        case PROTOCOL_CUSTOM:     return "Custom";
        default:                  return "Unknown";
    }
}

protocol_type_t proto_interface_parse_type(const char* name) {
    if (!name) return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "http") == 0 || strcasecmp(name, "jsonrpc") == 0) return PROTOCOL_HTTP;
    if (strcasecmp(name, "websocket") == 0 || strcasecmp(name, "ws") == 0) return PROTOCOL_WEBSOCKET;
    if (strcasecmp(name, "stdio") == 0) return PROTOCOL_STDIO;
    if (strcasecmp(name, "ipc") == 0) return PROTOCOL_IPC;
    if (strcasecmp(name, "mcp") == 0 || strcasecmp(name, "mcp_v1") == 0) return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "a2a") == 0 || strcasecmp(name, "a2a_v03") == 0) return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "openai") == 0) return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "openjiuwen") == 0) return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "openclaw") == 0) return PROTOCOL_CUSTOM;
    if (strcasecmp(name, "claude") == 0) return PROTOCOL_CUSTOM;
    return PROTOCOL_CUSTOM;
}
