/**
 * @file ws_gateway.h
 * @brief WebSocket 网关接口
 */
#ifndef BASERUNTIME_WS_GATEWAY_H
#define BASERUNTIME_WS_GATEWAY_H

#include "gateway.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建 WebSocket 网关实例
 * @param host 监听地址（如 "0.0.0.0"）
 * @param port 监听端口
 * @return 网关句柄，失败返回 NULL
 */
gateway_t* ws_gateway_create(const char* host, uint16_t port);

#ifdef __cplusplus
}
#endif

#endif /* BASERUNTIME_WS_GATEWAY_H */