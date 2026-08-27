// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_stream.c
 * @brief 聊天域流式渲染与思考进度（域拆分自 cli_chat.c，2026-08-27）。
 *
 * -p 模式流式回调把增量文本直写 stdout（[code] 归一化 + 跨 chunk carry），
 * 交互 TTY 流式的 "Connecting…" 擦除、思考进度行（[Dual Fast Think] 思考中）
 * 与最终预览计量（折叠擦除用），以及 t1-f/t1-p 模型槽缓存。
 * 共享声明见 cli_chat_internal.h。
 */

#include "cli_chat_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -p 模式流式渲染回调：把增量文本直写 stdout（stdout 保持纯净可管道；
 * 工具/进度 chrome 走 stderr，见 cli_trace）。交互模式仍走 spinner +
 * markdown 完整渲染，避免流式下 markdown 标记裸露。
 *
 * 流式归一化：部分模型用 [code]/[/code] 包裹代码而非 markdown 围栏
 * ``` ```。交互模式由 cli_render_markdown 统一识别；-p 流式直出时必须
 * 在此归一化（[code] → ```），否则管道输出裸露标签。跨 chunk 边界
 * （token 被 provider 截断成多个增量）用静态 carry 缓冲拼接。 */
static char s_code_carry[8];
static size_t s_code_carry_len = 0;

/* 交互 TTY 流式：首片到达时擦除 "Connecting..." 提示行（stderr 上的
 * 光标行），避免残留。仅在交互 TTY 流式模式下有意义（-p 不打印提示）。 */
static int s_stream_first_chunk = 0;

/* 交互 TTY 流式思考进度行状态（cli_chat_reasoning_cb 更新，收尾擦除）。 */
static int s_reasoning_progress = 0;      /* 进度行当前可见 */

/* 交互 TTY 流式状态（2026-08-17）：流式预览直出后记录其物理行数，
 * 完成后擦除预览并完整重绘最终形态（结果不折叠，仅思考链折叠）。
 * 仅最终轮有意义（工具轮的预览保留为过程展示，不擦除）。 */
size_t g_chat_fold_phys = 0;
/* 预览末尾是否无换行（光标停在最后一行行尾）：擦除时上移行数须少 1，
 * 否则会把预览起点上方那行（消息行）一并覆盖。 */
int g_chat_fold_tail_no_nl = 0;

static void cli_stream_norm_emit(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        cli_outc(s[i]);
    fflush(stdout);
}

void cli_stream_norm_flush_carry(void)
{
    if (s_code_carry_len) {
        cli_stream_norm_emit(s_code_carry, s_code_carry_len);
        s_code_carry_len = 0;
    }
}

/* s[0..n) 是否是 [code] 或 [/code] 的严格前缀（不含完整标签本身）？ */
static int cli_code_prefix(const char *s, size_t n)
{
    static const char *const tags[] = {"[code]", "[/code]"};
    if (n == 0 || n >= 7)
        return 0;
    for (size_t t = 0; t < sizeof(tags) / sizeof(tags[0]); t++) {
        if (n < strlen(tags[t]) && strncmp(s, tags[t], n) == 0)
            return 1;
    }
    return 0;
}

void cli_chat_stream_cb(const char *chunk, void *user_data)
{
    (void)user_data;
    if (!chunk)
        return;
    size_t n = strlen(chunk);
    if (n == 0)
        return;

    /* 首片到达：擦除 "Connecting..." / 思考进度提示行（同位置） */
    if (s_stream_first_chunk) {
        s_stream_first_chunk = 0;
        s_reasoning_progress = 0; /* 进度行已被首片擦除，避免收尾重复擦除 */
        fputs("\r\033[2K", stdout);
        fflush(stdout);
    }

    /* 跨 chunk：上一片末尾的标签前缀先与本次开头拼接判断。 */
    if (s_code_carry_len) {
        size_t take = 7 - s_code_carry_len;
        if (take > n)
            take = n;
        AIRY_MEMCPY(s_code_carry + s_code_carry_len, chunk, take);
        s_code_carry_len += take;
        chunk += take;
        n -= take;
        if (strncmp(s_code_carry, "[code]", 6) == 0 ||
            strncmp(s_code_carry, "[/code]", 7) == 0) {
            cli_stream_norm_emit("```", 3);
            s_code_carry_len = 0;
        } else {
            cli_stream_norm_flush_carry();
        }
        if (n == 0)
            return;
    }

    size_t i = 0;
    while (i < n) {
        if (n - i >= 6 && strncmp(chunk + i, "[code]", 6) == 0) {
            cli_stream_norm_emit("```", 3);
            i += 6;
            continue;
        }
        if (n - i >= 7 && strncmp(chunk + i, "[/code]", 7) == 0) {
            cli_stream_norm_emit("```", 3);
            i += 7;
            continue;
        }
        /* chunk 末尾的标签前缀（被 token 截断）→ 缓存，不提前输出。 */
        if (i + 6 >= n && cli_code_prefix(chunk + i, n - i)) {
            AIRY_MEMCPY(s_code_carry, chunk + i, n - i);
            s_code_carry_len = n - i;
            return;
        }
        cli_outc(chunk[i]);
        i++;
    }
    fflush(stdout);
}

