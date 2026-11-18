// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_ev_bridge.c
 * @brief 服务端 run-stream 事件 → CLI 事件帧的桥接与终态收尾。
 *
 * 桥接不新造语义：事件对象原样作为 payload，CLI 只补帧头并按类型推进
 * 运行状态。run_end 之后服务端仍可能补一个 error 帧（终态与失败载荷
 * 分开发送），故本层只标记终态、不要求调用方立刻停读。
 */

#include "cli_ev.h"

#include <stdio.h>
#include <string.h>

int cli_ev_bridge(const char *json, size_t len)
{
    char type[24];
    char st[24];

    if (!json || len == 0 || !cli_ev_typeof(json, len, type, sizeof(type))) {
        (void)cli_ev_note("event dropped: missing or invalid type");
        return 0;
    }
    if (cli_ev_is_term(type)) {
        cli_ev_state("done");
        return cli_ev_emit(type, json, len) == 0 ? 1 : -1;
    }
    if (strcmp(type, "run_start") == 0) {
        cli_ev_state("running");
    } else if (strcmp(type, "task_state") == 0 &&
               cli_ev_str(json, len, "state", st, sizeof(st)) > 0) {
        cli_ev_state(st);
    }
    return cli_ev_emit(type, json, len) == 0 ? 0 : -1;
}

/* 终态状态语义：completed/paused/unverified 都是"本轮有可交付产物"，取消
 * 与失败才落非零退出码。unverified 是判据取证不足而非执行失败，退出码
 * 不冒充失败，状态由帧载荷自证。 */
int cli_ev_status(const char *json, size_t len)
{
    char st[24];

    if (!json || cli_ev_str(json, len, "status", st, sizeof(st)) <= 0)
        return 1;
    if (strcmp(st, "completed") == 0 || strcmp(st, "paused") == 0 ||
        strcmp(st, "unverified") == 0)
        return 0;
    if (strcmp(st, "cancelled") == 0 || strcmp(st, "canceled") == 0)
        return 130;
    return 1;
}

int cli_ev_finish(const char *code)
{
    char esc[48];
    char pl[256];
    int n;

    if (cli_ev_terminal())
        return 0;
    (void)cli_ev_esc(esc, sizeof(esc), code ? code : "no_terminal");
    n = snprintf(pl, sizeof(pl),
                 "{\"type\":\"error\",\"code\":\"%s\","
                 "\"message\":\"stream ended without terminal frame\"}",
                 esc);
    if (n > 0 && (size_t)n < sizeof(pl))
        (void)cli_ev_emit("error", pl, (size_t)n);
    n = snprintf(pl, sizeof(pl),
                 "{\"type\":\"run_end\",\"status\":\"failed\",\"code\":\"%s\"}", esc);
    if (n <= 0 || (size_t)n >= sizeof(pl))
        return -1;
    return cli_ev_emit("run_end", pl, (size_t)n) == 0 ? 1 : -1;
}
