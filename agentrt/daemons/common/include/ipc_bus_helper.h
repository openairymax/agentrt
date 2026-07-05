// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file ipc_bus_helper.h
 * @brief C-L09: IPC Bus → daemon 自动注册便捷层
 *
 * 每个 daemon 启动时调用此模块的便捷 API 实现 IPC Bus 自动注册、
 * 消息路由和协议透明转发。
 *
 * 使用方式（典型 daemon main()）：
 * @code
 *   // 1. 初始化 IPC Bus 助手
 *   ipc_bus_helper_t *ibh = ipc_bus_helper_init("llm_d", NULL);
 *
 *   // 2. 注册 IPC 通道（P1.8.1）
 *   ipc_bus_helper_register_channel(ibh, "llm", IPC_BUS_PROTO_JSON_RPC);
 *
 *   // 3. 注册消息处理器（P1.8.2）
 *   ipc_bus_helper_register_handler(ibh, my_handler, NULL);
 *
 *   // 4. 发送消息到其他 daemon（P1.8.3）
 *   ipc_bus_helper_send(ibh, "tool_d", payload, len);
 *
 *   // ... daemon 主循环 ...
 *
 *   // 5. 关闭
 *   ipc_bus_helper_shutdown(ibh);
 * @endcode
 *
 * @see ipc_service_bus.h
 * @see P1.8 C-L09 连接线
 */

#ifndef AGENTRT_IPC_BUS_HELPER_H
#define AGENTRT_IPC_BUS_HELPER_H

#include "ipc_service_bus.h"
#include "ipc_backpressure.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 便捷类型 ==================== */

/**
 * @brief IPC Bus 助手句柄
 *
 * 封装了 ipc_service_bus 实例、通道、端点和协议路由。
 */
typedef struct ipc_bus_helper_s ipc_bus_helper_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 初始化 IPC Bus 助手
 *
 * 创建 ipc_service_bus 实例并启动。
 *
 * @param daemon_name daemon 名称（作为 bus 名称）
 * @param config      通道配置（NULL 使用默认配置）
 * @return 助手句柄，失败返回 NULL
 */
ipc_bus_helper_t *ipc_bus_helper_init(const char *daemon_name,
                                      const ipc_bus_channel_config_t *config);

/**
 * @brief 关闭 IPC Bus 助手
 *
 * 自动注销通道、释放资源。
 *
 * @param ibh 助手句柄
 */
void ipc_bus_helper_shutdown(ipc_bus_helper_t *ibh);

/* ==================== 通道注册（P1.8.1） ==================== */

/**
 * @brief 为当前 daemon 注册 IPC Bus 通道
 *
 * 创建一个命名的通信通道，其他 daemon 通过此通道发送消息。
 *
 * @param ibh             助手句柄
 * @param channel_name    通道名称（如 "llm"）
 * @param default_protocol 默认协议
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_register_channel(ipc_bus_helper_t *ibh,
                                    const char *channel_name,
                                    ipc_bus_proto_t default_protocol);

/**
 * @brief 注册 IPC Bus 端点（让其他 daemon 可发现）
 *
 * @param ibh          助手句柄
 * @param service_name 服务名称
 * @param endpoint     端点地址（如 "127.0.0.1:8080"）
 * @param protocols    支持的协议列表
 * @param proto_count  协议数量
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_register_endpoint(ipc_bus_helper_t *ibh,
                                     const char *service_name,
                                     const char *endpoint,
                                     const ipc_bus_proto_t *protocols,
                                     uint32_t proto_count);

/* ==================== 消息处理器（P1.8.2） ==================== */

