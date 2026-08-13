// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_display.c
 * @brief airy_cli display domain: banner, results and task board lines.
 *
 * Thin facade over the shared rendering layer (cli_render.c): banner,
 * result pretty-printing, node-level progress callbacks and board lines.
 * All terminal output flows through role-tagged render functions so the
 * whole CLI shares one visual language.
 */

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

    char truncated[640];
    snprintf(truncated, sizeof(truncated), "%.600s%s", result,
             strlen(result) > 600 ? "..." : "");
    cli_render_role_line(CLI_ROLE_SUPER_AGENT, CLI_ACTOR_SUPER_AGENT, "result", truncated);
}

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
 * Edges whose source is missing from the workflow (external dependency) or
 * self-loops are treated as satisfied so ordering still terminates.
 */
static int cli_plan_ready(const taskflow_workflow_t *wf, size_t idx,
                          const unsigned char *ordered)
{
    const char *target = wf->nodes[idx].id;
    for (size_t e = 0; e < wf->edge_count; e++) {
        const taskflow_edge_t *ed = &wf->edges[e];
        if (strcmp(ed->target_node_id, target) != 0)
            continue;
        if (strcmp(ed->source_node_id, target) == 0)
            continue;
        int src = cli_plan_node_index(wf, ed->source_node_id);
        if (src >= 0 && !ordered[src])
            return 0;
    }
    return 1;
}

/**
 * @brief Print the execution plan as an ordered task list.
 *
 * Renders nodes in dependency (topological) order, each line carrying a
 * sequence number, a state glyph, the handler and the incoming deps:
 *
 *     [Sub plan Agent] Execution plan (3 nodes, topo order):
 *         ◇ 1. n1  agent_analyze   analyze requirements
 *         ◇ 2. n2  agent_codegen   generate code   <- dep: n1
 */
void cli_print_plan_list(const taskflow_workflow_t *wf)
{
    if (!wf || wf->node_count == 0) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "plan",
                                  "Empty plan, nothing to execute.");
        return;
    }

    const size_t n = wf->node_count;
    char line[256];
    snprintf(line, sizeof(line), "Execution plan (%zu nodes, topo order):", n);
    cli_render_sub_agent_line(CLI_ROLE_TRACE, "plan", line);

    size_t *order = (size_t *)AIRY_MALLOC(n * sizeof(size_t));
    unsigned char *ordered = (unsigned char *)AIRY_CALLOC(n, 1);
    if (!order || !ordered) {
        AIRY_FREE(order);
        AIRY_FREE(ordered);
        return;
    }

    size_t placed = 0;
    for (size_t step = 0; step < n; step++) {
        size_t pick = (size_t)-1;
        for (size_t i = 0; i < n; i++) {
            if (ordered[i])
                continue;
            if (cli_plan_ready(wf, i, ordered)) {
                pick = i;
                break;
            }
        }
        if (pick == (size_t)-1)
            break; /* cycle or unconstrained remainder: stop ordering */
        order[placed++] = pick;
        ordered[pick] = 1;
    }
    for (size_t i = 0; i < n && placed < n; i++) {
        if (!ordered[i]) {
            order[placed++] = i;
            ordered[i] = 1;
        }
    }

    for (size_t r = 0; r < n; r++) {
        const taskflow_node_t *nd = &wf->nodes[order[r]];
        const char *g = cli_gutter_pad(4);
        /* Whole plan is background trace: node id keeps a cyan accent for
         * quick scanning, the rest renders dim so the plan cannot compete
         * with the reply (Claude Code keeps execution detail subtle). */
        printf("%s%s%2zu.%s %s◇%s %s%s%s %s%s%s %s%s%s",
               g, cli_c(CLR_DIM), r + 1, cli_c(CLR_RESET), cli_c(CLR_DIM),
               cli_c(CLR_RESET), cli_c(CLR_CYAN), nd->id, cli_c(CLR_RESET),
               cli_c(CLR_DIM), nd->task_handler_name ? nd->task_handler_name : "?",
               cli_c(CLR_RESET), cli_c(CLR_DIM), nd->name[0] ? nd->name : "",
               cli_c(CLR_RESET));

        /* incoming deps, best effort */
        char deps[128];
        size_t dlen = (size_t)snprintf(deps, sizeof(deps), "  %s<- dep:%s ", cli_c(CLR_DIM),
                                       cli_c(CLR_RESET));
        int first = 1;
        for (size_t e = 0; e < wf->edge_count; e++) {
            const taskflow_edge_t *ed = &wf->edges[e];
            if (strcmp(ed->target_node_id, nd->id) != 0)
                continue;
            if (strcmp(ed->source_node_id, nd->id) == 0)
                continue;
            if (!first && dlen < sizeof(deps) - 2)
                dlen += (size_t)snprintf(deps + dlen, sizeof(deps) - dlen, ", ");
            first = 0;
            if (dlen < sizeof(deps) - 2)
                dlen += (size_t)snprintf(deps + dlen, sizeof(deps) - dlen, "%s",
                                         ed->source_node_id);
        }
        printf("%s\n", first ? "" : deps);
    }

    AIRY_FREE(order);
    AIRY_FREE(ordered);
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
    if (!node_id) {
        if (state == TASKFLOW_STATE_COMPLETED)
            cli_render_sub_agent(execution_id, "Execution completed.");
        else if (state == TASKFLOW_STATE_FAILED)
            cli_render_sub_agent_line(CLI_ROLE_ERROR, execution_id, "Execution failed.");
        return;
    }
    cli_render_progress_bar(progress, 20, node_id);
}

