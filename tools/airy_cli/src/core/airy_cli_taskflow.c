// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_taskflow.c
 * @brief 任务执行管线（域拆分自 main.c，2026-08-27）。
 *
 * main 循环的任务回合编排：认知规划（唯一经 gateway → think_d，无内置
 * 引擎回退，失败即错误可见化）→ 提交（gateway → sched_d，唯一执行通路）
 * → 任务看板轮询 → 等待（后台线程 + stdin 中断轮询）→ 结果渲染与
 * 蓝本吸收。返回 1 = 规划/提交失败需提前 continue，0 = 正常完成。
 * 声明见 airy_cli_exec.h。
 */

#include "cli_internal.h"
#include "cli_review.h"
#include "cli_gw.h"
#include "airy_cli_exec.h"
/* 0.1.16 B2：corekern task.h 已摘除，airy_task_plan_t 来自 coreloopthree
 * cognition.h（经 cli_internal.h/airy_cli_exec.h 传入），错误码由
 * airy_types.h 提供。 */

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
/* 节点依赖数组；无依赖返回 NULL（上层据此省略字段）。 */
static cJSON *pl_deps_json(const airy_task_node_t *nd)
{
    cJSON *deps;
    size_t d;

    if (nd->task_node_depends_count == 0 || !nd->task_node_depends_on)
        return NULL;
    deps = cJSON_CreateArray();
    if (!deps)
        return NULL;
    for (d = 0; d < nd->task_node_depends_count; d++)
        if (nd->task_node_depends_on[d])
            cJSON_AddItemToArray(deps, cJSON_CreateString(nd->task_node_depends_on[d]));
    return deps;
}

/* 单节点序列化：字段名对齐 airy_plan_parse（SSoT：atoms/coreloopthree）。 */
static cJSON *pl_node_json(const airy_task_node_t *nd)
{
    cJSON *nj = cJSON_CreateObject();
    cJSON *deps;

    if (!nj)
        return NULL;
    if (nd->task_node_id)
        cJSON_AddStringToObject(nj, "id", nd->task_node_id);
    if (nd->task_node_goal)
        cJSON_AddStringToObject(nj, "goal", nd->task_node_goal);
    if (nd->task_node_handler_name)
        cJSON_AddStringToObject(nj, "handler", nd->task_node_handler_name);
    if (nd->task_node_agent_role)
        cJSON_AddStringToObject(nj, "role", nd->task_node_agent_role);
    deps = pl_deps_json(nd);
    if (deps)
        cJSON_AddItemToObject(nj, "depends", deps);
    return nj;
}

/* 节点数组：空槽跳过，保证下游节点序与 plan 内序一致。 */
static cJSON *pl_nodes_json(const airy_task_plan_t *plan)
{
    cJSON *nodes = cJSON_CreateArray();
    size_t i;

    if (!nodes)
        return NULL;
    for (i = 0; i < plan->task_plan_node_count; i++) {
        const airy_task_node_t *nd =
            plan->task_plan_nodes ? plan->task_plan_nodes[i] : NULL;
        cJSON *nj;

        if (!nd)
            continue;
        nj = pl_node_json(nd);
        if (nj)
            cJSON_AddItemToArray(nodes, nj);
    }
    return nodes;
}

/* 入口点数组；无入口返回 NULL（上层据此省略字段）。 */
static cJSON *pl_entry_json(const airy_task_plan_t *plan)
{
    cJSON *entries;
    size_t i;

    if (plan->task_plan_entry_count == 0 || !plan->task_plan_entry_points)
        return NULL;
    entries = cJSON_CreateArray();
    if (!entries)
        return NULL;
    for (i = 0; i < plan->task_plan_entry_count; i++)
        if (plan->task_plan_entry_points[i])
            cJSON_AddItemToArray(entries, cJSON_CreateString(plan->task_plan_entry_points[i]));
    return entries;
}