/* 2.3.5/2.3.6：thinking 模型（DeepSeek/Kimi 等）先产思考链再产正文，
 * 思考阶段可长达数十秒。原始推理碎片逐块上屏无阅读价值（用户反馈
 * "太长看不懂"），因此思考期间只显示轻量进度行
 * （[Dual Fast Think] 思考中… N 字 · 耗时，\r 原地刷新），首个
 * content 分片到达即擦除；完整思考链在回复完成后折叠展示。
 * 通道说明：交互模式 stderr 被重定向到日志，实时 UI 反馈（Connecting /
 * 思考进度）与流式正文**同流直写 stdout**（同一 fd 光标同步，\r 覆盖
 * 不留痕）；-p / --json / TUI 下这些反馈被各自前置条件禁用。 */
static int s_reasoning_chars = 0;         /* 思考字数累计 */
static uint64_t s_reasoning_start_ms = 0; /* 思考阶段开始时刻 */

void cli_chat_reasoning_cb(const char *delta, void *user_data)
{
    (void)user_data;
    /* 仅交互 TTY 流式显示进度；-p / --json / TUI 走各自无进度路径。 */
    if (!delta || g_cli_print_mode || g_cli_json_mode)
        return;
    /* 正文已开始（首片到达）：思考阶段结束，不再刷新进度行——上游
     * reasoning 增量与 content 首片可能交错到达（DeepSeek 流式分块），
     * 否则进度行 \r 覆盖会压到正文首行（2026-08-19 实测竞态）。 */
    if (!s_stream_first_chunk)
        return;
    if (!s_reasoning_start_ms)
        s_reasoning_start_ms = cli_now_ms();
    s_reasoning_chars += (int)strlen(delta);
    s_reasoning_progress = 1;
    uint64_t ms = cli_now_ms() - s_reasoning_start_ms;
    /* 角色名与全站一致：[Dual Slow/Fast/Prof Think]（2.3.14） */
    fprintf(stdout, "\r    %s[%s]%s 思考中… %d 字 · %lu.%lus   ",
            cli_c(CLR_YELLOW), cli_render_actor_name(cli_chat_think_actor()),
            cli_c(CLR_RESET), s_reasoning_chars, (unsigned long)(ms / 1000),
            (unsigned long)(ms % 1000) / 100);
    fflush(stdout);
}

void cli_chat_reasoning_clear(void)
{
    if (s_reasoning_progress) {
        s_reasoning_progress = 0;
        /* 2K erases the whole line (not just cursor-to-end): a partially
         * rendered progress row must not leak "… 字 · 0.6s" onto the first
         * streaming-reply line (reported debris). */
        fputs("\r\033[2K", stdout);
        fflush(stdout);
    }
}

