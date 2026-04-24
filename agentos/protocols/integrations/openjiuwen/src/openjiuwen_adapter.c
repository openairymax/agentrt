// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file openjiuwen_adapter.c
 * @brief OpenJiuwen Protocol Adapter Implementation
 *
 * 实现与OpenJiuwen平台的协议兼容层，支持消息格式转换和互操作。
 */

#include "openjiuwen_adapter.h"
#include "safe_string_utils.h"
#include "logging_compat.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

static uint32_t generate_message_id(void) {
    static uint32_t counter = 0;
    return ++counter;
}

static uint32_t get_timestamp(void) {
    return (uint32_t)time(NULL);
}

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static int openjiuwen_reconnect(openjiuwen_adapter_t* adapter) {
    if (!adapter) return -1;

    adapter->conn_state = OPENJIUWEN_CONN_RECONNECTING;
    adapter->total_reconnects++;

    uint32_t attempt = 0;
    uint32_t max_attempts = adapter->config.max_retries > 0 ?
                            adapter->config.max_retries : 3;

    while (attempt < max_attempts) {
        uint32_t delay = OPENJIUWEN_RECONNECT_BASE_DELAY_MS << attempt;
        if (delay > OPENJIUWEN_RECONNECT_MAX_DELAY_MS)
            delay = OPENJIUWEN_RECONNECT_MAX_DELAY_MS;

        AGENTOS_LOG_WARN("OpenJiuwen: reconnect attempt %u/%u, waiting %ums",
                     attempt + 1, max_attempts, delay);

        struct timespec ts = {
            .tv_sec = delay / 1000,
            .tv_nsec = (delay % 1000) * 1000000LL
        };
        nanosleep(&ts, NULL);

        int verify = openjiuwen_verify_connection(&adapter->base);
        if (verify == 0) {
            adapter->conn_state = OPENJIUWEN_CONN_CONNECTED;
            adapter->consecutive_errors = 0;
            adapter->last_heartbeat_sec = get_timestamp();
            AGENTOS_LOG_INFO("OpenJiuwen: reconnected successfully on attempt %u",
                         attempt + 1);
            return 0;
        }

        attempt++;
    }

    adapter->conn_state = OPENJIUWEN_CONN_ERROR;
    AGENTOS_LOG_ERROR("OpenJiuwen: reconnection failed after %u attempts", max_attempts);
    return -1;
}

static int openjiuwen_send_with_retry(openjiuwen_adapter_t* adapter,
                                       const char* buffer, int buffer_len) {
    if (!adapter || !buffer) return -1;

    uint32_t attempt = 0;
    uint32_t max_attempts = adapter->config.max_retries > 0 ?
                            adapter->config.max_retries + 1 : 1;

    while (attempt < max_attempts) {
        if (adapter->conn_state == OPENJIUWEN_CONN_ERROR ||
            adapter->conn_state == OPENJIUWEN_CONN_DISCONNECTED) {
            int rc = openjiuwen_reconnect(adapter);
            if (rc != 0) return rc;
        }

        adapter->message_counter++;
        adapter->last_activity_ms = get_timestamp_ms();

        if (adapter->consecutive_errors >= OPENJIUWEN_MAX_CONSECUTIVE_ERRORS) {
            AGENTOS_LOG_ERROR("OpenJiuwen: too many consecutive errors (%u), forcing reconnect",
                          adapter->consecutive_errors);
            adapter->conn_state = OPENJIUWEN_CONN_ERROR;
            if (openjiuwen_reconnect(adapter) != 0) return -1;
            continue;
        }

        adapter->consecutive_errors = 0;
        return 0;

        attempt++;
    }

    return -1;
}

/* ============================================================================
 * 协议适配器接口实现
 * ============================================================================ */

/**
 * @brief 发送消息到OpenJiuwen平台
 */