/* 序列化 airy_task_plan_t → JSON（反向序列化，与 airy_plan_parse 成对）。 */
static char *cli_plan_to_json(const airy_task_plan_t *plan)
{
    cJSON *root;
    cJSON *nodes;
    cJSON *entries;
    char *out;

    if (!plan)
        return NULL;
    root = cJSON_CreateObject();
    if (!root)
        return NULL;
    if (plan->task_plan_id && plan->task_plan_id[0])
        cJSON_AddStringToObject(root, "task_plan_id", plan->task_plan_id);
    nodes = pl_nodes_json(plan);
    if (nodes)
        cJSON_AddItemToObject(root, "nodes", nodes);
    entries = pl_entry_json(plan);
    if (entries)
        cJSON_AddItemToObject(root, "entry_points", entries);
    out = cJSON_PrintUnformatted(root);
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

/* 看板轮询态：一次提交的轮询上下文，全部落在调用栈上。 */
typedef struct {
    const char *exec_id;
    cli_dag_board_t *board;
    int polls;
    int stale;
    int live;
    char state[16];
    double prog;
} tp_board_t;

/* 状态面刷新：状态或进度发生可感知变化时打印一行并留痕，返回是否变化。 */
static int tp_board_note(tp_board_t *b, const char *state, double prog)
{
    char sbar[16];
    double delta = prog - b->prog;

    if (strcmp(state, b->state) == 0 && delta < 0.01 && delta > -0.01)
        return 0;
    cli_board_line("sched_d", b->exec_id, state, prog);
    snprintf(b->state, sizeof(b->state), "%s", state);
    b->prog = prog;
    cli_compact_bar(sbar, sizeof(sbar), prog, 8);
    cli_trace("status", "%s sched_d state=%s %s %3.0f%%",
              cli_icon_for_state(state), state, sbar, prog * 100.0);
    return 1;
}

/* 轮询主循环：取消、查询失败、终态三处收口，其余按 200ms 节拍推进。 */
static void tp_board_loop(tp_board_t *b)
{
    for (;;) {
        char state[16] = "";
        double prog = -1.0;
        char *final_result = NULL;
        cli_dag_snapshot_t *snap;
        cli_dag_poll_rc_t prc;

        if (cli_task_poll_input() < 0) {
            b->live = 0;
            cli_print_line("Sub cancel Agent",
                           "Abort requested, stopping the task ...");
            break;
        }
        airy_sleep_ms(200);
        if (g_cli_cancel) {
            b->live = 0;
            cli_print_line("Sub cancel Agent",
                           "Abort requested, stopping the task ...");
            break;
        }
        /* 0.1.17 R4-①: 每轮只拉一次 sched.dag_status，状态面与节点面共用同一
         * 份快照。此前两条路径各发一次 RPC，200ms 节拍下形成短连接洪泛。 */
        snap = cli_dag_snapshot_fetch(b->exec_id);
        if (!snap) {
            b->live = 0;
            cli_print_line("Sub sched_d Agent", "Status query failed.");
            break;
        }
        prc = cli_dag_snapshot_poll(snap, &prog, state, sizeof(state), &final_result);
        AIRY_FREE(final_result);
        if (b->board && cli_dag_node_board_tick(b->board, snap)) {
            cli_dag_node_board_destroy(b->board);
            b->board = NULL;
        }
        cli_dag_snapshot_free(snap);
        if (prc == CLI_DAG_POLL_DONE) {
            /* 终态即停止轮询；semantic_failed（节点进程成功但无产出）与
             * failed / canceled 一并按失败呈现，避免把「无产出」误报为成功。 */
            cli_board_line("sched_d", b->exec_id, state, prog);
            b->live = 0;
            break;
        }
        if (tp_board_note(b, state, prog))
            b->stale = 0;
        else
            b->stale++;
        b->polls++;
        if (b->polls >= 300 || b->stale >= 10)
            break;
    }
}

/* 看板驱动：建板 → 轮询 → 收板，返回本次轮询次数供等待阶段留痕。 */
static int tp_board_run(const char *exec_id)
{
    tp_board_t b;

    AIRY_MEMSET(&b, 0, sizeof(b));
    b.exec_id = exec_id;
    b.prog = -1.0;
    b.live = 1;
    cli_print_status("sched_d", "Running");
    b.board = cli_dag_node_board_create();
    tp_board_loop(&b);
    if (b.board)
        cli_dag_node_board_destroy(b.board);
    if (b.live && b.polls > 0 && b.stale >= 10)
        cli_print_status("Dual Think:sched_d",
                         "still running, waiting for completion ...");
    return b.polls;
}

/* 认知阶段并行子 agent 审查：失败或空报告按降级处理，不阻断主流程。 */
static void tp_review(const char *input, const airy_task_plan_t *plan)
{
    const char *agent_sock_env = getenv("AIRY_AGENT_SOCK");
    const char *plan_id;
    char *review_report = NULL;

    if (cli_cognition_review(agent_sock_env, input, plan, &review_report) <= 0 ||
        !review_report) {
        AIRY_FREE(review_report);
        return;
    }
    cli_trace("review", "parallel cognition review (fact+risk) merged");
    cli_print_status("Sub cognition Agent", "Parallel sub-agent review completed");
    plan_id = (plan && plan->task_plan_id) ? plan->task_plan_id : "";
    cli_hall_emit(plan_id[0] ? plan_id : "preflight", AIRY_HALL_CAT_VERIFY,
                  "{\"plan_id\":\"%s\",\"reviews\":%s}", plan_id, review_report);
    AIRY_FREE(review_report);
}

/* DAG 拓扑留痕：按节点序拼接 handler 串，供排障时核对依赖形状。 */
static void tp_dag_trace(const airy_task_plan_t *plan)
{
    char hdrs[256] = "";
    size_t ho = 0;

    for (size_t ni = 0; ni < plan->task_plan_node_count && ho < sizeof(hdrs) - 2; ni++) {
        char handler[96] = "";

        cli_node_handler(plan->task_plan_nodes ? plan->task_plan_nodes[ni] : NULL,
                         handler, sizeof(handler));
        ho += (size_t)snprintf(hdrs + ho, sizeof(hdrs) - ho, "%s%s",
                               ni > 0 ? "," : "", handler[0] ? handler : "?");
    }
    cli_trace("dag", "plan=%s nodes=%zu deps=%zu [%s]",
              plan->task_plan_id ? plan->task_plan_id : "?",
              plan->task_plan_node_count, cli_plan_deps_count(plan), hdrs);
}

/* 认知规划（唯一经 gateway → think_d）；成功即完成蓝图注册与审查留痕。 */
static int tp_plan(const char *input, airy_task_plan_t **out_plan)
{
    airy_task_plan_t *plan = NULL;
    airy_err_t err;

    *out_plan = NULL;
    cli_print_status("phase", "认知规划");
    cli_print_status("think_d", "Remote dual-thinking (think_d)");
    err = cli_think_process_remote(input, &plan);
    if (err != AIRY_EOK || !plan) {
        char line[128];

        cli_trace("plan", "failed err=%d", (int)err);
        snprintf(line, sizeof(line), "规划失败：%s", cli_err_desc((int)err));
        cli_print_line("Dual Slow Think:认知规划", line);
        return 1;
    }
    cli_print_status("Sub think_d Agent", "Remote plan generated.");
#ifdef AIRY_HAS_CJSON
    /* 0.1.9 M3：蓝图注册回灌 sched_d（sched.absorb 模式 A），L2 语义
     * 缓存由 sched_d 唯一持有；网关不可达静默跳过。 */
    cli_roadmap_absorb_plan(plan);
#endif
    cli_trace("plan", "plan_id=%s nodes=%zu entry=%zu",
              plan->task_plan_id ? plan->task_plan_id : "?",
              plan->task_plan_node_count, plan->task_plan_entry_count);
    if (plan->task_plan_id) {
        cli_hall_emit("preflight", AIRY_HALL_CAT_BLUEPRINT,
                      "{\"plan_id\":\"%s\",\"nodes\":%zu,\"entry\":%zu}",
                      plan->task_plan_id, plan->task_plan_node_count,
                      plan->task_plan_entry_count);
    }
    tp_review(input, plan);
    tp_dag_trace(plan);
    *out_plan = plan;
    return 0;
}

/* 提交唯一通路：plan → gateway → sched_d；失败即错误可见化，不做本地降级。 */
static int tp_submit(const airy_task_plan_t *plan, const char *input, const char *ws,
                     char **out_exec)
{
    airy_err_t err;

    *out_exec = NULL;
    cli_print_plan_list(plan);
    err = cli_dag_submit_remote(plan, input, ws, out_exec);
    if (err != AIRY_EOK || !*out_exec) {
        char line[128];

        snprintf(line, sizeof(line), "任务提交失败：%s", cli_err_desc((int)err));
        cli_print_line("Sub sched_d Agent", line);
        AIRY_FREE(*out_exec);
        *out_exec = NULL;
        return 1;
    }
    cli_trace("submit", "%s dag=%s", CLI_ICON_DIAMOND, *out_exec);
    cli_chain_record_submit(*out_exec, plan);
    return 0;
}

/* 等待收口：优先后台线程 + stdin 中断轮询，线程不可用时回退同步等待。 */
static void tp_wait(const char *exec_id, airy_err_t *out_err, char **out_result)
{
    cli_task_wait_ctx_t wctx;
    airy_thread_t wthr = AIRY_INVALID_THREAD;
    airy_err_t err;
    char *result = NULL;

    AIRY_MEMSET(&wctx, 0, sizeof(wctx));
    wctx.exec_id = exec_id;
    if (airy_thread_create(&wthr, cli_task_wait_worker, &wctx) == 0) {
        while (!wctx.done && !g_cli_cancel) {
            if (cli_task_poll_input() < 0)
                break;
            airy_sleep_ms(200);
        }
        airy_thread_join(wthr, NULL);
        err = wctx.err;
        result = wctx.result;
    } else {
        err = cli_dag_wait_remote(exec_id, &result);
    }
    *out_err = err;
    *out_result = result;
}

/* 回合账：节点/依赖规模叠加模型用量，交渲染层统一出账。 */
static void tp_metrics(const airy_task_plan_t *plan, uint64_t turn_start)
{
    char metrics[192];
    char usage[160];

    cli_chat_usage_metrics(usage, sizeof(usage));
    if (usage[0])
        snprintf(metrics, sizeof(metrics), "nodes=%zu deps=%zu · %s",
                 plan->task_plan_node_count, cli_plan_deps_count(plan), usage);
    else
        snprintf(metrics, sizeof(metrics), "nodes=%zu deps=%zu",
                 plan->task_plan_node_count, cli_plan_deps_count(plan));
    cli_print_turn(cli_now_ms() - turn_start, metrics);
}

/* 结果渲染与回灌：先出结果面，再把执行事实灌回 sched_d（模式 B）。 */
static void tp_report(const airy_task_plan_t *plan, const char *input, const char *exec_id,
                      const char *result, airy_err_t err, uint64_t turn_start)
{
    int succeeded = cli_task_result_render(result, err, exec_id, g_cli_cancel);

#ifdef AIRY_HAS_CJSON
    if (!g_cli_cancel && err == AIRY_EOK && result && input[0]) {
        int rs = succeeded ? 0 : 1; /* SUCCESS / NORMAL_FAIL（校验门随 hall 退役） */
        /* 0.1.9 M3：执行结果回灌 sched_d（sched.absorb 模式 B） */
        cli_roadmap_absorb_result(exec_id, input, result, rs, rs);
    }
#else
    (void)succeeded;
#endif
    tp_metrics(plan, turn_start);
}

int cli_run_task_pipeline(cli_runtime_ctx_t *rt, const char *input, uint64_t turn_start)
{
    airy_task_plan_t *plan = NULL;
    char *exec_id = NULL;
    char *result = NULL;
    airy_err_t err = AIRY_EOK;
    int polls;

    g_cli_cancel = 0;

    /* === 认知规划（唯一经 gateway → think_d）→ 提交 → 轮询 → 等待 → 结果汇总 === */
    if (tp_plan(input, &plan) != 0)
        return 1;
    if (tp_submit(plan, input, rt->main_workspace_dir, &exec_id) != 0) {
        airy_task_plan_free(plan);
        return 1;
    }
    polls = tp_board_run(exec_id);
    cli_trace("wait", "%s exec=%s awaiting completion (polls=%d)", CLI_ICON_DIAMOND, exec_id,
              polls);
    tp_wait(exec_id, &err, &result);
    cli_trace("wait", "%s done err=%d has_result=%d", CLI_ICON_DONE, (int)err,
              result ? 1 : 0);
    tp_report(plan, input, exec_id, result, err, turn_start);

    AIRY_FREE(result);
    AIRY_FREE(exec_id);
    airy_task_plan_free(plan);
    return 0;
}
