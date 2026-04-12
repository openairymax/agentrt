/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file ipc_common.h
 * @brief 进程间通信模块 - 跨平台 IPC 抽象层
 * 
 * @details
 * 本模块提供跨平台的进程间通信功能，包括：
 * - 管道 (Pipe)
 * - 命名管道 (Named Pipe / FIFO)
 * - Unix Domain Socket / Windows Named Pipe
 * - 共享内存 (Shared Memory)
 * - 消息队列 (Message Queue)
 * - RPC 调用框架
 * 
 * 支持平台：
 * - Windows (Named Pipe, Mailslot, Shared Memory)
 * - Linux (Unix Socket, POSIX MQ, Shared Memory)
 * - macOS (Unix Socket, POSIX MQ, Shared Memory)
 * 
 * 设计原则：
 * - 统一的消息格式和协议
 * - 支持同步和异步通信模式
 * - 内置超时和重试机制
 * - 线程安全设计
 * 
 * @author Spharx AgentOS Team
 * @date 2026-04-02
 * @version 1.0.0.6
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性原则
 */

#ifndef AGENTOS_IPC_COMMON_H
#define AGENTOS_IPC_COMMON_H

#include <error.h>
#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 类型系统说明
 * ============================================================================
 * 
 * 本模块（ipc_common.h）定义了两套类型系统：
 * 
 * 1. IPC 模块内部类型（本文件定义，使用 ipc_ 前缀）
 *    - 用于 IPC 模块的内部实现
 *    - 提供更细粒度的控制（如 IPC_TYPE_NAMED_PIPE 单独类型）
 *    - 包含完整的 IPC 功能（服务端/客户端/SHM/MQ）
 * 
 * 2. AgentOS 统一类型（types.h 定义，使用 agentos_ 前缀）
 *    - 用于跨模块接口契约
 *    - 提供简化的抽象层
 *    - 与其他 AgentOS 组件保持一致
 * 
 * 使用建议：
 * - 在 IPC 模块内部实现中使用 ipc_* 类型
 * - 在跨模块接口中使用 agentos_ipc_* 类型
 * - 两者可通过转换函数互转（见文件末尾的转换 API）
 */

/* ============================================================================
 * 常量定义
 * ============================================================================ */

/** @brief IPC 魔数 */
#define IPC_MAGIC                   0x49504300  /* "IPC\0" */

/** @brief 默认超时时间（毫秒） */
#define IPC_DEFAULT_TIMEOUT_MS      5000

/** @brief 最大消息大小 */
#define IPC_MAX_MESSAGE_SIZE        (1024 * 1024)  /* 1MB */

/** @brief 默认缓冲区大小 */
#define IPC_DEFAULT_BUFFER_SIZE     65536

/** @brief 最大通道名称长度 */
#define IPC_MAX_NAME_LEN            256

/** @brief 最大连接数 */
#define IPC_MAX_CONNECTIONS         128

/** @brief 消息对齐 */
#define IPC_MESSAGE_ALIGN           8

/* ============================================================================
 * 类型定义
 * ============================================================================ */

/**
 * @brief IPC 通道类型枚举
 */
typedef enum {
    IPC_TYPE_PIPE         = 0,    /**< 匿名管道 */
    IPC_TYPE_NAMED_PIPE   = 1,    /**< 命名管道 */
    IPC_TYPE_SOCKET       = 2,    /**< Unix Socket / Windows Named Pipe */
    IPC_TYPE_SHM          = 3,    /**< 共享内存 */
    IPC_TYPE_MQ           = 4,    /**< 消息队列 */
    IPC_TYPE_RPC          = 5     /**< RPC 调用 */
} ipc_type_t;

/**
 * @brief IPC 模式枚举
 */
typedef enum {
    IPC_MODE_READ         = 1,    /**< 只读模式 */
    IPC_MODE_WRITE        = 2,    /**< 只写模式 */
    IPC_MODE_READ_WRITE   = 3     /**< 读写模式 */
} ipc_mode_t;

/**
 * @brief IPC 状态枚举
 */
typedef enum {
    IPC_STATE_CLOSED      = 0,    /**< 已关闭 */
    IPC_STATE_OPENING     = 1,    /**< 打开中 */
    IPC_STATE_OPEN        = 2,    /**< 已打开 */
    IPC_STATE_CLOSING     = 3,    /**< 关闭中 */
    IPC_STATE_ERROR       = 4     /**< 错误状态 */
} ipc_state_t;

