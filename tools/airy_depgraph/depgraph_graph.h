/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_graph.h
 * @brief 依赖图图算法：Kahn 拓扑排序 + DFS 三色环路径（M3 x-cutting-b）。
 */

#ifndef AIRY_DG_GRAPH_H
#define AIRY_DG_GRAPH_H

#include "depgraph_core.h"

#include "graph_engine.h"
#include "taskflow.h"

/* graph_engine 保留 vertex_id=0 非法：子域 i（数组下标）映射为顶点 id=i+1 */
#define DG_VID(i) ((vertex_id_t)((i) + 1))

/**
 * @brief Kahn 拓扑排序：返回 0 且 order 填满 = DAG；否则返回未消费节点数。
 *        order[] 存数组下标（报告用）。
 */
size_t dg_kahn(graph_engine_handle_t engine, size_t n, vertex_id_t *order);

/** @brief DFS 三色环路径输出（gray 栈内回边即环）。 */
void dg_dfs_cycles(graph_engine_handle_t engine, const dg_manifest_t *mf, size_t n);

#endif /* AIRY_DG_GRAPH_H */
