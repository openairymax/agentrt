// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_taskflow.c
 * @brief 任务执行管线（域拆分自 main.c，2026-08-27）。
 *
 * main 循环的任务回合编排：认知规划（think_d 远程 / 内置引擎 + 并行
 * 子 agent 审查）→ DAG 适配 → 提交（远程 sched_d / 本地工作大厅）→
 * 任务看板轮询 → 等待（后台线程 + stdin 中断轮询）→ 结果渲染与
 * 蓝本吸收。返回 1 = 规划/提交失败需提前 continue，0 = 正常完成。
 * 声明见 airy_cli_exec.h。
 */

#include "cli_internal.h"
#include "cli_review.h"
#include "cli_gw.h"
#include "airy_cli_exec.h"

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 0.1.9 M3（roadmap CLI 切断）：蓝图注册/执行结果回灌改经 gateway →
 * sched_d sched.absorb RPC（L2 语义缓存由 sched_d 唯一持有）。网关
 * 不可达时静默跳过（与 roadmap_sched create 失败同款降级）。 */
#define CLI_ROADMAP_ABSORB_TIMEOUT_MS 6000

#ifdef AIRY_HAS_CJSON
/* 序列化 airy_task_plan_t → JSON（字段对齐 sched_d roadmap_plan_parse） */
static char *cli_plan_to_json(const airy_task_plan_t *plan)
{
    if (!plan)
        return NULL;
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;
    if (plan->task_plan_id && plan->task_plan_id[0])
        cJSON_AddStringToObject(root, "task_plan_id", plan->task_plan_id);
    cJSON *nodes = cJSON_CreateArray();
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        const airy_task_node_t *nd = plan->task_plan_nodes ? plan->task_plan_nodes[i] : NULL;
        if (!nd)
            continue;
        cJSON *nj = cJSON_CreateObject();
        if (nd->task_node_id)
            cJSON_AddStringToObject(nj, "id", nd->task_node_id);
        if (nd->task_node_goal)
            cJSON_AddStringToObject(nj, "goal", nd->task_node_goal);
        if (nd->task_node_handler_name)
            cJSON_AddStringToObject(nj, "handler", nd->task_node_handler_name);
        if (nd->task_node_agent_role)
            cJSON_AddStringToObject(nj, "role", nd->task_node_agent_role);
        if (nd->task_node_depends_count > 0 && nd->task_node_depends_on) {
            cJSON *deps = cJSON_CreateArray();
            for (size_t d = 0; d < nd->task_node_depends_count; d++)
                if (nd->task_node_depends_on[d])
                    cJSON_AddItemToArray(deps, cJSON_CreateString(nd->task_node_depends_on[d]));
            cJSON_AddItemToObject(nj, "depends", deps);
        }
        cJSON_AddItemToArray(nodes, nj);
    }
    cJSON_AddItemToObject(root, "nodes", nodes);
    if (plan->task_plan_entry_count > 0 && plan->task_plan_entry_points) {
        cJSON *entries = cJSON_CreateArray();
        for (size_t i = 0; i < plan->task_plan_entry_count; i++)
            if (plan->task_plan_entry_points[i])
                cJSON_AddItemToArray(entries, cJSON_CreateString(plan->task_plan_entry_points[i]));
        cJSON_AddItemToObject(root, "entry_points", entries);
    }
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

/* 蓝图注册吸收：sched.absorb 模式 A（plan 对象 JSON） */
static void cli_roadmap_absorb_plan(const airy_task_plan_t *plan)
{
    if (!plan)
        return;
    char *plan_json = cli_plan_to_json(plan);
    if (!plan_json)
        return;
    cJSON *params = cJSON_CreateObject();
    cJSON *pv = cJSON_Parse(plan_json);
    if (params && pv)
        cJSON_AddItemToObject(params, "plan", pv);
    else
        cJSON_Delete(pv);
    char *pj = params ? cJSON_PrintUnformatted(params) : NULL;
    cJSON_Delete(params);
    char *resp = NULL;
    if (pj)
        (void)cli_gw_call("sched.absorb", pj, CLI_ROADMAP_ABSORB_TIMEOUT_MS, &resp);
    AIRY_FREE(pj);
    AIRY_FREE(resp);
    AIRY_FREE(plan_json);
}

/* 执行结果回灌：sched.absorb 模式 B（exec_id + node_id + output_json +
 * result/verify），is_user_intent 固定置真（CLI 用户回合）。 */
