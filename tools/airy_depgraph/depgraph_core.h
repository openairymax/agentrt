/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_core.h
 * @brief airy_depgraph 共享类型/常量/工具（x-cutting-b：848 行 main.c 拆分）。
 *
 * M3 (0.1.9 §4.2 持续横切)：主文件按单一职责拆为
 *   depgraph_core.h     — 本文件：类型/常量/合规输出工具
 *   depgraph_manifest   — deps.txt 解析
 *   depgraph_drift      — include 漂移索引与双向校验
 *   depgraph_graph      — Kahn 拓扑 + DFS 环检测
 *   depgraph_links      — 链接白名单门禁
 *   main.c              — 入口 + 报告输出
 */

#ifndef AIRY_DG_CORE_H
#define AIRY_DG_CORE_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AIRY_DG_MAX_DOMAIN 96
#define AIRY_DG_MAX_DEP 64
#define AIRY_DG_MAX_LINE 512
#define AIRY_DG_BUF 2048
#define AIRY_DG_MAX_HEADERS 1024
#define AIRY_DG_MAX_INC 256

typedef struct {
    char name[64];
    char deps[AIRY_DG_MAX_DEP][64];
    size_t dep_count;
} dg_domain_t;

typedef struct {
    dg_domain_t domains[AIRY_DG_MAX_DOMAIN];
    size_t domain_count;
} dg_manifest_t;

typedef struct {
    char owner[64]; /* 归属子域 */
    char base[96];  /* 头文件 basename */
} dg_header_t;

typedef struct {
    char root[512];
    dg_header_t headers[AIRY_DG_MAX_HEADERS];
    size_t header_count;
} dg_index_t;

/* 合规输出：snprintf + fputs（printf/fprintf 被 poison） */
void dg_printf(const char *fmt, ...);
void dg_eprintf(const char *fmt, ...);

size_t dg_name_to_vertex(const dg_manifest_t *mf, const char *name);

/* 取路径最后一个 '/' 之后的部分（无 '/' 时取原名） */
const char *dg_basename(const char *path);

bool dg_domain_has_dep(const dg_domain_t *dom, const char *dep);

#endif /* AIRY_DG_CORE_H */
