// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_term.c
 * @brief Terminal capability probe implementation.
 *
 * Color level follows the common de-facto chain (NO_COLOR / TERM=dumb /
 * COLORTERM=truecolor / TERM=*256color / basic), so the CLI behaves
 * identically on a laptop, over SSH and in server logs.
 */

#include "cli_term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

static int g_color_level = -1; /* -1 = not probed yet */
static int g_tty = -1;

/* Fixed bottom strip rows reserved below the scroll region (three-zone
 * layout: hero / dialogue / input). Updated by cli_term_header_pin. */
static int g_footer_lines = 0;

static void cli_term_probe(void)
{
    int level = CLI_TERM_COLOR_BASIC;

    const char *no_color = getenv("NO_COLOR");
    const char *term = getenv("TERM");
    const char *colorterm = getenv("COLORTERM");

    if (no_color && no_color[0] && strcmp(no_color, "0") != 0) {
        level = CLI_TERM_COLOR_NONE;
    } else if (term && (strcmp(term, "dumb") == 0 || term[0] == '\0')) {
        level = CLI_TERM_COLOR_NONE;
    } else if (colorterm && (strstr(colorterm, "truecolor") != NULL ||
                             strstr(colorterm, "24bit") != NULL)) {
        level = CLI_TERM_COLOR_TRUECOLOR;
    } else if (term && strstr(term, "256color") != NULL) {
        level = CLI_TERM_COLOR_256;
    }
    g_color_level = level;

#ifdef _WIN32
    g_tty = _isatty(_fileno(stdout)) != 0;
#else
    g_tty = isatty(fileno(stdout)) != 0;
#endif
}

#ifdef _WIN32
/* 兜底定义：Windows 10+ 的 VT 模式宏在旧 SDK / 老 mingw-w64 的
 * wincon.h 中可能缺失（_WIN32_WINNT 低于 0x0A00）。用官方数值，
 * #ifndef 保护避免与已定义冲突。 */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif
#endif

void cli_term_init(void)
{
    cli_term_probe();

#ifdef _WIN32
    /* Windows 10+ 现代控制台：启用 VT 序列（ANSI 转义输出）与 VT 输入
     * 序列（方向键/功能键以 ESC [ x 形式到达），并把代码页切到 UTF-8，
     * 使中文提示符/输入不依赖系统 ANSI 代码页。旧控制台（Win7/8）
     * SetConsoleMode 失败即静默降级——TUI 退化为行渲染，行为同 POSIX
     * 非 TTY 路径。 */
    HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hout != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hout, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT;
            SetConsoleMode(hout, mode);
        }
    }
    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    if (hin != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hin, &mode)) {
            /* raw 输入（与 POSIX termios raw mode 对齐）：禁回显/行缓冲，
             * 禁 PROCESSED_INPUT（否则 Ctrl+C 变成进程中断，readline 收
             * 不到 0x03 取消键），禁 QUICK_EDIT（误点会进入选择模式卡死
             * 逐键输入）。 */
            mode |= ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_EXTENDED_FLAGS;
            mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT |
                      ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);
            SetConsoleMode(hin, mode);
        }
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

int cli_term_color_level(void)
{
    if (g_color_level < 0)
        cli_term_probe();
    return g_color_level;
}

int cli_color_enabled(void)
{
    if (cli_term_color_level() < CLI_TERM_COLOR_BASIC)
        return 0;
    if (cli_term_is_tty())
        return 1;
    /* Server-grade: piped / logged output stays monochrome even when the
     * environment advertises colors — color only means something on a TTY.
     * FORCE_COLOR overrides that explicitly (Claude Code / Codex parity). */
    const char *force = getenv("FORCE_COLOR");
    return force && force[0] && strcmp(force, "0") != 0;
}

int cli_term_is_tty(void)
{
    if (g_tty < 0)
        cli_term_probe();
    return g_tty;
}

