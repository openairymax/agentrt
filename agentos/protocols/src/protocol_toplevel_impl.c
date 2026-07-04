// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
// @owner: team-B

#include "memory_compat.h"
#include "protocol_router.h"
#include "types.h"
#include "unified_protocol.h"

#include <stdlib.h>
#include <string.h>
#include "../../commons/utils/error/include/error.h"
#include "error.h"

#define MAX_HANDLERS 64
#define MAX_HANDLER_NAME 64

typedef struct {
    char name[MAX_HANDLER_NAME];
    void *context;
    protocol_adapter_t *adapter;
} handler_entry_t;

struct protocol_handler_router_s {
    handler_entry_t handlers[MAX_HANDLERS];
    size_t handler_count;
    uint64_t messages_routed;
    uint64_t messages_failed;
};

int protocol_adapter_create(agentos_protocol_type_t type, protocol_adapter_t *adapter)
{
    if (!adapter)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "protocol_adapter_create: failed");
        return AGENTOS_ERR_UNKNOWN;
        }
    AGENTOS_MEMSET(adapter, 0, sizeof(*adapter));
    adapter->type = type;
    adapter->init = NULL;
    return 0;
}

void protocol_adapter_destroy(protocol_adapter_t adapter)
{
    if (adapter.context) {
        AGENTOS_FREE(adapter.context);
    }
}

int protocol_adapter_send(protocol_adapter_t adapter, const agentos_message_t *msg)
{
    if (!adapter.init)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "protocol_adapter_send: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!msg)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "protocol_adapter_send: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (adapter.send) {
        return adapter.send(adapter.context, msg->data, msg->len);
    }
    return AGENTOS_ERR_NOT_FOUND;
}

int protocol_adapter_recv(protocol_adapter_t adapter, agentos_message_t *msg, size_t max_len)
{
    if (!adapter.init)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "protocol_adapter_recv: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (!msg)
        {
        agentos_error_push_ex(AGENTOS_ERR_UNKNOWN, __FILE__, __LINE__, __func__, "protocol_adapter_recv: IO error");
        return AGENTOS_ERR_UNKNOWN;
        }
    if (adapter.receive) {
        void *data = NULL;
        size_t size = 0;
        int rc = adapter.receive(adapter.context, &data, &size, 0);
        if (rc == 0 && data && size > 0) {
            size_t copy_len = size < max_len ? size : max_len;
            msg->len = copy_len;
            AGENTOS_FREE(data);
        }
        return rc;
    }
    return AGENTOS_ERR_NOT_FOUND;
}

const char *protocol_type_name(agentos_protocol_type_t type)
{
    switch (type) {
    case AGENTOS_PROTOCOL_JSON_RPC:
        return "JSON-RPC";
    case AGENTOS_PROTOCOL_MCP:
        return "MCP";
    case AGENTOS_PROTOCOL_A2A:
        return "A2A";
    case AGENTOS_PROTOCOL_OPENAI:
        return "OpenAI";
    case AGENTOS_PROTOCOL_OPENJIUWEN:
        return "OpenJiuwen";
    case AGENTOS_PROTOCOL_CLAUDE:
        return "Claude";
    case AGENTOS_PROTOCOL_AGNTCY:
        return "AGNTCY";
    case AGENTOS_PROTOCOL_CHINA_ECO:
        return "ChinaEco";
    case AGENTOS_PROTOCOL_OPENCLAW:
        return "OpenClaw";
    default:
        return "Unknown";
    }
}

protocol_handler_route_result_t protocol_handler_router_create(protocol_handler_router_t *router)
{
    if (!router)
        return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;
    struct protocol_handler_router_s *r = (struct protocol_handler_router_s *)AGENTOS_CALLOC(
        1, sizeof(struct protocol_handler_router_s));
    if (!r)
        return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;
    *router = r;
    return PROTOCOL_HANDLER_ROUTE_OK;
}

void protocol_handler_router_destroy(protocol_handler_router_t router)
{
    if (!router)
        return;
    AGENTOS_MEMSET(router->handlers, 0, sizeof(router->handlers));
    router->handler_count = 0;
    AGENTOS_FREE(router);
}

protocol_handler_route_result_t protocol_handler_router_register(protocol_handler_router_t router,
                                                                 const char *protocol_name,
                                                                 void *handler_context)
{
    if (!router || !protocol_name)
        return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;
    if (router->handler_count >= MAX_HANDLERS)
        return PROTOCOL_HANDLER_ROUTE_NO_HANDLER;

    for (size_t i = 0; i < router->handler_count; i++) {
        if (strcmp(router->handlers[i].name, protocol_name) == 0) {
            router->handlers[i].context = handler_context;
            return PROTOCOL_HANDLER_ROUTE_OK;
        }
    }

    AGENTOS_STRNCPY_TERM(router->handlers[router->handler_count].name, protocol_name, MAX_HANDLER_NAME);
    router->handlers[router->handler_count].name[MAX_HANDLER_NAME - 1] = '\0';
    router->handlers[router->handler_count].context = handler_context;
    router->handlers[router->handler_count].adapter = NULL;
    router->handler_count++;
    return PROTOCOL_HANDLER_ROUTE_OK;
}

protocol_handler_route_result_t protocol_handler_router_route(protocol_handler_router_t router,
                                                              const char *target_protocol,
                                                              const void *message,
                                                              size_t message_len)
{
    if (!router || !target_protocol)
        return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;
    if (!message || message_len == 0)
        return PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG;

    for (size_t i = 0; i < router->handler_count; i++) {
        if (strcmp(router->handlers[i].name, target_protocol) == 0) {
            router->messages_routed++;
            return PROTOCOL_HANDLER_ROUTE_OK;
        }
    }

    router->messages_failed++;
    return PROTOCOL_HANDLER_ROUTE_ERR_NOT_FOUND;
}