/**
 * @brief 注册消息处理器
 *
 * 当其他 daemon 发送消息到此 daemon 的通道时，调用此处理器。
 *
 * @param ibh       助手句柄
 * @param handler   消息处理器
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_register_handler(ipc_bus_helper_t *ibh,
                                    ipc_bus_message_handler_t handler,
                                    void *user_data);

/**
 * @brief 注册事件处理器
 *
 * 监听 IPC Bus 事件（如端点上线/下线）。
 *
 * @param ibh        助手句柄
 * @param event_name 事件名称
 * @param handler    事件处理器
 * @param user_data  用户数据
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_register_event_handler(ipc_bus_helper_t *ibh,
                                          const char *event_name,
                                          ipc_bus_event_handler_t handler,
                                          void *user_data);

/* ==================== 消息发送（P1.8.3） ==================== */

/**
 * @brief 发送消息到目标 daemon
 *
 * 便捷封装 ipc_service_bus_send，自动创建消息并发送。
 *
 * @param ibh            助手句柄
 * @param target_service 目标服务名称
 * @param msg_type       消息类型
 * @param protocol       协议类型
 * @param payload        消息负载
 * @param payload_size   负载大小
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_send(ipc_bus_helper_t *ibh, const char *target_service,
                        ipc_bus_msg_type_t msg_type, ipc_bus_proto_t protocol,
                        const void *payload, size_t payload_size);

/**
 * @brief 发送请求并等待响应
 *
 * 便捷封装 ipc_service_bus_request。
 *
 * @param ibh            助手句柄
 * @param target_service 目标服务名称
 * @param request        请求消息
 * @param response       响应消息（输出）
 * @param timeout_ms     超时（毫秒），0 使用默认值
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_request(ipc_bus_helper_t *ibh, const char *target_service,
                           const ipc_bus_message_t *request,
                           ipc_bus_message_t *response, uint32_t timeout_ms);

/**
 * @brief 广播消息到所有 daemon
 *
 * @param ibh         助手句柄
 * @param message     广播消息
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_broadcast(ipc_bus_helper_t *ibh,
                             const ipc_bus_message_t *message);

/**
 * @brief 发送通知（fire-and-forget）
 *
 * @param ibh            助手句柄
 * @param target_service 目标服务名称
 * @param payload        通知负载
 * @param payload_size   负载大小
 * @param protocol       协议类型
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_notify(ipc_bus_helper_t *ibh, const char *target_service,
                          const void *payload, size_t payload_size,
                          ipc_bus_proto_t protocol);

/* ==================== 协议透明路由（P1.8.4） ==================== */

/**
 * @brief 自动选择协议并路由消息
 *
 * 根据目标服务支持的协议自动选择最佳协议，实现 JSON-RPC/MCP/A2A
 * 透明路由。如果目标服务支持多种协议，按优先级选择：
 *   JSON-RPC > MCP > A2A > OpenAI
 *
 * @param ibh            助手句柄
 * @param target_service 目标服务名称
 * @param payload        消息负载
 * @param payload_size   负载大小
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_route_auto(ipc_bus_helper_t *ibh,
                              const char *target_service,
                              const void *payload, size_t payload_size);

/**
 * @brief 发现支持特定协议的服务端点
 *
 * @param ibh          助手句柄
 * @param service_name 服务名称（NULL 表示所有服务）
 * @param protocol     协议类型
 * @param endpoints    输出端点数组
 * @param max_count    最大数量
 * @param found_count  实际找到数量
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_discover(ipc_bus_helper_t *ibh,
                            const char *service_name,
                            ipc_bus_proto_t protocol,
                            ipc_bus_endpoint_t *endpoints,
                            uint32_t max_count, uint32_t *found_count);

/* ==================== 状态查询 ==================== */

/**
 * @brief 获取底层 ipc_service_bus 句柄
 *
 * @param ibh 助手句柄
 * @return ipc_service_bus 句柄
 */
ipc_service_bus_t ipc_bus_helper_get_bus(ipc_bus_helper_t *ibh);

/**
 * @brief 检查 IPC Bus 是否运行中
 *
 * @param ibh 助手句柄
 * @return true 运行中
 */
bool ipc_bus_helper_is_running(ipc_bus_helper_t *ibh);

/* ==================== 背压控制集成（P1.24） ==================== */

