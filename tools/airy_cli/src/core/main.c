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
 * strategy), agentrt is the mechanism layer.  Degrades gracefully when the
 * llm_d/agent_d daemons are not running (heuristic confirmation, agent
 * unavailable).
 *
 * Split layout (2026-08-27):
 *   main.c             — entry, arg parsing, command dispatch, main loop
 *   airy_cli_pipeline.c — runtime context assembly/teardown, blueprint fastpath
 *   airy_cli_exec.c     — task wait worker, stdin poll, result rendering
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
#include "cli_review.h"
#include "cli_exec_review.h"
#include "airy_cli_pipeline.h"
#include "airy_cli_exec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

llm_svc_adapter_t *g_chat_adapter = NULL;
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
    {"/apikey", "配置模型 API Key：/apikey list | set <N> <key>（N=model.yaml 行号）", CLI_CAT_SECURITY, 0, cmd_apikey},
    {"/mem", "记忆链：/mem（最近记忆） /mem <query>（检索） /mem get <id>（详情）", CLI_CAT_RESOURCE, 0, cmd_mem},
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

int main(int argc, char *argv[])
{
    const char *print_prompt = NULL;
    if (cli_parse_args(argc, argv, &print_prompt) != 0)
        return 1;
    cli_term_init();
    cli_theme_init();
    cli_term_title("AgentRT · airy_cli");

    (void)airy_paths_init();

    log_set_module_level("*", LOG_LEVEL_ERROR);

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

    cli_tui_t *tui = NULL;
    if (!g_cli_print_mode) {
        cli_tui_create(&tui);
    }

#ifndef _WIN32
    if (!g_cli_print_mode || isatty(STDERR_FILENO)) {
        char logpath[512];
        const char *logdir = airy_log_dir();
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

    char m_s2[128], m_verify[128], m_expert[128];
    cli_think_cfg_load(m_s2, sizeof(m_s2), m_verify, sizeof(m_verify),
                       m_expert, sizeof(m_expert));
    cli_tui_set_header_models(tui, m_s2[0] ? m_s2 : NULL,
                              m_verify[0] ? m_verify : NULL,
                              m_expert[0] ? m_expert : NULL);

    cli_print_system_header(m_s2[0] ? m_s2 : NULL,
                            m_verify[0] ? m_verify : NULL,
                            m_expert[0] ? m_expert : NULL);
    if (g_cli_print_mode) {
        (void)0;
    } else if (cli_tui_active(tui)) {
        cli_tui_pin_header(tui);
    }

    airy_cognition_engine_t *cog = NULL;
    airy_core_loop_t *loop = cli_setup_core_engines(m_s2, m_verify, m_expert, &cog);
    if (!loop)
        return 1;

    airy_err_t err = AIRY_EOK;
    cli_runtime_ctx_t rt;
    err = cli_setup_runtime(loop, tui, &rt);
    if (err != AIRY_EOK) {
        airy_loop_destroy(loop);
        return 1;
    }

    err = airy_loop_dag_set_cancel_flag(loop, &g_cli_cancel);
    if (err != AIRY_EOK)
        AIRY_LOG_WARN("airy_cli: set cancel flag failed (err=%d)", (int)err);

    char input[8192];
    int quit_flag = 0;
    int switch_tui_flag = 0;
    cli_cmd_ctx_t cmd_ctx = {.hall = rt.hall, .quit = &quit_flag, .switch_tui = &switch_tui_flag};
    int print_consumed = 0;

    {
        const char *e_sh = getenv("AIRY_SELF_HEAL");
        const char *e_sh_agents = getenv("AIRY_SELF_HEAL_AGENTS");
        if ((e_sh && e_sh[0] && strcmp(e_sh, "0") != 0) || (e_sh_agents && e_sh_agents[0]))
            cli_daemon_lifecycle_init(e_sh_agents);
    }

    for (;;) {
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
        (void)airy_work_hall_redispatch_once(rt.hall);
        (void)airy_work_hall_ttl_purge(rt.hall);
        (void)cli_daemon_lifecycle_reconcile_once();
        size_t input_len = 0;
        if (g_cli_print_mode) {
            if (print_prompt && print_prompt[0]) {
                if (print_consumed)
                    break;
                print_consumed = 1;
                AIRY_STRNCPY_TERM(input, print_prompt, sizeof(input));
                input_len = strlen(input);
            } else {
                if (!fgets(input, sizeof(input), stdin))
                    break;
                input_len = strlen(input);
                while (input_len > 0 &&
                       (input[input_len - 1] == '\n' || input[input_len - 1] == '\r'))
                    input[--input_len] = '\0';
                if (input_len == 0)
                    continue;
                {
                    size_t nz = 0;
                    while (nz < input_len && (input[nz] == ' ' || input[nz] == '\t'))
                        nz++;
                    if (nz == input_len)
                        continue;
                }
            }
            if (cli_dispatch_command(input, &cmd_ctx)) {
                if (quit_flag)
                    break;
                continue;
            }
        } else {
            if (!cli_tui_active(tui)) {
                if (!cli_term_input_begin()) {
                    if (!cli_term_is_tty())
                        cli_outf("\n\n%sairy>%s ", cli_c(CLR_CYAN),
                                 cli_c(CLR_RESET));
                }
                fflush(stdout);
            }
            int rl = cli_tui_readline(tui, input, sizeof(input), &input_len);
            if (rl == 0) {
                cli_term_input_submit();
                break;
            }
            if (rl == 2) {
                cli_tui_enter(tui);
                if (cli_tui_active(tui)) {
                    cli_tui_reset_history(tui);
                    cli_render_set_tui(tui);
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
                cli_tui_leave(tui);
                cli_render_set_tui(NULL);
                cli_tui_rebuild_three_zone(tui);
                continue;
            }
            cli_term_input_submit();
            if (input_len > 0 && cli_term_input_active()) {
                cli_term_input_begin();
                cli_outf("%sairy>%s", cli_c(CLR_DIM), cli_c(CLR_RESET));
                cli_term_input_hop();
            }
            if (input_len == 0)
                continue;
            {
                size_t nz = 0;
                while (nz < input_len && (input[nz] == ' ' || input[nz] == '\t'))
                    nz++;
                if (nz == input_len)
                    continue;
            }
            if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0)
                break;
            if (input[0] == '?' || input[0] == '\xef') {
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

        /* 1.3 推理语言网关：输入标准化 */
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

        /* 4.0b Blueprint scheduling three-tier routing */
        if (cli_blueprint_fastpath(rt.rsched, input, turn_start))
            continue;

        int is_task = cli_classify_input(input);
        cli_trace("intent", "%s", is_task ? "task" : "chat");
        if (is_task == 0) {
            cli_chat_reply(input);
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

        /* === 认知规划 → 提交 → 轮询 → 等待 → 结果汇总 === */
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
        if (rt.rsched)
            airy_roadmap_sched_absorb(rt.rsched, plan, NULL, NULL);
        cli_trace("plan", "plan_id=%s nodes=%zu entry=%zu",
                  plan->task_plan_id ? plan->task_plan_id : "?",
                  plan->task_plan_node_count, plan->task_plan_entry_count);
        if (g_cli_hall_store && plan && plan->task_plan_id) {
            char ev[256];
            snprintf(ev, sizeof(ev), "{\"plan_id\":\"%s\",\"nodes\":%zu,\"entry\":%zu}",
                     plan->task_plan_id, plan->task_plan_node_count, plan->task_plan_entry_count);
            airy_hall_store_write(g_cli_hall_store, "default", "preflight", NULL,
                                  AIRY_HALL_CAT_BLUEPRINT, "cognition", ev, NULL, 0);
        }

        /* 认知阶段并行子 agent 审查 */
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

        cli_live_board_begin(wf);
        char *exec_id = NULL;
        const char *sched_sock = getenv("AIRY_SCHED_SOCK");
        int sched_remote = (sched_sock && sched_sock[0]) ? 1 : 0;
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
                airy_work_hall_set_blueprint(rt.hall, NULL);
                airy_workflow_free(wf);
                airy_task_plan_free(plan);
                continue;
            }
            cli_trace("submit", "%s exec=%s", CLI_ICON_DIAMOND, exec_id);
            cli_chain_record_submit(exec_id, plan, wf);
        }

        /* 4.4 Board polling */
        {
            char run_title[128];
            snprintf(run_title, sizeof(run_title), "Running (%s)",
                     sched_remote ? "sched_d" : "hall");
            cli_spinner_start(run_title);
        }
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
            int input_rc = cli_task_poll_input();
            if (input_rc == 1) {
                cli_live_board_done();
            }
            if (input_rc < 0) {
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
        if (node_board) {
            cli_dag_node_board_destroy(node_board);
            node_board = NULL;
        }
        if (spin_running) {
            if (board_polls > 0 && stale_polls >= 10) {
                cli_spinner_pause();
                cli_live_board_extra();
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
            while (!wctx.done && !g_cli_cancel) {
                int input_rc = cli_task_poll_input();
                if (input_rc == 1) {
                    cli_live_board_done();
                }
                if (input_rc < 0)
                    break;
                airy_sleep_ms(200);
                cli_spinner_tick();
            }
            airy_platform_thread_join(wthr, NULL);
            err = wctx.err;
            result = wctx.result;
        } else if (sched_remote) {
            err = cli_dag_wait_remote(sched_sock, exec_id, &result);
        } else {
            err = airy_work_hall_wait(rt.hall, exec_id, 0, &result);
            airy_work_hall_set_blueprint(rt.hall, NULL);
        }
        cli_trace("wait", "%s done err=%d has_result=%d", CLI_ICON_DONE, (int)err,
                  result ? 1 : 0);
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
        int task_succeeded =
            cli_task_result_render(result, err, exec_id, g_cli_cancel, rt.hall, vf_before);
        if (rt.rsched && !g_cli_cancel && err == AIRY_EOK && result && input[0]) {
            airy_rs_absorb_meta_t rmeta;
            __builtin_memset(&rmeta, 0, sizeof(rmeta));
            rmeta.node_id = input;
            rmeta.output_json = result;
            rmeta.is_user_intent = true;
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
        airy_workflow_free(wf);
        airy_task_plan_free(plan);
    }

    if (g_chat_adapter)
        llm_svc_adapter_destroy(g_chat_adapter);
    cli_teardown_runtime(&rt);
    airy_loop_destroy(loop);
    cli_render_set_tui(NULL);
    cli_tui_destroy(tui);

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
        char *const argv[] = {(char *)tui_bin, "--gateway-url", gw_url, "--resume", NULL};
        execve(tui_bin, argv, environ);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui",
                             "agentrt-tui 不可用，无法切换。");
        return 0;
#else
        (void)0;
#endif
    }

    if (!g_cli_print_mode) {
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