/**
 * @brief IPC 消息标志
 */
typedef enum {
    IPC_FLAG_NONE         = 0,    /**< 无标志 */
    IPC_FLAG_NONBLOCK     = 1,    /**< 非阻塞模式 */
    IPC_FLAG_PRIORITY     = 2,    /**< 优先级消息 */
    IPC_FLAG_BROADCAST    = 4,    /**< 广播消息 */
    IPC_FLAG_EXCLUSIVE    = 8,    /**< 独占模式 */
    IPC_FLAG_PERSISTENT   = 16    /**< 持久化消息 */
} ipc_flag_t;

/**
 * @brief IPC 消息类型
 */
typedef enum {
    IPC_MSG_DATA          = 0,    /**< 数据消息 */
    IPC_MSG_REQUEST       = 1,    /**< 请求消息 */
    IPC_MSG_RESPONSE      = 2,    /**< 响应消息 */
    IPC_MSG_NOTIFICATION  = 3,    /**< 通知消息 */
    IPC_MSG_ERROR         = 4,    /**< 错误消息 */
    IPC_MSG_CONTROL       = 5     /**< 控制消息 */
} ipc_msg_type_t;

/**
 * @brief IPC 事件类型
 */
typedef enum {
    IPC_EVENT_CONNECTED     = 1,    /**< 连接成功 */
    IPC_EVENT_DISCONNECTED  = 2,    /**< 连接断开 */
    IPC_EVENT_MESSAGE       = 3,    /**< 消息到达 */
    IPC_EVENT_ERROR         = 4,    /**< 错误发生 */
    IPC_EVENT_TIMEOUT       = 5,    /**< 超时 */
    IPC_EVENT_BUFFER_FULL   = 6,    /**< 缓冲区满 */
    IPC_EVENT_BUFFER_EMPTY  = 7     /**< 缓冲区空 */
} ipc_event_t;

/**
 * @brief IPC 消息头结构
 */
typedef struct {
    uint32_t magic;                     /**< 魔数 (IPC_MAGIC) */
    uint32_t version;                   /**< 协议版本 */
    uint32_t type;                      /**< 消息类型 */
    uint32_t flags;                     /**< 消息标志 */
    uint64_t msg_id;                    /**< 消息 ID */
    uint64_t correlation_id;            /**< 关联 ID（请求-响应模式） */
    char source[64];                    /**< 发送者标识 */
    char target[64];                    /**< 目标标识 */
    uint32_t payload_len;               /**< 负载长度 */
    uint32_t checksum;                  /**< 校验和 (CRC32) */
    agentos_timestamp_t timestamp;      /**< 时间戳 */
    uint8_t reserved[32];               /**< 保留字段 */
} ipc_message_header_t;

/**
 * @brief IPC 消息结构
 */
typedef struct {
    ipc_message_header_t header;        /**< 消息头 */
    void* payload;                      /**< 负载数据 */
    size_t payload_size;                /**< 负载大小 */
} ipc_message_t;

/**
 * @brief IPC 通道配置
 */
typedef struct {
    ipc_type_t type;                    /**< 通道类型 */
    const char* name;                   /**< 通道名称 */
    ipc_mode_t mode;                    /**< 读写模式 */
    uint32_t buffer_size;               /**< 缓冲区大小 */
    uint32_t max_message_size;          /**< 最大消息大小 */
    uint32_t timeout_ms;                /**< 默认超时 */
    uint32_t max_connections;           /**< 最大连接数（服务端） */
    bool nonblocking;                   /**< 是否非阻塞 */
    bool persistent;                    /**< 是否持久化 */
    const char* permissions;            /**< 权限设置（Unix 权限字符串） */
} ipc_config_t;

/**
 * @brief IPC 统计信息
 */
typedef struct {
    uint64_t messages_sent;             /**< 已发送消息数 */
    uint64_t messages_received;         /**< 已接收消息数 */
    uint64_t bytes_sent;                /**< 已发送字节数 */
    uint64_t bytes_received;            /**< 已接收字节数 */
    uint64_t errors;                    /**< 错误次数 */
    uint64_t timeouts;                  /**< 超时次数 */
    uint64_t avg_latency_us;            /**< 平均延迟（微秒） */
    uint64_t max_latency_us;            /**< 最大延迟（微秒） */
} ipc_stats_t;

