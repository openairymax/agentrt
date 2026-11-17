// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_cmdline.c
 * @brief CLI 命令分发匹配行为测试（社区反馈②：CLI 指令不清晰）。
 *
 * 锁死分发形态：精确命中 → 唯一前缀缩写放行 → 多义拒绝并列候选 →
 * 未知命令提示 → 非 / 输入落回对话。重点回归旧实现的两处静默改道：
 * 任何单字符输入（如 "/t"）误触首条命令、短一字符输入（如 "/tu"）
 * 直触 /tui、尾部多字符（如 "/statusx"）误触 /status。
 *
 * 命令实现以 cli_cmd_stubs.c 桩承接（quit/tui 桩置 ctx 标志，据此断
 * 言分发去向）；对 stdout 的提示文案不做内容断言。
 */

#include "cli_internal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            printf("    FAIL: %s (line %d)\n", #cond, __LINE__);                          \
            return 1;                                                                     \
        }                                                                                 \
    } while (0)

/* ctx 形态与 cli_internal.h cli_cmd_ctx_t 一致（quit/switch_tui 均为
 * int* 指针，cmd 桩经指针回写标志）——此前测试误传双 int 结构体，
 * 桩对越界偏移解引用导致 SIGSEGV，本形态即为该事故的回归锁。 */
static int run(const char *input, int *quit, int *switch_tui)
{
    *quit = 0;
    *switch_tui = 0;
    cli_cmd_ctx_t ctx = {quit, switch_tui};
    return cli_dispatch_command(input, &ctx);
}

static int test_exact_hits(void)
{
    printf("  test_exact_hits...\n");
    int quit = 0, tui = 0;

    CHECK(run("/tui", &quit, &tui) == 1);
    CHECK(tui == 1);
    CHECK(quit == 0);

    CHECK(run("/quit", &quit, &tui) == 1);
    CHECK(quit == 1);
    CHECK(tui == 0);

    /* 带参数的精确命中：命令词与参数正确分离（help 桩无副作用） */
    CHECK(run("/help orch", &quit, &tui) == 1);
    CHECK(quit == 0 && tui == 0);

    printf("    PASSED\n");
    return 0;
}

/* 社区反馈②实锤回归："/t" 旧实现以首字节 '/' 与首条命令恒等而误触
 * /help，"/tu" 短一字符直触 /tui。新形态：/t 多义拒绝（不动任何命令），
 * /tu 唯一前缀放行 /tui。 */
static int test_prefix_disambiguation(void)
{
    printf("  test_prefix_disambiguation...\n");
    int quit = 0, tui = 0;

    CHECK(run("/t", &quit, &tui) == 1);
    CHECK(tui == 0);
    CHECK(quit == 0);

    CHECK(run("/tu", &quit, &tui) == 1);
    CHECK(tui == 1);

    /* 尾部多字符不得误触被前缀的命令 */
    CHECK(run("/statusx", &quit, &tui) == 1);
    CHECK(run("/quitz", &quit, &tui) == 1);
    CHECK(quit == 0 && tui == 0);

    /* 缩写命中 needs_args 命令且无参：缺参提示，不执行 */
    CHECK(run("/orc", &quit, &tui) == 1);
    CHECK(quit == 0 && tui == 0);

    printf("    PASSED\n");
    return 0;
}

static int test_unknown_and_fallthrough(void)
{
    printf("  test_unknown_and_fallthrough...\n");
    int quit = 0, tui = 0;

    CHECK(run("/nosuch_cmd", &quit, &tui) == 1);
    CHECK(quit == 0);

    CHECK(run("hello world", &quit, &tui) == 0);

    printf("    PASSED\n");
    return 0;
}

static int test_lookup_helpers(void)
{
    printf("  test_lookup_helpers...\n");

    CHECK(cli_cmd_exact("/tui") != NULL);
    CHECK(cli_cmd_exact("/tu") == NULL);
    CHECK(cli_cmd_exact("/nope") == NULL);

    const cli_command_t *out = NULL;
    CHECK(cli_cmd_prefix("/tu", &out) == 1);
    CHECK(out != NULL && strcmp(out->name, "/tui") == 0);

    CHECK(cli_cmd_prefix("/t", &out) > 1);
    CHECK(cli_cmd_prefix("/nosuch_cmd", &out) == 0);

    char tok[CLI_CMD_TOK_MAX];
    cli_cmd_token("/orch build the thing", tok, sizeof(tok));
    CHECK(strcmp(tok, "/orch") == 0);
    cli_cmd_token("/quit", tok, sizeof(tok));
    CHECK(strcmp(tok, "/quit") == 0);

    printf("    PASSED\n");
    return 0;
}

/* usage 字段完备性（社区反馈②：缺参提示与 /help <cmd> 的权威用法源） */
static int test_usage_table(void)
{
    printf("  test_usage_table...\n");
    size_t n = cli_commands_count();
    CHECK(n == 33);
    for (size_t i = 0; i < n; i++) {
        CHECK(CLI_COMMANDS[i].usage != NULL);
        CHECK(CLI_COMMANDS[i].usage[0] == '/');
        CHECK(CLI_COMMANDS[i].fn != NULL);
    }
    printf("    PASSED\n");
    return 0;
}

/* 0.1.21 A-6：paths 自证子命令的 CLI 契约。仅裸词位（非 -p 提示词）
 * 才走子命令；成功按「已输出」返回 1，未知参数按用法错误返回 2。 */
static int test_paths_subcommand(void)
{
    printf("  test_paths_subcommand...\n");
    char a0[] = "airy_cli", a1[] = "paths", a2[] = "--bogus", a3[] = "-p";
    char a4[] = "--json";
    char *ok[] = {a0, a1};
    char *js[] = {a0, a1, a4};
    char *bad[] = {a0, a1, a2};
    char *prompt[] = {a0, a3, a1};
    const char *pp = NULL;

    g_cli_print_mode = 0;
    CHECK(cli_parse_args(2, ok, &pp) == 1);
    CHECK(cli_parse_args(3, js, &pp) == 1);
    CHECK(cli_parse_args(3, bad, &pp) == 2);

    /* -p paths：呈现模式下的裸词是提示词，必须落回对话（返回 0） */
    g_cli_print_mode = 0;
    CHECK(cli_parse_args(3, prompt, &pp) == 0);
    CHECK(pp != NULL && strcmp(pp, "paths") == 0);
    g_cli_print_mode = 0;

    printf("    PASSED\n");
    return 0;
}

int main(void)
{
    printf("test_cli_cmdline:\n");
    test_exact_hits();
    test_prefix_disambiguation();
    test_unknown_and_fallthrough();
    test_lookup_helpers();
    test_usage_table();
    test_paths_subcommand();
    printf("ALL PASSED\n");
    return 0;
}
