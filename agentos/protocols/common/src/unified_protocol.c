// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file unified_protocol.c
 * @brief Unified Protocol Implementation
 * 
 * 统一协议接口实现，提供协议无关的消息处理和路由功能。
 */

#include "unified_protocol.h"
#include "../../daemon/common/include/safe_string_utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ============================================================================
// 内部数据结构
// ============================================================================

typedef struct protocol_adapter_node_s {
    const protocol_adapter_t* adapter;
    void* context;
    struct protocol_adapter_node_s* next;
} protocol_adapter_node_t;

struct protocol_stack_s {
    protocol_stack_config_t config;
    protocol_adapter_node_t* adapters;
    size_t adapter_count;
    bool initialized;
    
    // 回调函数
    void (*message_callback)(const unified_message_t* message, void* user_data);
    void* callback_user_data;
    
    // 统计信息
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
};

// ============================================================================
// 静态函数声明
// ============================================================================

static protocol_adapter_node_t* find_adapter_node(protocol_stack_handle_t handle, protocol_type_t type);
static int validate_message(const unified_message_t* message);
static uint64_t get_current_timestamp(void);

// ============================================================================
// 核心API实现
// ============================================================================

protocol_stack_handle_t protocol_stack_create(const protocol_stack_config_t* config)
{
    if (!config || !config->name) {
        return NULL;
    }
    
    struct protocol_stack_s* stack = (struct protocol_stack_s*)calloc(1, sizeof(struct protocol_stack_s));
    if (!stack) {
        return NULL;
    }
    
    // 复制配置
    stack->config = *config;
    if (config->name) {
        size_t name_len = strlen(config->name) + 1;
        char* name_copy = (char*)malloc(name_len);
        if (name_copy) {
            safe_strcpy(name_copy, config->name, name_len);
            stack->config.name = name_copy;
        }
    }
    
    stack->adapters = NULL;
    stack->adapter_count = 0;
    stack->initialized = true;
    
    stack->messages_sent = 0;
    stack->messages_received = 0;
    stack->bytes_sent = 0;
    stack->bytes_received = 0;
    
    return stack;
}

void protocol_stack_destroy(protocol_stack_handle_t handle)
{
    if (!handle) return;
    
    struct protocol_stack_s* stack = (struct protocol_stack_s*)handle;
    
    // 销毁所有适配器
    protocol_adapter_node_t* node = stack->adapters;
    while (node) {
        protocol_adapter_node_t* next = node->next;
        if (node->adapter && node->adapter->destroy) {
            node->adapter->destroy(node->context);
        }
        free(node);
        node = next;
    }
    
    // 释放配置资源
    if (stack->config.name) {
        free((void*)stack->config.name);
    }
    if (stack->config.custom_config) {
        free(stack->config.custom_config);
    }
    
    free(stack);
}

int protocol_stack_register_adapter(protocol_stack_handle_t handle, const protocol_adapter_t* adapter)
{
    if (!handle || !adapter) {
        return -1;
    }
    
    struct protocol_stack_s* stack = (struct protocol_stack_s*)handle;
    
    // 检查是否已存在相同类型的适配器
    protocol_adapter_node_t* existing = find_adapter_node(handle, adapter->type);
    if (existing) {
        // 已存在，替换
        if (existing->adapter && existing->adapter->destroy) {
            existing->adapter->destroy(existing->context);
        }
        existing->adapter = adapter;
        if (adapter->init) {
            adapter->init(existing->context);
        }
        return 0;
    }
    
    // 创建新节点
    protocol_adapter_node_t* node = (protocol_adapter_node_t*)malloc(sizeof(protocol_adapter_node_t));
    if (!node) {
        return -1;
    }
    
    node->adapter = adapter;
    node->context = NULL;
    
    // 初始化适配器
    if (adapter->init) {
        int result = adapter->init(node->context);
        if (result != 0) {
            free(node);
            return result;
        }
    }
    
    // 添加到链表
    node->next = stack->adapters;
    stack->adapters = node;
    stack->adapter_count++;
    
    return 0;
}

int protocol_stack_send(protocol_stack_handle_t handle, const unified_message_t* message)
{
    if (!handle || !message) {
        return -1;
    }
    
    struct protocol_stack_s* stack = (struct protocol_stack_s*)handle;
    
    // 验证消息
    int valid = validate_message(message);
    if (valid != 0) {
        return valid;
    }
    
    // 查找对应的适配器
    protocol_adapter_node_t* adapter_node = find_adapter_node(handle, message->protocol);
    if (!adapter_node || !adapter_node->adapter) {
        return -1; // 未找到适配器
    }
    
    const protocol_adapter_t* adapter = adapter_node->adapter;
    
    // 编码消息
    void* encoded_data = NULL;
    size_t encoded_size = 0;
    if (adapter->encode) {
        int result = adapter->encode(adapter_node->context, message, &encoded_data, &encoded_size);
        if (result != 0) {
            return result;
        }
    } else {
        // 如果没有编码器，直接使用原始数据
        encoded_data = (void*)message->payload;
        encoded_size = message->payload_size;
    }
    
    // TODO: 实际发送逻辑（需要具体协议实现）
    // 这里只是存根实现
    
    // 更新统计信息
    stack->messages_sent++;
    stack->bytes_sent += encoded_size;
    
    // 清理编码数据
    if (encoded_data != message->payload && encoded_data) {
        free(encoded_data);
    }
    
    return 0;
}

