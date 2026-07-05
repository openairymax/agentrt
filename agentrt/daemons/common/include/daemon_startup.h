/**
 * @file daemon_startup.h
 * @brief daemon 启动顺序编排与依赖 DAG 定义
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 *
 * P1.23.1: 定义 12 daemon 的启动依赖 DAG（有向无环图）。
 *
 * 启动层级（Layer 越小越先启动）：
 *
 *   Layer 0 — 基础设施（无 daemon 依赖）
 *     monit_d, observe_d, info_d, notify_d
 *
 *   Layer 1 — 核心服务
 *     sched_d   → observe_d
 *     channel_d → notify_d
 *
 *   Layer 2 — Agent 服务
 *     llm_d    → sched_d
 *     tool_d   → llm_d, sched_d
 *     hook_d   → tool_d
 *     plugin_d → tool_d, hook_d
 *
 *   Layer 3 — 业务服务
 *     market_d → plugin_d
 *
 *   Layer 4 — 网关（依赖所有服务）
 *     gateway_d → llm_d, tool_d, market_d
 *
 * DAG 可视化：
 *
 *   monit_d ──┐
 *   observe_d ─┼──→ sched_d ──→ llm_d ──→ tool_d ──→ hook_d ──→ plugin_d ──→ market_d ──→ gateway_d
 *   info_d ────┤         └──────────────→ tool_d                      └──────────────→ gateway_d
 *   notify_d ──┼──→ channel_d                                               └──────────────→ gateway_d
 *              ┘
 */

#ifndef AGENTRT_DAEMON_STARTUP_H
#define AGENTRT_DAEMON_STARTUP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量 ==================== */

#define AGENTRT_DAEMON_COUNT     12
#define AGENTRT_MAX_DEPS_PER_DAEMON 4
#define AGENTRT_MAX_LAYERS       5

/* ==================== daemon 标识 ==================== */

typedef enum {
    AGENTRT_DAEMON_MONIT   = 0,
    AGENTRT_DAEMON_OBSERVE = 1,
    AGENTRT_DAEMON_INFO    = 2,
    AGENTRT_DAEMON_NOTIFY  = 3,
    AGENTRT_DAEMON_SCHED   = 4,
    AGENTRT_DAEMON_CHANNEL = 5,
    AGENTRT_DAEMON_LLM     = 6,
    AGENTRT_DAEMON_TOOL    = 7,
    AGENTRT_DAEMON_HOOK    = 8,
    AGENTRT_DAEMON_PLUGIN  = 9,
    AGENTRT_DAEMON_MARKET  = 10,
    AGENTRT_DAEMON_GATEWAY = 11,
    AGENTRT_DAEMON_INVALID = -1
} agentrt_daemon_id_t;

/* ==================== daemon 启动描述 ==================== */

typedef struct {
    agentrt_daemon_id_t id;          /**< daemon 标识 */
    const char *name;                /**< 进程名 (e.g. "gateway_d") */
    const char *service_type;        /**< 服务发现类型 (e.g. "gateway") */
    uint32_t layer;                  /**< 启动层级 (0=最先) */
    uint32_t health_timeout_ms;      /**< 健康检查超时 */
    uint32_t health_interval_ms;     /**< 健康检查轮询间隔 */
    uint16_t default_port;           /**< 默认端口 (0=Unix Socket) */
    int dep_count;                   /**< 依赖数量 */
    agentrt_daemon_id_t deps[AGENTRT_MAX_DEPS_PER_DAEMON]; /**< 依赖列表 */
} agentrt_daemon_desc_t;

/* ==================== DAG 表 ==================== */

/**
 * @brief 全局 daemon 描述表 — 按 layer 排序
 *
 * 启动编排器遍历此表，同 layer 内可并行启动，
 * 跨 layer 必须等待前一层所有 daemon 健康检查通过。
 */
