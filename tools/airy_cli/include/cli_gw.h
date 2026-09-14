/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_gw.h
 * @brief AgentRT 客户端统一网关客户端（轻量 HTTP/1.1 JSON-RPC）。
 *
 * 架构约束（2026-08-25 立法，2026-09-13 两面政策修订）：客户端访问
 * daemon 服务必须经 gateway 派发，禁止绕过 gateway 直连业务功能。
 * 本模块为 airy_cli 提供统一网关访问：JSON-RPC over HTTP POST /
 * （非流式）。
 *
 * 显式登记例外（0.1.16 审计 G-1）：CLI 生命周期/自愈面（daemon
 * start/stop/status、health_check、self-heal reconcile）不经本模块，
 * 直接走 daemon_rpc_client（A-IPC，Blueprint 8.3.3 corekern 优先、
 * socket 位等灰度回退）。理由：监管者不得依赖被监管者——gateway
 * 宕机/未启动时，CLI 必须仍能独立探活、拉起与恢复服务面。此例外
 * 仅限生命周期面；业务功能（chat/tools/memory/llm/think/agent）
 * 一律经本模块（gateway），违反即为缺陷。
 */

#ifndef AIRY_RT_CLI_GW_H
#define AIRY_RT_CLI_GW_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 网关 JSON-RPC 调用（POST /，method 如 "think.process"）。
 * @param method      方法名（含命名空间前缀）
 * @param params_json 参数 JSON（可 NULL）
 * @param timeout_ms  超时毫秒（>0）
 * @param out_result  [out] JSON-RPC result JSON 字符串（OWNER，AIRY_FREE）
 * @return 0 成功；-1 失败（失败原因已写入 g_cli_gw_err：不可达 / 响应
 *         超时 / HTTP 错误 / JSON-RPC error）；AIRY_ERR_CANCELED 用户取消
 *         （SIGINT；S-02：与 sched.dag_cancel 语义对齐，接收等待循环内
 *         命中即中断）
 */
int cli_gw_call(const char *method, const char *params_json, int timeout_ms, char **out_result);

/** @brief gateway 可达性（HTTP /health）。 @return 1 在线；0 离线 */
int cli_gw_health(int timeout_ms);

/**
 * @brief 解析网关端点（AIRY_GATEWAY_URL 或默认 127.0.0.1:8080）。
 * @param host      [out] 主机名
 * @param host_len  host 缓冲区大小
 * @param port      [out] 端口
 */
void cli_gw_endpoint(char *host, size_t host_len, int *port);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CLI_GW_H */
