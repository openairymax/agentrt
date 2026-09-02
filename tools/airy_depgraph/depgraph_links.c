/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_links.c
 * @brief 链接白名单门禁实现（从 main.c 拆分，M3 x-cutting-b）。
 */

#include "depgraph_links.h"

#include <stdio.h>
#include <string.h>

/* 项目内部库前缀：白名单外出现即视为未登记项目库（fail-closed） */
static bool dg_is_project_lib(const char *lib)
{
    static const char *const k_prefix[] = {"airy_", "libairy_", "coreloopthree",
                                           "cognition", "svc_", "daemon_", NULL};
    for (size_t i = 0; k_prefix[i] != NULL; i++) {
        if (strncmp(lib, k_prefix[i], strlen(k_prefix[i])) == 0)
            return true;
    }
    return false;
}

/* 库名归一化：去 -l 前缀 / 目录路径 / lib 前缀 / .so|.a|.dylib|.dll 后缀 */
static void dg_lib_basename(const char *src, char *out, size_t outsz)
{
    const char *base = src;
    if (base[0] == '-' && base[1] == 'l')
        base += 2;
    const char *slash = strrchr(base, '/');
    if (slash)
        base = slash + 1;
    size_t len = strlen(base);
    size_t start = 0;
    if (len > 3 && strncmp(base, "lib", 3) == 0)
        start = 3;
    size_t end = len;
    static const char *const k_suffix[] = {".so", ".a", ".dylib", ".dll", NULL};
    for (size_t i = 0; k_suffix[i] != NULL; i++) {
        size_t sl = strlen(k_suffix[i]);
        if (end > start + sl && strncmp(base + end - sl, k_suffix[i], sl) == 0) {
            end -= sl;
            break;
        }
    }
    size_t n = end - start;
    if (n >= outsz)
        n = outsz - 1;
    for (size_t i = 0; i < n; i++)
        out[i] = base[start + i];
    out[n] = '\0';
}

int dg_check_links(const dg_manifest_t *wl, const dg_manifest_t *actual)
{
    /* 项目库全集 = 白名单所有目标允许库的并集 */
    char univ[AIRY_DG_MAX_DOMAIN * AIRY_DG_MAX_DEP][64];
    size_t ucount = 0;
    for (size_t i = 0; i < wl->domain_count; i++) {
        for (size_t j = 0; j < wl->domains[i].dep_count; j++) {
            const char *lib = wl->domains[i].deps[j];
            bool dup = false;
            for (size_t k = 0; k < ucount; k++) {
                if (strcmp(univ[k], lib) == 0) {
                    dup = true;
                    break;
                }
            }
            if (!dup && ucount < AIRY_DG_MAX_DOMAIN * AIRY_DG_MAX_DEP)
                snprintf(univ[ucount++], sizeof(univ[0]), "%s", lib);
        }
    }

    int violations = 0;
    for (size_t i = 0; i < actual->domain_count; i++) {
        const dg_domain_t *tgt = &actual->domains[i];
        size_t wl_idx = dg_name_to_vertex(wl, tgt->name);
        if (wl_idx == SIZE_MAX)
            continue; /* 未登记目标：CMake 侧仅对登记目标生成 actual */
        const dg_domain_t *allowed = &wl->domains[wl_idx];
        for (size_t j = 0; j < tgt->dep_count; j++) {
            char lib[64];
            dg_lib_basename(tgt->deps[j], lib, sizeof(lib));
            if (lib[0] == '\0' || lib[0] == '$')
                continue; /* 空项 / 生成器表达式 */
            bool in_univ = false;
            for (size_t k = 0; k < ucount; k++) {
                if (strcmp(univ[k], lib) == 0) {
                    in_univ = true;
                    break;
                }
            }
            if (in_univ) {
                if (!dg_domain_has_dep(allowed, lib)) {
                    dg_eprintf("airy_depgraph: link violation: '%s' links '%s' not in whitelist\n",
                               tgt->name, lib);
                    violations++;
                }
            } else if (dg_is_project_lib(lib)) {
                dg_eprintf("airy_depgraph: link violation: '%s' links undeclared project lib '%s'\n",
                           tgt->name, lib);
                violations++;
            }
        }
    }
    if (violations > 0) {
        dg_eprintf("airy_depgraph: %d link whitelist violation(s), build blocked (fail-closed)\n",
                   violations);
        return 2;
    }
    return 0;
}
