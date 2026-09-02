/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file depgraph_drift.c
 * @brief include 漂移门禁实现（从 main.c 拆分，M3 x-cutting-b）。
 */

#include "depgraph_drift.h"

#include "airy_dirent.h"

#include <stdio.h>
#include <string.h>

/* 取子域公共头目录（与 dg_index_build 同规则） */
static void dg_domain_header_dir(const dg_index_t *idx, const char *name, char *out,
                                 size_t outsz)
{
    if (strcmp(name, "platform") == 0)
        snprintf(out, outsz, "%s/platform/include", idx->root);
    else if (strcmp(name, "include") == 0)
        snprintf(out, outsz, "%s/include", idx->root);
    else
        snprintf(out, outsz, "%s/utils/%s", idx->root, name);
}

/* 扫描单个目录下 *.h，owner 为该目录归属子域。
 * 0.1.9 0d 扁平化：utils/<域>/ 平铺后，公共头与内部头同目录；内部头按
 * 约定以 _internal.h 后缀命名，不属于公共契约，跳过不参与漂移门禁。 */
static void dg_index_scan_dir(dg_index_t *idx, const char *dir, const char *owner)
{
    DIR *dp = opendir(dir);
    if (!dp)
        return;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n < 3 || de->d_name[n - 1] != 'h' || de->d_name[n - 2] != '.')
            continue;
        /* 0d: _internal.h 内部头跳过（不构成公共 API 契约） */
        static const char k_internal_suffix[] = "_internal.h";
        size_t isuf = sizeof(k_internal_suffix) - 1;
        if (n >= isuf && strcmp(de->d_name + n - isuf, k_internal_suffix) == 0)
            continue;
        if (idx->header_count >= AIRY_DG_MAX_HEADERS)
            break;
        dg_header_t *h = &idx->headers[idx->header_count];
        snprintf(h->base, sizeof(h->base), "%s", de->d_name);
        snprintf(h->owner, sizeof(h->owner), "%s", owner);
        h->base[sizeof(h->base) - 1] = '\0';
        idx->header_count++;
    }
    closedir(dp);
}

void dg_index_build(dg_index_t *idx, const dg_manifest_t *mf)
{
    char path[AIRY_DG_BUF];
    for (size_t i = 0; i < mf->domain_count; i++) {
        const char *name = mf->domains[i].name;
        if (strcmp(name, "platform") == 0) {
            snprintf(path, sizeof(path), "%s/platform/include", idx->root);
            dg_index_scan_dir(idx, path, name);
        } else if (strcmp(name, "include") == 0) {
            snprintf(path, sizeof(path), "%s/include/airymax", idx->root);
            dg_index_scan_dir(idx, path, name);
            snprintf(path, sizeof(path), "%s/include", idx->root);
            dg_index_scan_dir(idx, path, name);
        } else {
            snprintf(path, sizeof(path), "%s/utils/%s", idx->root, name);
            dg_index_scan_dir(idx, path, name);
        }
    }
}

/* 解析一个 #include 行：返回 token 写入 buf；无效返回 NULL */
static const char *dg_parse_include_line(const char *line, char *buf, size_t bufsz)
{
    const char *p = line;
    while (*p == ' ' || *p == '\t')
        p++;
    if (strncmp(p, "#include", 8) != 0)
        return NULL;
    p += 8;
    while (*p == ' ' || *p == '\t')
        p++;
    char open_ch = *p;
    if (open_ch != '"' && open_ch != '<')
        return NULL;
    p++;
    const char *close_ch = (open_ch == '"') ? strchr(p, '"') : strchr(p, '>');
    if (!close_ch || close_ch == p)
        return NULL;
    size_t len = (size_t)(close_ch - p);
    if (len >= bufsz)
        len = bufsz - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = p[i];
    buf[len] = '\0';
    return buf;
}

/* 系统/第三方头 basename 排除清单：裸名且命中即视为非项目头（如 MSVC
 * <io.h>、POSIX <unistd.h>、标准库头），避免 Windows 兼容层误报。 */
static bool dg_is_system_basename(const char *base)
{
    static const char *const k_system[] = {
        "io.h",       "unistd.h",   "direct.h",   "process.h",  "windows.h",
        "winsock2.h", "ws2tcpip.h", "fcntl.h",    "stdio.h",    "stdlib.h",
        "string.h",   "strings.h",  "ctype.h",    "errno.h",    "locale.h",
        "stdarg.h",   "stdbool.h",  "stddef.h",   "stdint.h",   "time.h",
        "wchar.h",    "wctype.h",   "pthread.h",  "semaphore.h", "sched.h",
        "malloc.h",   "dbghelp.h",  "execinfo.h", "inttypes.h", "stdatomic.h",
        "assert.h",   "signal.h",   "limits.h",   "float.h",    "math.h",
        "setjmp.h",   "regex.h",    "libgen.h",   "alloca.h",   "stdbool.h",
        NULL};
    for (size_t i = 0; k_system[i] != NULL; i++) {
        if (strcmp(k_system[i], base) == 0)
            return true;
    }
    return false;
}

