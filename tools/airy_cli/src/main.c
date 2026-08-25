// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file main.c
 * @brief airy_cli - AgentRT interactive product entry.
 *
 * Full closed-loop demo (productized form): natural-language task
 * instruction -> GCCP intent confirmation (reasoning + four questions) ->
 * cognition pipeline planning (Phase 0-1) -> Plan -> TaskFlow DAG adaption
 * -> work-hall submit/board/wait -> agent_d drives real execution.
 *
 * Mechanism/strategy separation: the CLI is the product layer (interaction
 * strategy), agentrt is the mechanism layer. Degrades gracefully when the
 * llm_d/agent_d daemons are not running (heuristic confirmation, agent
 * unavailable).
 */

#include "airy_rt.h"
#include "loop.h"
#include "roadmap_sched.h"
#include "lang_gateway.h"
#include "platform.h"
#include "cognition.h"
#include "gccp.h"
#include "work_hall.h"
#include "hall_store.h"
#include "governance.h"
#include "plan_to_dag.h"
#include "taskflow_advanced.h"
#include "llm_svc_adapter.h"
#include "logger.h"
#include "logging.h"
#include "airy_memory.h"
#include "string_compat.h"
#include "daemon_rpc_client.h"
#include "daemon_cmds.h"
#include "cli_internal.h"
/* 阶段 4（item 4）: 认知阶段并行子 agent 审查（事实/风险） */
#include "cli_review.h"
/* 阶段 4（改进6，P3）: 执行复核管线接线（门禁 -> t2 语义复核 -> t1-f 终裁） */
#include "cli_exec_review.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Runtime color gate: shared cli_c() from cli_render (monochrome under
 * NO_COLOR / piped output). */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#endif

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* Task-set cancellation flag: set by the SIGINT handler; run_to_completion checks
  * it each round (the engine holds this pointer); the current node finishes, then aborts.
  * Reset to 0 before each new task. */
volatile sig_atomic_t g_cli_cancel = 0;

/* Server one-shot mode (-p/--print) and --json structured output. */
int g_cli_print_mode = 0;
int g_cli_json_mode = 0;

#if !defined(_WIN32)
static void cli_sigint_handler(int sig)
{
    (void)sig;
    g_cli_cancel = 1;
}
#endif

/* ---- 2.3.7 任务集中途打断/插入对话 ----
 * 任务执行（4.5 wait 段）原为阻塞式，用户在任务完成前无法输入。
 * 将 wait 放入后台线程推进引擎，主线程轮询 stdin：
 *   - 中断指令（quit/exit/abort/stop/cancel/打断/停止）→ g_cli_cancel=1
 *   - 其他输入 → 作为插入对话处理（cli_chat_reply），任务继续后台执行
 * 线程完成（done=1）或取消时退出轮询，回到结果汇总。 */
typedef struct {
    airy_work_hall_t *hall;       /* 本地 wait 用（BORROW） */
    const char *sched_sock;       /* 远程 wait 用（BORROW） */
    const char *exec_id;          /* BORROW */
    int sched_remote;
    airy_err_t err;
    char *result;                 /* OWNER，线程完成后调用方释放 */
    volatile int done;
} cli_task_wait_ctx_t;

static void *cli_task_wait_worker(void *arg)
{
    cli_task_wait_ctx_t *c = (cli_task_wait_ctx_t *)arg;
    if (!c)
        return NULL;
    if (c->sched_remote) {
        c->err = cli_dag_wait_remote(c->sched_sock, c->exec_id, &c->result);
    } else {
        c->err = airy_work_hall_wait(c->hall, c->exec_id, 0, &c->result);
        /* 改进6（P3）: 本轮执行复核（wait 内门禁/t2/t1-f）已结束，
         * 解绑蓝图——BORROW 指针随本轮 plan 释放失效，避免跨轮悬垂。 */
        airy_work_hall_set_blueprint(c->hall, NULL);
    }
    c->done = 1;
    return NULL;
}

/* 2.3.14 GRAD 决策链可见性：cognition feedback → [Dual Think] 阶段行。
 * 事件（grad_coordinator progress_cb）：grad_s2_done / grad_verify_start /
 * grad_verify_done / grad_arbiter_start / grad_arbiter_done / grad_done。
 * -p/--json 不渲染（脚本结构化输出）；TUI 面板模式渲染交给 TUI 自身，
 * 但事件始终写入 hall_store 决策链事件流（/chain 与事件流面板可见）。 */
static void cli_grad_feedback_cb(int level, const char *module, const char *event,
                                 const char *data, size_t data_len, void *user_data)
{
    (void)level;
    (void)module;
    (void)user_data;
    if (!event || strncmp(event, "grad_", 5) != 0)
        return;
    cli_tui_t *tui = cli_tui_get_default();
    int tui_active = tui && cli_tui_active(tui);
    int render = !g_cli_print_mode && !g_cli_json_mode && !tui_active;

    if (render) {
        cli_actor_t actor = CLI_ACTOR_DUAL_THINK;
        const char *tag = "GRAD";
        if (strcmp(event, "grad_s2_done") == 0) {
            actor = CLI_ACTOR_DUAL_SLOW_THINK;
            tag = "S2 骨架";
        } else if (strcmp(event, "grad_verify_start") == 0) {
            actor = CLI_ACTOR_DUAL_PROF_THINK;
            tag = "四向验证";
        } else if (strcmp(event, "grad_verify_done") == 0) {
            actor = CLI_ACTOR_DUAL_PROF_THINK;
            tag = "验证结果";
        } else if (strcmp(event, "grad_arbiter_start") == 0) {
            actor = CLI_ACTOR_DUAL_FAST_THINK;
            tag = "上下文仲裁";
        } else if (strcmp(event, "grad_arbiter_done") == 0) {
            actor = CLI_ACTOR_DUAL_FAST_THINK;
            tag = "仲裁结论";
        } else if (strcmp(event, "grad_done") == 0) {
            actor = CLI_ACTOR_DUAL_THINK;
            tag = "GRAD 收敛";
        } else {
            return; /* 非 GRAD 决策链事件，不渲染 */
        }

        char line[512];
        if (data && data_len > 0) {
            int n = snprintf(line, sizeof(line), "%s", data);
            if (n < 0 || (size_t)n >= sizeof(line))
                line[sizeof(line) - 1] = '\0';
        } else {
            line[0] = '\0';
        }
        cli_spinner_pause();
        cli_render_role_line(CLI_ROLE_DUAL_THINK, actor, tag, line);
        cli_spinner_resume();
    }

    /* 阶段 4：GRAD 决策链 → 事件流单一真相源（/chain 与决策链面板可见）。
     * 事件体复用 progress 的 JSON data（追加 event 字段）。 */
    if (g_cli_hall_store) {
        char ev[768];
        if (data && data_len > 0)
            snprintf(ev, sizeof(ev), "{\"event\":\"%s\",\"data\":%.*s}", event, (int)data_len,
                     data);
        else
            snprintf(ev, sizeof(ev), "{\"event\":\"%s\"}", event);
        airy_hall_store_write(g_cli_hall_store, "default", "grad", NULL, AIRY_HALL_CAT_CHAIN,
                              "cognition", ev, NULL, 0);
    }
}

/* 任务执行期间轮询 stdin（交互行模式，非 -p/--json/TUI）：
 *   - 无输入返回 0
 *   - 中断指令 → g_cli_cancel=1，返回 -1（打断任务）
 *   - 其他文本 → 渲染用户消息 + 插入对话回复，返回 1（任务继续）
 * 仅在 TTY 行模式启用；-p 管道流、--json、TUI 面板模式保持原语义。 */
static int cli_task_poll_input(void)
{
    if (g_cli_print_mode || g_cli_json_mode)
        return 0;
    cli_tui_t *tui = cli_tui_get_default();
    if (tui && cli_tui_active(tui))
        return 0;
#ifndef _WIN32
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int r = poll(&pfd, 1, 0);
    if (r <= 0 || !(pfd.revents & POLLIN))
        return 0;
    char line[4096];
    if (!fgets(line, sizeof(line), stdin))
        return 0;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
    if (n == 0)
        return 0;
    /* 纯空白输入（空格/Tab 串）：跳过，不进对话（与主输入一致） */
    {
        size_t nz = 0;
        while (nz < n && (line[nz] == ' ' || line[nz] == '\t'))
            nz++;
        if (nz == n)
            return 0;
    }
    /* 中断指令：不进入对话，直接打断任务（与 SIGINT 同语义） */
    if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 ||
        strcmp(line, "abort") == 0 || strcmp(line, "stop") == 0 ||
        strcmp(line, "cancel") == 0 || strcmp(line, "打断") == 0 ||
        strcmp(line, "停止") == 0 || strcmp(line, "取消") == 0) {
        g_cli_cancel = 1;
        return -1;
    }
    /* 插入对话：渲染用户消息 + 对话回复；任务在后台继续执行 */
    cli_spinner_pause();
    cli_render_user_message(line);
    cli_chat_reply(line);
    cli_spinner_resume();
    return 1;
#else
    (void)0;
    return 0;
#endif
}

llm_svc_adapter_t *g_chat_adapter = NULL;
/* 阶段 4（2026-08-15）：决策链事件流句柄（hall_store 创建后赋值）。 */
airy_hall_store_t *g_cli_hall_store = NULL;

/* 1.3 推理语言网关：全局句柄（cli_setup_runtime 创建后赋值）+ 最新一轮
 * 语言约束注入物（输入环节 process 填充，cli_chat.c 消费；每轮覆盖前释放）。 */
airy_lang_gateway_t *g_cli_lang_gateway = NULL;
char *g_cli_lang_sys_prompt = NULL;
airy_lang_t g_cli_lang_output = AIRY_LANG_UNKNOWN;

/* 会话开始时刻（TUI 状态栏耗时计算；交互模式才有意义）。 */
static uint64_t g_session_start_ms;

const cli_command_t CLI_COMMANDS[] = {
    {"/help", "显示所有命令", CLI_CAT_SESSION, 0, cmd_help},
    {"/clear", "清屏并清空对话上下文", CLI_CAT_SESSION, 0, cmd_clear},
    {"/status", "查看执行大厅状态", CLI_CAT_SYSTEM, 0, cmd_status},
    {"/chain", "决策链可视化：/chain [task_id]（默认列出最近任务）", CLI_CAT_SYSTEM, 0, cmd_chain},
    {"/orch", "流程编排：/orch <task>（七阶段管线：分解→规划→生成→批判→验证→审计→对齐）", CLI_CAT_SYSTEM, 1, cmd_orch},
    {"/quit", "退出 agentrt", CLI_CAT_SESSION, 0, cmd_quit},
    {"/tui", "切换到图形 TUI（agentrt-tui）", CLI_CAT_SESSION, 0, cmd_tui},
    {"/daemons", "查看全部 daemon 在线状态", CLI_CAT_SYSTEM, 0, cmd_daemons},
    {"/daemon", "管理 daemon：/daemon start|stop|restart|status [ns...]（默认全部）", CLI_CAT_SYSTEM, 0, cmd_daemon},
    {"/rpc", "直接调用 daemon 方法：/rpc <ns>.<method> [json]（ns 或 ns_d 均可）", CLI_CAT_SYSTEM, 1, cmd_rpc},
    {"/stats", "查看 daemon 统计：/stats [ns]", CLI_CAT_SYSTEM, 0, cmd_stats},
    {"/agents", "列出已注册智能体", CLI_CAT_RESOURCE, 0, cmd_agents},
    {"/tools", "列出可用工具", CLI_CAT_RESOURCE, 0, cmd_tools},
    {"/hooks", "列出事件钩子", CLI_CAT_RESOURCE, 0, cmd_hooks},
    {"/plugins", "列出插件", CLI_CAT_RESOURCE, 0, cmd_plugins},
    {"/channels", "列出消息通道", CLI_CAT_RESOURCE, 0, cmd_channels},
    {"/market", "搜索市场（/market skill 搜技能）", CLI_CAT_RESOURCE, 0, cmd_market},
    {"/models", "列出 LLM 模型", CLI_CAT_RESOURCE, 0, cmd_models},
    {"/mem", "记忆检索：/mem [query]", CLI_CAT_RESOURCE, 1, cmd_mem},
    {"/a2a", "发现 A2A 智能体", CLI_CAT_RESOURCE, 0, cmd_a2a},
    {"/metrics", "查询观测指标", CLI_CAT_SYSTEM, 0, cmd_metrics},
    {"/alerts", "查看监控告警", CLI_CAT_SYSTEM, 0, cmd_alerts},
    {"/tasks", "调度状态与检查点", CLI_CAT_SYSTEM, 0, cmd_tasks},
    {"/info", "系统信息", CLI_CAT_SYSTEM, 0, cmd_info},
    {"/notify", "发布通知：/notify <channel> <msg>", CLI_CAT_SECURITY, 1, cmd_notify},
    {"/vault", "凭据保险库：/vault list", CLI_CAT_SECURITY, 1, cmd_vault},
    {"/perm", "权限裁决：/perm <agent> <action> <resource>", CLI_CAT_SECURITY, 1, cmd_perm},
    {"/sanitize", "输入净化：/sanitize <input>", CLI_CAT_SESSION, 1, cmd_sanitize},
    {"/security", "安全状态（网络规则统计）", CLI_CAT_SECURITY, 0, cmd_security},
};

