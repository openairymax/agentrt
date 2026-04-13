// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ipc_service_bus.h
 * @brief IPC服务总线 - 守护进程间统一通信框架
 *
 * 提供守护进程间的高效通信抽象层，集成UnifiedProtocol协议栈，
 * 支持多协议消息传递、服务发现和负载均衡。
 *
 * 设计原则：
 * 1. 统一消息总线：所有守护进程通过统一总线通信
 * 2. 协议感知：消息携带协议类型，支持MCP/A2A/OpenAI API等
 * 3. 位置透明：服务消费者无需知道提供者的物理位置
 * 4. 弹性通信：内置重试、超时和熔断机制
 *
 * @see svc_common.h 服务管理框架
 * @see ipc_common.h IPC底层抽象
 */

#ifndef AGENTOS_IPC_SERVICE_BUS_H
#define AGENTOS_IPC_SERVICE_BUS_H

#include "svc_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */

#define IPC_BUS_MAX_SERVICES        64
#define IPC_BUS_MAX_CHANNELS        32
#define IPC_BUS_MAX_MESSAGE_SIZE    (512 * 1024)
#define IPC_BUS_DEFAULT_TIMEOUT_MS  5000
#define IPC_BUS_MAX_RETRIES         3
#define IPC_BUS_CHANNEL_NAME_LEN    128
#define IPC_BUS_SERVICE_ID_LEN      64

/* ==================== 服务总线消息类型 ==================== */

typedef enum {
    IPC_BUS_MSG_REQUEST       = 0,
    IPC_BUS_MSG_RESPONSE      = 1,
    IPC_BUS_MSG_NOTIFICATION  = 2,
    IPC_BUS_MSG_BROADCAST     = 3,
    IPC_BUS_MSG_HEARTBEAT     = 4,
    IPC_BUS_MSG_DISCOVERY     = 5,
    IPC_BUS_MSG_CONTROL       = 6
} ipc_bus_msg_type_t;

/* ==================== 服务总线协议类型 ==================== */

typedef enum {
    IPC_BUS_PROTO_JSON_RPC    = 0,
    IPC_BUS_PROTO_MCP         = 1,
    IPC_BUS_PROTO_A2A         = 2,
    IPC_BUS_PROTO_OPENAI      = 3,
    IPC_BUS_PROTO_AUTO        = 4
} ipc_bus_proto_t;

/* ==================== 服务总线消息头 ==================== */

typedef struct {
    uint32_t magic;
    uint32_t version;
    ipc_bus_msg_type_t msg_type;
    ipc_bus_proto_t protocol;
    uint64_t msg_id;
    uint64_t correlation_id;
    char source[IPC_BUS_SERVICE_ID_LEN];
    char target[IPC_BUS_SERVICE_ID_LEN];
    uint32_t payload_len;
    uint32_t flags;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t reserved[16];
} ipc_bus_message_header_t;

#define IPC_BUS_MESSAGE_MAGIC    0x49534200
#define IPC_BUS_MESSAGE_VERSION  1

/* ==================== 服务总线消息 ==================== */

typedef struct {
    ipc_bus_message_header_t header;
    void* payload;
    size_t payload_size;
} ipc_bus_message_t;

/* ==================== 服务总线通道配置 ==================== */

typedef struct {
    char name[IPC_BUS_CHANNEL_NAME_LEN];
    ipc_bus_proto_t default_protocol;
    uint32_t timeout_ms;
    uint32_t max_retries;
    uint32_t buffer_size;
    bool enable_compression;
    bool enable_encryption;
} ipc_bus_channel_config_t;

/* ==================== 服务总线服务端点 ==================== */

typedef struct {
    char service_name[IPC_BUS_SERVICE_ID_LEN];
    char endpoint[256];
    ipc_bus_proto_t supported_protocols[4];
    uint32_t protocol_count;
    uint32_t weight;
    bool healthy;
    uint64_t last_heartbeat;
    uint32_t active_connections;
    uint32_t max_connections;
} ipc_bus_endpoint_t;

/* ==================== 服务总线统计 ==================== */

typedef struct {
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint64_t errors;
    uint64_t timeouts;
    uint64_t avg_latency_us;
    uint64_t max_latency_us;
    uint32_t active_channels;
    uint32_t active_endpoints;
} ipc_bus_stats_t;

