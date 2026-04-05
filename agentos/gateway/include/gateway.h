/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway.h
 * @brief AgentOS 网关统一公共接口
 *
 * 网关层只负责协议转换，将外部请求转换为系统调用。
 * 所有业务逻辑通过 agentos/atoms/syscall 接口调用。
 *
 * 架构定位：
 *   agentos/daemon/gateway_d/ --> agentos/gateway/ --> agentos/atoms/syscall/
 *                      ^
 *                 协议转换层
 *
 * 设计原则（符合 ARCHITECTURAL_PRINCIPLES.md）：
 *   K-1 内核极简：网关只做协议翻译，零业务逻辑
 *   K-2 接口契约化：所有 API 带完整 Doxygen 契约
 *   S-2 层次分解：严格分层，不跳层访问
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_GATEWAY_H
#define AGENTOS_GATEWAY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 错误码 ========== */

/**
 * @brief 网关专用错误码（扩展 AgentOS 标准错误码）
 *
 * @note 网关层 API 同时返回 agentos_error_t 和 gateway_error_t。
 *       gateway_error_t 用于网关特有的错误场景。
 */
typedef enum {
    GATEWAY_SUCCESS = 0,          /**< 成功 */
    GATEWAY_ERROR_INVALID = -1,   /**< 无效参数 */
    GATEWAY_ERROR_MEMORY = -2,    /**< 内存不足 */
    GATEWAY_ERROR_IO = -3,        /**< I/O 错误 */
    GATEWAY_ERROR_TIMEOUT = -4,   /**< 超时 */
    GATEWAY_ERROR_CLOSED = -5,    /**< 连接已关闭 */
    GATEWAY_ERROR_PROTOCOL = -6   /**< 协议错误 */
} gateway_error_t;

/* ========== 网关类型 ========== */

/**
 * @brief 网关类型枚举
 */
typedef enum {
    GATEWAY_TYPE_HTTP = 0,   /**< HTTP 网关 (libmicrohttpd) */
    GATEWAY_TYPE_WS,         /**< WebSocket 网关 (libwebsockets) */
    GATEWAY_TYPE_STDIO       /**< Stdio 网关 (标准输入输出) */
} gateway_type_t;

/* ========== 网关句柄 ========== */

/**
 * @brief 网关不透明句柄
 *
 * @note 内部包含 ops 操作表 + impl 实现指针 + type 类型标记
 */
typedef struct gateway gateway_t;

/* ========== 回调类型 ========== */

/**
 * @brief 请求处理回调函数类型
 *
 * 当外部需要自定义请求处理逻辑时，通过此回调注入。
 * 网关层收到请求后，如果设置了 handler，会优先调用此回调；
 * 否则走默认的 syscall 路由流程。
 *
 * @param[in] request_json 请求 JSON 字符串（JSON-RPC 2.0 格式），不转让所有权
 * @param[out] response_json 输出响应 JSON 字符串，由回调分配，调用者释放
 * @param[in] user_data 用户数据（gateway_set_handler 时传入）
 * @return 0 成功，非0 失败（返回 gateway_error_t 负值）
 *
 * @threadsafe 回调可能被多线程调用（HTTP/WebSocket 场景）
 * @ownership response_json 必须由回调 malloc/strdup，调用者负责 free
 *
 * @see gateway_set_handler()
 */
typedef int (*gateway_request_handler_t)(
    const char* request_json,
    char** response_json,
    void* user_data
);

/* ========== 通用接口 - 生命周期 ========== */

/**
 * @brief 创建 HTTP 网关实例
 *
 * 创建基于 libmicrohttpd 的 HTTP 网关，监听指定地址和端口，
 * 接收 JSON-RPC 2.0 POST 请求并转换为系统调用。
 *
 * @param[in] host 监听地址（如 "127.0.0.1", "0.0.0.0"），不能为 NULL
 * @param[in] port 监听端口（如 8080）
 * @return 网关句柄，失败返回 NULL（内存不足或参数无效）
 *
 * @ownership 调用者必须通过 gateway_destroy() 释放
 * @threadsafe 安全
 * @since 1.0.0
 *
 * @code
 * gateway_t* gw = gateway_http_create("0.0.0.0", 8080);
 * if (!gw) { /* 处理错误 */ }
 * gateway_start(gw);
 * // ... 运行 ...
 * gateway_destroy(gw);
 * @endcode
 */
gateway_t* gateway_http_create(const char* host, uint16_t port);

/**
 * @brief 创建 WebSocket 网关实例
 *
 * 创建基于 libwebsockets 的 WebSocket 网关，支持双向 RPC 通信。
 *
 * @param[in] host 监听地址，不能为 NULL
 * @param[in] port 监听端口
 * @return 网关句柄，失败返回 NULL
 *
 * @ownership 调用者必须通过 gateway_destroy() 释放
 * @threadsafe 安全
 * @since 1.0.0
 */