static const agentrt_daemon_desc_t agentrt_daemon_table[AGENTRT_DAEMON_COUNT] = {
    /* --- Layer 0: 基础设施 --- */
    [AGENTRT_DAEMON_MONIT] = {
        .id = AGENTRT_DAEMON_MONIT,
        .name = "monit_d",
        .service_type = "monitor",
        .layer = 0,
        .health_timeout_ms = 15000,
        .health_interval_ms = 500,
        .default_port = 0,
        .dep_count = 0,
        .deps = {0},
    },
    [AGENTRT_DAEMON_OBSERVE] = {
        .id = AGENTRT_DAEMON_OBSERVE,
        .name = "observe_d",
        .service_type = "observability",
        .layer = 0,
        .health_timeout_ms = 15000,
        .health_interval_ms = 500,
        .default_port = 0,
        .dep_count = 0,
        .deps = {0},
    },
    [AGENTRT_DAEMON_INFO] = {
        .id = AGENTRT_DAEMON_INFO,
        .name = "info_d",
        .service_type = "info",
        .layer = 0,
        .health_timeout_ms = 15000,
        .health_interval_ms = 500,
        .default_port = 0,
        .dep_count = 0,
        .deps = {0},
    },
    [AGENTRT_DAEMON_NOTIFY] = {
        .id = AGENTRT_DAEMON_NOTIFY,
        .name = "notify_d",
        .service_type = "notification",
        .layer = 0,
        .health_timeout_ms = 15000,
        .health_interval_ms = 500,
        .default_port = 0,
        .dep_count = 0,
        .deps = {0},
    },

    /* --- Layer 1: 核心服务 --- */
    [AGENTRT_DAEMON_SCHED] = {
        .id = AGENTRT_DAEMON_SCHED,
        .name = "sched_d",
        .service_type = "scheduler",
        .layer = 1,
        .health_timeout_ms = 20000,
        .health_interval_ms = 500,
        .default_port = 0,
        .dep_count = 1,
        .deps = { AGENTRT_DAEMON_OBSERVE },
    },
    [AGENTRT_DAEMON_CHANNEL] = {
        .id = AGENTRT_DAEMON_CHANNEL,
        .name = "channel_d",
        .service_type = "channel",
        .layer = 1,
        .health_timeout_ms = 20000,
        .health_interval_ms = 500,
        .default_port = 0,
        .dep_count = 1,
        .deps = { AGENTRT_DAEMON_NOTIFY },
    },

    /* --- Layer 2: Agent 服务 --- */
    [AGENTRT_DAEMON_LLM] = {
        .id = AGENTRT_DAEMON_LLM,
        .name = "llm_d",
        .service_type = "llm",
        .layer = 2,
        .health_timeout_ms = 30000,
        .health_interval_ms = 1000,
        .default_port = 0,
        .dep_count = 1,
        .deps = { AGENTRT_DAEMON_SCHED },
    },
    [AGENTRT_DAEMON_TOOL] = {
        .id = AGENTRT_DAEMON_TOOL,
        .name = "tool_d",
        .service_type = "tool",
        .layer = 2,
        .health_timeout_ms = 30000,
        .health_interval_ms = 1000,
        .default_port = 8082,
        .dep_count = 2,
        .deps = { AGENTRT_DAEMON_LLM, AGENTRT_DAEMON_SCHED },
    },
    [AGENTRT_DAEMON_HOOK] = {
        .id = AGENTRT_DAEMON_HOOK,
        .name = "hook_d",
        .service_type = "hook",
        .layer = 2,
        .health_timeout_ms = 20000,
        .health_interval_ms = 500,
        .default_port = 0,
        .dep_count = 1,
        .deps = { AGENTRT_DAEMON_TOOL },
    },
    [AGENTRT_DAEMON_PLUGIN] = {
        .id = AGENTRT_DAEMON_PLUGIN,
        .name = "plugin_d",
        .service_type = "plugin",
        .layer = 2,
        .health_timeout_ms = 30000,
        .health_interval_ms = 1000,
        .default_port = 0,
        .dep_count = 2,
        .deps = { AGENTRT_DAEMON_TOOL, AGENTRT_DAEMON_HOOK },
    },

    /* --- Layer 3: 业务服务 --- */
    [AGENTRT_DAEMON_MARKET] = {
        .id = AGENTRT_DAEMON_MARKET,
        .name = "market_d",
        .service_type = "marketplace",
        .layer = 3,
        .health_timeout_ms = 30000,
        .health_interval_ms = 1000,
        .default_port = 0,
        .dep_count = 1,
        .deps = { AGENTRT_DAEMON_PLUGIN },
    },

    /* --- Layer 4: 网关 --- */
    [AGENTRT_DAEMON_GATEWAY] = {
        .id = AGENTRT_DAEMON_GATEWAY,
        .name = "gateway_d",
        .service_type = "gateway",
        .layer = 4,
        .health_timeout_ms = 30000,
        .health_interval_ms = 1000,
        .default_port = 8080,
        .dep_count = 3,
        .deps = { AGENTRT_DAEMON_LLM, AGENTRT_DAEMON_TOOL, AGENTRT_DAEMON_MARKET },
    },
};

/* ==================== 查询 API ==================== */

/**
 * @brief 根据 daemon 名称查找描述
 * @param name daemon 进程名 (e.g. "gateway_d")
 * @return 描述指针，未找到返回 NULL
 */
static inline const agentrt_daemon_desc_t *agentrt_daemon_find_by_name(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < AGENTRT_DAEMON_COUNT; i++) {
        if (agentrt_daemon_table[i].name &&
            __builtin_strcmp(agentrt_daemon_table[i].name, name) == 0) {
            return &agentrt_daemon_table[i];
        }
    }
    return NULL;
}

/**
 * @brief 获取指定 layer 的 daemon 数量
 */
static inline int agentrt_daemon_count_in_layer(uint32_t layer)
{
    int count = 0;
    for (int i = 0; i < AGENTRT_DAEMON_COUNT; i++) {
        if (agentrt_daemon_table[i].layer == layer)
            count++;
    }
    return count;
}

/**
 * @brief 获取最大 layer 值
 */
static inline uint32_t agentrt_daemon_max_layer(void)
{
    uint32_t max_l = 0;
    for (int i = 0; i < AGENTRT_DAEMON_COUNT; i++) {
        if (agentrt_daemon_table[i].layer > max_l)
            max_l = agentrt_daemon_table[i].layer;
    }
    return max_l;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_DAEMON_STARTUP_H */
