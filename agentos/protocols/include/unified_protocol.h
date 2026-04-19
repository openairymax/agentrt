/*
 * AgentOS Unified Protocol - 统一协议接口
 * 
 * 本文件定义AgentOS统一协议系统的核心接口，提供对多种
 * 通信协议（JSON-RPC、MCP、A2A、OpenAI、OpenJiuwen）的
 * 统一抽象层。
 *
 * 原位置: agentos/include/agentos/unified_protocol.h
 * 迁移至: agentos/protocols/include/ (2026-04-19 include/整合重构)
 */

#ifndef AGENTOS_UNIFIED_PROTOCOL_H
#define AGENTOS_UNIFIED_PROTOCOL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 支持的协议类型
 */
typedef enum {
    AGENTOS_PROTOCOL_JSON_RPC = 0,
    AGENTOS_PROTOCOL_MCP,
    AGENTOS_PROTOCOL_A2A,
    AGENTOS_PROTOCOL_OPENAI,
    AGENTOS_PROTOCOL_OPENJIUWEN,
    AGENTOS_PROTOCOL_COUNT
} agentos_protocol_type_t;

/**
 * @brief 协议适配器句柄
 */
typedef struct protocol_adapter_s* protocol_adapter_t;

/**
 * @brief 消息结构
 */
typedef struct {
    const void* data;
    size_t len;
    agentos_protocol_type_t source_protocol;
} agentos_message_t;

/**
 * @brief 创建指定类型的协议适配器
 */
int protocol_adapter_create(agentos_protocol_type_t type, protocol_adapter_t* adapter);

/**
 * @brief 销毁协议适配器
 */
void protocol_adapter_destroy(protocol_adapter_t adapter);

/**
 * @brief 通过适配器发送消息
 */
int protocol_adapter_send(protocol_adapter_t adapter, const agentos_message_t* msg);

/**
 * @brief 通过适配器接收消息
 */
int protocol_adapter_recv(protocol_adapter_t adapter, agentos_message_t* msg, size_t max_len);

/**
 * @brief 获取协议类型名称
 */
const char* protocol_type_name(agentos_protocol_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UNIFIED_PROTOCOL_H */
