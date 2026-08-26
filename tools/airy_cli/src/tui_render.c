// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_render.c
 * @brief TUI 引擎渲染域（域拆分自 cli_tui.c，2026-08-27）。
 *
 * 包含终端几何、全量/增量重绘、header/viewport/input 三区渲染、
 * 硬件信息面板（F2）与架构判定。
 */

#include "cli_tui_internal.h"

/* ---- terminal helpers ---- */

void tui_get_size(cli_tui_t *t)
{
    if (!t)
        return;
    /* 复用 cli_term_size 的完整回退链（ioctl → COLUMNS/LINES → 默认 24/80）。
     * 此前 winsize=0（script/CI/非交互终端）时回退固定 24×80，与
     * cli_term_header_pin 的 rows 计算（COLUMNS/LINES 回退）不同步——
     * 输入行画进滚动区（row 24），破坏三区布局（2026-08-20 PTY 复现）。 */
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    t->rows = (rows > 0) ? rows : 24;
    t->cols = (cols > 0) ? cols : 80;
}

void tui_write_literal(const char *s)
{
    if (s && *s)
        fputs(s, stdout);
}

/* ---- viewport geometry ---- */

size_t tui_middle_rows(cli_tui_t *t)
{
    size_t avail = t->rows > 0 ? (size_t)t->rows : 24;
    size_t header = t->hist.pinned;
    size_t input_rows = TUI_BOTTOM_INPUT_LINES;
    if (avail <= header + input_rows)
        return 0;
    return avail - header - input_rows;
}

/* P1：把 viewport 区域设为终端滚动区（DECSTBM），几何变化时才重设。
 * 窗口前移时终端滚动比逐行全量重绘省一个数量级的字节。 */
static void tui_scroll_region_set(cli_tui_t *t)
{
    size_t rows = tui_middle_rows(t);
    if (rows == 0)
        return;
    size_t top = t->hist.pinned + 1;
    size_t bottom = t->hist.pinned + rows; /* viewport 最后一行 */
    if (t->scr_set && t->scr_top == top && t->scr_bottom == bottom)
        return;
    tui_write_literal("\033[");
    char num[24];
    snprintf(num, sizeof(num), "%zu", top);
    tui_write_literal(num);
    tui_write_literal(";");
    snprintf(num, sizeof(num), "%zu", bottom);
    tui_write_literal(num);
    tui_write_literal("r");
    t->scr_top = top;
    t->scr_bottom = bottom;
    t->scr_set = 1;
}

/* ---- full redraw ---- */

void tui_clear_line(void)
{
    tui_write_literal("\033[2K");
}

/* 看板选中行反显（reverse video）；不改变字节长度，供 fputs 直接输出 */
static void tui_render_select(char *buf, size_t cap)
{
    char tmp[512];
    size_t n = strlen(buf);
    if (n + 1 >= sizeof(tmp))
        n = sizeof(tmp) - 1;
    AIRY_MEMCPY(tmp, buf, n);
    tmp[n] = '\0';
    snprintf(buf, cap, "\033[7m%s\033[27m", tmp);
}

void tui_render_header(cli_tui_t *t)
{
    for (size_t i = 0; i < t->hist.pinned && i < t->hist.count; i++) {
        tui_write_literal("\033[");
        /* 1-based row */
        char num[16];
        snprintf(num, sizeof(num), "%zu", i + 1);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(t->hist.lines[i], stdout);
    }
}

/* 架构字符串（编译期判定，跨平台）：与 install.sh detect_arch 同口径。 */
static const char *tui_arch_name(void)
{
#if defined(_WIN32) || defined(_WIN64)
# if defined(_M_ARM64)
    return "arm64";
# elif defined(_M_X64)
    return "x86_64";
# else
    return "windows";
# endif
#elif defined(__APPLE__)
# if defined(__aarch64__)
    return "arm64";
# else
    return "x86_64";
# endif
#else
# if defined(__riscv)
    return "riscv64";
# elif defined(__aarch64__)
    return "aarch64";
# elif defined(__arm__)
    return "armv7l";
# else
    return "x86_64";
# endif
#endif
}

/* ---- 硬件信息面板（F2，2026-08-25）----
 * 静态渲染本机实时环境：OS/版本/主机名、CPU 型号/核心数、内存总量/可用、
 * 架构、统一数据根路径。airy_get_sysinfo 跨平台采集。 */

