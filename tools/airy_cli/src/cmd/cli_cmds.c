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
#include "cli_gw.h"
#include "cli_gw_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_help(const char *arg, void *ctx)
{
    (void)ctx;
    /* /help <cmd>：单命令用法查询（社区反馈②）——精确/唯一缩写命中
     * 展示描述与 usage 权威参数形态；多义并列候选交用户裁决；未知命令
     * 显式提示。匹配形态与 cli_dispatch_command 完全一致。 */
    if (arg && arg[0]) {
        char tok[CLI_CMD_TOK_MAX];
        cli_cmd_token(arg, tok, sizeof(tok));
        const cli_command_t *cmd = cli_cmd_exact(tok);
        if (cmd) {
            cli_outf("  %s%s%s  %s\n", cli_c(CLR_CYAN), cmd->name,
                   cli_c(CLR_RESET), cmd->desc);
            cli_outf("  用法：%s\n", cmd->usage);
            return 0;
        }
        int hits = cli_cmd_prefix(tok, &cmd);
        if (hits == 1) {
            cli_outf("  （%s 为 %s%s%s 缩写）\n", tok, cli_c(CLR_CYAN),
                   cmd->name, cli_c(CLR_RESET));
            cli_outf("  %s  %s\n", cmd->name, cmd->desc);
            cli_outf("  用法：%s\n", cmd->usage);
            return 0;
        }
        if (hits > 1) {
            cli_cmd_list(tok);
            return 0;
        }
        cli_outf("  %s未知命令%s %s，输入 %s/help%s 查看全部命令。\n",
               cli_c(CLR_YELLOW), cli_c(CLR_RESET), tok, cli_c(CLR_CYAN),
               cli_c(CLR_RESET));
        return 0;
    }
    /* 分组顺序与 CLI_CAT_* 枚举一致（会话/系统/资源/安全） */
    static const struct {
        cli_cmd_category_t cat;
        const char        *title;
    } groups[] = {
        {CLI_CAT_SESSION, "会话"},
        {CLI_CAT_SYSTEM, "系统"},
        {CLI_CAT_RESOURCE, "资源"},
        {CLI_CAT_SECURITY, "安全"},
    };
    cli_print_status("Super Agent", "可用命令（按类别）");
    size_t ncmds = cli_commands_count();
    for (size_t g = 0; g < sizeof(groups) / sizeof(groups[0]); g++) {
        cli_outf("  ── %s ──\n", groups[g].title);
        for (size_t i = 0; i < ncmds; i++) {
            if (CLI_COMMANDS[i].category != groups[g].cat)
                continue;
            cli_outf("    %-8s  %s\n", CLI_COMMANDS[i].name, CLI_COMMANDS[i].desc);
        }
    }
    cli_print_status("Super Agent", "普通输入直接对话或下达任务指令。");
    cli_outf("  快捷键：F8 全屏↔行渲染切换 · F10/F9 内置输入法"
             "中/英切换（AIRY_IME_KEY=f9/f10/both 可配置）\n");
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
    (void)ctx;
    /* 0.1.9 M1-1c 查询面迁移：/status 改经 gateway → sched.dag_list 展示
     * sched_d 权威执行实例（本地 work_hall 已无写入方，C2d 退役实例本体）。 */
    cli_dag_item_t items[32];
    size_t count = 0;
    airy_err_t err = cli_dag_list_remote(items, 32, &count);
    if (err != AIRY_EOK) {
        char line[128];
        snprintf(line, sizeof(line), "任务列表查询失败：%s", cli_err_desc((int)err));
        cli_print_line("Super Agent:状态", line);
        return 0;
    }

    if (count == 0) {
        if (g_cli_print_mode) {
            cli_outf("sched idle\n");
        } else {
            cli_print_status("Super Agent:status",
                             "调度器空闲，无活动任务");
        }
        return 0;
    }

    char line[160];
    snprintf(line, sizeof(line), "sched_d · %zu 个任务", count);
    if (g_cli_print_mode) {
        cli_outf("sched dags=%zu\n", count);
    } else {
        cli_print_status("Super Agent:status", line);
    }
    for (size_t i = 0; i < count; i++) {
        cli_dag_item_t *it = &items[i];
        double prog = (it->node_count > 0)
                          ? ((double)it->done / (double)it->node_count)
                          : 0.0;
        if (g_cli_print_mode) {
            cli_outf("  %s %s %.0f%%\n", it->dag_id, it->status, prog * 100.0);
        } else {
            char hdr[96], txt[64];
            snprintf(hdr, sizeof(hdr), "sched_d:%s", it->dag_id);
            snprintf(txt, sizeof(txt), "%s %.0f%%", cli_state_cn(it->status),
                     prog * 100.0);
            cli_print_status(hdr, txt);
        }
    }
    return 0;
}

