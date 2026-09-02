/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_graph.c
 * @brief 依赖图图算法实现（从 main.c 拆分，M3 x-cutting-b）。
 */

#include "depgraph_graph.h"

/* Kahn 拓扑排序：复用 graph_engine_get_in_edges 计算入度。 */
size_t dg_kahn(graph_engine_handle_t engine, size_t n, vertex_id_t *order)
{
    graph_edge_t edges[AIRY_DG_MAX_DOMAIN];
    size_t indeg[AIRY_DG_MAX_DOMAIN] = {0};
    bool consumed[AIRY_DG_MAX_DOMAIN] = {false};

    for (size_t v = 0; v < n; v++) {
        indeg[v] =
            graph_engine_get_in_edges(engine, DG_VID(v), edges, AIRY_DG_MAX_DOMAIN);
    }

    size_t done = 0;
    for (;;) {
        bool progress = false;
        for (size_t v = 0; v < n; v++) {
            if (consumed[v] || indeg[v] != 0)
                continue;
            consumed[v] = true;
            order[done++] = (vertex_id_t)v; /* order 存数组下标，报告用 */
            size_t out =
                graph_engine_get_out_edges(engine, DG_VID(v), edges, AIRY_DG_MAX_DOMAIN);
            for (size_t e = 0; e < out; e++)
                indeg[edges[e].target - 1]--;
            progress = true;
        }
        if (!progress)
            break;
    }
    return n - done;
}

/* DFS 三色环路径：从每个未访问顶点出发，gray 栈内回边即环。 */
void dg_dfs_cycles(graph_engine_handle_t engine, const dg_manifest_t *mf, size_t n)
{
    unsigned char color[AIRY_DG_MAX_DOMAIN] = {0}; /* 0 white, 1 gray, 2 black */
    vertex_id_t stack[AIRY_DG_MAX_DOMAIN];
    size_t next_idx[AIRY_DG_MAX_DOMAIN] = {0};
    bool found = false;

    for (size_t start = 0; start < n; start++) {
        if (color[start] != 0)
            continue;
        size_t sp = 0;
        color[start] = 1;
        stack[sp++] = DG_VID(start);

        while (sp > 0) {
            vertex_id_t cur = stack[sp - 1];
            size_t cur_idx = (size_t)(cur - 1);
            graph_edge_t edges[AIRY_DG_MAX_DOMAIN];
            size_t out =
                graph_engine_get_out_edges(engine, cur, edges, AIRY_DG_MAX_DOMAIN);
            bool advanced = false;
            while (next_idx[cur_idx] < out) {
                vertex_id_t tgt = edges[next_idx[cur_idx]++].target;
                size_t tgt_idx = (size_t)(tgt - 1);
                if (color[tgt_idx] == 0) {
                    color[tgt_idx] = 1;
                    stack[sp++] = tgt;
                    advanced = true;
                    break;
                } else if (color[tgt_idx] == 1) {
                    if (!found) {
                        dg_printf("\n== 检测到循环依赖（环路径） ==\n");
                        found = true;
                    }
                    size_t k = 0;
                    while (k < sp && stack[k] != tgt)
                        k++;
                    dg_printf("  环: ");
                    for (size_t i = k; i < sp; i++)
                        dg_printf("%s -> ", mf->domains[stack[i] - 1].name);
                    dg_printf("%s\n", mf->domains[tgt - 1].name);
                }
            }
            if (!advanced) {
                color[cur_idx] = 2;
                sp--;
            }
        }
    }
    if (!found)
        dg_printf("  无\n");
}