/* 2.3.14 (2026-08-17)：对话思考链来自 t1-f（context arbiter）模型，
 * 实时思考标签为 [Dual Fast Think]；未配置（走 llm_d 默认模型）时
 * 回落通用 [Dual Think]。2026-08-19：t1-f 配置统一来自
 * cli_think_cfg_load（env > model.yaml think 段 > llm.model 默认），
 * 与 think_d 实际生效模型一致；静态缓存避免每帧进度行重复读文件。 */
const char *cli_chat_t1f_cached(void)
{
    static char s_t1f[128];
    static int s_loaded = 0;
    if (!s_loaded) {
        s_loaded = 1;
        char t2[128], t1p[128];
        cli_think_cfg_load(t2, sizeof(t2), s_t1f, sizeof(s_t1f), t1p, sizeof(t1p));
    }
    return s_t1f;
}

/* 2.1.1.2 修复：t1-p（PROF）模型缓存——GCCP 意图确认（[Dual Prof Think]）
 * 使用该模型槽，与 CLI 渲染标签一致。配置源与 t1f 同（cli_think_cfg_load：
 * env > model.yaml think 段 > 默认）。非 static：cli_chat_gccp.c 的
 * cli_gccp_interact 逐问确认调用（原型见 cli_internal.h）。 */
const char *cli_chat_t1p_cached(void)
{
    static char s_t1p[128];
    static int s_loaded = 0;
    if (!s_loaded) {
        s_loaded = 1;
        char t2[128], t1f[128];
        cli_think_cfg_load(t2, sizeof(t2), t1f, sizeof(t1f), s_t1p, sizeof(s_t1p));
    }
    return s_t1p;
}

cli_actor_t cli_chat_think_actor(void)
{
    const char *t1f = cli_chat_t1f_cached();
    return (t1f && t1f[0]) ? CLI_ACTOR_DUAL_FAST_THINK : CLI_ACTOR_DUAL_THINK;
}

/* 交互 TTY 流式单轮调用（cli_chat_reply 的工具回路内使用）：
 * 计量本轮直出预览的行数（折叠擦除用），完成时返回上游 ret。 */
int cli_chat_stream_round(llm_svc_adapter_t *adapter, const llm_request_config_t *cfg,
                          int folding, llm_response_t **out_resp)
{
    cli_line_meter_t meter;
    if (folding)
        cli_render_meter_begin(&meter);
    /* 连接反馈：流式前显示连接状态（与正文同流 stdout），首片到达时擦除。
     * 避免连接阶段（RPC 握手 + 模型推理首 token）用户无任何反馈。 */
    s_stream_first_chunk = !g_cli_print_mode;
    if (s_stream_first_chunk) {
        fprintf(stdout, "    %s●%s Connecting…\r",
                cli_c(CLR_DIM), cli_c(CLR_RESET));
        fflush(stdout);
    }
    /* 思考阶段进度：每轮重置计数，reasoning 增量经回调实时上屏 */
    s_reasoning_start_ms = 0;
    s_reasoning_chars = 0;
    s_reasoning_progress = 0;
    int ret = llm_svc_adapter_complete_stream(adapter, cfg, cli_chat_stream_cb, NULL,
                                              cli_chat_reasoning_cb, NULL, out_resp);
    /* 思考进度行残留清理（reasoning-only 或异常中断场景） */
    cli_chat_reasoning_clear();
    /* 流式收尾：flush 可能残留的 [code] 前缀 carry（流提前结束时
     * 最后一片未触达标签判定的字节）。 */
    cli_stream_norm_flush_carry();
    /* 流结束但无首片到达（连接失败/空响应）：擦除 Connecting 行 */
    if (s_stream_first_chunk) {
        s_stream_first_chunk = 0;
        fputs("\r\033[2K", stdout);
        fflush(stdout);
    }
    if (folding) {
        g_chat_fold_phys = cli_render_meter_phys(&meter);
        g_chat_fold_tail_no_nl = (meter.row_len > 0) ? 1 : 0;
        cli_render_meter_end(&meter);
    }
    return ret;
}
