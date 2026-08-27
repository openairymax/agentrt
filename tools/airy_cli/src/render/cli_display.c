// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_display.c
 * @brief airy_cli result/plan presentation: results, plan list and progress.
 *
 * 结果呈现门面：执行结果 pretty-print（JSON 提取 output/agent_id）、静态
 * 执行计划列表（拓扑序）、节点级进度回调分发与降级看板行。自 2026-08-27
 * 起本文件只保留这三块职责：live plan board（TTY 原位重绘）拆至
 * cli_live_board.c，启动横幅（蓝框 hero）拆至 cli_banner.c。跨文件的
 * 拓扑序 helper（cli_plan_topology_build）与 UTF-8 安全截断
 * （cli_hero_clip）经 cli_display_internal.h 共享，保证静态计划与重绘
 * 看板的节点次序、截断观感一致。所有终端输出仍走角色化渲染层
 * （cli_render.c），整个 CLI 共享一套视觉语言。
 */

#include "cli_internal.h"
#include "cli_display_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/**
 * @brief Pretty-print execution results (prefer extracting output/agent_id from JSON).
 *
 * Renders as the Super Agent talking: a short status line then the produced
 * output rendered as markdown (helps read multi-line agent output).
 */
void cli_print_result(const char *result)
{
    if (!result) {
        cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, "result",
                             "No result was produced.");
        return;
    }

#ifdef AIRY_HAS_CJSON
    cJSON *root = cJSON_Parse(result);
    if (root) {
        cJSON *output = cJSON_GetObjectItem(root, "output");
        cJSON *agent = cJSON_GetObjectItem(root, "agent_id");
        if (output && cJSON_IsString(output)) {
            /* One-shot server mode (-p)：stdout 保持纯净可管道，跳过角色行
             * 前缀，直接渲染结果内容（markdown 归一化，[code] 归一为 ```）。 */
            if (g_cli_print_mode) {
                cli_render_markdown(output->valuestring, 0);
                cJSON_Delete(root);
                return;
            }
            if (agent && cJSON_IsString(agent)) {
                char line[160];
                snprintf(line, sizeof(line), "Execution finished by %s.", agent->valuestring);
                cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, "result", line);
            } else {
                cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, "result",
                                     "Execution finished.");
            }
            cli_render_markdown(output->valuestring, 4);
            cJSON_Delete(root);
            return;
        }
        cJSON_Delete(root);
    }
#endif /* AIRY_HAS_CJSON */

    if (g_cli_print_mode) {
        cli_outf("%s\n", result ? result : "");
        return;
    }

    char truncated[640];
    /* Back off to a UTF-8 boundary so a long result never shows half a
     * multi-byte character (stray continuation byte garbles the preview). */
    size_t keep = cli_utf8_safe_len(result, 600);
    snprintf(truncated, sizeof(truncated), "%.*s%s", (int)keep, result,
             strlen(result) > keep ? "..." : "");
    cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, "result", truncated);
}

/* ---- execution plan ordering (shared by plan list and live board) ---- */

/**
 * @brief Index of a node by id within a workflow (or -1 when absent).
 */
static int cli_plan_node_index(const taskflow_workflow_t *wf, const char *id)
{
    for (size_t i = 0; i < wf->node_count; i++) {
        if (strcmp(wf->nodes[i].id, id) == 0)
            return (int)i;
    }
    return -1;
}

/**
 * @brief True when every in-edge of `idx` comes from an already-ordered node.
 *
 * Edges whose source is missing from the workflow (external dependency),
 * self-loops, or sources outside the ordering window [0, count) are treated
 * as satisfied so ordering still terminates. The window guard keeps callers
 * that sort only the first `count` nodes (e.g. live board truncation at
 * CLI_LIVE_BOARD_MAX) away from an out-of-bounds read on `ordered`.
 */
static int cli_plan_ready(const taskflow_workflow_t *wf, size_t idx,
                          const unsigned char *ordered, size_t count)
{
    const char *target = wf->nodes[idx].id;
    for (size_t e = 0; e < wf->edge_count; e++) {
        const taskflow_edge_t *ed = &wf->edges[e];
        if (strcmp(ed->target_node_id, target) != 0)
            continue;
        if (strcmp(ed->source_node_id, target) == 0)
            continue;
        int src = cli_plan_node_index(wf, ed->source_node_id);
        if (src >= 0 && (size_t)src < count && !ordered[src])
            return 0;
    }
    return 1;
}