/* 面板行写入：定位到视口第 row 行（相对 start_row）→ 清行 → 打印。 */
static void tui_hw_line(size_t row, size_t start_row, const char *fmt, ...)
{
    va_list ap;
    char buf[512];
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tui_write_literal("\033[");
    char num[16];
    snprintf(num, sizeof(num), "%zu", start_row + row);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();
    fputs(buf, stdout);
}

static void tui_render_hw_panel(cli_tui_t *t, size_t rows, size_t start_row)
{
    airy_sysinfo_t si;
    int ok = (airy_get_sysinfo(&si) == AIRY_SUCCESS);

    size_t r = 0;
    tui_hw_line(r++, start_row, "%s%s%s  %s%s%s", cli_c(CLR_BOLD),
                cli_c(CLR_CYAN), tui_mode_name(t->mode), cli_c(CLR_DIM),
                "· 本机实时环境 · F2/Esc 返回", cli_c(CLR_RESET));

    if (!ok) {
        tui_hw_line(r++, start_row, "%s%s 硬件信息采集失败（airy_get_sysinfo）%s",
                    cli_c(CLR_RED), cli_c(CLR_RESET), cli_c(CLR_RESET));
        for (; r < rows; r++)
            tui_hw_line(r, start_row, " ");
        return;
    }

    char mem_total[32], mem_free[32];
    {
        uint64_t mt = si.memory_total, mf = si.memory_free;
        const char *unit = "B";
        if (mt >= (1024ULL * 1024 * 1024)) {
            mt /= (1024 * 1024 * 1024);
            mf /= (1024 * 1024 * 1024);
            unit = "GiB";
        } else if (mt >= 1024 * 1024) {
            mt /= (1024 * 1024);
            mf /= (1024 * 1024);
            unit = "MiB";
        }
        snprintf(mem_total, sizeof(mem_total), "%llu %s",
                 (unsigned long long)mt, unit);
        snprintf(mem_free, sizeof(mem_free), "%llu %s",
                 (unsigned long long)mf, unit);
    }

    tui_hw_line(r++, start_row, "%sOS%s       %s%s%s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), cli_c(CLR_BOLD),
                si.os_name[0] ? si.os_name : "unknown", cli_c(CLR_RESET));
    if (si.os_version[0])
        tui_hw_line(r++, start_row, "%s版本%s     %s", cli_c(CLR_DIM),
                    cli_c(CLR_RESET), si.os_version);
    tui_hw_line(r++, start_row, "%s主机名%s   %s%s%s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), cli_c(CLR_BOLD),
                si.hostname[0] ? si.hostname : "unknown", cli_c(CLR_RESET));
    tui_hw_line(r++, start_row, "%sCPU%s      %s（%u 核）", cli_c(CLR_DIM),
                cli_c(CLR_RESET),
                si.cpu_model[0] ? si.cpu_model : "unknown", si.cpu_count);
    tui_hw_line(r++, start_row, "%s内存%s     %s（可用 %s）", cli_c(CLR_DIM),
                cli_c(CLR_RESET), mem_total, mem_free);
    tui_hw_line(r++, start_row, "%s架构%s     %s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), tui_arch_name());
    tui_hw_line(r++, start_row, "%s数据根%s   %s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), airy_data_dir());
    tui_hw_line(r++, start_row, "%s运行时%s   %s", cli_c(CLR_DIM),
                cli_c(CLR_RESET), airy_runtime_dir());

    for (r = r + 1; r < rows; r++)
        tui_hw_line(r, start_row, " ");
}

void tui_render_viewport(cli_tui_t *t)
{
    size_t rows = tui_middle_rows(t);
    size_t start_row = t->hist.pinned + 1;

    if (rows == 0)
        return;

    /* 阶段 4：面板模式（任务看板/事件流）渲染面板内容；对话模式走历史 */
    if (t->mode != CLI_TUI_MODE_CHAT) {
        const cli_tui_panel_count_fn count = t->panel[t->mode].count;
        const cli_tui_panel_line_fn line = t->panel[t->mode].line;
        const void *ud = t->panel[t->mode].ud;

        /* ---- 硬件信息面板（F2，2026-08-25）：静态渲染本机环境 ---- */
        if (t->mode == CLI_TUI_MODE_HW) {
            tui_render_hw_panel(t, rows, start_row);
            return;
        }

        /* ---- 任务详情视图（BOARD，Enter 进入，Esc/Enter 返回）---- */
        if (t->detail_active) {
            size_t dl = t->detail_len;
            size_t dstart = 0;
            char buf[512];
            for (size_t r = 0; r < rows; r++) {
                tui_write_literal("\033[");
                char num[16];
                snprintf(num, sizeof(num), "%zu", start_row + r);
                tui_write_literal(num);
                tui_write_literal(";1H");
                tui_clear_line();
                if (r == 0) {
                    snprintf(buf, sizeof(buf), "%s%s%s  %s%s%s",
                             cli_c(CLR_BOLD), cli_c(CLR_CYAN),
                             "任务详情", cli_c(CLR_DIM),
                             "· Esc 返回列表", cli_c(CLR_RESET));
                } else {
                    size_t ln = 0;
                    while (dstart + ln < dl && t->detail[dstart + ln] != '\n')
                        ln++;
                    size_t keep = (ln < sizeof(buf) - 1) ? ln : sizeof(buf) - 1;
                    AIRY_MEMCPY(buf, t->detail + dstart, keep);
                    buf[keep] = '\0';
                    dstart += ln;
                    if (dstart < dl)
                        dstart++; /* 跳过换行 */
                }
                fputs(buf, stdout);
            }
            return;
        }

        size_t total = (count && ud) ? count((void *)ud) : 0;
        size_t lines = total + 1; /* 标题行 + 内容行 */

        /* 滚动/跟随窗口 */
        size_t live = lines > rows ? rows : lines;
        size_t max_off = lines > rows ? lines - rows : 0;
        if (t->mode == CLI_TUI_MODE_BOARD) {
            if (total == 0)
                t->sel = 0;
            else if (t->sel >= total)
                t->sel = total - 1;
            /* 选中行保持在窗口内（默认窗口头部，越界时整体移动） */
            t->scroll_off = 0;
            size_t start = 0;
            if (max_off && t->sel + 1 >= live)
                start = t->sel + 1 - live + 1;
            if (start > max_off)
                start = max_off;
            for (size_t r = 0; r < rows; r++) {
                tui_write_literal("\033[");
                char num[16];
                snprintf(num, sizeof(num), "%zu", start_row + r);
                tui_write_literal(num);
                tui_write_literal(";1H");
                tui_clear_line();
                size_t rel = start + r;
                if (rel < lines) {
                    char buf[512];
                    if (rel == 0) {
                        snprintf(buf, sizeof(buf), "%s%s%s  %s%zu 条%s  %s%s%s",
                                 cli_c(CLR_BOLD), cli_c(CLR_CYAN),
                                 tui_mode_name(t->mode), cli_c(CLR_DIM), total,
                                 cli_c(CLR_RESET), cli_c(CLR_DIM),
                                 "· ↑↓ 选择 Enter 详情 x 取消",
                                 cli_c(CLR_RESET));
                        if (t->note[0]) {
                            snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                                     "  %s", t->note);
                        }
                    } else if (line && ud) {
                        if (!line((void *)ud, rel - 1, buf, sizeof(buf)))
                            buf[0] = '\0';
                        /* 选中行反显（标记是看板行索引 rel-1） */
                        if (rel - 1 == t->sel)
                            tui_render_select(buf, sizeof(buf));
                    } else {
                        buf[0] = '\0';
                    }
                    fputs(buf, stdout);
                }
            }
            return;
        }

        /* ---- 事件流：跟随尾部 / 回放浏览 ---- */
        if (t->follow)
            t->scroll_off = 0;
        if (t->scroll_off > max_off)
            t->scroll_off = max_off;
        size_t start = t->follow
                           ? (lines > rows ? lines - rows : 0)
                           : (t->scroll_off > 0 ? lines - t->scroll_off - live : 0);

        for (size_t r = 0; r < rows; r++) {
            char num[16];
            tui_write_literal("\033[");
            snprintf(num, sizeof(num), "%zu", start_row + r);
            tui_write_literal(num);
            tui_write_literal(";1H");
            tui_clear_line();
            size_t rel = start + r;
            if (rel < lines) {
                char buf[512];
                if (rel == 0) {
                    const char *hint;
                    if (t->mode == CLI_TUI_MODE_MEM)
                        hint = "· 尾部实时刷新  ↑↓ 回放  Esc 返回";
                    else
                        hint = t->follow ? "· 跟随中(f 关)" : "· 回放浏览(f 跟随)";
                    snprintf(buf, sizeof(buf), "%s%s%s  %s%zu 条%s  %s%s%s",
                             cli_c(CLR_BOLD), cli_c(CLR_CYAN),
                             tui_mode_name(t->mode), cli_c(CLR_DIM), total,
                             cli_c(CLR_RESET), cli_c(CLR_DIM),
                             hint,
                             cli_c(CLR_RESET));
                    if (t->note[0]) {
                        snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
                                 "  %s", t->note);
                    }
                } else if (line && ud) {
                    if (!line((void *)ud, rel - 1, buf, sizeof(buf)))
                        buf[0] = '\0';
                } else {
                    buf[0] = '\0';
                }
                fputs(buf, stdout);
            }
        }
        return;
    }

    size_t total = t->hist.count;
    int have_partial = t->cur && t->cur_len > 0;

    /* Viewport window over the content (history minus header), plus the
     * in-progress partial line (streamed text not yet committed). */
    size_t content = (total > t->hist.pinned ? total - t->hist.pinned : 0) +
                     (have_partial ? 1 : 0);
    size_t live = content > rows ? rows : content;
    size_t start = 0;
    if (t->scroll_off > 0) {
        size_t max_off = content > rows ? content - rows : 0;
        if (t->scroll_off > max_off)
            t->scroll_off = max_off;
        start = content - t->scroll_off - live;
    } else {
        start = content > rows ? content - rows : 0;
    }
    /* 全量渲染完成 → 记录窗口顶部，供 cli_tui_emit 增量重绘检测；
     * 同步 viewport 滚动区，供窗口前移时终端滚动增量。 */
    t->vp_start_rendered = start;
    t->vp_start_valid = 1;
    t->vp_content_rendered = content;
    tui_scroll_region_set(t);

    for (size_t r = 0; r < rows; r++) {
        tui_write_literal("\033[");
        char num[16];
        snprintf(num, sizeof(num), "%zu", start_row + r);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        size_t rel = start + r;
        if (rel >= content)
            continue;
        if (have_partial && rel == content - 1) {
            fputs(t->cur, stdout);
            continue;
        }
        size_t idx = t->hist.pinned + rel;
        if (idx < t->hist.count)
            fputs(t->hist.lines[idx], stdout);
    }
}

