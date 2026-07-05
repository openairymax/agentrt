/**
 * @file are_ipc.h
 * @brief ARE (Airymax Runtime Engineering) L2 统一 IPC 消息头规范
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @section 架构定位
 * 本头文件定义 **ARE L2 服务通信协议** 的 128 字节定长消息头
 * `are_ipc_message_header_t`，是 Airymax 0.1.1 内所有 daemon 间 IPC
 * 通信的统一 ABI。
 *
 * **与 L1 内核 IPC 的关系**：
 * - L1（`ipc.h` 的 `agentrt_kernel_ipc_message_t`，40 字节）：
 *   微内核内部高性能通信，零依赖，5 字段
 * - L2（本文件 `are_ipc_message_header_t`，128 字节）：
 *   daemon 间标准化通信，14 字段，含 magic/version/trace_id/checksum
 * - L2 消息在 L1 通道上传输时，整个 header + payload 作为 L1
 *   `agentrt_kernel_ipc_message_t.data` 传入；接收方按 magic 字段
 *   判别协议版本
 *
 * **设计原则**：
 * - 定长 ABI：128 字节固定不变，跨版本二进制兼容
 * - 自描述：magic + version + protocol 让接收方独立判别格式
 * - 可观测：trace_id 跨进程贯穿，全链路追踪零遗漏
 * - 完整性：CRC32 校验覆盖 header[0:116] + payload
 *
 * @see Docs/Capital_Specifications/are_standards/L2_service_protocol.md
 */

#ifndef AGENTRT_ARE_IPC_H
#define AGENTRT_ARE_IPC_H

#include "agentrt_time.h"
#include "error.h"
#include "export.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 常量定义
 * ================================================================ */

/**
 * @brief Magic 字段值，big-endian ASCII = 'A','R','E','1'
 *
 * 接收方必须先校验 magic，不匹配则丢弃消息并记录 WARN。
 */
#define ARE_IPC_MAGIC 0x41524531u

/**
 * @brief 当前协议版本
 *
 * v1 为 Airymax 0.1.1 唯一版本。破坏性变更需递增并发布迁移说明。
 */
#define ARE_IPC_VERSION 1u

/**
 * @brief 消息头定长（字节）
 *
 * 禁止变更（影响所有 ABI）。`_Static_assert` 在编译期强制验证。
 */
#define ARE_IPC_HEADER_SIZE 128u

/**
 * @brief source/target 字段最大长度（含终结符）
 */
#define ARE_IPC_MAX_SOURCE_LEN 32u
#define ARE_IPC_MAX_TARGET_LEN 32u

/**
 * @brief 单条消息最大 payload（512 KiB）
 *
 * 超过此值的 payload 必须分片（使用 ARE_FLAG_STREAMING）。
 */
#define ARE_IPC_MAX_PAYLOAD (512u * 1024u)

/* ================================================================
 * 消息类型（msg_type 字段取值）
 * ================================================================ */

/**
 * @brief L2 消息类型枚举
 *
 * REQUEST/RESPONSE 为请求-响应模式；NOTIFY 为单向通知；
 * ERROR 替代 RESPONSE 表示失败响应。
 */
typedef enum {
    ARE_MSG_REQUEST  = 0, /**< 请求-响应模式的请求 */
    ARE_MSG_RESPONSE = 1, /**< 请求-响应模式的响应 */
    ARE_MSG_NOTIFY   = 2, /**< 单向通知（无响应） */
    ARE_MSG_ERROR    = 3, /**< 错误响应（替代 RESPONSE 时表示失败） */
} are_msg_type_t;

/* ================================================================
 * 协议字段（protocol 字段取值）
 * ================================================================ */

/**
 * @brief payload 序列化协议枚举
 *
 * 指示 payload 的序列化协议，决定接收方反序列化器选择。
 */
typedef enum {
    ARE_PROTO_JSON_RPC = 0, /**< JSON-RPC 2.0，默认且强制支持 */
    ARE_PROTO_MCP      = 1, /**< Model Context Protocol */
    ARE_PROTO_A2A      = 2, /**< Agent-to-Agent Protocol */
    ARE_PROTO_OPENAI   = 3, /**< OpenAI 兼容 API */
    ARE_PROTO_CUSTOM   = 4, /**< 实现者自定义（payload 自描述） */
} are_proto_t;

/* ================================================================
 * flags 位定义
 * ================================================================ */

