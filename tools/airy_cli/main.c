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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* Task-set cancellation flag: set by the SIGINT handler; run_to_completion checks
  * it each round (the engine holds this pointer); the current node finishes, then aborts.
  * Reset to 0 before each new task. */
volatile sig_atomic_t g_cli_cancel = 0;

#if !defined(_WIN32)
static void cli_sigint_handler(int sig)
{
    (void)sig;
    g_cli_cancel = 1;
}
#endif

llm_svc_adapter_t *g_chat_adapter = NULL;

const cli_command_t CLI_COMMANDS[] = {
    {"/help", "显示所有命令", 0, cmd_help},
    {"/clear", "清屏并清空对话上下文", 0, cmd_clear},
    {"/status", "查看执行大厅状态", 0, cmd_status},
    {"/quit", "退出 agentrt", 0, cmd_quit},
    {"/daemons", "查看全部 daemon 在线状态", 0, cmd_daemons},
    {"/rpc", "直接调用 daemon 方法：/rpc <ns>.<method> [json]", 1, cmd_rpc},
    {"/stats", "查看 daemon 统计：/stats [ns]", 0, cmd_stats},
    {"/agents", "列出已注册智能体", 0, cmd_agents},
    {"/tools", "列出可用工具", 0, cmd_tools},
    {"/hooks", "列出事件钩子", 0, cmd_hooks},
    {"/plugins", "列出插件", 0, cmd_plugins},
    {"/channels", "列出消息通道", 0, cmd_channels},
    {"/market", "搜索市场（/market skill 搜技能）", 0, cmd_market},
    {"/models", "列出 LLM 模型", 0, cmd_models},
    {"/mem", "记忆检索：/mem [query]", 1, cmd_mem},
    {"/a2a", "发现 A2A 智能体", 0, cmd_a2a},
    {"/metrics", "查询观测指标", 0, cmd_metrics},
    {"/alerts", "查看监控告警", 0, cmd_alerts},
    {"/tasks", "调度状态与检查点", 0, cmd_tasks},
    {"/info", "系统信息", 0, cmd_info},
    {"/notify", "发布通知：/notify <channel> <msg>", 1, cmd_notify},
    {"/vault", "凭据保险库：/vault list", 1, cmd_vault},
    {"/perm", "权限裁决：/perm <agent> <action> <resource>", 1, cmd_perm},
    {"/sanitize", "输入净化：/sanitize <input>", 1, cmd_sanitize},
    {"/security", "安全状态（网络规则统计）", 0, cmd_security},
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
                    printf("  %s%s%s 需要参数。%s\n", CLR_YELLOW, cmd->name, CLR_RESET, cmd->desc);
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

    printf("  %s未知命令%s %s，输入 %s/help%s 查看可用命令。\n", CLR_YELLOW, CLR_RESET, input,
           CLR_CYAN, CLR_RESET);
    return 1;
}