/**
 * @brief IPC 通道句柄（不透明指针）
 */
typedef struct ipc_channel ipc_channel_t;

/**
 * @brief IPC 服务端句柄（不透明指针）
 */
typedef struct ipc_server ipc_server_t;

/**
 * @brief IPC 客户端句柄（不透明指针）
 */
typedef struct ipc_client ipc_client_t;

/**
 * @brief IPC 事件回调函数类型
 * @param channel 通道句柄
 * @param event 事件类型
 * @param data 事件数据
 * @param data_len 数据长度
 * @param user_data 用户数据
 */
typedef void (*ipc_event_callback_t)(
    ipc_channel_t* channel,
    ipc_event_t event,
    const void* data,
    size_t data_len,
    void* user_data
);

/**
 * @brief IPC 消息回调函数类型
 * @param channel 通道句柄
 * @param message 消息结构
 * @param user_data 用户数据
 * @return 返回 0 表示继续处理，非 0 表示停止处理
 */
typedef int (*ipc_message_callback_t)(
    ipc_channel_t* channel,
    const ipc_message_t* message,
    void* user_data
);

/* ============================================================================
 * 初始化与清理 API
 * ============================================================================ */

/**
 * @brief 初始化 IPC 子系统
 * @return 错误码
 */
agentos_error_t ipc_init(void);

/**
 * @brief 清理 IPC 子系统
 */
void ipc_cleanup(void);

/* ============================================================================
 * 通道管理 API
 * ============================================================================ */

/**
 * @brief 创建默认 IPC 配置
 * @param type 通道类型
 * @return 默认配置结构体
 */
ipc_config_t ipc_create_default_config(ipc_type_t type);

/**
 * @brief 创建 IPC 通道
 * @param config 通道配置
 * @return 通道句柄，失败返回 NULL
 * @ownership 调用者负责调用 ipc_channel_destroy 释放
 */
ipc_channel_t* ipc_channel_create(const ipc_config_t* config);

/**
 * @brief 销毁 IPC 通道
 * @param channel 通道句柄
 */
void ipc_channel_destroy(ipc_channel_t* channel);

/**
 * @brief 打开 IPC 通道
 * @param channel 通道句柄
 * @return 错误码
 */
agentos_error_t ipc_channel_open(ipc_channel_t* channel);

/**
 * @brief 关闭 IPC 通道
 * @param channel 通道句柄
 * @return 错误码
 */
agentos_error_t ipc_channel_close(ipc_channel_t* channel);

/**
 * @brief 获取通道状态
 * @param channel 通道句柄
 * @return 通道状态
 */
ipc_state_t ipc_channel_get_state(const ipc_channel_t* channel);

/**
 * @brief 获取通道名称
 * @param channel 通道句柄
 * @return 通道名称
 */
const char* ipc_channel_get_name(const ipc_channel_t* channel);

/**
 * @brief 获取通道类型
 * @param channel 通道句柄
 * @return 通道类型
 */
ipc_type_t ipc_channel_get_type(const ipc_channel_t* channel);

/**
 * @brief 设置通道超时
 * @param channel 通道句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
agentos_error_t ipc_channel_set_timeout(ipc_channel_t* channel, uint32_t timeout_ms);

/**
 * @brief 设置事件回调
 * @param channel 通道句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
agentos_error_t ipc_channel_set_event_callback(
    ipc_channel_t* channel,
    ipc_event_callback_t callback,
    void* user_data
);

/**
 * @brief 获取统计信息
 * @param channel 通道句柄
 * @param stats [out] 统计信息
 * @return 错误码
 */
agentos_error_t ipc_channel_get_stats(const ipc_channel_t* channel, ipc_stats_t* stats);

/**
 * @brief 重置统计信息
 * @param channel 通道句柄
 * @return 错误码
 */
agentos_error_t ipc_channel_reset_stats(ipc_channel_t* channel);

/* ============================================================================
 * 消息发送 API
 * ============================================================================ */

/**
 * @brief 发送消息
 * @param channel 通道句柄
 * @param message 消息结构
 * @return 错误码
 */
agentos_error_t ipc_send(ipc_channel_t* channel, const ipc_message_t* message);

/**
 * @brief 发送数据（简化接口）
 * @param channel 通道句柄
 * @param data 数据缓冲区
 * @param len 数据长度
 * @param sent [out] 实际发送字节数（可选）
 * @return 错误码
 */