int protocol_stack_receive(protocol_stack_handle_t handle, unified_message_t* message, uint32_t timeout_ms)
{
    if (!handle || !message) {
        return -1;
    }
    
    struct protocol_stack_s* stack = (struct protocol_stack_s*)handle;
    
    // TODO: 实际接收逻辑（需要具体协议实现）
    // 这里只是存根实现
    
    // 模拟接收
    memset(message, 0, sizeof(unified_message_t));
    message->message_id = 0;
    message->protocol = PROTOCOL_HTTP;
    message->direction = DIRECTION_RESPONSE;
    message->endpoint = "/api/test";
    message->payload = NULL;
    message->payload_size = 0;
    message->timestamp = get_current_timestamp();
    
    // 更新统计信息
    stack->messages_received++;
    
    return 0;
}

int protocol_stack_set_callback(protocol_stack_handle_t handle, 
                               void (*callback)(const unified_message_t* message, void* user_data),
                               void* user_data)
{
    if (!handle) {
        return -1;
    }
    
    struct protocol_stack_s* stack = (struct protocol_stack_s*)handle;
    stack->message_callback = callback;
    stack->callback_user_data = user_data;
    
    return 0;
}

int protocol_stack_get_stats(protocol_stack_handle_t handle, void* stats)
{
    if (!handle || !stats) {
        return -1;
    }
    
    struct protocol_stack_s* stack = (struct protocol_stack_s*)handle;
    
    // TODO: 填充统计信息结构
    // 这里只是存根实现
    
    return 0;
}

// ============================================================================
// 工具函数实现
// ============================================================================

unified_message_t unified_message_create(protocol_type_t protocol,
                                        message_direction_t direction,
                                        const char* endpoint,
                                        const void* payload,
                                        size_t payload_size)
{
    unified_message_t message;
    memset(&message, 0, sizeof(message));
    
    static uint64_t next_message_id = 1;
    
    message.message_id = next_message_id++;
    message.protocol = protocol;
    message.direction = direction;
    message.endpoint = endpoint;
    message.payload = payload;
    message.payload_size = payload_size;
    message.timestamp = get_current_timestamp();
    
    return message;
}

void unified_message_destroy(unified_message_t* message)
{
    if (!message) return;
    
    // 注意：这里不释放payload，由调用者管理
    memset(message, 0, sizeof(unified_message_t));
}

const char* protocol_type_to_string(protocol_type_t type)
{
    static const char* names[] = {
        "HTTP",
        "WebSocket",
        "gRPC",
        "MQTT",
        "AMQP",
        "Raw TCP",
        "Raw UDP",
        "Custom"
    };
    
    if (type < 0 || type > PROTOCOL_CUSTOM) {
        return "Unknown";
    }
    
    return names[type];
}

protocol_type_t protocol_type_from_string(const char* str)
{
    if (!str) return PROTOCOL_CUSTOM;
    
    const char* names[] = {"http", "websocket", "grpc", "mqtt", "amqp", "tcp", "udp"};
    protocol_type_t types[] = {
        PROTOCOL_HTTP,
        PROTOCOL_WEBSOCKET,
        PROTOCOL_GRPC,
        PROTOCOL_MQTT,
        PROTOCOL_AMQP,
        PROTOCOL_RAW_TCP,
        PROTOCOL_RAW_UDP
    };
    
    for (int i = 0; i < 7; i++) {
        if (strcasecmp(str, names[i]) == 0) {
            return types[i];
        }
    }
    
    return PROTOCOL_CUSTOM;
}

// ============================================================================
// 静态函数实现
// ============================================================================

static protocol_adapter_node_t* find_adapter_node(protocol_stack_handle_t handle, protocol_type_t type)
{
    struct protocol_stack_s* stack = (struct protocol_stack_s*)handle;
    protocol_adapter_node_t* node = stack->adapters;
    
    while (node) {
        if (node->adapter && node->adapter->type == type) {
            return node;
        }
        node = node->next;
    }
    
    return NULL;
}

static int validate_message(const unified_message_t* message)
{
    if (!message) return -1;
    
    if (message->protocol < PROTOCOL_HTTP || message->protocol > PROTOCOL_CUSTOM) {
        return -1;
    }
    
    if (message->direction < DIRECTION_REQUEST || message->direction > DIRECTION_NOTIFICATION) {
        return -1;
    }
    
    if (message->endpoint && strlen(message->endpoint) > 1024) {
        return -1;
    }
    
    return 0;
}

static uint64_t get_current_timestamp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}