// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file main.c
 * @brief airy_cli - AgentRT interactive product entry.
 *
 * Full closed-loop product flow: natural-language instruction -> GCCP intent
 * confirmation (reasoning + four questions) -> cognition pipeline planning
 * (think_d over gateway) -> Plan -> DAG adaption -> gateway → sched_d submit/
 * board/wait -> agent_d drives real execution.
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
 *
 * 2026-08-27 二轮拆分（893 行 → 3 个职责模块）：
 *   main.c               本文件：入口、全局运行态与主循环骨架
 *   airy_cli_cmdline.c   命令面（CLI_COMMANDS 表 / 分发 / 参数解析）
 *   airy_cli_taskflow.c  任务执行管线（规划 → DAG → 提交 → 轮询 → 等待 → 结果）
 */

#include <airy_types.h> /* 0.1.16 B2: userspace error contract (commons SSoT) */
#include "loop.h"
#include "cli_gw.h"
#include "platform.h"
#include "cognition.h"
#include "gccp.h"
#include "hall_store.h"
#include "plan_to_dag.h"
#include "llm_svc_adapter.h"
#include "logger.h"
#include "logging.h"
#include "airy_memory.h"
#include "string_compat.h"
#include "daemon_rpc_client.h"
#include "daemon_cmds.h"
#include "cli_internal.h"
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
/* 全屏 TUI 渲染层模式（--tui，0.1.17 R5-G6）。 */
int g_cli_tui_mode = 0;
/* 会话恢复视图模式（--continue / --resume，0.1.17 R5-G5）：启动时读一次
 * mem.recent 装配 g_history_*，前端不落任何本地会话状态。 */
int g_cli_resume_mode = 0;

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
char *g_cli_lang_sys_prompt = NULL;
int g_cli_lang_output = 0;

/* 会话开始时刻（TUI 状态栏耗时计算；交互模式才有意义）。 */
static uint64_t g_session_start_ms;

/* 1.3 推理语言 wire 值 → 展示名（lang_gateway.h 枚举：0=未知/1=中文/2=英文）。
 * M1-1c 后 CLI 不再直连 lang_gateway 库，本地保留渲染层映射。 */
static const char *cli_lang_name(int lang)
{
    switch (lang) {
    case 1:
        return "中文";
    case 2:
        return "英文";
    default:
        return "未知";
    }
}

