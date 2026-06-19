// @owner: team-B
#ifndef AGENTOS_PROTOCOL_ROUTER_TOPLEVEL_H
#define AGENTOS_PROTOCOL_ROUTER_TOPLEVEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct protocol_handler_router_s *protocol_handler_router_t;

typedef enum {
    PROTOCOL_HANDLER_ROUTE_OK = 0,
    PROTOCOL_HANDLER_ROUTE_ERR_INVALID_ARG,
    PROTOCOL_HANDLER_ROUTE_ERR_NOT_FOUND,
    PROTOCOL_HANDLER_ROUTE_NO_HANDLER,
} protocol_handler_route_result_t;

protocol_handler_route_result_t protocol_handler_router_create(protocol_handler_router_t *router);
void protocol_handler_router_destroy(protocol_handler_router_t router);
protocol_handler_route_result_t protocol_handler_router_register(protocol_handler_router_t router,
                                                                 const char *protocol_name,
                                                                 void *handler_context);
protocol_handler_route_result_t protocol_handler_router_route(protocol_handler_router_t router,
                                                              const char *target_protocol,
                                                              const void *message,
                                                              size_t message_len);

#ifdef __cplusplus
}
#endif

#endif
