// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_panel_dispatch.c
 * @brief TUI panel key-dispatch domain (split from cli_tui.c, 2026-08-27).
 *
 * 2026-08-27 域拆分（cli_tui.c 1260 行 → 7 文件）：
 *   - cli_tui.c              引擎骨架 + 生命周期 + readline 主循环
 *   - tui_keys.c             按键读取域
 *   - tui_input.c            输入编辑域
 *   - tui_ime.c              内置拼音输入法域
 *   - tui_history.c          历史与搜索域
 *   - tui_render.c           渲染域
 *   - tui_panel_dispatch.c   面板按键分派域（硬件信息/任务看板/事件流）
 *
 * 本文件从 cli_tui_readline 主循环中提取面板模式（非 CHAT 模式）的
 * 按键处理逻辑，封装为 tui_panel_dispatch()。readline 在检测到非 CHAT
 * 模式时调用此函数，所有按键均在此消费（返回 1），不再回到主循环的
 * 其他分支。
 */

#include "cli_tui_internal.h"

/**
 * @brief 面板模式按键分派（硬件信息/任务看板/事件流/记忆链）。
 *
 * 在 cli_tui_readline 主循环中，当 t->mode != CLI_TUI_MODE_CHAT 时调用。
 * 所有按键均在此处理：
 *   - 面板专有操作（导航/详情/过滤等）→ 执行并重绘，返回 1
 *   - "打字即退出浏览"的未识别按键 → 回退到对话模式并送入输入框，返回 1
 *
 * @param t   TUI 引擎句柄
 * @param key 按键码
 * @return 1（按键已消费，调用方应 continue 主循环）
 */
