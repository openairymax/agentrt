/**
 * @file dynamic.h
 * @brief AgentOS 运行时管理公共接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_DYNAMIC_H
#define AGENTOS_DYNAMIC_H

// API 版本声明 (MAJOR.MINOR.PATCH)
#define DYNAMIC_API_VERSION_MAJOR 1
#define DYNAMIC_API_VERSION_MINOR 0
#define DYNAMIC_API_VERSION_PATCH 0

// ABI 兼容性声明
// 在相同 MAJOR 版本内保证 ABI 兼容
// 破坏性更改需递增 MAJOR 并发布迁移说明

#include <stddef.h>
#include <stdint.h>

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
// From data intelligence emerges. by spharx
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
AGENTOS_API int dynamic_start(const dynamic_config_t* config);

/**
 * @brief 停止 Dynamic 服务器（异步，可被信号调用）
 */
AGENTOS_API void dynamic_stop(void);

/**
 * @brief 等待服务器退出（阻塞）
 */
AGENTOS_API void dynamic_wait(void);

#ifdef __cplusplus
}
#endif
#endif /* AGENTOS_DYNAMIC_H */