/**
 * @brief Fill `order` with the topological order of the first `count`
 *        workflow nodes (see cli_display_internal.h for the contract).
 *
 * Kahn-free greedy selection identical to the original duplicated loops in
 * cli_print_plan_list()/cli_live_board_begin(); cycles fall back to natural
 * order so the render never stalls.
 */
int cli_plan_topology_build(const taskflow_workflow_t *wf, size_t count, size_t *order,
                            unsigned char *scratch)
{
    if (!wf || !order || !scratch || count == 0 || wf->node_count == 0)
        return 0;

    size_t placed = 0;
    for (size_t step = 0; step < count; step++) {
        size_t pick = (size_t)-1;
        for (size_t i = 0; i < count; i++) {
            if (scratch[i])
                continue;
            if (cli_plan_ready(wf, i, scratch, count)) {
                pick = i;
                break;
            }
        }
        if (pick == (size_t)-1)
            break;
        order[placed++] = pick;
        scratch[pick] = 1;
    }
    for (size_t i = 0; i < count && placed < count; i++) {
        if (!scratch[i]) {
            order[placed++] = i;
            scratch[i] = 1;
        }
    }
    return placed == count;
}

/**
 * @brief Print the execution plan as an ordered task list.
 *
 * Renders nodes in dependency (topological) order with clear visual hierarchy:
 *
 *     ◇ 执行计划 (3 nodes, 2 deps)
 *       □ 1. analyze requirements        [agent_analyze]
 *       □ 2. generate code               [agent_codegen]    ← n1
 *       □ 3. verify output               [agent_verify]     ← n2
 */
void cli_print_plan_list(const taskflow_workflow_t *wf)
{
    if (g_cli_print_mode)
        return;
    if (!wf || wf->node_count == 0) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "plan",
                                  "Empty plan, nothing to execute.");
        return;
    }

    const size_t n = wf->node_count;
    const char *g = cli_gutter_pad(2);

    cli_outf("%s%s%s %s执行计划%s %s(%zu nodes, %zu deps)%s\n",
             g, cli_c(CLR_CYAN), CLI_ICON_DIAMOND,
             cli_c(CLR_RESET), cli_c(CLR_DIM), cli_c(CLR_DIM),
             n, wf->edge_count, cli_c(CLR_RESET));

    size_t *order = (size_t *)AIRY_MALLOC(n * sizeof(size_t));
    unsigned char *scratch = (unsigned char *)AIRY_CALLOC(n, 1);
    if (!order || !scratch || !cli_plan_topology_build(wf, n, order, scratch)) {
        AIRY_FREE(order);
        AIRY_FREE(scratch);
        return;
    }

    const char *ig = cli_gutter_pad(4);
    for (size_t r = 0; r < n; r++) {
        const taskflow_node_t *nd = &wf->nodes[order[r]];
        const char *handler = nd->task_handler_name ? nd->task_handler_name : "?";
        const char *goal = nd->name[0] ? nd->name : "";

        cli_outf("%s%s%s%s %zu. %s%s%s  %s%s[%s]%s",
                 ig, cli_c(CLR_DIM), CLI_ICON_TODO, cli_c(CLR_RESET),
                 r + 1,
                 cli_c(CLR_RESET), goal, cli_c(CLR_DIM),
                 cli_c(CLR_DIM), handler, cli_c(CLR_DIM), cli_c(CLR_RESET));

        char deps[128];
        size_t dlen = 0;
        int has_dep = 0;
        for (size_t e = 0; e < wf->edge_count; e++) {
            const taskflow_edge_t *ed = &wf->edges[e];
            if (strcmp(ed->target_node_id, nd->id) != 0)
                continue;
            if (strcmp(ed->source_node_id, nd->id) == 0)
                continue;
            if (has_dep && dlen < sizeof(deps) - 2)
                dlen += (size_t)snprintf(deps + dlen, sizeof(deps) - dlen, ", ");
            if (!has_dep) {
                dlen = (size_t)snprintf(deps, sizeof(deps), "  %s←%s ",
                                        cli_c(CLR_DIM), cli_c(CLR_RESET));
                has_dep = 1;
            }
            if (dlen < sizeof(deps) - 2)
                dlen += (size_t)snprintf(deps + dlen, sizeof(deps) - dlen, "%s%s%s",
                                         cli_c(CLR_CYAN), ed->source_node_id,
                                         cli_c(CLR_DIM));
        }
        cli_outf("%s\n", has_dep ? deps : "");
    }

    AIRY_FREE(order);
    AIRY_FREE(scratch);
}

