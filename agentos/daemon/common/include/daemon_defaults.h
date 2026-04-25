/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file daemon_defaults.h
 * @brief 守护进程共享默认值集中定义
 *
 * 将分散在各daemon模块中的硬编码超时/重试/缓冲区/端口等默认值
 * 统一到此处，便于全局调整策略和运维配置。
 */

#ifndef AGENTOS_DAEMON_DEFAULTS_H
#define AGENTOS_DAEMON_DEFAULTS_H

/* ========== 超时默认值 ========== */

#define AGENTOS_DEFAULT_TIMEOUT_MS          30000
#define AGENTOS_DEFAULT_TIMEOUT_SEC         30
#define AGENTOS_SHUTDOWN_TIMEOUT_MS         5000
#define AGENTOS_HEALTHCHECK_INTERVAL_MS     5000
#define AGENTOS_HEARTBEAT_INTERVAL_MS       30000
#define AGENTOS_SOCKET_ACCEPT_TIMEOUT_MS    5000
#define AGENTOS_CONNECT_TIMEOUT_MS          10000

/* ========== 重试默认值 ========== */

#define AGENTOS_DEFAULT_MAX_RETRIES         3
#define AGENTOS_DEFAULT_RETRY_DELAY_MS      100
#define AGENTOS_DEFAULT_BACKOFF_FACTOR      2
#define AGENTOS_DEFAULT_JITTER_RATIO        10

/* ========== 缓冲区大小 ========== */

#define AGENTOS_DEFAULT_RECV_BUFFER         65536
#define AGENTOS_DEFAULT_COMMAND_BUFFER      4096
#define AGENTOS_DEFAULT_OUTPUT_BUFFER       4096
#define AGENTOS_MAX_REQUEST_SIZE_HTTP       (10 * 1024 * 1024)
#define AGENTOS_MAX_REQUEST_SIZE_WS         (10 * 1024 * 1024)
#define AGENTOS_MAX_REQUEST_SIZE_STDIO      (1 * 1024 * 1024)

/* ========== 并发/线程 ========== */

#define AGENTOS_DEFAULT_MAX_WORKERS         4
#define AGENTOS_DEFAULT_MAX_CLIENTS         64
#define AGENTOS_DEFAULT_MAX_CONCURRENT      1000
#define AGENTOS_DEFAULT_THREAD_POOL_SIZE    8

/* ========== 缓存 ========== */

#define AGENTOS_DEFAULT_CACHE_CAPACITY      1024
#define AGENTOS_DEFAULT_CACHE_TTL_SEC       3600

/* ========== 端口/路径 ========== */

#define AGENTOS_DEFAULT_HTTP_PORT           8080
#define AGENTOS_DEFAULT_WS_PORT             8081
#define AGENTOS_DEFAULT_TOOL_PORT           8082
#define AGENTOS_DEFAULT_LLM_SOCK_PATH       "/var/run/agentos/llm.sock"
#define AGENTOS_DEFAULT_TOOL_SOCK_PATH      "/var/run/agentos/tool.sock"

/* ========== 安全/认证 ========== */

#define AGENTOS_DEFAULT_TOKEN_TTL_SEC       3600
#define AGENTOS_DEFAULT_REFRESH_THRESHOLD   300
#define AGENTOS_DEFAULT_RPS_LIMIT           100
#define AGENTOS_DEFAULT_BURST_SIZE          20

/* ========== 熔断器 ========== */

#define AGENTOS_CB_FAILURE_THRESHOLD        5
#define AGENTOS_CB_SUCCESS_THRESHOLD        3
#define AGENTOS_CB_HALF_OPEN_MAX            1
#define AGENTOS_CB_WINDOW_SIZE_MS           60000
#define AGENTOS_CB_SLOW_CALL_MS             5000
#define AGENTOS_CB_SLOW_CALL_RATE_PCT       50
#define AGENTOS_CB_FAILURE_RATE_PCT         50
#define AGENTOS_CB_TIMEOUT_MS               30000

/* ========== API恢复 ========== */

#define AGENTOS_API_REC_MAX_RETRY           5
#define AGENTOS_API_REC_BASE_DELAY_MS       200
#define AGENTOS_API_REC_BACKOFF_FACTOR      2.0f
#define AGENTOS_API_REC_JITTER_PCT          10
#define AGENTOS_API_REC_HEALTH_DECAY        0.9f
#define AGENTOS_API_REC_HEALTH_PENALTY      0.3f
#define AGENTOS_API_REC_HEALTH_MIN          0.2f
#define AGENTOS_API_REC_CONSECUTIVE_DISABLE 5

/* ========== 监控/告警 ========== */

#define AGENTOS_MONITOR_INTERVAL_MS         30000
#define AGENTOS_ALERT_EVAL_INTERVAL_MS      10000
#define AGENTOS_ALERT_COOLDOWN_MS           60000
#define AGENTOS_ALERT_ESCALATION_MS         300000
#define AGENTOS_ALERT_MAX_NOTIFICATIONS     10

/* ========== 配置 ========== */

#define AGENTOS_CONFIG_WATCH_INTERVAL_MS    5000
#define AGENTOS_VAULT_AUTO_LOCK_SEC         300
#define AGENTOS_VAULT_MAX_RETRIES           3
#define AGENTOS_VAULT_MAX_CHAIN_DEPTH       5

#endif /* AGENTOS_DAEMON_DEFAULTS_H */