agentos_error_t ipc_send_data(
    ipc_channel_t* channel,
    const void* data,
    size_t len,
    size_t* sent
);

/**
 * @brief 发送请求并等待响应
 * @param channel 通道句柄
 * @param request 请求消息
 * @param response [out] 响应消息
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
agentos_error_t ipc_send_request(
    ipc_channel_t* channel,
    ipc_message_t* request,
    ipc_message_t* response,
    uint32_t timeout_ms
);

/**
 * @brief 发送广播消息
 * @param channel 通道句柄
 * @param message 消息结构
 * @return 错误码
 */
agentos_error_t ipc_broadcast(ipc_channel_t* channel, const ipc_message_t* message);

/**
 * @brief 发送通知消息
 * @param channel 通道句柄
 * @param notification 通知数据
 * @param len 数据长度
 * @return 错误码
 */
agentos_error_t ipc_notify(
    ipc_channel_t* channel,
    const void* notification,
    size_t len
);

/* ============================================================================
 * 消息接收 API
 * ============================================================================ */

/**
 * @brief 接收消息
 * @param channel 通道句柄
 * @param message [out] 消息结构
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
agentos_error_t ipc_receive(
    ipc_channel_t* channel,
    ipc_message_t* message,
    uint32_t timeout_ms
);

/**
 * @brief 接收数据（简化接口）
 * @param channel 通道句柄
 * @param buffer 接收缓冲区
 * @param len 缓冲区长度
 * @param received [out] 实际接收字节数
 * @return 错误码
 */
agentos_error_t ipc_receive_data(
    ipc_channel_t* channel,
    void* buffer,
    size_t len,
    size_t* received
);

/**
 * @brief 尝试接收消息（非阻塞）
 * @param channel 通道句柄
 * @param message [out] 消息结构
 * @return 错误码，无消息时返回 AGENTOS_EBUSY
 */
agentos_error_t ipc_try_receive(ipc_channel_t* channel, ipc_message_t* message);

/**
 * @brief 设置消息回调
 * @param channel 通道句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
agentos_error_t ipc_set_message_callback(
    ipc_channel_t* channel,
    ipc_message_callback_t callback,
    void* user_data
);

/* ============================================================================
 * 服务端 API
 * ============================================================================ */

/**
 * @brief 创建 IPC 服务端
 * @param config 服务端配置
 * @return 服务端句柄，失败返回 NULL
 */
ipc_server_t* ipc_server_create(const ipc_config_t* config);

/**
 * @brief 销毁 IPC 服务端
 * @param server 服务端句柄
 */
void ipc_server_destroy(ipc_server_t* server);

/**
 * @brief 启动 IPC 服务端
 * @param server 服务端句柄
 * @return 错误码
 */
agentos_error_t ipc_server_start(ipc_server_t* server);

/**
 * @brief 停止 IPC 服务端
 * @param server 服务端句柄
 * @return 错误码
 */
agentos_error_t ipc_server_stop(ipc_server_t* server);

/**
 * @brief 接受客户端连接
 * @param server 服务端句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 客户端通道句柄，失败返回 NULL
 * @ownership 调用者负责调用 ipc_channel_destroy 释放
 */
ipc_channel_t* ipc_server_accept(ipc_server_t* server, uint32_t timeout_ms);

/**
 * @brief 获取服务端连接数
 * @param server 服务端句柄
 * @return 当前连接数
 */
size_t ipc_server_connection_count(const ipc_server_t* server);

/**
 * @brief 广播消息给所有客户端
 * @param server 服务端句柄
 * @param message 消息结构
 * @return 错误码
 */
agentos_error_t ipc_server_broadcast(ipc_server_t* server, const ipc_message_t* message);

/* ============================================================================
 * 客户端 API
 * ============================================================================ */

/**
 * @brief 创建 IPC 客户端
 * @param config 客户端配置
 * @return 客户端句柄，失败返回 NULL
 */
ipc_client_t* ipc_client_create(const ipc_config_t* config);

/**
 * @brief 销毁 IPC 客户端
 * @param client 客户端句柄
 */
void ipc_client_destroy(ipc_client_t* client);

/**
 * @brief 连接到服务端
 * @param client 客户端句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 错误码
 */
agentos_error_t ipc_client_connect(ipc_client_t* client, uint32_t timeout_ms);