#define CLI_COMMANDS_COUNT (sizeof(CLI_COMMANDS) / sizeof(CLI_COMMANDS[0]))

size_t cli_commands_count(void)
{
    return CLI_COMMANDS_COUNT;
}

/**
  * @brief Slash-command dispatch: match /name; return 0 to fall through on miss
 */
static int cli_dispatch_command(const char *input, void *ctx)
{
    if (input[0] != '/')
        return 0;

    for (size_t i = 0; i < CLI_COMMANDS_COUNT; i++) {
        const cli_command_t *cmd = &CLI_COMMANDS[i];
        size_t nlen = strlen(cmd->name);
        if (strncmp(input, cmd->name, nlen) == 0) {

            const char *rest = input + nlen;
            if (*rest == '\0' || *rest == ' ') {
                const char *arg = (*rest == ' ') ? rest + 1 : NULL;
                if (cmd->needs_args && (!arg || arg[0] == '\0')) {
                    cli_outf("  %s%s%s 需要参数。%s\n", cli_c(CLR_YELLOW), cmd->name,
                           cli_c(CLR_RESET), cmd->desc);
                    return 1;
                }
                cmd->fn(arg, ctx);
                return 1;
            }

            if (nlen >= 2 && strncmp(input, cmd->name, strlen(input) - 1) == 0 &&
                cmd->name[strlen(input) - 1] != '\0' && !cmd->needs_args) {
                cmd->fn(NULL, ctx);
                return 1;
            }
        }
    }

    cli_outf("  %s未知命令%s %s，输入 %s/help%s 查看可用命令。\n", cli_c(CLR_YELLOW),
           cli_c(CLR_RESET), input, cli_c(CLR_CYAN), cli_c(CLR_RESET));
    return 1;
}

/* 阶段 4（2026-08-15）：任务提交 → 决策链 COMMAND 事件（exec_id 任务，
 * "cognition" 角色，ACL 允许；plan_id 关联 preflight 计划事件）。 */
static void cli_chain_record_submit(const char *exec_id, const airy_task_plan_t *plan,
                                    const taskflow_workflow_t *wf)
{
    if (!g_cli_hall_store || !exec_id || !exec_id[0])
        return;
    char ev[384];
    snprintf(ev, sizeof(ev),
             "{\"dag_id\":\"%s\",\"plan_id\":\"%s\",\"nodes\":%zu,\"edges\":%zu}",
             exec_id, (plan && plan->task_plan_id) ? plan->task_plan_id : "",
             wf ? wf->node_count : 0, wf ? wf->edge_count : 0);
    airy_hall_store_write(g_cli_hall_store, "default", exec_id, NULL, AIRY_HALL_CAT_COMMAND,
                          "cognition", ev, NULL, 0);
}

static void cli_print_usage(void)
{
    cli_outf("用法: airy_cli [选项]\n");
    cli_outf("  %s-p%s, %s--print%s [PROMPT]  服务器单轮模式：执行一条指令后退出\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("                           （无 banner/提示符；省略 PROMPT 时从 stdin 读取）\n");
    cli_outf("  %s--json%s                 结构化 JSON 输出（与 %s-p%s 组合使用）\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("  %s-h%s, %s--help%s             显示本帮助\n",
           cli_c(CLR_CYAN), cli_c(CLR_RESET), cli_c(CLR_CYAN), cli_c(CLR_RESET));
    cli_outf("交互模式：直接运行 airy_cli 进入对话；输入 /help 查看命令。\n");
}

/* 命令行解析（2026-08-21 自 main 抽离降圈复杂度）：支持选项与
 * -p 模式 prompt 任意顺序（`-p --json "prompt"`、`--json -p "prompt"`、
 * `-p "prompt" --json` 均合法），首个非选项 token 作为 prompt（含空格的
 * prompt 须用引号包裹）。返回 0=继续启动；1=已输出帮助或错误并退出。 */
static int cli_parse_args(int argc, char *argv[], const char **out_print_prompt)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--print") == 0) {
            g_cli_print_mode = 1;
        } else if (strcmp(argv[i], "--json") == 0) {
            g_cli_json_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            cli_print_usage();
            return 1;
        } else if (argv[i][0] == '-') {
            cli_outf("airy_cli: 未知选项 %s%s%s\n", cli_c(CLR_YELLOW), argv[i],
                   cli_c(CLR_RESET));
            cli_print_usage();
            return 1;
        } else if (g_cli_print_mode && !*out_print_prompt) {
            /* -p 模式下首个非选项 token 作为 prompt。 */
            *out_print_prompt = argv[i];
        } else if (g_cli_print_mode) {
            cli_outf("airy_cli: 多出的参数 %s%s%s（-p 只接受一个 prompt）\n",
                   cli_c(CLR_YELLOW), argv[i], cli_c(CLR_RESET));
            cli_print_usage();
            return 1;
        } else {
            cli_outf("airy_cli: 未知参数 %s%s%s（非选项参数仅可用于 %s-p%s 模式）\n",
                   cli_c(CLR_YELLOW), argv[i], cli_c(CLR_RESET),
                   cli_c(CLR_CYAN), cli_c(CLR_RESET));
            cli_print_usage();
            return 1;
        }
    }
    return 0;
}

/* 任务回合结果汇总（2026-08-22 自 main 任务段抽离降圈复杂度）：
 * 解析执行结果 JSON 判定真实成败（status/success/error 字段，防失败
 * 结果被误判成功写缓存），按 g_cli_json_mode / 行渲染分派输出，并在
 * 产物校验统计上升时给出 PROF 校验失败提示。仅读执行状态，不触碰
 * 管线。 */
static int cli_task_result_render(const char *result, airy_err_t err, const char *exec_id,
                                  int canceled, airy_work_hall_t *hall, uint32_t vf_before)
{
    /* 任务实际成败：远程 DAG 终态可能是 failed/canceled，此时
     * cli_dag_wait_remote 仍返回 EOK + 结果 JSON，必须解析 status
     * 而非仅看 err，否则失败结果会被误 absorb 为 SUCCESS 写入 L2
     * 语义缓存（缓存中毒、错误记忆累积）。嵌入式路径的 result 也是
     * JSON（含 success 字段），同样必须解析——agent 执行失败（如
     * tool loop / 工具被拒）返回 {"success":false,...} 时 err 仍为
     * EOK，若只看 err 会把失败当成功缓存（P0-1 L2 中毒根因）。 */
    int task_succeeded = (err == AIRY_EOK && result) ? 1 : 0;
    if (task_succeeded && result) {
#ifdef AIRY_HAS_CJSON
        cJSON *rstat = cJSON_Parse(result);
        if (rstat) {
            cJSON *st = cJSON_GetObjectItem(rstat, "status");
            if (cJSON_IsString(st) && st->valuestring) {
                if (strcmp(st->valuestring, "completed") != 0)
                    task_succeeded = 0;
            } else {
                /* 嵌入式路径：result 可能是 agent 输出 JSON，含
                 * success 字段；无 status 时按 success 判定 */
                cJSON *ok = cJSON_GetObjectItem(rstat, "success");
                if (cJSON_IsBool(ok) && !cJSON_IsTrue(ok))
                    task_succeeded = 0;
                else if (cJSON_IsString(ok) && strcmp(ok->valuestring, "true") != 0)
                    task_succeeded = 0;
                cJSON *errj = cJSON_GetObjectItem(rstat, "error");
                if (errj && cJSON_IsString(errj) && errj->valuestring &&
                    errj->valuestring[0])
                    task_succeeded = 0;
            }
            cJSON_Delete(rstat);
        }
#else
        if (strstr(result, "\"failed\"") || strstr(result, "\"canceled\"") ||
            strstr(result, "\"success\":false") || strstr(result, "\"error\":"))
            task_succeeded = 0;
#endif /* AIRY_HAS_CJSON */
    }
    if (canceled) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "cancel",
                                  "Task aborted (stopped after the current node).");
        cli_trace("result", "%s canceled", CLI_ICON_CROSS);
    } else if (err == AIRY_EOK && result) {
        cli_render_phase("结果汇总");
        if (g_cli_json_mode) {
#ifdef AIRY_HAS_CJSON
            cJSON *jroot = cJSON_CreateObject();
            cJSON_AddStringToObject(jroot, "role", "super_agent");
            cJSON_AddStringToObject(jroot, "type", "task");
            cJSON_AddBoolToObject(jroot, "success", task_succeeded);
            cJSON_AddStringToObject(jroot, "dag_id", exec_id ? exec_id : "");
            cJSON_AddStringToObject(jroot, "result", result);
            char *js = cJSON_PrintUnformatted(jroot);
            if (js) {
                cli_outf("%s\n", js);
                cJSON_free(js);
            }
            cJSON_Delete(jroot);
#else
            cli_outf("{\"role\":\"super_agent\",\"type\":\"task\",\"success\":%s,"
                     "\"dag_id\":\"%s\",\"result\":\"%s\"}\n",
                     task_succeeded ? "true" : "false",
                     exec_id ? exec_id : "", result);
#endif /* AIRY_HAS_CJSON */
        } else {
            cli_print_result(result);
        }
        cli_trace("result", "%s success=%d", task_succeeded ? CLI_ICON_CHECK : CLI_ICON_CROSS,
                  task_succeeded ? 1 : 0);
    } else {
        if (g_cli_json_mode) {
#ifdef AIRY_HAS_CJSON
            cJSON *jroot = cJSON_CreateObject();
            cJSON_AddStringToObject(jroot, "role", "super_agent");
            cJSON_AddStringToObject(jroot, "type", "task");
            cJSON_AddBoolToObject(jroot, "success", 0);
            cJSON_AddStringToObject(jroot, "dag_id", exec_id ? exec_id : "");
            cJSON_AddNumberToObject(jroot, "code", (double)(int)err);
            char *js = cJSON_PrintUnformatted(jroot);
            if (js) {
                cli_outf("%s\n", js);
                cJSON_free(js);
            }
            cJSON_Delete(jroot);
#else
            cli_outf("{\"role\":\"super_agent\",\"type\":\"task\",\"success\":false,"
                     "\"dag_id\":\"%s\",\"code\":%d}\n",
                     exec_id ? exec_id : "", (int)err);
#endif /* AIRY_HAS_CJSON */
        } else {
            char line[128];
            snprintf(line, sizeof(line), "任务执行无结果：%s", cli_err_desc((int)err));
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "执行结果", line);
        }
    }
    /* Decision G: validation gate annotation - mark FAIL clearly when artifacts
      * fail validation, so the user can replan/retry (sched_d owns node-level retries).
      * t1-p (PROF) is the logic verifier in the GRAD separation of powers, so an
      * artifact-validation failure carries the PROF label, not the fast-think actor. */
    if (!canceled) {
        uint32_t vf_after = 0;
        airy_work_hall_verify_stats(hall, NULL, &vf_after, NULL);
        if (vf_after > vf_before)
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_DUAL_PROF_THINK, "校验",
                                 "Artifact validation failed - replan or retry the task.");
    }
    /* 返回真实成败，供调用方决定 L2 语义缓存 absorb 的成败指纹 */
    return task_succeeded;
}