gateway_t* gateway_ws_create(const char* host, uint16_t port);

/**
 * @brief 创建 Stdio 网关实例
 *
 * 创建基于标准输入输出的命令行网关，适用于 CLI/管道场景。
 * start() 后进入阻塞式交互循环，直到用户输入 "exit"。
 *
 * @return 网关句柄，失败返回 NULL
 *
 * @ownership 调用者必须通过 gateway_destroy() 释放
 * @threadsafe 安全（但 start 后为阻塞式单线程）
 * @since 1.0.0
 *
 * @note Stdio 网关的 start() 是阻塞调用，会在当前线程运行 REPL 循环
 */
gateway_t* gateway_stdio_create(void);

/**
 * @brief 销毁网关实例并释放所有资源
 *
 * 自动停止运行中的网关，然后释放所有关联资源。
 * 对 NULL 输入静默忽略（安全）。
 *
 * @param[in] gw 网关句柄（可为 NULL，静默忽略）
 *
 * @ownership 转移网关句柄所有权给此函数，调用后 gw 不可再使用
 * @threadsafe 不安全，需调用者保证串行
 * @since 1.0.0
 */
void gateway_destroy(gateway_t* gw);

/* ========== 通用接口 - 控制操作 ========== */

/**
 * @brief 启动网关
 *
 * 启动网关开始监听连接/接收输入。
 * HTTP/WS 网关为非阻塞启动（后台线程处理）；
 * Stdio 网关为阻塞启动（REPL 循环）。
 *
 * @param[in] gw 网关句柄
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_EBUSY 端口占用或资源忙
 *
 * @pre 网关已通过 gateway_http/ws/stdio_create 创建
 * @post 网关状态变为运行中，可通过 gateway_is_running() 检查
 * @threadsafe 安全
 * @since 1.0.0
 */
agentos_error_t gateway_start(gateway_t* gw);

/**
 * @brief 停止网关
 *
 * 优雅停止网关，等待进行中的请求处理完成。
 * 对 NULL 或已停止的网关静默忽略。
 *
 * @param[in] gw 网关句柄（可为 NULL）
 * @return AGENTOS_SUCCESS 成功或静默忽略
 *
 * @post 网关停止接受新连接，is_running() 返回 false
 * @threadsafe 安全
 * @since 1.0.0
 */
agentos_error_t gateway_stop(gateway_t* gw);

/**
 * @brief 设置自定义请求处理回调
 *
 * 设置后，网关收到请求时会优先调用此回调。
 * 如果回调返回非 0，则使用回调的 response_json 作为响应；
 * 如果回调返回 0 且未设置 response_json，则走默认 syscall 路由。
 *
 * @param[in] gw 网关句柄
 * @param[in] handler 回调函数（NULL 表示清除自定义处理器）
 * @param[in] user_data 传递给回调的用户数据
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 *
 * @threadsafe 安全（原子设置）
 * @since 1.0.0
 */
agentos_error_t gateway_set_handler(
    gateway_t* gw,
    gateway_request_handler_t handler,
    void* user_data
);

/* ========== 通用接口 - 查询操作 ========== */

/**
 * @brief 获取网关类型
 *
 * @param[in] gw 网关句柄（可为 NULL）
 * @return 网关类型枚举值，NULL 返回 GATEWAY_TYPE_HTTP（默认值）
 *
 * @threadsafe 安全
 * @since 1.0.0
 */
gateway_type_t gateway_get_type(gateway_t* gw);

/**
 * @brief 检查网关是否正在运行
 *
 * @param[in] gw 网关句柄（可为 NULL）
 * @return true 正在运行中
 * @return false 已停止或参数无效
 *
 * @threadsafe 安全（原子读取）
 * @since 1.0.0
 */
bool gateway_is_running(gateway_t* gw);

/**
 * @brief 获取网关统计信息
 *
 * 返回 JSON 格式的统计数据，包括请求数、字节数、错误数等。
 *
 * @param[in] gw 网关句柄
 * @param[out] out_json 输出 JSON 字符串，需调用者 free()
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 *
 * @ownership out_json 由函数分配，调用者必须 free()
 * @threadsafe 安全（快照式读取）
 * @since 1.0.0
 *
 * @code
 * char* stats = NULL;
 * if (gateway_get_stats(gw, &stats) == AGENTOS_SUCCESS) {
 *     printf("Stats: %s\n", stats);
 *     free(stats);
 * }
 * @endcode
 */
agentos_error_t gateway_get_stats(gateway_t* gw, char** out_json);

/**
 * @brief 获取网关名称
 *
 * @param[in] gw 网关句柄（可为 NULL）
 * @return 网关名称字符串（如 "HTTP Gateway"），NULL 返回 "unknown"
 *
 * @threadsafe 安全
 * @since 1.0.0
 */
const char* gateway_get_name(gateway_t* gw);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_H */
