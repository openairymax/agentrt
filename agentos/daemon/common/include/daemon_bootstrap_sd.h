// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file daemon_bootstrap_sd.h
 * @brief P1.7 C-L08: daemon ServiceDiscovery 一键引导模块
 *
 * 每个 daemon 启动时调用此模块即可自动完成：
 * 1. ServiceDiscovery 初始化
 * 2. 服务注册（name/type/host/port/tags/ttl）
 * 3. 心跳线程启动
 * 4. 关闭时自动注销
 *
 * 使用方式（典型 daemon main()）：
 * @code
 *   #include "daemon_bootstrap_sd.h"
 *
 *   // 1. 引导服务发现
 *   daemon_bootstrap_sd_t *bsd = daemon_bootstrap_sd_start(
 *       "llm_d", "llm", "127.0.0.1", 8080, "ai,core", 0);
 *
 *   // ... daemon 主循环 ...
 *
 *   // 2. 关闭时自动注销
 *   daemon_bootstrap_sd_stop(bsd);
 * @endcode
 *
 * @see service_discovery_helper.h
 * @see P1.7 C-L08 连接线
 */

#ifndef AGENTOS_DAEMON_BOOTSTRAP_SD_H
#define AGENTOS_DAEMON_BOOTSTRAP_SD_H

#include "service_discovery_helper.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 引导句柄 ==================== */

typedef struct daemon_bootstrap_sd_s daemon_bootstrap_sd_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 一键引导：初始化 SD + 注册服务 + 启动心跳
 *
 * @param name        服务名称（如 "llm_d"）
 * @param type        服务类型（如 "llm"）
 * @param host        监听地址（如 "127.0.0.1"），NULL 使用 Unix socket
 * @param port        监听端口，0 表示使用 Unix socket
 * @param tags        标签（逗号分隔，如 "ai,core"，NULL 表示无）
 * @param ttl_ms      心跳 TTL（毫秒），0 使用默认 30000
 * @return 引导句柄，失败返回 NULL
 */
daemon_bootstrap_sd_t *daemon_bootstrap_sd_start(const char *name, const char *type,
                                                  const char *host, uint16_t port,
                                                  const char *tags, uint32_t ttl_ms);

/**
 * @brief 一键引导（Unix Socket 版本）
 *
 * @param name        服务名称
 * @param type        服务类型
 * @param socket_path Unix socket 路径
 * @param tags        标签
 * @param ttl_ms      心跳 TTL
 * @return 引导句柄，失败返回 NULL
 */
daemon_bootstrap_sd_t *daemon_bootstrap_sd_start_unix(const char *name, const char *type,
                                                       const char *socket_path,
                                                       const char *tags, uint32_t ttl_ms);

/**
 * @brief 停止服务发现引导（注销服务 + 停止心跳 + 释放资源）
 *
 * @param bsd 引导句柄
 */
void daemon_bootstrap_sd_stop(daemon_bootstrap_sd_t *bsd);

/* ==================== 查询 ==================== */

/**
 * @brief 获取底层 sd_helper 句柄（用于高级操作如 sd_helper_find/select）
 *
 * @param bsd 引导句柄
 * @return sd_helper 句柄，NULL 如果未引导
 */
sd_helper_t *daemon_bootstrap_sd_get_helper(daemon_bootstrap_sd_t *bsd);

/**
 * @brief 检查引导是否成功运行中
 *
 * @param bsd 引导句柄
 * @return true 运行中
 */
bool daemon_bootstrap_sd_is_running(daemon_bootstrap_sd_t *bsd);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_DAEMON_BOOTSTRAP_SD_H */