void cli_board_line(const char *tag, const char *id, const char *state, double progress)
{
    cli_render_task_line(tag, id, state, progress);
}

/* ---- welcome screen: brand box, role legend, model panel ---- */

/* Color gating uses the shared cli_c() from cli_render.c (NO_COLOR / piped
 * output renders the banner monochrome too, server-grade clean logs). */

/* Box frame row: <left> + <fill> x inner + <right>, whole line in cyan.
 * Used for the top/bottom edges (┌─┐ / └─┘) and the ═ separator rows. */
static void cli_box_line(const char *g, size_t inner, const char *left,
                         const char *fill, const char *right)
{
    printf("%s%s%s", g, cli_c(CLR_CYAN), left);
    for (size_t i = 0; i < inner; i++)
        fputs(fill, stdout);
    printf("%s%s\n", right, cli_c(CLR_RESET));
}

/* Centered content row: │ <text> │ with CJK-aware padding so the right
 * border lines up even when the row mixes ASCII and CJK text. */
static void cli_box_row(const char *g, size_t inner, const char *text,
                        const char *color)
{
    size_t w = cli_disp_width(text);
    size_t l = w < inner ? (inner - w) / 2 : 0;
    size_t r = w < inner ? inner - w - l : 0;

    printf("%s%s│%s", g, cli_c(CLR_CYAN), cli_c(CLR_RESET));
    for (size_t i = 0; i < l; i++)
        fputc(' ', stdout);
    fputs(cli_c(color), stdout);
    fputs(text, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    for (size_t i = 0; i < r; i++)
        fputc(' ', stdout);
    printf("%s│%s\n", cli_c(CLR_CYAN), cli_c(CLR_RESET));
}

/* Four-role conversation legend rendered under the brand box.  Each bracket
 * keeps its role color so the scheme [For Thee] / [Super Agent] /
 * [Super Think] / [Sub Agent] is visible at a glance on startup. */
static void cli_banner_legend(const char *g)
{
    static const struct {
        const char *color;
        const char *name;
        const char *label;
    } roles[] = {
        {CLR_CYAN, "For Thee", "你"},
        {CLR_GREEN, "Super Agent", "agentrt"},
        {CLR_YELLOW, "Super Think", "思考"},
        {CLR_MAGENTA, "Sub Agent", "执行体"},
    };

    fputs(g, stdout);
    for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
        if (i > 0) {
            fputs(cli_c(CLR_DIM), stdout);
            fputs(" · ", stdout);
            fputs(cli_c(CLR_RESET), stdout);
        }
        fputs("[", stdout);
        fputs(cli_c(roles[i].color), stdout);
        fputs(roles[i].name, stdout);
        fputs(cli_c(CLR_DIM), stdout);
        fputs("]", stdout);
        fputs(cli_c(CLR_RESET), stdout);
        printf(" %s", roles[i].label);
    }
    fputs(cli_c(CLR_RESET), stdout);
    fputc('\n', stdout);
}