/* 解析 include token 的归属子域（与 main.c 原 dg_owner_of 语义一致） */
static const char *dg_owner_of(const dg_manifest_t *mf, const dg_index_t *idx,
                               const char *token)
{
    /* 跳过相对路径前缀（../ 或 ./ 重复段） */
    while ((token[0] == '.' && token[1] == '.' && token[2] == '/') ||
           (token[0] == '.' && token[1] == '/'))
        token += (token[0] == '.' && token[1] == '.' && token[2] == '/') ? 3 : 2;
    const char *slash = strchr(token, '/');
    if (slash) {
        if (strncmp(token, "airymax/", 8) == 0)
            return "include";
        for (size_t i = 0; i < mf->domain_count; i++) {
            size_t len = strlen(mf->domains[i].name);
            if (strncmp(token, mf->domains[i].name, len) == 0 && token[len] == '/')
                return mf->domains[i].name;
        }
        return NULL; /* sys/ netinet/ openssl/ 等系统或第三方路径 */
    }
    const char *base = dg_basename(token);
    if (dg_is_system_basename(base))
        return NULL;
    const char *found = NULL;
    bool multi = false;
    for (size_t i = 0; i < idx->header_count; i++) {
        if (strcmp(idx->headers[i].base, base) != 0)
            continue;
        if (!found) {
            found = idx->headers[i].owner;
        } else if (strcmp(found, idx->headers[i].owner) != 0) {
            if (strcmp(found, "include") == 0) {
                found = idx->headers[i].owner; /* 契约层被 utils 子域优先 */
            } else if (strcmp(idx->headers[i].owner, "include") != 0) {
                multi = true; /* 两个 utils 子域重名，无法判定 */
            }
        }
    }
    return multi ? NULL : found;
}

/* 解析并登记一个头文件的全部 #include；返回漂移错误数 */
static size_t dg_scan_header_file(dg_index_t *idx, const dg_manifest_t *mf,
                                  const dg_domain_t *dom, const char *dir,
                                  const char *file, size_t *used_dep)
{
    char path[AIRY_DG_BUF];
    snprintf(path, sizeof(path), "%s/%s", dir, file);
    FILE *fp = fopen(path, "r");
    if (!fp)
        return 0;
    char line[AIRY_DG_MAX_LINE];
    size_t drift = 0;
    while (fgets(line, sizeof(line), fp)) {
        char token[AIRY_DG_MAX_LINE];
        if (!dg_parse_include_line(line, token, sizeof(token)))
            continue;
        const char *owner = dg_owner_of(mf, idx, token);
        if (!owner || strcmp(owner, dom->name) == 0)
            continue; /* 系统头 / 无法判定 / 自域头 */
        if (!dg_domain_has_dep(dom, owner)) {
            dg_eprintf("airy_depgraph: include drift: %s/%s -> %s (declared? no)\n",
                       dom->name, file, token);
            drift++;
        }
        /* 登记已用的声明边（防陈旧边） */
        for (size_t d = 0; d < dom->dep_count; d++) {
            if (strcmp(dom->deps[d], owner) == 0)
                used_dep[d] = 1;
        }
    }
    fclose(fp);
    return drift;
}

size_t dg_scan_domain_includes(dg_index_t *idx, const dg_manifest_t *mf,
                               const dg_domain_t *dom, size_t *used_dep)
{
    char dir[AIRY_DG_BUF];
    dg_domain_header_dir(idx, dom->name, dir, sizeof(dir));
    DIR *dp = opendir(dir);
    if (!dp)
        return 0; /* 无公共头目录（如纯实现域）不校验 */
    size_t drift = 0;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        size_t n = strlen(de->d_name);
        if (n < 3 || de->d_name[n - 1] != 'h' || de->d_name[n - 2] != '.')
            continue;
        /* 0d: _internal.h 内部头跳过（与 dg_index_scan_dir 同规则） */
        static const char k_internal_suffix[] = "_internal.h";
        size_t isuf = sizeof(k_internal_suffix) - 1;
        if (n >= isuf && strcmp(de->d_name + n - isuf, k_internal_suffix) == 0)
            continue;
        drift += dg_scan_header_file(idx, mf, dom, dir, de->d_name, used_dep);
    }
    closedir(dp);
    return drift;
}
