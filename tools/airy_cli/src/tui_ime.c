// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_ime.c
 * @brief TUI 引擎内置拼音输入法域（域拆分自 cli_tui.c，2026-08-27）。
 *
 * 2.2.3：无 IME 设备的中文输入路径。F10 中/英切换（默认，AIRY_IME_KEY
 * 可配置），拼音模式字母进 ime_buf 并实时查候选，数字/空格上屏，退格删
 * 拼音，,/. 或 PgUp/PgDn 翻页，←/→ 移动候选高亮，Enter 上屏高亮候选，
 * Esc 取消拼音。词典加载失败时 ime==NULL 功能整体禁用。
 */

#include "cli_tui_internal.h"

/* 拼音原文上屏：逐字节插入输入行光标处，清空拼音缓冲。 */
void tui_ime_commit_raw(cli_tui_t *t)
{
    for (size_t i = 0; i < t->ime_buf_len; i++)
        tui_input_append(t, t->ime_buf[i]);
    t->ime_buf_len = 0;
    t->ime_buf[0] = '\0';
    t->ime_cand_count = 0;
    t->ime_page = 0;
    t->ime_sel = 0;
}

/* 上屏候选：UTF-8 逐字节插入光标处（tui_input_append 支持中插），清空
 * 拼音缓冲并保持拼音模式，连续词组输入不中断（翻页/高亮归零）。 */
void tui_ime_commit_cand(cli_tui_t *t, const char *text)
{
    if (!text)
        return;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
        tui_input_append(t, (char)*p);
    t->ime_buf_len = 0;
    t->ime_buf[0] = '\0';
    t->ime_cand_count = 0;
    t->ime_page = 0;
    t->ime_sel = 0;
}

/* 以当前拼音缓冲刷新候选列表（微信式分页：候选池 27，每页 9 个）。
 * 拼音变化后重置页码与高亮（新输入上下文从第一页首候选开始）。 */
void tui_ime_refresh(cli_tui_t *t)
{
    t->ime_cand_count =
        airy_ime_query(t->ime, t->ime_buf, t->ime_cands,
                       (int)(sizeof(t->ime_cands) / sizeof(t->ime_cands[0])));
    t->ime_pages = (t->ime_cand_count + 8) / 9;
    if (t->ime_pages < 1)
        t->ime_pages = 1;
    if (t->ime_page >= t->ime_pages)
        t->ime_page = t->ime_pages - 1;
    if (t->ime_page < 0)
        t->ime_page = 0;
    if (t->ime_sel > 8)
        t->ime_sel = 8;
    if (t->ime_sel < 0)
        t->ime_sel = 0;
}

/* 当前高亮候选在候选池中的绝对下标（-1=无候选）。 */
int tui_ime_sel_index(const cli_tui_t *t)
{
    int idx = t->ime_page * 9 + t->ime_sel;
    if (idx < 0 || idx >= t->ime_cand_count)
        return -1;
    return idx;
}

/* 翻页（微信式，,/. 与 PgUp/PgDn）：越界回绕。 */
void tui_ime_page_flip(cli_tui_t *t, int dir)
{
    if (t->ime_pages <= 1)
        return;
    t->ime_page += dir;
    if (t->ime_page < 0)
        t->ime_page = t->ime_pages - 1;
    if (t->ime_page >= t->ime_pages)
        t->ime_page = 0;
}

/* 绘制拼音候选条（输入行上方一行，微信式分页）：拼音高亮 + 当前页
 * 数字键候选（页内高亮以蓝底标记）+ 页码指示（多页时显示 ‹1/2›）。
 * 返回 1=已绘制（占用该行）；0=无拼音态（调用方继续画分隔线等）。 */
int tui_ime_draw_cands(cli_tui_t *t, int input_row)
{
    if (!t->ime || !t->ime_active || t->ime_buf_len == 0)
        return 0;
    char num[16];
    size_t brow = input_row > 1 ? (size_t)input_row - 1 : 1;
    tui_write_literal("\033[");
    snprintf(num, sizeof(num), "%zu", brow);
    tui_write_literal(num);
    tui_write_literal(";1H");
    tui_clear_line();
    /* 拼音高亮：紫（轻盈科技感，区别于对话区青/蓝） */
    fputs(cli_c(CLR_BOLD), stdout);
    fputs(cli_c(CLR_MAGENTA), stdout);
    fwrite(t->ime_buf, 1, t->ime_buf_len, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    fputs(" ", stdout);
    size_t used = cli_disp_width(t->ime_buf) + 1;
    /* 当前页切片：page*9 .. min(page*9+9, count) */
    int start = t->ime_page * 9;
    int end = start + 9;
    if (end > t->ime_cand_count)
        end = t->ime_cand_count;
    for (int i = start; i < end; i++) {
        const char *txt = t->ime_cands[i].text;
        char tag[4];
        snprintf(tag, sizeof(tag), "%d", (i - start) + 1);
        size_t w = cli_disp_width(txt) + strlen(tag) + 1;
        if (used + w > (size_t)t->cols)
            break;
        if (i == t->ime_page * 9 + t->ime_sel) {
            /* 页内高亮：反显 + 加粗 + 青前景（不依赖蓝底，深浅色终端均醒目） */
            fputs(cli_c(CLR_REVERSE), stdout);
            fputs(cli_c(CLR_BOLD), stdout);
            fputs(cli_c(CLR_CYAN), stdout);
        } else {
            fputs(cli_c(CLR_DIM), stdout);
        }
        fputs(tag, stdout);
        fputs(txt, stdout);
        fputs(cli_c(CLR_RESET), stdout);
        fputs(" ", stdout);
        used += w;
    }
    /* 页码指示：多页时尾部显示 ‹cur/total›（微信式翻页反馈） */
    if (t->ime_pages > 1) {
        char pgbuf[32];
        snprintf(pgbuf, sizeof(pgbuf), " ‹%d/%d›", t->ime_page + 1, t->ime_pages);
        if (used + (size_t)strlen(pgbuf) + 2 <= (size_t)t->cols) {
            fputs(cli_c(CLR_DIM), stdout);
            fputs(pgbuf, stdout);
            fputs(cli_c(CLR_RESET), stdout);
        }
    }
    fflush(stdout);
    return 1;
}

/* 返回可执行文件所在目录（无尾分隔符），失败返回 -1。用于二进制包
 * 解压即用布局：词典在 <exe>/../share/agentrt/ime/airy_ime.dat。 */
static int tui_exe_dir(char *buf, size_t cap)
{
    if (!buf || cap < 2)
        return -1;
#ifdef _WIN32
    DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)cap);
    if (n == 0 || n >= (DWORD)cap)
        return -1;