/**
 * @brief P1.24: 为 IPC Bus 助手启用背压控制
 *
 * 创建背压控制器并关联到助手。启用后，send/notify 调用会自动
 * 检查背压级别，丢弃可丢弃消息或拒绝发送。
 *
 * @param ibh    助手句柄
 * @param config 背压配置（NULL 使用默认配置）
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_enable_backpressure(ipc_bus_helper_t *ibh,
                                       const ipc_bp_config_t *config);

/**
 * @brief P1.24: 更新队列深度（由 daemon 定期调用）
 *
 * daemon 应每 5s 调用一次，传入当前消息队列深度。
 * 背压控制器根据深度自动调整级别。
 *
 * @param ibh           助手句柄
 * @param current_depth 当前队列深度（消息数）
 * @return 当前背压级别
 */
ipc_bp_level_t ipc_bus_helper_update_backpressure(ipc_bus_helper_t *ibh,
                                                   size_t current_depth);

/**
 * @brief P1.24: 发送消息时自动检查背压
 *
 * 替代 ipc_bus_helper_send，在发送前检查背压级别。
 * 如果背压级别为 DROP 且消息可丢弃，则丢弃消息。
 * 如果背压级别为 REJECT 且消息非关键，则拒绝发送。
 *
 * @param ibh          助手句柄
 * @param target       目标服务
 * @param msg_type     消息类型
 * @param protocol     协议
 * @param payload      负载
 * @param payload_size 负载大小
 * @param is_droppable 消息是否可丢弃（日志/指标等低优先级）
 * @return 0 成功，-1 失败，1 被背压丢弃
 */
int ipc_bus_helper_send_with_bp(ipc_bus_helper_t *ibh, const char *target,
                                ipc_bus_msg_type_t msg_type, ipc_bus_proto_t protocol,
                                const void *payload, size_t payload_size,
                                bool is_droppable);

/**
 * @brief P1.24: 检查是否应接受新连接
 *
 * @param ibh 助手句柄
 * @return true 接受，false 拒绝（背压 REJECT 级别）
 */
bool ipc_bus_helper_should_accept_connection(ipc_bus_helper_t *ibh);

/**
 * @brief P1.24: 获取背压统计
 *
 * @param ibh       助手句柄
 * @param out_stats 输出统计
 * @return 0 成功，非0 失败（未启用背压或参数无效）
 */
int ipc_bus_helper_get_bp_stats(ipc_bus_helper_t *ibh, ipc_bp_stats_t *out_stats);

/**
 * @brief P1.24: 获取当前背压级别
 *
 * @param ibh 助手句柄
 * @return 背压级别（未启用背压返回 IPC_BP_NORMAL）
 */
ipc_bp_level_t ipc_bus_helper_get_bp_level(ipc_bus_helper_t *ibh);

/* ==================== P1.8: 路由统计查询 ==================== */

/**
 * @brief P1.8: 获取 IPC Bus 路由统计信息
 *
 * 返回消息路由的累计统计，包括发送次数、自动路由次数、
 * 降级次数、失败次数、背压丢弃/拒绝次数。
 * 所有输出参数均可为 NULL（跳过对应统计）。
 *
 * @param ibh                  助手句柄
 * @param out_total_sends      总发送次数（可为 NULL）
 * @param out_total_routes     总自动路由次数（可为 NULL）
 * @param out_route_fallbacks  路由降级次数（可为 NULL）
 * @param out_send_failures    发送失败次数（可为 NULL）
 * @param out_bp_drops         背压丢弃次数（可为 NULL）
 * @param out_bp_rejects       背压拒绝次数（可为 NULL）
 * @return 0 成功，非0 失败
 */
int ipc_bus_helper_get_routing_stats(ipc_bus_helper_t *ibh,
                                     uint64_t *out_total_sends,
                                     uint64_t *out_total_routes,
                                     uint64_t *out_route_fallbacks,
                                     uint64_t *out_send_failures,
                                     uint64_t *out_bp_drops,
                                     uint64_t *out_bp_rejects);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_IPC_BUS_HELPER_H */