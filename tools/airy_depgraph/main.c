/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file main.c
 * @brief 依赖图校验工具入口（0.1.6 P1-2 + P1-1 原子化门禁）：commons 子域
 *        依赖声明 -> DAG -> 拓扑排序 + 环检测 + 报告 + include 漂移门禁。
 *
 * M3 (0.1.9 §4.2 持续横切 x-cutting-b)：848 行主文件按单一职责拆分为
 * depgraph_core/manifest/drift/graph/links 五模块，本文件仅保留入口、
 * 参数解析、图构建与报告输出。构建期门禁 fail-closed：
 *   退出码 0 = 无环（输出拓扑序报告）
 *   退出码 1 = 存在环（输出全部环路径）
 *   退出码 2 = manifest 解析错误 / include 漂移 / 链接白名单违规
 *
 * 用法:
 *   airy_depgraph [-o <report>] [--root <commons-dir>]
 *                 [--links <whitelist> --actual <actual-links>] <manifest>
 */

#include "depgraph_core.h"
#include "depgraph_drift.h"
#include "depgraph_graph.h"
#include "depgraph_links.h"
#include "depgraph_manifest.h"

#include "graph_engine.h"
#include "taskflow.h"

/* ------------------------------------------------------------------ */
static void dg_write_report(FILE *out, const dg_manifest_t *mf, size_t n,
                            const vertex_id_t *order, size_t order_len, size_t edges)
{
    char buf[AIRY_DG_BUF];
    snprintf(buf, sizeof(buf),
             "# commons 子域依赖图报告（0.1.6 P1-2）\n\n"
             "- 子域数: %zu\n- 依赖边数: %zu\n- 有向无环（DAG）: %s\n\n",
             n, edges, order_len == n ? "是" : "否（存在环，见上方环路径）");
    fputs(buf, out);

    if (order_len == n) {
        snprintf(buf, sizeof(buf), "## 拓扑序（依赖先于依赖者）\n\n");
        fputs(buf, out);
        for (size_t i = 0; i < n; i++)
            snprintf(buf, sizeof(buf), "  %zu. %s\n", i + 1, mf->domains[order[i]].name), fputs(buf, out);
    }

    snprintf(buf, sizeof(buf), "\n## 各子域依赖\n\n");
    fputs(buf, out);
    for (size_t i = 0; i < n; i++) {
        const dg_domain_t *dom = &mf->domains[i];
        snprintf(buf, sizeof(buf), "  %s:", dom->name);
        fputs(buf, out);
        if (dom->dep_count == 0) {
            fputs(" (无)\n", out);
        } else {
            for (size_t j = 0; j < dom->dep_count; j++)
                snprintf(buf, sizeof(buf), " %s", dom->deps[j]), fputs(buf, out);
            fputs("\n", out);
        }
    }
}

