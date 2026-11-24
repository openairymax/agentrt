/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_gw_json.h
 * @brief 网关客户端 JSON 参数面：cJSON 出入参的调用收敛封装。
 *
 * 三面分工（0.1.19 §459）：cli_gw.h 是传输契约，刻意不暴露 cJSON；
 * cli_gw_int.h 是传输层与 RPC 层之间的内部面；本头是**调用方**使用的
 * JSON 参数面。凡"建 params → 序列化 → 调用 → 解析 result"的骨架一律
 * 走 cli_gw_json，消除各业务模块重复的三段错误处理与所有权管理。
 */

#ifndef AIRY_RT_CLI_GW_JSON_H
#define AIRY_RT_CLI_GW_JSON_H

#include <cjson/cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 以 cJSON 参数调用网关并取回 cJSON 结果。
 * @param method     方法名（含命名空间前缀，如 "mem.search"）
 * @param params     参数对象（所有权移交：无论成败均由本函数释放；
 *                   NULL 等价于空参数对象）
 * @param timeout_ms 超时毫秒（>0）
 * @param out_root   [out] JSON-RPC result 解析树（OWNER，调用方 cJSON_Delete）
 * @return 0 成功；否则为 cli_gw_call 的真实 AIRY_ERR_*（失败原因见
 *         cli_gw_last_err / cli_gw_err_take）；result 非合法 JSON 返回
 *         AIRY_ERR_PROTOCOL
 */
int cli_gw_json(const char *method, cJSON *params, int timeout_ms, cJSON **out_root);

/**
 * @brief cli_gw_json 的续期轮询变体（长管线专用，语义同 cli_gw_call_q）。
 */
int cli_gw_json_q(const char *method, cJSON *params, int timeout_ms, int renewals,
                  cJSON **out_root);

/**
 * @brief result 被再包一层 JSON 字符串时的解包（与 TUI 客户端同容错）。
 *
 * 两个终端的 result 形态曾出现差异：一侧返回对象，另一侧返回「对象的
 * JSON 字符串」。入参 root 的所有权在解包成功时被释放；未命中时原样返回，
 * 调用方始终按单一所有权 cJSON_Delete 即可。
 * @param root 解析后的 result（可 NULL）
 * @return 解包后的对象或原 root
 */
cJSON *cli_json_unwrap(cJSON *root);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_RT_CLI_GW_JSON_H */
