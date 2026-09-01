/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file main.c
 * @brief 依赖图校验工具（0.1.6 P1-2 + P1-1 原子化门禁）：commons 子域依赖
 *        声明 -> DAG -> 拓扑排序 + 环检测 + 报告 + include 漂移门禁。
 *
 * 复用 taskflow 图引擎（graph_engine）承载图结构与遍历原语；Kahn 拓扑
 * 排序与 DFS 三色环路径输出在本文件实现。构建期门禁 fail-closed：
 *   退出码 0 = 无环（输出拓扑序报告）
 *   退出码 1 = 存在环（输出全部环路径）
 *   退出码 2 = manifest 解析错误 / include 漂移（公共头引用了未声明的
 *              跨子域公共头，或声明的依赖在公共头中无实际 include）
 *
 * include 漂移门禁（--root 提供 commons 根时启用）：deps.txt 头部声明
 * "公共头引用的依赖必须在此声明，否则 include 漂移无法被门禁捕获"——
 * 本工具据实双向校验：① 子域公共头 include 了其他子域公共头而未声明 →
 * 错误；② 声明的依赖在公共头中零实际 include → 警告（陈旧边）。
 *
 * 合规约束（AIRY_COMPLIANCE_STRICT）：全局 poison unsafe 函数
 * （memset/strtok/printf 等），本文件全部使用合规替代
 * （snprintf+fputs / 显式初始化 / 手写 tokenizer / airy_dirent 目录遍历）。
 *
 * 用法:
 *   airy_depgraph [-o <report文件>] [--root <commons根目录>] <manifest>
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "airy_dirent.h"
#include "graph_engine.h"
#include "taskflow.h"

/* ------------------------------------------------------------------ */

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
    char owner[64];   /* 归属子域 */
    char base[96];    /* 头文件 basename */
} dg_header_t;

typedef struct {
    char root[512];
    dg_header_t headers[AIRY_DG_MAX_HEADERS];
    size_t header_count;
} dg_index_t;

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

/* 取路径最后一个 '/' 之后的部分（无 '/' 时取原名） */
static const char *dg_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static bool dg_domain_has_dep(const dg_domain_t *dom, const char *dep)
{
    for (size_t i = 0; i < dom->dep_count; i++) {
        if (strcmp(dom->deps[i], dep) == 0)
            return true;
    }
    return false;
}

/* ------------------------------------------------------------------
 * include 漂移门禁：头文件索引（basename -> 归属子域；冲突则 skip）
 * ------------------------------------------------------------------ */

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

/* 冲突消歧在 dg_owner_of 中按 include 搜索顺序处理（utils 子域优先于
 * 契约层；两个 utils 子域重名则无法判定，直接忽略），此处不再预标记。 */

/* 依据 manifest 的节点名建立索引（utils/<域>/、platform/include、
 * include/ + include/airymax 契约层）
 * 0.1.9 0d 扁平化：utils/<域>/ 平铺，公共头目录即 utils/<域> 本身 */
static void dg_index_build(dg_index_t *idx, const dg_manifest_t *mf)
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

/* 解析 include token 的归属子域：
 * 1) 带路径前缀：airymax/ -> 契约层("include")；<子域名>/ -> 该子域；
 *    其余前缀（sys/ net/ openssl/ curl/ ...）视为系统/第三方头，忽略；
 * 2) 裸 basename：命中系统头清单则忽略；否则索引查全部归属，重名冲突
 *    时优先非契约子域（构建 include 搜索顺序中 utils/<域>/include 先于
 *    commons/include，故裸名如 error.h 解析到 utils/error 而非
 *    airymax/error.h）。两个 utils 子域重名无法判定，返回 NULL。 */
static const char *dg_owner_of(const dg_manifest_t *mf, const dg_index_t *idx,
                               const char *token)
{
    /* 跳过相对路径前缀（../ 或 ./ 重复段，如 ../../observability/include/logger.h） */
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

/* 扫描子域全部公共头，返回漂移错误总数 */
static size_t dg_scan_domain_includes(dg_index_t *idx, const dg_manifest_t *mf,
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

/* ------------------------------------------------------------------
 * manifest 解析（手写 tokenizer，strtok 被 poison）
 * ------------------------------------------------------------------ */
static int dg_parse_manifest(const char *path, dg_manifest_t *mf, bool allow_unknown)
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

/* ------------------------------------------------------------------
 * 链接白名单门禁（M1-1b，2026-09-02）：
 *   --links <whitelist> --actual <actual> 成对启用。
 *   whitelist: "目标: 允许链接的项目内库..."（link-whitelist.txt 单一权威）
 *   actual   : CMake 侧 get_target_property(LINK_LIBRARIES) 生成的实际链接
 * 规则（fail-closed）：
 *   - 实际链接库在白名单"库全集"内但不在该目标允许集 → 链接越权，fail
 *   - 实际链接库不在库全集但以项目库前缀开头 → 未登记项目库，fail
 *   - 系统/第三方库（cjson/microhttpd/... 或路径形式）→ 忽略
 * ------------------------------------------------------------------ */

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

/* 链接白名单校验；返回 0=通过，2=存在越权/未登记（fail-closed） */
static int dg_check_links(const dg_manifest_t *wl, const dg_manifest_t *actual)
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