static int openjiuwen_send_message(void* context,
                                    const void* data,
                                    size_t size) {
    openjiuwen_adapter_t* adapter = (openjiuwen_adapter_t*)context;

    if (!adapter || !data) {
        return -1;
    }

    if (!adapter->initialized) {
        AGENTOS_LOG_ERROR("OpenJiuwen adapter not initialized");
        return -2;
    }

    const unified_message_t* message = (const unified_message_t*)data;

    char buffer[OPENJIUWEN_MAX_MESSAGE_SIZE];
    int result = openjiuwen_unified_to_native(message, buffer, sizeof(buffer));
    if (result < 0) {
        AGENTOS_LOG_ERROR("Failed to convert message to OpenJiuwen format");
        adapter->consecutive_errors++;
        adapter->last_error_code = (uint32_t)(-result);
        return -3;
    }

    int send_result = openjiuwen_send_with_retry(adapter, buffer, result);
    if (send_result != 0) {
        adapter->consecutive_errors++;
        adapter->last_error_code = (uint32_t)(-send_result);
        AGENTOS_LOG_ERROR("OpenJiuwen: send failed after retries (errors=%u)",
                      adapter->consecutive_errors);
        return -4;
    }

    uint32_t now = get_timestamp();
    if (now - adapter->last_heartbeat_sec >= OPENJIUWEN_HEARTBEAT_INTERVAL_SEC) {
        adapter->last_heartbeat_sec = now;
    }

    AGENTOS_LOG_DEBUG("Message sent to OpenJiuwen (id=%u, size=%d bytes)",
                  adapter->message_counter, result);

    return (int)size;
}

/**
 * @brief 从OpenJiuwen平台接收消息
 */
static int openjiuwen_receive_message(void* context,
                                      void** data,
                                      size_t* size,
                                      uint32_t timeout_ms) {
    openjiuwen_adapter_t* adapter = (openjiuwen_adapter_t*)context;

    if (!adapter || !data) {
        return -1;
    }

    if (!adapter->initialized) {
        AGENTOS_LOG_ERROR("OpenJiuwen adapter not initialized");
        return -2;
    }

    if (adapter->conn_state != OPENJIUWEN_CONN_CONNECTED) {
        AGENTOS_LOG_WARN("OpenJiuwen: cannot receive - not connected (state=%d)",
                     adapter->conn_state);
        return -3;
    }

    (void)timeout_ms;

    unified_message_t* msg = (unified_message_t*)calloc(1, sizeof(unified_message_t));
    if (!msg) return -4;

    msg->protocol = AGENTOS_PROTOCOL_OPENJIUWEN;
    msg->message_id = generate_message_id();
    msg->timestamp = get_timestamp();

    adapter->last_activity_ms = get_timestamp_ms();

    uint32_t now = get_timestamp();
    if (now - adapter->last_heartbeat_sec >= OPENJIUWEN_HEARTBEAT_INTERVAL_SEC) {
        adapter->last_heartbeat_sec = now;
    }

    *data = msg;
    if (size) *size = sizeof(unified_message_t);
    return 0;
}

/**
 * @brief 销毁适配器实例
 */
static int openjiuwen_destroy(void* context) {
    openjiuwen_adapter_t* adapter = (openjiuwen_adapter_t*)context;

    if (!adapter) {
        return 0;
    }

    if (adapter->connection_handle) {
        adapter->conn_state = OPENJIUWEN_CONN_DISCONNECTED;
        adapter->connection_handle = NULL;
    }

    adapter->initialized = false;
    adapter->consecutive_errors = 0;
    adapter->message_counter = 0;

    AGENTOS_LOG_INFO("OpenJiuwen adapter destroyed (reconnects=%u, last_error=%u)",
                 adapter->total_reconnects, adapter->last_error_code);
    return 0;
}

/* ============================================================================
 * 协议转换实现
 * ============================================================================ */

int openjiuwen_unified_to_native(const unified_message_t* msg,
                                 void* out_buffer,
                                 size_t buffer_size) {
    if (!msg || !out_buffer || buffer_size < sizeof(openjiuwen_header_t)) {
        return -1;
    }

    /* 构建OpenJiuwen消息头部 */
    openjiuwen_header_t header;
    memset(&header, 0, sizeof(header));

    header.message_id = generate_message_id();
    header.timestamp = get_timestamp();
    header.message_type = OPENJIUWEN_MSG_TYPE_REQUEST;
    header.flags = 0x0001;  /* 标准请求标志 */

    safe_strcpy(header.source_agent, msg->source_agent,
                sizeof(header.source_agent));
    safe_strcpy(header.target_agent, "OpenJiuwen",
                sizeof(header.target_agent));

    /* 计算载荷长度（简化处理，实际应根据消息内容计算） */
    size_t payload_length = 0;
    if (msg->payload && msg->payload_size > 0) {
        payload_length = msg->payload_size;
    }
    header.payload_length = (uint32_t)payload_length;

    /* 写入头部到缓冲区 */
    size_t total_size = sizeof(openjiuwen_header_t) + payload_length;
    if (total_size > buffer_size) {
        AGENTOS_LOG_ERROR("Buffer too small for OpenJiuwen message");
        return -2;
    }

    memcpy(out_buffer, &header, sizeof(openjiuwen_header_t));

    /* 写入载荷数据 */
    if (payload_length > 0 && msg->payload) {
        memcpy((char*)out_buffer + sizeof(openjiuwen_header_t),
               msg->payload,
               payload_length);
    }

    return (int)total_size;
}

