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

/* Runtime color gate: shared cli_c() from cli_render (monochrome under
 * NO_COLOR / piped output). */

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
                    printf("  %s%s%s 需要参数。%s\n", cli_c(CLR_YELLOW), cmd->name,
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

    printf("  %s未知命令%s %s，输入 %s/help%s 查看可用命令。\n", cli_c(CLR_YELLOW),
           cli_c(CLR_RESET), input, cli_c(CLR_CYAN), cli_c(CLR_RESET));
    return 1;
}

int main(void)
{
    /* Terminal capability probe (TTY / color level / NO_COLOR) before any
     * output so every render call degrades consistently on servers and logs. */
    cli_term_init();
    cli_term_title("AgentRT · airy_cli");

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
        cli_print_model_config(m_s2, m_verify, m_expert);
        cli_render_footer_hint();

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
        printf("\n%sairy>%s ", cli_c(CLR_CYAN), cli_c(CLR_RESET));
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

        cli_render_user_message(input);

        uint64_t turn_start = cli_now_ms();

        /* 4.0b Blueprint scheduling three-tier routing (checked before intent
          * classification: transfer commands like "continue/next" are pure
          * blueprint instructions and must short-circuit here, otherwise the
          * LLM classifier may route them into the chat branch and the L1 state
          * machine would never advance).
          *   L1 state-machine hit (zero tokens) -> next step;
          *   L2 semantic-cache hit (few tokens) -> a suggestion;
          *   MISS_L3 -> falls through to the five-phase pipeline. */
        if (rsched) {
            char *rs_out = NULL;
            airy_rs_dispatch_t rs_disp = AIRY_RS_DISPATCH_MISS_L3;
            airy_err_t rs_err = airy_roadmap_sched_process(rsched, input, NULL, &rs_out, &rs_disp);
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
                if (next_buf[0]) {
                    char line[1024];
                    snprintf(line, sizeof(line), "L1 blueprint state machine: advance to step "
                                                 "%s%s%s (zero token)",
                             cli_c(CLR_CYAN), next_buf, cli_c(CLR_RESET));
                    cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_SUPER_THINK, "blueprint",
                                         line);
                } else
#endif /* AIRY_HAS_CJSON */
                {
                    char line[1024];
                    snprintf(line, sizeof(line), "L1 state machine hit (zero token): %s",
                             rs_out ? rs_out : "{}");
                    cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_SUPER_THINK, "blueprint",
                                         line);
                }
                AIRY_FREE(rs_out);
                cli_render_turn_separator(cli_now_ms() - turn_start, NULL);
                continue;
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
                    cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_SUPER_THINK, "blueprint",
                                         "L2 semantic cache hit (low token): replaying last result");
                    cli_render_super_agent(sugg);
                    AIRY_FREE(sugg);
                } else
