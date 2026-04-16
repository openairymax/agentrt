/**
 * @file protocol_router.h
 * @brief Standard protocol router interface for AgentOS
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 */

#ifndef AGENTOS_PROTOCOL_ROUTER_STANDARD_H
#define AGENTOS_PROTOCOL_ROUTER_STANDARD_H

#include <agentos/unified_protocol.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int source_protocol;
    int target_protocol;
    const char* source_endpoint;
    const char* target_endpoint;
    uint32_t priority;
    void* transformer_context;
} protocol_rule_t;

typedef int (*message_transformer_t)(const unified_message_t* source,
                                     unified_message_t* target,
                                     void* context);

typedef int (*route_decision_func_t)(const unified_message_t* message,
                                     const protocol_rule_t* rules,
                                     size_t rule_count);

typedef struct protocol_router_s* protocol_router_handle_t;

protocol_router_handle_t protocol_router_create(int default_protocol);
void protocol_router_destroy(protocol_router_handle_t router);
int protocol_router_add_rule(protocol_router_handle_t router,
                             const protocol_rule_t* rule,
                             message_transformer_t transformer);
int protocol_router_route(protocol_router_handle_t router,
                          const unified_message_t* message,
                          unified_message_t* transformed);
int protocol_router_route_batch(protocol_router_handle_t router,
                                const unified_message_t* messages,
                                size_t count,
                                unified_message_t* transformed);
int protocol_router_set_decision_func(protocol_router_handle_t router,
                                      route_decision_func_t decision_func);
int protocol_router_get_stats(protocol_router_handle_t router, char** stats_json);

int protocol_transformer_jsonrpc_to_mcp(const unified_message_t* source,
                                        unified_message_t* target, void* context);
int protocol_transformer_mcp_to_jsonrpc(const unified_message_t* source,
                                        unified_message_t* target, void* context);
int protocol_transformer_openai_to_jsonrpc(const unified_message_t* source,
                                           unified_message_t* target, void* context);
int protocol_transformer_a2a_to_jsonrpc(const unified_message_t* source,
                                        unified_message_t* target, void* context);
int protocol_transformer_default(const unified_message_t* source,
                                 unified_message_t* target, void* context);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PROTOCOL_ROUTER_STANDARD_H */
