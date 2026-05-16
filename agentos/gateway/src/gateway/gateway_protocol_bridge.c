// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file gateway_protocol_bridge.c
 * @brief Gateway ↔ Protocols Module Bridge Implementation
 */

#include "gateway_protocol_bridge.h"
#include "protocol_registry.h"
#include "protocol_extension_framework.h"
#include "unified_protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

struct gw_protocol_bridge_s {
    gw_protocol_bridge_config_t config;
    void* router;
    void* registry;
    void* ext_framework;
    void* default_handler;
    void* handlers[GW_PROTO_COUNT];
    char handler_patterns[GW_PROTO_COUNT][256];
    gw_bridge_stats_t stats;
    bool initialized;
};

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int gw_protocol_bridge_create(const gw_protocol_bridge_config_t* config,
                               gw_protocol_bridge_handle_t* out_handle) {
    if (!config || !out_handle) return -1;

    struct gw_protocol_bridge_s* bridge =
        calloc(1, sizeof(struct gw_protocol_bridge_s));
    if (!bridge) return -2;

    bridge->config = *config;
    bridge->default_handler = NULL;
    bridge->initialized = false;

    bridge->router = NULL;

    bridge->registry = NULL;

    bridge->ext_framework = NULL;

    bridge->initialized = true;
    memset(&bridge->stats, 0, sizeof(bridge->stats));

    *out_handle = bridge;
    return 0;
}

void gw_protocol_bridge_destroy(gw_protocol_bridge_handle_t handle) {
    if (!handle) return;
    struct gw_protocol_bridge_s* bridge = (struct gw_protocol_bridge_s*)handle;
    if (bridge->router) free(bridge->router);
    if (bridge->registry) free(bridge->registry);
    if (bridge->ext_framework) free(bridge->ext_framework);
    bridge->initialized = false;
    free(bridge);
}

bool gw_protocol_bridge_is_ready(gw_protocol_bridge_handle_t handle) {
    return handle && ((struct gw_protocol_bridge_s*)handle)->initialized;
}

/* ============================================================================
 * Protocol Handler Registration
 * ============================================================================ */

int gw_protocol_bridge_register_handler(
    gw_protocol_bridge_handle_t bridge,
    gw_proto_type_t proto_type,
    const char* endpoint_pattern,
    void* (*handler)(const void*, size_t, size_t*))
{
    if (!bridge || !handler || proto_type >= GW_PROTO_COUNT) return -1;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;
    if (!b->initialized) return -2;

    b->handlers[proto_type] = (void*)handler;
    if (endpoint_pattern) {
        strncpy(b->handler_patterns[proto_type], endpoint_pattern,
                sizeof(b->handler_patterns[proto_type]) - 1);
    }
    return 0;
}

int gw_protocol_bridge_set_default_handler(
    gw_protocol_bridge_handle_t bridge,
    void* (*handler)(const void*, size_t, size_t*))
{
    if (!bridge) return -1;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;
    b->default_handler = (void*)handler;
    return 0;
}

/* ============================================================================
 * Auto-Detection
 * ============================================================================ */

