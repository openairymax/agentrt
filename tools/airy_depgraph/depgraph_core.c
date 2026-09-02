/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_core.c
 * @brief airy_depgraph 合规输出工具与共享查询实现（M3 x-cutting-b 拆分）。
 */

#include "depgraph_core.h"

void dg_printf(const char *fmt, ...)
{
    char buf[AIRY_DG_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
}

void dg_eprintf(const char *fmt, ...)
{
    char buf[AIRY_DG_BUF];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stderr);
}

size_t dg_name_to_vertex(const dg_manifest_t *mf, const char *name)
{
    for (size_t i = 0; i < mf->domain_count; i++) {
        if (strcmp(mf->domains[i].name, name) == 0)
            return i;
    }
    return SIZE_MAX;
}

const char *dg_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

bool dg_domain_has_dep(const dg_domain_t *dom, const char *dep)
{
    for (size_t i = 0; i < dom->dep_count; i++) {
        if (strcmp(dom->deps[i], dep) == 0)
            return true;
    }
    return false;
}