void cli_print_banner(void)
{
    const char *g = cli_gutter_pad(2);
    const size_t inner = 53; /* same content width as the installer banner */

    printf("\n");
    cli_box_line(g, inner, "┌", "─", "┐");
    cli_box_row(g, inner, "AgentRT · 智能体运行时", CLR_BOLD CLR_CYAN);
    cli_box_row(g, inner, "AI Agent Runtime Platform  ·  v" AIRY_CLI_VERSION, CLR_DIM);
    cli_box_line(g, inner, "│", "═", "│");
    cli_box_row(g, inner, "对话 · 任务 · 蓝图调度 · 双思考 · GCCP · 工具执行", CLR_YELLOW);
    cli_box_line(g, inner, "│", "═", "│");
    cli_box_row(g, inner, " \"Agents, To the open air. To OpenAirymax. To hope.\"", CLR_DIM);
    cli_box_line(g, inner, "└", "─", "┘");

    cli_banner_legend(g);

    printf("%s%sAgentRT v%s%s %s·%s 输入 %s/help%s 查看命令 · %squit%s/%sexit%s 退出%s\n",
           g, cli_c(CLR_GREEN), AIRY_CLI_VERSION, cli_c(CLR_RESET), cli_c(CLR_DIM),
           cli_c(CLR_DIM), cli_c(CLR_YELLOW), cli_c(CLR_RESET), cli_c(CLR_YELLOW),
           cli_c(CLR_RESET), cli_c(CLR_YELLOW), cli_c(CLR_RESET), cli_c(CLR_RESET));
}

/* ---- combined system header: banner left, model config right, pinned ----
 *
 * The startup block (brand box + four-role legend + one-line hint) doubles as
 * a pinned header: on a wide TTY the model configuration renders to the right
 * of the brand box on the same rows, and the ANSI scroll region locks all of
 * it in place so conversation output scrolls below it instead of pushing the
 * header off screen (Terminal feedback: "system header must stay fixed").
 *
 * Widths: brand box spans cols 1..57 (2 gutter + 55 frame), the model column
 * starts at col 60. Requires >= 118 columns; narrower terminals / non-TTY
 * fall back to the stacked layout (banner above, model config below).
 */

#define CLI_HDR_MODEL_COL 60
#define CLI_HDR_MIN_COLS 118

/* One model row rendered at an absolute column (no leading gutter). */
static void cli_model_row_at(int col, const char *key, const char *role,
                             const char *model, const char *note)
{
    size_t kw = cli_disp_width(key);
    size_t rw = cli_disp_width(role);

    printf("%s%s%s", cli_c(CLR_CYAN), key, cli_c(CLR_RESET));
    for (size_t i = kw; i < 10; i++)
        fputc(' ', stdout);
    fputs(cli_c(CLR_DIM), stdout);
    fputs(role, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    for (size_t i = rw; i < 10; i++)
        fputc(' ', stdout);
    printf("%s→%s %s%s%s", cli_c(CLR_DIM), cli_c(CLR_RESET), cli_c(CLR_YELLOW), model,
           cli_c(CLR_RESET));
    if (note && note[0])
        printf("  %s%s%s", cli_c(CLR_DIM), note, cli_c(CLR_RESET));
}

void cli_print_system_header(const char *t2, const char *t1f, const char *t1p)
{
    const char *a = (t2 && t2[0]) ? t2 : "默认";
    const char *b = (t1f && t1f[0]) ? t1f : "默认";
    const char *c = (t1p && t1p[0]) ? t1p : "默认";

    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);

    if (!cli_term_is_tty() || cols < CLI_HDR_MIN_COLS) {
        /* Narrow / non-TTY: stacked layout (full compatibility). */
        cli_print_banner();
        cli_print_model_config(t2, t1f, t1p);
        return;
    }

    /* Wide TTY: brand box (11 lines) left, model config right on the same
     * header rows. The header is then pinned so scrolling stays below it. */
    cli_print_banner(); /* cursor lands at line 12 (header is 11 lines tall) */

    cli_term_cursor_to(2, CLI_HDR_MODEL_COL);
    printf("%s模型配置%s  %sA 生成 · B 仲裁/日常 · C 校验%s\n", cli_c(CLR_GREEN),
           cli_c(CLR_RESET), cli_c(CLR_DIM), cli_c(CLR_RESET));
    cli_term_cursor_to(3, CLI_HDR_MODEL_COL);
    cli_model_row_at(CLI_HDR_MODEL_COL, "A · t2", "生成器", a, NULL);
    cli_term_cursor_to(4, CLI_HDR_MODEL_COL);
    cli_model_row_at(CLI_HDR_MODEL_COL, "B · t1-f", "仲裁/日常", b, "(最先激活)");
    cli_term_cursor_to(5, CLI_HDR_MODEL_COL);
    cli_model_row_at(CLI_HDR_MODEL_COL, "C · t1-p", "校验", c, NULL);
    cli_term_cursor_to(6, CLI_HDR_MODEL_COL);
    printf("%s%s%s%s", cli_c(CLR_DIM), "env: ", cli_c(CLR_YELLOW),
           "AIRY_MODEL_T2 · T1F · T1P");
    printf("%s%s", cli_c(CLR_RESET), cli_c(CLR_DIM));
    printf("  云端 API 或本地 Ollama/vLLM%s\n", cli_c(CLR_RESET));

    /* Back to the first scrollable line, then pin the 11-line header. */
    cli_term_header_pin(11);
    fflush(stdout);
}

