// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_cmds.c
 * @brief airy_cli builtin command domain: /help /clear /status /quit.
 *
 * Four local daemon-independent commands: command list (walks CLI_COMMANDS),
 * clear screen and chat context, work-hall status, and quit flag.
 * Daemon-related commands (/daemons /rpc etc.) live in daemon_cmds.c.
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_help(const char *arg, void *ctx)
{
    (void)arg;
    (void)ctx;
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, NULL, "可用命令");
    size_t ncmds = cli_commands_count();
    for (size_t i = 0; i < ncmds; i++) {
        cli_outf("    %s%-8s%s  %s\n", cli_c(CLR_CYAN), CLI_COMMANDS[i].name, cli_c(CLR_RESET),
               CLI_COMMANDS[i].desc);
    }
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, NULL,
                         "普通输入直接对话或下达任务指令。");
    return 0;
}

int cmd_clear(const char *arg, void *ctx)
{
    (void)arg;
    (void)ctx;
    cli_history_clear();
#ifdef _WIN32
    system("cls");
#else
    cli_outf("\033[2J\033[H");
#endif
    /* Re-render the pinned blue-framed system header so the clear keeps the
     * fixed-header layout consistent (models unified with model.yaml). */
    char t2[128], t1f[128], t1p[128];
    cli_think_cfg_load(t2, sizeof(t2), t1f, sizeof(t1f), t1p, sizeof(t1p));
    cli_print_system_header(t2[0] ? t2 : NULL, t1f[0] ? t1f : NULL,
                            t1p[0] ? t1p : NULL);
    return 0;
}

int cmd_status(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (!c || !c->hall) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "状态",
                             "任务大厅不可用");
        return 0;
    }

    airy_work_hall_entry_t **entries = NULL;
    size_t count = 0;
    airy_err_t err = airy_work_hall_list(c->hall, &entries, &count);
    if (err != AIRY_SUCCESS) {
        char line[128];
        snprintf(line, sizeof(line), "任务大厅查询失败：%s", cli_err_desc((int)err));
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "状态", line);
        return 0;
    }

    if (count == 0) {
        if (g_cli_print_mode) {
            cli_outf("hall idle\n");
        } else {
            cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "status",
                                 "任务大厅空闲，尚无执行实例");
        }
        return 0;
    }

    char line[160];
    snprintf(line, sizeof(line), "任务大厅 · %zu 个执行实例", count);
    if (g_cli_print_mode) {
        cli_outf("hall instances=%zu\n", count);
    } else {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "status", line);
    }
    for (size_t i = 0; i < count; i++) {
        airy_work_hall_entry_t *e = entries[i];
        if (g_cli_print_mode)
            cli_outf("  %s %s %.0f%%\n", e->execution_id, e->state, e->progress * 100.0);
        else
            cli_render_task_line("hall", e->execution_id, e->state, e->progress);
    }
    airy_work_hall_list_free(entries, count);
    return 0;
}

/* ================================================================
 * /chain 决策链可视化（阶段 4，2026-08-15）
 *
 * 底座：⑥单一真相源事件流（hall_store gseq）。CLI 决策点以 "cognition"
 * 角色写入 CHAIN/COMMAND 事件（preflight 任务聚合 GCCP/蓝图命中/计划，
 * exec_id 任务聚合提交/进度/结果/复核），本命令按 gseq 全局因果序合并
 * 全部类别事件并渲染，还原真实决策链：GCCP 五问 → 蓝图 L1/L2/L3 命中 →
 * 计划 → 提交 → 任务大厅复核/终裁。
 * ================================================================ */

#define CLI_CHAIN_CONTENT_CAP 512

/* 提取 "gseq":<digits>（事件流全局因果序）——导出供 cli_panel.c 事件流面板复用 */
uint64_t cli_chain_extract_gseq(const char *json)
{
    const char *p = strstr(json, "\"gseq\":");
    if (!p)
        return 0;
    p += 7;
    uint64_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (uint64_t)(*p - '0');
        p++;
    }
    return v;
}

/* 提取 "seq":<digits>（事件文件序号；header 首现。跨进程事件流的稳定
 * 全局序为 (ts_utc, seq)——gseq 进程内单调，跨进程会撞号不可用作排序） */
uint32_t cli_chain_extract_seq(const char *json)
{
    const char *p = strstr(json, "\"seq\":");
    if (!p)
        return 0;
    p += 6;
    uint32_t v = 0;
    while (*p >= '0' && *p <= '9') {
        v = v * 10 + (uint32_t)(*p - '0');
        p++;
    }
    return v;
}

/* 提取 "content":{...}（去掉事件信封，返回 content 对象自身） */
void cli_chain_extract_content(const char *json, char *out, size_t cap)
{
    out[0] = '\0';
    if (!json || cap == 0)
        return;
    const char *p = strstr(json, "\"content\":");
    if (!p)
        return;
    p += 10; /* 跳过 "content": */
    size_t len = strlen(p);
    /* p 以 "{" 开头；末尾的 "}" 是事件信封收尾（内容对象自身 "}" 在其前一位） */
    if (len > 0)
        len--;
    if (len >= cap)
        len = cap - 1;
    __builtin_memcpy(out, p, len);
    out[len] = '\0';
}

