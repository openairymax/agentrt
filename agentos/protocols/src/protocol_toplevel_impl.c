// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "unified_protocol.h"
#include "protocol_router.h"
#include <stdlib.h>
#include <string.h>

#define MAX_HANDLERS 64
#define MAX_HANDLER_NAME 64

typedef struct {
    char name[MAX_HANDLER_NAME];
    void* context;
    protocol_adapter_t* adapter;
} handler_entry_t;

struct protocol_handler_router_s {
    handler_entry_t handlers[MAX_HANDLERS];
    size_t handler_count;
    uint64_t messages_routed;
    uint64_t messages_failed;
};

int protocol_adapter_create(agentos_protocol_type_t type,
                              protocol_adapter_t* adapter) {
    if (!adapter) return -1;
    memset(adapter, 0, sizeof(*adapter));
    adapter->type = type;
    adapter->init = NULL;
    return 0;
}

void protocol_adapter_destroy(protocol_adapter_t adapter) {
    if (adapter.context) {
        free(adapter.context);
    }
}

int protocol_adapter_send(protocol_adapter_t adapter,
                            const agentos_message_t* msg) {
    if (!adapter.init) return -1;
    if (!msg) return -1;
    if (adapter.send) {
        return adapter.send(adapter.context, msg->data, msg->len);
    }
    return -2;
}

int protocol_adapter_recv(protocol_adapter_t adapter,
                            agentos_message_t* msg, size_t max_len) {
    if (!adapter.init) return -1;
    if (!msg) return -1;
    if (adapter.receive) {
        void* data = NULL;
        size_t size = 0;
        int rc = adapter.receive(adapter.context, &data, &size, 0);
        if (rc == 0 && data && size > 0) {
            size_t copy_len = size < max_len ? size : max_len;
            msg->len = copy_len;
            free(data);
        }
        return rc;
    }
    return -2;
}

const char* protocol_type_name(agentos_protocol_type_t type) {
    switch (type) {
        case AGENTOS_PROTOCOL_JSON_RPC: return "JSON-RPC";
        case AGENTOS_PROTOCOL_MCP: return "MCP";
        case AGENTOS_PROTOCOL_A2A: return "A2A";
        case AGENTOS_PROTOCOL_OPENAI: return "OpenAI";
        case AGENTOS_PROTOCOL_OPENJIUWEN: return "OpenJiuwen";
        case AGENTOS_PROTOCOL_CLAUDE: return "Claude";
        case AGENTOS_PROTOCOL_AGNTCY: return "AGNTCY";
        case AGENTOS_PROTOCOL_CHINA_ECO: return "ChinaEco";
        case AGENTOS_PROTOCOL_OPENCLAW: return "OpenClaw";
        default: return "Unknown";
    }
}

protocol_handler_route_result_t protocol_handler_router_create(protocol_handler_router_t* router) {
    if (!router) return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;
    struct protocol_handler_router_s* r = (struct protocol_handler_router_s*)calloc(1, sizeof(struct protocol_handler_router_s));
    if (!r) return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;
    *router = r;
    return PROTOCOL_HANDLER_ROUTE_OK;
}

void protocol_handler_router_destroy(protocol_handler_router_t router) {
    if (!router) return;
    memset(router->handlers, 0, sizeof(router->handlers));
    router->handler_count = 0;
    free(router);
}

protocol_handler_route_result_t protocol_handler_router_register(protocol_handler_router_t router,
                                                                     const char* protocol_name,
                                                                     void* handler_context) {
    if (!router || !protocol_name) return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;
    if (router->handler_count >= MAX_HANDLERS) return PROTOCOL_HANDLER_ROUTE_NO_HANDLER;

    for (size_t i = 0; i < router->handler_count; i++) {
        if (strcmp(router->handlers[i].name, protocol_name) == 0) {
            router->handlers[i].context = handler_context;
            return PROTOCOL_HANDLER_ROUTE_OK;
        }
    }

    strncpy(router->handlers[router->handler_count].name, protocol_name, MAX_HANDLER_NAME - 1);
    router->handlers[router->handler_count].name[MAX_HANDLER_NAME - 1] = '\0';
    router->handlers[router->handler_count].context = handler_context;
    router->handlers[router->handler_count].adapter = NULL;
    router->handler_count++;
    return PROTOCOL_HANDLER_ROUTE_OK;
}

protocol_handler_route_result_t protocol_handler_router_route(protocol_handler_router_t router,
                                                                  const char* target_protocol,
                                                                  const void* message,
                                                                  size_t message_len) {
    if (!router || !target_protocol) return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;
    if (!message || message_len == 0) return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;

    for (size_t i = 0; i < router->handler_count; i++) {
        if (strcmp(router->handlers[i].name, target_protocol) == 0) {
            router->messages_routed++;
            return PROTOCOL_HANDLER_ROUTE_OK;
        }
    }

    router->messages_failed++;
    return PROTOCOL_HANDLER_ROUTE_ERR_NOT_FOUND;
}