/* One model row of the startup panel: "A · t2   生成器    → <model>".
 * Columns are padded against cli_disp_width so ASCII keys and CJK role
 * labels share the same left edge. */
static void cli_model_row(const char *g, const char *key, const char *role,
                          const char *model, const char *note)
{
    size_t kw = cli_disp_width(key);
    size_t rw = cli_disp_width(role);

    printf("%s%s%s%s", g, cli_c(CLR_CYAN), key, cli_c(CLR_RESET));
    for (size_t i = kw; i < 10; i++)
        fputc(' ', stdout);
    fputs(cli_c(CLR_DIM), stdout);
    fputs(role, stdout);
    fputs(cli_c(CLR_RESET), stdout);
    for (size_t i = rw; i < 10; i++)
        fputc(' ', stdout);
    printf("%s→%s %s%s%s", cli_c(CLR_DIM), cli_c(CLR_RESET), cli_c(CLR_YELLOW), model,
           cli_c(CLR_RESET));
    if (note && note[0])
        printf("  %s%s%s", cli_c(CLR_DIM), note, cli_c(CLR_RESET));
    fputc('\n', stdout);
}

/* Three-model startup panel (GRAD separation of powers):
 *   A · t2    生成器     → <model>
 *   B · t1-f  仲裁/日常  → <model>   (最先激活)
 *   C · t1-p  校验       → <model>
 * Unset env vars fall back to the provider default ("默认"). */
void cli_print_model_config(const char *t2, const char *t1f, const char *t1p)
{
    const char *g = cli_gutter_pad(2);
    const char *a = (t2 && t2[0]) ? t2 : "默认";
    const char *b = (t1f && t1f[0]) ? t1f : "默认";
    const char *c = (t1p && t1p[0]) ? t1p : "默认";

    printf("\n");
    printf("%s%s模型配置%s  %sA 生成 · B 仲裁/日常 · C 校验%s\n", g, cli_c(CLR_GREEN),
           cli_c(CLR_RESET), cli_c(CLR_DIM), cli_c(CLR_RESET));
    cli_model_row(g, "A · t2", "生成器", a, NULL);
    cli_model_row(g, "B · t1-f", "仲裁/日常", b, "(最先激活)");
    cli_model_row(g, "C · t1-p", "校验", c, NULL);
    printf("%s%senv 覆盖: %sAIRY_MODEL_T2%s · %sAIRY_MODEL_T1F%s · %sAIRY_MODEL_T1P%s"
           "  云端 API 或本地 Ollama/vLLM%s\n",
           g, cli_c(CLR_DIM), cli_c(CLR_YELLOW), cli_c(CLR_RESET), cli_c(CLR_YELLOW),
           cli_c(CLR_RESET), cli_c(CLR_YELLOW), cli_c(CLR_RESET), cli_c(CLR_RESET));
}
