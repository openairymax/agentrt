/**
 * @file dynamic.h
 * @brief AgentOS 运行时管理公共接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_DYNAMIC_H
#define AGENTOS_DYNAMIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dynamic 服务器不透明句柄
 */
typedef struct dynamic_server dynamic_server_t;

/**
 * @brief Dynamic 配置结构
 */
typedef struct dynamic_config {
    const char* http_host;          /**< HTTP 监听地址，如 "0.0.0.0" */
    uint16_t     http_port;          /**< HTTP 端口 */
    const char* ws_host;            /**< WebSocket 监听地址 */
    uint16_t     ws_port;            /**< WebSocket 端口 */
    int         enable_stdio;       /**< 是否启用 stdio 网关（1 启用） */
    uint32_t    max_sessions;       /**< 最大并发会话数 */
    uint32_t    session_timeout_sec; /**< 会话闲置超时（秒） */
    uint32_t    health_interval_sec; /**< 健康检查间隔（秒） */
    const char* metrics_path;       /**< 指标导出路径（如 "/metrics"） */
} dynamic_config_t;

/**
 * @brief 启动 Dynamic 服务器（阻塞调用，直到停止）
 * @param config 配置（若为 NULL 则使用默认值）
 * @return 0 成功，-1 失败
 */
int dynamic_start(const dynamic_config_t* config);

/**
 * @brief 停止 Dynamic 服务器（异步，可被信号调用）
 */
void dynamic_stop(void);

/**
 * @brief 等待服务器退出（阻塞）
 */
void dynamic_wait(void);

#ifdef __cplusplus
}
#endif
#endif /* AGENTOS_DYNAMIC_H */