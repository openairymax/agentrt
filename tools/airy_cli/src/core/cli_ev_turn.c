// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_ev_turn.c
 * @brief 事件模式会话主循环：提交 run-stream、切片桥接事件、终态收敛。
 *
 * 事件模式的唯一事件源是 gateway 的 agent.run_stream：CLI 不在本地重放
 * 任务状态机，也不自行拼装帧语义。stdin 为管道时按帧持续服务多轮
 * 至 EOF；为终端时单回合返回（与 -p 语义一致）。
 */

#include "cli_ev.h"
#include "cli_gw.h"
#include "cli_render.h"
#include "cli_sse.h"
#include "airy_memory.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#define EV_IN_MAX (1024u * 1024u)
#define EV_ASK_MS 120000
#define EV_OPEN_MS 10000
#define EV_READ_MS 200
#define EV_STALL_MS 600000u /* 无任何事件的上限，超时按流丢失收尾 */
#define EV_GRACE_MS 3000u   /* run_end 后补收 error 帧的宽限 */

extern volatile sig_atomic_t g_cli_cancel;

#ifdef _WIN32
#define EV_TTY() (_isatty(_fileno(stdin)) != 0)
#else
#define EV_TTY() (isatty(fileno(stdin)) != 0)
#endif

/* 风险管理面：提交前扫描明显破坏性动作，交由反向控制通道裁决。 */
static const char *const EV_RISK[] = {
    "rm -rf",      "mkfs",      "dd if=",  "sudo ",  "shutdown",
    "reboot",      "chmod 777 /", ":(){",   "> /dev/sd", NULL
};

static int ev_risk(const char *s)
{
    int i;

    if (!s || !*s)
        return 0;
    for (i = 0; EV_RISK[i]; i++)
        if (strstr(s, EV_RISK[i]))
            return 1;
    return 0;
}

static char *ev_body(const char *goal, const char *opts)
{
    size_t n = strlen(goal ? goal : "");
    size_t xl = (opts && opts[0] == '{') ? strlen(opts) : 0;
    char *body = (char *)AIRY_MALLOC(2 * n + xl + 512);

    if (!body)
        return NULL;
    if (cli_ev_stream_body(body, 2 * n + xl + 512, goal, opts) == 0) {
        AIRY_FREE(body);
        return NULL;
    }
    return body;
}

static int ev_cancel(void)
{
    static const char pl[] = "{\"type\":\"run_end\",\"status\":\"cancelled\"}";

    if (!cli_ev_terminal())
        (void)cli_ev_emit("run_end", pl, sizeof(pl) - 1);
    return 130;
}

/* 循环前停止判定：用户取消或出口已关闭。 */
static int ev_pre_stop(int *cx)
{
    if (g_cli_cancel || cli_ev_in_cancel()) {
        *cx = 1;
        return 1;
    }
    return cli_ev_closed();
}

/* 读到帧后停止判定：终态宽限到期或增量停滞。 */
static int ev_post_stop(uint64_t now, uint64_t last, int term, uint64_t grace)
{
    if (term)
        return now >= grace;
    if (now - last >= EV_STALL_MS) {
        (void)cli_ev_note("run-stream stalled; aborting");
        return 1;
    }
    return 0;
}

/* 处理一帧：<0 中止，1 终态（记录状态码与宽限点），0 继续。 */
static int ev_frame_step(const char *buf, uint64_t now, uint64_t *grace,
                         int *trc, int *term)
{
    int t = cli_ev_bridge(buf, strlen(buf));

    if (t < 0)
        return -1;
    if (t == 1) {
        *term = 1;
        *trc = cli_ev_status(buf, strlen(buf));
        *grace = now + EV_GRACE_MS;
    }
    return 0;
}

static int ev_stream(const char *goal, const char *opts)
{
    char *body = ev_body(goal, opts);
    char *buf = (char *)AIRY_MALLOC(CLI_SSE_EVMAX);
    cli_sse_t *s;
    uint64_t last;
    uint64_t grace = 0;
    int term = 0;
    int trc = 1;
    int cx = 0;

    if (!body || !buf) {
        AIRY_FREE(body);
        AIRY_FREE(buf);
        (void)cli_ev_note("out of memory for run-stream request");
        (void)cli_ev_finish("oom");
        return 1;
    }
    s = cli_sse_open(CLI_GW_RUN_STREAM_PATH, body, EV_OPEN_MS);
    AIRY_FREE(body);
    if (!s) {
        AIRY_FREE(buf);
        (void)cli_ev_note("gateway run-stream unavailable");
        (void)cli_ev_finish("gateway_unreachable");
        return 1;
    }
    if (cli_sse_http(s) != 200) {
        cli_sse_close(s);
        AIRY_FREE(buf);
        (void)cli_ev_note("run-stream rejected by gateway");
        (void)cli_ev_finish("stream_rejected");
        return 1;
    }
    last = cli_now_ms();
    for (;;) {
        uint64_t now;
        int nr;

        if (ev_pre_stop(&cx))
            break;
        nr = cli_sse_next(s, buf, CLI_SSE_EVMAX, EV_READ_MS);
        now = cli_now_ms();
        if (nr > 0) {
            last = now;
            if (ev_frame_step(buf, now, &grace, &trc, &term) < 0)
                break;
        } else if (nr < 0) {
            break;
        } else {
            (void)cli_ev_in_poll();
            (void)cli_ev_beat();
        }
        if (ev_post_stop(now, last, term, grace))
            break;
    }
    cli_sse_close(s);
    AIRY_FREE(buf);
    if (cx)
        return ev_cancel();
    if (term)
        return trc;
    (void)cli_ev_finish("stream_lost");
    return 1;
}

static int ev_turn(const char *goal, const char *opts)
{
    if (ev_risk(goal) && !cli_ev_ask("shell.exec", goal, EV_ASK_MS)) {
        (void)cli_ev_note("high-risk action denied; submit aborted");
        (void)cli_ev_finish("denied");
        return 1;
    }
    return ev_stream(goal, opts);
}

int cli_ev_run(const char *prompt)
{
    char *buf = (char *)AIRY_MALLOC(EV_IN_MAX);
    char *opts = (char *)AIRY_MALLOC(EV_IN_MAX);
    int tty = EV_TTY();
    int more;
    int rc = 0;

    cli_ev_init();
    cli_ev_begin();
    if (!buf || !opts) {
        (void)cli_ev_note("out of memory for input buffer");
        (void)cli_ev_finish("oom");
        cli_ev_fini();
        AIRY_FREE(buf);
        AIRY_FREE(opts);
        return 1;
    }
    if (prompt && *prompt) {
        rc = ev_turn(prompt, NULL);
        more = !tty;
    } else {
        more = 1;
    }
    while (more && !cli_ev_closed() && !cli_ev_in_cancel()) {
        int pr = cli_ev_in_prompt(buf, EV_IN_MAX);

        if (pr <= 0)
            break;
        (void)cli_ev_in_opts(opts, EV_IN_MAX);
        if (!buf[0])
            continue;
        cli_ev_begin();
        rc = ev_turn(buf, opts);
        if (tty)
            break;
    }
    cli_ev_fini();
    AIRY_FREE(buf);
    AIRY_FREE(opts);
    return rc;
}