void cli_term_title(const char *title)
{
    if (!title || !cli_term_is_tty())
        return;

    /* OSC 0 (xterm): set the terminal title; strip ESC so an untrusted
     * title cannot inject further escape sequences. */
    fputs("\033]0;", stdout);
    for (const char *p = title; *p; p++) {
        if (*p == '\033')
            break;
        fputc(*p, stdout);
    }
    fputs("\007", stdout);
    fflush(stdout);
}

#ifndef _WIN32
#include <sys/ioctl.h>
#endif

void cli_term_size(int *out_rows, int *out_cols)
{
    if (out_rows)
        *out_rows = 0;
    if (out_cols)
        *out_cols = 0;
    if (!cli_term_is_tty())
        return;

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        if (out_rows)
            *out_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        if (out_cols)
            *out_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
#else
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        if (out_rows)
            *out_rows = ws.ws_row;
        if (out_cols)
            *out_cols = ws.ws_col;
        return;
    }
    /* PTY winsize 未设置（script/CI/非交互终端常为 0）时回退：
     * ① 环境变量 COLUMNS（多数 shell 导出）② 默认 80。否则折行计量
     * 依赖 cols=0 而失效，长行预览擦除不足导致最终形态与残留重叠
     * （2026-08-20 PTY 复现：fold_phys=1 而真实折行 22 行）。 */
    if (out_cols) {
        const char *env_cols = getenv("COLUMNS");
        int c = env_cols ? atoi(env_cols) : 0;
        *out_cols = (c > 0) ? c : 80;
    }
    if (out_rows) {
        const char *env_rows = getenv("LINES");
        int r = env_rows ? atoi(env_rows) : 0;
        *out_rows = (r > 0) ? r : 24;
    }
#endif
}

void cli_term_cursor_to(int row, int col)
{
    if (!cli_term_is_tty())
        return;
    if (row < 1)
        row = 1;
    if (col < 1)
        col = 1;
    printf("\033[%d;%dH", row, col);
}

void cli_term_header_pin(int header_lines, int footer_lines)
{
    g_footer_lines = (footer_lines > 0) ? footer_lines : 0;
    if (!cli_term_is_tty() || header_lines < 1)
        return;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows - g_footer_lines <= header_lines) {
        /* Terminal too short for a pinned header plus a fixed footer strip:
         * keep full-screen scroll (three-zone layout degrades gracefully). */
        g_footer_lines = 0;
        return;
    }
    /* Scroll region = header+1 .. rows-footer_lines; then home the cursor to
     * the first scrollable line so subsequent output stays below the pinned
     * hero. The bottom footer_lines rows stay fixed (input zone). */
    printf("\033[%d;%dr", header_lines + 1, rows - g_footer_lines);
    cli_term_cursor_to(header_lines + 1, 1);
    fflush(stdout);
}

void cli_term_header_unpin(void)
{
    g_footer_lines = 0;
    if (!cli_term_is_tty())
        return;
    fputs("\033[r", stdout);
    /* 部分终端（tmux 等）在重置滚动区（DECSTBM 空参）时会把光标送回
     * 屏幕左上角；显式回到最后一行，保证后续输出（如退出横幅）继续
     * 画在对话结束处，而不是覆盖屏幕顶部的 hero 区。 */
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows > 0)
        printf("\033[%d;1H", rows);
    fflush(stdout);
}

/* ---- fixed bottom input strip (three-zone layout helpers) ----
 *
 * 输入区：pin 时保留的底部行。这些助手只在「TTY + 已保留底部条」时生效，
 * 否则返回 0 / no-op，piped / logged 输出保持传统换行提示符布局。
 */

int cli_term_input_on(void)
{
    if (!cli_term_is_tty() || g_footer_lines <= 0)
        return 0;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    return (rows > g_footer_lines) ? 1 : 0;
}

