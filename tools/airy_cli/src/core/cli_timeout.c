/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_timeout.c
 * @brief airy_cli 超时阶梯实现（0.1.21 D-8）。
 *
 * 四档缺省值由 commons 预算树（C-21）派生，不再各写一个魔数：
 *   rpc        = 10s（管理 RPC，不参与工具执行）
 *   tool.sync  = max(30s, tool 层 60s)      → 客户端不早于服务端撤销
 *   tool.async = client 层 600s + 10s 余量  → 覆盖 600s 作业上限
 *   think      = outer(think 层 120s)=150s  → 留 25% 余量
 * 每档另设服务端下限，解析结果低于下限时只向上钳制。
 */

#include "cli_timeout.h"

#include "airy_budget.h"
#include "cli_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TMO_RPC_DEF 10000
#define TMO_SYNC_DEF 30000
#define TMO_THINK_DEF 120000
#define TMO_SLACK_MS 10000

static int g_ovr_ms;

/* 十进制毫秒解析；空串/非数字/越界返回 0（配置错误按未设置处理）。 */
static long long tmo_dec(const char *s)
{
    long long v = 0;
    const char *p;

    if (!s)
        return 0;
    p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    for (; *p >= '0' && *p <= '9'; p++) {
        v = v * 10 + (*p - '0');
        if (v > 0x7FFFFFFF)
            return 0;
    }
    return p > s ? v : 0;
}

static const char *tmo_env(cli_tmo_tier_t tier)
{
    switch (tier) {
    case CLI_TMO_RPC:
        return "AIRY_TIMEOUT_RPC_MS";
    case CLI_TMO_TOOL_SYNC:
        return "AIRY_TIMEOUT_TOOL_SYNC_MS";
    case CLI_TMO_TOOL_ASYNC:
        return "AIRY_TIMEOUT_TOOL_ASYNC_MS";
    default:
        /* think 档无独立 env：AIRY_TIMEOUT_THINK_MS 是 C-21 预算树的 think
         * 层输入，经 airy_budget_ms() 进入下限（outer 已含 25% 余量），CLI
         * 再读一次会二次放大；think 的 CLI 级覆盖只走 model.yaml。 */
        return NULL;
    }
}

static const char *tmo_section(cli_tmo_tier_t tier)
{
    return tier == CLI_TMO_THINK ? "think" : "timeout";
}

static const char *tmo_key(cli_tmo_tier_t tier)
{
    switch (tier) {
    case CLI_TMO_RPC:
        return "rpc_ms";
    case CLI_TMO_TOOL_SYNC:
        return "tool_sync_ms";
    case CLI_TMO_TOOL_ASYNC:
        return "tool_async_ms";
    case CLI_TMO_THINK:
        return "timeout_ms";
    default:
        return NULL;
    }
}

/* 服务端下限：客户端不得先于该层放弃（与 C-21 只向上钳制一致）。 */
static int tmo_floor(cli_tmo_tier_t tier)
{
    switch (tier) {
    case CLI_TMO_TOOL_SYNC:
        return (int)airy_budget_ms(AIRY_BUDGET_TOOL);
    case CLI_TMO_TOOL_ASYNC:
        return (int)airy_budget_ms(AIRY_BUDGET_CLIENT);
    case CLI_TMO_THINK:
        return (int)airy_budget_outer(airy_budget_ms(AIRY_BUDGET_THINK));
    default:
        return 0;
    }
}

static int tmo_def(cli_tmo_tier_t tier)
{
    switch (tier) {
    case CLI_TMO_RPC:
        return TMO_RPC_DEF;
    case CLI_TMO_TOOL_SYNC:
        return TMO_SYNC_DEF;
    case CLI_TMO_TOOL_ASYNC:
        return (int)airy_budget_ms(AIRY_BUDGET_CLIENT) + TMO_SLACK_MS;
    case CLI_TMO_THINK:
        return TMO_THINK_DEF;
    default:
        return 0;
    }
}

static int tmo_cfg(cli_tmo_tier_t tier)
{
    char path[512];
    char val[32];

    cli_cfg_path(path, sizeof(path));
    if (cli_cfg_yaml_get(path, tmo_section(tier), tmo_key(tier), val, sizeof(val)) != 0)
        return 0;
    return (int)tmo_dec(val);
}

int cli_tmo_ms(cli_tmo_tier_t tier)
{
    long long v;
    int floor;

    if (tier < 0 || tier >= CLI_TMO_TIERS)
        return 0;
    if (g_ovr_ms > 0) {
        v = g_ovr_ms;
    } else {
        const char *ev = tmo_env(tier);
        v = ev ? tmo_dec(getenv(ev)) : 0;
        if (v <= 0)
            v = tmo_cfg(tier);
        if (v <= 0)
            v = tmo_def(tier);
    }
    floor = tmo_floor(tier);
    if (v < floor)
        v = floor;
    return (int)v;
}

int cli_tmo_set(int ms)
{
    if (ms < 0)
        return -1;
    g_ovr_ms = ms;
    return 0;
}

int cli_tmo_parse_sec(const char *s)
{
    long long v = 0;
    const char *p = s;

    if (!p)
        return -1;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p < '0' || *p > '9')
        return -1;
    for (; *p >= '0' && *p <= '9'; p++) {
        v = v * 10 + (*p - '0');
        if (v > 86400)
            return -1;
    }
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != '\0' || v <= 0)
        return -1;
    return (int)(v * 1000);
}

int cli_tmo_method(const char *method)
{
    if (method && strncmp(method, "tool.", 5) == 0)
        return cli_tmo_ms(CLI_TMO_TOOL_SYNC);
    return cli_tmo_ms(CLI_TMO_RPC);
}

const char *cli_tmo_name(cli_tmo_tier_t tier)
{
    switch (tier) {
    case CLI_TMO_RPC:
        return "rpc";
    case CLI_TMO_TOOL_SYNC:
        return "tool.sync";
    case CLI_TMO_TOOL_ASYNC:
        return "tool.async";
    case CLI_TMO_THINK:
        return "think";
    default:
        return "?";
    }
}
