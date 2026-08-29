/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file main.c
 * @brief 依赖图校验工具（0.1.6 P1-2）：commons 子域依赖声明 -> DAG -> 拓扑
 *        排序 + 环检测 + 报告。
 *
 * 复用 taskflow 图引擎（graph_engine）承载图结构与遍历原语；Kahn 拓扑
 * 排序与 DFS 三色环路径输出在本文件实现。构建期门禁 fail-closed：
 *   退出码 0 = 无环（输出拓扑序报告）
 *   退出码 1 = 存在环（输出全部环路径）
 *   退出码 2 = manifest 解析错误
 *
 * 合规约束（AIRY_COMPLIANCE_STRICT）：全局 poison unsafe 函数
 * （memset/strtok/printf 等），本文件全部使用合规替代
 * （snprintf+fputs / 显式初始化 / 手写 tokenizer）。
 *
 * 用法:
 *   airy_depgraph [-o <report文件>] <manifest>
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph_engine.h"
#include "taskflow.h"

/* ------------------------------------------------------------------ */

#define AIRY_DG_MAX_DOMAIN 96
#define AIRY_DG_MAX_DEP 64
#define AIRY_DG_MAX_LINE 512
#define AIRY_DG_BUF 2048

typedef struct {
    char name[64];
    char deps[AIRY_DG_MAX_DEP][64];
    size_t dep_count;
} dg_domain_t;

typedef struct {
    dg_domain_t domains[AIRY_DG_MAX_DOMAIN];
    size_t domain_count;
} dg_manifest_t;

/* 合规输出：snprintf + fputs（printf/fprintf 被 poison） */
static void dg_printf(const char *fmt, ...)
{
    char buf[AIRY_DG_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
}

static void dg_eprintf(const char *fmt, ...)
{
    char buf[AIRY_DG_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stderr);
}

static size_t dg_name_to_vertex(const dg_manifest_t *mf, const char *name)
{
    for (size_t i = 0; i < mf->domain_count; i++) {
        if (strcmp(mf->domains[i].name, name) == 0)
            return i;
    }
    return SIZE_MAX;
}

/* ------------------------------------------------------------------
 * manifest 解析（手写 tokenizer，strtok 被 poison）
 * ------------------------------------------------------------------ */
static int dg_parse_manifest(const char *path, dg_manifest_t *mf)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        dg_eprintf("airy_depgraph: cannot open manifest '%s'\n", path);
        return -1;
    }

    char line[AIRY_DG_MAX_LINE];
    size_t lineno = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == '\0')
            continue;
        char *hash = strchr(p, '#');
        if (hash)
            *hash = '\0';
        char *nl = strchr(p, '\n');
        if (nl)
            *nl = '\0';
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t'))
            p[--len] = '\0';
        if (p[0] == '\0')
            continue;

        char *colon = strchr(p, ':');
        if (!colon) {
            dg_eprintf("airy_depgraph: %s:%zu: missing ':'\n", path, lineno);
            fclose(fp);
            return -1;
        }
        *colon = '\0';
        char *name = p;
        while (*name == ' ' || *name == '\t')
            name++;
        if (*name == '\0') {
            dg_eprintf("airy_depgraph: %s:%zu: empty domain name\n", path, lineno);
            fclose(fp);
            return -1;
        }

        if (mf->domain_count >= AIRY_DG_MAX_DOMAIN) {
            dg_eprintf("airy_depgraph: %s:%zu: too many domains\n", path, lineno);
            fclose(fp);
            return -1;
        }
        dg_domain_t *dom = &mf->domains[mf->domain_count];
        dom->dep_count = 0;
        snprintf(dom->name, sizeof(dom->name), "%s", name);

        /* 手写按空白分割依赖列表 */
        char *rest = colon + 1;
        while (*rest != '\0') {
            while (*rest == ' ' || *rest == '\t')
                rest++;
            if (*rest == '\0')
                break;
            char *end = rest;
            while (*end != '\0' && *end != ' ' && *end != '\t')
                end++;
            if (strncmp(rest, dom->name, (size_t)(end - rest)) == 0 &&
                dom->name[end - rest] == '\0') {
                dg_eprintf("airy_depgraph: %s:%zu: self-dependency '%s'\n", path, lineno,
                           dom->name);
                fclose(fp);
                return -1;
            }
            if (dom->dep_count >= AIRY_DG_MAX_DEP) {
                dg_eprintf("airy_depgraph: %s:%zu: too many deps for '%s'\n", path, lineno,
                           dom->name);
                fclose(fp);
                return -1;
            }
            size_t toklen = (size_t)(end - rest);
            if (toklen >= sizeof(dom->deps[0]))
                toklen = sizeof(dom->deps[0]) - 1;
            for (size_t ci = 0; ci < toklen; ci++)
                dom->deps[dom->dep_count][ci] = rest[ci];
            dom->deps[dom->dep_count][toklen] = '\0';
            dom->dep_count++;
            rest = end;
        }
        mf->domain_count++;
    }
    fclose(fp);

    for (size_t i = 0; i < mf->domain_count; i++) {
        const dg_domain_t *dom = &mf->domains[i];
        for (size_t j = 0; j < dom->dep_count; j++) {
            if (dg_name_to_vertex(mf, dom->deps[j]) == SIZE_MAX) {
                dg_eprintf("airy_depgraph: unknown dependency '%s' of '%s'\n", dom->deps[j],
                           dom->name);
                return -1;
            }
        }
    }
    return 0;
}

/* graph_engine 保留 vertex_id=0 非法：子域 i（数组下标）映射为顶点 id=i+1 */
#define DG_VID(i) ((vertex_id_t)((i) + 1))

/* ------------------------------------------------------------------
 * Kahn 拓扑排序：复用 graph_engine_get_in_edges 计算入度。
 * 返回 0 且 order 填满 = DAG；否则返回未消费节点数。
 * ------------------------------------------------------------------ */
static size_t dg_kahn(graph_engine_handle_t engine, size_t n, vertex_id_t *order)
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

/* ------------------------------------------------------------------
 * DFS 三色环路径：从每个未访问顶点出发，gray 栈内回边即环。
 * 复用 graph_engine_get_out_edges 遍历邻居（graph_engine 遍历原语）。
 * ------------------------------------------------------------------ */
static void dg_dfs_cycles(graph_engine_handle_t engine, const dg_manifest_t *mf, size_t n)
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

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            report_path = argv[++i];
        } else if (manifest_path == NULL) {
            manifest_path = argv[i];
        } else {
            dg_eprintf("usage: airy_depgraph [-o <report>] <manifest>\n");
            return 2;
        }
    }
    if (!manifest_path) {
        dg_eprintf("usage: airy_depgraph [-o <report>] <manifest>\n");
        return 2;
    }

    dg_manifest_t mf = {0};
    if (dg_parse_manifest(manifest_path, &mf) != 0)
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
    return remain == 0 ? 0 : 1;
}
