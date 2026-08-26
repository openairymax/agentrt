/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_gw.h
 * @brief AgentRT 客户端统一网关客户端（轻量 HTTP/1.1 JSON-RPC）。
 *
 * 架构约束（2026-08-25）：所有客户端（CLI/TUI/其他）必须经 gateway 派发
 * 到微核心服务，禁止直连 daemon socket。本模块为 airy_cli 提供统一网关
 * 访问：JSON-RPC over HTTP POST /（非流式），以及 /api/v1/chat/stream
 * （SSE 流式对话）。
 */

#ifndef AIRY_RT_CLI_GW_H
#define AIRY_RT_CLI_GW_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief SSE 流式行回调（data: 载荷，不含前缀；[DONE] 也回调）。 */
typedef void (*cli_gw_line_cb)(const char *line, void *user_data);

/**
 * @brief 网关 JSON-RPC 调用（POST /，method 如 "think.process"）。
 * @param method      方法名（含命名空间前缀）
 * @param params_json 参数 JSON（可 NULL）
 * @param timeout_ms  超时毫秒（>0）
 * @param out_result  [out] JSON-RPC result JSON 字符串（OWNER，AIRY_FREE）
 * @return 0 成功；非 0 失败（gateway 不可达 / HTTP 错误 / JSON-RPC error）
 */
int cli_gw_call(const char *method, const char *params_json, int timeout_ms, char **out_result);

/**
 * @brief 流式对话（POST /api/v1/chat/stream，SSE）。
 * @param payload    请求体 JSON（含 prompt/messages/model）
 * @param timeout_ms 总超时毫秒
 * @param cb         data: 行回调（可 NULL）
 * @param ud         回调上下文
 * @return 0 成功（收到 [DONE]）；非 0 失败/超时
 */
int cli_gw_stream(const char *payload, int timeout_ms, cli_gw_line_cb cb, void *ud);

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