#define ARE_FLAG_COMPRESSED     0x0001u /**< payload 经 zlib 压缩 */
#define ARE_FLAG_ENCRYPTED      0x0002u /**< payload 经 TLS/AES-GCM 加密 */
#define ARE_FLAG_STREAMING      0x0004u /**< 流式消息分片（需重组） */
#define ARE_FLAG_DROPPABLE      0x0008u /**< 背压时可丢弃（日志/指标类） */
#define ARE_FLAG_IDEMPOTENT     0x0010u /**< 幂等请求，可安全重试 */
#define ARE_FLAG_TRACE_SAMPLED  0x0020u /**< 该 trace 已被采样 */

/* ================================================================
 * 128 字节消息头结构体
 * ================================================================ */

/**
 * @brief L2 统一 IPC 消息头（128 字节定长）
 *
 * @section 字段布局
 * | offset | 字段           | 类型     | 说明 |
 * |--------|----------------|----------|------|
 * |   0    | magic          | uint32   | 0x41524531 ("ARE1") |
 * |   4    | version        | uint16   | 协议版本，当前 = 1 |
 * |   6    | msg_type       | uint16   | are_msg_type_t |
 * |   8    | protocol       | uint32   | are_proto_t |
 * |  12    | msg_id         | uint64   | 消息唯一 ID（请求方分配） |
 * |  20    | trace_id       | uint64   | 分布式追踪 ID，贯穿全链路 |
 * |  28    | correlation_id | uint64   | 请求-响应关联 ID |
 * |  36    | source[32]     | char[]   | 源 daemon 名（NULL-terminated） |
 * |  68    | target[32]     | char[]   | 目标 daemon 名 |
 * | 100    | payload_len    | uint32   | 负载字节数（≤ ARE_IPC_MAX_PAYLOAD） |
 * | 104    | flags          | uint32   | are_flag_t 位域 |
 * | 108    | timestamp_ns   | uint64   | 纳秒时间戳（CLOCK_REALTIME） |
 * | 116    | checksum       | uint32   | CRC32(header[0:120] + payload); checksum 字段以 0 参与计算 |
 * | 120    | reserved       | uint32   | 保留，必须为 0（未来扩展） |
 * | 124    | _abi_pad       | uint32   | ABI 对齐填充至 128 字节，必须为 0 |
 *
 * @section 紧凑布局说明
 * 规范要求 msg_id@12 / trace_id@20 / correlation_id@28 / timestamp_ns@108
 * 等 uint64 字段处于非自然对齐位置（8 字节对齐）。为实现规范定义的精确偏移，
 * 结构体使用 `#pragma pack(push, 1)` 强制字节对齐。这在 x86-64 / ARM64 上
 * 由硬件支持非对齐访问，性能影响可忽略；对 IPC 消息头（每消息一次访问）无影响。
 *
 * @section 一致性要求
 * - `sizeof(are_ipc_message_header_t) == 128`（`_Static_assert` 验证）
 * - `checksum` 计算范围：header[0:120)（magic ~ checksum，checksum 以 0 参与）+ payload；
 *   reserved 与 _abi_pad 不在 CRC 内，由 validate 独立检查 == 0
 * - magic/version/checksum 任一校验失败时必须丢弃消息，不得回复 ERROR
 * - source 与 target 必须以 `\0` 结尾，长度 ≤ 31 字节
 * - _abi_pad 与 reserved 必须为 0，校验失败返回 AGENTRT_ERR_PROTOCOL
 */
#pragma pack(push, 1)
typedef struct are_ipc_message_header {
    uint32_t magic;          /**< offset  0: 0x41524531 ("ARE1") */
    uint16_t version;        /**< offset  4: 协议版本，当前 = 1 */
    uint16_t msg_type;       /**< offset  6: are_msg_type_t */
    uint32_t protocol;       /**< offset  8: are_proto_t */
    uint64_t msg_id;         /**< offset 12: 消息唯一 ID（请求方分配） */
    uint64_t trace_id;       /**< offset 20: 分布式追踪 ID，贯穿全链路 */
    uint64_t correlation_id; /**< offset 28: 请求-响应关联 ID */
    char     source[ARE_IPC_MAX_SOURCE_LEN]; /**< offset 36: 源 daemon 名 */
    char     target[ARE_IPC_MAX_TARGET_LEN]; /**< offset 68: 目标 daemon 名 */
    uint32_t payload_len;    /**< offset 100: 负载字节数 */
    uint32_t flags;          /**< offset 104: are_flag_t 位域 */
    uint64_t timestamp_ns;   /**< offset 108: 纳秒时间戳（CLOCK_REALTIME） */
    uint32_t checksum;       /**< offset 116: CRC32(header[0:120] + payload) */
    uint32_t reserved;       /**< offset 120: 保留，必须为 0（未来扩展） */
    uint32_t _abi_pad;       /**< offset 124: ABI 填充至 128 字节，必须为 0 */
} are_ipc_message_header_t;
#pragma pack(pop)