/* ==================== 服务总线句柄 ==================== */

typedef struct ipc_service_bus_s* ipc_service_bus_t;
typedef struct ipc_bus_channel_s* ipc_bus_channel_t;

/* ==================== 回调函数类型 ==================== */

typedef int (*ipc_bus_message_handler_t)(
    ipc_bus_channel_t channel,
    const ipc_bus_message_t* message,
    void* user_data
);

typedef void (*ipc_bus_event_handler_t)(
    ipc_service_bus_t bus,
    const char* event_name,
    const void* event_data,
    size_t data_len,
    void* user_data
);

/* ==================== 服务总线生命周期 ==================== */

/**
 * @brief 创建服务总线实例
 * @param bus_name 总线名称
 * @param config 总线配置（NULL使用默认）
 * @return 总线句柄，失败返回NULL
 */
AGENTOS_API ipc_service_bus_t ipc_service_bus_create(
    const char* bus_name,
    const ipc_bus_channel_config_t* config
);

/**
 * @brief 销毁服务总线实例
 * @param bus 总线句柄
 */
AGENTOS_API void ipc_service_bus_destroy(ipc_service_bus_t bus);

/**
 * @brief 启动服务总线
 * @param bus 总线句柄
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_start(ipc_service_bus_t bus);

/**
 * @brief 停止服务总线
 * @param bus 总线句柄
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_stop(ipc_service_bus_t bus);

/* ==================== 通道管理 ==================== */

/**
 * @brief 创建通信通道
 * @param bus 总线句柄
 * @param config 通道配置
 * @return 通道句柄，失败返回NULL
 */
AGENTOS_API ipc_bus_channel_t ipc_bus_channel_create(
    ipc_service_bus_t bus,
    const ipc_bus_channel_config_t* config
);

/**
 * @brief 销毁通信通道
 * @param channel 通道句柄
 */
AGENTOS_API void ipc_bus_channel_destroy(ipc_bus_channel_t channel);

/**
 * @brief 获取通道名称
 * @param channel 通道句柄
 * @return 通道名称
 */
AGENTOS_API const char* ipc_bus_channel_get_name(ipc_bus_channel_t channel);

/* ==================== 消息发送 ==================== */

/**
 * @brief 发送消息到指定服务
 * @param bus 总线句柄
 * @param target_service 目标服务名称
 * @param message 消息结构
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_send(
    ipc_service_bus_t bus,
    const char* target_service,
    const ipc_bus_message_t* message
);

/**
 * @brief 发送请求并等待响应
 * @param bus 总线句柄
 * @param target_service 目标服务名称
 * @param request 请求消息
 * @param response [out] 响应消息
 * @param timeout_ms 超时时间
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_request(
    ipc_service_bus_t bus,
    const char* target_service,
    const ipc_bus_message_t* request,
    ipc_bus_message_t* response,
    uint32_t timeout_ms
);

/**
 * @brief 广播消息到所有服务
 * @param bus 总线句柄
 * @param message 消息结构
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_broadcast(
    ipc_service_bus_t bus,
    const ipc_bus_message_t* message
);

/**
 * @brief 发送通知消息
 * @param bus 总线句柄
 * @param target_service 目标服务名称
 * @param payload 负载数据
 * @param payload_size 负载大小
 * @param protocol 协议类型
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_notify(
    ipc_service_bus_t bus,
    const char* target_service,
    const void* payload,
    size_t payload_size,
    ipc_bus_proto_t protocol
);

/* ==================== 消息接收 ==================== */

/**
 * @brief 注册消息处理器
 * @param bus 总线句柄
 * @param handler 消息处理函数
 * @param user_data 用户数据
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_register_handler(
    ipc_service_bus_t bus,
    ipc_bus_message_handler_t handler,
    void* user_data
);

/**
 * @brief 注销消息处理器
 * @param bus 总线句柄
 * @param handler 消息处理函数
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_unregister_handler(
    ipc_service_bus_t bus,
    ipc_bus_message_handler_t handler
);

/**
 * @brief 注册事件处理器
 * @param bus 总线句柄
 * @param event_name 事件名称
 * @param handler 事件处理函数
 * @param user_data 用户数据
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_register_event_handler(
    ipc_service_bus_t bus,
    const char* event_name,
    ipc_bus_event_handler_t handler,
    void* user_data
);

/* ==================== 服务端点管理 ==================== */

