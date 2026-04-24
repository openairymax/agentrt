// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file openjiuwen_adapter.h
 * @brief OpenJiuwen Protocol Adapter Interface
 *
 * 提供与OpenJiuwen平台的协议兼容层，支持消息格式转换和互操作。
 */

#ifndef OPENJIUWEN_ADAPTER_H
#define OPENJIUWEN_ADAPTER_H

#include "unified_protocol.h"
#include "protocol_extension_framework.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 配置常量
 * ============================================================================ */

#define OPENJIUWEN_PROTOCOL_VERSION "1.0.0"
#define OPENJIUWEN_MAX_MESSAGE_SIZE (64 * 1024)
#define OPENJIUWEN_DEFAULT_ENDPOINT "/openjiuwen/v1"
#define OPENJIUWEN_TIMEOUT_MS 30000
#define OPENJIUWEN_HEARTBEAT_INTERVAL_SEC 30
#define OPENJIUWEN_MAX_CONSECUTIVE_ERRORS 5
#define OPENJIUWEN_RECONNECT_BASE_DELAY_MS 1000
#define OPENJIUWEN_RECONNECT_MAX_DELAY_MS 60000
#define OPENJIUWEN_SEND_QUEUE_SIZE 64

typedef enum {
    OPENJIUWEN_CONN_DISCONNECTED = 0,
    OPENJIUWEN_CONN_CONNECTING,
    OPENJIUWEN_CONN_CONNECTED,
    OPENJIUWEN_CONN_RECONNECTING,
    OPENJIUWEN_CONN_ERROR
} openjiuwen_conn_state_t;

/* ============================================================================
 * 数据类型定义
 * ============================================================================ */

/**
 * @brief OpenJiuwen配置结构体
 */
typedef struct openjiuwen_config_s {
    char endpoint[256];           /* API端点URL */
    char api_key[128];            /* API密钥（可选） */
    int timeout_ms;               /* 超时时间（毫秒） */
    bool enable_compression;      /* 是否启用压缩 */
    bool enable_encryption;       /* 是否启用加密 */
    uint32_t max_retries;         /* 最大重试次数 */
} openjiuwen_config_t;

/**
 * @brief OpenJiuwen消息头部
 */
typedef struct openjiuwen_header_s {
    uint32_t message_id;          /* 消息ID */
    uint32_t timestamp;           /* 时间戳 */
    uint16_t message_type;        /* 消息类型 */
    uint16_t flags;              /* 标志位 */
    uint32_t payload_length;     /* 载荷长度 */
    char source_agent[64];        /* 来源智能体ID */
    char target_agent[64];        /* 目标智能体ID */
} openjiuwen_header_t;

/**
 * @brief OpenJiuwen消息类型枚举
 */
typedef enum openjiuwen_message_type_e {
    OPENJIUWEN_MSG_TYPE_REQUEST = 0x0001,     /* 请求消息 */
    OPENJIUWEN_MSG_TYPE_RESPONSE = 0x0002,    /* 响应消息 */
    OPENJIUWEN_MSG_TYPE_NOTIFICATION = 0x0003, /* 通知消息 */
    OPENJIUWEN_MSG_TYPE_HEARTBEAT = 0x0004,   /* 心跳消息 */
    OPENJIUWEN_MSG_TYPE_ERROR = 0x0005        /* 错误消息 */
} openjiuwen_message_type_t;

/**
 * @brief OpenJiuwen适配器实例（继承自openjiuwen_base_adapter_t）
 */
typedef struct openjiuwen_adapter_s {
    protocol_adapter_t base;
    openjiuwen_config_t config;
    void* connection_handle;
    bool initialized;
    uint32_t message_counter;
    void* user_data;
    openjiuwen_conn_state_t conn_state;
    uint32_t consecutive_errors;
    uint32_t total_reconnects;
    uint32_t last_heartbeat_sec;
    uint32_t last_error_code;
    uint64_t last_activity_ms;
} openjiuwen_adapter_t;

/* ============================================================================
 * 公共接口函数
 * ============================================================================ */

/**
 * @brief 创建OpenJiuwen适配器实例
 *
 * @param config OpenJiuwen配置（可为NULL使用默认配置）
 * @return 新创建的适配器实例，失败返回NULL
 */
const protocol_adapter_t* openjiuwen_adapter_create(const openjiuwen_config_t* config);

/**
 * @brief 初始化默认配置
 *
 * @param config 输出的配置结构体指针
 */
void openjiuwen_get_default_config(openjiuwen_config_t* config);

/**
 * @brief 验证OpenJiuwen连接
 *
 * @param adapter 适配器实例
 * @return 0成功，非0错误码
 */
int openjiuwen_verify_connection(const protocol_adapter_t* adapter);

/**
 * @brief 获取OpenJiuwen支持的协议能力
 *
 * @param adapter 适配器实例
 * @param capabilities 输出的能力描述字符串
 * @param max_len 字符串缓冲区最大长度
 * @return 0成功，非0错误码
 */
int openjiuwen_get_capabilities(const protocol_adapter_t* adapter,
                                char* capabilities,
                                size_t max_len);

/* ============================================================================
 * 协议转换函数（内部使用）
 * ============================================================================ */

/**
 * @brief 将unified_message转换为OpenJiuwen格式
 *
 * @param msg 统一消息格式
 * @param out_buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际写入的字节数，错误返回负值
 */
int openjiuwen_unified_to_native(const unified_message_t* msg,
                                 void* out_buffer,
                                 size_t buffer_size);

/**
 * @brief 将OpenJiuwen格式转换为unified_message
 *
 * @param in_buffer 输入缓冲区
 * @param buffer_size 输入数据大小
 * @param msg 输出的统一消息格式
 * @return 0成功，非0错误码
 */
int openjiuwen_native_to_unified(const void* in_buffer,
                                 size_t buffer_size,
                                 unified_message_t* msg);

/* ============================================================================
 * 全局接口实例（供外部注册使用）
 * ============================================================================ */

/**
 * @brief OpenJiuwen适配器的标准接口定义
 *
 * 此全局变量可直接传递给unified_protocol_register_adapter()进行注册
 */
extern const protocol_adapter_t openjiuwen_adapter_interface;

#ifdef __cplusplus
}
#endif

#endif /* OPENJIUWEN_ADAPTER_H */