/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * @file gateway_forward.h
 * @brief C-L11: gateway_d → gateway 协议路由转发放接口
 *
 * 实现外部 HTTP 请求通过 gateway → gateway_d → 协议路由 → 目标 daemon
 * 的完整转发链路，支持 A2A / MCP / OpenAI 兼容三种协议路径。
 */

#ifndef AGENTOS_GATEWAY_FORWARD_H
#define AGENTOS_GATEWAY_FORWARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 转发协议类型 ==================== */

typedef enum {
    GW_FWD_PROTO_A2A = 0,   /**< A2A Agent-to-Agent 协议 */
    GW_FWD_PROTO_MCP,       /**< MCP Model Context Protocol */
    GW_FWD_PROTO_OPENAI,    /**< OpenAI 兼容 API */
    GW_FWD_PROTO_JSONRPC,   /**< JSON-RPC 通用协议 */
    GW_FWD_PROTO_COUNT
} gw_fwd_proto_t;

/* ==================== 转发配置 ==================== */

typedef struct {
    /** 目标 daemon 映射 */
    const char *a2a_target_daemon;      /**< A2A 目标 daemon（默认 "agent_d"） */
    const char *mcp_target_daemon;      /**< MCP 目标 daemon（默认 "tool_d"） */
    const char *openai_target_daemon;   /**< OpenAI 目标 daemon（默认 "llm_d"） */
    const char *jsonrpc_target_daemon;  /**< JSON-RPC 目标 daemon（默认 "sched_d"） */

    /** IPC 通道名称 */
    const char *a2a_channel;
    const char *mcp_channel;
    const char *openai_channel;
    const char *jsonrpc_channel;

    /** 超时设置（毫秒） */
    uint32_t request_timeout_ms;

    /** 是否启用转发统计 */
    bool enable_stats;
} gw_forward_config_t;

/** 默认转发配置 */
#define GW_FORWARD_CONFIG_DEFAULTS                                             \
    {                                                                          \
        .a2a_target_daemon = "agent_d", .mcp_target_daemon = "tool_d",        \
        .openai_target_daemon = "llm_d", .jsonrpc_target_daemon = "sched_d",  \
        .a2a_channel = "a2a", .mcp_channel = "mcp", .openai_channel = "llm",  \
        .jsonrpc_channel = "sched", .request_timeout_ms = 30000,              \
        .enable_stats = true                                                  \
    }

/* ==================== 转发统计 ==================== */

typedef struct {
    uint64_t total_forwarded;
    uint64_t by_proto[GW_FWD_PROTO_COUNT];
    uint64_t forward_errors;
    uint64_t timeout_errors;
    uint64_t avg_latency_us;
    /* C-L11: 扩展统计字段 */
    uint64_t max_latency_us;        /**< 最大延迟（微秒） */
    uint64_t min_latency_us;        /**< 最小延迟（微秒，0=未初始化） */
    uint64_t last_latency_us;       /**< 最近一次延迟（微秒） */
    uint64_t last_forward_time;     /**< 最近一次转发的时间戳（秒） */
    uint64_t body_size_total;       /**< 累计请求体大小（字节） */
    uint64_t response_size_total;   /**< 累计响应体大小（字节） */
} gw_forward_stats_t;

/* ==================== 转发句柄 ==================== */

typedef struct gw_forward_s gw_forward_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief C-L11: 创建协议转发器
 * @param config 转发配置（NULL 使用默认）
 * @return 转发器句柄，失败返回 NULL
 */
gw_forward_t *gw_forward_create(const gw_forward_config_t *config);

/**
 * @brief C-L11: 销毁协议转发器
 * @param fw 转发器句柄
 */
void gw_forward_destroy(gw_forward_t *fw);

/* ==================== 协议转发 ==================== */

/**
 * @brief C-L11.1: 根据协议类型转发请求到目标 daemon
 *
 * 外部 HTTP 请求 → gateway → gateway_d → 协议路由 → 对应 daemon
 *
 * @param fw 转发器句柄
 * @param proto 检测到的协议类型
 * @param method HTTP 方法（GET/POST）
 * @param path 请求路径
 * @param body 请求体（JSON）
 * @param body_len 请求体长度
 * @param out_response 输出响应 JSON（调用者释放）
 * @param out_response_len 输出响应长度
 * @return 0 成功，非 0 失败
 */
int gw_forward_request(gw_forward_t *fw, gw_fwd_proto_t proto, const char *method,
                       const char *path, const char *body, size_t body_len,
                       char **out_response, size_t *out_response_len);

/**
 * @brief C-L11.2: A2A 协议转发路径
 *
 * Agent-to-Agent 请求 → IPC Bus → agent_d
 */
int gw_forward_a2a(gw_forward_t *fw, const char *method, const char *path,
                   const char *body, size_t body_len, char **out_response,
                   size_t *out_response_len);

/**
 * @brief C-L11.3: MCP 协议转发路径
 *
 * MCP 请求 → IPC Bus → tool_d / llm_d
 */
int gw_forward_mcp(gw_forward_t *fw, const char *method, const char *path,
                   const char *body, size_t body_len, char **out_response,
                   size_t *out_response_len);

/**
 * @brief C-L11.4: OpenAI 兼容转发路径
 *
 * OpenAI API 请求 → IPC Bus → llm_d
 */
int gw_forward_openai(gw_forward_t *fw, const char *method, const char *path,
                      const char *body, size_t body_len, char **out_response,
                      size_t *out_response_len);

/* ==================== 协议检测辅助 ==================== */

/**
 * @brief 从 Content-Type + Path + Body 检测协议类型
 * @param content_type HTTP Content-Type
 * @param path 请求路径
 * @param body 请求体
 * @param body_len 请求体长度
 * @return 检测到的协议类型
 */
gw_fwd_proto_t gw_forward_detect_proto(const char *content_type, const char *path,
                                       const char *body, size_t body_len);

/* ==================== 统计 ==================== */

/**
 * @brief 获取转发统计
 */
int gw_forward_get_stats(gw_forward_t *fw, gw_forward_stats_t *stats);

/**
 * @brief 重置转发统计
 */
void gw_forward_reset_stats(gw_forward_t *fw);

/**
 * @brief 检查转发器健康状态
 */
bool gw_forward_is_healthy(gw_forward_t *fw);

/**
 * @brief C-L11: 输出转发统计摘要（单行格式，适合周期性日志）
 *
 * 格式: "C-L11: STATS total=N (A2A=N MCP=N OpenAI=N JSONRPC=N) "
 *        "errors=N timeouts=N latency=avg/max/min us "
 *        "throughput=req/s body=N resp=N"
 *
 * @param fw 转发器句柄
 * @param interval_sec 统计周期（秒），用于计算吞吐量
 */
void gw_forward_dump_stats(gw_forward_t *fw, uint32_t interval_sec);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_GATEWAY_FORWARD_H */