#else
    ssize_t n = readlink("/proc/self/exe", buf, cap - 1);
    if (n <= 0)
        return -1;
    buf[n] = '\0';
#endif
    char *slash = strrchr(buf, TUI_PATH_SEP);
#ifdef _WIN32
    char *bslash = strrchr(buf, '/');
    if (bslash && (!slash || bslash > slash))
        slash = bslash;
#endif
    if (!slash)
        return -1;
    *slash = '\0';
    return (int)strlen(buf);
}

/* 加载内置拼音词典（2.2.3）。路径优先级：AIRY_IME_DICT 环境变量 →
 * $AIRY_HOME/share/agentrt/ime/airy_ime.dat（安装布局）→ 可执行文件
 * 同级上溯 share/agentrt/ime/airy_ime.dat（二进制包解压即用）→ 当前
 * 目录 share/agentrt/ime/airy_ime.dat（开发/便携布局）。airy_ime_load
 * 对不存在/损坏文件 fail-closed 返回 NULL；全部失败返回 NULL（输入法
 * 整体降级禁用，英文输入不受影响）。 */
airy_ime_t *tui_ime_load_dict(void)
{
    const char *dict = getenv("AIRY_IME_DICT");
    if (dict && dict[0]) {
        airy_ime_t *ime = airy_ime_load(dict);
        if (ime)
            return ime;
    }
    const char *home = airy_home_dir();
    if (home && home[0]) {
        char path[AIRY_PATH_MAX];
        snprintf(path, sizeof(path), "%s/share/agentrt/ime/airy_ime.dat", home);
        airy_ime_t *ime = airy_ime_load(path);
        if (ime)
            return ime;
    }
    /* 二进制包解压即用：bin/ 与 share/ 平级，跳过 $AIRY_HOME 缺失时
     * 用户直接从包解压运行 airy_cli（未安装）也能用内置输入法。 */
    {
        char exe_dir[AIRY_PATH_MAX];
        if (tui_exe_dir(exe_dir, sizeof(exe_dir)) > 0) {
            char path[AIRY_PATH_MAX];
            snprintf(path, sizeof(path),
                     "%s/../share/agentrt/ime/airy_ime.dat", exe_dir);
            airy_ime_t *ime = airy_ime_load(path);
            if (ime)
                return ime;
        }
    }
    return airy_ime_load("share/agentrt/ime/airy_ime.dat");
}

/* 解析中/英切换键（2.2.3 可配置）：环境变量 AIRY_IME_KEY=f9/f10/both，
 * 默认 F10 主键 + F9 备键。F11 因终端模拟器全屏冲突不可用，显式拒绝。 */
int tui_ime_key_resolve(void)
{
    const char *key = getenv("AIRY_IME_KEY");
    if (key && key[0]) {
        if (strcmp(key, "f9") == 0)
            return TUI_KEY_F9;
        if (strcmp(key, "f10") == 0)
            return TUI_KEY_F10;
        if (strcmp(key, "both") == 0)
            return TUI_KEY_F10;
        /* 未知值回落默认，不阻断启动 */
    }
    return TUI_KEY_F10;
}

/* 备键解析：AIRY_IME_KEY=f9/f10 时备键为对侧；both/默认时备键 F9。 */
int tui_ime_key_alt_resolve(void)
{
    const char *key = getenv("AIRY_IME_KEY");
    if (key && key[0]) {
        if (strcmp(key, "f9") == 0)
            return TUI_KEY_F10;
        if (strcmp(key, "f10") == 0)
            return TUI_KEY_F9;
        /* both / 未知值：备键 F9 */
    }
    return TUI_KEY_F9;
}

/* 中/英切换键命中判定：主键或备键任一即命中。 */
int tui_ime_key_hit(const cli_tui_t *t, int key)
{
    if (!t->ime)
        return 0;
    return key == t->ime_key || key == t->ime_key_alt;
}