int main(int argc, char *argv[])
{
    const char *print_prompt = NULL;
    if (cli_parse_args(argc, argv, &print_prompt) != 0)
        return 1;
    /* 0.1.17 R5-G6 入口唯一化：--tui 为 CLI 的一个模式——调起 agentrt-tui
     * 子进程并回传退出码；CLI 父进程不初始化终端（保持零终端改性），
     * TUI 异常退出仅报错，不做接力。R5-G5：--resume/--continue 原样转发，
     * 使 `airy_cli --tui --resume` 与 `agentrt-tui --resume` 呈现同一会话
     * （会话权威在 mem_d，两个前端各自只读装配，均不落本地会话状态）。 */
    if (g_cli_tui_mode)
        return cli_run_tui_frontend(g_cli_resume_mode);
    cli_term_init();
#ifndef _WIN32
    cli_term_crash_guard_install(); /* T-19：崩溃前还原终端改性 */
#endif
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

    /* 0.1.16 B2 (design §5.2): the CLI is a pure gateway client — no
     * in-process microkernel (airy_init() withdrawn, 2026-09-14 ruling).
     * Boot evidence is a gateway reachability probe (HTTP /health); the
     * retired "badge=0 degraded" concept no longer applies: offline gateway
     * only means calls will fail until it is up, not a degraded kernel. */
    {
        int gw_up = cli_gw_health(2000);
        /* Boot evidence goes to stderr, not AIRY_LOG_*: main() pins the
         * module level to LOG_LEVEL_ERROR, which would silence INFO/WARN
         * evidence lines entirely. stderr is the CLI's boot diagnostic
         * channel (same as the log-dir fallback above); in TUI mode it is
         * dup2'ed into airy_cli.log, in -p mode it stays on the real
         * stderr. */
        if (gw_up == 1) {
            fprintf(stderr, "[airy_cli] gateway reachable - client mode ready\n");
        } else {
            fprintf(stderr, "[airy_cli] gateway unreachable -"
                            " calls will fail until it is up\n");
        }
    }

    airy_core_loop_t *loop = cli_setup_core_engines();
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

    /* 0.1.17 R5-G5：--continue/--resume 会话恢复视图——启动时一次性只读
     * 装配（mem.recent → g_history_*），此后请求/渲染路径自动同源生效。 */
    if (g_cli_resume_mode)
        cli_session_restore();

    char input[8192];
    int quit_flag = 0;
    int switch_tui_flag = 0;
    cli_cmd_ctx_t cmd_ctx = {.quit = &quit_flag, .switch_tui = &switch_tui_flag};
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
        (void)cli_daemon_lifecycle_reconcile_once();
        /* 0.1.17 R5-G6：/tui 切换到全屏 TUI 渲染层。fork agentrt-tui 子进程
         * （唯一实现 cli_run_tui_frontend），TUI 退出后返回行式对话；不 exec
         * 替换 CLI 进程，故会话上下文与历史保留。 */
        if (switch_tui_flag) {
            switch_tui_flag = 0;
            cli_term_header_unpin();
            (void)cli_run_tui_frontend(0);
            if (cli_tui_active(tui)) {
                cli_tui_pin_header(tui);
                cli_tui_redraw(tui);
            }
            continue;
        }
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
            /* 0.1.17 R5-G2：行式是 CLI 唯一交互形态，readline 只返回 0/1；
             * 全屏切换（原 rl==2 / rl==3）随 C 侧全屏套件冻结退役。 */
            cli_term_input_submit();
            if (input_len > 0 && cli_term_input_on()) {
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

        /* 1.3 推理语言网关服务面化（M1-1c）：CLI 不持 lang_gateway 句柄，
         * 输入标准化经 gateway → think.lang_process（think_d 承载）。
         * 网关不可达/失败时静默降级：语言约束缺失不阻塞对话主流程。 */
#ifdef AIRY_HAS_CJSON
        {
            cJSON *lp = cJSON_CreateObject();
            cJSON *lt = cJSON_CreateString(input);
            if (lp && lt)
                cJSON_AddItemToObject(lp, "text", lt);
            else
                cJSON_Delete(lt);
            char *lp_json = lp ? cJSON_PrintUnformatted(lp) : NULL;
            cJSON_Delete(lp);
            char *lp_res = NULL;
            if (lp_json && cli_gw_call("think.lang_process", lp_json, 6000, &lp_res) == 0 &&
                lp_res) {
                cJSON *lr = cJSON_Parse(lp_res);
                if (lr) {
                    cJSON *sp = cJSON_GetObjectItem(lr, "system_prompt");
                    cJSON *ol = cJSON_GetObjectItem(lr, "output_lang");
                    cJSON *rl = cJSON_GetObjectItem(lr, "reasoning_lang");
                    cJSON *dr = cJSON_GetObjectItem(lr, "decision_reason");
                    if (cJSON_IsString(sp) && sp->valuestring) {
                        AIRY_FREE(g_cli_lang_sys_prompt);
                        g_cli_lang_sys_prompt = AIRY_STRDUP(sp->valuestring);
                    }
                    if (cJSON_IsNumber(ol))
                        g_cli_lang_output = (int)ol->valuedouble;
                    int r_lang = cJSON_IsNumber(rl) ? (int)rl->valuedouble : 0;
                    char lg_line[192];
                    snprintf(lg_line, sizeof(lg_line), "推理语言: %s · 输出语言: %s · %s",
                             cli_lang_name(r_lang), cli_lang_name(g_cli_lang_output),
                             cJSON_IsString(dr) && dr->valuestring ? dr->valuestring : "");
                    cli_render_sub_agent_line(CLI_ROLE_TRACE, "lang", lg_line);
                    cli_trace("lang", "reasoning=%s output=%s", cli_lang_name(r_lang),
                              cli_lang_name(g_cli_lang_output));
                    cJSON_Delete(lr);
                }
                AIRY_FREE(lp_res);
            } else {
                cli_trace("lang", "think.lang_process unavailable (gateway offline)");
                AIRY_FREE(lp_res);
            }
            AIRY_FREE(lp_json);
        }
#endif /* AIRY_HAS_CJSON */

        /* 4.0b Blueprint scheduling three-tier routing（0.1.9 M3：经
         * gateway → sched_d sched.plan RPC，CLI 不再持有本地 roadmap） */
        if (cli_blueprint_fastpath(input, turn_start))
            continue;

        int is_task = cli_classify_input(input);
        cli_trace("intent", "%s", is_task ? "task" : "chat");
        if (is_task == 0) {
            cli_chat_reply(input);
            char chat_metrics[192];
            cli_chat_usage_metrics(chat_metrics, sizeof(chat_metrics));
            cli_render_turn_separator(cli_now_ms() - turn_start,
                                      chat_metrics[0] ? chat_metrics : NULL);
            continue;
        }

        /* === 任务回合：认知规划（gateway → think_d）→ DAG 适配 → 提交 →
         * 轮询 → 等待 → 结果汇总（airy_cli_taskflow.c；返回 1 = 规划/提交
         * 失败，提前继续下一轮） === */
        if (cli_run_task_pipeline(&rt, input, turn_start))
            continue;
    }

    if (g_chat_adapter)
        llm_svc_adapter_destroy(g_chat_adapter);
    cli_teardown_runtime(&rt);
    airy_loop_destroy(loop);
    cli_render_set_tui(NULL);
    cli_tui_destroy(tui);

    if (!g_cli_print_mode) {
        if (cli_term_input_on())
            cli_term_input_submit();
        else
            cli_outc('\n');
        cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, NULL,
                             "AgentRT has exited. Thank you for using it.");
    }
    cli_term_header_unpin();
    return 0;
}