/**
 * @brief Per-node progress callback for task sets.
 *
 * Rendered as a Sub Agent report: node id + a progress bar. The final
 * aggregate event (node_id empty) renders as the work hall finishing.
 */
void cli_progress_cb(const char *execution_id, const char *node_id, taskflow_state_t state,
                            double progress, void *user_data)
{
    (void)user_data;

    /* One-shot server mode (-p): node-level progress on the stderr channel
     * (stdout carries only the final answer), throttled to state changes or
     * >=5% progress deltas per node so long-running tasks stay observable
     * without line spam. */
    if (g_cli_print_mode) {
        static char s_exec[64];
        static char s_node[128];
        static int s_state = -1;
        static double s_prog = -1.0;
        if (!node_id) {
            if (state == TASKFLOW_STATE_COMPLETED)
                cli_trace("progress", "%s 执行完成: %s", CLI_ICON_CHECK,
                          execution_id ? execution_id : "?");
            else if (state == TASKFLOW_STATE_FAILED)
                cli_trace("progress", "%s 执行失败: %s", CLI_ICON_CROSS,
                          execution_id ? execution_id : "?");
            return;
        }
        int changed = (strcmp(s_exec, execution_id ? execution_id : "") != 0 ||
                       strcmp(s_node, node_id) != 0 || (int)state != s_state ||
                       progress - s_prog >= 0.05 || s_prog - progress >= 0.05);
        if (!changed)
            return;
        static const char *const st[] = {"pending",  "ready",  "running", "waiting",
                                         "completed", "failed", "canceled", "skipped",
                                         "retrying"};
        const char *ss = (state >= 0 && (int)state < 9) ? st[state] : "?";
        snprintf(s_exec, sizeof(s_exec), "%s", execution_id ? execution_id : "");
        snprintf(s_node, sizeof(s_node), "%s", node_id);
        s_state = (int)state;
        s_prog = progress;
        char sbar[16];
        cli_compact_bar(sbar, sizeof(sbar), progress, 8);
        cli_trace("progress", "%s %s %s %s %3.0f%%", cli_icon_for_state(ss), node_id,
                  cli_state_cn(ss), sbar, progress * 100.0);
        return;
    }

    /* Live plan board 激活（TTY 原位重绘）：回调线程只记录节点状态快照，
     * 由主线程轮询循环统一重绘看板块——避免回调线程直接写终端的交错风险，
     * 也保证图标编排与 footer 汇总来自同一帧数据。 */
    if (cli_live_board_active()) {
        static const char *const st[] = {"pending",  "ready",  "running", "waiting",
                                         "completed", "failed", "canceled", "skipped",
                                         "retrying"};
        const char *ss = (node_id && state >= 0 && (int)state < 9) ? st[state] : "pending";
        cli_live_board_set_node(node_id, ss);
        return;
    }

    if (!node_id) {
        if (state == TASKFLOW_STATE_COMPLETED)
            cli_render_sub_agent(execution_id, "任务执行完成");
        else if (state == TASKFLOW_STATE_FAILED)
            cli_render_sub_agent_line(CLI_ROLE_ERROR, execution_id, "任务执行失败");
        return;
    }
    /* 节点级进度：分支符号 + 二级缩进，与任务级行形成清晰层级。 */
    static const char *const st[] = {"pending",  "ready",  "running", "waiting",
                                     "completed", "failed", "canceled", "skipped",
                                     "retrying"};
    const char *ss = (state >= 0 && (int)state < 9) ? st[state] : "?";
    char buf[96];
    snprintf(buf, sizeof(buf), "%s %s", cli_state_cn(ss), node_id);
    cli_render_progress_bar(progress, 20, buf);
}

void cli_board_line(const char *tag, const char *id, const char *state, double progress)
{
    cli_render_task_line(tag, id, state, progress);
}