void tui_render_input(cli_tui_t *t)
{
    size_t row = (size_t)t->rows;
    tui_write_literal("\033[");
    char num[16];
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();

    /* 多候选 Tab 补全：在输入行上方显示候选列表（当前候选高亮）。
     * 2.2.3 拼音候选条优先：IME 拼音态时占用该行，回落原绘制路径。 */
    if (!tui_ime_draw_cands(t, (int)row) && t->tab_active && t->tab_count > 1) {
        size_t brow = row > 1 ? row - 1 : 1;
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", brow);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(cli_c(CLR_DIM), stdout);
        fputs("candidates:", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        size_t used = 0;
        for (size_t i = 0; i < t->tab_count; i++) {
            const char *nm = NULL;
            if (t->tab_kind == 1)
                nm = t->tab_cand_strs[i];
            else if (t->tab_cands[i] < cli_commands_count())
                nm = CLI_COMMANDS[t->tab_cands[i]].name;
            if (!nm)
                break;
            size_t w = cli_disp_width(nm) + 1;
            if (used + w > (size_t)t->cols)
                break;
            if (i == t->tab_sel) {
                fputs(cli_c(CLR_CYAN), stdout);
                fputs(nm, stdout);
                fputs(cli_c(CLR_RESET), stdout);
            } else {
                fputs(cli_c(CLR_DIM), stdout);
                fputs(nm, stdout);
                fputs(cli_c(CLR_RESET), stdout);
            }
            fputs(" ", stdout);
            used += w;
        }
        fflush(stdout);
    } else if (t->mode != CLI_TUI_MODE_CHAT) {
        /* 阶段 4：面板模式的 tab 栏（输入行上方一行，当前 tab 高亮） */
        size_t brow = row > 1 ? row - 1 : 1;
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", brow);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(cli_c(CLR_DIM), stdout);
        fputs("  tabs:", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        for (int m = 0; m < CLI_TUI_MODE_MAX; m++) {
            const char *name = tui_mode_name((cli_tui_mode_t)m);
            if ((cli_tui_mode_t)m == t->mode) {
                fputs(cli_c(CLR_BG_BLUE), stdout);
                fputs(cli_c(CLR_BOLD), stdout);
            } else {
                fputs(cli_c(CLR_DIM), stdout);
            }
            fputs(" ", stdout);
            fputs(name, stdout);
            fputs(" ", stdout);
            fputs(cli_c(CLR_RESET), stdout);
        }
        fputs(cli_c(CLR_DIM), stdout);
        fputs("  Ctrl+P/O 切换", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        fflush(stdout);
    } else {
        /* 对话视图（2026-08-19）：输入行上方画一条 dim 分隔线，与
         * 行渲染三区布局一致——对话滚动区与输入区边界清晰不重叠。 */
        size_t brow = row > 1 ? row - 1 : 1;
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", brow);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
        fputs(cli_c(CLR_DIM), stdout);
        for (int c = 0; c < t->cols; c++)
            fputs("─", stdout); /* UTF-8 整字符（fputc 只写首字节会乱码） */
        fputs(cli_c(CLR_RESET), stdout);
        fflush(stdout);
    }

    if (t->search_active) {
        /* Search line (readline convention): "(reverse-i-search)`q': <match>" */
        const char *prompt = t->search_forward ? "(i-search)`" : "(reverse-i-search)`";
        fputs(cli_c(CLR_DIM), stdout);
        fputs(prompt, stdout);
        fputs(cli_c(CLR_RESET), stdout);
        fwrite(t->search_query, 1, t->search_query_len, stdout);
        fputs(cli_c(CLR_DIM), stdout);
        fputs("': ", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        if (t->search_match >= 0 && (size_t)t->search_match < t->cmd_hist.count)
            fputs(cli_c(CLR_CYAN), stdout);
        if (t->search_match >= 0 && (size_t)t->search_match < t->cmd_hist.count)
            fputs(t->cmd_hist.entries[t->search_match], stdout);
        if (t->search_match >= 0 && (size_t)t->search_match < t->cmd_hist.count)
            fputs(cli_c(CLR_RESET), stdout);
        else if (t->search_query_len > 0)
            fputs(cli_c(CLR_RED), stdout), fputs("(failed)", stdout), fputs(cli_c(CLR_RESET), stdout);
        size_t col = 1 + cli_disp_width(prompt) +
                     cli_disp_width(t->search_query) + cli_disp_width("': ") + 1;
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", row);
        tui_write_literal(num);
        tui_write_literal(";");
        snprintf(num, sizeof(num), "%zu", col > 0 ? col : 1);
        tui_write_literal(num);
        tui_write_literal("H");
        fflush(stdout);
        return;
    }

    /* 2.2.3 IME 状态标记：[中]/[英]（词典可用时显示，dim） */
    const char *ime_tag = t->ime ? (t->ime_active ? "[中] " : "[英] ") : "";
    size_t ime_tag_w = strlen(ime_tag);
    if (ime_tag_w > 0) {
        fputs(cli_c(CLR_DIM), stdout);
        fputs(ime_tag, stdout);
        fputs(cli_c(CLR_RESET), stdout);
    }
    fputs(cli_c(CLR_CYAN), stdout);
    fputs(TUI_INPUT_PREFIX, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    size_t input_w = tui_caret_print(t); /* 2.2.1.5 反显光标（黑白闪烁） */
    /* Session status (model · msgs · elapsed), right-aligned dim; hidden
     * when the typed line would overlap it. */
    if (t->status[0] && t->mode == CLI_TUI_MODE_CHAT) {
        size_t st_w = cli_disp_width(t->status) + 2;
        size_t used_w = ime_tag_w + (size_t)strlen(TUI_INPUT_PREFIX) +
                        cli_disp_width_of(t->input, t->input_len);
        if (used_w + st_w < (size_t)t->cols) {
            size_t scol = (size_t)t->cols - st_w + 2;
            tui_write_literal("\033[");
            snprintf(num, sizeof(num), "%zu", row);
            tui_write_literal(num);
            tui_write_literal(";");
            snprintf(num, sizeof(num), "%zu", scol);
            tui_write_literal(num);
            tui_write_literal("H");
            fputs(cli_c(CLR_DIM), stdout);
            fputs(t->status, stdout);
            fputs(cli_c(CLR_RESET), stdout);
        }
    }
    /* Place the cursor at the edit position (byte offset -> display width). */
    size_t col = ime_tag_w + (size_t)strlen(TUI_INPUT_PREFIX) + input_w;
    tui_write_literal("\033[");
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";");
    snprintf(num, sizeof(num), "%zu", col > 0 ? col : 1);
    tui_write_literal(num);
    tui_write_literal("H");
    fflush(stdout);
}

/* P1 增量重绘（emit 场景）：chat 跟随模式下仅重绘 viewport 尾部变化行
 * （最后可见内容行 + 输入行），替代每次 chunk 全量重绘。窗口移动（内容
 * 溢出导致 start 前移）或首次进入时回退全量。 */
void tui_redraw_tail(cli_tui_t *t)
{
    size_t rows = tui_middle_rows(t);
    if (rows == 0) {
        cli_tui_redraw(t);
        return;
    }
    size_t content = tui_content_lines(t);
    int have_partial = t->cur && t->cur_len > 0;
    if (content == 0) {
        cli_tui_redraw(t);
        return;
    }
    size_t live = content > rows ? rows : content;
    size_t start = content > rows ? content - rows : 0;

    /* 窗口前移（尾部溢出）：滚动区增量——终端滚动 k 行 + 重绘新进
     * 视口的 k 行，避免全量逐行重绘。仅当旧窗口已填满（上次渲染时
     * 内容行数 >= 视口行数）时安全；首次进入或窗口未满（中间行从未
     * 渲染，滚动会带出空洞）时回退全量。 */
    if (start != t->vp_start_rendered) {
        size_t k = start > t->vp_start_rendered
                       ? start - t->vp_start_rendered
                       : 0;
        if (!t->vp_start_valid || k == 0 || k > rows || !t->scr_set ||
            t->vp_content_rendered < rows) {
            cli_tui_redraw(t);
            return;
        }
        /* 光标到滚动区底部，k 次换行触发终端上滚；滚动后底部 k 行空。 */
        char num[16];
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", t->scr_bottom);
        tui_write_literal(num);
        tui_write_literal(";1H");
        for (size_t i = 0; i < k; i++)
            tui_write_literal("\n");
        /* 重绘新进入视口的 k 行（滚动后的空行区） */
        for (size_t r = rows - k; r < rows; r++) {
            size_t rel = start + r;
            if (rel >= content)
                break;
            tui_write_literal("\033[");
            snprintf(num, sizeof(num), "%zu", t->hist.pinned + 1 + r);
            tui_write_literal(num);
            tui_write_literal(";1H");
            tui_clear_line();
            if (have_partial && rel == content - 1) {
                fputs(t->cur, stdout);
            } else {
                size_t idx = t->hist.pinned + rel;
                if (idx < t->hist.count)
                    fputs(t->hist.lines[idx], stdout);
            }
        }
        t->vp_start_rendered = start;
        t->vp_content_rendered = content;
        tui_render_input(t);
        t->dirty = 0;
        fflush(stdout);
        return;
    }
    /* 窗口未移动：只重绘最后可见内容行（含未提交的部分行）与输入行。 */
    size_t cols = t->cols > 0 ? (size_t)t->cols : 80;
    size_t row = t->hist.pinned + live; /* start_row + live - 1 */
    tui_write_literal("\033[");
    char num[16];
    snprintf(num, sizeof(num), "%zu", row);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();
    size_t disp = 0;
    if (have_partial) {
        fputs(t->cur, stdout);
        disp = cli_disp_width_of(t->cur, t->cur_len);
    } else {
        size_t idx = t->hist.pinned + start + live - 1;
        if (idx < t->hist.count) {
            fputs(t->hist.lines[idx], stdout);
            disp = cli_disp_width_of(t->hist.lines[idx],
                                     strlen(t->hist.lines[idx]));
        }
    }
    /* 清 wrap 溢出区（占用物理行数 - 1）；不越过输入行。 */
    size_t phys = disp > 0 ? (disp + cols - 1) / cols : 1;
    for (size_t i = 1; i < phys && row + i < (size_t)t->rows; i++) {
        tui_write_literal("\033[");
        snprintf(num, sizeof(num), "%zu", row + i);
        tui_write_literal(num);
        tui_write_literal(";1H");
        tui_clear_line();
    }
    tui_render_input(t);
    t->dirty = 0;
    fflush(stdout);
}

/* P1 重绘节流入口（emit 侧）：距上次重绘 >= TUI_REDRAW_MIN_MS 才实际
 * 重绘，否则仅挂起 redraw_pending（由下一次 emit / flush / readline 轮询
 * 消费）。逐字节流式输出因此合并为 ~60fps 的整帧刷新。 */
void tui_schedule_redraw(cli_tui_t *t)
{
    long long now = cli_now_ms();
    if (now - t->redraw_last_ms >= TUI_REDRAW_MIN_MS) {
        t->redraw_last_ms = now;
        t->redraw_pending = 0;
        tui_redraw_tail(t);
    } else {
        t->redraw_pending = 1;
    }
}