static void cli_roadmap_absorb_result(const char *exec_id, const char *node_id,
                                      const char *output_json, int result, int verify)
{
    if (!node_id || !node_id[0])
        return;
    cJSON *params = cJSON_CreateObject();
    if (!params)
        return;
    if (exec_id && exec_id[0])
        cJSON_AddStringToObject(params, "exec_id", exec_id);
    cJSON_AddStringToObject(params, "node_id", node_id);
    if (output_json && output_json[0])
        cJSON_AddStringToObject(params, "output_json", output_json);
    cJSON_AddNumberToObject(params, "result", result);
    cJSON_AddNumberToObject(params, "verify", verify);
    cJSON_AddTrueToObject(params, "is_user_intent");
    char *pj = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    char *resp = NULL;
    if (pj)
        (void)cli_gw_call("sched.absorb", pj, CLI_ROADMAP_ABSORB_TIMEOUT_MS, &resp);
    AIRY_FREE(pj);
    AIRY_FREE(resp);
}
#endif /* AIRY_HAS_CJSON */

int cli_run_task_pipeline(cli_runtime_ctx_t *rt, airy_cognition_engine_t *cog,
                          const char *input, size_t input_len, uint64_t turn_start)
{
    airy_err_t err = AIRY_EOK;

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
            return 1;
        }
        cli_spinner_stop(1, NULL);
    }
#ifdef AIRY_HAS_CJSON
    /* 0.1.9 M3：蓝图注册回灌 sched_d（sched.absorb 模式 A），L2 语义
     * 缓存由 sched_d 唯一持有；网关不可达静默跳过。 */
    cli_roadmap_absorb_plan(plan);
#endif
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
        return 1;
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
        airy_work_hall_set_blueprint(rt->hall, plan);
    if (sched_remote) {
        err = cli_dag_submit_remote(sched_sock, wf, input, rt->main_workspace_dir, &exec_id);
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
        err = airy_work_hall_submit(rt->hall, wf, input, &exec_id);
        if (err != AIRY_EOK || !exec_id) {
            char line[128];
            snprintf(line, sizeof(line), "任务提交失败：%s", cli_err_desc((int)err));
            cli_render_sub_agent_line(CLI_ROLE_ERROR, "hall", line);
            airy_work_hall_set_blueprint(rt->hall, NULL);
            airy_workflow_free(wf);
            airy_task_plan_free(plan);
            return 1;
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
                if (cli_board_active())
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
            airy_err_t st_err = airy_work_hall_status(rt->hall, exec_id, &entry);
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
    airy_work_hall_verify_stats(rt->hall, NULL, &vf_before, NULL);
    char *result = NULL;
    cli_trace("wait", "%s exec=%s awaiting completion (polls=%d)", CLI_ICON_DIAMOND, exec_id,
              board_polls);
    cli_task_wait_ctx_t wctx;
    __builtin_memset(&wctx, 0, sizeof(wctx));
    wctx.hall = rt->hall;
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
        err = airy_work_hall_wait(rt->hall, exec_id, 0, &result);
        airy_work_hall_set_blueprint(rt->hall, NULL);
    }
    cli_trace("wait", "%s done err=%d has_result=%d", CLI_ICON_DONE, (int)err,
              result ? 1 : 0);
    if (spin_running && cli_board_active() && !g_cli_cancel) {
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
        cli_task_result_render(result, err, exec_id, g_cli_cancel, rt->hall, vf_before);
#ifdef AIRY_HAS_CJSON
    if (!g_cli_cancel && err == AIRY_EOK && result && input[0]) {
        const char *rv = rt->hall ? airy_work_hall_entry_verdict(rt->hall, exec_id) : "";
        int review_rejected = (strcmp(rv, "DRIFT") == 0 || strcmp(rv, "REJECT") == 0);
        int rs_result = (task_succeeded && !review_rejected) ? 0 : 1; /* SUCCESS / NORMAL_FAIL */
        int rs_verify = (task_succeeded && !review_rejected) ? 0 : 1; /* PASS / FAIL */
        /* 0.1.9 M3：执行结果回灌 sched_d（sched.absorb 模式 B） */
        cli_roadmap_absorb_result(exec_id, input, result, rs_result, rs_verify);
    }
#endif
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
    return 0;
}
