// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_render.h
 * @brief airy_cli 终端输出机制层公共接口。
 *
 * 0.1.19 M5-3 清退收口后的 render 域唯一头：输出原语、-p 进度通道、
 * 主题色门、UTF-8 宽度、缩进槽、plain 直印出口与执行体状态辅助。
 * role 卡片 / spinner / phase 等体验渲染已整体删除，排版体验由
 * agentrt-tui（sdk/tui）承接。
 *
 * 面向流安全、行式输出：无 TTY 捕获、无 curses，直写 stdout，本地
 * 终端、SSH 与管道日志行为完全一致。
 */

#ifndef AIRY_CLI_RENDER_H
#define AIRY_CLI_RENDER_H

#include <stddef.h>
#include <stdint.h>

#include "cli_term.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One-shot server mode flags (defined in main.c): -p/--print suppresses
 * execution chrome; --json switches command/task output to structured JSON.
 * Exported via the render header so command handlers (cmd/cli_cmds)
 * can bypass the suppression for command *responses* (server scripting). */
extern int g_cli_print_mode;
extern int g_cli_json_mode;

/* 空回复占位：模型未产生文本回复（thinking 模型可能只输出
 * reasoning_content，或 provider 异常）时渲染明确提示，避免对话中出现
 * "空返回"却无任何说明。 */
#define CLI_REPLY_EMPTY_HINT "（未产生回复：模型可能仅生成了思考内容，请重试）"

/* 截断提示：finish_reason=length 表示收到的正文只是模型答案的前缀，
 * 渲染明确提示，避免用户误判为完整答案。 */
#define CLI_REPLY_TRUNCATED_HINT "（回复已达输出上限被截断，以上内容不完整；请缩短问题后重试）"

/* 思考链默认隐藏提示：思考链与对话正文不共用渲染通道，默认不上屏；
 * 完整文本经诊断通道留存，显式取用（-p 的 stderr trace / --json 的
 * reasoning 字段 / 该日志）。 */
#define CLI_REPLY_THINK_HIDDEN_HINT \
    "（思考链已默认隐藏；完整文本见 $AIRY_HOME/logs/airy_reasoning.log）"

/**
 * @brief Unified output primitives.
 *
 * These are the single exit point for every conversation/render line:
 *   - cli_out(s)      writes a NUL-terminated string
 *   - cli_outn(s, n)  writes n bytes
 *   - cli_outc(c)     writes one char
 *   - cli_outf(fmt…)  formatted output
 * Every primitive streams straight to stdout (stream-safe, pipe/log
 * friendly).
 */
void cli_out(const char *s);
void cli_outn(const char *s, size_t n);
void cli_outc(char c);
#if defined(__GNUC__)
void cli_outf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#else
void cli_outf(const char *fmt, ...);
#endif

/**
 * @brief One-shot server mode (-p) progress channel: "[tag] message" to stderr.
 *
 * stdout stays reserved for the final answer (Claude Code -p convention), so
 * scripts can parse the reply while operators watch the pipeline progress on
 * stderr. No-op in interactive mode, where the role chrome already shows
 * progress.
 *
 * @param tag  short phase tag (e.g. "plan", "submit", "status")
 * @param fmt  printf-style message
 */