/* 编译期验证：消息头必须为 128 字节（影响所有 ABI，禁止变更） */
_Static_assert(sizeof(are_ipc_message_header_t) == ARE_IPC_HEADER_SIZE,
               "are_ipc_message_header_t must be exactly 128 bytes (ARE L2 ABI)");

/* 编译期验证：字段偏移与规范一致 */
_Static_assert(offsetof(are_ipc_message_header_t, magic) == 0,
               "magic must be at offset 0");
_Static_assert(offsetof(are_ipc_message_header_t, version) == 4,
               "version must be at offset 4");
_Static_assert(offsetof(are_ipc_message_header_t, msg_type) == 6,
               "msg_type must be at offset 6");
_Static_assert(offsetof(are_ipc_message_header_t, protocol) == 8,
               "protocol must be at offset 8");
_Static_assert(offsetof(are_ipc_message_header_t, msg_id) == 12,
               "msg_id must be at offset 12");
_Static_assert(offsetof(are_ipc_message_header_t, trace_id) == 20,
               "trace_id must be at offset 20");
_Static_assert(offsetof(are_ipc_message_header_t, correlation_id) == 28,
               "correlation_id must be at offset 28");
_Static_assert(offsetof(are_ipc_message_header_t, source) == 36,
               "source must be at offset 36");
_Static_assert(offsetof(are_ipc_message_header_t, target) == 68,
               "target must be at offset 68");
_Static_assert(offsetof(are_ipc_message_header_t, payload_len) == 100,
               "payload_len must be at offset 100");
_Static_assert(offsetof(are_ipc_message_header_t, flags) == 104,
               "flags must be at offset 104");
_Static_assert(offsetof(are_ipc_message_header_t, timestamp_ns) == 108,
               "timestamp_ns must be at offset 108");
_Static_assert(offsetof(are_ipc_message_header_t, checksum) == 116,
               "checksum must be at offset 116");
_Static_assert(offsetof(are_ipc_message_header_t, reserved) == 120,
               "reserved must be at offset 120");
_Static_assert(offsetof(are_ipc_message_header_t, _abi_pad) == 124,
               "_abi_pad must be at offset 124");

/* ================================================================
 * 完整消息结构
 * ================================================================ */

/**
 * @brief L2 完整 IPC 消息（header + payload）
 *
 * payload 指针的所有权归调用者：发送方负责填充，接收方负责释放。
 * `payload_size` 是实际分配容量，必须 ≥ `header.payload_len`。
 */
typedef struct {
    are_ipc_message_header_t header; /**< 消息头（128 字节） */
    void    *payload;                /**< 调用者拥有所有权，长度 = header.payload_len */
    size_t   payload_size;           /**< 实际分配容量，≥ payload_len */
} are_ipc_message_t;

/* ================================================================
 * API：消息头生命周期
 * ================================================================ */

/**
 * @brief 初始化消息头为有效的 L2 REQUEST
 *
 * 填充 magic/version/timestamp_ns，source/target 通过 strncpy 安全拷贝，
 * checksum 置 0（调用方在填充完 payload 后调用 are_ipc_checksum_compute
 * 计算并写入 checksum）。
 *
 * @param header [in/out] 待初始化的消息头
 * @param msg_type [in] 消息类型（ARE_MSG_REQUEST/RESPONSE/NOTIFY/ERROR）
 * @param protocol [in] payload 协议（ARE_PROTO_JSON_RPC/MCP/A2A/OPENAI/CUSTOM）
 * @param msg_id [in] 消息唯一 ID（请求方分配，RESPONSE 必须 echo）
 * @param trace_id [in] 分布式追踪 ID（0 = 未启用追踪）
 * @param source [in] 源 daemon 名（NULL-terminated，≤ 31 字节；NULL → "unknown"）
 * @param target [in] 目标 daemon 名（NULL-terminated，≤ 31 字节；广播填 "*"；NULL → "*"）
 * @return AGENTRT_SUCCESS 成功；AGENTRT_EINVAL 参数无效
 *
 * @threadsafe 是（无共享状态）
 * @reentrant 是
 */
AGENTRT_API agentrt_error_t are_ipc_header_init(are_ipc_message_header_t *header,
                                                are_msg_type_t msg_type,
                                                are_proto_t protocol,
                                                uint64_t msg_id,
                                                uint64_t trace_id,
                                                const char *source,
                                                const char *target);

/* ================================================================
 * API：CRC32 校验
 * ================================================================ */