/* ================================================================
 * /chain 决策链可视化（0.1.21 C-18-05：读侧统一到 gateway hall.*）
 *
 * 底座：工作大厅事件账本（agent_d 单点写盘，主键 = task_id，chat 回退
 * session_id）。CLI 不再持有进程内 hall_store，也不再以 "preflight" 等
 * 自造任务名写事件；/chain 与 /hall 一律经 gateway 只读视图读取同一份
 * 账本：
 *   hall.replay {task_id}  单任务全链（网关按 ts_utc/seq 全局有序）
 *   hall.tasks             账本任务清单
 *   hall.stream {limit}    全局事件尾部
 * 完整性核对由网关账本单点负责，客户端不再持有索引。
 * ================================================================ */

#define CLI_CHAIN_CONTENT_CAP 512
#define CLI_HALL_GW_TIMEOUT_MS 5000

/* 取事件对象中的字符串字段（父对象缺省 / 字段非字符串返回 0）。 */
static int cli_chain_field_str(const cJSON *obj, const char *key, char *out, size_t cap)
{
    const cJSON *v = obj ? cJSON_GetObjectItem(obj, key) : NULL;

    if (!cJSON_IsString(v) || !v->valuestring)
        return 0;
    AIRY_STRNCPY_TERM(out, v->valuestring, cap);
    return 1;
}

/* 取事件对象中的数值字段（缺失返回 0）。 */
static int cli_chain_field_num(const cJSON *obj, const char *key, double *out)
{
    const cJSON *v = obj ? cJSON_GetObjectItem(obj, key) : NULL;

    if (!cJSON_IsNumber(v))
        return 0;
    *out = v->valuedouble;
    return 1;
}

