// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_timeout.c
 * @brief D-8 CLI 超时阶梯单测（纯函数，无桩数据面、不触网关）。
 *
 * 覆盖 §19.1 D-8 判据：
 *  - 四档缺省由 C-21 预算树派生（10s / 60s / 610s / 150s）；
 *  - 解析优先级 --timeout > env > model.yaml > 缺省；
 *  - 客户端上限低于服务端下限时只向上钳制（不早于服务端放弃）；
 *  - cli_tmo_method 按 tool.* 选同步档、其余走 RPC 档；
 *  - --timeout 秒参数严格解析（1..86400，拒绝尾随垃圾）。
 */

#include "cli_timeout.h"
#include "compat.h" /* Windows 腿 setenv 垫片 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_fail;

#define CHECK(cond, what)                                                       \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, what);                \
            g_fail = 1;                                                         \
        }                                                                       \
    } while (0)

#define CHECK_EQ(got, want, what)                                               \
    do {                                                                        \
        int g_ = (got);                                                         \
        int w_ = (want);                                                        \
        if (g_ != w_) {                                                         \
            printf("FAIL %s:%d %s (got=%d want=%d)\n", __FILE__, __LINE__,      \
                   what, g_, w_);                                               \
            g_fail = 1;                                                         \
        }                                                                       \
    } while (0)

/* 空串在各消费点等价未设置（tmo_dec/路径判空），避免依赖 unsetenv
 * 在 Windows 腿的可用性。 */
static void env_clear(void)
{
    setenv("AIRY_TIMEOUT_RPC_MS", "", 1);
    setenv("AIRY_TIMEOUT_TOOL_SYNC_MS", "", 1);
    setenv("AIRY_TIMEOUT_TOOL_ASYNC_MS", "", 1);
    setenv("AIRY_TIMEOUT_THINK_MS", "", 1);
    setenv("AIRY_MODEL_CONFIG", "", 1);
    cli_tmo_set(0);
}

static void test_defaults(void)
{
    env_clear();
    CHECK_EQ(cli_tmo_ms(CLI_TMO_RPC), 10000, "rpc default 10s");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TOOL_SYNC), 60000, "tool.sync floor=tool 60s");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TOOL_ASYNC), 610000, "tool.async=client 600s+10s");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_THINK), 150000, "think=outer(120s)");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TIERS), 0, "invalid tier -> 0");
    CHECK(strcmp(cli_tmo_name(CLI_TMO_TOOL_SYNC), "tool.sync") == 0,
          "tier name tool.sync");
    CHECK_EQ(cli_tmo_method("tool.execute"), 60000, "tool.* -> sync tier");
    CHECK_EQ(cli_tmo_method("llm.chat"), 10000, "non-tool -> rpc tier");
}

static void test_env(void)
{
    env_clear();
    setenv("AIRY_TIMEOUT_RPC_MS", "4000", 1);
    setenv("AIRY_TIMEOUT_TOOL_SYNC_MS", "5000", 1);   /* 低于下限 → 抬升 */
    setenv("AIRY_TIMEOUT_TOOL_ASYNC_MS", "900000", 1);
    setenv("AIRY_TIMEOUT_THINK_MS", "180000", 1); /* 预算树 think 层输入 */
    CHECK_EQ(cli_tmo_ms(CLI_TMO_RPC), 4000, "env rpc override");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TOOL_SYNC), 60000, "sync below floor clamped up");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TOOL_ASYNC), 900000, "env async override");
    /* think 层 180s → 客户端 outer 225s，避免调用方先于服务端放弃。 */
    CHECK_EQ(cli_tmo_ms(CLI_TMO_THINK), 225000, "think = outer(budget layer)");
    env_clear();
}

static void test_yaml(void)
{
    char path[512];
    const char *dir = getenv("TMPDIR");
#ifdef _WIN32
    dir = getenv("TEMP");
#endif
    if (!dir || !dir[0])
        dir = ".";
    snprintf(path, sizeof(path), "%s/airy_tmo_%lu.yaml", dir,
             (unsigned long)time(NULL));

    FILE *f = fopen(path, "w");
    CHECK(f != NULL, "create temp model.yaml");
    if (!f)
        return;
    fputs("timeout:\n"
          "  rpc_ms: 25000\n"
          "  tool_sync_ms: 70000\n"
          "  tool_async_ms: 800000\n"
          "think:\n"
          "  timeout_ms: 160000\n", f);
    fclose(f);

    env_clear();
    setenv("AIRY_MODEL_CONFIG", path, 1);
    CHECK_EQ(cli_tmo_ms(CLI_TMO_RPC), 25000, "yaml rpc_ms");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TOOL_SYNC), 70000, "yaml tool_sync_ms");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TOOL_ASYNC), 800000, "yaml tool_async_ms");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_THINK), 160000, "yaml think timeout_ms");

    /* env 优先于 yaml。 */
    setenv("AIRY_TIMEOUT_RPC_MS", "12000", 1);
    CHECK_EQ(cli_tmo_ms(CLI_TMO_RPC), 12000, "env wins over yaml");
    env_clear();
    remove(path);
}

static void test_override(void)
{
    env_clear();
    /* --timeout 统一覆盖，但仍受服务端下限钳制。 */
    CHECK_EQ(cli_tmo_set(120000), 0, "set override");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_RPC), 120000, "override rpc");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TOOL_SYNC), 120000, "override sync");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_TOOL_ASYNC), 600000, "override clamped to client floor");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_THINK), 150000, "override clamped to think floor");

    CHECK_EQ(cli_tmo_set(0), 0, "clear override");
    CHECK_EQ(cli_tmo_ms(CLI_TMO_RPC), 10000, "cleared -> default");
    CHECK_EQ(cli_tmo_set(-1), -1, "negative rejected");
}

static void test_parse_sec(void)
{
    CHECK_EQ(cli_tmo_parse_sec("45"), 45000, "45s -> ms");
    CHECK_EQ(cli_tmo_parse_sec("1"), 1000, "1s -> ms");
    CHECK_EQ(cli_tmo_parse_sec("86400"), 86400000, "max 86400s");
    CHECK_EQ(cli_tmo_parse_sec("0"), -1, "0 rejected");
    CHECK_EQ(cli_tmo_parse_sec("-5"), -1, "negative rejected");
    CHECK_EQ(cli_tmo_parse_sec("abc"), -1, "non-digit rejected");
    CHECK_EQ(cli_tmo_parse_sec("12x"), -1, "trailing junk rejected");
    CHECK_EQ(cli_tmo_parse_sec("86401"), -1, "over max rejected");
    CHECK_EQ(cli_tmo_parse_sec(NULL), -1, "NULL rejected");
}

int main(void)
{
    test_defaults();
    test_env();
    test_yaml();
    test_override();
    test_parse_sec();

    if (g_fail) {
        printf("FAILED\n");
        return 1;
    }
    printf("PASS test_cli_timeout\n");
    return 0;
}
