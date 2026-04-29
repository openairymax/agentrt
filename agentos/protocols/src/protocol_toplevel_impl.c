// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "unified_protocol.h"
#include "protocol_router.h"
#include <stdlib.h>
#include <string.h>

int protocol_adapter_create(agentos_protocol_type_t type,
                              protocol_adapter_t* adapter) {
    if (!adapter) return -1;
    memset(adapter, 0, sizeof(*adapter));
    adapter->type = type;
    return 0;
}

void protocol_adapter_destroy(protocol_adapter_t adapter) {
    (void)adapter;
}

int protocol_adapter_send(protocol_adapter_t adapter,
                            const agentos_message_t* msg) {
    if (!adapter.init) return -1;
    if (!msg) return -1;
    return 0;
}

int protocol_adapter_recv(protocol_adapter_t adapter,
                            agentos_message_t* msg, size_t max_len) {
    if (!adapter.init) return -1;
    if (!msg) return -1;
    (void)max_len;
    return 0;
}

const char* protocol_type_name(agentos_protocol_type_t type) {
    switch (type) {
        case AGENTOS_PROTOCOL_JSON_RPC: return "JSON-RPC";
        case AGENTOS_PROTOCOL_MCP: return "MCP";
        case AGENTOS_PROTOCOL_A2A: return "A2A";
        case AGENTOS_PROTOCOL_OPENAI: return "OpenAI";
        case AGENTOS_PROTOCOL_OPENJIUWEN: return "OpenJiuwen";
        default: return "Unknown";
    }
}

struct protocol_router_s {
    int dummy;
};

protocol_route_result_t protocol_router_create(protocol_router_t* router) {
    if (!router) return PROTOCOL_ROUTE_ERR_INVALID_ARG;
    *router = calloc(1, sizeof(struct protocol_router_s));
    return *router ? PROTOCOL_ROUTE_OK : PROTOCOL_ROUTE_ERR_INVALID_ARG;
}

void protocol_router_destroy(protocol_router_t router) {
    free(router);
}

protocol_route_result_t protocol_router_register_handler(protocol_router_t router,
                                                           const char* protocol_name,
                                                           void* handler_context) {
    if (!router || !protocol_name) return PROTOCOL_ROUTE_ERR_INVALID_ARG;
    (void)handler_context;
    return PROTOCOL_ROUTE_OK;
}

protocol_route_result_t protocol_router_route(protocol_router_t router,
                                                const char* target_protocol,
                                                const void* message,
                                                size_t message_len) {
    if (!router || !target_protocol) return PROTOCOL_ROUTE_ERR_INVALID_ARG;
    (void)message; (void)message_len;
    return PROTOCOL_ROUTE_OK;
}
