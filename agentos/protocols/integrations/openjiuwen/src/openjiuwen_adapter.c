// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file openjiuwen_adapter.c
 * @brief OpenJiuwen Protocol Adapter Implementation
 *
 * 实现与OpenJiuwen平台的协议兼容层，支持消息格式转换和互操作。
 */

#include "openjiuwen_adapter.h"
#include "common/include/safe_string_utils.h"

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

/* ============================================================================
 * 协议适配器接口实现
 * ============================================================================ */

/**
 * @brief 发送消息到OpenJiuwen平台
 */
static int openjiuwen_send_message(void* context,
                                    const unified_message_t* message) {
    openjiuwen_adapter_t* adapter = (openjiuwen_adapter_t*)context;

    if (!adapter || !message) {
        return -1;
    }

    if (!adapter->initialized) {
        SVC_LOG_ERROR("OpenJiuwen adapter not initialized");
        return -2;
    }

    /* 将unified_message转换为OpenJiuwen原生格式 */
    char buffer[OPENJIUWEN_MAX_MESSAGE_SIZE];
    int result = openjiuwen_unified_to_native(message, buffer, sizeof(buffer));
    if (result < 0) {
        SVC_LOG_ERROR("Failed to convert message to OpenJiuwen format");
        return -3;
    }

    /* TODO: 实际发送逻辑（需要网络库支持） */
    /* 这里模拟发送成功 */
    adapter->message_counter++;

    SVC_LOG_DEBUG("Message sent to OpenJiuwen (id=%u, size=%d bytes)",
                  adapter->message_counter, result);

    return 0;
}

/**
 * @brief 从OpenJiuwen平台接收消息
 */
static int openjiuwen_receive_message(void* context,
                                      unified_message_t* message) {
    openjiuwen_adapter_t* adapter = (openjiuwen_adapter_t*)context;

    if (!adapter || !message) {
        return -1;
    }

    if (!adapter->initialized) {
        SVC_LOG_ERROR("OpenJiuwen adapter not initialized");
        return -2;
    }

    /* TODO: 实际接收逻辑（需要网络库支持） */
    /* 这里模拟接收一个空消息 */
    memset(message, 0, sizeof(unified_message_t));
    message->protocol_type = UNIFIED_PROTOCOL_OPENJIUWEN;

    return 0;
}

/**
 * @brief 销毁适配器实例
 */
static void openjiuwen_destroy(void* context) {
    openjiuwen_adapter_t* adapter = (openjiuwen_adapter_t*)context;

    if (!adapter) {
        return;
    }

    if (adapter->connection_handle) {
        /* TODO: 关闭连接 */
        adapter->connection_handle = NULL;
    }

    adapter->initialized = false;

    SVC_LOG_INFO("OpenJiuwen adapter destroyed");
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

    safe_strcpy(header.source_agent, msg->source_agent ? msg->source_agent : "AgentOS",
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
        SVC_LOG_ERROR("Buffer too small for OpenJiuwen message");
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
        SVC_LOG_ERROR("Invalid OpenJiuwen message: incomplete data");
        return -2;
    }

    /* 填充统一消息格式 */
    memset(msg, 0, sizeof(unified_message_t));

    msg->protocol_type = UNIFIED_PROTOCOL_OPENJIUWEN;
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
        SVC_LOG_ERROR("Failed to allocate OpenJiuwen adapter");
        return NULL;
    }

    /* 初始化配置 */
    if (config) {
        memcpy(&adapter->config, config, sizeof(openjiuwen_config_t));
    } else {
        openjiuwen_get_default_config(&adapter->config);
    }

    /* 设置协议适配器接口 */
    adapter->base.protocol_type = UNIFIED_PROTOCOL_OPENJIUWEN;
    safe_strcpy(adapter->base.name, "OpenJiuwen Protocol Adapter",
                sizeof(adapter->base.name));
    safe_strcpy(adapter->base.version, OPENJIUWEN_PROTOCOL_VERSION,
                sizeof(adapter->base.version));

    adapter->base.context = adapter;
    adapter->base.send_message = openjiuwen_send_message;
    adapter->base.receive_message = openjiuwen_receive_message;
    adapter->base.destroy = openjiuwen_destroy;

    adapter->initialized = true;
    adapter->message_counter = 0;
    adapter->connection_handle = NULL;

    SVC_LOG_INFO("OpenJiuwen adapter created successfully (endpoint=%s)",
                 adapter->config.endpoint);

    return &adapter->base;
}

int openjiuwen_verify_connection(const protocol_adapter_t* adapter) {
    if (!adapter || adapter->protocol_type != UNIFIED_PROTOCOL_OPENJIUWEN) {
        return -1;
    }

    /* TODO: 实现实际的连接验证逻辑 */
    /* 这里模拟验证成功 */
    SVC_LOG_INFO("OpenJiuwen connection verification successful");

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
        .protocol_type = UNIFIED_PROTOCOL_OPENJIUWEN,
        .name = "OpenJiuwen Protocol Adapter",
        .version = OPENJIUWEN_PROTOCOL_VERSION,
        .context = NULL,  /* 需要通过create()设置 */
        .send_message = NULL,  /* 需要通过create()设置 */
        .receive_message = NULL,  /* 需要通过create()设置 */
        .destroy = NULL  /* 需要通过create()设置 */
    },
    .config = {0},
    .connection_handle = NULL,
    .initialized = false,
    .message_counter = 0,
    .user_data = NULL
};

const protocol_adapter_t openjiuwen_adapter_interface = {
    .protocol_type = UNIFIED_PROTOCOL_OPENJIUWEN,
    .name = "OpenJiuwen Protocol Adapter",
    .version = OPENJIUWEN_PROTOCOL_VERSION,
    .context = &g_default_instance,
    .send_message = openjiuwen_send_message,
    .receive_message = openjiuwen_receive_message,
    .destroy = openjiuwen_destroy
};