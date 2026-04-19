/*
 * AgentOS Protocol Router - 协议路由器接口
 * 
 * 本文件定义协议路由器的统一接口，用于在多种通信协议之间进行
 * 智能路由和消息转发。
 *
 * 原位置: agentos/include/agentos/protocol_router.h
 * 迁移至: agentos/protocols/include/ (2026-04-19 include/整合重构)
 */

#ifndef AGENTOS_PROTOCOL_ROUTER_H
#define AGENTOS_PROTOCOL_ROUTER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 协议路由器句柄
 */
typedef struct protocol_router_s* protocol_router_t;

/**
 * @brief 路由结果
 */
typedef enum {
    PROTOCOL_ROUTE_OK = 0,           /* 路由成功 */
    PROTOCOL_ROUTE_ERR_INVALID_ARG,  /* 无效参数 */
    PROTOCOL_ROUTE_ERR_NOT_FOUND,     /* 未找到目标 */
    PROTOCOL_ROUTE_ERR_NO_HANDLER,   /* 无可用处理器 */
} protocol_route_result_t;

/**
 * @brief 创建协议路由器
 */
protocol_route_result_t protocol_router_create(protocol_router_t* router);

/**
 * @brief 销毁协议路由器
 */
void protocol_router_destroy(protocol_router_t router);

/**
 * @brief 注册协议处理器
 */
protocol_route_result_t protocol_router_register_handler(
    protocol_router_t router,
    const char* protocol_name,
    void* handler_context);

/**
 * @brief 路由消息到指定协议
 */
protocol_route_result_t protocol_router_route(
    protocol_router_t router,
    const char* target_protocol,
    const void* message,
    size_t message_len);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PROTOCOL_ROUTER_H */