/* 提取 "key":"value" 的原始 value（不反转义；找不到返回 0） */
int cli_chain_str_field(const char *json, const char *key, char *out, size_t cap)
{
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (!p)
        return 0;
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < cap)
        out[i++] = *p++;
    out[i] = '\0';
    return i > 0;
}

/* 类别 + content → 人类可读标签 */
void cli_chain_label(int cat, const char *content, char *out, size_t cap)
{
    char v[128] = "";
    switch (cat) {
    case AIRY_HALL_CAT_CHAIN:
        if (cli_chain_str_field(content, "event", v, sizeof(v))) {
            if (strcmp(v, "gccp_confirm") == 0)
                snprintf(out, cap, "GCCP 意图确认");
            else if (strcmp(v, "blueprint_hit") == 0) {
                char layer[16] = "";
                cli_chain_str_field(content, "layer", layer, sizeof(layer));
                snprintf(out, cap, "蓝图命中 %s", layer[0] ? layer : "?"); /* layer 形如 "L1" */
            } else {
                snprintf(out, cap, "决策 %s", v);
            }
        } else {
            snprintf(out, cap, "决策链事件");
        }
        break;
    case AIRY_HALL_CAT_COMMAND: {
        char dag[64] = "", pid[64] = "";
        if (cli_chain_str_field(content, "dag_id", dag, sizeof(dag)))
            snprintf(out, cap, "提交 dag=%s", dag);
        else if (cli_chain_str_field(content, "plan_id", pid, sizeof(pid)))
            snprintf(out, cap, "计划 %s", pid);
        else
            snprintf(out, cap, "指令");
        break;
    }
    case AIRY_HALL_CAT_VERIFY: {
        char verdict[32] = "", reason[160] = "";
        cli_chain_str_field(content, "verdict", verdict, sizeof(verdict));
        cli_chain_str_field(content, "reason", reason, sizeof(reason));
        if (verdict[0])
            snprintf(out, cap, "复核 %s%s%s", verdict, reason[0] ? " · " : "", reason);
        else
            snprintf(out, cap, "复核");
        break;
    }
    case AIRY_HALL_CAT_PROGRESS: {
        char st[32] = "", nid[64] = "";
        cli_chain_str_field(content, "status", st, sizeof(st));
        cli_chain_str_field(content, "node_id", nid, sizeof(nid));
        if (st[0])
            snprintf(out, cap, "进度 %s%s%s", nid, nid[0] ? " " : "", st);
        else
            snprintf(out, cap, "进度");
        break;
    }
    case AIRY_HALL_CAT_RESULT:
        snprintf(out, cap, "结果");
        break;
    case AIRY_HALL_CAT_ISSUE:
        snprintf(out, cap, "问题");
        break;
    default:
        snprintf(out, cap, "%s",
                 airy_hall_category_name((airy_hall_category_t)cat));
    }
}

/* 长 content 截断（展示用，保持单行） */
static const char *cli_chain_trim(const char *s)
{
    static char buf[CLI_CHAIN_CONTENT_CAP + 16];
    size_t len = strlen(s);
    if (len <= 120)
        snprintf(buf, sizeof(buf), "%s", s);
    else {
        __builtin_memcpy(buf, s, 117);
        buf[117] = '.'; buf[118] = '.'; buf[119] = '.';
        buf[120] = '\0';
    }
    return buf;
}

