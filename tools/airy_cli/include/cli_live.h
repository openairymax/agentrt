/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_live.h
 * @brief CLI 会话存活感知：后端代次/可达性心跳与 stale 会话重建。
 *
 * 0.1.19 §3.7.2 N4 机制件。readline 空闲节拍内低频探活，发现网关代次
 * 变化（网关进程重建）或单个服务由下线转上线（daemon 进程重建）时，
 * 先提示用户再经绑定的重建回调重装会话视图；两个判据同时生效、互补，
 * 探测本身不得污染业务诊断（失败原因缓冲按快照恢复）。
 *
 * 节拍经 AIRY_CLI_LIVE_MS 调整（毫秒，0 = 关闭周期探测，默认 2000，
 * 有效区间 50~30000），供资源受限环境与脚本模式收敛后台开销。
 */

#ifndef AIRY_RT_CLI_LIVE_H
#define AIRY_RT_CLI_LIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 重置存活视图并启用节拍（保留已绑定的重建回调）。
 *
 * 服务状态与网关代次基线全部清零：首次 tick 只建立基线，不触发重建，
 * 避免启动瞬间把正常在线误判为重启。
 */
void cli_live_init(void);

/**
 * @brief 绑定 stale 会话重建回调（NULL 解绑）。
 * @param on_stale 重建入口，典型为 cli_session_rebind
 */
void cli_live_bind(void (*on_stale)(void));

/**
 * @brief 节拍推进：到期即探测一轮，命中迁移则提示并重建。
 *
 * 调用点位于 readline 空闲分支，无并发业务调用；单轮探测受预算约束，
 * 超预算即让出，剩余服务顺延至下一节拍。
 */
void cli_live_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CLI_LIVE_H */
