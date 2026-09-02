/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_manifest.c
 * @brief deps.txt manifest 解析实现（从 main.c 拆分，M3 x-cutting-b）。
 */

#include "depgraph_manifest.h"

#include <stdio.h>
#include <string.h>

int dg_parse_manifest(const char *path, dg_manifest_t *mf, bool allow_unknown)
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
                if (allow_unknown)
                    continue; /* 链接白名单模式：库名不必是图节点 */
                dg_eprintf("airy_depgraph: unknown dependency '%s' of '%s'\n", dom->deps[j],
                           dom->name);
                return -1;
            }
        }
    }
    return 0;
}
