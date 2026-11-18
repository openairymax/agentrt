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
 * -3 接收超时（总预算耗尽仍无完整响应）。 */
#define CLI_GW_EXCH_CANCELED (-2)
#define CLI_GW_EXCH_TIMEOUT (-3)

/**
 * @brief 一次 HTTP/1.1 交换（发送请求，接收并切出正文）。
 * @param method      "GET" / "POST"
 * @param host        目标主机
 * @param port        目标端口
 * @param path        请求路径（"/" 或 "/health"）
 * @param body        POST 正文（GET 传 NULL）
 * @param timeout_ms  预算量子毫秒
 * @param renewals    量子耗尽可能续期次数（0=定长预算；>0 时对端未关
 *                    连接则继续轮询同一响应，总上界 timeout_ms×(1+renewals)）
 * @param out_body    [out] 正文（OWNER，AIRY_FREE）
 * @param out_body_len [out] 正文长度
 * @param out_status  [out] 状态码（可 NULL；0 表示未识别）
 * @return 0 成功；-1 传输失败；CLI_GW_EXCH_CANCELED；CLI_GW_EXCH_TIMEOUT
 */
int cli_gw_exchange(const char *method, const char *host, int port, const char *path,
                    const char *body, int timeout_ms, int renewals, char **out_body,
                    size_t *out_body_len, int *out_status);

/**
 * @brief C-10：把 gateway 的 JSON-RPC error.data 压成「失败在哪一环」单行。
 *
 * 只读 data 中的 stage/grad_rounds/grad_rejections/affected_node/err_detail
 * 与 stats.err_code，不改变错误语义：返回 daemon 侧原始错误码，供上层
 * 保留 code（-31 只能作为进程退出码，不得吞掉 -110 等根因）。
 *
 * @param data_json error.data 的 JSON 文本（可为 NULL 或空串）
 * @param out       [out] 单行归因（空串表示无可归因信息）
 * @param cap       输出容量
 * @return daemon 原始错误码（负值，取自 data.stats.err_code）；无则 0
 */
int cli_gw_fail_line(const char *data_json, char *out, size_t cap);

/* SSE 流式接入（0.1.21 D-15）：传输层导出连接/请求构造/发送/关闭四个
 * 薄面，SSE 客户端只做「行翻译 + 续传」，不重复实现 HTTP 报文搬运。 */
int cli_gw_conn_open(const char *host, int port, int timeout_ms);
int cli_gw_req_build(const char *method, const char *host, int port,
                     const char *path, const char *body, char *req, size_t reqsz);
int cli_gw_req_send(int fd, const char *req, size_t reqn, const char *body);
void cli_gw_conn_close(int fd);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CLI_GW_INT_H */
