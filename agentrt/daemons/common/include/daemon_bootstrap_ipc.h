// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file daemon_bootstrap_ipc.h
 * @brief P1.8 C-L09: daemon IPC Bus 一键引导模块
 *
 * 每个 daemon 启动时调用此模块即可自动完成：
 * 1. IPC Bus 初始化
 * 2. 通道注册（channel）
 * 3. 端点注册（endpoint，让其他 daemon 可发现）
 * 4. 默认消息处理器注册
 * 5. 协议自动路由
 * 6. 关闭时自动注销
 *
 * 使用方式（典型 daemon main()）：
 * @code
 *   #include "daemon_bootstrap_ipc.h"
 *
 *   // 1. 引导 IPC Bus
 *   daemon_bootstrap_ipc_t *bipc = daemon_bootstrap_ipc_start(
 *       "llm_d", "llm", "127.0.0.1", 8080, IPC_BUS_PROTO_JSON_RPC);
 *
 *   // 2. 注册自定义消息处理器（可选）
 *   daemon_bootstrap_ipc_register_handler(bipc, my_handler, NULL);
 *
 *   // ... daemon 主循环 ...
 *
 *   // 3. 关闭
 *   daemon_bootstrap_ipc_stop(bipc);
 * @endcode
 *
 * @see ipc_bus_helper.h
 * @see P1.8 C-L09 连接线
 */

#ifndef AGENTRT_DAEMON_BOOTSTRAP_IPC_H
#define AGENTRT_DAEMON_BOOTSTRAP_IPC_H

#include "ipc_bus_helper.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 引导句柄 ==================== */

typedef struct daemon_bootstrap_ipc_s daemon_bootstrap_ipc_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 一键引导：初始化 IPC Bus + 注册通道 + 注册端点
 *
 * @param daemon_name    daemon 名称（如 "llm_d"）
 * @param channel_name   通道名称（如 "llm"）
 * @param host           监听地址（如 "127.0.0.1"），NULL 使用 Unix socket
 * @param port           监听端口，0 使用 Unix socket
 * @param protocol       默认协议
 * @return 引导句柄，失败返回 NULL
 */
daemon_bootstrap_ipc_t *daemon_bootstrap_ipc_start(const char *daemon_name,
                                                    const char *channel_name,
                                                    const char *host, uint16_t port,
                                                    ipc_bus_proto_t protocol);

/**
 * @brief 一键引导（Unix Socket 版本）
 *
 * @param daemon_name   daemon 名称
 * @param channel_name  通道名称
 * @param socket_path   Unix socket 路径
 * @param protocol      默认协议
 * @return 引导句柄，失败返回 NULL
 */
daemon_bootstrap_ipc_t *daemon_bootstrap_ipc_start_unix(const char *daemon_name,
                                                         const char *channel_name,
                                                         const char *socket_path,
                                                         ipc_bus_proto_t protocol);

/**
 * @brief 停止 IPC Bus 引导
 *
 * @param bipc 引导句柄
 */
void daemon_bootstrap_ipc_stop(daemon_bootstrap_ipc_t *bipc);

/* ==================== 消息处理器 ==================== */

/**
 * @brief 注册自定义消息处理器
 *
 * @param bipc     引导句柄
 * @param handler  消息处理器
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
int daemon_bootstrap_ipc_register_handler(daemon_bootstrap_ipc_t *bipc,
                                           ipc_bus_message_handler_t handler,
                                           void *user_data);

/* ==================== 消息发送 ==================== */

/**
 * @brief 便捷发送方法（自动路由）
 *
 * @param bipc           引导句柄
 * @param target_service 目标服务名称
 * @param payload        消息负载
 * @param payload_size   负载大小
 * @return 0 成功，非0 失败
 */
int daemon_bootstrap_ipc_send(daemon_bootstrap_ipc_t *bipc,
                               const char *target_service,
                               const void *payload, size_t payload_size);

/* ==================== 查询 ==================== */

/**
 * @brief 获取底层 ipc_bus_helper 句柄
 *
 * @param bipc 引导句柄
 * @return ipc_bus_helper 句柄
 */
ipc_bus_helper_t *daemon_bootstrap_ipc_get_helper(daemon_bootstrap_ipc_t *bipc);

/**
 * @brief 检查 IPC Bus 是否运行中
 *
 * @param bipc 引导句柄
 * @return true 运行中
 */
bool daemon_bootstrap_ipc_is_running(daemon_bootstrap_ipc_t *bipc);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_BOOTSTRAP_IPC_H */