/**
 * @brief 注册服务端点
 * @param bus 总线句柄
 * @param endpoint 端点信息
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_register_endpoint(
    ipc_service_bus_t bus,
    const ipc_bus_endpoint_t* endpoint
);

/**
 * @brief 注销服务端点
 * @param bus 总线句柄
 * @param service_name 服务名称
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_unregister_endpoint(
    ipc_service_bus_t bus,
    const char* service_name
);

/**
 * @brief 发现服务端点
 * @param bus 总线句柄
 * @param service_name 服务名称（NULL表示所有）
 * @param protocol 协议过滤（IPC_BUS_PROTO_AUTO表示不过滤）
 * @param endpoints [out] 端点数组
 * @param max_count 数组最大容量
 * @param found_count [out] 实际找到数量
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_discover(
    ipc_service_bus_t bus,
    const char* service_name,
    ipc_bus_proto_t protocol,
    ipc_bus_endpoint_t* endpoints,
    uint32_t max_count,
    uint32_t* found_count
);

/**
 * @brief 选择最优端点（负载均衡）
 * @param bus 总线句柄
 * @param service_name 服务名称
 * @param protocol 协议类型
 * @param endpoint [out] 选中的端点
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_select_endpoint(
    ipc_service_bus_t bus,
    const char* service_name,
    ipc_bus_proto_t protocol,
    ipc_bus_endpoint_t* endpoint
);

/**
 * @brief 更新端点健康状态
 * @param bus 总线句柄
 * @param service_name 服务名称
 * @param healthy 是否健康
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_update_endpoint_health(
    ipc_service_bus_t bus,
    const char* service_name,
    bool healthy
);

/* ==================== 消息辅助函数 ==================== */

/**
 * @brief 创建服务总线消息
 * @param msg_type 消息类型
 * @param protocol 协议类型
 * @param payload 负载数据
 * @param payload_size 负载大小
 * @return 消息结构，失败返回NULL
 */
AGENTOS_API ipc_bus_message_t* ipc_bus_message_create(
    ipc_bus_msg_type_t msg_type,
    ipc_bus_proto_t protocol,
    const void* payload,
    size_t payload_size
);

/**
 * @brief 释放服务总线消息
 * @param message 消息结构
 */
AGENTOS_API void ipc_bus_message_free(ipc_bus_message_t* message);

/**
 * @brief 复制消息
 * @param message 源消息
 * @return 新消息，失败返回NULL
 */
AGENTOS_API ipc_bus_message_t* ipc_bus_message_clone(
    const ipc_bus_message_t* message
);

/**
 * @brief 协议类型转字符串
 * @param proto 协议类型
 * @return 协议名称字符串
 */
AGENTOS_API const char* ipc_bus_proto_to_string(ipc_bus_proto_t proto);

/**
 * @brief 字符串转协议类型
 * @param str 协议名称
 * @return 协议类型
 */
AGENTOS_API ipc_bus_proto_t ipc_bus_proto_from_string(const char* str);

/* ==================== 统计与诊断 ==================== */

/**
 * @brief 获取服务总线统计信息
 * @param bus 总线句柄
 * @param stats [out] 统计信息
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_get_stats(
    ipc_service_bus_t bus,
    ipc_bus_stats_t* stats
);

/**
 * @brief 重置统计信息
 * @param bus 总线句柄
 * @return 0成功，非0失败
 */
AGENTOS_API agentos_error_t ipc_service_bus_reset_stats(ipc_service_bus_t bus);

/**
 * @brief 获取总线名称
 * @param bus 总线句柄
 * @return 总线名称
 */
AGENTOS_API const char* ipc_service_bus_get_name(ipc_service_bus_t bus);

/**
 * @brief 检查总线是否运行中
 * @param bus 总线句柄
 * @return true运行中，false未运行
 */
AGENTOS_API bool ipc_service_bus_is_running(ipc_service_bus_t bus);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_IPC_SERVICE_BUS_H */