int tui_panel_dispatch(cli_tui_t *t, int key)
{
    /* 硬件信息面板：静态视图，Esc 返回对话，其余按键不进入面板动作。 */
    if (t->mode == CLI_TUI_MODE_HW) {
        if (key == 0x1b) {
            cli_tui_mode_set(t, CLI_TUI_MODE_CHAT);
            cli_tui_redraw(t);
            fflush(stdout);
        }
        return 1;
    }

    /* ---- 面板模式（任务看板/事件流）：可操作浏览 ---- */
    const cli_tui_panel_action_fn act = t->panel[t->mode].action;
    const void *ud = t->panel[t->mode].ud;
    size_t rows_page = tui_middle_rows(t) > 1 ? tui_middle_rows(t) - 1 : 1;

    /* 详情视图：Esc/Enter 返回列表 */
    if (t->detail_active) {
        if (key == 0x1b || key == '\n' || key == '\r') {
            t->detail_active = 0;
            t->detail_len = 0;
            cli_tui_redraw(t);
        }
        fflush(stdout);
        return 1;
    }

    if (t->mode == CLI_TUI_MODE_BOARD) {
        /* 看板计数：按键时重新拉取（同时刷新 entries 缓存） */
        size_t total = (t->panel[t->mode].count && ud)
                           ? t->panel[t->mode].count((void *)ud)
                           : 0;

        if (key == TUI_KEY_UP || key == TUI_KEY_DOWN ||
            key == TUI_KEY_PGUP || key == TUI_KEY_PGDN ||
            key == TUI_KEY_HOME || key == TUI_KEY_END) {
            if (key == TUI_KEY_UP && t->sel > 0)
                t->sel--;
            else if (key == TUI_KEY_DOWN && total > 0 && t->sel + 1 < total)
                t->sel++;
            else if (key == TUI_KEY_PGUP)
                t->sel = (t->sel > rows_page) ? t->sel - rows_page : 0;
            else if (key == TUI_KEY_PGDN && total > 0)
                t->sel = (t->sel + rows_page < total - 1) ? t->sel + rows_page
                                                          : total - 1;
            else if (key == TUI_KEY_HOME)
                t->sel = 0;
            else if (key == TUI_KEY_END && total > 0)
                t->sel = total - 1;
            cli_tui_redraw(t);
            return 1;
        }
        if (key == '\n' || key == '\r') {
            /* Enter：查看选中任务详情（DETAIL action 回填 detail）；
             * 空看板时 Enter 回对话。 */
            t->detail_len = 0;
            if (act && ud && total > 0 && t->sel < total) {
                t->detail[0] = '\0';
                if (act((void *)ud, CLI_TUI_ACT_DETAIL, t->sel, t->detail,
                        sizeof(t->detail)))
                    t->detail_len = strlen(t->detail);
                if (t->detail_len)
                    t->detail_active = 1;
            } else if (total == 0) {
                t->mode = CLI_TUI_MODE_CHAT;
            }
            cli_tui_redraw(t);
            fflush(stdout);
            return 1;
        }
        if (key == 'x' || key == 'X') {
            /* x：请求取消选中任务（结果提示进标题栏） */
            if (act && ud && total > 0 && t->sel < total)
                act((void *)ud, CLI_TUI_ACT_CANCEL, t->sel, t->note,
                    sizeof(t->note));
            cli_tui_redraw(t);
            fflush(stdout);
            return 1;
        }
        if (key == 0x1b) { /* Esc：返回对话 */
            t->mode = CLI_TUI_MODE_CHAT;
            cli_tui_redraw(t);
            fflush(stdout);
            return 1;
        }
    } else {
        /* 事件流：f 跟随 / c 过滤 / 方向键回放 */
        if (key == 'f' || key == 'F') {
            t->follow = !t->follow;
            t->scroll_off = 0;
            cli_tui_redraw(t);
            fflush(stdout);
            return 1;
        }
        if (key == 'c' || key == 'C') {
            if (act && ud)
                act((void *)ud, CLI_TUI_ACT_CYCLE_FILTER, 0, t->note,
                    sizeof(t->note));
            t->scroll_off = 0;
            cli_tui_redraw(t);
            fflush(stdout);
            return 1;
        }
        if (key == TUI_KEY_UP) {
            t->follow = 0;
            t->scroll_off++;
            cli_tui_redraw(t);
            return 1;
        }
        if (key == TUI_KEY_DOWN) {
            if (t->scroll_off > 0)
                t->scroll_off--;
            cli_tui_redraw(t);
            return 1;
        }
        if (key == TUI_KEY_PGUP) {
            t->follow = 0;
            t->scroll_off += rows_page;
            cli_tui_redraw(t);
            return 1;
        }
        if (key == TUI_KEY_PGDN) {
            t->follow = 0;
            t->scroll_off = (t->scroll_off > rows_page)
                                ? t->scroll_off - rows_page
                                : 0;
            cli_tui_redraw(t);
            return 1;
        }
        if (key == TUI_KEY_HOME) {
            t->follow = 0;
            t->scroll_off = SIZE_MAX; /* 渲染时 clamp 到面板末尾 */
            cli_tui_redraw(t);
            return 1;
        }
        if (key == TUI_KEY_END) {
            t->scroll_off = 0;
            t->follow = 1; /* 回到尾部 = 恢复跟随 */
            cli_tui_redraw(t);
            return 1;
        }
        if (key == 0x1b || key == '\n' || key == '\r') {
            t->mode = CLI_TUI_MODE_CHAT;
            cli_tui_redraw(t);
            fflush(stdout);
            return 1;
        }
    }

    /* 其他键：返回对话模式，并把该击键送入输入框（"打字即
     * 退出浏览"的 Claude Code 风格）。 */
    t->mode = CLI_TUI_MODE_CHAT;
    if (key >= 0x20 && key <= 0xFF) {
        tui_input_append(t, (char)key);
        /* UTF-8 完整序列才重绘（避免中文逐字节渲染乱码帧） */
        if (tui_input_utf8_complete(t->input, t->input_len))
            tui_render_input(t);
    } else {
        cli_tui_redraw(t);
    }
    fflush(stdout);
    return 1;
}