int main(int argc, char **argv)
{
    const char *manifest_path = NULL;
    const char *report_path = NULL;
    const char *links_whitelist = NULL;
    const char *links_actual = NULL;
    dg_index_t idx = {0};

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            report_path = argv[++i];
        } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            snprintf(idx.root, sizeof(idx.root), "%s", argv[++i]);
        } else if (strcmp(argv[i], "--links") == 0 && i + 1 < argc) {
            links_whitelist = argv[++i];
        } else if (strcmp(argv[i], "--actual") == 0 && i + 1 < argc) {
            links_actual = argv[++i];
        } else if (manifest_path == NULL) {
            manifest_path = argv[i];
        } else {
            dg_eprintf("usage: airy_depgraph [-o <report>] [--root <commons-dir>]\n"
                       "                    [--links <whitelist> --actual <actual-links>] <manifest>\n");
            return 2;
        }
    }
    /* --links/--actual 必须成对；无 manifest 时允许仅链接白名单模式 */
    if ((links_whitelist == NULL) != (links_actual == NULL)) {
        dg_eprintf("airy_depgraph: --links and --actual must be used together\n");
        return 2;
    }
    if (!manifest_path && !links_whitelist) {
        dg_eprintf("usage: airy_depgraph [-o <report>] [--root <commons-dir>]\n"
                   "                    [--links <whitelist> --actual <actual-links>] <manifest>\n");
        return 2;
    }

    int exit_code = 0;
    if (manifest_path) {
        dg_manifest_t mf = {0};
        if (dg_parse_manifest(manifest_path, &mf, false) != 0)
            return 2;

        size_t n = mf.domain_count;
        size_t edge_count = 0;
        for (size_t i = 0; i < n; i++)
            edge_count += mf.domains[i].dep_count;

        taskflow_config_t cfg = {0};
        cfg.max_vertices = AIRY_DG_MAX_DOMAIN;
        cfg.max_edges = AIRY_DG_MAX_DOMAIN * AIRY_DG_MAX_DEP;

        graph_engine_handle_t engine = graph_engine_create(&cfg);
        if (!engine) {
            dg_eprintf("airy_depgraph: graph_engine_create failed\n");
            return 2;
        }
        graph_engine_init(engine);

        for (size_t i = 0; i < n; i++) {
            graph_vertex_t v = {0};
            v.id = (vertex_id_t)(i + 1); /* graph_engine 保留 vertex_id=0 非法 */
            v.value = (void *)mf.domains[i].name; /* 工具自有静态内存，图不释放 */
            v.value_size = strlen(mf.domains[i].name) + 1;
            graph_engine_add_vertex(engine, &v);
        }

        graph_edge_t edge = {0};
        for (size_t i = 0; i < n; i++) {
            const dg_domain_t *dom = &mf.domains[i];
            for (size_t j = 0; j < dom->dep_count; j++) {
                edge.id = (edge_id_t)((i + 1) * AIRY_DG_MAX_DEP + j + 1);
                edge.source = (vertex_id_t)(i + 1);
                edge.target = (vertex_id_t)(dg_name_to_vertex(&mf, dom->deps[j]) + 1);
                graph_engine_add_edge(engine, &edge);
            }
        }

        dg_printf("airy_depgraph: commons 子域依赖图（%zu 子域, %zu 边）\n", n, edge_count);

        vertex_id_t order[AIRY_DG_MAX_DOMAIN] = {0};
        size_t remain = dg_kahn(engine, n, order);

        if (remain == 0)
            dg_printf("拓扑排序: DAG OK（%zu/%zu 节点全部消费）\n", n, n);
        else
            dg_printf("拓扑排序: 有 %zu 个节点未被消费（存在环）\n", remain);

        dg_dfs_cycles(engine, &mf, n);

        /* include 漂移门禁（--root 提供 commons 根时启用，双向校验） */
        exit_code = (remain == 0) ? 0 : 1;
        if (idx.root[0] != '\0' && remain == 0) {
            dg_index_build(&idx, &mf);
            size_t total_drift = 0;
            size_t stale_edges = 0;
            for (size_t i = 0; i < n; i++) {
                size_t used[AIRY_DG_MAX_DEP] = {0};
                size_t drift = dg_scan_domain_includes(&idx, &mf, &mf.domains[i], used);
                if (drift > 0) {
                    dg_printf("include 漂移: 子域 '%s' 存在 %zu 处未声明跨子域引用\n",
                              mf.domains[i].name, drift);
                    total_drift += drift;
                }
                for (size_t d = 0; d < mf.domains[i].dep_count; d++) {
                    if (used[d] == 0) {
                        dg_printf("include 漂移(陈旧边): '%s' 声明的依赖 '%s' 在公共头中无实际 include\n",
                                  mf.domains[i].name, mf.domains[i].deps[d]);
                        stale_edges++;
                    }
                }
            }
            if (total_drift > 0) {
                dg_eprintf("airy_depgraph: %zu 处 include 漂移（未声明跨子域引用），禁止构建\n",
                           total_drift);
                exit_code = 2;
            } else if (stale_edges > 0) {
                dg_printf("airy_depgraph: 发现 %zu 条陈旧声明边（公共头无实际 include），建议清理\n",
                          stale_edges);
            } else {
                dg_printf("include 漂移门禁: OK（全部子域公共头引用均已声明）\n");
            }
        }

        if (report_path) {
            FILE *rf = fopen(report_path, "w");
            if (!rf) {
                dg_eprintf("airy_depgraph: cannot write report '%s'\n", report_path);
                graph_engine_destroy(engine);
                return 2;
            }
            dg_write_report(rf, &mf, n, order, n - remain, edge_count);
            fclose(rf);
            dg_printf("依赖图报告: %s\n", report_path);
        } else {
            dg_write_report(stdout, &mf, n, order, n - remain, edge_count);
        }

        graph_engine_destroy(engine);
    } /* manifest 图校验分支结束 */

    /* 链接白名单门禁（M1-1b）：越权/未登记项目库即 fail-closed */
    if (links_whitelist) {
        dg_manifest_t wl = {0};
        dg_manifest_t ac = {0};
        if (dg_parse_manifest(links_whitelist, &wl, true) != 0)
            return 2;
        if (dg_parse_manifest(links_actual, &ac, true) != 0)
            return 2;
        int link_exit = dg_check_links(&wl, &ac);
        if (link_exit != 0)
            return link_exit;
        dg_printf("airy_depgraph: 链接白名单门禁: OK（目标链接均在白名单内）\n");
    }

    return exit_code;
}