#endif /* AIRY_HAS_CJSON */
                {
                    char line[1024];
                    snprintf(line, sizeof(line), "L2 semantic cache hit (low token): %s",
                             rs_out ? rs_out : "{}");
                    cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_SUPER_THINK, "blueprint",
                                         line);
                }
                AIRY_FREE(rs_out);
                cli_render_turn_separator(cli_now_ms() - turn_start, NULL);
                continue;
            }
            AIRY_FREE(rs_out);
        }

        int is_task = cli_classify_input(input);
        if (is_task == 0) {
            cli_chat_reply(input);
            cli_render_turn_separator(cli_now_ms() - turn_start, NULL);
            continue;
        }

        g_cli_cancel = 0;
        airy_task_plan_t *plan = NULL;
        const char *think_sock = getenv("AIRY_THINK_SOCK");
        if (think_sock && think_sock[0]) {
            cli_spinner_start("Remote dual-thinking (think_d)");
            err = cli_think_process_remote(think_sock, input, input_len, &plan);
            if (err != AIRY_EOK || !plan) {
                cli_spinner_stop(0, "remote thinking failed");
                cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_THINK, "think_d",
                                     "Remote thinking failed, falling back to the embedded engine.");
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
                char line[128];
                snprintf(line, sizeof(line), "Planning failed: err=%d", (int)err);
                cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_THINK, "cognition", line);
                continue;
            }
            cli_spinner_stop(1, NULL);
        }
        /* 4.1.5 Absorb converged blueprints: register the L1 state machine and reset the TTL,
          * so later "continue/next" hits L1 (zero-token progress). Failure only degrades. */
        if (rsched)
            airy_roadmap_sched_absorb(rsched, plan, NULL, NULL);
        {
            char line[256];
            snprintf(line, sizeof(line), "Plan generated: plan_id=%s nodes=%zu entry=%zu",
                     plan->task_plan_id ? plan->task_plan_id : "?", plan->task_plan_node_count,
                     plan->task_plan_entry_count);
            cli_render_sub_agent_line(CLI_ROLE_TRACE, "cognition", line);
        }

        taskflow_workflow_t *wf = NULL;
        err = airy_plan_to_workflow(plan, &wf);
        if (err != AIRY_EOK || !wf) {
            char line[128];
            snprintf(line, sizeof(line), "Workflow adaption failed: err=%d", (int)err);
            cli_render_sub_agent_line(CLI_ROLE_ERROR, "DAG", line);
            airy_task_plan_free(plan);
            continue;
        }
        {
            char line[256];
            snprintf(line, sizeof(line), "Workflow adapted: id=%s nodes=%zu edges=%zu", wf->id,
                     wf->node_count, wf->edge_count);
            cli_render_sub_agent_line(CLI_ROLE_TRACE, "DAG", line);
        }

        cli_print_plan_list(wf);

        /* 4.3 Submit execution (with AIRY_SCHED_SOCK set, use remote sched_d blueprint DAG;
          * otherwise submit to the embedded work hall. input is the raw task text for agents;
          * if omitted, the first node handler gets empty input and produces boilerplate) */
        char *exec_id = NULL;
        const char *sched_sock = getenv("AIRY_SCHED_SOCK");
        int sched_remote = (sched_sock && sched_sock[0]) ? 1 : 0;
        if (sched_remote) {
            err = cli_dag_submit_remote(sched_sock, wf, input, &exec_id);
            if (err != AIRY_EOK || !exec_id) {
                char line[128];
                snprintf(line, sizeof(line), "Remote DAG submit failed (err=%d), falling back.",
                         (int)err);
                cli_render_sub_agent_line(CLI_ROLE_ERROR, "sched_d", line);
                AIRY_FREE(exec_id);
                exec_id = NULL;
                sched_remote = 0;
            } else {
                char line[256];
                snprintf(line, sizeof(line), "Submitted: dag=%s", exec_id);
                cli_render_sub_agent_line(CLI_ROLE_TRACE, "sched_d", line);
            }
        }
        if (!sched_remote) {
            err = airy_work_hall_submit(hall, wf, input, &exec_id);
            if (err != AIRY_EOK || !exec_id) {
                char line[128];
                snprintf(line, sizeof(line), "Submit failed: err=%d", (int)err);
                cli_render_sub_agent_line(CLI_ROLE_ERROR, "hall", line);
                airy_workflow_free(wf);
                airy_task_plan_free(plan);
                continue;
            }
            {
                char line[256];
                snprintf(line, sizeof(line), "Submitted: exec=%s", exec_id);
                cli_render_sub_agent_line(CLI_ROLE_TRACE, "hall", line);
            }
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
        int board_polls = 0;
        int stale_polls = 0;
        int done = 0;
        int run_failed = 0;
        int spin_running = 1;
        char last_state[16] = "";
        double last_progress = -1.0;
        for (;;) {
#ifdef _WIN32
            Sleep(200);
#else
            usleep(200 * 1000);
#endif
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
                if (prc == CLI_DAG_POLL_DONE) {
                    run_failed = (strcmp(cur_state, "failed") == 0 ||
                                  strcmp(cur_state, "canceled") == 0);
                    cli_spinner_pause();
                    cli_board_line("sched_d", exec_id, cur_state, cur_progress);
                    cli_spinner_stop(!run_failed, NULL);
                    spin_running = 0;
                    done = 1;
                    break;
                }
            } else {
                airy_work_hall_entry_t *entry = NULL;
                airy_err_t st_err = airy_work_hall_status(hall, exec_id, &entry);
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
                cli_board_line(sched_remote ? "sched_d" : "hall", exec_id, cur_state,
                               cur_progress);
                cli_spinner_resume();
                snprintf(last_state, sizeof(last_state), "%s", cur_state);
                last_progress = cur_progress;
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
        if (spin_running) {
            /* Polling exhausted without a terminal state (stale board):
             * leave the status line running and fall into the blocking wait,
             * which drives the engine to the real completion. A single dim
             * trace line, no internal jargon. */
            if (board_polls > 0 && stale_polls >= 10) {
                cli_spinner_pause();
                cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_SUPER_THINK,
                                     sched_remote ? "sched_d" : "hall",
                                     "still running, waiting for completion ...");
                cli_spinner_resume();
            }
        }

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
        if (spin_running) {
            if (g_cli_cancel)
                cli_spinner_stop(0, "aborted");
            else if (err == AIRY_EOK && result)
                cli_spinner_stop(1, NULL);
            else
                cli_spinner_stop(0, "no result");
            spin_running = 0;
        }
        if (g_cli_cancel) {
            cli_render_sub_agent_line(CLI_ROLE_ERROR, "cancel",
                                      "Task aborted (stopped after the current node).");
        } else if (err == AIRY_EOK && result) {
            cli_print_result(result);
        } else {
            char line[128];
            snprintf(line, sizeof(line), "No result (err=%d)", (int)err);
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "result", line);
        }
        /* Decision G: validation gate annotation - mark FAIL clearly when artifacts
          * fail validation, so the user can replan/retry (sched_d owns node-level retries). */
        if (!g_cli_cancel) {
            airy_work_hall_verify_stats(hall, NULL, &vf_after, NULL);
            if (vf_after > vf_before)
                cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_THINK, "validate",
                                     "Artifact validation failed - replan or retry the task.");
        }
        /* L2 semantic cache write-back: register the executed blueprint under the
          * user's original intent, so a repeated or similar task hits L2 (low token)
          * instead of a full L3 replan. Absorb requires PASS + SUCCESS to admit. */
        if (rsched && !g_cli_cancel && err == AIRY_EOK && result && input[0]) {
            airy_rs_absorb_meta_t rmeta;
            __builtin_memset(&rmeta, 0, sizeof(rmeta));
            rmeta.node_id = input;
            rmeta.output_json = result;
            rmeta.result = AIRY_RS_RESULT_SUCCESS;
            rmeta.verify = AIRY_RS_VERIFY_PASS;
            airy_roadmap_sched_absorb(rsched, NULL, exec_id, &rmeta);
        }
        {
            char metrics[128];
            snprintf(metrics, sizeof(metrics), "nodes=%zu deps=%zu", wf->node_count,
                     wf->edge_count);
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
    cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, NULL,
                         "AgentRT has exited. Thank you for using it.");
    return 0;
}
