#ifndef AGENTOS_GATEWAY_A2A_HANDLER_H
#define AGENTOS_GATEWAY_A2A_HANDLER_H

#include "gateway_protocol_router.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gw_a2a_handler gw_a2a_handler_t;

typedef struct {
    char agent_name[128];
    char agent_version[32];
    char agent_url[512];
    uint32_t capabilities;
    uint32_t default_timeout_ms;
} gw_a2a_handler_config_t;

#define GW_A2A_HANDLER_CONFIG_DEFAULTS { \
    .agent_name = "agentos-a2a", \
    .agent_version = "0.3.0", \
    .agent_url = "http://localhost:8080/a2a", \
    .capabilities = 0x3F, \
    .default_timeout_ms = 60000 }

typedef int (*gw_a2a_task_exec_fn)(const char* task_id,
                                    const char* task_type,
                                    const char* input_json,
                                    char** output_json,
                                    void* user_data);

gw_a2a_handler_t* gw_a2a_handler_create(const gw_a2a_handler_config_t* config);
void gw_a2a_handler_destroy(gw_a2a_handler_t* handler);

int gw_a2a_handler_init(gw_a2a_handler_t* handler);
int gw_a2a_handler_shutdown(gw_a2a_handler_t* handler);

int gw_a2a_handler_register_task_type(gw_a2a_handler_t* handler,
                                       const char* task_type,
                                       gw_a2a_task_exec_fn exec_fn,
                                       void* user_data);

int gw_a2a_handler_get_agent_card(gw_a2a_handler_t* handler,
                                   char** card_json);

int gw_a2a_handler_handle_request(gw_a2a_handler_t* handler,
                                   const char* method,
                                   const char* path,
                                   const char* body_json,
                                   char** response_json);

gw_proto_request_handler_t gw_a2a_handler_get_handler(gw_a2a_handler_t* handler);
void* gw_a2a_handler_get_handler_data(gw_a2a_handler_t* handler);

bool gw_a2a_handler_is_healthy(gw_a2a_handler_t* handler);

#ifdef __cplusplus
}
#endif

#endif