/**
 * @brief 断开连接
 * @param client 客户端句柄
 * @return 错误码
 */
agentos_error_t ipc_client_disconnect(ipc_client_t* client);

/**
 * @brief 获取客户端通道
 * @param client 客户端句柄
 * @return 通道句柄
 */
ipc_channel_t* ipc_client_get_channel(ipc_client_t* client);

/* ============================================================================
 * 共享内存 API
 * ============================================================================ */

/**
 * @brief 共享内存句柄
 */
typedef struct ipc_shm ipc_shm_t;

/**
 * @brief 共享内存配置
 */
typedef struct {
    const char* name;               /**< 共享内存名称 */
    size_t size;                    /**< 共享内存大小 */
    bool read_only;                 /**< 是否只读 */
    bool create;                    /**< 是否创建（不存在则创建） */
    bool exclusive;                 /**< 是否独占创建 */
    const char* permissions;        /**< 权限设置 */
} ipc_shm_config_t;

/**
 * @brief 创建共享内存
 * @param config 共享内存配置
 * @return 共享内存句柄，失败返回 NULL
 */
ipc_shm_t* ipc_shm_create(const ipc_shm_config_t* config);

/**
 * @brief 销毁共享内存
 * @param shm 共享内存句柄
 */
void ipc_shm_destroy(ipc_shm_t* shm);

/**
 * @brief 映射共享内存到进程地址空间
 * @param shm 共享内存句柄
 * @return 映射地址，失败返回 NULL
 */
void* ipc_shm_map(ipc_shm_t* shm);

/**
 * @brief 取消映射共享内存
 * @param shm 共享内存句柄
 * @return 错误码
 */
agentos_error_t ipc_shm_unmap(ipc_shm_t* shm);

/**
 * @brief 获取共享内存大小
 * @param shm 共享内存句柄
 * @return 共享内存大小
 */
size_t ipc_shm_get_size(const ipc_shm_t* shm);

/**
 * @brief 同步共享内存
 * @param shm 共享内存句柄
 * @return 错误码
 */
agentos_error_t ipc_shm_sync(ipc_shm_t* shm);

/* ============================================================================
 * 消息队列 API
 * ============================================================================ */

/**
 * @brief 消息队列句柄
 */
typedef struct ipc_mq ipc_mq_t;

/**
 * @brief 消息队列配置
 */
typedef struct {
    const char* name;               /**< 队列名称 */
    size_t max_messages;            /**< 最大消息数 */
    size_t max_message_size;        /**< 最大消息大小 */
    bool create;                    /**< 是否创建 */
    bool exclusive;                 /**< 是否独占 */
    const char* permissions;        /**< 权限设置 */
} ipc_mq_config_t;

/**
 * @brief 创建消息队列
 * @param config 消息队列配置
 * @return 消息队列句柄，失败返回 NULL
 */
ipc_mq_t* ipc_mq_create(const ipc_mq_config_t* config);

/**
 * @brief 销毁消息队列
 * @param mq 消息队列句柄
 */
void ipc_mq_destroy(ipc_mq_t* mq);

/**
 * @brief 发送消息到队列
 * @param mq 消息队列句柄
 * @param data 消息数据
 * @param len 数据长度
 * @param priority 优先级（0 为最低）
 * @return 错误码
 */
agentos_error_t ipc_mq_send(
    ipc_mq_t* mq,
    const void* data,
    size_t len,
    unsigned int priority
);

/**
 * @brief 从队列接收消息
 * @param mq 消息队列句柄
 * @param buffer 接收缓冲区
 * @param len 缓冲区长度
 * @param received [out] 实际接收字节数
 * @param priority [out] 消息优先级（可选）
 * @param timeout_ms 超时时间
 * @return 错误码
 */
agentos_error_t ipc_mq_receive(
    ipc_mq_t* mq,
    void* buffer,
    size_t len,
    size_t* received,
    unsigned int* priority,
    uint32_t timeout_ms
);

/**
 * @brief 获取队列当前消息数
 * @param mq 消息队列句柄
 * @return 消息数量
 */
size_t ipc_mq_count(const ipc_mq_t* mq);

/**
 * @brief 清空消息队列
 * @param mq 消息队列句柄
 * @return 错误码
 */
agentos_error_t ipc_mq_clear(ipc_mq_t* mq);