/* 运行时上下文（2026-08-22 拆分自 main 初始化/清理段，SSoT 收敛）：
 * 由 cli_setup_runtime 装配、cli_teardown_runtime 按序释放，main 不再
 * 直接持有这些组件——初始化与清理的对称性收进同一处。 */
typedef struct {
    airy_roadmap_sched_t *rsched;
    airy_work_hall_t *hall;
    airy_hall_store_t *hall_store;
    void *board_ud;
    void *events_ud;
    airy_artifact_validator_t *validator;
    airy_governance_t *governance;
    void *reviewer; /* cli_exec_review 句柄（不透明，仅透传销毁） */
    const char *main_workspace_dir; /* 主工作区（AIRY_WORKSPACE_MAIN_DIR 或 cwd） */
    /* 1.3 推理语言网关：自然语言 → 标准推理指令（Canonical Data Format）。
     * 输入环节标准化 + 语言约束注入 + 输出后处理；生命周期与 rsched 同级。 */
    airy_lang_gateway_t *lang_gateway;
} cli_runtime_ctx_t;

/* 核心引擎装配（2026-08-22 从 main 初始化段拆分，收敛圈复杂度）：
 * loop 创建 → 记忆引擎注入（2.2.4 对话路径读写，cli_chat_reply 使用）→
 * cognition 引擎接线（GCCP 交互回调、TC3 三模型注入、GRAD 决策链反馈）。
 * out_cog 可空（主循环内 airy_cognition_process 使用）；任一失败返回
 * NULL（调用方统一 return 1），不静默降级。 */
static airy_core_loop_t *cli_setup_core_engines(const char *m_s2, const char *m_verify,
                                                const char *m_expert,
                                                airy_cognition_engine_t **out_cog)
{
    airy_core_loop_t *loop = NULL;
    airy_err_t err = airy_loop_create(NULL, &loop);
    if (err != AIRY_EOK || !loop) {
        AIRY_LOG_ERROR("airy_cli: loop create failed (err=%d)", (int)err);
        return NULL;
    }

    /* 2.2.4 对话记忆：把记忆引擎注入 CLI 对话路径（cli_chat_reply 读写），
     * 此前对话零记忆接线——"记不住/想不准"的持久化接入点。 */
    airy_memory_engine_t *mem = NULL;
    airy_loop_get_engines(loop, NULL, &mem);
    if (mem) {
        g_cli_memory_engine = mem;
        AIRY_LOG_INFO("airy_cli: chat memory engine attached");
    }

    airy_cognition_engine_t *cog = NULL;
    airy_loop_get_engines(loop, &cog, NULL);
    if (cog) {
        airy_cognition_set_gccp_interact(cog, cli_gccp_interact, NULL);
        AIRY_LOG_INFO("airy_cli: GCCP interaction callback attached");

        /* Dual-thinking three-model injection (GRAD separation of powers; user-chosen models):
          *   AIRY_MODEL_T2    -> model A (generator)
          *   AIRY_MODEL_T1F   -> model B (context arbiter)
          *   AIRY_MODEL_T1P   -> model C (logic verifier)
          * Unset values use the provider default model (backward compatible).
          * Values were read above for the combined system header. */
        airy_cognition_set_tc3_models(cog, m_s2 && m_s2[0] ? m_s2 : NULL,
                                      m_verify && m_verify[0] ? m_verify : NULL,
                                      m_expert && m_expert[0] ? m_expert : NULL);
        if ((m_s2 && m_s2[0]) || (m_verify && m_verify[0]) || (m_expert && m_expert[0])) {
            AIRY_LOG_INFO("airy_cli: TC3 models injected (s2=%s verify=%s expert=%s)",
                          m_s2 && m_s2[0] ? m_s2 : "(default)",
                          m_verify && m_verify[0] ? m_verify : "(default)",
                          m_expert && m_expert[0] ? m_expert : "(default)");
        }

        /* Decision B (2026-08-09): three model config points - t2=A (generator, cloud-first),
          * t1-f=B (arbiter/daily chat, first to activate, local-first), t1-p=C (verifier);
          * each may use cloud APIs or local endpoints (Ollama/vLLM); the user decides.
          * The model panel was already rendered inside the blue-framed
          * system header above (cli_print_system_header); only inject. */

        airy_cognition_set_grad_enabled(cog, 1);
        /* 2.3.14 GRAD 决策链可见性：注册 feedback 回调，任务规划期
         * GRAD 阶段进度实时渲染为 [Dual Slow/Prof/Fast Think] 行。 */
        airy_cognition_set_feedback(cog, cli_grad_feedback_cb, NULL);
        AIRY_LOG_INFO("airy_cli: GRAD decision-chain feedback attached");
    }
    if (out_cog)
        *out_cog = cog;

    return loop;
}

/* 运行时上下文装配（2026-08-22 从 main 初始化段拆分，收敛圈复杂度）：
 * rsched（蓝图调度 L2 反馈）→ 产物校验器 → 执行复核管线 → hall_store
 * 事件流 → 统一治理 → work_hall 创建 → chat adapter → TUI 面板绑定。
 * 失败（work_hall 无法创建）返回 AIRY_EOK 以外的错误码，调用方统一
 * 清理并退出；其余子组件失败仅降级不阻断。 */
