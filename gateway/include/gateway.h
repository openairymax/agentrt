/**
 * @file gateway.h
 * @brief AgentOS gateway Runtime - 网关层公共接口
 * 
 * gateway 模块是 AgentOS 的统一通信网关层，提供：
 * - HTTP/REST 网关（JSON-RPC 2.0）
 * - WebSocket 网关（实时双向通信）
 * - Stdio 网关（本地进程通信）
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 1.1.0
 */

#ifndef AGENTOS_GATEWAY_H
#define AGENTOS_GATEWAY_H

/* API 版本声明 (MAJOR.MINOR.PATCH) */
#define GATEWAY_API_VERSION_MAJOR 1
#define GATEWAY_API_VERSION_MINOR 1
#define GATEWAY_API_VERSION_PATCH 0

/* ABI 兼容性声明：在相同 MAJOR 版本内保证 ABI 兼容 */
#define GATEWAY_ABI_VERSION 1

#include <stddef.h>
#include <stdint.h>

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief gateway 服务器不透明句柄
 * 
 * 调用者不应直接访问此结构体的内部成员，
 * 应通过本头文件定义的 API 进行操作。
 */
typedef struct gateway_server gateway_server_t;

/**
 * @brief gateway 配置结构
 * 
 * 所有字符串指针在服务器运行期间必须保持有效。
 * 若字段为 NULL 或 0，将使用默认值。
 */
typedef struct gateway_config {
    const char* http_host;            /**< HTTP 监听地址，默认 "127.0.0.1" */
    uint16_t    http_port;            /**< HTTP 端口，默认 18789 */
    const char* ws_host;              /**< WebSocket 监听地址，默认 "127.0.0.1" */
    uint16_t    ws_port;              /**< WebSocket 端口，默认 18790 */
    int         enable_stdio;         /**< 是否启用 stdio 网关，默认 1 */
    uint32_t    max_sessions;         /**< 最大并发会话数，默认 1000 */
    uint32_t    session_timeout_sec;  /**< 会话闲置超时（秒），默认 3600 */
    uint32_t    health_interval_sec;  /**< 健康检查间隔（秒），默认 30 */
    const char* metrics_path;         /**< 指标导出路径，默认 "/metrics" */
    uint32_t    max_request_size;     /**< 最大请求体大小（字节），默认 1MB */
    uint32_t    request_timeout_ms;   /**< 请求超时（毫秒），默认 30000 */
} gateway_config_t;

/**
 * @brief 创建 gateway 服务器实例
 * 
 * @param[in] manager 配置参数，若为 NULL 则使用默认值
 * @param[out] out_server 输出服务器句柄
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_ENOMEM 内存不足
 * @return AGENTOS_EBUSY 服务器已存在（单例模式）
 * 
 * @note 此函数仅创建实例，不启动服务
 * @note 调用者需通过 gateway_server_destroy() 释放资源
 * 
 * @ownership out_server 所有权转移给调用者
 * @threadsafe 否
 */
AGENTOS_API agentos_error_t gateway_server_create(
    const gateway_config_t* manager,
    gateway_server_t** out_server);

/**
 * @brief 启动 gateway 服务器（阻塞调用）
 * 
 * 此函数会阻塞当前线程，直到调用 gateway_server_stop() 或
 * 收到 SIGINT/SIGTERM 信号。
 * 
 * @param[in] server 服务器句柄
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_EBUSY 服务器已在运行
 * 
 * @threadsafe 否（同一服务器实例不可并发调用）
 */
AGENTOS_API agentos_error_t gateway_server_start(gateway_server_t* server);

/**
 * @brief 停止 gateway 服务器（异步）
 * 
 * 此函数可从信号处理程序或其他线程安全调用。
 * 它会触发 gateway_server_start() 返回。
 * 
 * @param[in] server 服务器句柄
 * 
 * @threadsafe 是
 */
AGENTOS_API void gateway_server_stop(gateway_server_t* server);

/**
 * @brief 等待服务器退出（阻塞）
 * 
 * 若服务器已通过 gateway_server_start() 启动，此函数会阻塞
 * 直到服务器停止。若服务器未启动，立即返回。
 * 
 * @param[in] server 服务器句柄
 * @param[in] timeout_ms 超时时间（毫秒），0 表示无限等待
 * @return AGENTOS_SUCCESS 服务器已停止
 * @return AGENTOS_ETIMEDOUT 超时
 * @return AGENTOS_EINVAL 参数无效
 * 
 * @threadsafe 是
 */
AGENTOS_API agentos_error_t gateway_server_wait(
    gateway_server_t* server,
    uint32_t timeout_ms);

/**
 * @brief 销毁 gateway 服务器实例
 * 
 * 若服务器仍在运行，会先停止再销毁。
 * 销毁后句柄不再有效。
 * 
 * @param[in] server 服务器句柄
 * 
 * @ownership 释放 server 及其所有内部资源
 * @threadsafe 否
 */
AGENTOS_API void gateway_server_destroy(gateway_server_t* server);

/**
 * @brief 获取服务器健康状态（JSON 格式）
 * 
 * @param[in] server 服务器句柄
 * @param[out] out_json 输出 JSON 字符串（需调用者 free）
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * 
 * @ownership out_json 所有权转移给调用者
 * @threadsafe 是
 */
AGENTOS_API agentos_error_t gateway_server_get_health(
    gateway_server_t* server,
    char** out_json);

/**
 * @brief 获取服务器指标（Prometheus 格式）
 * 
 * @param[in] server 服务器句柄
 * @param[out] out_metrics 输出指标字符串（需调用者 free）
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * 
 * @ownership out_metrics 所有权转移给调用者
 * @threadsafe 是
 */
AGENTOS_API agentos_error_t gateway_server_get_metrics(
    gateway_server_t* server,
    char** out_metrics);

/**
 * @brief 获取服务器统计信息
 * 
 * @param[in] server 服务器句柄
 * @param[out] out_stats 输出统计 JSON 字符串（需调用者 free）
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * 
 * @ownership out_stats 所有权转移给调用者
 * @threadsafe 是
 */
AGENTOS_API agentos_error_t gateway_server_get_stats(
    gateway_server_t* server,
    char** out_stats);

/* ========== 便捷 API（单例模式） ========== */

/**
 * @brief 启动 gateway 服务器（单例模式，阻塞调用）
 * 
 * 此函数使用单例模式，全局只允许一个服务器实例。
 * 内部会创建、启动、等待、销毁服务器。
 * 
 * @param[in] manager 配置参数，若为 NULL 则使用默认值
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_ENOMEM 内存不足
 * 
 * @note 此函数会阻塞直到收到停止信号
 * @threadsafe 否
 */
AGENTOS_API agentos_error_t gateway_run(const gateway_config_t* manager);

/**
 * @brief 请求停止单例服务器
 * 
 * 可从信号处理程序或其他线程安全调用。
 * 会触发 gateway_run() 返回。
 * 
 * @threadsafe 是
 */
AGENTOS_API void gateway_request_stop(void);

/* ========== 版本查询 ========== */

/**
 * @brief 获取 API 版本字符串
 * @return 版本字符串，如 "1.1.0"
 */
AGENTOS_API const char* gateway_get_version(void);

/**
 * @brief 获取 ABI 版本号
 * @return ABI 版本号
 */
AGENTOS_API uint32_t gateway_get_abi_version(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_H */
