// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_pipeline.c
 * @brief Runtime context assembly/teardown and blueprint fastpath.
 *
 * Extracted from main.c to keep the entry/main-loop file under control.
 * Owns cli_runtime_ctx_t lifecycle (cli_setup_runtime / cli_teardown_runtime)
 * and the three-tier blueprint routing (L1/L2/L3) that short-circuits
 * repeated tasks before the full cognition pipeline.
 */

#include "airy_cli_pipeline.h"
#include "cli_internal.h"
#include "cli_render.h"
#include "cli_review.h"
#include "cli_exec_review.h"
#include "daemon_cmds.h"

#include "airy_rt.h"
#include "loop.h"
#include "cli_gw.h"
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

/* 0.1.9 M3（roadmap CLI 切断）：sched.plan/absorb RPC 超时（ms）。
 * 网关/调度器不可达时静默按 L3 miss 降级，不阻塞对话主流程。 */
#define CLI_ROADMAP_RPC_TIMEOUT_MS 6000

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 2.3.14 GRAD decision-chain visibility: cognition feedback → [Dual Think]
 * phase lines.  Events (grad_coordinator progress_cb): grad_s2_done /
 * grad_verify_start / grad_verify_done / grad_arbiter_start /
 * grad_arbiter_done / grad_done.
 * -p/--json suppress rendering (script structured output); TUI panel mode
 * delegates to the TUI, but events always go to hall_store chain stream. */
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
            return;
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

airy_core_loop_t *cli_setup_core_engines(const char *m_s2, const char *m_verify,
                                          const char *m_expert,
                                          airy_cognition_engine_t **out_cog)
{
    airy_core_loop_t *loop = NULL;
    airy_err_t err = airy_loop_create(NULL, &loop);
    if (err != AIRY_EOK || !loop) {
        AIRY_LOG_ERROR("airy_cli: loop create failed (err=%d)", (int)err);
        return NULL;
    }

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

        airy_cognition_set_tc3_models(cog, m_s2 && m_s2[0] ? m_s2 : NULL,
                                      m_verify && m_verify[0] ? m_verify : NULL,
                                      m_expert && m_expert[0] ? m_expert : NULL);
        if ((m_s2 && m_s2[0]) || (m_verify && m_verify[0]) || (m_expert && m_expert[0])) {
            AIRY_LOG_INFO("airy_cli: TC3 models injected (s2=%s verify=%s expert=%s)",
                          m_s2 && m_s2[0] ? m_s2 : "(default)",
                          m_verify && m_verify[0] ? m_verify : "(default)",
                          m_expert && m_expert[0] ? m_expert : "(default)");
        }

        airy_cognition_set_grad_enabled(cog, 1);
        airy_cognition_set_feedback(cog, cli_grad_feedback_cb, NULL);
        AIRY_LOG_INFO("airy_cli: GRAD decision-chain feedback attached");
    }
    if (out_cog)
        *out_cog = cog;

    return loop;
}