#if defined(__GNUC__)
void cli_trace(const char *tag, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#else
void cli_trace(const char *tag, const char *fmt, ...);
#endif

/* ANSI color helpers. 经 cli_theme_seq() 解析，随终端背景在浅色 / 深色
 * 主题间自动切换（见 cli_term.h theme 段；调用点零改动，编译期常量场景
 * 不可用，仅用于 printf 参数）。 */
#define CLR_BOLD cli_theme_seq(CLI_TH_BOLD)
#define CLR_DIM cli_theme_seq(CLI_TH_DIM)
#define CLR_UNDERLINE cli_theme_seq(CLI_TH_UNDERLINE)
#define CLR_CYAN cli_theme_seq(CLI_TH_CYAN)
#define CLR_GREEN cli_theme_seq(CLI_TH_GREEN)
#define CLR_YELLOW cli_theme_seq(CLI_TH_YELLOW)
#define CLR_RED cli_theme_seq(CLI_TH_RED)
#define CLR_MAGENTA cli_theme_seq(CLI_TH_MAGENTA)
#define CLR_BLUE cli_theme_seq(CLI_TH_BLUE)
#define CLR_BG_GRAY cli_theme_seq(CLI_TH_BG_GRAY)
#define CLR_BG_BLUE cli_theme_seq(CLI_TH_BG_BLUE)
#define CLR_REVERSE cli_theme_seq(CLI_TH_REVERSE) /* 反显：输入光标闪烁 */
#define CLR_RESET cli_theme_seq(CLI_TH_RESET)

/* Small icon set shared across the CLI (Claude Code / Codex style). */
#define CLI_ICON_USER "\u203A"          /* › user prompt  */
#define CLI_ICON_BULLET "\u2022"        /* • generic event */
#define CLI_ICON_CHECK "\u2713"         /* ✓ success      */
#define CLI_ICON_CROSS "\u2717"         /* ✗ failure      */
#define CLI_ICON_DIAMOND "\u25C7"       /* ◇ plan/think   */
#define CLI_ICON_TODO "\u25A1"          /* □ pending      */
#define CLI_ICON_DONE "\u2714"          /* ✔ completed    */
#define CLI_ICON_WARN "\u26A0"          /* ⚠ warning      */
#define CLI_ICON_INFO "\u24D8"          /* ⓘ info         */
#define CLI_ICON_CLOCK "\u25F7"         /* ◷ scheduled    */
#define CLI_ICON_CANCEL "\u2298"        /* ⊘ canceled     */
#define CLI_ICON_ERR "\u25A0"           /* ■ error        */
#define CLI_ICON_BRANCH "\u2514"        /* └ detail indent */
#define CLI_ICON_TOOL "\u26CF"          /* ⛏ tool invocation */
#define CLI_ICON_HALF "\u25D0"          /* ◐ in-progress (live plan board) */

/* 非 TTY 状态行辅助：状态图标 + 紧凑进度条，供 -p 模式下
 * [progress]/[status]/[result] 等 cli_trace 行使用，让管道/CI 输出同样
 * 一眼可读（Linux 工程哲学：最小但完整的结构化信号）。 */
const char *cli_icon_for_state(const char *state);
void cli_compact_bar(char *out, size_t cap, double progress, size_t cells);

/* 用户可读的错误描述：内部错误码（AIRY_ERR_* 负值）对用户无意义，统一
 * 映射为可理解的中文描述；未识别码返回通用描述。返回值指向静态字符串。 */
const char *cli_err_desc(int err);

/* 任务状态中文化：内部状态机字符串（completed/running/...）对用户无
 * 意义，统一映射为简短中文；未识别状态返回原文。返回值指向静态字符串。 */
const char *cli_state_cn(const char *state);

/* 执行体终态 / 失败态判定（单一事实源）：completed / semantic_failed /
 * failed / canceled 均为终态；其中 semantic_failed（进程成功但无产出）、
 * failed、canceled 视为失败态。轮询、看板、快照与任务流共用，避免新增状态
 * 时判定漂移。 */
int cli_state_terminal(const char *state);
int cli_state_failed(const char *state);

/**
 * @brief Runtime color gate: return `seq` when color is enabled, "" otherwise.
 *
 * Every ANSI sequence in the CLI must flow through this gate so NO_COLOR /
 * piped / logged output stays clean monochrome (server-grade). Backed by
 * cli_term_color_enabled(); on Windows it always returns "".
 */
const char *cli_c(const char *seq);

/**
 * @brief Terminal display width of a UTF-8 string (CJK/full-width = 2 cells).
 *
 * Shared by the banner and the truncation helpers so every column accounts
 * for wide glyphs. Mirrors the width used by wcwidth for the common ranges
 * the CLI emits.
 */
size_t cli_disp_width(const char *s);

/**
 * @brief Terminal display width of the first `n` bytes of a UTF-8 string.
 *
 * Same width rules as cli_disp_width but stops at a byte boundary, so the
 * cursor can be positioned mid-line (edit position) without miscounting
 * partial multi-byte sequences.
 */
size_t cli_disp_width_of(const char *s, size_t n);

/**
 * @brief Largest byte count <= max_bytes that ends on a UTF-8 boundary.
 *
 * Truncating with snprintf precision can cut a multi-byte char in half and
 * print a stray continuation byte. Call this before any byte-wise
 * truncation of user/agent text (tags, headers, previews).
 */
size_t cli_utf8_safe_len(const char *s, size_t max_bytes);

/**
 * @brief Return a left gutter of `indent` spaces (shared static buffer).
 */
const char *cli_gutter(size_t indent);

/**
 * @brief Print raw text verbatim with a left gutter (plain mode).
 *
 * Line-oriented pass-through: no markdown parsing, no ANSI styling, no
 * protocol normalization. 0.1.19 直印化：行式 REPL 的内容输出为 plain 直印，
 * markdown 排版体验由 agentrt-tui（sdk/tui）承接。
 *
 * @param text   raw text (may contain \n)
 * @param indent left gutter width in spaces
 */
void cli_print_plain(const char *text, size_t indent);

/**
 * @brief Print a plain conversation line: "  [<header>] <content>".
 *
 * 0.1.19 直印化统一出口：role/tool 过程行在此收口。无色、无列对齐、
 * 无 icon（排版体验由 agentrt-tui 承接）；首行接在头后，续行回落
 * gutter，多行内容逐行原样。无模式判断——-p 抑制语义由调用点保持。
 *
 * @param header  bracket header (e.g. "Super Agent:对话")
 * @param content message text (plain, may contain \n); NULL prints header only
 */
void cli_print_line(const char *header, const char *content);

/**
 * @brief Print a status/trace hint line, silenced under -p.
 *
 * STATUS/TRACE 面统一分流：-p 管道模式静默（stdout 只留最终答案与错误），
 * 交互模式同 cli_print_line。模式判断在此收口，调用点不再内联。
 */
void cli_print_status(const char *header, const char *content);

/**
 * @brief Print the end-of-turn metrics line, silenced under -p.
 *
 * 轮次收口：耗时格式化在此单一事实源化（<1s "完成" / <60s "耗时 Ns" /
 * 其余 "耗时 Nm NS"），metrics 可选附注。装饰破折线由 agentrt-tui 承接。
 */
void cli_print_turn(uint64_t elapsed_ms, const char *metrics);

/**
 * @brief Monotonic wall-clock in milliseconds (for turn separators).
 */
uint64_t cli_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* AIRY_CLI_RENDER_H */
