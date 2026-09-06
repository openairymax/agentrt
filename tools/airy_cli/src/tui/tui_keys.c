// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file tui_keys.c
 * @brief TUI 引擎按键读取域（域拆分自 cli_tui.c，2026-08-27）。
 *
 * 包含跨平台按键读取（POSIX poll + Windows 控制台键事件翻译层）、
 * ESC 序列解析与 bracketed paste 结束序列识别。
 */

#include "cli_tui_internal.h"

/* ---- Windows 输入：控制台键事件 → 伪 VT 字节流 ----
 * tui_read_key 解析 ESC 序列，但 Windows 控制台不产生 raw 字节流。
 * 这里把 ReadConsoleInputW 的 KEY_EVENT 翻译为字节：普通字符按 UTF-8
 * （含代理对），方向/功能/修饰键翻译为 xterm VT 序列，使 POSIX 的
 * 按键解析代码在 Windows 上零改动复用。 */
#ifdef _WIN32

static char g_win_key_buf[16];    /* 翻译后的字节序列缓存 */
static size_t g_win_key_len = 0;
static size_t g_win_key_off = 0;
static WCHAR g_win_high_surrogate = 0; /* UTF-16 高代理暂存（跨事件） */

static void tui_win_flush_buf(const char *s, size_t n)
{
    if (n > sizeof(g_win_key_buf))
        n = sizeof(g_win_key_buf);
    memcpy(g_win_key_buf, s, n);
    g_win_key_len = n;
    g_win_key_off = 0;
}