int cli_term_input_begin(void)
{
    if (!cli_term_input_on())
        return 0;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    /* 三区布局（2026-08-19）：footer >= 2 时，倒数第二行画一条 dim
     * 分隔线，末行留给输入。对话滚动区止于 rows-footer，分隔线与
     * 输入行固定不动，hero / 对话 / 输入三区边界始终清晰。 */
    if (g_footer_lines >= 2 && cols > 0) {
        cli_term_cursor_to(rows - 1, 1);
        printf("\033[2K");
        printf("\033[2m");
        for (int c = 0; c < cols; c++)
            fputs("─", stdout); /* UTF-8 整字符（fputc 只写首字节会乱码） */
        printf("\033[0m");
    }
    /* 定位到末行并整行擦除，随后由调用方打印提示符。 */
    cli_term_cursor_to(rows, 1);
    printf("\033[2K");
    fflush(stdout);
    return 1;
}

void cli_term_input_submit(void)
{
    if (!cli_term_input_on())
        return;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows < 1)
        return;
    /* 擦除用户输入回显，光标回到滚动区末行，对话输出从那里继续流动，
     * 永不覆盖底部输入条。 */
    cli_term_cursor_to(rows, 1);
    printf("\033[2K");
    cli_term_cursor_to(rows - g_footer_lines, 1);
    fflush(stdout);
}

void cli_term_input_hop(void)
{
    if (!cli_term_input_on())
        return;
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    if (rows < 1)
        return;
    /* 在底部输入条上打印内容（如占位提示符）后，把光标送回滚动区末行，
     * 后续流式输出从对话区继续，不会写进输入条。 */
    cli_term_cursor_to(rows - g_footer_lines, 1);
    fflush(stdout);
}

/* ==================== 主题（浅色 / 深色，2026-08-25） ====================
 *
 * 配色随终端背景自适应：深色背景用高对比亮色，浅色背景切到深色调
 * （256 色前景），分隔块背景换浅色变体，保证两种终端下角色标签与
 * 背景块都可读。CLR_* 宏经 cli_theme_seq() 解析，调用点零改动。
 */

static cli_theme_mode_t g_theme_mode = CLI_THEME_AUTO;
static int g_theme_probed = 0;

/* 深色主题（默认）：与历史 16 色 / 256 色深底配色一致。 */
static const char *const k_theme_dark[CLI_TH_COUNT] = {
    [CLI_TH_BOLD]      = "\033[1m",
    [CLI_TH_DIM]       = "\033[2m",
    [CLI_TH_UNDERLINE] = "\033[4m",
    [CLI_TH_CYAN]      = "\033[36m",
    [CLI_TH_GREEN]     = "\033[32m",
    [CLI_TH_YELLOW]    = "\033[33m",
    [CLI_TH_RED]       = "\033[31m",
    [CLI_TH_MAGENTA]   = "\033[35m",
    [CLI_TH_BLUE]      = "\033[34m",
    [CLI_TH_BG_GRAY]   = "\033[48;5;236m",
    [CLI_TH_BG_BLUE]   = "\033[48;5;24m",
    [CLI_TH_REVERSE]   = "\033[7m",
    [CLI_TH_RESET]     = "\033[0m",
};

/* 浅色主题：前景用 256 色深色调（浅底上保持对比度），背景块换浅色。 */
static const char *const k_theme_light[CLI_TH_COUNT] = {
    [CLI_TH_BOLD]      = "\033[1m",
    [CLI_TH_DIM]       = "\033[2m",
    [CLI_TH_UNDERLINE] = "\033[4m",
    [CLI_TH_CYAN]      = "\033[38;5;30m",   /* 深青 */
    [CLI_TH_GREEN]     = "\033[38;5;28m",   /* 深绿 */
    [CLI_TH_YELLOW]    = "\033[38;5;130m",  /* 深黄/棕 */
    [CLI_TH_RED]       = "\033[38;5;124m",  /* 深红 */
    [CLI_TH_MAGENTA]   = "\033[38;5;90m",   /* 深紫 */
    [CLI_TH_BLUE]      = "\033[38;5;25m",   /* 深蓝 */
    [CLI_TH_BG_GRAY]   = "\033[48;5;250m",  /* 浅灰 */
    [CLI_TH_BG_BLUE]   = "\033[48;5;117m",  /* 浅蓝 */
    [CLI_TH_REVERSE]   = "\033[7m",
    [CLI_TH_RESET]     = "\033[0m",
};