/**
 * @brief 计算 CRC32-IEEE 802.3（与 zlib/PNG 兼容）
 *
 * 覆盖范围：header[0:120)（即 magic ~ checksum 字段，共 120 字节；
 * checksum 字段以 0 参与计算）+ payload。
 * 不覆盖 reserved（offset 120-123）和 _abi_pad（offset 124-127），
 * 这两个字段由 are_ipc_header_validate 独立检查 == 0。
 *
 * @param header [in] 消息头（checksum 字段会被视为 0 参与计算，不修改原值）
 * @param payload [in] payload 数据（可为 NULL 若 payload_len=0）
 * @param payload_len [in] payload 字节数
 * @return uint32_t CRC32 校验和
 *
 * @threadsafe 是（纯函数）
 * @reentrant 是
 */
AGENTRT_API uint32_t are_ipc_checksum_compute(const are_ipc_message_header_t *header,
                                              const void *payload,
                                              size_t payload_len);

/**
 * @brief 校验消息完整性（magic + version + reserved + _abi_pad + checksum）
 *
 * 校验顺序：
 * 1. magic == ARE_IPC_MAGIC，否则返回 AGENTRT_ERR_PROTOCOL
 * 2. version == ARE_IPC_VERSION，否则返回 AGENTRT_ERR_PROTOCOL
 * 3. reserved == 0，否则返回 AGENTRT_ERR_PROTOCOL
 * 4. _abi_pad == 0，否则返回 AGENTRT_ERR_PROTOCOL
 * 5. payload_len ≤ ARE_IPC_MAX_PAYLOAD，否则返回 AGENTRT_ERR_INVALID_PARAM
 * 6. 调用方传入的 payload_len 必须与 header.payload_len 一致
 * 7. checksum 与重算结果一致，否则返回 AGENTRT_ERR_CHECKSUM
 *
 * @param header [in] 待校验的消息头
 * @param payload [in] payload 数据（可为 NULL 若 payload_len=0）
 * @param payload_len [in] payload 字节数
 * @return AGENTRT_SUCCESS 校验通过；AGENTRT_ERR_PROTOCOL magic/version/reserved/_abi_pad 错误；
 *         AGENTRT_ERR_INVALID_PARAM payload_len 超限或不一致；AGENTRT_ERR_CHECKSUM 校验和不匹配
 *
 * @threadsafe 是（纯函数）
 * @reentrant 是
 */
AGENTRT_API agentrt_error_t are_ipc_header_validate(const are_ipc_message_header_t *header,
                                                    const void *payload,
                                                    size_t payload_len);

/* ================================================================
 * API：完整消息生命周期
 * ================================================================ */

/**
 * @brief 创建完整 L2 消息（分配 header + payload）
 *
 * @param msg_type [in] 消息类型
 * @param protocol [in] payload 协议
 * @param msg_id [in] 消息唯一 ID
 * @param trace_id [in] 追踪 ID
 * @param source [in] 源 daemon 名
 * @param target [in] 目标 daemon 名
 * @param payload [in] payload 数据（可为 NULL；会被拷贝到内部缓冲区）
 * @param payload_len [in] payload 字节数
 * @param out_msg [out] 输出消息指针（调用者负责 are_ipc_message_destroy）
 * @return AGENTRT_SUCCESS 成功；AGENTRT_EINVAL 参数无效；AGENTRT_ENOMEM 内存不足
 *
 * @ownership out_msg 由调用者负责，通过 are_ipc_message_destroy() 释放
 * @threadsafe 是
 */
AGENTRT_API agentrt_error_t are_ipc_message_create(are_msg_type_t msg_type,
                                                   are_proto_t protocol,
                                                   uint64_t msg_id,
                                                   uint64_t trace_id,
                                                   const char *source,
                                                   const char *target,
                                                   const void *payload,
                                                   size_t payload_len,
                                                   are_ipc_message_t **out_msg);

/**
 * @brief 销毁 L2 消息（释放 header + payload）
 *
 * @param msg [in] 消息指针（NULL 安全）
 *
 * @threadsafe 是
 */
AGENTRT_API void are_ipc_message_destroy(are_ipc_message_t *msg);

/* ================================================================
 * API：辅助函数
 * ================================================================ */

/**
 * @brief 将消息类型转换为可读字符串（用于日志）
 *
 * @param msg_type [in] 消息类型
 * @return 字符串字面量（无需释放）；未知类型返回 "UNKNOWN"
 */
AGENTRT_API const char *are_ipc_msg_type_str(are_msg_type_t msg_type);

/**
 * @brief 将协议类型转换为可读字符串（用于日志）
 *
 * @param protocol [in] 协议类型
 * @return 字符串字面量（无需释放）；未知类型返回 "UNKNOWN"
 */
AGENTRT_API const char *are_ipc_proto_str(are_proto_t protocol);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_ARE_IPC_H */