static airy_err_t cli_setup_runtime(airy_core_loop_t *loop, cli_tui_t *tui,
                                    cli_runtime_ctx_t *rt)
{
    if (!loop || !rt)
        return AIRY_EINVAL;
    AIRY_MEMSET(rt, 0, sizeof(*rt));

    airy_err_t err = AIRY_EOK;

    /* 3. Blueprint scheduling (Roadmap Sched): result feedback hook (synergy point 1).
      * Node final states feed back via progress_cb into the L2 cache / failure fingerprints / L1.
      * P1e: L2 dual-write persistence ($AIRY_HOME/data/agentrt/roadmap/l2_semantic_cache.json,
      * restored on restart); Embedding + HNSW vector index (MemoryRovol, degrades when unlinked).
      * Creation failure only degrades (no feedback); the CLI main flow continues. */
    airy_rs_config_t rs_cfg;
    __builtin_memset(&rs_cfg, 0, sizeof(rs_cfg));
    {
        static char rs_persist_path[512];
        snprintf(rs_persist_path, sizeof(rs_persist_path),
                 "%s/agentrt/roadmap/l2_semantic_cache.json", airy_data_dir());
        rs_cfg.l2_persist_path = rs_persist_path;
    }
    airy_roadmap_sched_t *rsched = NULL;
    err = airy_roadmap_sched_create(&rs_cfg, &rsched);
    if (err != AIRY_EOK || !rsched) {
        AIRY_LOG_WARN("airy_cli: roadmap_sched create failed (err=%d), "
                      "execution feed-back disabled",
                      (int)err);
        rsched = NULL;
    }
    rt->rsched = rsched;

    /* Decision G (2026-08-09): validation gate - inject an artifact validator.
      * Rules come from AIRY_VALIDATOR_RULES (JSON); default exit_code=0;
      * node-level gates live in sched_d write_back; the CLI marks FAIL after wait. */
    airy_artifact_validator_t *cli_validator = NULL;
    {
        const char *rules_json = getenv("AIRY_VALIDATOR_RULES");
        if (!rules_json || !rules_json[0])
            rules_json = "{\"exit_code\":0}";
        airy_err_t vrc = airy_artifact_validator_from_json(&cli_validator, rules_json);
        if (vrc != AIRY_SUCCESS) {
            AIRY_LOG_WARN("airy_cli: output_validator create failed (err=%d), gate disabled",
                          (int)vrc);
            cli_validator = NULL;
        }
    }
    rt->validator = cli_validator;
    airy_work_hall_config_t wh_cfg;
    __builtin_memset(&wh_cfg, 0, sizeof(wh_cfg));
    wh_cfg.progress_cb = cli_progress_cb;
    wh_cfg.roadmap_sched = rsched;
    wh_cfg.output_validator = cli_validator;
    /* 改进6（P3）: 执行复核管线——wait 返回前对聚合产物跑门禁 -> t2 语义
     * 复核 -> t1-f 终裁，verdict 写 hall_store verify 事件并回灌 L2（PASS
     * 可缓存 / DRIFT 不缓存 / REJECT 失效）。t2/t1-f 委托走 llm_d；
     * llm_d 不可用时降级为纯确定性门禁。 */
    wh_cfg.reviewer = cli_exec_review_create();
    rt->reviewer = wh_cfg.reviewer;
    if (wh_cfg.reviewer)
        AIRY_LOG_INFO("airy_cli: execution review pipeline attached (gate -> t2 -> t1-f)");
    /* Decision C (2026-08-09): task file model - full-visibility storage ($AIRY_HOME/data/agentrt/hall).
      * Progress/result/issue events go to the hall store for replay and experience mining. */
    airy_hall_store_t *hall_store = airy_hall_store_create(NULL);
    if (!hall_store)
        AIRY_LOG_WARN("airy_cli: hall store create failed, full visibility disabled");
    g_cli_hall_store = hall_store;
    rt->hall_store = hall_store;
    wh_cfg.hall_store = hall_store;
    /* P23 reconcile extension: execution-level failure auto re-dispatch
     * (opt-in via AIRY_WORK_HALL_REDISPATCH_MAX, 0 = disabled).
     * The main loop below calls airy_work_hall_redispatch_once() each
     * iteration to drive the reconcile poll. */
    {
        const char *e_rd = getenv("AIRY_WORK_HALL_REDISPATCH_MAX");
        const char *e_rd_delay = getenv("AIRY_WORK_HALL_REDISPATCH_DELAY_MS");
        if (e_rd && e_rd[0] && strtol(e_rd, NULL, 10) > 0) {
            wh_cfg.redispatch_max = (int32_t)strtol(e_rd, NULL, 10);
            if (e_rd_delay && e_rd_delay[0])
                wh_cfg.redispatch_delay_ms = (uint32_t)strtoul(e_rd_delay, NULL, 10);
            AIRY_LOG_INFO("airy_cli: execution reconcile attached "
                          "(redispatch_max=%d, delay_ms=%u)",
                          wh_cfg.redispatch_max, wh_cfg.redispatch_delay_ms);
        }
    }
    /* Decision E (2026-08-09): workspace isolation of the main workspace path.
      * Enabled when AIRY_WORKSPACE_MAIN_DIR is set (snapshot the main workspace -> a sandbox
      * -> merge artifacts back); otherwise unchanged (the executor touches the main workspace).
      * AIRY_WORKSPACE_ISOLATION=0 disables isolation further (snapshot/merge return ENOTSUP).
      * When unset, default the main workspace to the CLI's cwd so the agent acts
      * on the real project tree (snapshot-isolated) instead of an empty dir —
      * otherwise the agent exhausts its tool rounds exploring a bare workspace
      * (P0-1 end-to-end: "create a test file" -> tool loop exceeded). */
    {
        const char *ws_main = getenv("AIRY_WORKSPACE_MAIN_DIR");
        static char ws_main_buf[1024];
        if (ws_main && ws_main[0]) {
            wh_cfg.main_workspace_dir = ws_main;
        } else {
#if AIRY_PLATFORM_POSIX
            if (getcwd(ws_main_buf, sizeof(ws_main_buf)))
                wh_cfg.main_workspace_dir = ws_main_buf;
#else
            if (_getcwd(ws_main_buf, (int)sizeof(ws_main_buf)))
                wh_cfg.main_workspace_dir = ws_main_buf;
#endif
        }
        if (wh_cfg.main_workspace_dir)
            AIRY_LOG_INFO("airy_cli: main workspace = %s", wh_cfg.main_workspace_dir);
        rt->main_workspace_dir = wh_cfg.main_workspace_dir;
    }
    /* Decision F (2026-08-09): unified governance via environment variables (GRAD axiom II
      * R_total runtime projection). AIRY_GOV_TOKEN_BUDGET (global budget, 0=unlimited) /
      * AIRY_GOV_SLOTS (concurrent slots, 0=default 8) / AIRY_GOV_MAX_CONCURRENT (hard
      * cap, 0=no hard cap) / AIRY_GOV_TOKEN_PER_NODE (per-node estimate, task-level
      * gate) / AIRY_GOV_DEADLINE_MS (graph default deadline, 0=unlimited).
      * If none are set (no budget and no slots), governance is off. */
    airy_governance_t *governance = NULL;
    {
        const char *e_budget = getenv("AIRY_GOV_TOKEN_BUDGET");
        const char *e_slots = getenv("AIRY_GOV_SLOTS");
        if ((e_budget && e_budget[0] && strtoull(e_budget, NULL, 10) > 0) ||
            (e_slots && e_slots[0] && strtoul(e_slots, NULL, 10) > 0)) {
            airy_governance_config_t gcfg;
            __builtin_memset(&gcfg, 0, sizeof(gcfg));
            gcfg.token_budget = e_budget ? strtoull(e_budget, NULL, 10) : 0;
            gcfg.concurrency_slots = e_slots ? (uint32_t)strtoul(e_slots, NULL, 10) : 0;
            {
                const char *e_max = getenv("AIRY_GOV_MAX_CONCURRENT");
                const char *e_dl = getenv("AIRY_GOV_DEADLINE_MS");
                if (e_max && e_max[0])
                    gcfg.max_concurrent = (uint32_t)strtoul(e_max, NULL, 10);
                if (e_dl && e_dl[0])
                    gcfg.default_deadline_ms = strtoull(e_dl, NULL, 10);
            }
            governance = airy_governance_create(&gcfg);
            if (governance) {
                wh_cfg.governance = governance;
                const char *e_per_node = getenv("AIRY_GOV_TOKEN_PER_NODE");
                wh_cfg.token_per_node =
                    (e_per_node && e_per_node[0]) ? strtoull(e_per_node, NULL, 10) : 0;
                AIRY_LOG_INFO("airy_cli: unified governance attached "
                              "(token_budget=%llu, slots=%u, max_concurrent=%u, "
                              "token_per_node=%llu)",
                              (unsigned long long)gcfg.token_budget, gcfg.concurrency_slots,
                              gcfg.max_concurrent, (unsigned long long)wh_cfg.token_per_node);
            } else {
                AIRY_LOG_WARN("airy_cli: governance create failed, "
                              "unified governance disabled");
            }
        }
    }
    rt->governance = governance;
    airy_work_hall_t *hall = NULL;
    err = airy_work_hall_create(&wh_cfg, loop, &hall);
    if (err != AIRY_EOK || !hall) {
        AIRY_LOG_ERROR("airy_cli: work hall create failed (err=%d)", (int)err);
        if (wh_cfg.reviewer)
            airy_execution_review_destroy(wh_cfg.reviewer);
        rt->reviewer = NULL;
        return AIRY_ERR_GENERIC_FAIL;
    }
    airy_work_hall_bind_ops(hall);
    rt->hall = hall;

    llm_svc_adapter_config_t chat_cfg;
    __builtin_memset(&chat_cfg, 0, sizeof(chat_cfg));
    chat_cfg.llm_d_service_name = "llm_d";
    chat_cfg.channel_name = "coreloopthree-llm";
    g_chat_adapter = llm_svc_adapter_create(&chat_cfg);
    if (!g_chat_adapter)
        AIRY_LOG_WARN("airy_cli: chat adapter create failed, "
                      "falling back to task-only mode");

    /* 阶段 4：TUI 面板数据源绑定（任务看板 = work_hall 实时；事件流 =
     * hall_store gseq 因果序）。面板 ud 生命周期与调用方共存，退出前销毁。
     * 2026-08-19：绑定可操作动作（详情/取消/过滤），面板从只读升级为可操作。
     * 绑定不依赖 TUI 激活时刻（F8 之后随时可进入面板模式），ud 持有
     * 会话级 hall/hall_store 指针，成本可忽略。 */
    void *board_ud = NULL;
    void *events_ud = NULL;
    if (tui) {
        cli_panel_board_create(hall, &board_ud);
        cli_panel_events_create(hall_store, &events_ud);
        if (board_ud) {
            cli_tui_set_panel(tui, CLI_TUI_MODE_BOARD, board_ud, cli_panel_board_count,
                              cli_panel_board_line);
            cli_tui_set_panel_action(tui, CLI_TUI_MODE_BOARD, cli_panel_board_action);
        }
        if (events_ud) {
            cli_tui_set_panel(tui, CLI_TUI_MODE_EVENTS, events_ud, cli_panel_events_count,
                              cli_panel_events_line);
            cli_tui_set_panel_action(tui, CLI_TUI_MODE_EVENTS, cli_panel_events_action);
        }
    }
    rt->board_ud = board_ud;
    rt->events_ud = events_ud;

    /* 1.3 推理语言网关：创建时自动对 llm_d 已配置模型执行 Tokenizer 特征
     * 校准（lang_ratio），llm_d 不可用时降级为启发式决策（不阻断）。
     * 输入环节标准化（自然语言 → Canonical Data Format）+ 语言约束注入
     * + 输出后处理 + 每 100 次指令自动重校准（tick）。 */
    {
        airy_lang_gateway_config_t lg_cfg;
        __builtin_memset(&lg_cfg, 0, sizeof(lg_cfg));
        lg_cfg.auto_calibrate_on_create = 1;
        airy_lang_gateway_t *lg = NULL;
        airy_err_t lge = airy_lang_gateway_create(&lg_cfg, &lg);
        if (lge != AIRY_EOK || !lg) {
            AIRY_LOG_WARN("airy_cli: lang_gateway create failed (err=%d), "
                          "language routing disabled",
                          (int)lge);
            lg = NULL;
        } else {
            AIRY_LOG_INFO("airy_cli: lang_gateway attached (tokenizer calibration "
                          "on startup)");
        }
        rt->lang_gateway = lg;
        g_cli_lang_gateway = lg;
    }

    return AIRY_EOK;
}

/* 运行时上下文按序释放（2026-08-22 新增，与 cli_setup_runtime 对称）：
 * 面板 → reviewer → work_hall → validator → hall_store → governance →
 * rsched。幂等：全部字段置 NULL，重复调用安全。 */
static void cli_teardown_runtime(cli_runtime_ctx_t *rt)
{
    if (!rt)
        return;
    if (rt->events_ud)
        cli_panel_events_destroy(rt->events_ud);
    if (rt->board_ud)
        cli_panel_board_destroy(rt->board_ud);
    if (rt->reviewer)
        airy_execution_review_destroy(rt->reviewer);
    if (rt->hall)
        airy_work_hall_destroy(rt->hall);
    if (rt->validator)
        airy_artifact_validator_destroy(rt->validator);
    if (rt->hall_store)
        airy_hall_store_destroy(rt->hall_store);
    if (rt->governance)
        airy_governance_destroy(rt->governance);
    if (rt->rsched)
        airy_roadmap_sched_destroy(rt->rsched);
    if (rt->lang_gateway)
        airy_lang_gateway_destroy(rt->lang_gateway);
    AIRY_MEMSET(rt, 0, sizeof(*rt));
}

/* 蓝图三层快速路由（2026-08-22 从主循环提取，收敛圈复杂度）：
 * L1 状态机命中（零 token）→ 渲染 next_step；L2 语义缓存命中（低 token）
 * → 重放建议；两条命中路径均写决策链事件流并渲染 turn 分隔，返回 1
 * （调用方 continue）。MISS_L3（或 rsched 为 NULL）→ 释放 rs_out 后
 * 返回 0，调用方落回五阶段管线。 */
static int cli_blueprint_fastpath(airy_roadmap_sched_t *rsched, const char *input,
                                  uint64_t turn_start)
{
    if (!rsched)
        return 0;
    char *rs_out = NULL;
    airy_rs_dispatch_t rs_disp = AIRY_RS_DISPATCH_MISS_L3;
    airy_err_t rs_err = airy_roadmap_sched_process(rsched, input, &rs_out, &rs_disp);
    if (rs_err == AIRY_EOK && rs_disp == AIRY_RS_DISPATCH_HIT_L1) {
#ifdef AIRY_HAS_CJSON
        char next_buf[128] = "";
        if (rs_out) {
            cJSON *r = cJSON_Parse(rs_out);
            if (r) {
                cJSON *n = cJSON_GetObjectItem(r, "next_step");
                if (cJSON_IsString(n) && n->valuestring)
                    AIRY_STRNCPY_TERM(next_buf, n->valuestring, sizeof(next_buf));
                cJSON_Delete(r);
            }
        }
        if (g_cli_json_mode) {
            cJSON *jroot = cJSON_CreateObject();
            cJSON_AddStringToObject(jroot, "role", "dual_think");
            cJSON_AddStringToObject(jroot, "type", "l1_hit");
            cJSON_AddBoolToObject(jroot, "success", 1);
            cJSON_AddStringToObject(jroot, "next_step", next_buf);
            char *js = cJSON_PrintUnformatted(jroot);
            if (js) {
                cli_outf("%s\n", js);
                cJSON_free(js);
            }
            cJSON_Delete(jroot);
        } else if (next_buf[0] && g_cli_print_mode) {
            /* One-shot server mode (-p): L1 hit = the answer for this
             * turn; emit the next-step with provenance on stderr. */
            cli_trace("blueprint", "L1 state machine hit (zero token)");
            cli_outf("%s\n", next_buf);
        } else if (next_buf[0]) {
            char line[1024];
            snprintf(line, sizeof(line), "L1 blueprint state machine: advance to step "
                                         "%s%s%s (zero token)",
                     cli_c(CLR_CYAN), next_buf, cli_c(CLR_RESET));
            cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_PROF_THINK, "blueprint",
                                 line);
        } else
#endif /* AIRY_HAS_CJSON */
        {
            char line[1024];
            snprintf(line, sizeof(line), "L1 state machine hit (zero token): %s",
                     rs_out ? rs_out : "{}");
            cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_PROF_THINK, "blueprint",
                                 line);
            cli_trace("blueprint", "%s", line);
        }
        /* 阶段 4：蓝图 L1 状态机命中 → 决策链事件（preflight，cognition） */
        if (g_cli_hall_store) {
            char ev[512];
            snprintf(ev, sizeof(ev),
                     "{\"event\":\"blueprint_hit\",\"layer\":\"L1\",\"result\":%s}",
                     rs_out ? rs_out : "null");
            airy_hall_store_write(g_cli_hall_store, "default", "preflight", NULL,
                                  AIRY_HALL_CAT_CHAIN, "cognition", ev, NULL, 0);
        }
        AIRY_FREE(rs_out);
        if (!g_cli_json_mode)
            cli_render_turn_separator(cli_now_ms() - turn_start, NULL);
        return 1;
    }
    if (rs_err == AIRY_EOK && rs_disp == AIRY_RS_DISPATCH_HIT_L2) {
#ifdef AIRY_HAS_CJSON
        char *sugg = NULL;
        if (rs_out) {
            cJSON *r = cJSON_Parse(rs_out);
            if (r) {
                cJSON *s = cJSON_GetObjectItem(r, "suggestion");
                if (cJSON_IsString(s) && s->valuestring)
                    sugg = AIRY_STRDUP(s->valuestring);
                cJSON_Delete(r);
            }
        }
        if (sugg && sugg[0]) {
            if (g_cli_json_mode) {
                cJSON *jroot = cJSON_CreateObject();
                cJSON_AddStringToObject(jroot, "role", "super_agent");
                cJSON_AddStringToObject(jroot, "type", "l2_hit");
                cJSON_AddBoolToObject(jroot, "success", 1);
                cJSON_AddStringToObject(jroot, "result", sugg);
                char *js = cJSON_PrintUnformatted(jroot);
                if (js) {
                    cli_outf("%s\n", js);
                    cJSON_free(js);
                }
                cJSON_Delete(jroot);
            } else if (g_cli_print_mode) {
                /* One-shot server mode (-p): the cached suggestion is the
                 * result — emit it, with the replay provenance on stderr. */
                cli_trace("blueprint",
                          "L2 semantic cache hit (low token): replaying last result");
                cli_render_markdown(sugg, 0);
            } else {
                cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_PROF_THINK, "blueprint",
                                     "L2 semantic cache hit (low token): replaying last result");
                cli_render_super_agent(sugg);
            }
            AIRY_FREE(sugg);
        } else
