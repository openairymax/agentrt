// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file gateway_protocol_bridge.h
 * @brief Gateway ↔ Protocols Module Bridge Interface
 *
 * 网关与协议系统之间的桥接层，将 protocols 模块的统一协议能力
 * 集成到 gateway_d 的 HTTP/WS/Stdio 三种传输模式中。
 *
 * 架构:
 *   Gateway HTTP/WS/Stdio → bridge → protocol_router → transformer → handler
 *
 * @since 2.0.0
 */

#ifndef AGENTOS_GATEWAY_PROTOCOL_BRIDGE_H
#define AGENTOS_GATEWAY_PROTOCOL_BRIDGE_H

#include "gateway.h"
#include "protocol_router.h"
#include "protocol_transformers.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Bridge Configuration
 * ============================================================================ */

typedef struct {
    bool auto_detect_enabled;
    bool transform_enabled;
    char default_protocol[32];
    int max_concurrent_requests;
    int request_timeout_ms;
    size_t max_request_size;
    bool metrics_enabled;
    bool logging_enabled;
} gw_protocol_bridge_config_t;

/* ============================================================================
 * Bridge Handle & Lifecycle
 * ============================================================================ */

typedef struct gw_protocol_bridge_s* gw_protocol_bridge_handle_t;

int gw_protocol_bridge_create(const gw_protocol_bridge_config_t* config,
                               gw_protocol_bridge_handle_t* out_handle);

void gw_protocol_bridge_destroy(gw_protocol_bridge_handle_t handle);

bool gw_protocol_bridge_is_ready(gw_protocol_bridge_handle_t handle);

/* ============================================================================
 * Protocol Registration (from gateway perspective)
 * ============================================================================ */

typedef enum {
    GW_PROTO_JSONRPC = 0,
    GW_PROTO_MCP,
    GW_PROTO_A2A,
    GW_PROTO_OPENAI,
    GW_PROTO_OPENJIUWEN,
    GW_PROTO_COUNT
} gw_proto_type_t;

int gw_protocol_bridge_register_handler(
    gw_protocol_bridge_handle_t bridge,
    gw_proto_type_t proto_type,
    const char* endpoint_pattern,
    void* (*handler)(const void* request, size_t request_size, size_t* response_size)
);

int gw_protocol_bridge_set_default_handler(
    gw_protocol_bridge_handle_t bridge,
    void* (*handler)(const void* request, size_t request_size, size_t* response_size));

/* ============================================================================
 * Request Processing Pipeline
 * ============================================================================ */

typedef struct {
    const char* raw_data;
    size_t raw_size;
    const char* content_type;
    const char* accept_type;
    const char* path_info;
    const char* query_string;
    const char* x_trace_id;
    const char* authorization;
} gw_incoming_request_t;

typedef struct {
    char* response_data;
    size_t response_size;
    char* content_type;
    int status_code;
    char* detected_protocol;
    uint64_t process_time_ns;
    bool transformed;
} gw_processed_response_t;

int gw_protocol_bridge_process_request(
    gw_protocol_bridge_handle_t bridge,
    const gw_incoming_request_t* incoming,
    gw_processed_response_t* out_response);

/* ============================================================================
 * Auto-Detection
 * ============================================================================ */

typedef struct {
    gw_proto_type_t detected_type;
    char type_name[32];
    double confidence;
    bool is_streaming;
    bool has_binary_payload;
} gw_detection_result_t;

int gw_protocol_bridge_detect_protocol(
    gw_protocol_bridge_handle_t bridge,
    const char* data,
    size_t size,
    const char* content_type_hint,
    gw_detection_result_t* out_result);

/* ============================================================================
 * Statistics & Diagnostics
 * ============================================================================ */

typedef struct {
    uint64_t total_requests;
    uint64_t requests_by_proto[GW_PROTO_COUNT];
    uint64_t transformations_performed;
    uint64_t detection_failures;
    uint64_t avg_process_time_ns;
    double detection_accuracy;
    char active_protocols[256];
} gw_bridge_stats_t;

int gw_protocol_bridge_get_stats(gw_protocol_bridge_handle_t bridge,
                                 gw_bridge_stats_t* out_stats);

int gw_protocol_bridge_reset_stats(gw_protocol_bridge_handle_t bridge);

char* gw_protocol_bridge_diagnose(gw_protocol_bridge_handle_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_PROTOCOL_BRIDGE_H */
