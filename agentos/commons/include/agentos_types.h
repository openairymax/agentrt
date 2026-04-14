/*
 * agentos_types.h - AgentOS 统一类型定义权威源
 * 
 * 作为全项目唯一的类型定义权威源，解决模块间类型定义冲突。
 * 遵循标准化统一方案，确保跨平台编译兼容性。
 * 
 * 设计原则：
 * 1. 权威性：commons作为唯一权威基础库
 * 2. 统一性：全项目使用统一的类型定义和接口契约
 * 3. 兼容性：确保Windows、Linux、macOS三平台兼容
 * 
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AGENTOS_UNIFIED_TYPES_H
#define AGENTOS_UNIFIED_TYPES_H

/* ==================== 平台检测和基础定义 ==================== */
#include "../platform/include/platform.h"

/* ==================== 统一的基础类型定义 ==================== */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 统一的错误码定义（权威源） ==================== */
/**
 * @brief 错误码类型
 * @details 所有错误码为负值，成功为0
 */
typedef int32_t agentos_error_t;

/**
 * @brief 成功返回值
 */
#define AGENTOS_SUCCESS         0

/**
 * @brief 通用错误码定义（权威定义）
 */
#define AGENTOS_EINVAL         (-1)    /**< 参数无效 */
#define AGENTOS_ENOMEM         (-2)    /**< 内存不足 */
#define AGENTOS_EBUSY          (-3)    /**< 资源忙碌 */
#define AGENTOS_ENOENT         (-4)    /**< 资源不存在 */
#define AGENTOS_EPERM          (-5)    /**< 权限不足 */
#define AGENTOS_ETIMEDOUT      (-6)    /**< 操作超时 */
#define AGENTOS_EIO            (-7)    /**< I/O 错误 */
#define AGENTOS_EEXIST         (-8)    /**< 资源已存在 */
#define AGENTOS_ENOTINIT       (-9)    /**< 引擎未初始化 */
#define AGENTOS_ECANCELLED     (-10)   /**< 操作已取消 */
#define AGENTOS_ENOTSUP        (-11)   /**< 操作不支持 */
#define AGENTOS_EOVERFLOW      (-12)   /**< 溢出错误 */
#define AGENTOS_EPROTO         (-13)   /**< 协议错误 */
#define AGENTOS_ENOTCONN       (-14)   /**< 未连接 */
#define AGENTOS_ECONNRESET     (-15)   /**< 连接重置 */
#define AGENTOS_EUNKNOWN       (-99)   /**< 未知错误 */

/* ==================== 统一的同步原语类型（来自platform.h） ==================== */
/* 
 * 以下类型在platform.h中定义，此处仅作声明引用：
 * - agentos_thread_t
 * - agentos_thread_id_t
 * - agentos_mutex_t
 * - agentos_cond_t
 * - agentos_socket_t
 * - agentos_process_t
 * - agentos_pid_t
 */

/* ==================== 统一的IPC类型定义（解决冲突） ==================== */
/**
 * @section IPC类型架构说明
 * 
 * AgentOS采用**分层IPC架构**，遵循微内核设计原则（Liedtke微内核原则）：
 * 
 * **Level 1: 内核级IPC (Kernel-Level)**
 * - 类型：agentos_kernel_ipc_message_t
 * - 位置：corekern/include/ipc.h
 * - 用途：微内核内部进程间通信
 * - 特点：
 *   ✓ 轻量级结构（40字节）：code, data, size, fd, msg_id
 *   ✓ 零外部依赖（不依赖commons）
 *   ✓ 极致性能（微秒级延迟）
 *   ✓ 简单易用（适合内核态编程）
 *   
 * **Level 2: 应用级IPC (Application-Level)**  
 * - 类型：agentos_ipc_message_t + agentos_ipc_header_t
 * - 位置：本文件（权威定义）
 * - 用途：跨模块、应用层、服务间通信
 * - 特点：
 *   ✓ 完整元数据（magic, version, source, target等）
 *   ✓ 标准化接口（支持序列化、校验和）
 *   ✓ 功能丰富（RPC、Pub/Sub、流式传输）
 *   ✓ 跨平台兼容（Windows/Linux/macOS）
 *   
 * **Level 3: IPC模块内部类型 (Implementation Detail)**
 * - 类型：ipc_message_t + ipc_message_header_t
 * - 位置：commons/utils/ipc/include/ipc_common.h
 * - 用途：IPC子系统内部实现
 * - 特点：包含实现细节字段（reserved等），不应在公共API中使用
 * 
 * **设计决策理由：**
 * 1. **微内核纯净性**：corekern不依赖任何外部库，保持最小化
 * 2. **性能优化**：内核级IPC避免不必要的内存拷贝和解析开销
 * 3. **职责分离**：内核关注机制，应用层关注策略和功能
 * 4. **向前兼容**：两级架构允许独立演进，不影响对方
 * 
 * **使用指南：**
 * - 在corekern模块内 → 使用 agentos_kernel_ipc_message_t
 * - 在daemon/services/应用层 → 使用 agentos_ipc_message_t
 * - 跨层通信 → 使用转换函数（见下方）
 */

/**
 * @brief IPC消息头结构（权威定义）
 */
typedef struct {
    uint32_t magic;                 /**< 魔数 (0x414F5350 = "AOSP") */
    uint32_t version;               /**< 协议版本 */
    uint32_t type;                  /**< 消息类型 */
    uint32_t flags;                 /**< 消息标志 */
    uint64_t msg_id;                /**< 消息ID */
    uint64_t correlation_id;        /**< 关联ID（请求-响应模式） */
    char source[64];                /**< 发送者标识 */
    char target[64];                /**< 目标标识 */
    uint32_t payload_len;           /**< 负载长度 */
    uint32_t checksum;              /**< 校验和 */
    uint64_t timestamp;             /**< 时间戳（纳秒） */
} agentos_ipc_header_t;

/**
 * @brief 应用级IPC消息结构（权威定义）
 * @note 这是应用层标准的agentos_ipc_message_t定义，与内核级agentos_kernel_ipc_message_t区分
 */
typedef struct {
    agentos_ipc_header_t header;    /**< 消息头 */
    void* payload;                  /**< 负载数据 */
    size_t payload_size;            /**< 负载大小 */
} agentos_ipc_message_t;

/* ==================== IPC类型转换函数（跨层通信支持） ==================== */
/*
 * 内核级IPC消息类型说明：
 * 
 * 类型名：agentos_kernel_ipc_message_t
 * 定义位置：corekern/include/ipc.h
 * 用途：微内核内部进程间通信（轻量级、高性能）
 * 
 * 使用场景：
 * - 当daemon服务需要将应用级消息转换为内核级消息时
 * - 当需要在不同IPC层次间桥接时
 * 
 * 注意：此类型仅在corekern模块内使用，应用层应使用agentos_ipc_message_t
 */

/* ==================== 统一的任务相关类型 ==================== */
/**
 * @brief 任务ID类型
 */
typedef uint64_t agentos_task_id_t;

/**
 * @brief 消息ID类型
 */
typedef uint64_t agentos_message_id_t;

/* ==================== 统一的函数接口契约 ==================== */
/*
 * 函数接口契约标准：
 * 1. 所有平台相关函数返回int类型（0成功，负数错误码）
 * 2. 参数顺序：输出参数在前，输入参数在后（遵循C语言惯例）
 * 3. 错误处理：使用统一的错误码定义
 */

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UNIFIED_TYPES_H */