#endif /* AIRY_HAS_CJSON */
        {
            char line[1024];
            snprintf(line, sizeof(line), "L2 semantic cache hit (low token): %s",
                     rs_out ? rs_out : "{}");
            cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_PROF_THINK, "blueprint",
                                 line);
        }
        /* 阶段 4：蓝图 L2 语义缓存命中 → 决策链事件（preflight，cognition） */
        if (g_cli_hall_store) {
            char ev[512];
            snprintf(ev, sizeof(ev),
                     "{\"event\":\"blueprint_hit\",\"layer\":\"L2\",\"result\":%s}",
                     rs_out ? rs_out : "null");
            airy_hall_store_write(g_cli_hall_store, "default", "preflight", NULL,
                                  AIRY_HALL_CAT_CHAIN, "cognition", ev, NULL, 0);
        }
        AIRY_FREE(rs_out);
        if (!g_cli_json_mode)
            cli_render_turn_separator(cli_now_ms() - turn_start, NULL);
        return 1;
    }
    /* L3 全量规划（miss 或语义提示）：消费 rs_out 的 semantic_hint，
     * 有相似历史任务建议时轻量提示（不改变路由，仅信息增量）。
     * 命中率审计（2026-08-20）：此前 L2 语义命中被完全丢弃。 */
    if (rs_out && rs_out[0]) {
        char *hint = NULL;
        cJSON *r = cJSON_Parse(rs_out);
        if (r) {
            const char *reason = NULL;
            cJSON *rz = cJSON_GetObjectItem(r, "reason");
            if (cJSON_IsString(rz))
                reason = rz->valuestring;
            if (reason && strcmp(reason, "semantic_hint") == 0) {
                cJSON *s = cJSON_GetObjectItem(r, "suggestion");
                if (cJSON_IsString(s) && s->valuestring)
                    hint = AIRY_STRDUP(s->valuestring);
            }
            cJSON_Delete(r);
        }
        if (hint) {
            cli_trace("blueprint", "L2 semantic hint for similar task");
            if (!g_cli_print_mode && !g_cli_json_mode) {
                char line[512];
                snprintf(line, sizeof(line), "检测到相似历史任务，可参考：%s", hint);
                cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_PROF_THINK,
                                     "blueprint", line);
            }
            AIRY_FREE(hint);
        }
    }
    AIRY_FREE(rs_out);
    return 0;
}