int gw_protocol_bridge_detect_protocol(
    gw_protocol_bridge_handle_t bridge,
    const char* data,
    size_t size,
    const char* content_type_hint,
    gw_detection_result_t* out_result)
{
    if (!bridge || !data || !out_result) return -1;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;

    memset(out_result, 0, sizeof(*out_result));

    double confidence = 50.0;
    gw_proto_type_t detected = GW_PROTO_JSONRPC;
    bool is_streaming = false;
    bool has_binary = false;

    if (size > 4 && (unsigned char)data[0] == 0x4F &&
        (unsigned char)data[1] == 0x4A && (unsigned char)data[2] == 0x57) {
        detected = GW_PROTO_OPENJIUWEN;
        confidence = 95.0;
        has_binary = true;
        goto done;
    }

    if (content_type_hint) {
        if (strstr(content_type_hint, "application/json")) {
            confidence += 10.0;
            if (strstr(content_type_hint, "rpc")) {
                detected = GW_PROTO_JSONRPC;
                confidence += 20.0;
            } else if (strstr(data, "\"model\"") ||
                       strstr(data, "\"messages\"")) {
                detected = GW_PROTO_OPENAI;
                confidence += 25.0;
            } else if (strstr(data, "\"tools\"") ||
                       strstr(data, "\"method\"") && strstr(data, "\"params\"")) {
                detected = GW_PROTO_MCP;
                confidence += 15.0;
            } else if (strstr(data, "\"agent\"") ||
                       strstr(data, "\"task\"")) {
                detected = GW_PROTO_A2A;
                confidence += 15.0;
            }
        } else if (strstr(content_type_hint, "text/event-stream")) {
            is_streaming = true;
            confidence += 10.0;
        }
    }

    if (data && size > 0) {
        if (strncmp(data, "{\"jsonrpc\"", 11) == 0) {
            detected = GW_PROTO_JSONRPC;
            confidence = 98.0;
        } else if (strstr(data, "\"jsonrpc\":\"2.0\"")) {
            detected = GW_PROTO_JSONRPC;
            confidence = 95.0;
        } else if (strstr(data, "\"type\": \"message\"") &&
                   strstr(data, "\"role\": \"user\"")) {
            detected = GW_PROTO_MCP;
            confidence = 85.0;
        } else if (strstr(data, "\"protocol\": \"a2a\"") ||
                   strstr(data, "\"task/delegate\"")) {
            detected = GW_PROTO_A2A;
            confidence = 90.0;
        } else if (strstr(data, "\"model\": \"gpt") ||
                   strstr(data, "\"model\": \"o1") ||
                   (strstr(data, "\"model\"") &&
                    strstr(data, "\"openai\""))) {
            detected = GW_PROTO_OPENAI;
            confidence = 88.0;
        } else if ((strstr(data, "\"model\"") &&
                    (strstr(data, "\"claude-") || strstr(data, "\"anthropic\""))) ||
                   (strstr(data, "\"max_tokens\"") &&
                    strstr(data, "\"anthropic-version"))) {
            detected = GW_PROTO_CLAUDE;
            confidence = 90.0;
        } else if (strstr(data, "\"openclaw\"") ||
                   strstr(data, "\"cluster_mode\"") ||
                   (strstr(data, "\"agent_id\"") && strstr(data, "\"security_level\""))) {
            detected = GW_PROTO_OPENCLAW;
            confidence = 88.0;
        }

        for (size_t i = 0; i < size; i++) {
            unsigned char c = (unsigned char)data[i];
            if (c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
                has_binary = true;
                break;
            }
        }
    }

done:
    out_result->detected_type = detected;
    out_result->confidence = confidence > 100.0 ? 100.0 : confidence;
    out_result->is_streaming = is_streaming;
    out_result->has_binary_payload = has_binary;

    static const char* type_names[] = {
        "jsonrpc", "mcp", "a2a", "openai", "openjiuwen", "openclaw", "claude"
    };
    strncpy(out_result->type_name, type_names[detected],
            sizeof(out_result->type_name) - 1);

    b->stats.total_requests++;
    if (detected < GW_PROTO_COUNT) {
        b->stats.requests_by_proto[detected]++;
    } else {
        b->stats.detection_failures++;
    }

    return 0;
}

/* ============================================================================
 * Request Processing Pipeline
 * ============================================================================ */