airy_err_t cli_setup_runtime(airy_core_loop_t *loop, cli_tui_t *tui,
                              cli_runtime_ctx_t *rt)
{
    if (!loop || !rt)
        return AIRY_EINVAL;
    AIRY_MEMSET(rt, 0, sizeof(*rt));

    airy_err_t err = AIRY_EOK;

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
    /* 0.1.9 M3（roadmap CLI 切断）：不注入 roadmap_sched——蓝图调度由
     * sched_d 唯一持有，CLI 经 sched.plan/absorb RPC 交互；work_hall
     * 执行反馈经 cli_roadmap_absorb_result RPC 回灌，不再本地双写
     * l2_semantic_cache.json。 */
    wh_cfg.output_validator = cli_validator;
    wh_cfg.reviewer = cli_exec_review_create();
    rt->reviewer = wh_cfg.reviewer;
    if (wh_cfg.reviewer)
        AIRY_LOG_INFO("airy_cli: execution review pipeline attached (gate -> t2 -> t1-f)");

    airy_hall_store_t *hall_store = airy_hall_store_create(NULL);
    if (!hall_store)
        AIRY_LOG_WARN("airy_cli: hall store create failed, full visibility disabled");
    g_cli_hall_store = hall_store;
    rt->hall_store = hall_store;
    wh_cfg.hall_store = hall_store;
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

    void *board_ud = NULL;
    void *events_ud = NULL;
    void *mem_ud = NULL;
    if (tui) {
        cli_panel_board_create(&board_ud);
        cli_panel_events_create(hall_store, &events_ud);
        cli_panel_mem_create(&mem_ud);
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
        if (mem_ud) {
            cli_tui_set_panel(tui, CLI_TUI_MODE_MEM, mem_ud, cli_panel_mem_count,
                              cli_panel_mem_line);
        }
    }
    rt->board_ud = board_ud;
    rt->events_ud = events_ud;
    rt->mem_ud = mem_ud;

    /* M1-1c：CLI 不再进程内持有 lang_gateway（推理语言网关服务面化至
     * think_d，经 gateway → think.lang_process 调用）。输入标准化与
     * 输出后处理在 main.c / cli_chat_finalize.c 经 cli_gw_call 完成。 */

    return AIRY_EOK;
}

void cli_teardown_runtime(cli_runtime_ctx_t *rt)
{
    if (!rt)
        return;
    if (rt->events_ud)
        cli_panel_events_destroy(rt->events_ud);
    if (rt->board_ud)
        cli_panel_board_destroy(rt->board_ud);
    if (rt->mem_ud)
        cli_panel_mem_destroy(rt->mem_ud);
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
    AIRY_MEMSET(rt, 0, sizeof(*rt));
}

int cli_blueprint_fastpath(const char *input, uint64_t turn_start)
{
    if (!input || !input[0])
        return 0;
    char *rs_out = NULL;
    char tier[8] = "l3";

#ifdef AIRY_HAS_CJSON
    /* 0.1.9 M3（roadmap CLI 切断）：三级路由判定经 gateway → sched_d
     * sched.plan RPC，L2 语义缓存由 sched_d 唯一持有；网关不可达时
     * 静默按 L3 miss 降级（与 think.lang_process 同款降级策略）。 */
    cJSON *prm = cJSON_CreateObject();
    cJSON *pit = cJSON_CreateString(input);
    if (prm && pit)
        cJSON_AddItemToObject(prm, "input", pit);
    else
        cJSON_Delete(pit);
    char *prm_json = prm ? cJSON_PrintUnformatted(prm) : NULL;
    cJSON_Delete(prm);
    char *resp = NULL;
    if (prm_json && cli_gw_call("sched.plan", prm_json, CLI_ROADMAP_RPC_TIMEOUT_MS,
                                &resp) == 0 && resp) {
        cJSON *jr = cJSON_Parse(resp);
        if (jr) {
            cJSON *dt = cJSON_GetObjectItem(jr, "dispatch");
            cJSON *rr = cJSON_GetObjectItem(jr, "result");
            if (cJSON_IsString(dt) && dt->valuestring && dt->valuestring[0])
                AIRY_STRNCPY_TERM(tier, dt->valuestring, sizeof(tier));
            if (cJSON_IsString(rr) && rr->valuestring && rr->valuestring[0])
                rs_out = AIRY_STRDUP(rr->valuestring);
            cJSON_Delete(jr);
        }
        AIRY_FREE(resp);
    }
    AIRY_FREE(prm_json);
#endif

    if (strcmp(tier, "l1") == 0) {
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
#endif
        {
            char line[1024];
            snprintf(line, sizeof(line), "L1 state machine hit (zero token): %s",
                     rs_out ? rs_out : "{}");
            cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_PROF_THINK, "blueprint",
                                 line);
            cli_trace("blueprint", "%s", line);
        }
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
    if (strcmp(tier, "l2") == 0) {
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
#endif
        {
            char line[1024];
            snprintf(line, sizeof(line), "L2 semantic cache hit (low token): %s",
                     rs_out ? rs_out : "{}");
            cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_PROF_THINK, "blueprint",
                                 line);
        }
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
