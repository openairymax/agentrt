// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_live.c
 * @brief CLI 会话存活感知：per-daemon 心跳、网关代次比对与 stale 重建。
 *
 * 会话权威在 mem_d（见 cli_chat_session.c），但后端重建后 CLI 既有历史
 * 视图与在途会话已失效。此前该失效表现为无名 503 或"响应不是合法 JSON"，
 * 用户无法区分"服务重启"与"协议损坏"；本模块把失效判定前移到空闲节拍，
 * 使恢复动作与故障原因同时对用户可见。
 *
 * 判据分两层，且同时生效：
 *   - 网关代次（/health 的 epoch）：网关进程重建即全会话失效；
 *   - per-daemon health_check 的 down→up 迁移：daemon 进程重建即该服务
 *     的会话失效。两者互补——网关存活而单个 daemon 重启时只有迁移可见。
 * 超时/取消类失败视为"结果不确定"，保持既有视图；只有明确失败才登记
 * 为下线，防止模型短暂排队被误判为服务重启。
 *
 * 探测经 gateway 派发（客户端业务访问唯一入口），不直连 A-IPC，与
 * cli_chat_session.c 的会话恢复同源同路径。
 */

#include "cli_live.h"

#include "cli_gw.h"
#include "cli_render.h"
#include "cli_term.h"
#include "airy_memory.h"
#include "error_codes.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIVE_DEF_MS   2000  /* 默认探测周期 */
#define LIVE_MIN_MS   50    /* 周期下限：防止误配置成忙轮询 */
#define LIVE_MAX_MS   30000 /* 周期上限：资源受限环境的保底节拍 */
#define LIVE_PROBE_MS 200   /* 单项探测预算（本地回环，正常在毫秒级返回） */
#define LIVE_ROUND_MS 500   /* 单轮总预算：超预算让出输入，剩余顺延 */
#define LIVE_NAME_CAP 128   /* 失效名单聚合上限 */

typedef struct {
    const char *ns;
    unsigned char seen; /* 已建立过一次明确视图（首轮不判迁移） */
    unsigned char up;
} live_svc_t;

static live_svc_t g_live_svc[] = {
    {"mem", 0, 0}, {"think", 0, 0}, {"llm", 0, 0},
    {"tool", 0, 0}, {"agent", 0, 0}, {"sched", 0, 0},
};

#define LIVE_SVC_N (sizeof(g_live_svc) / sizeof(g_live_svc[0]))

static void (*g_live_heal)(void);
static uint64_t g_live_next;   /* 下次探测时刻（单调毫秒） */
static uint64_t g_live_epoch;  /* 已观测的网关代次 */
static unsigned char g_live_gw_seen;
static unsigned char g_live_ready;

static uint64_t live_period_ms(void)
{
    const char *env = getenv("AIRY_CLI_LIVE_MS");
    if (!env || !env[0])
        return LIVE_DEF_MS;
    char *end = NULL;
    unsigned long v = strtoul(env, &end, 10);
    if (end == env)
        return LIVE_DEF_MS;
    if (v == 0)
        return 0;
    if (v < LIVE_MIN_MS)
        v = LIVE_MIN_MS;
    if (v > LIVE_MAX_MS)
        v = LIVE_MAX_MS;
    return (uint64_t)v;
}

static void live_mark(char *buf, size_t cap, size_t *used, const char *name)
{
    if (!buf || !used || !name || *used >= cap)
        return;
    int n = snprintf(buf + *used, cap - *used, "%s%s", *used ? ", " : "", name);
    if (n > 0 && (size_t)n < cap - *used)
        *used += (size_t)n;
}

/* 1 = 在线；0 = 明确失败（下线/业务错误）；-1 = 结果不确定（超时/取消） */
static int live_probe(const char *ns)
{
    char method[64];
    snprintf(method, sizeof(method), "%s.health_check", ns);
    char *res = NULL;
    int rc = cli_gw_call(method, NULL, LIVE_PROBE_MS, &res);
    AIRY_FREE(res);
    if (rc == 0)
        return 1;
    if (rc == AIRY_ERR_TIMEOUT || rc == AIRY_ERR_CANCELED)
        return -1;
    return 0;
}

static void live_step(live_svc_t *svc, char *names, size_t cap, size_t *used)
{
    int state = live_probe(svc->ns);
    if (state < 0)
        return;
    /* 只认 down→up：下线时刻重建必然失败（mem.recent 也拿不到），
     * 且抖动会反复清空历史；服务重新上线才说明进程已重建、会话已失效。 */
    if (svc->seen && !svc->up && state == 1)
        live_mark(names, cap, used, svc->ns);
    svc->up = (unsigned char)state;
    svc->seen = 1;
}

/* 返回失效名单长度；0 = 无迁移（含网关不可达：状态不明时不动既有视图） */
static size_t live_round(char *names, size_t cap)
{
    unsigned long long epoch = 0;
    if (!cli_gw_probe(LIVE_PROBE_MS, &epoch))
        return 0;

    int moved = 0;
    if (epoch != 0) {
        moved = (g_live_gw_seen && epoch != g_live_epoch) ? 1 : 0;
        g_live_epoch = (uint64_t)epoch;
        g_live_gw_seen = 1;
    }

    size_t used = 0;
    uint64_t deadline = cli_now_ms() + LIVE_ROUND_MS;
    for (size_t i = 0; i < LIVE_SVC_N; i++) {
        if (cli_now_ms() >= deadline)
            break;
        live_step(&g_live_svc[i], names, cap, &used);
    }
    if (moved)
        live_mark(names, cap, &used, "gateway");
    return used;
}

static void live_emit(const char *names)
{
    if (!cli_term_is_tty())
        return;
    char msg[192];
    snprintf(msg, sizeof(msg), "检测到后端重启（%s），会话已失效，正在重建…", names);
    cli_out("\r\033[2K");
    cli_print_line("Lifecycle", msg);
    fflush(stdout);
}

void cli_live_init(void)
{
    for (size_t i = 0; i < LIVE_SVC_N; i++) {
        g_live_svc[i].seen = 0;
        g_live_svc[i].up = 0;
    }
    g_live_epoch = 0;
    g_live_gw_seen = 0;
    g_live_next = 0;
    g_live_ready = 1;
}

void cli_live_bind(void (*on_stale)(void))
{
    g_live_heal = on_stale;
}

void cli_live_tick(void)
{
    if (!g_live_ready)
        return;
    uint64_t period = live_period_ms();
    if (period == 0)
        return;
    uint64_t now = cli_now_ms();
    if (now < g_live_next)
        return;
    g_live_next = now + period;

    /* 探测走公共同一路径，会写失败原因缓冲；快照后恢复，业务诊断不被污染 */
    char saved[256];
    snprintf(saved, sizeof(saved), "%s", cli_gw_last_err());

    char names[LIVE_NAME_CAP];
    names[0] = '\0';
    size_t used = live_round(names, sizeof(names));

    cli_gw_err_set(saved);
    if (used == 0)
        return;
    live_emit(names);
    if (g_live_heal)
        g_live_heal();
}