/* 类别 + content → 人类可读标签（content 为账本事件内容对象） */
static void cli_chain_label(const char *cat, const cJSON *content, char *out, size_t cap)
{
    char v[128] = "";

    if (!cat)
        cat = "";
    if (strcmp(cat, "chain") == 0) {
        if (cli_chain_field_str(content, "event", v, sizeof(v))) {
            if (strcmp(v, "gccp_confirm") == 0)
                snprintf(out, cap, "GCCP 意图确认");
            else if (strcmp(v, "blueprint_hit") == 0) {
                char layer[16] = "";
                cli_chain_field_str(content, "layer", layer, sizeof(layer));
                snprintf(out, cap, "蓝图命中 %s", layer[0] ? layer : "?");
            } else {
                snprintf(out, cap, "决策 %s", v);
            }
        } else {
            snprintf(out, cap, "决策链事件");
        }
    } else if (strcmp(cat, "blueprint") == 0) {
        char goal[160] = "";
        if (cli_chain_field_str(content, "goal", goal, sizeof(goal)))
            snprintf(out, cap, "蓝图 %s", goal);
        else
            snprintf(out, cap, "蓝图");
    } else if (strcmp(cat, "command") == 0) {
        char dag[64] = "", pid[64] = "";
        if (cli_chain_field_str(content, "dag_id", dag, sizeof(dag)))
            snprintf(out, cap, "提交 dag=%s", dag);
        else if (cli_chain_field_str(content, "plan_id", pid, sizeof(pid)))
            snprintf(out, cap, "计划 %s", pid);
        else
            snprintf(out, cap, "指令");
    } else if (strcmp(cat, "verify") == 0) {
        char verdict[32] = "", reason[160] = "";
        cli_chain_field_str(content, "verdict", verdict, sizeof(verdict));
        cli_chain_field_str(content, "reason", reason, sizeof(reason));
        if (verdict[0])
            snprintf(out, cap, "复核 %s%s%s", verdict, reason[0] ? " · " : "", reason);
        else
            snprintf(out, cap, "复核");
    } else if (strcmp(cat, "progress") == 0) {
        char st[32] = "", reason[160] = "";
        double pg = 0;
        cli_chain_field_str(content, "state", st, sizeof(st));
        cli_chain_field_str(content, "reason", reason, sizeof(reason));
        if (st[0] && cli_chain_field_num(content, "progress", &pg))
            snprintf(out, cap, "进度 %s %.0f%%%s%s", st, pg * 100.0,
                     reason[0] ? " · " : "", reason);
        else if (st[0])
            snprintf(out, cap, "进度 %s%s%s", st, reason[0] ? " · " : "", reason);
        else
            snprintf(out, cap, "进度");
    } else if (strcmp(cat, "result") == 0) {
        char st[32] = "";
        if (cli_chain_field_str(content, "state", st, sizeof(st)))
            snprintf(out, cap, "终态 %s", st);
        else
            snprintf(out, cap, "结果");
    } else if (strcmp(cat, "issue") == 0) {
        char reason[160] = "";
        double rc = 0;
        cli_chain_field_str(content, "reason", reason, sizeof(reason));
        if (cli_chain_field_num(content, "rc", &rc))
            snprintf(out, cap, "问题 rc=%.0f%s%s", rc, reason[0] ? " · " : "", reason);
        else
            snprintf(out, cap, "问题%s%s", reason[0] ? " " : "", reason);
    } else {
        snprintf(out, cap, "%s", cat[0] ? cat : "事件");
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
    cJSON *params = cJSON_CreateObject();

    if (!params) {
        cli_print_line("Super Agent:chain", "决策链查询失败：内存不足");
        return 0;
    }
    cJSON_AddStringToObject(params, "task_id", task_id);

    cJSON *res = NULL;
    int rc = cli_gw_json("hall.replay", params, CLI_HALL_GW_TIMEOUT_MS, &res);
    if (rc != 0 || !res) {
        const char *why = cli_gw_last_err();
        char line[224];
        snprintf(line, sizeof(line), "任务 %s 决策链查询失败：%s", task_id,
                 (why && why[0]) ? why : cli_err_desc(rc));
        cli_print_line("Super Agent:chain", line);
        return 0;
    }

    /* 网关已按 (ts_utc, seq) 排好序，客户端按原序渲染即可 */
    const cJSON *events = cJSON_GetObjectItem(res, "events");
    int nev = cJSON_IsArray(events) ? cJSON_GetArraySize(events) : 0;
    if (nev == 0) {
        char line[192];
        snprintf(line, sizeof(line), "任务 %s 无事件（账本未记录该任务）", task_id);
        if (g_cli_print_mode)
            cli_outf("chain task=%s events=0\n", task_id);
        else
            cli_print_status("Super Agent:chain", line);
        cJSON_Delete(res);
        return 0;
    }

    if (g_cli_print_mode) {
        cli_outf("chain task=%s events=%d\n", task_id, nev);
    } else {
        char head[256];
        snprintf(head, sizeof(head), "决策链 %s · %d 事件（账本 ts_utc/seq 序）", task_id, nev);
        cli_print_status("Super Agent:chain", head);
    }

    for (int i = 0; i < nev; i++) {
        const cJSON *ev = cJSON_GetArrayItem(events, i);
        const cJSON *file = cJSON_GetObjectItem(ev, "file");
        const cJSON *content = cJSON_GetObjectItem(ev, "content");
        char cat[32] = "";
        double gseq = 0;
        cli_chain_field_str(file, "category", cat, sizeof(cat));
        cli_chain_field_num(file, "gseq", &gseq);
        char *body = cJSON_IsObject(content) ? cJSON_PrintUnformatted(content) : NULL;
        const char *shown = cli_chain_trim(body ? body : "");

        if (g_cli_print_mode) {
            cli_outf("  %s %llu %s\n", cat[0] ? cat : "event", (unsigned long long)gseq, shown);
        } else {
            char label[256];
            cli_chain_label(cat, content, label, sizeof(label));
            cli_outf("  [%s:%03llu] %s\n", cat[0] ? cat : "event",
                     (unsigned long long)gseq, label);
            cli_outf("      %s\n", shown);
        }
        AIRY_FREE(body);
    }
    cJSON_Delete(res);
    return 0;
}

int cmd_chain(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && arg[0])
        return cli_chain_render_task(arg);

    /* 无参数：列出账本任务（网关按 latest_ts 最新在前） */
    cJSON *res = NULL;
    int rc = cli_gw_json("hall.tasks", NULL, CLI_HALL_GW_TIMEOUT_MS, &res);
    if (rc != 0 || !res) {
        const char *why = cli_gw_last_err();
        char line[224];
        snprintf(line, sizeof(line), "任务枚举失败：%s",
                 (why && why[0]) ? why : cli_err_desc(rc));
        cli_print_line("Super Agent:事件流", line);
        return 0;
    }
    const cJSON *tasks = cJSON_GetObjectItem(res, "tasks");
    int n = cJSON_IsArray(tasks) ? cJSON_GetArraySize(tasks) : 0;
    if (n == 0) {
        if (g_cli_print_mode)
            cli_outf("chain tasks=0\n");
        else
            cli_print_status("Super Agent:chain",
                             "事件流为空：执行过任务后可用 /chain 查看真实决策链");
        cJSON_Delete(res);
        return 0;
    }
    if (g_cli_print_mode) {
        cli_outf("chain tasks=%d\n", n);
    } else {
        char line[128];
        snprintf(line, sizeof(line), "事件流任务 · %d 个", n);
        cli_print_status("Super Agent:chain", line);
    }
    for (int i = 0; i < n; i++) {
        const cJSON *t = cJSON_GetArrayItem(tasks, i);
        char tid[128] = "";
        double cnt = 0;
        cli_chain_field_str(t, "task_id", tid, sizeof(tid));
        cli_chain_field_num(t, "event_count", &cnt);
        cli_outf("    %s  (%llu 事件)\n", tid, (unsigned long long)cnt);
    }
    if (!g_cli_print_mode)
        cli_outf("    查看单任务决策链：/chain <task_id>\n");
    cJSON_Delete(res);
    return 0;
}