int main(void)
{
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

    cli_print_banner();

    airy_core_loop_t *loop = NULL;
    airy_err_t err = airy_loop_create(NULL, &loop);
    if (err != AIRY_EOK || !loop) {
        AIRY_LOG_ERROR("airy_cli: loop create failed (err=%d)", (int)err);
        return 1;
    }

    airy_cognition_engine_t *cog = NULL;
    airy_loop_get_engines(loop, &cog, NULL, NULL);
    if (cog) {
        airy_cognition_set_gccp_interact(cog, cli_gccp_interact, NULL);
        AIRY_LOG_INFO("airy_cli: GCCP interaction callback attached");

        /* Dual-thinking three-model injection (GRAD separation of powers; user-chosen models):
          *   AIRY_MODEL_T2    -> model A (generator)
          *   AIRY_MODEL_T1F   -> model B (context arbiter)
          *   AIRY_MODEL_T1P   -> model C (logic verifier)
          * Unset variables use the provider default model (backward compatible). */
        const char *m_s2 = getenv("AIRY_MODEL_T2");
        const char *m_verify = getenv("AIRY_MODEL_T1F");
        const char *m_expert = getenv("AIRY_MODEL_T1P");
        airy_cognition_set_tc3_models(cog, m_s2, m_verify, m_expert);
        if (m_s2 || m_verify || m_expert) {
            AIRY_LOG_INFO("airy_cli: TC3 models injected (s2=%s verify=%s expert=%s)",
                          m_s2 ? m_s2 : "(default)", m_verify ? m_verify : "(default)",
                          m_expert ? m_expert : "(default)");
        }

        /* Decision B (2026-08-09): three model config points - t2=A (generator, cloud-first),
          * t1-f=B (arbiter/daily chat, first to activate, local-first), t1-p=C (verifier);
          * each may use cloud APIs or local endpoints (Ollama/vLLM); the user decides. */
        printf("  %s[模型配置]%s t2(A)=%s | t1-f(B)=%s | t1-p(C)=%s\n", CLR_GREEN, CLR_RESET,
               m_s2 ? m_s2 : "(未配置，llm_d 默认)", m_verify ? m_verify : "(未配置，llm_d 默认)",
               m_expert ? m_expert : "(未配置，llm_d 默认)");
        printf("    %s提示：%s 每点均可指向云端 API 或本地（Ollama/vLLM）；"
               "激活顺序 t1-f（B）最先（日常对话/意图分流）。\n",
               CLR_YELLOW, CLR_RESET);

        airy_cognition_set_grad_enabled(cog, 1);
    }

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

    /* Decision G (2026-08-09): validation gate - inject an artifact validator.
      * Rules come from AIRY_VALIDATOR_RULES (JSON); default exit_code=0;
      * node-level gates live in sched_d write_back; the CLI marks FAIL after wait. */
    airy_artifact_validator_t *cli_validator = NULL;
    const char *rules_json = getenv("AIRY_VALIDATOR_RULES");
    if (!rules_json || !rules_json[0])
        rules_json = "{\"exit_code\":0}";
    airy_err_t vrc = airy_artifact_validator_from_json(&cli_validator, rules_json);
    if (vrc != AIRY_SUCCESS) {
        AIRY_LOG_WARN("airy_cli: output_validator create failed (err=%d), gate disabled", (int)vrc);
        cli_validator = NULL;
    }
    airy_work_hall_config_t wh_cfg;
    __builtin_memset(&wh_cfg, 0, sizeof(wh_cfg));
    wh_cfg.progress_cb = cli_progress_cb;
    wh_cfg.roadmap_sched = rsched;
    wh_cfg.output_validator = cli_validator;
    /* Decision C (2026-08-09): task file model - full-visibility storage ($AIRY_HOME/data/agentrt/hall).
      * Progress/result/issue events go to the hall store for replay and experience mining. */
    airy_hall_store_t *hall_store = airy_hall_store_create(NULL);
    if (!hall_store)
        AIRY_LOG_WARN("airy_cli: hall store create failed, full visibility disabled");
    wh_cfg.hall_store = hall_store;
    /* Decision E (2026-08-09): workspace isolation of the main workspace path.
      * Enabled when AIRY_WORKSPACE_MAIN_DIR is set (snapshot the main workspace -> a sandbox
      * -> merge artifacts back); otherwise unchanged (the executor touches the main workspace).
      * AIRY_WORKSPACE_ISOLATION=0 disables isolation further (snapshot/merge return ENOTSUP). */
    {
        const char *ws_main = getenv("AIRY_WORKSPACE_MAIN_DIR");
        if (ws_main && ws_main[0])
            wh_cfg.main_workspace_dir = ws_main;
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
    airy_work_hall_t *hall = NULL;
    err = airy_work_hall_create(&wh_cfg, loop, &hall);
    if (err != AIRY_EOK || !hall) {
        AIRY_LOG_ERROR("airy_cli: work hall create failed (err=%d)", (int)err);
        if (rsched)
            airy_roadmap_sched_destroy(rsched);
        airy_loop_destroy(loop);
        return 1;
    }
    airy_work_hall_bind_ops(hall);

    /* Task-set cancellation: the engine holds g_cli_cancel; after SIGINT,
      * run_to_completion polls and aborts (after the current node finishes) */
    err = airy_loop_dag_set_cancel_flag(loop, &g_cli_cancel);
    if (err != AIRY_EOK)
        AIRY_LOG_WARN("airy_cli: set cancel flag failed (err=%d)", (int)err);

    llm_svc_adapter_config_t chat_cfg;
    __builtin_memset(&chat_cfg, 0, sizeof(chat_cfg));
    chat_cfg.llm_d_service_name = "llm_d";
    chat_cfg.channel_name = "coreloopthree-llm";
    g_chat_adapter = llm_svc_adapter_create(&chat_cfg);
    if (!g_chat_adapter)
        AIRY_LOG_WARN("airy_cli: chat adapter create failed, "
                      "falling back to task-only mode");

    char input[8192];
    int quit_flag = 0;
    cli_cmd_ctx_t cmd_ctx = {.hall = hall, .quit = &quit_flag};
    for (;;) {
        printf("\n%sairy>%s ", CLR_CYAN, CLR_RESET);
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin))
            break;
        input[strcspn(input, "\r\n")] = '\0';
        if (input[0] == '\0')
            continue;
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0)
            break;

        size_t input_len = strlen(input);

        if (cli_dispatch_command(input, &cmd_ctx)) {
            if (quit_flag)
                break;
            continue;
        }

        printf("  %s[你]%s %s\n", CLR_CYAN, CLR_RESET, input);

        int is_task = cli_classify_input(input);
        if (is_task == 0) {
            cli_chat_reply(input);
            continue;
        }

        /* 4.0b Blueprint scheduling three-tier routing (wired before the cognition entry).
          * L1 state-machine hit (zero tokens) -> next step; L2 semantic-cache hit (few tokens) ->
          * a suggestion; neither enters the LLM pipeline. Only MISS_L3 runs the five-phase pipeline.
          * Absorbed L3 blueprints register the state machine; later "continue/next" hits L1. */
        if (rsched) {
            char *rs_out = NULL;
            airy_rs_dispatch_t rs_disp = AIRY_RS_DISPATCH_MISS_L3;
            airy_err_t rs_err = airy_roadmap_sched_process(rsched, input, NULL, &rs_out, &rs_disp);
            if (rs_err == AIRY_EOK && rs_disp == AIRY_RS_DISPATCH_HIT_L1) {
                printf("  %s[蓝图]%s L1 状态机命中（零 Token）：%s%s%s\n", CLR_GREEN, CLR_RESET,
                       CLR_YELLOW, rs_out ? rs_out : "{}", CLR_RESET);
                AIRY_FREE(rs_out);
                continue;
            }
            if (rs_err == AIRY_EOK && rs_disp == AIRY_RS_DISPATCH_HIT_L2) {
                printf("  %s[蓝图]%s L2 语义缓存命中（低 Token）：%s\n", CLR_GREEN, CLR_RESET,
                       rs_out ? rs_out : "{}");
                AIRY_FREE(rs_out);
                continue;
            }
            AIRY_FREE(rs_out);
        }

        g_cli_cancel = 0;
        airy_task_plan_t *plan = NULL;
        const char *think_sock = getenv("AIRY_THINK_SOCK");
        if (think_sock && think_sock[0]) {

            printf("  %s[认知:think_d]%s 正在远端双思考（%s）...\n", CLR_CYAN, CLR_RESET,
                   think_sock);
            err = cli_think_process_remote(think_sock, input, input_len, &plan);
            if (err != AIRY_EOK || !plan) {
                printf("  %s[认知:think_d]%s 远端思考失败（err=%d），回退内嵌引擎\n", CLR_RED,
                       CLR_RESET, (int)err);
                plan = NULL;
            } else {
                printf("  %s[认知:think_d]%s 远端计划已生成\n", CLR_GREEN, CLR_RESET);
            }
        }
        if (!plan) {
            printf("  %s[认知:内嵌]%s 正在分析任务（LLM 拆解 + 意图确认）...\n", CLR_CYAN,
                   CLR_RESET);
            err = airy_cognition_process(cog, input, input_len, &plan);
            if (err != AIRY_EOK || !plan) {
                printf("  %s[认知:内嵌]%s 规划失败：err=%d\n", CLR_RED, CLR_RESET, (int)err);
                continue;
            }
        }
        /* 4.1.5 Absorb converged blueprints: register the L1 state machine and reset the TTL,
          * so later "continue/next" hits L1 (zero-token progress). Failure only degrades. */
        if (rsched)
            airy_roadmap_sched_absorb(rsched, plan, NULL, NULL);
        printf("  %s[认知]%s 计划已生成：%splan_id=%s%s 节点=%zu 入口=%zu\n", CLR_GREEN, CLR_RESET,
               CLR_YELLOW, plan->task_plan_id ? plan->task_plan_id : "?", CLR_RESET,
               plan->task_plan_node_count, plan->task_plan_entry_count);

        taskflow_workflow_t *wf = NULL;
        err = airy_plan_to_workflow(plan, &wf);
        if (err != AIRY_EOK || !wf) {
            printf("  %s[DAG]%s 工作流适配失败：err=%d\n", CLR_RED, CLR_RESET, (int)err);
            airy_task_plan_free(plan);
            continue;
        }
        printf("  %s[DAG]%s 工作流已适配：id=%s 节点=%zu 边=%zu\n", CLR_GREEN, CLR_RESET, wf->id,
               wf->node_count, wf->edge_count);

        printf("  %s[计划]%s 节点明细：\n", CLR_GREEN, CLR_RESET);
        for (size_t ni = 0; ni < wf->node_count; ni++) {
            const taskflow_node_t *nd = &wf->nodes[ni];
            printf("    %s%-8s%s %s%-14s%s %s\n", CLR_CYAN, nd->id, CLR_RESET, CLR_GREEN,
                   nd->task_handler_name ? nd->task_handler_name : "?", CLR_RESET,
                   nd->name[0] ? nd->name : "");
        }

        /* 4.3 Submit execution (with AIRY_SCHED_SOCK set, use remote sched_d blueprint DAG;
          * otherwise submit to the embedded work hall. input is the raw task text for agents;
          * if omitted, the first node handler gets empty input and produces boilerplate) */
        char *exec_id = NULL;
        const char *sched_sock = getenv("AIRY_SCHED_SOCK");
        int sched_remote = (sched_sock && sched_sock[0]) ? 1 : 0;
        if (sched_remote) {

            err = cli_dag_submit_remote(sched_sock, wf, &exec_id);
            if (err != AIRY_EOK || !exec_id) {
                printf("  %s[大厅:sched_d]%s 远端 DAG 提交失败（err=%d），回退内嵌大厅\n", CLR_RED,
                       CLR_RESET, (int)err);
                AIRY_FREE(exec_id);
                exec_id = NULL;
                sched_remote = 0;
            } else {
                printf("  %s[大厅:sched_d]%s 已提交：dag=%s\n", CLR_GREEN, CLR_RESET, exec_id);
            }
        }
        if (!sched_remote) {
            err = airy_work_hall_submit(hall, wf, input, &exec_id);
            if (err != AIRY_EOK || !exec_id) {
                printf("  %s[大厅]%s 提交失败：err=%d\n", CLR_RED, CLR_RESET, (int)err);
                airy_workflow_free(wf);
                airy_task_plan_free(plan);
                continue;
            }
            printf("  %s[大厅]%s 已提交：exec=%s\n", CLR_GREEN, CLR_RESET, exec_id);
        }

        /* 4.4 Board polling
          * taskflow_engine_start only runs the start node synchronously (progress=1/N);
          * the rest is driven by run_to_completion (dag_wait). Poll here to
          * show live status; like Claude CLI, print a line only when state/progress
          * changes (with a progress bar), avoiding a fixed 200ms spam. If stale for
          * too long (stale_polls past the threshold), break into the 4.5 wait path.
          * Remote sched_d polls dag_status instead of the airy_work_hall_status board. */
        int board_polls = 0;
        int stale_polls = 0;
        int done = 0;
        char last_state[16] = "";
        double last_progress = -1.0;
        for (;;) {
#ifdef _WIN32
            Sleep(200);
#else
            usleep(200 * 1000);
#endif

            if (g_cli_cancel) {
                printf("  %s[取消]%s 已收到中止请求，正在停止任务...\n", CLR_YELLOW, CLR_RESET);
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
                    printf("  %s[看板:sched_d]%s 状态查询失败\n", CLR_RED, CLR_RESET);
                    break;
                }
                if (prc == CLI_DAG_POLL_DONE) {
                    cli_board_line("看板:sched_d", exec_id, cur_state, cur_progress);
                    done = 1;
                    break;
                }
            } else {
                airy_work_hall_entry_t *entry = NULL;
                airy_err_t st_err = airy_work_hall_status(hall, exec_id, &entry);
                if (st_err != AIRY_EOK || !entry) {
                    printf("  %s[看板]%s 状态查询失败\n", CLR_RED, CLR_RESET);
                    break;
                }
                snprintf(cur_state, sizeof(cur_state), "%s", entry->state);
                cur_progress = entry->progress;
                done =
                    (strcmp(entry->state, "completed") == 0 ||
                     strcmp(entry->state, "failed") == 0 || strcmp(entry->state, "canceled") == 0);
                airy_work_hall_entry_free(entry);
            }
            int state_changed = (strcmp(cur_state, last_state) != 0);
            double prog_changed =
                (cur_progress - last_progress) >= 0.01 || (cur_progress - last_progress) <= -0.01;
            if (state_changed || prog_changed) {
                cli_board_line(sched_remote ? "看板:sched_d" : "看板", exec_id, cur_state,
                               cur_progress);
                snprintf(last_state, sizeof(last_state), "%s", cur_state);
                last_progress = cur_progress;
            }
            board_polls++;
            if (!state_changed && !prog_changed) {
                stale_polls++;
            } else {
                stale_polls = 0;
            }
            if (done || board_polls >= 300 || stale_polls >= 10)
                break;
        }
        if (!done && board_polls > 0 && stale_polls >= 10)
            printf("  %s[看板]%s 状态无推进，驱动执行完成...\n", CLR_YELLOW, CLR_RESET);

        uint32_t vf_before = 0, vf_after = 0;
        airy_work_hall_verify_stats(hall, NULL, &vf_before, NULL);
        char *result = NULL;
        if (sched_remote) {
            /* Remote: poll dag_status to the final state (replaces airy_work_hall_wait),
              * aggregating node outputs/errors for display at the final state */
            err = cli_dag_wait_remote(sched_sock, exec_id, &result);
        } else {
            err = airy_work_hall_wait(hall, exec_id, 0, &result);
        }
        if (g_cli_cancel) {
            printf("  %s[取消]%s 任务已中止（当前节点执行完后停止）\n", CLR_YELLOW, CLR_RESET);
        } else if (err == AIRY_EOK && result) {
            cli_print_result(result);
        } else {
            printf("  %s[结果]%s （未获取到结果，err=%d）\n", CLR_YELLOW, CLR_RESET, (int)err);
        }
        /* Decision G: validation gate annotation - mark FAIL clearly when artifacts
          * fail validation, so the user can replan/retry (sched_d owns node-level retries). */
        if (!g_cli_cancel) {
            airy_work_hall_verify_stats(hall, NULL, &vf_after, NULL);
            if (vf_after > vf_before)
                printf("  %s[验证]%s 本次执行产物验证失败——结果已标注，"
                       "可通过重新规划或重试处理\n",
                       CLR_RED, CLR_RESET);
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
    airy_work_hall_destroy(hall);
    if (cli_validator)
        airy_artifact_validator_destroy(cli_validator);
    if (hall_store)
        airy_hall_store_destroy(hall_store);
    if (governance)
        airy_governance_destroy(governance);
    if (rsched)
        airy_roadmap_sched_destroy(rsched);
    airy_loop_destroy(loop);
    printf("\n%sAgentRT 已退出，感谢使用。%s\n", CLR_CYAN, CLR_RESET);
    return 0;
}
