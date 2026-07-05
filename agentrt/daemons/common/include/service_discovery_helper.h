// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file service_discovery_helper.h
 * @brief C-L08: ServiceDiscovery → daemon 自动注册便捷层
 *
 * 每个 daemon 启动时调用此模块的便捷 API 实现自动注册和心跳。
 * 基于 service_discovery.h 的核心 API 封装，简化 daemon 集成。
 *
 * 使用方式（典型 daemon main()）：
 * @code
 *   // 1. 初始化服务发现
 *   sd_helper_t *sdh = sd_helper_init(NULL);
 *
 *   // 2. 自动注册
 *   sd_helper_register(sdh, "llm_d", "llm", "127.0.0.1", 8080,
 *                      "ai,core", 10000);
 *
 *   // 3. 启动心跳线程
 *   sd_helper_start_heartbeat(sdh);
 *
 *   // ... daemon 主循环 ...
 *
 *   // 4. 关闭时自动注销
 *   sd_helper_shutdown(sdh);
 * @endcode
 *
 * @see service_discovery.h
 * @see P1.7 C-L08 连接线
 */

#ifndef AGENTRT_SERVICE_DISCOVERY_HELPER_H
#define AGENTRT_SERVICE_DISCOVERY_HELPER_H

#include "service_discovery.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 便捷类型 ==================== */

/**
 * @brief 服务发现助手句柄
 *
 * 封装了 service_discovery 实例、心跳线程和注册信息。
 */
typedef struct sd_helper_s sd_helper_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 初始化服务发现助手
 *
 * 创建 service_discovery 实例并启动后台管理。
 *
 * @param config 服务发现配置（NULL 使用默认配置）
 * @return 助手句柄，失败返回 NULL
 */
sd_helper_t *sd_helper_init(const sd_config_t *config);

/**
 * @brief 关闭服务发现助手
 *
 * 自动注销已注册的服务，停止心跳，释放资源。
 *
 * @param sdh 助手句柄
 */
void sd_helper_shutdown(sd_helper_t *sdh);

/* ==================== 服务注册（P1.7.1） ==================== */

/**
 * @brief 注册当前 daemon 到服务发现
 *
 * 自动生成 instance_id（基于 host:port），填充 sd_instance_t 并调用 sd_register。
 *
 * @param sdh      助手句柄
 * @param name     服务名称（如 "llm_d"）
 * @param type     服务类型（如 "llm"）
 * @param host     监听地址（如 "127.0.0.1"）
 * @param port     监听端口（如 8080）
 * @param tags     标签（逗号分隔，如 "ai,core"，NULL 表示无标签）
 * @param ttl_ms   心跳 TTL（毫秒），0 使用默认值 30000
 * @return 0 成功，非0 失败
 */
int sd_helper_register(sd_helper_t *sdh, const char *name, const char *type,
                       const char *host, uint16_t port, const char *tags,
                       uint32_t ttl_ms);

/**
 * @brief 注册当前 daemon 到服务发现（Unix Socket 版本）
 *
 * @param sdh         助手句柄
 * @param name        服务名称
 * @param type        服务类型
 * @param socket_path Unix socket 路径
 * @param tags        标签
 * @param ttl_ms      心跳 TTL
 * @return 0 成功，非0 失败
 */
int sd_helper_register_unix(sd_helper_t *sdh, const char *name, const char *type,
                            const char *socket_path, const char *tags,
                            uint32_t ttl_ms);

/* ==================== 心跳管理（P1.7.2） ==================== */

/**
 * @brief 启动后台心跳线程
 *
 * 按配置的 heartbeat_interval_ms 定期发送心跳。
 * 心跳线程在 sd_helper_shutdown 时自动停止。
 *
 * @param sdh 助手句柄
 * @return 0 成功，非0 失败
 */
int sd_helper_start_heartbeat(sd_helper_t *sdh);

/**
 * @brief 停止心跳线程
 *
 * @param sdh 助手句柄
 */
void sd_helper_stop_heartbeat(sd_helper_t *sdh);

/**
 * @brief 手动发送一次心跳
 *
 * @param sdh 助手句柄
 * @return 0 成功，非0 失败
 */
int sd_helper_send_heartbeat(sd_helper_t *sdh);

/* ==================== 服务发现（P1.7.3） ==================== */

/**
 * @brief 发现可用服务实例
 *
 * 便捷封装 sd_discover，返回可用实例列表。
 *
 * @param sdh          助手句柄
 * @param service_name 服务名称
 * @param instances    输出实例数组
 * @param max_count    最大数量
 * @param found_count  实际找到数量
 * @return 0 成功，非0 失败
 */
int sd_helper_find(sd_helper_t *sdh, const char *service_name,
                   sd_instance_t *instances, uint32_t max_count,
                   uint32_t *found_count);

/* ==================== 负载均衡选择（P1.7.4） ==================== */

/**
 * @brief 选择最优服务实例
 *
 * 便捷封装 sd_select_instance，使用默认负载均衡策略。
 *
 * @param sdh          助手句柄
 * @param service_name 服务名称
 * @param instance     输出的选中实例
 * @return 0 成功，非0 失败
 */
int sd_helper_select(sd_helper_t *sdh, const char *service_name,
                     sd_instance_t *instance);

/**
 * @brief 选择最优服务实例（指定策略）
 *
 * @param sdh          助手句柄
 * @param service_name 服务名称
 * @param strategy     负载均衡策略
 * @param instance     输出的选中实例
 * @return 0 成功，非0 失败
 */
int sd_helper_select_with_strategy(sd_helper_t *sdh, const char *service_name,
                                   sd_lb_strategy_t strategy,
                                   sd_instance_t *instance);

/* ==================== 状态查询 ==================== */

/**
 * @brief 获取底层 service_discovery 句柄
 *
 * 用于需要直接调用 service_discovery API 的场景。
 *
 * @param sdh 助手句柄
 * @return service_discovery 句柄
 */
service_discovery_t sd_helper_get_sd(sd_helper_t *sdh);

/**
 * @brief 检查服务发现是否运行中
 *
 * @param sdh 助手句柄
 * @return true 运行中
 */
bool sd_helper_is_running(sd_helper_t *sdh);

/**
 * @brief 获取已注册服务数量
 *
 * @param sdh 助手句柄
 * @return 服务数量
 */
uint32_t sd_helper_service_count(sd_helper_t *sdh);

/**
 * @brief C-L08: 输出服务发现统计摘要（单行格式，适合周期性日志）
 *
 * @param sdh 助手句柄
 */
void sd_helper_dump_stats(sd_helper_t *sdh);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_SERVICE_DISCOVERY_HELPER_H */