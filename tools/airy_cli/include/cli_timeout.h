/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_timeout.h
 * @brief airy_cli 超时阶梯（0.1.21 D-8 单一事实源）。
 *
 * 此前 CLI 四类等待各写死一个常量（rpc 10s / tool 30s / 异步 610s /
 * think 120s），与 commons 预算树（C-21）无关联：服务端 cap 抬到 60s
 * 后客户端仍按 30s 放弃，调用方先撤、服务端照常执行。
 *
 * 本契约把四档收敛到一张表，解析顺序固定为：
 *   显式 --timeout > 环境变量 > model.yaml [timeout] 段 > 内置缺省。
 * 每档还有一个「服务端下限」：解析结果低于该层时只向上钳制（与 C-21
 * 「只向上、永不缩短」一致），保证客户端不会先于服务端放弃。
 */

#ifndef AIRY_CLI_TIMEOUT_H
#define AIRY_CLI_TIMEOUT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CLI_TMO_RPC = 0,   /* 管理 RPC（非工具路径）：缺省 10s，无服务端下限 */
    CLI_TMO_TOOL_SYNC, /* tool.execute 同步执行：不低于 tool 层 */
    CLI_TMO_TOOL_ASYNC,/* tool.submit/poll 等到终态：不低于 client 层 */
    CLI_TMO_THINK,     /* think/orch：不低于 think 层 +25% 余量 */
    CLI_TMO_TIERS
} cli_tmo_tier_t;

/** @brief 该档生效毫秒数；非法档号返回 0。 */
int cli_tmo_ms(cli_tmo_tier_t tier);

/** @brief 设置 --timeout 全局覆盖（毫秒）；ms<=0 清除。返回 0/-1。 */
int cli_tmo_set(int ms);

/** @brief 解析 --timeout 的秒数：成功返回毫秒，非法（1..86400 之外）返回 -1。 */
int cli_tmo_parse_sec(const char *s);

/** @brief 按方法名选档：tool.* → 同步档，其余 → RPC 档。 */
int cli_tmo_method(const char *method);

/** @brief 档名（日志用）：rpc/tool.sync/tool.async/think。 */
const char *cli_tmo_name(cli_tmo_tier_t tier);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_TIMEOUT_H */