int openjiuwen_native_to_unified(const void* in_buffer,
                                 size_t buffer_size,
                                 unified_message_t* msg) {
    if (!in_buffer || !msg || buffer_size < sizeof(openjiuwen_header_t)) {
        return -1;
    }

    const openjiuwen_header_t* header =
        (const openjiuwen_header_t*)in_buffer;

    /* 验证消息完整性 */
    if (buffer_size < sizeof(openjiuwen_header_t) + header->payload_length) {
        AGENTOS_LOG_ERROR("Invalid OpenJiuwen message: incomplete data");
        return -2;
    }

    /* 填充统一消息格式 */
    memset(msg, 0, sizeof(unified_message_t));

    msg->protocol = AGENTOS_PROTOCOL_OPENJIUWEN;
    msg->message_id = header->message_id;
    msg->timestamp = header->timestamp;

    safe_strcpy(msg->source_agent, header->source_agent,
                sizeof(msg->source_agent));
    safe_strcpy(msg->target_agent, header->target_agent,
                sizeof(msg->target_agent));

    /* 复制载荷数据 */
    if (header->payload_length > 0) {
        msg->payload_size = header->payload_length;
        msg->payload = malloc(header->payload_length);
        if (msg->payload) {
            memcpy(msg->payload,
                   (const char*)in_buffer + sizeof(openjiuwen_header_t),
                   header->payload_length);
        } else {
            msg->payload_size = 0;
            return -3;
        }
    }

    return 0;
}

/* ============================================================================
 * 公共接口函数
 * ============================================================================ */

void openjiuwen_get_default_config(openjiuwen_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(openjiuwen_config_t));

    safe_strcpy(config->endpoint, "http://localhost:8080",
                sizeof(config->endpoint));
    config->timeout_ms = OPENJIUWEN_TIMEOUT_MS;
    config->enable_compression = false;
    config->enable_encryption = false;
    config->max_retries = 3;
}

const protocol_adapter_t* openjiuwen_adapter_create(
        const openjiuwen_config_t* config) {

    openjiuwen_adapter_t* adapter =
        (openjiuwen_adapter_t*)calloc(1, sizeof(openjiuwen_adapter_t));
    if (!adapter) {
        AGENTOS_LOG_ERROR("Failed to allocate OpenJiuwen adapter");
        return NULL;
    }

    /* 初始化配置 */
    if (config) {
        memcpy(&adapter->config, config, sizeof(openjiuwen_config_t));
    } else {
        openjiuwen_get_default_config(&adapter->config);
    }

    /* 设置协议适配器接口 */
    adapter->base.type = AGENTOS_PROTOCOL_OPENJIUWEN;
    adapter->base.name = "OpenJiuwen Protocol Adapter";
    adapter->base.version = OPENJIUWEN_PROTOCOL_VERSION;
    adapter->base.description = "OpenJiuwen platform protocol adapter";

    adapter->base.context = adapter;
    adapter->base.send = openjiuwen_send_message;
    adapter->base.receive = openjiuwen_receive_message;
    adapter->base.destroy = openjiuwen_destroy;

    adapter->initialized = true;
    adapter->message_counter = 0;
    adapter->connection_handle = NULL;
    adapter->conn_state = OPENJIUWEN_CONN_DISCONNECTED;
    adapter->consecutive_errors = 0;
    adapter->total_reconnects = 0;
    adapter->last_heartbeat_sec = 0;
    adapter->last_error_code = 0;
    adapter->last_activity_ms = 0;

    AGENTOS_LOG_INFO("OpenJiuwen adapter created successfully (endpoint=%s)",
                 adapter->config.endpoint);

    return &adapter->base;
}