int gw_protocol_bridge_process_request(
    gw_protocol_bridge_handle_t bridge,
    const gw_incoming_request_t* incoming,
    gw_processed_response_t* out_response)
{
    if (!bridge || !incoming || !out_response) return -1;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;
    if (!b->initialized) return -2;

    memset(out_response, 0, sizeof(*out_response));

    uint64_t start_ns = agentos_time_ns();

    gw_detection_result_t detection;
    int det_ret = gw_protocol_bridge_detect_protocol(
        bridge, incoming->raw_data, incoming->raw_size,
        incoming->content_type, &detection);

    if (det_ret != 0) {
        out_response->status_code = 400;
        out_response->response_data = strdup("{\"error\":\"Protocol detection failed\"}");
        if (out_response->response_data) {
            out_response->response_size = strlen(out_response->response_data);
        }
        out_response->content_type = strdup("application/json");
        out_response->detected_protocol = strdup("unknown");
        return det_ret;
    }

    out_response->detected_protocol = strdup(detection.type_name);

    unified_message_t source_msg;
    memset(&source_msg, 0, sizeof(source_msg));
    source_msg.protocol = PROTOCOL_HTTP;
    strncpy(source_msg.protocol_name, detection.type_name,
            sizeof(source_msg.protocol_name) - 1);
    source_msg.payload = (void*)incoming->raw_data;
    source_msg.payload_size = incoming->raw_size;
    if (incoming->x_trace_id) {
        strncpy(source_msg.metadata.trace_id, incoming->x_trace_id,
                sizeof(source_msg.metadata.trace_id) - 1);
    }

    if (b->handlers[detection.detected_type]) {
        void* (*handler_fn)(const void*, size_t, size_t*) =
            (void* (*)(const void*, size_t, size_t*))b->handlers[detection.detected_type];
        size_t resp_size = 0;
        void* result = handler_fn(incoming->raw_data, incoming->raw_size, &resp_size);

        if (result && resp_size > 0) {
            out_response->response_data = (char*)malloc(resp_size + 1);
            if (out_response->response_data) {
                memcpy(out_response->response_data, result, resp_size);
                out_response->response_data[resp_size] = '\0';
                out_response->response_size = resp_size;
            }
            free(result);
        }
        out_response->status_code = 200;
        out_response->transformed = false;
    } else if (b->default_handler) {
        void* (*def_handler)(const void*, size_t, size_t*) =
            (void* (*)(const void*, size_t, size_t*))b->default_handler;
        size_t resp_size = 0;
        void* result = def_handler(incoming->raw_data, incoming->raw_size, &resp_size);

        if (result && resp_size > 0) {
            out_response->response_data = (char*)malloc(resp_size + 1);
            if (out_response->response_data) {
                memcpy(out_response->response_data, result, resp_size);
                out_response->response_data[resp_size] = '\0';
                out_response->response_size = resp_size;
            }
            free(result);
        }
        out_response->status_code = 200;
        out_response->transformed = false;
    } else {
        unified_message_t target_msg;
        int transform_ret = protocol_auto_transform(
            &source_msg, &target_msg, "jsonrpc");

        if (transform_ret == 0) {
            out_response->transformed = true;
            b->stats.transformations_performed++;

            if (target_msg.payload && target_msg.payload_size > 0) {
                out_response->response_data = malloc(target_msg.payload_size + 1);
                if (out_response->response_data) {
                    memcpy(out_response->response_data, target_msg.payload,
                           target_msg.payload_size);
                    out_response->response_data[target_msg.payload_size] = '\0';
                    out_response->response_size = target_msg.payload_size;
                }
            }
            out_response->status_code = 200;
        } else {
            out_response->response_data =
                strdup("{\"error\":\"No handler and transformation failed\"}");
            out_response->response_size = strlen(out_response->response_data);
            out_response->status_code = 500;
        }
    }

    out_response->content_type = strdup("application/json");

    uint64_t end_ns = agentos_time_ns();
    out_response->process_time_ns = end_ns - start_ns;

    uint64_t total_time = b->stats.total_requests > 0 ?
        b->stats.avg_process_time_ns : 0;
    b->stats.avg_process_time_ns =
        (total_time + out_response->process_time_ns) / 2;

    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int gw_protocol_bridge_get_stats(gw_protocol_bridge_handle_t bridge,
                                 gw_bridge_stats_t* out_stats) {
    if (!bridge || !out_stats) return -1;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;
    memcpy(out_stats, &b->stats, sizeof(*out_stats));

    static const char* names[] = {"jsonrpc","mcp","a2a","openai","openjiuwen","openclaw","claude"};
    char buf[256] = {0};
    size_t offset = 0;
    for (int i = 0; i < GW_PROTO_COUNT; i++) {
        if (b->stats.requests_by_proto[i] > 0) {
            if (offset > 0) offset += snprintf(buf + offset, sizeof(buf) - offset, ",");
            offset += snprintf(buf + offset, sizeof(buf) - offset,
                               "%s(%llu)", names[i],
                               (unsigned long long)b->stats.requests_by_proto[i]);
        }
    }
    strncpy(out_stats->active_protocols, buf, sizeof(out_stats->active_protocols) - 1);
    return 0;
}

int gw_protocol_bridge_reset_stats(gw_protocol_bridge_handle_t bridge) {
    if (!bridge) return -1;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;
    memset(&b->stats, 0, sizeof(b->stats));
    return 0;
}

char* gw_protocol_bridge_diagnose(gw_protocol_bridge_handle_t bridge) {
    if (!bridge) return NULL;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;

    gw_bridge_stats_t stats;
    gw_protocol_bridge_get_stats(bridge, &stats);

    int registry_count = 0;

    char* diag = malloc(3072);
    if (!diag) return NULL;

    snprintf(diag, 3072,
        "{\n"
        "  \"bridge_status\": \"%s\",\n"
        "  \"auto_detect\": %s,\n"
        "  \"transform_enabled\": %s,\n"
        "  \"default_protocol\": \"%s\",\n"
        "  \"total_requests\": %llu,\n"
        "  \"requests_by_protocol\": {\n"
        "    \"jsonrpc\": %llu,\n"
        "    \"mcp\": %llu,\n"
        "    \"a2a\": %llu,\n"
        "    \"openai\": %llu,\n"
        "    \"openjiuwen\": %llu,\n"
        "    \"openclaw\": %llu,\n"
        "    \"claude\": %llu\n"
        "  },\n"
        "  \"transformations_performed\": %llu,\n"
        "  \"detection_failures\": %llu,\n"
        "  \"avg_process_time_ns\": %llu,\n"
        "  \"active_handlers\": %zu,\n"
        "  \"registry_protocols\": %d,\n"
        "  \"extension_adapters\": %zu\n"
        "}",
        b->initialized ? "READY" : "NOT_INITIALIZED",
        b->config.auto_detect_enabled ? "true" : "false",
        b->config.transform_enabled ? "true" : "false",
        b->config.default_protocol,
        (unsigned long long)stats.total_requests,
        (unsigned long long)stats.requests_by_proto[0],
        (unsigned long long)stats.requests_by_proto[1],
        (unsigned long long)stats.requests_by_proto[2],
        (unsigned long long)stats.requests_by_proto[3],
        (unsigned long long)stats.requests_by_proto[4],
        (unsigned long long)stats.requests_by_proto[5],
        (unsigned long long)stats.requests_by_proto[6],
        (unsigned long long)stats.transformations_performed,
        (unsigned long long)stats.detection_failures,
        (unsigned long long)stats.avg_process_time_ns,
        (size_t)(
            (b->handlers[0]?1:0) + (b->handlers[1]?1:0) +
            (b->handlers[2]?1:0) + (b->handlers[3]?1:0) +
            (b->handlers[4]?1:0) + (b->handlers[5]?1:0) +
            (b->handlers[6]?1:0)
        ),
        registry_count,
        (size_t)0
    );

    return diag;
}

/* ============================================================================
 * Registry & Extension Integration
 * ============================================================================ */

int gw_protocol_bridge_list_registry_protocols(
    gw_protocol_bridge_handle_t bridge,
    char** protocols_json) {
    if (!bridge || !protocols_json) return -1;
    *protocols_json = strdup("{\"registered_protocols\":[],\"total\":0}");
    return 0;
}

int gw_protocol_bridge_load_extensions_from_config(
    gw_protocol_bridge_handle_t bridge,
    const char* config_json) {
    if (!bridge || !config_json) return -1;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;

    if (!b->ext_framework) return -ENOSYS;
    return -ENOSYS;
}

int gw_protocol_bridge_register_extension_adapter(
    gw_protocol_bridge_handle_t bridge,
    gw_proto_type_t proto_type,
    void* handler) {
    if (!bridge || !handler || proto_type >= GW_PROTO_COUNT) return -1;
    struct gw_protocol_bridge_s* b = (struct gw_protocol_bridge_s*)bridge;
    if (!b->initialized) return -2;

    b->handlers[proto_type] = handler;
    static const char* patterns[] = {
        "/rpc", "/mcp", "/a2a", "/v1/chat/completions",
        "/openjiuwen", "/openclaw", "/v1/messages"
    };
    strncpy(b->handler_patterns[proto_type], patterns[proto_type],
            sizeof(b->handler_patterns[proto_type]) - 1);
    return 0;
}