/* ============================================================================
 * 消息辅助函数
 * ============================================================================ */

/**
 * @brief 创建消息
 * @param type 消息类型
 * @param payload 负载数据
 * @param payload_len 负载长度
 * @return 消息结构，失败返回 NULL
 * @ownership 调用者负责调用 ipc_message_free 释放
 */
ipc_message_t* ipc_message_create(ipc_msg_type_t type, const void* payload, size_t payload_len);

/**
 * @brief 释放消息
 * @param message 消息结构
 */
void ipc_message_free(ipc_message_t* message);

/**
 * @brief 复制消息
 * @param message 源消息
 * @return 新消息，失败返回 NULL
 */
ipc_message_t* ipc_message_clone(const ipc_message_t* message);

/**
 * @brief 计算消息校验和
 * @param message 消息结构
 * @return CRC32 校验和
 */
uint32_t ipc_message_checksum(const ipc_message_t* message);

/**
 * @brief 验证消息校验和
 * @param message 消息结构
 * @return true 校验通过，false 校验失败
 */
bool ipc_message_verify(const ipc_message_t* message);

/**
 * @brief 序列化消息为字节流
 * @param message 消息结构
 * @param buffer 输出缓冲区
 * @param buffer_len 缓冲区长度
 * @param written [out] 实际写入字节数
 * @return 错误码
 */
agentos_error_t ipc_message_serialize(
    const ipc_message_t* message,
    void* buffer,
    size_t buffer_len,
    size_t* written
);

/**
 * @brief 从字节流反序列化消息
 * @param buffer 输入缓冲区
 * @param len 缓冲区长度
 * @param message [out] 消息结构
 * @return 错误码
 */
agentos_error_t ipc_message_deserialize(
    const void* buffer,
    size_t len,
    ipc_message_t* message
);

/* ============================================================================
 * 工具函数
 * ============================================================================ */

/**
 * @brief 获取错误消息
 * @param channel 通道句柄
 * @return 错误消息字符串
 */
const char* ipc_get_error_message(const ipc_channel_t* channel);

/**
 * @brief 检查通道是否可用
 * @param channel 通道句柄
 * @return true 可用，false 不可用
 */
bool ipc_is_valid(const ipc_channel_t* channel);

/**
 * @brief 刷新通道缓冲区
 * @param channel 通道句柄
 * @return 错误码
 */
agentos_error_t ipc_flush(ipc_channel_t* channel);

/* ============================================================================
 * 类型转换 API（IPC 内部类型 ↔ AgentOS 统一类型）
 * ============================================================================ */

/**
 * @brief 将 AgentOS 统一 IPC 类型转换为 IPC 模块内部类型
 * @param agentos_type AgentOS 统一 IPC 类型
 * @return IPC 模块内部类型
 */
static inline ipc_type_t ipc_type_from_agentos(agentos_ipc_type_t agentos_type) {
    switch (agentos_type) {
        case AGENTOS_IPC_PIPE:   return IPC_TYPE_PIPE;
        case AGENTOS_IPC_SOCKET: return IPC_TYPE_SOCKET;
        case AGENTOS_IPC_SHM:    return IPC_TYPE_SHM;
        case AGENTOS_IPC_MQ:     return IPC_TYPE_MQ;
        case AGENTOS_IPC_RPC:    return IPC_TYPE_RPC;
        default:                 return IPC_TYPE_PIPE;
    }
}

/**
 * @brief 将 IPC 模块内部类型转换为 AgentOS 统一 IPC 类型
 * @param ipc_type IPC 模块内部类型
 * @return AgentOS 统一 IPC 类型
 */
static inline agentos_ipc_type_t ipc_type_to_agentos(ipc_type_t ipc_type) {
    switch (ipc_type) {
        case IPC_TYPE_PIPE:       return AGENTOS_IPC_PIPE;
        case IPC_TYPE_NAMED_PIPE: return AGENTOS_IPC_SOCKET; /* 命名管道映射到 Socket */
        case IPC_TYPE_SOCKET:     return AGENTOS_IPC_SOCKET;
        case IPC_TYPE_SHM:        return AGENTOS_IPC_SHM;
        case IPC_TYPE_MQ:         return AGENTOS_IPC_MQ;
        case IPC_TYPE_RPC:        return AGENTOS_IPC_RPC;
        default:                  return AGENTOS_IPC_PIPE;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_IPC_COMMON_H */
