// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0

#include "protocols.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static char s_last_error[512] = {0};
static bool s_framework_initialized = false;

int protocols_framework_init(void) {
    if (s_framework_initialized) return 0;
    s_framework_initialized = true;
    return 0;
}

void protocols_framework_cleanup(void) {
    s_framework_initialized = false;
}

const char* protocols_framework_version(void) {
    return "1.0.0";
}

typedef struct protocol_manager_s {
    protocol_stack_handle_t stacks[32];
    size_t stack_count;
} protocol_manager_s;

protocol_manager_handle_t protocol_manager_create(void) {
    struct protocol_manager_s* mgr = calloc(1, sizeof(struct protocol_manager_s));
    return (protocol_manager_handle_t)mgr;
}

void protocol_manager_destroy(protocol_manager_handle_t manager) {
    if (!manager) return;
    struct protocol_manager_s* mgr = (struct protocol_manager_s*)manager;
    for (size_t i = 0; i < mgr->stack_count; i++) {
        if (mgr->stacks[i]) {
            protocol_stack_destroy(mgr->stacks[i]);
        }
    }
    free(mgr);
}

protocol_stack_handle_t protocol_manager_create_stack(protocol_manager_handle_t manager,
                                                        const protocol_stack_config_t* config) {
    if (!manager || !config) return NULL;
    struct protocol_manager_s* mgr = (struct protocol_manager_s*)manager;
    if (mgr->stack_count >= 32) return NULL;
    protocol_stack_handle_t stack = protocol_stack_create(config);
    if (stack) {
        mgr->stacks[mgr->stack_count++] = stack;
    }
    return stack;
}

void protocol_manager_destroy_stack(protocol_manager_handle_t manager,
                                      protocol_stack_handle_t stack) {
    if (!manager || !stack) return;
    struct protocol_manager_s* mgr = (struct protocol_manager_s*)manager;
    for (size_t i = 0; i < mgr->stack_count; i++) {
        if (mgr->stacks[i] == stack) {
            protocol_stack_destroy(stack);
            mgr->stacks[i] = mgr->stacks[mgr->stack_count - 1];
            mgr->stacks[mgr->stack_count - 1] = NULL;
            mgr->stack_count--;
            return;
        }
    }
}

size_t protocol_manager_get_stacks(protocol_manager_handle_t manager,
                                     protocol_stack_handle_t* stacks,
                                     size_t max_count) {
    if (!manager) return 0;
    struct protocol_manager_s* mgr = (struct protocol_manager_s*)manager;
    size_t count = mgr->stack_count < max_count ? mgr->stack_count : max_count;
    if (stacks) {
        memcpy(stacks, mgr->stacks, count * sizeof(protocol_stack_handle_t));
    }
    return count;
}

static protocol_adapter_t s_http_adapter = {0};
static protocol_adapter_t s_ws_adapter = {0};
static protocol_adapter_t s_grpc_adapter = {0};
static protocol_adapter_t s_mqtt_adapter = {0};
static bool s_adapters_init = false;

static void init_static_adapters(void) {
    if (s_adapters_init) return;
    s_http_adapter.type = PROTOCOL_HTTP;
    s_http_adapter.name = "http";
    s_http_adapter.version = "0.0.5";
    s_http_adapter.description = "HTTP Protocol Adapter";

    s_ws_adapter.type = PROTOCOL_HTTP;
    s_ws_adapter.name = "websocket";
    s_ws_adapter.version = "0.0.5";
    s_ws_adapter.description = "WebSocket Protocol Adapter";

    s_grpc_adapter.type = PROTOCOL_GRPC;
    s_grpc_adapter.name = "grpc";
    s_grpc_adapter.version = "0.0.5";
    s_grpc_adapter.description = "gRPC Protocol Adapter (HTTP/2 framing)";

    s_mqtt_adapter.type = PROTOCOL_MQTT;
    s_mqtt_adapter.name = "mqtt";
    s_mqtt_adapter.version = "3.1.1";
    s_mqtt_adapter.description = "MQTT Protocol Adapter (pub/sub)";

    s_adapters_init = true;
}

const protocol_adapter_t* protocol_adapter_http(void) {
    init_static_adapters();
    return &s_http_adapter;
}

const protocol_adapter_t* protocol_adapter_websocket(void) {
    init_static_adapters();
    return &s_ws_adapter;
}

const protocol_adapter_t* protocol_adapter_grpc(void) {
    init_static_adapters();
    return &s_grpc_adapter;
}

const protocol_adapter_t* protocol_adapter_mqtt(void) {
    init_static_adapters();
    return &s_mqtt_adapter;
}

const char* protocol_error_to_string(protocol_error_t error) {
    switch (error) {
        case PROTOCOL_SUCCESS: return "Success";
        case PROTOCOL_ERROR_INVALID_ARG: return "Invalid argument";
        case PROTOCOL_ERROR_MEMORY: return "Out of memory";
        case PROTOCOL_ERROR_NOT_INITIALIZED: return "Not initialized";
        case PROTOCOL_ERROR_NOT_CONNECTED: return "Not connected";
        case PROTOCOL_ERROR_TIMEOUT: return "Timeout";
        case PROTOCOL_ERROR_ENCODE: return "Encoding error";
        case PROTOCOL_ERROR_DECODE: return "Decoding error";
        case PROTOCOL_ERROR_NETWORK: return "Network error";
        case PROTOCOL_ERROR_PROTOCOL: return "Protocol error";
        case PROTOCOL_ERROR_INTERNAL: return "Internal error";
        default: return "Unknown error";
    }
}

const char* protocol_get_last_error(void) {
    return s_last_error[0] ? s_last_error : "No error";
}

protocol_stack_config_t protocol_stack_config_default(const char* name) {
    protocol_stack_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    if (name) {
        strncpy(cfg.name, name, sizeof(cfg.name) - 1);
        cfg.name[sizeof(cfg.name) - 1] = '\0';
    }
    cfg.default_protocol = PROTOCOL_HTTP;
    cfg.max_message_size = 65536;
    cfg.timeout_ms = 30000;
    cfg.enable_compression = false;
    cfg.enable_encryption = false;
    cfg.custom_config = NULL;
    return cfg;
}

void protocol_stack_config_destroy(protocol_stack_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
}
