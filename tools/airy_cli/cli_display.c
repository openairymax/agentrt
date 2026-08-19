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
            break;
        order[placed++] = pick;
        ordered[pick] = 1;
    }
    for (size_t i = 0; i < n && placed < n; i++) {
        if (!ordered[i]) {
            order[placed++] = i;
            ordered[i] = 1;
        }
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

/* ---- hero: blue-framed welcome panel (Claude-style, pinned) ----
 *
 * The startup header is a blue box with the brand on the top edge:
 *
 *   ┌─ ◆ Airymax - Agent Runtime Platform Engineering ─┐
 *   │  版本 v0.1.2：对话 · 任务 · 蓝图调度 · 双思考 · … │
 *   │  [For Thee] 你  [Super Agent] agentrt  …         │
 *   │  A·t2 → …  B·t1-f → …  C·t1-p → …               │
 *   │  ? /help 查看命令 · quit/exit 退出  "Agents…"     │
 *   └───────────────────────────────────────────────────┘
 *
 * The frame is blue (CLR_BLUE); inner rows keep their own role colors so
 * the [For Thee]/[Super Agent]/[Dual Think]/[Sub Agent] scheme stays
 * visible. The whole block is pinned so conversation output scrolls below
 * it — a clear visual boundary between the system header and the dialogue.
 * Color gating uses the shared cli_c() (NO_COLOR / piped output renders
 * monochrome too, keeping the box geometry).
 *
 * Every content row is width-budgeted (cli_hero_content_max): segments
 * that would overflow the frame are dropped or clipped (UTF-8 safe, "…"),
 * so on narrow terminals the box stays intact instead of wrapping — a
 * wrapped row would shift the pinned line count and let the dialogue
 * overlap the header.
 */

/* Frame width: adapts to the terminal, clamped for readability. */
static size_t cli_hero_frame_w(void)
{
    int rows = 0, cols = 0;
    cli_term_size(&rows, &cols);
    size_t w = (cols > 4) ? (size_t)cols - 4 : 74;
    if (w < 74)
        w = 74;
    if (w > 110)
        w = 110;
    return w;
}

/* Content budget inside the frame: each row is
 *   gutter + "│ " + content + padding + "│"
 * so content may use at most w-3 cells (one padding cell remains). */
static size_t cli_hero_content_max(size_t w)
{
    return (w > 3) ? w - 3 : 0;
}

/* Width-aware, UTF-8-safe truncation of `s` into buf (cap bytes) so it
 * fits max_w cells; appends "…" (1 cell) when text was cut. Returns the
 * display width of what was emitted. */
static size_t cli_hero_clip(const char *s, size_t max_w, char *buf, size_t cap)
{
    size_t len = strlen(s);
    size_t n = 0, w = 0;
    while (n < len) {
        unsigned char c = (unsigned char)s[n];
        size_t cbytes = (c < 0x80) ? 1
                      : ((c & 0xE0) == 0xC0) ? 2
                      : ((c & 0xF0) == 0xE0) ? 3
                      : ((c & 0xF8) == 0xF0) ? 4 : 1;
        if (n + cbytes >= cap)
            break; /* keep room for the terminator */
        /* width of the current character only (not the rest of the
         * string): pass cbytes, otherwise the whole remaining tail is
         * counted and every row gets clipped to its first glyph */
        size_t cw = cli_disp_width_of(s + n, cbytes);
        if (w + cw > max_w)
            break;
        AIRY_MEMCPY(buf + n, s + n, cbytes);
        n += cbytes;
        w += cw;
    }
    if (n < len && w + 1 <= max_w && n + 3 < cap) {
        /* cut: append "…" (U+2026, 3 bytes, 1 cell) */
        buf[n] = '\xE2';
        buf[n + 1] = '\x80';
        buf[n + 2] = '\xA6';
        buf[n + 3] = '\0';
        w += 1;
    } else {
        buf[n] = '\0';
    }
    return w;
}

/* Close a framed content row: pad spaces to the frame width then "│". */
static void cli_hero_line_end(size_t used, size_t w)
{
    cli_out(cli_c(CLR_BLUE));
    while (used + 1 < w) {
        cli_out(" ");
        used++;
    }
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* Top edge with the brand in the title area: ┌─ ◆ Airymax - Agent Runtime
 * Platform Engineering ──────────────────────────┐ */
static void cli_hero_brand(const char *g)
{
    size_t w = cli_hero_frame_w();
    size_t used = 3; /* "┌─ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("┌─ ");
    cli_out(cli_c(CLR_BOLD));
    cli_out(cli_c(CLR_CYAN));
    char buf[96];
    /* leave one cell for a "─" filler before "┐" */
    size_t tmax = cli_hero_content_max(w);
    if (tmax > 1)
        tmax -= 1;
    size_t tw = cli_hero_clip("◆ Airymax - Agent Runtime Platform Engineering",
                              tmax, buf, sizeof(buf));
    cli_out(buf);
    cli_out(cli_c(CLR_RESET));
    used += tw;
    cli_out(cli_c(CLR_BLUE));
    while (used + 1 < w) {
        cli_out("─");
        used++;
    }
    cli_out("┐");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* Bottom edge: └─────┘ */
static void cli_hero_frame_bottom(const char *g)
{
    size_t w = cli_hero_frame_w();
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("└");
    for (size_t i = 1; i + 1 < w; i++)
        cli_out("─");
    cli_out("┘");
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

/* One quiet capabilities row inside the frame, carrying the version and
 * what the runtime does at a glance: 版本 v0.1.2：对话 · 任务 · … */
static void cli_hero_capabilities(const char *g)
{
    char text[160];
    /* 1 leading space matches the other hero rows ("│ [For Thee]",
     * "│ A·t2", "│ ? /help") so all content is left-aligned inside
     * the frame. */
    snprintf(text, sizeof(text), " 版本 v%s：对话 · 任务 · 蓝图调度 · 双思考 · GCCP · 工具执行",
             AIRY_CLI_VERSION);
    size_t w = cli_hero_frame_w();
    size_t budget = cli_hero_content_max(w);
    char out[160];
    size_t tw = cli_hero_clip(text, budget, out, sizeof(out));
    size_t used = 2 + tw; /* "│ " + content */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_out(" ");
    cli_out(cli_c(CLR_DIM));
    cli_out(out);
    cli_out(cli_c(CLR_RESET));
    cli_hero_line_end(used, w);
}

/* Four-role conversation legend. Each bracket keeps its role color so the
 * scheme [For Thee] / [Super Agent] / [Dual Think] / [Sub Agent] is visible
 * at a glance on startup and stays consistent with every conversation line.
 * On narrow terminals later roles are dropped before the frame wraps. */
static void cli_banner_legend(const char *g)
{
    static const struct {
        const char *color;
        const char *name;
        const char *label;
    } roles[] = {
        {CLR_CYAN, "For Thee", "你"},
        {CLR_GREEN, "Super Agent", "agentrt"},
        {CLR_YELLOW, "Dual Think", "思考"},
        {CLR_MAGENTA, "Sub Agent", "执行体"},
    };

    size_t w = cli_hero_frame_w();
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_out(" ");
    for (size_t i = 0; i < sizeof(roles) / sizeof(roles[0]); i++) {
        size_t seg = 2 + cli_disp_width(roles[i].name) +
                     1 + cli_disp_width(roles[i].label);
        if (i > 0)
            seg += 3;
        if (used + seg > budget)
            break; /* narrow terminal: drop the remaining roles */
        if (i > 0) {
            cli_out("   ");
            used += 3;
        }
        cli_out("[");
        cli_out(cli_c(roles[i].color));
        cli_out(roles[i].name);
        cli_out(cli_c(CLR_RESET));
        cli_out("]");
        used += 2 + cli_disp_width(roles[i].name);
        cli_outf(" %s", roles[i].label);
        used += 1 + cli_disp_width(roles[i].label);
    }
    cli_out(cli_c(CLR_RESET));
    cli_hero_line_end(used, w);
}

/* Emit one "key → model" segment for the model row. The model name is
 * clipped to the remaining budget (UTF-8 safe) so a narrow terminal keeps
 * the frame intact. Returns 0 when not even the key + arrow fit (stop the
 * row). */
static int cli_hero_model_seg(const char *key, const char *model,
                              size_t *used, size_t budget, int lead_sep)
{
    size_t sep = lead_sep ? 3 : 0;
    size_t key_w = cli_disp_width(key);
    size_t model_w = cli_disp_width(model);
    if (*used + sep + key_w + 3 + model_w <= budget) {
        if (lead_sep) {
            cli_out("   ");
            *used += 3;
        }
        cli_out(cli_c(CLR_CYAN));
        cli_out(key);
        cli_out(cli_c(CLR_RESET));
        *used += key_w;
        cli_out(cli_c(CLR_DIM));
        cli_out(" → ");
        cli_out(cli_c(CLR_RESET));
        *used += 3;
        cli_out(cli_c(CLR_YELLOW));
        cli_out(model);
        cli_out(cli_c(CLR_RESET));
        *used += model_w;
        return 1;
    }
    size_t left = (*used + sep + key_w + 3 <= budget)
                      ? budget - *used - sep - key_w - 3
                      : 0;
    if (left < 2)
        return 0;
    char buf[64];
    size_t mw = cli_hero_clip(model, left, buf, sizeof(buf));
    if (lead_sep) {
        cli_out("   ");
        *used += 3;
    }
    cli_out(cli_c(CLR_CYAN));
    cli_out(key);
    cli_out(cli_c(CLR_RESET));
    *used += key_w;
    cli_out(cli_c(CLR_DIM));
    cli_out(" → ");
    cli_out(cli_c(CLR_RESET));
    *used += 3;
    cli_out(cli_c(CLR_YELLOW));
    cli_out(buf);
    cli_out(cli_c(CLR_RESET));
    *used += mw;
    return 1;
}

/* One compact model-config row: the three GRAD roles (A·t2 generator,
 * B·t1-f arbiter, C·t1-p verifier). Empty env/yaml values fall back to
 * the provider default ("默认"). */
static void cli_model_line(const char *g, const char *t2, const char *t1f,
                           const char *t1p)
{
    const char *a = (t2 && t2[0]) ? t2 : "默认";
    const char *b = (t1f && t1f[0]) ? t1f : "默认";
    const char *c = (t1p && t1p[0]) ? t1p : "默认";

    size_t w = cli_hero_frame_w();
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_out(" ");

    static const char *const keys[] = {"A·t2", "B·t1-f", "C·t1-p"};
    const char *models[] = {a, b, c};
    for (size_t i = 0; i < 3; i++) {
        if (!cli_hero_model_seg(keys[i], models[i], &used, budget, i > 0))
            break;
    }
    cli_hero_line_end(used, w);
}

/* In-frame footer row: command hints + the project motto. Lives inside the
 * frame so the whole system header reads as one pinned block above the
 * dialogue. */
static void cli_hero_footer(const char *g)
{
    size_t w = cli_hero_frame_w();
    size_t budget = cli_hero_content_max(w);
    size_t used = 2; /* "│ " */
    cli_out(g);
    cli_out(cli_c(CLR_BLUE));
    cli_out("│");
    cli_out(cli_c(CLR_RESET));
    cli_out(" ");

    cli_out(cli_c(CLR_DIM));
    cli_out("? ");
    cli_out(cli_c(CLR_RESET));
    used += 2;
    cli_out(cli_c(CLR_YELLOW));
    cli_out("/help");
    cli_out(cli_c(CLR_RESET));
    used += 5;
    cli_out(cli_c(CLR_DIM));
    cli_out(" 查看命令 · ");
    cli_out(cli_c(CLR_RESET));
    used += cli_disp_width(" 查看命令 · ");
    cli_out(cli_c(CLR_YELLOW));
    cli_out("quit");
    cli_out(cli_c(CLR_RESET));
    used += 4;
    cli_out(cli_c(CLR_DIM));
    cli_out("/exit");
    cli_out(cli_c(CLR_RESET));
    used += 5;
    cli_out(cli_c(CLR_DIM));
    cli_out(" 退出");
    cli_out(cli_c(CLR_RESET));
    used += 1 + cli_disp_width("退出");

    /* The motto is clipped to whatever room remains (narrow terminals). */
    size_t left = budget - used;
    if (left >= 2) {
        char buf[64];
        static const char *motto = "  \"Agents, To the open air.\"";
        size_t mw = cli_hero_clip(motto, left, buf, sizeof(buf));
        cli_out(cli_c(CLR_DIM));
        cli_out(buf);
        cli_out(cli_c(CLR_RESET));
        used += mw;
    }
    cli_hero_line_end(used, w);
}

/* ---- unified blue-framed system header ----
 *
 * Title edge + version/capabilities + role legend + model config + footer
 * hints, all enclosed in a blue frame (6 pinned lines including the two
 * edges). The whole block is pinned so conversation output scrolls below
 * it (Terminal feedback: "system header must stay fixed"); the frame marks
 * the boundary between the system header and the dialogue.
 */
void cli_print_system_header(const char *t2, const char *t1f, const char *t1p)
{
    /* One-shot server mode (-p): no startup hero/panel, output is the
     * single-turn result only (Claude Code -p / Codex exec convention). */
    if (g_cli_print_mode)
        return;

    const char *g = cli_gutter_pad(2);
    cli_hero_brand(g);
    cli_hero_capabilities(g);
    cli_banner_legend(g);
    cli_model_line(g, t2, t1f, t1p);
    cli_hero_footer(g);
    cli_hero_frame_bottom(g);

    /* Full-screen TUI page pins its own header boundary (history-based)
     * after this; non-TTY output just scrolls. Only interactive plain
     * TTYs pin the 6-line block; the bottom row is reserved as the fixed
     * input zone (three-zone layout: hero / dialogue / input). */
    if (!cli_term_is_tty() || cli_tui_active(cli_tui_get_default()))
        return;
    cli_term_header_pin(CLI_HDR_LINES, 1);
    fflush(stdout);
}



