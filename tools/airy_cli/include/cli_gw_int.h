/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_gw_int.h
 * @brief 网关客户端模块内契约：传输层（cli_gw.c）与 JSON-RPC 层
 *        （cli_gw_rpc.c）之间唯一的共享面。
 *
 * 分层边界：传输层只负责「按 HTTP/1.1 换一次报文」，不知道 JSON-RPC
 * 语义；RPC 层只负责「信封构造 / result 提取 / 失败分诊」，不碰 socket。
 * 两层共用错误细分码，保证取消与超时在 RPC 层可具名（S-02）。
 */

#ifndef AIRY_RT_CLI_GW_INT_H
#define AIRY_RT_CLI_GW_INT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* cli_gw_exchange 失败细分：-1 传输失败（连接/发送/协议）；-2 用户取消；
 * -3 接收超时（预算耗尽仍无完整响应头）。 */
#define CLI_GW_EXCH_CANCELED (-2)
#define CLI_GW_EXCH_TIMEOUT (-3)

/**
 * @brief 一次 HTTP/1.1 交换（发送请求，接收并切出正文）。
 * @param method      "GET" / "POST"
 * @param host        目标主机
 * @param port        目标端口
 * @param path        请求路径（"/" 或 "/health"）
 * @param body        POST 正文（GET 传 NULL）
 * @param timeout_ms  总预算毫秒
 * @param out_body    [out] 正文（OWNER，AIRY_FREE）
 * @param out_body_len [out] 正文长度
 * @param out_status  [out] 状态码（可 NULL；0 表示未识别）
 * @return 0 成功；-1 传输失败；CLI_GW_EXCH_CANCELED；CLI_GW_EXCH_TIMEOUT
 */
int cli_gw_exchange(const char *method, const char *host, int port, const char *path,
                    const char *body, int timeout_ms, char **out_body, size_t *out_body_len,
                    int *out_status);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CLI_GW_INT_H */