int cmd_hall(const char *arg, void *ctx)
{
    (void)ctx;
    const char *sub = arg;
    while (sub && (*sub == ' ' || *sub == '\t'))
        sub++;

    /* /hall audit：网关账本任务/事件计数。客户端不再持有索引，完整性
     * 由账本单点负责（写入方 append-only，"已记录⟺可见"在 DEBUG 构建
     * 写后回读断言），此处不宣称客户端核对过磁盘。 */
    if (sub && strncmp(sub, "audit", 5) == 0) {
        cJSON *res = NULL;
        int rc = cli_gw_json("hall.tasks", NULL, CLI_HALL_GW_TIMEOUT_MS, &res);
        if (rc != 0 || !res) {
            const char *why = cli_gw_last_err();
            char line[224];
            snprintf(line, sizeof(line), "审计失败：%s",
                     (why && why[0]) ? why : cli_err_desc(rc));
            cli_print_line("Super Agent:事件流", line);
            return 0;
        }
        const cJSON *tasks = cJSON_GetObjectItem(res, "tasks");
        int n = cJSON_IsArray(tasks) ? cJSON_GetArraySize(tasks) : 0;
        unsigned long long total = 0;
        for (int i = 0; i < n; i++) {
            double c = 0;
            if (cli_chain_field_num(cJSON_GetArrayItem(tasks, i), "event_count", &c))
                total += (unsigned long long)c;
        }
        if (g_cli_print_mode) {
            cli_outf("hall audit tasks=%d events=%llu source=gateway\n", n, total);
        } else {
            char line[256];
            snprintf(line, sizeof(line),
                     "账本任务 %d 个 / 事件 %llu 条（源：网关账本；客户端不持有索引，"
                     "完整性由账本单点负责）",
                     n, total);
            cli_print_status("Super Agent:hall", line);
        }
        cJSON_Delete(res);
        return 0;
    }

    /* /hall replay [limit]：全局事件尾部（网关按 ts_utc/seq 排序），默认 8 条 */
    if (sub && strncmp(sub, "replay", 6) == 0) {
        long want = sub[6] ? strtol(sub + 6, NULL, 10) : 0;
        size_t limit = want > 0 ? (size_t)want : 8;
        cJSON *params = cJSON_CreateObject();
        if (!params) {
            cli_print_line("Super Agent:事件流", "回放失败：内存不足");
            return 0;
        }
        cJSON_AddNumberToObject(params, "limit", (double)limit);

        cJSON *res = NULL;
        int rc = cli_gw_json("hall.stream", params, CLI_HALL_GW_TIMEOUT_MS, &res);
        if (rc != 0 || !res) {
            const char *why = cli_gw_last_err();
            char line[224];
            snprintf(line, sizeof(line), "回放失败：%s",
                     (why && why[0]) ? why : cli_err_desc(rc));
            cli_print_line("Super Agent:事件流", line);
            return 0;
        }
        const cJSON *events = cJSON_GetObjectItem(res, "events");
        int shown = cJSON_IsArray(events) ? cJSON_GetArraySize(events) : 0;
        double total = 0;
        cli_chain_field_num(res, "total", &total);

        if (g_cli_print_mode) {
            cli_outf("hall replay total=%llu shown=%d\n", (unsigned long long)total, shown);
        } else {
            char line[192];
            snprintf(line, sizeof(line),
                     "事件流全局回放 · 共 %llu 条（账本 ts_utc/seq 序，显示 %d）",
                     (unsigned long long)total, shown);
            cli_print_status("Super Agent:hall", line);
        }
        for (int i = 0; i < shown; i++) {
            const cJSON *ev = cJSON_GetArrayItem(events, i);
            const cJSON *file = cJSON_GetObjectItem(ev, "file");
            const cJSON *content = cJSON_GetObjectItem(ev, "content");
            char cat[32] = "", tid[128] = "";
            cli_chain_field_str(file, "category", cat, sizeof(cat));
            cli_chain_field_str(file, "task_id", tid, sizeof(tid));
            char *body = cJSON_IsObject(content) ? cJSON_PrintUnformatted(content) : NULL;
            cli_outf("  [%s] %s %s\n", cat[0] ? cat : "event", tid,
                     cli_chain_trim(body ? body : ""));
            AIRY_FREE(body);
        }
        cJSON_Delete(res);
        return 0;
    }

    /* 无参数：任务统计 */
    cJSON *res = NULL;
    int rc = cli_gw_json("hall.tasks", NULL, CLI_HALL_GW_TIMEOUT_MS, &res);
    if (rc != 0 || !res) {
        const char *why = cli_gw_last_err();
        char line[224];
        snprintf(line, sizeof(line), "任务统计失败：%s",
                 (why && why[0]) ? why : cli_err_desc(rc));
        cli_print_line("Super Agent:事件流", line);
        return 0;
    }
    const cJSON *tasks = cJSON_GetObjectItem(res, "tasks");
    int n = cJSON_IsArray(tasks) ? cJSON_GetArraySize(tasks) : 0;
    if (g_cli_print_mode) {
        cli_outf("hall tasks=%d\n", n);
    } else {
        char line[192];
        snprintf(line, sizeof(line),
                 "事件流任务 %d 个 · /hall audit 账本计数 /hall replay [n] 全局回放", n);
        cli_print_status("Super Agent:hall", line);
    }
    cJSON_Delete(res);
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

/* 0.1.17 R5-G6：/tui 切换到全屏 TUI 渲染层。只置请求标志；主循环随后
 * fork agentrt-tui 子进程（唯一实现 cli_run_tui_frontend），TUI 退出后
 * 回到行式对话（exec 替换形态已退役）。 */
int cmd_tui(const char *arg, void *ctx)
{
    (void)arg;
    if (g_cli_print_mode) {
        cli_outf("  -p 模式无界面，/tui 不可用。\n");
        return 0;
    }
    cli_cmd_ctx_t *c = (cli_cmd_ctx_t *)ctx;
    if (c && c->switch_tui)
        *c->switch_tui = 1;
    return 0;
}