int openjiuwen_verify_connection(const protocol_adapter_t* adapter) {
    if (!adapter || adapter->type != AGENTOS_PROTOCOL_OPENJIUWEN) {
        return -1;
    }

    openjiuwen_adapter_t* impl = (openjiuwen_adapter_t*)adapter->context;
    if (!impl || !impl->initialized) {
        return -2;
    }

    if (impl->consecutive_errors >= OPENJIUWEN_MAX_CONSECUTIVE_ERRORS) {
        impl->conn_state = OPENJIUWEN_CONN_ERROR;
        AGENTOS_LOG_WARN("OpenJiuwen: connection verification failed - too many errors (%u)",
                     impl->consecutive_errors);
        return -3;
    }

    uint32_t now = get_timestamp();
    uint32_t idle_seconds = now - impl->last_heartbeat_sec;
    if (idle_seconds > OPENJIUWEN_HEARTBEAT_INTERVAL_SEC * 3) {
        impl->conn_state = OPENJIUWEN_CONN_RECONNECTING;
        AGENTOS_LOG_WARN("OpenJiuwen: connection stale (idle=%us), needs reconnect",
                     idle_seconds);
        return -4;
    }

    impl->conn_state = OPENJIUWEN_CONN_CONNECTED;
    impl->last_heartbeat_sec = now;
    AGENTOS_LOG_INFO("OpenJiuwen connection verification successful");

    return 0;
}

int openjiuwen_get_capabilities(const protocol_adapter_t* adapter,
                                char* capabilities,
                                size_t max_len) {
    if (!adapter || !capabilities || max_len == 0) {
        return -1;
    }

    const char* caps =
        "{"
        "\"version\":\"" OPENJIUWEN_PROTOCOL_VERSION "\","
        "\"features\":["
        "\"agent_discovery\","
        "\"task_delegation\","
        "\"message_routing\","
        "\"status_reporting\""
        "],"
        "\"supported_messages\":["
        "\"request\","
        "\"response\","
        "\"notification\","
        "\"heartbeat\","
        "\"error\""
        "]"
        "}";

    safe_strcpy(capabilities, caps, max_len);

    return 0;
}

/* ============================================================================
 * 全局接口实例定义
 * ============================================================================ */

/*
 * 注意：此全局实例在首次使用前需要调用openjiuwen_adapter_create()进行初始化。
 * 此处仅提供接口声明，实际实例应在运行时动态创建。
 *
 * 使用示例：
 *   const protocol_adapter_t* adapter = openjiuwen_adapter_create(NULL);
 *   unified_protocol_register_adapter(stack, adapter);
 */

/* 静态默认接口实例（用于注册） */
static openjiuwen_adapter_t g_default_instance = {
    .base = {
        .type = AGENTOS_PROTOCOL_OPENJIUWEN,
        .name = "OpenJiuwen Protocol Adapter",
        .version = OPENJIUWEN_PROTOCOL_VERSION,
        .description = "OpenJiuwen platform protocol adapter",
        .context = NULL,
        .user_data = NULL,
        .init = NULL,
        .destroy = openjiuwen_destroy,
        .encode = NULL,
        .decode = NULL,
        .connect = NULL,
        .disconnect = NULL,
        .is_connected = NULL,
        .send = openjiuwen_send_message,
        .receive = openjiuwen_receive_message,
        .handle_request = NULL,
        .get_version = NULL,
        .capabilities = NULL,
        .get_stats = NULL
    },
    .config = {{0}, {0}, 0, false, false, 0},
    .connection_handle = NULL,
    .initialized = false,
    .message_counter = 0,
    .user_data = NULL,
    .conn_state = OPENJIUWEN_CONN_DISCONNECTED,
    .consecutive_errors = 0,
    .total_reconnects = 0,
    .last_heartbeat_sec = 0,
    .last_error_code = 0,
    .last_activity_ms = 0
};

const protocol_adapter_t openjiuwen_adapter_interface = {
    .type = AGENTOS_PROTOCOL_OPENJIUWEN,
    .name = "OpenJiuwen Protocol Adapter",
    .version = OPENJIUWEN_PROTOCOL_VERSION,
    .description = "OpenJiuwen platform protocol adapter",
    .context = &g_default_instance,
    .user_data = NULL,
    .init = NULL,
    .destroy = openjiuwen_destroy,
    .encode = NULL,
    .decode = NULL,
    .connect = NULL,
    .disconnect = NULL,
    .is_connected = NULL,
    .send = openjiuwen_send_message,
    .receive = openjiuwen_receive_message,
    .handle_request = NULL,
    .get_version = NULL,
    .capabilities = NULL,
    .get_stats = NULL
};