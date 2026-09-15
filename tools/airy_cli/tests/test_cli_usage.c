// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_usage.c
 * @brief 回合计费展示口径（cli_chat_usage_metrics）单测。
 *
 * 网关不可达时（本测试指向不可解析的网关地址）计费真值回退本轮 chat 累计，
 * 覆盖：累计字段 = 厂商 usage 的 prompt_tokens / completion_tokens /
 * cost_usd；展示串与厂商字段逐项对照；零消耗回合不产出指标串；reset 清零。
 */

#define _POSIX_C_SOURCE 200809L

#include "cli_internal.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* cli_gw.c 引用的跨 TU 符号（正常由 main.c 拥有）。 */
volatile sig_atomic_t g_cli_cancel = 0;

static int g_run = 0;
static int g_pass = 0;

#define CHECK(cond, name)                                                      \
    do {                                                                       \
        g_run++;                                                               \
        if (cond) {                                                            \
            printf("  [PASS] %s\n", name);                                     \
            g_pass++;                                                          \
        } else {                                                               \
            printf("  [FAIL] %s (line %d)\n", name, __LINE__);                 \
        }                                                                      \
    } while (0)

static llm_response_t resp_make(uint32_t prompt, uint32_t completion,
                                uint32_t total, double cost)
{
    llm_response_t r;
    AIRY_MEMSET(&r, 0, sizeof(r));
    r.prompt_tokens = prompt;
    r.completion_tokens = completion;
    r.total_tokens = total;
    r.cost_usd = cost;
    return r;
}

static void test_zero_turn(void)
{
    char buf[192];
    CHECK(cli_chat_usage_metrics(buf, sizeof(buf)) == 0 && buf[0] == '\0',
          "metrics: zero consumption yields no metric string");
}

static void test_vendor_fields(void)
{
    llm_response_t a = resp_make(120, 60, 180, 0.0003);
    llm_response_t b = resp_make(80, 40, 120, 0.0006);
    cli_chat_usage_add(&a);
    cli_chat_usage_add(&b);

    char buf[192];
    CHECK(cli_chat_usage_metrics(buf, sizeof(buf)) == 1,
          "metrics: consumption yields metric string");
    CHECK(strcmp(buf, "Tokens: 300 (prompt 200 · completion 100) · Cost: $0.000900") == 0,
          "metrics: prompt/completion map to vendor usage fields");
}

static void test_total_only(void)
{
    cli_chat_usage_reset();
    llm_response_t r = resp_make(0, 0, 50, 0.001);
    cli_chat_usage_add(&r);

    char buf[192];
    CHECK(cli_chat_usage_metrics(buf, sizeof(buf)) == 1 &&
              strcmp(buf, "Tokens: 50 · Cost: $0.001000") == 0,
          "metrics: total-only usage falls back to flat form");
}

static void test_cost_only(void)
{
    cli_chat_usage_reset();
    llm_response_t r = resp_make(0, 0, 0, 0.0025);
    cli_chat_usage_add(&r);

    char buf[192];
    CHECK(cli_chat_usage_metrics(buf, sizeof(buf)) == 1 &&
              strcmp(buf, "Tokens: 0 · Cost: $0.002500") == 0,
          "metrics: cost-only usage still reported");
}

static void test_reset(void)
{
    cli_chat_usage_reset();
    char buf[192];
    CHECK(cli_chat_usage_metrics(buf, sizeof(buf)) == 0 && buf[0] == '\0',
          "metrics: reset clears accumulated usage");
}

int main(void)
{
    /* 非 IPv4 字面量的主机名：端点解析即失败，cli_gw_call 立即返回 -1，
     * 走"回退本轮 chat 累计"路径（不依赖对端是否回 RST，避免等满预算）。 */
    setenv("AIRY_GATEWAY_URL", "http://airymaxrt.invalid:1", 1);

    test_zero_turn();
    test_vendor_fields();
    test_total_only();
    test_cost_only();
    test_reset();

    printf("test_cli_usage: %d/%d passed\n", g_pass, g_run);
    return g_pass == g_run ? 0 : 1;
}
