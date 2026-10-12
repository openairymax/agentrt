// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_banner.c
 * @brief Startup header banner (compact, non-pinned) and UTF-8 clip helper.
 *
 * 0.1.7 交互改版：弃用「固定滚动区 + 底部输入条」三区布局。社区反馈
 * （2026-08-31）：固定英雄区 + DECSTBM 滚动区让普通终端的上下滚动
 * 失效、长回复时头部被顶出/残留，交互体验割裂。现改为普通滚动 REPL：
 *
 *   ◆ Airymax AgentRT · v<版本>
 *   A·t2=xxx  B·t1-f=xxx  C·t1-p=xxx
 *   (空行)
 *
 * 头部打印一次后随内容自然滚动，不再 pin；对话、工具卡片、思考链均
 * 在同一滚动流中（Claude Code / ChatGPT CLI 惯例）。系统状态信息仍可
 * 随时经 /models、/help 查看。
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 一个 "key=model" 段（紧凑头部模型槽行用）：青 key · dim "=" · 黄 model。
 * lead_sep 为 1 时前置两个空格分隔。 */
static void cli_hero_model_seg(const char *key, const char *model, int lead_sep)
{
    if (lead_sep) {
        cli_out("  ");
    }
    cli_out(cli_c(CLR_CYAN));
    cli_out(key);
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_out("=");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_YELLOW));
    cli_out(model);
    cli_out(cli_c(CLR_RESET));
}

/**
 * @brief 紧凑启动头部（3 行，普通滚动，不再 pin）。
 *
 *   ◆ Airymax AgentRT · v<版本>
 *   A·t2=xxx  B·t1-f=xxx  C·t1-p=xxx
 *   (空行)
 *
 * 行数与 CLI_HDR_LINES=3 严格对应（cli_tui.c 退出全屏 TUI 重建三区时
 * 依赖该行数）。-p 单轮模式不打印（stdout 保持纯净）。
 */
void cli_print_system_header(const char *t2, const char *t1f, const char *t1p)
{
    /* One-shot server mode (-p): no startup hero/panel, output is the
     * single-turn result only (Claude Code -p / Codex exec convention). */
    if (g_cli_print_mode)
        return;

    const char *g = cli_gutter_pad(2);
    /* 行 1：品牌 + 版本 */
    cli_out(g);
    cli_out(cli_c(CLR_BOLD));
    cli_out(cli_c(CLR_CYAN));
    cli_out("◆ Airymax AgentRT");
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_outf(" · v%s", AIRY_CLI_VERSION);
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
    /* 行 2：三模型槽（空值回落"默认"） */
    const char *a = (t2 && t2[0]) ? t2 : "默认";
    const char *b = (t1f && t1f[0]) ? t1f : "默认";
    const char *c = (t1p && t1p[0]) ? t1p : "默认";
    cli_out(g);
    cli_hero_model_seg("A·t2", a, 0);
    cli_hero_model_seg("B·t1-f", b, 1);
    cli_hero_model_seg("C·t1-p", c, 1);
    cli_outc('\n');
    /* 行 3：明显分界线（替代空白行，保持 CLI_HDR_LINES=3 契约）。
     * 社区反馈：启动区与对话区需要清晰分隔，视觉效果上不能靠残留转义
     * 序列"凑"出分界。 */
    cli_out(g);
    cli_out(cli_c(CLR_DIM));
    {
        char rule[192];
        size_t rw = 56, rn = 0;
        while (rn < rw && rn + 3 < sizeof(rule)) {
            rule[rn++] = '\xE2';
            rule[rn++] = '\x94';
            rule[rn++] = '\x80';   /* U+2500 ─（1 cell） */
        }
        rule[rn] = '\0';
        cli_out(rule);
    }
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
    fflush(stdout);
}
