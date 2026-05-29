/**
 * @file mcp_transport.h
 * @brief MCP Transport Layer Interface
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * Defines the transport abstraction for MCP protocol.
 * Supports STDIO and HTTP+SSE transport modes per MCP specification.
 */

#ifndef AGENTOS_MCP_TRANSPORT_H
#define AGENTOS_MCP_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCP_TRANSPORT_STDIO = 0,
    MCP_TRANSPORT_HTTP_SSE = 1,
    MCP_TRANSPORT_STREAMABLE_HTTP = 2
} mcp_transport_type_t;

typedef enum {
    MCP_TRANSPORT_DISCONNECTED = 0,
    MCP_TRANSPORT_CONNECTING = 1,
    MCP_TRANSPORT_CONNECTED = 2,
    MCP_TRANSPORT_ERROR = 3
} mcp_transport_state_t;

typedef struct mcp_transport mcp_transport_t;

typedef void (*mcp_transport_message_fn)(const char *message, size_t length, void *user_data);

typedef void (*mcp_transport_error_fn)(int error_code, const char *error_message, void *user_data);

typedef void (*mcp_transport_state_fn)(mcp_transport_state_t new_state, void *user_data);

typedef struct {
    mcp_transport_type_t type;
    mcp_transport_message_fn on_message;
    mcp_transport_error_fn on_error;
    mcp_transport_state_fn on_state_change;
    void *user_data;
    uint32_t read_timeout_ms;
    uint32_t write_timeout_ms;
    size_t max_message_size;
    union {
        struct {
            int input_fd;
            int output_fd;
        } stdio;
        struct {
            const char *base_url;
            const char *api_key;
            const char *sse_endpoint;
            const char *post_endpoint;
            uint32_t reconnect_interval_ms;
            uint32_t max_reconnect_attempts;
        } http;
    } config;
} mcp_transport_config_t;

mcp_transport_config_t mcp_transport_config_stdio_default(void);

mcp_transport_config_t mcp_transport_config_http_default(const char *base_url);

mcp_transport_t *mcp_transport_create(const mcp_transport_config_t *config);

void mcp_transport_destroy(mcp_transport_t *transport);

int mcp_transport_start(mcp_transport_t *transport);

int mcp_transport_stop(mcp_transport_t *transport);

int mcp_transport_send(mcp_transport_t *transport, const char *message, size_t length);

int mcp_transport_receive(mcp_transport_t *transport, char **out_message, size_t *out_length,
                          uint32_t timeout_ms);

mcp_transport_state_t mcp_transport_get_state(const mcp_transport_t *transport);

mcp_transport_type_t mcp_transport_get_type(const mcp_transport_t *transport);

const char *mcp_transport_state_string(mcp_transport_state_t state);

const char *mcp_transport_type_string(mcp_transport_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MCP_TRANSPORT_H */