int main(int argc, char *argv[])
{
    /* Server one-shot mode (-p/--print): run a single prompt then exit. */
    const char *print_prompt = NULL;
    if (cli_parse_args(argc, argv, &print_prompt) != 0)
        return 1;
    /* Terminal capability probe (TTY / color level / NO_COLOR) before any
     * output so every render call degrades consistently on servers and logs. */
    cli_term_init();
    /* 2026-08-25：主题（浅色/深色）自适应——OSC 11 查询终端背景色，
     * 需在进入交互 raw mode 前完成（此时 stdin 仍可读查询响应）。 */
    cli_theme_init();
    cli_term_title("AgentRT · airy_cli");

    /* 运行时根解析 SSoT：airy_paths_init() 在 commons/platform 统一解析
     * AIRY_HOME（显式 env → install.env 固化安装根 → $HOME/.airymaxrt），
     * 并 setenv 兼容变量（AIRY_HOME/AIRY_RUNTIME_DIR 等）供 cli_rt_dir 等
     * getenv 消费者读取。直接运行 airy_cli（无 AIRY_HOME 环境变量）时，
     * 此前解析到 $HOME/.airymaxrt 导致 llm.sock 连接失败；本调用使独立
     * 调用与 airymaxrt 启动器的解析结果一致（2026-08-16）。 */
    (void)airy_paths_init();

    /* Keep the CLI chat UI clean: raise the global log level to ERROR to silence
      * GCCP/ThinkDual INFO/WARN noise (the logging constructor already ran
      * log_init with defaults; adjust the level at runtime, no re-init). */
    log_set_module_level("*", LOG_LEVEL_ERROR);

    /* Task-set cancel entry: Ctrl+C sets the flag (no process exit);
      * run_to_completion aborts after the current node completes. */
#if !defined(_WIN32)
    {
        struct sigaction sa;
        __builtin_memset(&sa, 0, sizeof(sa));
        sa.sa_handler = cli_sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
    }
#endif

    /* 交互界面（2.3.7）：默认行渲染流式模式（打字机 + 思考链折叠 +
     * markdown 精修）；全屏 TUI 页面按 F8 切换进入（Claude Code style：
     * pinned header + scrollable history + bottom input line）。Piped/
     * logged output keeps the line-oriented renderer. One-shot server
     * mode (-p) never enters the TUI. */
    cli_tui_t *tui = NULL;
    if (!g_cli_print_mode) {
        cli_tui_create(&tui);
    }

    /* 交互模式（行渲染流式 + 全屏 TUI）：把进程 stderr 重定向到日志文件
     * （$AIRY_HOME/logs/airy_cli.log）。daemon RPC 层在 llm_d 离线等故障
     * 时会向 stderr 打印 ERROR（如 C-L02 / rpc_connect_unix），直接泄漏
     * 会污染对话/破坏全屏界面（2.3.11：不把内部实现暴露进对话）；落盘
     * 保留完整诊断。-p 模式同样在终端（stderr 为 TTY）下重定向——人类使
     * 用时 trace 行（[intent]/[chat]，cli_trace）与 ERROR 日志会与答案
     * 视觉混叠，落盘既保诊断又不污染输出；仅当 stderr 被管道/脚本消费
     * （非 TTY）时保留直连，供脚本读取 trace 进度（cli_trace 约定）。
     * Windows 无全屏 TUI 且 dup2/fileno 为 POSIX 接口，整体跳过。 */
#ifndef _WIN32
    if (!g_cli_print_mode || isatty(STDERR_FILENO)) {
        char logpath[512];
        const char *logdir = airy_log_dir();
        /* 2.3.4：重定向前确保日志目录存在——此前 fopen 失败（首次运行/
         * AIRY_HOME 被清/权限变更）会静默跳过重定向，ERROR 级日志（如
         * llm_d 离线的 rpc_connect_unix）直打终端混入对话。失败时明确
         * 告警，不再静默降级。 */
        if (airy_mkdir_p(logdir) != 0) {
            fprintf(stderr, "[airy_cli] 无法创建日志目录: %s（stderr 将直连终端）\n", logdir);
        } else {
            snprintf(logpath, sizeof(logpath), "%s/airy_cli.log", logdir);
            FILE *lf = fopen(logpath, "a");
            if (lf) {
                fflush(stderr);
                dup2(fileno(lf), STDERR_FILENO);
                fclose(lf);
            } else {
                fprintf(stderr, "[airy_cli] 无法打开日志文件: %s（stderr 将直连终端）\n", logpath);
            }
        }
    }
#endif
    g_session_start_ms = cli_now_ms();

    /* Dual-thinking three-model injection (GRAD separation of powers; user-chosen models):
      *   AIRY_MODEL_T2    -> model A (generator)
      *   AIRY_MODEL_T1F   -> model B (context arbiter)
      *   AIRY_MODEL_T1P   -> model C (logic verifier)
      * Config is unified with think_d's model.yaml (env > think section >
      * llm.model default; see cli_think_cfg_load), so the header shows the
      * models that actually take effect. Unset values use the provider
      * default model (backward compatible). Read early so the combined
      * blue-framed header can render them in one pinned startup block. */
    char m_s2[128], m_verify[128], m_expert[128];
    cli_think_cfg_load(m_s2, sizeof(m_s2), m_verify, sizeof(m_verify),
                       m_expert, sizeof(m_expert));
    /* 2.2.1.3：hero 模型名快照（终端 resize / F8 退出重建三区时用） */
    cli_tui_set_header_models(tui, m_s2[0] ? m_s2 : NULL,
                              m_verify[0] ? m_verify : NULL,
                              m_expert[0] ? m_expert : NULL);

    cli_print_system_header(m_s2[0] ? m_s2 : NULL,
                            m_verify[0] ? m_verify : NULL,
                            m_expert[0] ? m_expert : NULL);
    if (g_cli_print_mode) {
        /* One-shot server mode: no banner, no pinned header, no footer hint. */
        (void)0;
    } else if (cli_tui_active(tui)) {
        /* Full-screen TUI pins its own header boundary after the hero. */
        cli_tui_pin_header(tui);
    }
    /* Plain TTY: cli_print_system_header already pinned the blue-framed
     * header, keeping the system block fixed above the dialogue. */

    /* 核心引擎装配（2026-08-22 拆分自 main 初始化段）：loop 创建 +
     * 记忆引擎注入（2.2.4 对话路径读写）+ cognition 引擎接线（GCCP
     * 交互回调、TC3 三模型、GRAD 决策链反馈）。失败返回 NULL。 */
    airy_cognition_engine_t *cog = NULL;
    airy_core_loop_t *loop = cli_setup_core_engines(m_s2, m_verify, m_expert, &cog);
    if (!loop)
        return 1;

    /* 后续初始化段的错误码（rsched / work_hall 创建等） */
    airy_err_t err = AIRY_EOK;

    /* 3. Blueprint scheduling (Roadmap Sched): result feedback hook (synergy point 1).
      * Node final states feed back via progress_cb into the L2 cache / failure fingerprints / L1.
      * P1e: L2 dual-write persistence ($AIRY_HOME/data/agentrt/roadmap/l2_semantic_cache.json,
      * restored on restart); Embedding + HNSW vector index (MemoryRovol, degrades when unlinked).
      * Creation failure only degrades (no feedback); the CLI main flow continues.
      * 运行时上下文装配（2026-08-22 拆分自 main）：rsched / validator /
      * hall_store / governance / work_hall / chat adapter / 自愈 / TUI 面板，
      * 统一收敛进 cli_runtime_ctx_t（cli_setup_runtime / cli_teardown_runtime
      * 对称装配与释放，main 不直接持有组件）。 */
    cli_runtime_ctx_t rt;
    err = cli_setup_runtime(loop, tui, &rt);
    if (err != AIRY_EOK) {
        airy_loop_destroy(loop);
        return 1;
    }

    /* Task-set cancellation: the engine holds g_cli_cancel; after SIGINT,
      * run_to_completion polls and aborts (after the current node finishes) */
    err = airy_loop_dag_set_cancel_flag(loop, &g_cli_cancel);
    if (err != AIRY_EOK)
        AIRY_LOG_WARN("airy_cli: set cancel flag failed (err=%d)", (int)err);

    char input[8192];
    int quit_flag = 0;
    int switch_tui_flag = 0;
    cli_cmd_ctx_t cmd_ctx = {.hall = rt.hall, .quit = &quit_flag, .switch_tui = &switch_tui_flag};
    int print_consumed = 0;

    /* 阶段 2 生命周期层 reconcile：agent 自愈重启（AIRY_SELF_HEAL=1 或
     * AIRY_SELF_HEAL_AGENTS 列表启用；主循环每轮 reconcile_once 驱动） */
    {
        const char *e_sh = getenv("AIRY_SELF_HEAL");
        const char *e_sh_agents = getenv("AIRY_SELF_HEAL_AGENTS");
        if ((e_sh && e_sh[0] && strcmp(e_sh, "0") != 0) || (e_sh_agents && e_sh_agents[0]))
            cli_daemon_lifecycle_init(e_sh_agents);
    }

    for (;;) {
        /* 会话状态栏（Claude Code 风格）：模型 · 消息数 · 已耗时，输入行
         * 右侧 dim 显示。每轮开头刷新，覆盖 L1/L2/chat/plan 全部分支。 */
        if (cli_tui_active(tui)) {
            char st[96];
            const char *mdl = m_verify[0] ? m_verify : "default";
            uint64_t sess_sec = (cli_now_ms() - g_session_start_ms) / 1000;
            snprintf(st, sizeof(st), "\u25c7 %zu msgs \u00b7 %02llu:%02llu \u00b7 %s",
                     g_history_count / 2, (unsigned long long)(sess_sec / 60),
                     (unsigned long long)(sess_sec % 60),
                     (mdl && mdl[0]) ? mdl : "default");
            cli_tui_set_status(tui, st);
        }
        /* P23 reconcile extension: fire due execution-level re-dispatches
         * (no-op when redispatch_max == 0). Controller-driven reconcile —
         * each turn polls the failed-execution queue. */
        (void)airy_work_hall_redispatch_once(rt.hall);
        /* p6 (2026-08-24): board TTL housekeeping — purge terminal board
         * entries past their retention window (same controller rhythm). */
        (void)airy_work_hall_ttl_purge(rt.hall);
        /* 阶段 2 生命周期层 reconcile：agent 自愈重启（AIRY_SELF_HEAL=1 或
         * AIRY_SELF_HEAL_AGENTS 列表启用；未启用时 no-op）。与执行层
         * redispatch 并列，构成声明式自愈第三层。 */
        (void)cli_daemon_lifecycle_reconcile_once();
        size_t input_len = 0;
        if (g_cli_print_mode) {
            /* One-shot server mode (-p)：prompt 参数模式单轮；stdin 流模式
             * 逐行执行直到 EOF（非 TTY 会话/管道连续对话，每行一轮）。
             * 无 prompt 回显。 */
            if (print_prompt && print_prompt[0]) {
                if (print_consumed)
                    break;
                print_consumed = 1;
                AIRY_STRNCPY_TERM(input, print_prompt, sizeof(input));
                input_len = strlen(input);
            } else {
                if (!fgets(input, sizeof(input), stdin))
                    break; /* EOF：stdin 流结束 */
                input_len = strlen(input);
                while (input_len > 0 &&
                       (input[input_len - 1] == '\n' || input[input_len - 1] == '\r'))
                    input[--input_len] = '\0';
                if (input_len == 0)
                    continue; /* 空行跳过；EOF 由 fgets 返回 NULL 终止循环 */
                /* 纯空白行（空格/Tab）：跳过，不进入对话（与交互模式一致）。 */
                {
                    size_t nz = 0;
                    while (nz < input_len && (input[nz] == ' ' || input[nz] == '\t'))
                        nz++;
                    if (nz == input_len)
                        continue;
                }
            }
            /* One-shot server mode still honors slash commands: /daemon etc.
             * are dispatchable, so scripting `-p "/daemon status"` works the
             * same as typing it in the interactive session. */
            if (cli_dispatch_command(input, &cmd_ctx)) {
                if (quit_flag)
                    break;
                continue;
            }
        } else {
            if (!cli_tui_active(tui)) {
                /* 三区布局（TTY）：提示符由 readline（tui_line_redraw /
                 * TUI_INPUT_PREFIX）在固定底部输入行绘制，此处不再打印，
                 * 避免双重提示符回显（2026-08-20 根因：双 airy> 叠加）；
                 * 非 TTY（管道/日志）无底部条时保持传统换行提示符。 */
                if (!cli_term_input_begin()) {
                    if (!cli_term_is_tty())
                        cli_outf("\n\n%sairy>%s ", cli_c(CLR_CYAN),
                                 cli_c(CLR_RESET));
                }
                fflush(stdout);
            }
            int rl = cli_tui_readline(tui, input, sizeof(input), &input_len);
            if (rl == 0) {
                /* EOF / abort：先清输入行并把光标送回滚动区，退出横幅
                 * 渲染在对话区而不是输入行上。 */
                cli_term_input_submit();
                break;
            }
            if (rl == 2) {
                /* 2.3.7：行渲染模式 F8 / 空输入 ↑ → 进入全屏页面。
                 * 先重置页面历史（避免反复 F8 切换时 hero 重复累积），
                 * 再切渲染目标进 TUI，重放 hero（此时 cli_out 走 TUI
                 * emit），pin 头部，最后重放行模式对话历史。 */
                cli_tui_enter(tui);
                if (cli_tui_active(tui)) {
                    cli_tui_reset_history(tui);
                    cli_render_set_tui(tui);
                    cli_term_header_unpin();
                    cli_print_system_header(m_s2[0] ? m_s2 : NULL,
                                            m_verify[0] ? m_verify : NULL,
                                            m_expert[0] ? m_expert : NULL);
                    cli_tui_pin_header(tui);
                    cli_tui_replay_history(tui);
                    cli_tui_redraw(tui);
                }
                continue;
            }
            if (rl == 3) {
                /* 2.3.7：全屏 F8 → 退出回行渲染流式模式。
                 * 2.2.1.2 修复：退出 alt screen 后，主屏仍是进入 TUI 前
                 * 的画面——直接重 pin 会让新输出与残留内容重叠（用户
                 * 反馈"英雄区混乱/界面重叠"）。统一走三区重建：解 pin →
                 * 清屏 → 重绘 hero → 重放对话历史 → 光标回滚动区末行。 */
                cli_tui_leave(tui);
                cli_render_set_tui(NULL);
                cli_tui_rebuild_three_zone(tui);
                continue;
            }
            /* 三区布局：清掉输入行回显并把光标送回滚动区末行，之后无论
             * 是对话输出、slash 命令还是退出横幅，都不会盖住底部输入条。
             * 本轮对话进行期间在输入区保留一个 dim 占位提示符，使三区
             * （英雄区/对话区/输入区）边界始终可见。 */
            cli_term_input_submit();
            if (input_len > 0 && cli_term_input_active()) {
                cli_term_input_begin();
                cli_outf("%sairy>%s", cli_c(CLR_DIM), cli_c(CLR_RESET));
                cli_term_input_hop();
            }
            if (input_len == 0)
                continue;
            /* 纯空白输入（空格/Tab 串）：静默跳过，不进入对话与命令分发。
             * 此前仅跳过空串，空格串会被当作对话发给 LLM（无效轮次）。 */
            {
                size_t nz = 0;
                while (nz < input_len && (input[nz] == ' ' || input[nz] == '\t'))
                    nz++;
                if (nz == input_len)
                    continue;
            }
            if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0)
                break;
            /* hero 提示 "? /help 查看命令"：单独的 ? 即帮助入口，
             * 不应作为对话发送给 LLM（2026-08-22 交互一致性修复）。
             * trim 尾部空白后比较（用户可能输入 "? " 或 "？"）。 */
            if (input[0] == '?' || input[0] == '\xef') { /* ? 或 UTF-8 ？(0xEF 起始) */
                size_t tl = input_len;
                while (tl > 0 && (input[tl - 1] == ' ' || input[tl - 1] == '\t'))
                    tl--;
                int is_qmark = (tl == 1 && input[0] == '?') ||
                               (tl == 3 && (unsigned char)input[0] == 0xEF &&
                                (unsigned char)input[1] == 0xBC &&
                                (unsigned char)input[2] == 0x9F);
                if (is_qmark) {
                    cmd_help(NULL, &cmd_ctx);
                    continue;
                }
            }

            if (cli_dispatch_command(input, &cmd_ctx)) {
                if (quit_flag)
                    break;
                continue;
            }
        }

        cli_render_user_message(input);

        uint64_t turn_start = cli_now_ms();

        /* 1.3 推理语言网关：输入标准化（自然语言 → Canonical Data Format）。
         * 纯本地信号提取（语言/任务/上下文/代码含量）+ 多因子路由决策，
         * 零 LLM 调用——省 token 从输入环节开始。语言约束 System Prompt
         * 供 chat 路径注入首条 system；决策链渲染供用户感知推理/输出语言
         * 策略。指令计数每 100 次自动触发 Tokenizer 特征重校准。 */
        if (rt.lang_gateway) {
            airy_canonical_request_t *lg_req = NULL;
            if (airy_lang_gateway_process(rt.lang_gateway, input, NULL, 0,
                                          &lg_req) == AIRY_EOK && lg_req) {
                AIRY_FREE(g_cli_lang_sys_prompt);
                g_cli_lang_sys_prompt =
                    AIRY_STRDUP(lg_req->system_prompt ? lg_req->system_prompt : "");
                g_cli_lang_output = lg_req->routing.output_lang;
                char lg_line[192];
                snprintf(lg_line, sizeof(lg_line), "推理语言: %s · 输出语言: %s · %s",
                         airy_lang_name(lg_req->routing.reasoning_lang),
                         airy_lang_name(lg_req->routing.output_lang),
                         lg_req->routing.decision_reason
                             ? lg_req->routing.decision_reason
                             : "");
                cli_render_sub_agent_line(CLI_ROLE_TRACE, "lang", lg_line);
                cli_trace("lang", "detected=%s reasoning=%s output=%s",
                          airy_lang_name(lg_req->signals.detected_lang),
                          airy_lang_name(lg_req->routing.reasoning_lang),
                          airy_lang_name(lg_req->routing.output_lang));
                airy_lang_gateway_free_canonical(lg_req);
            }
            if (airy_lang_gateway_tick(rt.lang_gateway))
                cli_trace("lang", "periodic tokenizer recalibration triggered");
        }

        /* 4.0b Blueprint scheduling three-tier routing (checked before intent
          * classification: transfer commands like "continue/next" are pure
          * blueprint instructions and must short-circuit here, otherwise the
          * LLM classifier may route them into the chat branch and the L1 state
          * machine would never advance).
          *   L1 state-machine hit (zero tokens) -> next step;
          *   L2 semantic-cache hit (few tokens) -> a suggestion;
          *   MISS_L3 -> falls through to the five-phase pipeline. */
        if (cli_blueprint_fastpath(rt.rsched, input, turn_start))
            continue;

        int is_task = cli_classify_input(input);
        cli_trace("intent", "%s", is_task ? "task" : "chat");
        if (is_task == 0) {
            cli_chat_reply(input);
            /* 2.1.1.5：回合分隔带真实 token/费用（llm_d usage 回填，
             * 含思考 token），用户可感知真实消耗与计费。 */
            char chat_metrics[96];
            chat_metrics[0] = '\0';
            uint64_t toks = 0;
            double cost = 0.0;
            cli_chat_usage_get_session(&toks, &cost);
            if (toks > 0 || cost > 0.0)
                snprintf(chat_metrics, sizeof(chat_metrics), "Tokens: %llu · Cost: $%.6f",
                         (unsigned long long)toks, cost);
            cli_render_turn_separator(cli_now_ms() - turn_start,
                                      chat_metrics[0] ? chat_metrics : NULL);
            continue;
        }

        g_cli_cancel = 0;

        cli_render_phase("认知规划");
        airy_task_plan_t *plan = NULL;
        const char *think_sock = getenv("AIRY_THINK_SOCK");
        if (think_sock && think_sock[0]) {
            cli_spinner_start("Remote dual-thinking (think_d)");
            err = cli_think_process_remote(think_sock, input, input_len, &plan);
            if (err != AIRY_EOK || !plan) {
                cli_spinner_stop(0, "remote thinking failed");
                cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_DUAL_SLOW_THINK, "深度思考",
                                     "远程思考引擎不可用，已回退内置引擎。");
                plan = NULL;
            } else {
                cli_spinner_stop(1, NULL);
                cli_render_sub_agent_line(CLI_ROLE_TRACE, "think_d", "Remote plan generated.");
            }
        }
        if (!plan) {
            cli_spinner_start("Analyzing task (LLM decomposition + intent)");
            err = airy_cognition_process(cog, input, input_len, &plan);
            if (err != AIRY_EOK || !plan) {
                cli_spinner_stop(0, "planning failed");
                cli_trace("plan", "failed err=%d", (int)err);
                char line[128];
                snprintf(line, sizeof(line), "规划失败：%s", cli_err_desc((int)err));
                cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_DUAL_SLOW_THINK, "认知规划", line);
                continue;
            }
            cli_spinner_stop(1, NULL);
        }
        /* 4.1.5 Absorb converged blueprints: register the L1 state machine and reset the TTL,
          * so later "continue/next" hits L1 (zero-token progress). Failure only degrades. */
        if (rt.rsched)
            airy_roadmap_sched_absorb(rt.rsched, plan, NULL, NULL);
        cli_trace("plan", "plan_id=%s nodes=%zu entry=%zu",
                  plan->task_plan_id ? plan->task_plan_id : "?",
                  plan->task_plan_node_count, plan->task_plan_entry_count);
        /* 阶段 4：计划生成 → 决策链 BLUEPRINT 事件（preflight，cognition；
         * 计划即蓝图，plan_id 供提交事件关联，exec 链与 preflight 链由此
         * 可追溯。BLUEPRINT 类目由此获得生产写入点） */
        if (g_cli_hall_store && plan && plan->task_plan_id) {
            char ev[256];
            snprintf(ev, sizeof(ev), "{\"plan_id\":\"%s\",\"nodes\":%zu,\"entry\":%zu}",
                     plan->task_plan_id, plan->task_plan_node_count, plan->task_plan_entry_count);
            airy_hall_store_write(g_cli_hall_store, "default", "preflight", NULL,
                                  AIRY_HALL_CAT_BLUEPRINT, "cognition", ev, NULL, 0);
        }

        /* 阶段 4（item 4）：认知阶段并行子 agent 审查——计划生成后派出
         * fact（完整性/目标覆盖）与 risk（风险/边界/依赖）两个审查子 agent
         * 同步并行审查，汇总意见写 preflight verify 事件（决策链可见）。
         * agent_d 不可用或 AIRY_COGNITION_REVIEW=0 时静默跳过，不阻塞执行。 */
        {
            char *review_report = NULL;
            const char *agent_sock_env = getenv("AIRY_AGENT_SOCK");
            if (cli_cognition_review(agent_sock_env, input, plan, &review_report) > 0 &&
                review_report) {
                cli_trace("review",
                          "parallel cognition review (fact+risk) merged");
                cli_render_sub_agent_line(CLI_ROLE_TRACE, "cognition",
                                          "Parallel sub-agent review completed");
                if (g_cli_hall_store) {
                    /* #12 修复：preflight VERIFY 事件 task_id 语义——此前用
                     * 字面量 "preflight" 且 content 为裸 review_report，无法与
                     * 后续执行（exec_id）及 BLUEPRINT 计划事件关联。改用
                     * plan_id 作 task_id（计划审查归属计划，执行审查归属
                     * exec_id），并在 content 中冗余 plan_id 便于决策链检索。 */
                    const char *plan_id = (plan && plan->task_plan_id) ? plan->task_plan_id : "";
                    char ev[512];
                    snprintf(ev, sizeof(ev), "{\"plan_id\":\"%s\",\"reviews\":%s}",
                             plan_id, review_report);
                    airy_hall_store_write(g_cli_hall_store, "default",
                                          plan_id[0] ? plan_id : "preflight", NULL,
                                          AIRY_HALL_CAT_VERIFY, "cognition", ev, NULL, 0);
                }
                AIRY_FREE(review_report);
            }
        }

        taskflow_workflow_t *wf = NULL;
        err = airy_plan_to_workflow(plan, &wf);
        if (err != AIRY_EOK || !wf) {
            char line[128];
            snprintf(line, sizeof(line), "工作流适配失败：%s", cli_err_desc((int)err));
            cli_render_sub_agent_line(CLI_ROLE_ERROR, "DAG", line);
            airy_task_plan_free(plan);
            continue;
        }
        {
            char hdrs[256] = "";
            size_t ho = 0;
            for (size_t ni = 0; ni < wf->node_count && ho < sizeof(hdrs) - 2; ni++) {
                ho += (size_t)snprintf(hdrs + ho, sizeof(hdrs) - ho, "%s%s",
                                       ni > 0 ? "," : "",
                                       wf->nodes[ni].task_handler_name
                                           ? wf->nodes[ni].task_handler_name
                                           : "?");
            }
            cli_trace("dag", "id=%s nodes=%zu edges=%zu [%s]", wf->id,
                      wf->node_count, wf->edge_count, hdrs);
        }

        /* 4.3 执行计划：直播看板（结构 + 图标编排）。看板块打印后，下方
         * 紧跟 spinner 开始运行，期间不再输出新行，保证原位重绘的相对
         * 几何稳定；提交过程静默（cli_trace 记录），不再单独打阶段头。 */
        cli_live_board_begin(wf);
        char *exec_id = NULL;
        const char *sched_sock = getenv("AIRY_SCHED_SOCK");
        int sched_remote = (sched_sock && sched_sock[0]) ? 1 : 0;
        /* 改进6（P3）: 绑定执行蓝图（BORROW）——执行复核在 wait 返回前读取
         * entry 节点的 I/O 契约（node_id/node_goal/output_signatures/
         * invariant_guard）判定产物。plan 生命周期覆盖 submit~wait，
         * 本轮结束后由调用方统一释放。 */
        if (!sched_remote)
            airy_work_hall_set_blueprint(rt.hall, plan);
        if (sched_remote) {
            err = cli_dag_submit_remote(sched_sock, wf, input, rt.main_workspace_dir, &exec_id);
            if (err != AIRY_EOK || !exec_id) {
                char line[128];
                snprintf(line, sizeof(line), "远程提交失败（%s），已回退本地执行。",
                         cli_err_desc((int)err));
                cli_render_sub_agent_line(CLI_ROLE_ERROR, "sched_d", line);
                AIRY_FREE(exec_id);
                exec_id = NULL;
                sched_remote = 0;
            } else {
                cli_trace("submit", "%s dag=%s", CLI_ICON_DIAMOND, exec_id);
                cli_chain_record_submit(exec_id, plan, wf);
            }
        }
        if (!sched_remote) {
            err = airy_work_hall_submit(rt.hall, wf, input, &exec_id);
            if (err != AIRY_EOK || !exec_id) {
                char line[128];
                snprintf(line, sizeof(line), "任务提交失败：%s", cli_err_desc((int)err));
                cli_render_sub_agent_line(CLI_ROLE_ERROR, "hall", line);
                /* 改进6（P3）: 提交失败即解绑蓝图（BORROW 指针随 plan 释放失效） */
                airy_work_hall_set_blueprint(rt.hall, NULL);
                airy_workflow_free(wf);
                airy_task_plan_free(plan);
                continue;
            }
            cli_trace("submit", "%s exec=%s", CLI_ICON_DIAMOND, exec_id);
            cli_chain_record_submit(exec_id, plan, wf);
        }

        /* 4.4 Board polling
          * taskflow_engine_start only runs the start node synchronously (progress=1/N);
          * the rest is driven by run_to_completion (dag_wait). Poll here to
          * show live status; like Claude CLI, print a line only when state/progress
          * changes (with a progress bar), avoiding a fixed 200ms spam. If stale for
          * too long (stale_polls past the threshold), break into the 4.5 wait path.
          * Remote sched_d polls dag_status instead of the airy_work_hall_status board.
          * A one-line status indicator (spinner) runs below the board lines and is
          * paused around every full line so output never interleaves. */
        {
            char run_title[128];
            snprintf(run_title, sizeof(run_title), "Running (%s)",
                     sched_remote ? "sched_d" : "hall");
            cli_spinner_start(run_title);
        }
        /* Node-level live board for remote DAGs: one icon+goal line per node,
         * re-printed only when a node's state changes (Claude Code style). */
        cli_dag_board_t *node_board =
            sched_remote ? cli_dag_node_board_create() : NULL;
        int board_polls = 0;
        int stale_polls = 0;
        int done = 0;
        int run_failed = 0;
        int spin_running = 1;
        char last_state[16] = "";
        double last_progress = -1.0;
        for (;;) {
            /* 2.3.7：轮询节拍期间检测用户输入（插入对话/中断）。
             * 有输入先处理（不阻塞任务轮询），再继续本轮的 200ms 节拍。 */
            int input_rc = cli_task_poll_input();
            if (input_rc == 1) {
                /* 插入对话：回复渲染在看板块下方，原位重绘的相对几何失效，
                 * 看板退化为追加行模式（后续状态变化走 cli_board_line）。 */
                cli_live_board_done();
            }
            if (input_rc < 0) {
                /* 用户请求打断：等同 SIGINT，统一走取消路径 */
                cli_spinner_stop(0, "aborted");
                spin_running = 0;
                cli_render_sub_agent_line(CLI_ROLE_ERROR, "cancel",
                                          "Abort requested, stopping the task ...");
                break;
            }
            airy_sleep_ms(200);
            cli_spinner_tick();

            if (g_cli_cancel) {
                cli_spinner_stop(0, "aborted");
                spin_running = 0;
                cli_render_sub_agent_line(CLI_ROLE_ERROR, "cancel",
                                          "Abort requested, stopping the task ...");
                break;
            }
            char cur_state[16];
            double cur_progress = -1.0;
            if (sched_remote) {
                char *final_result = NULL;
                cli_dag_poll_rc_t prc =
                    cli_dag_poll_remote(sched_sock, exec_id, &cur_progress, cur_state,
                                        sizeof(cur_state), &final_result);
                AIRY_FREE(final_result);
                if (prc == CLI_DAG_POLL_ERROR) {
                    cli_spinner_stop(0, "status query failed");
                    spin_running = 0;
                    cli_render_sub_agent_line(CLI_ROLE_ERROR, "sched_d",
                                              "Status query failed.");
                    break;
                }
                if (node_board) {
                    /* TTY 直播看板激活：远程节点状态只喂快照，由看板统一
                     * 原位重绘；退化模式（非 TTY）仍用追加式节点行。 */
                    cli_spinner_pause();
                    if (cli_live_board_active())
                        cli_dag_board_snapshot(sched_sock, exec_id, cli_live_board_set_node);
                    else {
                        int nb_terminal =
                            cli_dag_node_board_tick(node_board, sched_sock, exec_id);
                        if (nb_terminal)
                            cli_dag_node_board_destroy(node_board), node_board = NULL;
                    }
                    cli_spinner_resume();
                }
                if (prc == CLI_DAG_POLL_DONE) {
                    run_failed = (strcmp(cur_state, "failed") == 0 ||
                                  strcmp(cur_state, "canceled") == 0);
                    cli_spinner_pause();
                    if (!cli_live_board_refresh(cur_state, cur_progress))
                        cli_board_line("sched_d", exec_id, cur_state, cur_progress);
                    cli_spinner_stop(!run_failed, NULL);
                    spin_running = 0;
                    done = 1;
                    break;
                }
            } else {
                airy_work_hall_entry_t *entry = NULL;
                airy_err_t st_err = airy_work_hall_status(rt.hall, exec_id, &entry);
                if (st_err != AIRY_EOK || !entry) {
                    cli_spinner_stop(0, "status query failed");
                    spin_running = 0;
                    cli_render_sub_agent_line(CLI_ROLE_ERROR, "hall",
                                              "Status query failed.");
                    break;
                }
                snprintf(cur_state, sizeof(cur_state), "%s", entry->state);
                cur_progress = entry->progress;
                done =
                    (strcmp(entry->state, "completed") == 0 ||
                     strcmp(entry->state, "failed") == 0 || strcmp(entry->state, "canceled") == 0);
                run_failed =
                    (strcmp(entry->state, "failed") == 0 ||
                     strcmp(entry->state, "canceled") == 0);
                airy_work_hall_entry_free(entry);
            }
            int state_changed = (strcmp(cur_state, last_state) != 0);
            double prog_changed =
                (cur_progress - last_progress) >= 0.01 || (cur_progress - last_progress) <= -0.01;
            if (state_changed || prog_changed) {
                cli_spinner_pause();
                if (!cli_live_board_refresh(cur_state, cur_progress))
                    cli_board_line(sched_remote ? "sched_d" : "hall", exec_id, cur_state,
                                   cur_progress);
                cli_spinner_resume();
                snprintf(last_state, sizeof(last_state), "%s", cur_state);
                last_progress = cur_progress;
                char sbar[16];
                cli_compact_bar(sbar, sizeof(sbar), cur_progress, 8);
                cli_trace("status", "%s %s state=%s %s %3.0f%%",
                          cli_icon_for_state(cur_state), sched_remote ? "sched_d" : "hall",
                          cur_state, sbar, cur_progress * 100.0);
            }
            if (done) {
                cli_spinner_stop(!run_failed, NULL);
                spin_running = 0;
                break;
            }
            board_polls++;
            if (!state_changed && !prog_changed) {
                stale_polls++;
            } else {
                stale_polls = 0;
            }
            if (board_polls >= 300 || stale_polls >= 10)
                break;
        }
        /* Release any board not already freed by the terminal tick. */
        if (node_board) {
            cli_dag_node_board_destroy(node_board);
            node_board = NULL;
        }
        if (spin_running) {
            /* Polling exhausted without a terminal state (stale board):
             * leave the status line running and fall into the blocking wait,
             * which drives the engine to the real completion. A single dim
             * trace line, no internal jargon. */
            if (board_polls > 0 && stale_polls >= 10) {
                cli_spinner_pause();
                cli_live_board_extra(); /* 回退行打在块与 spinner 之间 */
                cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_THINK,
                                     sched_remote ? "sched_d" : "hall",
                                     "still running, waiting for completion ...");
                cli_spinner_resume();
            }
        }

        uint32_t vf_before = 0;
        airy_work_hall_verify_stats(rt.hall, NULL, &vf_before, NULL);
        char *result = NULL;
        cli_trace("wait", "%s exec=%s awaiting completion (polls=%d)", CLI_ICON_DIAMOND, exec_id,
                  board_polls);
        /* 2.3.7：wait 放后台线程推进引擎，主线程轮询 stdin（插入对话/中断）。
         * 线程创建失败时退化回阻塞 wait（原语义，功能不受影响）。 */
        cli_task_wait_ctx_t wctx;
        __builtin_memset(&wctx, 0, sizeof(wctx));
        wctx.hall = rt.hall;
        wctx.sched_sock = sched_remote ? sched_sock : NULL;
        wctx.exec_id = exec_id;
        wctx.sched_remote = sched_remote;
        airy_thread_t wthr = AIRY_INVALID_THREAD;
        int wait_threaded =
            (airy_platform_thread_create(&wthr, cli_task_wait_worker, &wctx) == 0);
        if (wait_threaded) {
            /* 任务在后台推进；主线程轮询输入直到完成/取消 */
            while (!wctx.done && !g_cli_cancel) {
                int input_rc = cli_task_poll_input();
                if (input_rc < 0)
                    break; /* 用户打断：g_cli_cancel 已置位 */
                airy_sleep_ms(200);
                cli_spinner_tick();
            }
            airy_platform_thread_join(wthr, NULL);
            err = wctx.err;
            result = wctx.result;
        } else if (sched_remote) {
            /* 线程创建失败：回退阻塞 wait（远程） */
            err = cli_dag_wait_remote(sched_sock, exec_id, &result);
        } else {
            err = airy_work_hall_wait(rt.hall, exec_id, 0, &result);
            airy_work_hall_set_blueprint(rt.hall, NULL);
        }
        cli_trace("wait", "%s done err=%d has_result=%d", CLI_ICON_DONE, (int)err,
                  result ? 1 : 0);
        /* 阻塞 wait 结束后（轮询早退的 stale 路径）补一次最终原位重绘：
         * 节点图标翻到终态、footer 汇总完成度。仅当 spinner 仍在运行
         * （几何仍以看板块为参照）时执行；轮询内已完成终态刷新的路径
         * （spin_running=0）跳过，避免二次重绘破坏布局。 */
        if (spin_running && cli_live_board_active() && !g_cli_cancel) {
            if (sched_remote)
                cli_dag_board_snapshot(sched_sock, exec_id, cli_live_board_set_node);
            cli_spinner_pause();
            cli_live_board_refresh((err == AIRY_EOK && result) ? "completed" : "failed",
                                   1.0);
            cli_spinner_resume();
        }
        if (spin_running) {
            if (g_cli_cancel)
                cli_spinner_stop(0, "aborted");
            else if (err == AIRY_EOK && result)
                cli_spinner_stop(1, NULL);
            else
                cli_spinner_stop(0, "no result");
            spin_running = 0;
        }
        /* 结果汇总（2026-08-22 抽离函数）：成败 JSON 判定 + 渲染 +
         * 产物校验失败提示（详见 cli_task_result_render）；返回真实成败
         * 供下方 L2 语义缓存 absorb 决定 SUCCESS / NORMAL_FAIL 指纹。 */
        int task_succeeded =
            cli_task_result_render(result, err, exec_id, g_cli_cancel, rt.hall, vf_before);
        /* L2 semantic cache write-back: register the executed blueprint under the
          * user's original intent, so a repeated or similar task hits L2 (low token)
          * instead of a full L3 replan. Absorb requires PASS + SUCCESS to admit;
          * a failed/canceled run is absorbed as a NORMAL_FAIL fingerprint instead
          * (never cached as a success, avoiding L2 cache poisoning). */
        if (rt.rsched && !g_cli_cancel && err == AIRY_EOK && result && input[0]) {
            airy_rs_absorb_meta_t rmeta;
            __builtin_memset(&rmeta, 0, sizeof(rmeta));
            rmeta.node_id = input;
            rmeta.output_json = result;
            /* 2.7.2：用户整轮输入写入用户意图键空间（is_user_intent），
             * 使重复/相似任务在 process 查询时可命中 L2（此前错误写入
             * 节点 ID 键空间，process 查询过滤后命中率恒为 0）。 */
            rmeta.is_user_intent = true;
            /* 复核 verdict 感知（2026-08-17 缓存污染修复）：DRIFT/REJECT
             * 的执行即使 status=completed 也不得缓存为 PASS——复核否决的
             * 产物入 L2 会作为"已验证成功"的记忆污染后续相似任务。
             * work_hall 已在 wait 返回前把 verdict 记录到 board entry。 */
            {
                const char *rv = rt.hall ? airy_work_hall_entry_verdict(rt.hall, exec_id) : "";
                int review_rejected =
                    (strcmp(rv, "DRIFT") == 0 || strcmp(rv, "REJECT") == 0);
                if (task_succeeded && !review_rejected) {
                    rmeta.result = AIRY_RS_RESULT_SUCCESS;
                    rmeta.verify = AIRY_RS_VERIFY_PASS;
                } else {
                    rmeta.result = AIRY_RS_RESULT_NORMAL_FAIL;
                    rmeta.verify = AIRY_RS_VERIFY_FAIL;
                }
            }
            airy_roadmap_sched_absorb(rt.rsched, NULL, exec_id, &rmeta);
        }
        {
            char metrics[192];
            /* 1.7 任务回合分隔带全链路真实 token/费用（llm_d 会话差值，
             * 覆盖认知规划/GRAD/执行的真实消耗；离线回退 chat 累计） */
            uint64_t toks = 0;
            double cost = 0.0;
            cli_chat_usage_get_session(&toks, &cost);
            if (toks > 0 || cost > 0.0)
                snprintf(metrics, sizeof(metrics),
                         "nodes=%zu deps=%zu · Tokens: %llu · Cost: $%.6f",
                         wf->node_count, wf->edge_count, (unsigned long long)toks, cost);
            else
                snprintf(metrics, sizeof(metrics), "nodes=%zu deps=%zu",
                         wf->node_count, wf->edge_count);
            cli_render_turn_separator(cli_now_ms() - turn_start, metrics);
        }

        if (result)
            AIRY_FREE(result);
        if (exec_id)
            AIRY_FREE(exec_id);
        /* After submit, the engine holds deep copies of wf's fields (decision H fix,
          * 2026-08-09: BORROW semantics; the engine no longer shallow-shares caller fields),
          * so release wf fully here (including nodes/edges/initial_node_id). */
        airy_workflow_free(wf);
        airy_task_plan_free(plan);
    }

    if (g_chat_adapter)
        llm_svc_adapter_destroy(g_chat_adapter);
    /* 运行时上下文统一释放（2026-08-22 收敛进 cli_teardown_runtime）：
     * 面板 → reviewer → work_hall → validator → hall_store → governance
     * → rsched，与 cli_setup_runtime 装配对称，main 不再逐组件释放。 */
    cli_teardown_runtime(&rt);
    airy_loop_destroy(loop);
    /* Leave the full-screen TUI page (restore alt screen + raw mode) before
     * the farewell so it renders in the normal terminal again. The pinned
     * header scroll region only applies to the line-oriented mode. */
    cli_render_set_tui(NULL);
    cli_tui_destroy(tui);

    /* 2026-08-17：/tui 切换——TUI 页面已销毁（终端恢复），用 agentrt-tui
     * 替换当前进程（exec 语义），同一终端由图形前端接管，无进程嵌套。
     * 仅交互模式（非 -p）且请求过切换时执行。切换前先解 pin（全屏滚动）。 */
    if (!g_cli_print_mode && switch_tui_flag) {
        cli_term_header_unpin();
        char tui_bin[AIRY_PATH_MAX];
        snprintf(tui_bin, sizeof(tui_bin), "%s/agentrt-tui", airy_bin_dir());
#ifndef _WIN32
        extern char **environ;
        char gw_url[128] = "";
        const char *gw = getenv("AIRY_GATEWAY_URL");
        if (gw && gw[0])
            snprintf(gw_url, sizeof(gw_url), "%s", gw);
        else
            snprintf(gw_url, sizeof(gw_url), "http://127.0.0.1:8080");
        char *const argv[] = {(char *)tui_bin, "--gateway-url", gw_url, NULL};
        execve(tui_bin, argv, environ);
        /* exec 失败（二进制缺失等）：提示后正常退出，不崩溃。 */
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui",
                             "agentrt-tui 不可用，无法切换。");
        return 0;
#else
        (void)0;
#endif
    }

    if (!g_cli_print_mode) {
        /* 退出横幅先于 unpin 渲染：此时滚动区（含底部输入条）仍激活，
         * 光标已由 submit 送到滚动区末行，横幅画在对话末尾，不会覆盖
         * hero 或输入行。之后再解 pin 交给 shell 接管全屏滚动——
         * 部分终端在 \033[r（重置滚动区）时会同时把光标送回 (1,1)，
         * 若先 unpin 再输出，横幅会画在屏幕顶部破坏 hero。
         * 非 TTY（管道/日志）无底部输入条：先换行再画，避免横幅直接
         * 跟在 "airy> " 提示符后。 */
        if (cli_term_input_active())
            cli_term_input_submit();
        else
            cli_outc('\n');
        cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, NULL,
                             "AgentRT has exited. Thank you for using it.");
    }
    cli_term_header_unpin();
    return 0;
}