static int cli_chain_render_task(const char *task_id)
{
    enum { MAX_EV = 4096 };
    typedef struct {
        uint64_t gseq;
        int cat;
        char *content;
    } chain_ev_t;
    chain_ev_t evs[MAX_EV];
    size_t nev = 0;

    for (int cat = 0; cat < AIRY_HALL_CAT_MAX && nev < MAX_EV; cat++) {
        char **jsons = NULL;
        size_t cnt = 0;
        airy_err_t err = airy_hall_store_replay(g_cli_hall_store, "default", task_id,
                                                (airy_hall_category_t)cat, &jsons, &cnt);
        if (err != AIRY_SUCCESS || cnt == 0)
            continue;
        for (size_t i = 0; i < cnt && nev < MAX_EV; i++) {
            char content[CLI_CHAIN_CONTENT_CAP];
            uint64_t gseq = cli_chain_extract_gseq(jsons[i]);
            cli_chain_extract_content(jsons[i], content, sizeof(content));
            evs[nev].gseq = gseq;
            evs[nev].cat = cat;
            evs[nev].content = AIRY_STRDUP(content);
            if (!evs[nev].content)
                break;
            nev++;
        }
        airy_hall_store_free_strings(jsons, cnt);
    }

    if (nev == 0) {
        char line[192];
        snprintf(line, sizeof(line), "任务 %s 无事件（本会话未产生该任务的事件流）", task_id);
        if (g_cli_print_mode)
            cli_outf("chain task=%s events=0\n", task_id);
        else
            cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "chain", line);
        return 0;
    }

    /* 按 gseq 插入排序（跨类别合并，小规模） */
    for (size_t i = 1; i < nev; i++) {
        chain_ev_t tmp = evs[i];
        size_t j = i;
        while (j > 0 && evs[j - 1].gseq > tmp.gseq) {
            evs[j] = evs[j - 1];
            j--;
        }
        evs[j] = tmp;
    }

    if (g_cli_print_mode) {
        /* One-shot server 模式：纯文本行（决策链按 gseq 因果序） */
        cli_outf("chain task=%s events=%zu\n", task_id, nev);
        for (size_t i = 0; i < nev; i++) {
            const char *catn = airy_hall_category_name((airy_hall_category_t)evs[i].cat);
            cli_outf("  %s %llu %s\n", catn, (unsigned long long)evs[i].gseq,
                     cli_chain_trim(evs[i].content));
        }
        for (size_t i = 0; i < nev; i++)
            AIRY_FREE(evs[i].content);
        return 0;
    }

    char head[256];
    snprintf(head, sizeof(head), "决策链 %s · %zu 事件（gseq 全局因果序）", task_id, nev);
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "chain", head);
    for (size_t i = 0; i < nev; i++) {
        char label[256];
        cli_chain_label(evs[i].cat, evs[i].content, label, sizeof(label));
        const char *catn = airy_hall_category_name((airy_hall_category_t)evs[i].cat);
        cli_outf("  %s●%s [%s:%03llu] %s%s%s\n", cli_c(CLR_CYAN), cli_c(CLR_RESET), catn,
                 (unsigned long long)evs[i].gseq, cli_c(CLR_BOLD), label, cli_c(CLR_RESET));
        cli_outf("      %s%s%s\n", cli_c(CLR_DIM), cli_chain_trim(evs[i].content),
                 cli_c(CLR_RESET));
    }
    for (size_t i = 0; i < nev; i++)
        AIRY_FREE(evs[i].content);
    return 0;
}

int cmd_chain(const char *arg, void *ctx)
{
    (void)ctx;
    if (!g_cli_hall_store) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "事件流",
                             "事件流不可用（hall store 未创建）");
        return 0;
    }
    if (arg && arg[0])
        return cli_chain_render_task(arg);

    /* 无参数：列出事件流任务（最新在前） */
    char **tasks = NULL;
    size_t n = 0;
    airy_err_t err = airy_hall_store_list_tasks(g_cli_hall_store, "default", &tasks, &n);
    if (err != AIRY_SUCCESS) {
        char line[128];
        snprintf(line, sizeof(line), "任务枚举失败：%s", cli_err_desc((int)err));
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "事件流", line);
        return 0;
    }
    if (n == 0) {
        if (g_cli_print_mode)
            cli_outf("chain tasks=0\n");
        else
            cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "chain",
                                 "事件流为空：执行过任务后可用 /chain 查看真实决策链");
        return 0;
    }
    if (g_cli_print_mode) {
        cli_outf("chain tasks=%zu\n", n);
        for (size_t i = 0; i < n; i++)
            cli_outf("  %s\n", tasks[i]);
        airy_hall_store_free_strings(tasks, n);
        return 0;
    }
    char line[128];
    snprintf(line, sizeof(line), "事件流任务 · %zu 个", n);
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUPER_AGENT, "chain", line);
    for (size_t i = 0; i < n; i++) {
        cli_outf("    %s◇%s %s%s%s\n", cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_BOLD),
                 tasks[i], cli_c(CLR_RESET));
    }
    cli_outf("    %s查看单任务决策链：/chain <task_id>（如 /chain preflight）%s\n",
             cli_c(CLR_DIM), cli_c(CLR_RESET));
    airy_hall_store_free_strings(tasks, n);
    return 0;
}

int cmd_quit(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (c && c->quit)
        *c->quit = 1;
    return 0;
}

/* 2026-08-17：/tui 切换到图形 TUI（agentrt-tui）。
 * 定位同 $AIRY_HOME/bin 下的 agentrt-tui（由 airymaxrt 启动器保证两个
 * 前端二进制同装），仅当 CLI 运行于全屏 TUI 页面时切换才有意义——该
 * 页面激活说明终端已就绪，exec 后由 agentrt-tui 接管同一终端。切换请求
 * 只置标志，真正的 exec 发生在主循环清理之后（main.c），保证无进程嵌套。 */
int cmd_tui(const char *arg, void *ctx)
{
    (void)arg;
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (c && c->quit)
        *c->quit = 1;
    if (c && c->switch_tui)
        *c->switch_tui = 1;
    return 0;
}