#ifndef _WIN32
#include <poll.h>

/* OSC 11 背景色查询（xterm/kitty/ghostty 等）：发出查询后在 150ms 内
 * 等待响应（poll 超时防止不支持 OSC 的终端阻塞启动）。响应形如
 *   ESC ] 11 ; rgb:RRRR/GGGG/BBBB ESC \
 * 每段 1~4 位 hex（xterm 用 4 位）；取前 2 位 hex 归一化到 0-255。 */
static int term_query_bg(unsigned char *or, unsigned char *og,
                                     unsigned char *ob)
{
    if (!cli_term_is_tty())
        return 0;
    fputs("\033]11;?\033\\", stdout);
    fflush(stdout);
    char buf[128];
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 150) <= 0)
        return 0;
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0)
        return 0;
    buf[n] = '\0';
    char *p = strstr(buf, "rgb:");
    if (!p)
        return 0;
    p += 4;
    unsigned int vals[3];
    for (int i = 0; i < 3; i++) {
        char *end = NULL;
        unsigned long v = strtoul(p, &end, 16);
        if (end == p)
            return 0;
        /* 1 位 hex（如 "f"）按 ANSI 惯例放大到 0xff */
        unsigned int width = (unsigned int)(end - p);
        if (width <= 1 && v <= 0xf)
            v = (v << 4) | v;
        vals[i] = (unsigned int)(v > 255 ? v / 257 : v); /* 4 位 hex → 8 位 */
        if (*end == '\0' || i == 2)
            break;
        p = end + 1; /* 跳过 '/' */
    }
    *or = (unsigned char)vals[0];
    *og = (unsigned char)vals[1];
    *ob = (unsigned char)vals[2];
    return 1;
}
#endif

void cli_theme_init(void)
{
    if (g_theme_probed)
        return;
    g_theme_probed = 1;

    const char *env = getenv("AIRY_CLI_THEME");
    if (env && env[0]) {
        if (strcmp(env, "light") == 0) {
            g_theme_mode = CLI_THEME_LIGHT;
            return;
        }
        if (strcmp(env, "dark") == 0) {
            g_theme_mode = CLI_THEME_DARK;
            return;
        }
        /* auto / 其他取值：继续检测 */
    }
    g_theme_mode = CLI_THEME_DARK; /* 无 TTY / 检测失败兜底 */
#ifdef _WIN32
    /* Windows 经典控制台 / Windows Terminal 用 COLORFGBG 暴露背景色
     * （"fg;bg"，bg 0-15；>=7 即浅色）。 */
    const char *cfbg = getenv("COLORFGBG");
    if (cfbg && cfbg[0]) {
        const char *slash = strrchr(cfbg, ';');
        int bg = slash ? atoi(slash + 1) : atoi(cfbg);
        if (bg >= 7)
            g_theme_mode = CLI_THEME_LIGHT;
    }
#else
    unsigned char r = 0, g = 0, b = 0;
    if (term_query_bg(&r, &g, &b)) {
        double lum = (0.299 * (double)r + 0.587 * (double)g + 0.114 * (double)b) / 255.0;
        g_theme_mode = (lum >= 0.5) ? CLI_THEME_LIGHT : CLI_THEME_DARK;
    }
#endif
}

cli_theme_mode_t cli_theme_mode(void)
{
    return g_theme_mode;
}

const char *cli_theme_seq(cli_theme_t th)
{
    if ((int)th < 0 || (int)th >= CLI_TH_COUNT)
        return "\033[0m";
    if (!cli_color_enabled())
        return "";
    return (g_theme_mode == CLI_THEME_LIGHT) ? k_theme_light[th] : k_theme_dark[th];
}