/* UTF-16 单字符 → UTF-8（含代理对组合）。无内容时不动缓存。 */
static void tui_win_enqueue_wchar(WCHAR wc)
{
    char buf[4];
    size_t n;
    if (wc < 0x80) {
        buf[0] = (char)wc;
        n = 1;
    } else if (wc < 0x800) {
        buf[0] = (char)(0xC0 | (wc >> 6));
        buf[1] = (char)(0x80 | (wc & 0x3F));
        n = 2;
    } else if (wc < 0xD800 || wc > 0xDFFF) {
        /* BMP 非代理（中文等 3 字节字符） */
        buf[0] = (char)(0xE0 | (wc >> 12));
        buf[1] = (char)(0x80 | ((wc >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (wc & 0x3F));
        n = 3;
    } else if (wc >= 0xDC00) {
        /* 低代理：与暂存的高代理组合成 4 字节（无高代理则丢弃） */
        if (g_win_high_surrogate == 0)
            return;
        unsigned long cp = 0x10000 +
                (((unsigned long)(g_win_high_surrogate - 0xD800)) << 10) +
                (wc - 0xDC00);
        g_win_high_surrogate = 0;
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    } else {
        /* 高代理：暂存，等待随后的低代理事件 */
        g_win_high_surrogate = wc;
        return;
    }
    tui_win_flush_buf(buf, n);
}

static void tui_win_enqueue_key(WORD vk, WCHAR wc, DWORD ctl)
{
    const int ctrl = (ctl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    const int alt = (ctl & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
    const char *seq = NULL;

    switch (vk) {
    case VK_UP:     seq = "\x1b[A";      break;
    case VK_DOWN:   seq = "\x1b[B";      break;
    case VK_RIGHT:  seq = ctrl ? "\x1b[1;5C" : alt ? "\x1b[1;3C" : "\x1b[C"; break;
    case VK_LEFT:   seq = ctrl ? "\x1b[1;5D" : alt ? "\x1b[1;3D" : "\x1b[D"; break;
    case VK_HOME:   seq = "\x1b[H";      break;
    case VK_END:    seq = "\x1b[F";      break;
    case VK_PRIOR:  seq = "\x1b[5~";     break;
    case VK_NEXT:   seq = "\x1b[6~";     break;
    case VK_DELETE: seq = "\x1b[3~";     break;
    case VK_F6:     seq = "\x1b[17~";    break;
    case VK_F7:     seq = "\x1b[18~";    break;
    case VK_F8:     seq = "\x1b[19~";    break;
    case VK_TAB:    seq = "\t";          break;
    case VK_RETURN: seq = "\r";          break;
    case VK_BACK:   seq = "\x7f";        break; /* termios DEL，与 POSIX 一致 */
    case VK_ESCAPE: seq = "\x1b";        break;
    default:
        break;
    }
    if (seq) {
        tui_win_flush_buf(seq, strlen(seq));
        return;
    }
    if (alt && (vk == 'B' || vk == 'F')) {
        /* Alt+B / Alt+F：词左/右移（xterm 的 ESC b / ESC f）。
         * 注意 "\x1b" 须与 "b"/"f" 分字面量，否则 "\x1bb" 的十六进制转义
         * 吞掉 b 变成单字符 0x1bb（C7744 越界，#112 实证） */
        const char *ab = (vk == 'B') ? "\x1b" "b" : "\x1b" "f";
        tui_win_flush_buf(ab, 2);
        return;
    }
    if (ctrl && vk >= 'A' && vk <= 'Z') {
        /* Ctrl+letter → 控制字节 0x01-0x1A（Ctrl+C=0x03 等） */
        char b = (char)((vk - 'A') + 1);
        tui_win_flush_buf(&b, 1);
        return;
    }
    if (wc) {
        tui_win_enqueue_wchar(wc);
        return;
    }
    /* 无字符可翻译（Shift 等纯修饰键）：缓存保持空，调用方重试。 */
}

#endif /* _WIN32 */

/* 带超时的按键等待。timeout_ms < 0 无限等待（原阻塞语义）。
 * 返回 1 有数据（*out 有效）；0 = 超时（*eof=0）或 EOF（*eof=1）。 */
int tui_wait_byte(cli_tui_t *t, char *out, int timeout_ms, int *eof)
{
#ifdef _WIN32
    (void)t;
    /* 优先消耗上一个键事件翻译出的字节序列。 */
    if (g_win_key_off < g_win_key_len) {
        *out = g_win_key_buf[g_win_key_off++];
        *eof = 0;
        return 1;
    }
    g_win_key_len = g_win_key_off = 0;

    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    if (hin == INVALID_HANDLE_VALUE || hin == NULL) {
        *eof = 1;
        return 0;
    }
    DWORD ms = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    if (WaitForSingleObject(hin, ms) != WAIT_OBJECT_0) {
        *eof = 0;
        return 0;
    }
    if (GetFileType(hin) != FILE_TYPE_CHAR) {
        /* 重定向的管道/文件：逐字节读（与 POSIX read 语义一致）。 */
        char c;
        DWORD n = 0;
        if (ReadFile(hin, &c, 1, &n, NULL) && n == 1) {
            *out = c;
            *eof = 0;
            return 1;
        }
        *eof = 1;
        return 0;
    }
    /* 控制台：ReadConsoleInputW 取 KEY_EVENT，跳过非键事件后翻译。 */
    for (;;) {
        INPUT_RECORD rec;
        DWORD n = 0;
        if (!ReadConsoleInputW(hin, &rec, 1, &n) || n == 0) {
            if (WaitForSingleObject(hin, ms) != WAIT_OBJECT_0) {
                *eof = 0;
                return 0;
            }
            continue;
        }
        if (rec.EventType == KEY_EVENT &&
            rec.Event.KeyEvent.bKeyDown) {
            tui_win_enqueue_key(rec.Event.KeyEvent.wVirtualKeyCode,
                                rec.Event.KeyEvent.uChar.UnicodeChar,
                                rec.Event.KeyEvent.dwControlKeyState);
            if (g_win_key_len > 0) {
                *out = g_win_key_buf[g_win_key_off++];
                *eof = 0;
                return 1;
            }
        }
    }
#else
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int r;
    for (;;) {
        r = poll(&pfd, 1, timeout_ms);
        if (r >= 0 || errno != EINTR)
            break;
        /* SIGWINCH interrupted the wait: surface a resize tick so the caller
         * refreshes the geometry and redraws. */
        if (g_tui_resize_pending)
            break;
    }
    if (r <= 0) {
        *eof = 0; /* 超时 / poll 错误 / resize tick */
        return 0;
    }
    ssize_t n = read(STDIN_FILENO, out, 1);
    if (n == 1) {
        *eof = 0;
        return 1;
    }
    *eof = 1; /* read 0 = EOF */
    return 0;
#endif
}

/* 读取 bracketed-paste 结束序列的剩余字节（ESC 已被调用方消费，
 * 这里只读 "[201~" 5 字节）。返回 1 = 完整结束序列；0 = 不匹配。 */
int tui_paste_read_end(cli_tui_t *t)
{
    char want[] = {'[', '2', '0', '1', '~'};
    char got[sizeof(want)];
    for (size_t i = 0; i < sizeof(want); i++) {
        int eof = 0;
        if (!tui_wait_byte(t, &got[i], 50, &eof))
            return 0;
        if (got[i] != want[i])
            return 0;
    }
    return 1;
}

/* 读取一个按键（带第一字节超时）。返回键码；0 = EOF；-1 = 轮询超时
 * （*eof 保持 0；面板模式以此节拍刷新）。ESC 序列后续字节用 50ms
 * 短超时，避免孤立 ESC 键阻塞。 */
int tui_read_key(cli_tui_t *t, int timeout_ms, int *eof)
{
    char c;
    if (!tui_wait_byte(t, &c, timeout_ms, eof))
        return *eof ? 0 : -1;
    if (c == 0x1b) {
        char b;
        if (!tui_wait_byte(t, &b, 120, eof))
            return 0x1b; /* lone ESC */
        if (b == '[') {
            char x;
            if (!tui_wait_byte(t, &x, 120, eof))
                return TUI_KEY_UNKNOWN;
            switch (x) {
            case 'A': return TUI_KEY_UP;
            case 'B': return TUI_KEY_DOWN;
            case 'C': return TUI_KEY_RIGHT;
            case 'D': return TUI_KEY_LEFT;
            case 'H': return TUI_KEY_HOME;
            case 'F': return TUI_KEY_END;
            case '3': /* ESC [ 3 ~ = Delete; ESC [ 3 D / C = Alt+Left/Right */
                if (!tui_wait_byte(t, &b, 120, eof))
                    return TUI_KEY_UNKNOWN;
                if (b == '~')
                    return TUI_KEY_DEL;
                if (b == 'D')
                    return TUI_KEY_ALT_LEFT;
                if (b == 'C')
                    return TUI_KEY_ALT_RIGHT;
                return TUI_KEY_UNKNOWN;
            case '5': /* ESC [ 5 D / C = Ctrl+Left/Right; ESC [ 5 ~ = PageUp */
                if (!tui_wait_byte(t, &b, 120, eof))
                    return TUI_KEY_UNKNOWN;
                if (b == 'D')
                    return TUI_KEY_CTRL_LEFT;
                if (b == 'C')
                    return TUI_KEY_CTRL_RIGHT;
                if (b == '~')
                    return TUI_KEY_PGUP;
                return TUI_KEY_UNKNOWN;
            case '6': /* page down: ESC [ 6 ~ */
                if (tui_wait_byte(t, &b, 120, eof) && b == '~')
                    return TUI_KEY_PGDN;
                return TUI_KEY_UNKNOWN;
            case '1': /* ESC[11~=F1；ESC[12~=F2；ESC[13~=F3；ESC[14~=F4；
                       * ESC[15~=F5；ESC[17~=F6；ESC[18~=F7；ESC[19~=F8 */
            {
                char semi, mod, dir;
                if (!tui_wait_byte(t, &semi, 120, eof))
                    return TUI_KEY_UNKNOWN;
                if (semi == '1') { /* F1 (linux console): ESC [ 1 1 ~ */
                    if (!tui_wait_byte(t, &dir, 120, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F1;
                }
                if (semi == '3') { /* F3 (linux console): ESC [ 1 3 ~ */
                    if (!tui_wait_byte(t, &dir, 120, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F3;
                }
                if (semi == '9') { /* F8: ESC [ 1 9 ~ */
                    if (!tui_wait_byte(t, &dir, 120, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F8;
                }
                if (semi == '4') { /* F4 (linux console): ESC [ 1 4 ~ */
                    if (!tui_wait_byte(t, &dir, 120, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F4;
                }
                if (semi == '7') { /* F6: ESC [ 1 7 ~ */
                    if (!tui_wait_byte(t, &dir, 120, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F6;
                }
                if (semi == '8') { /* F7: ESC [ 1 8 ~ */
                    if (!tui_wait_byte(t, &dir, 120, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F7;
                }
                if (semi == '2') { /* F2: ESC [ 1 2 ~（xterm 标准 F2 序列） */
                    if (!tui_wait_byte(t, &dir, 120, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F2;
                }
                if (semi == '5') { /* F5: ESC [ 1 5 ~（xterm 标准 F5 序列） */
                    if (!tui_wait_byte(t, &dir, 120, eof) || dir != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F5;
                }
                if (semi != ';')
                    return TUI_KEY_UNKNOWN;
                if (!tui_wait_byte(t, &mod, 120, eof))
                    return TUI_KEY_UNKNOWN;
                if (!tui_wait_byte(t, &dir, 120, eof))
                    return TUI_KEY_UNKNOWN;
                if (mod == '5' && dir == 'D')
                    return TUI_KEY_CTRL_LEFT;
                if (mod == '5' && dir == 'C')
                    return TUI_KEY_CTRL_RIGHT;
                if (mod == '3' && dir == 'D')
                    return TUI_KEY_ALT_LEFT;
                if (mod == '3' && dir == 'C')
                    return TUI_KEY_ALT_RIGHT;
                return TUI_KEY_UNKNOWN;
            }
            case '2': /* ESC[20~ = F9；ESC[21~ = F10；ESC[200~ = paste start */
                if (!tui_wait_byte(t, &b, 120, eof))
                    return TUI_KEY_UNKNOWN;
                if (b == '0') { /* F9: ESC [ 2 0 ~；paste: ESC [ 2 0 0 ~ */
                    if (!tui_wait_byte(t, &b, 120, eof))
                        return TUI_KEY_UNKNOWN;
                    if (b == '~')
                        return TUI_KEY_F9;
                    if (b != '0')
                        return TUI_KEY_UNKNOWN;
                    if (!tui_wait_byte(t, &b, 120, eof) || b != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_PASTE_START;
                }
                if (b == '1') { /* F10: ESC [ 2 1 ~ */
                    if (!tui_wait_byte(t, &b, 120, eof) || b != '~')
                        return TUI_KEY_UNKNOWN;
                    return TUI_KEY_F10;
                }
                return TUI_KEY_UNKNOWN;
            case '<': /* SGR mouse: ESC [ < b ; r ; c M  (ignore) */
                return TUI_KEY_UNKNOWN;
            default:
                return TUI_KEY_UNKNOWN;
            }
        }
        /* Alt+letter: xterm sends ESC followed by the letter. */
        if (b == 'b')
            return TUI_KEY_ALT_B;
        if (b == 'f')
            return TUI_KEY_ALT_F;
        /* Application cursor mode (smkx): ESC O A/B/C/D = 方向键； */
        if (b == 'O') {
            char x;
            if (!tui_wait_byte(t, &x, 120, eof))
                return TUI_KEY_UNKNOWN;
            switch (x) {
            case 'A': return TUI_KEY_UP;
            case 'B': return TUI_KEY_DOWN;
            case 'C': return TUI_KEY_RIGHT;
            case 'D': return TUI_KEY_LEFT;
            case 'Q': return TUI_KEY_F2;  /* F2（应用键区 smkx：ESC O Q） */
            case 'S': return TUI_KEY_F4;
            case 'T': return TUI_KEY_F5;  /* F5（应用键区 smkx：ESC O T） */
            case 'P': return TUI_KEY_F1;  /* F1（macOS Terminal/部分终端 SS3） */
            case 'R': return TUI_KEY_F3;  /* F3（macOS Terminal/部分终端 SS3） */
            default:  return TUI_KEY_UNKNOWN;
            }
        }
        return TUI_KEY_UNKNOWN;
    }
    return (unsigned char)c;
}
