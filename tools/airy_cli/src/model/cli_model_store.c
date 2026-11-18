// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_model_store.c
 * @brief 本地模型注册表的键值存储与声明片段收敛（0.1.21 D-6）。
 *
 * 数据面与 ops 的 airy-local 同源：注册表 $AIRY_HOME/local/models/installed.env，
 * 服务注册 $AIRY_HOME/local/registry/installed.env，声明片段
 * $AIRY_HOME/config/managed.d/llamacpp.env。写路一律「读全量 → 原位改单键 →
 * 临时文件 + fsync + rename」，只动本实例的 AIRYRT_MANAGED_* 键，保留同文件
 * 内其他实例/其他键（airy-local 的整文件覆写会静默抹掉第二实例，此处不复刻）。
 */
#include "cli_model_internal.h"

#include "airy_cli_cmd_internal.h"
#include "cli_internal.h"
#include "platform_misc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mr_ctx_init(mr_ctx_t *c)
{
    const char *h = getenv("AIRY_HOME");
    const char *p = getenv("AIRY_LOCAL_LLM_PORT");

    snprintf(c->home, sizeof(c->home), "%s", (h && h[0]) ? h : airy_home_dir());
    snprintf(c->reg, sizeof(c->reg), "%s/local/models/installed.env", c->home);
    snprintf(c->dir, sizeof(c->dir), "%s/local/models", c->home);
    snprintf(c->srv, sizeof(c->srv), "%s/local/registry/installed.env", c->home);
    snprintf(c->frag, sizeof(c->frag), "%s/config/managed.d/llamacpp.env", c->home);
    snprintf(c->yaml, sizeof(c->yaml), "%s/config/model.yaml", c->home);
    snprintf(c->port, sizeof(c->port), "%s", (p && p[0]) ? p : "11435");
}

void mr_free(mr_kv_t *kv)
{
    for (size_t i = 0; i < kv->n; i++)
        AIRY_FREE(kv->ln[i]);
    AIRY_FREE(kv->ln);
    kv->ln = NULL;
    kv->n = 0;
    kv->cap = 0;
}

static int mr_push(mr_kv_t *kv, const char *s)
{
    char *cp;

    if (kv->n == kv->cap) {
        size_t cap = kv->cap ? kv->cap * 2 : 16;
        char **ln = (char **)AIRY_MALLOC(cap * sizeof(*ln));

        if (!ln)
            return -1;
        for (size_t i = 0; i < kv->n; i++)
            ln[i] = kv->ln[i];
        AIRY_FREE(kv->ln);
        kv->ln = ln;
        kv->cap = cap;
    }
    cp = (char *)AIRY_MALLOC(strlen(s) + 1);
    if (!cp)
        return -1;
    AIRY_MEMCPY(cp, s, strlen(s) + 1);
    kv->ln[kv->n++] = cp;
    return 0;
}

/* 文件不存在视为空表（首次安装/默认安装均合法）；读取失败返回 -1。 */
int mr_load(mr_kv_t *kv, const char *path)
{
    FILE *f;
    char buf[MR_VAL_MAX];

    AIRY_MEMSET(kv, 0, sizeof(*kv));
    f = fopen(path, "r");
    if (!f)
        return 0;
    while (fgets(buf, sizeof(buf), f)) {
        size_t n = strlen(buf);

        while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
            buf[--n] = '\0';
        if (n == 0)
            continue;
        if (mr_push(kv, buf) != 0) {
            fclose(f);
            mr_free(kv);
            return -1;
        }
    }
    fclose(f);
    return 0;
}

static void mr_dir(const char *path, char *out, size_t cap)
{
    const char *s = strrchr(path, '/');

    out[0] = '\0';
    if (!s)
        return;
    if ((size_t)(s - path) < cap) {
        AIRY_MEMCPY(out, path, (size_t)(s - path));
        out[s - path] = '\0';
    }
}

/* 原子落盘：同目录临时文件 → fsync → rename，替换不是「读到半截」的状态。 */
int mr_save(const mr_kv_t *kv, const char *path)
{
    char dir[AIRY_PATH_MAX];
    char tmp[AIRY_PATH_MAX];
    FILE *f;
    int rc = 0;

    mr_dir(path, dir, sizeof(dir));
    if (dir[0] && airy_mkdir_p(dir) != 0)
        return -1;
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "w");
    if (!f)
        return -1;
    for (size_t i = 0; i < kv->n; i++) {
        if (fputs(kv->ln[i], f) == EOF || fputc('\n', f) == EOF) {
            rc = -1;
            break;
        }
    }
    if (rc == 0 && fflush(f) != 0)
        rc = -1;
#if !defined(_WIN32)
    if (rc == 0 && fsync(fileno(f)) != 0)
        rc = -1;
#endif
    if (fclose(f) != 0)
        rc = -1;
    if (rc == 0 && airy_file_rename(tmp, path) != 0) {
        airy_file_remove(tmp);
        rc = -1;
    }
    return rc;
}

/* 取键值：去两侧引号（与 airy_config_get 的引号语义对齐），空值视为未设置。 */
int mr_get(const mr_kv_t *kv, const char *key, char *buf, size_t cap)
{
    size_t kl = strlen(key);

    buf[0] = '\0';
    for (size_t i = 0; i < kv->n; i++) {
        const char *l = kv->ln[i];
        const char *v;
        size_t n, s = 0;

        if (strncmp(l, key, kl) != 0 || l[kl] != '=')
            continue;
        v = l + kl + 1;
        n = strlen(v);
        while (s < n && (v[s] == '"' || v[s] == '\''))
            s++;
        while (n > s && (v[n - 1] == '"' || v[n - 1] == '\''))
            n--;
        if (n - s >= cap)
            n = s + cap - 1;
        AIRY_MEMCPY(buf, v + s, n - s);
        buf[n - s] = '\0';
        return buf[0] != '\0';
    }
    return 0;
}

int mr_set(mr_kv_t *kv, const char *key, const char *val)
{
    size_t kl = strlen(key);
    char line[MR_VAL_MAX];

    snprintf(line, sizeof(line), "%s=%s", key, val);
    for (size_t i = 0; i < kv->n; i++) {
        char *l = kv->ln[i];

        if (strncmp(l, key, kl) != 0 || l[kl] != '=')
            continue;
        if (strcmp(l, line) == 0)
            return 0;
        l = (char *)AIRY_MALLOC(strlen(line) + 1);
        if (!l)
            return -1;
        AIRY_MEMCPY(l, line, strlen(line) + 1);
        AIRY_FREE(kv->ln[i]);
        kv->ln[i] = l;
        return 0;
    }
    return mr_push(kv, line);
}

void mr_del(mr_kv_t *kv, const char *key)
{
    size_t kl = strlen(key);
    size_t w = 0;

    for (size_t i = 0; i < kv->n; i++) {
        const char *l = kv->ln[i];

        if (strncmp(l, key, kl) == 0 && l[kl] == '=') {
            AIRY_FREE(kv->ln[i]);
            continue;
        }
        kv->ln[w++] = kv->ln[i];
    }
    kv->n = w;
}

/* 模型名参与键名与文件名，拒绝空、分隔符、空白与路径穿越。 */
int mr_name_ok(const char *name)
{
    if (!name || !name[0] || strlen(name) >= MR_NAME_MAX)
        return 0;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return 0;
    for (const char *p = name; *p; p++) {
        if (*p == '=' || *p == '/' || *p == '\\' || *p == ' ' || *p == '\t')
            return 0;
    }
    return 1;
}

size_t mr_names(const mr_kv_t *kv, char out[][MR_NAME_MAX], size_t cap)
{
    size_t n = 0;

    for (size_t i = 0; i < kv->n; i++) {
        const char *l = kv->ln[i];
        const char *p;
        const char *dot;
        size_t len;
        int dup = 0;

        if (strncmp(l, "model.", 6) != 0)
            continue;
        p = l + 6;
        dot = strstr(p, ".asset=");
        if (!dot)
            continue;
        len = (size_t)(dot - p);
        if (len == 0 || len >= MR_NAME_MAX)
            continue;
        for (size_t j = 0; j < n; j++)
            if (strlen(out[j]) == len && strncmp(out[j], p, len) == 0)
                dup = 1;
        if (!dup && n < cap) {
            AIRY_MEMCPY(out[n], p, len);
            out[n][len] = '\0';
            n++;
        }
    }
    return n;
}

int mr_asset(const mr_kv_t *kv, const char *name, char *out, size_t cap)
{
    char key[MR_NAME_MAX + 16];

    snprintf(key, sizeof(key), "model.%s.asset", name);
    return mr_get(kv, key, out, cap);
}

/* 与 airy_local_model_active 同语义：active 指向在位工件即用它，否则按注册表
 * 顺序取首个在位模型；两者都无则无服务模型。 */
int mr_active(const mr_ctx_t *c, const mr_kv_t *kv, char *out, size_t cap)
{
    char names[MR_MAX_MODELS][MR_NAME_MAX];
    char asset[MR_VAL_MAX];
    char path[AIRY_PATH_MAX];
    char act[MR_NAME_MAX];
    size_t n;

    if (mr_get(kv, "active_model", act, sizeof(act)) &&
        mr_asset(kv, act, asset, sizeof(asset))) {
        snprintf(path, sizeof(path), "%s/%s", c->dir, asset);
        if (airy_file_exists(path)) {
            snprintf(out, cap, "%s", act);
            return 1;
        }
    }
    n = mr_names(kv, names, MR_MAX_MODELS);
    for (size_t i = 0; i < n; i++) {
        if (!mr_asset(kv, names[i], asset, sizeof(asset)))
            continue;
        snprintf(path, sizeof(path), "%s/%s", c->dir, asset);
        if (airy_file_exists(path)) {
            snprintf(out, cap, "%s", names[i]);
            return 1;
        }
    }
    out[0] = '\0';
    return 0;
}

/* 声明片段按当前安装态收敛：只更新本实例的 5 个键，同文件内其他键原样保留；
 * 实例名归政策层——片段已声明 NAME 时沿用（ops 侧 airy-local 的声明），只在
 * 空片段上给中性缺省，机制核不写死厂商实例名；无服务模型时只清本实例键，
 * 清空后才删文件。 */
int mr_sync(const mr_ctx_t *c, const mr_kv_t *reg)
{
    static const char *keys[MR_DECL_KEYS] = {
        "AIRYRT_MANAGED_NAME", "AIRYRT_MANAGED_BIN", "AIRYRT_MANAGED_EP",
        "AIRYRT_MANAGED_ARGS", "AIRYRT_MANAGED_MEM"
    };
    mr_kv_t frag;
    mr_kv_t srv = { 0 };
    char server[MR_VAL_MAX];
    char name[MR_NAME_MAX];
    char asset[MR_VAL_MAX];
    char args[MR_VAL_MAX];
    char mem[64];
    char model[AIRY_PATH_MAX];
    char line[MR_VAL_MAX];
    char key[MR_NAME_MAX + 16];
    int rc = 0;

    if (mr_load(&frag, c->frag) != 0)
        return -1;
    AIRY_MEMSET(&srv, 0, sizeof(srv));
    if (mr_load(&srv, c->srv) != 0)
        rc = -1;
    if (rc == 0 &&
        (!mr_get(&srv, "server", server, sizeof(server)) ||
         !airy_file_exists(server)))
        server[0] = '\0';
    mr_free(&srv);
    if (rc != 0) {
        mr_free(&frag);
        return -1;
    }
    if (!server[0] || !mr_active(c, reg, name, sizeof(name)) ||
        !mr_asset(reg, name, asset, sizeof(asset))) {
        for (size_t i = 0; i < MR_DECL_KEYS; i++)
            mr_del(&frag, keys[i]);
        if (frag.n == 0)
            rc = airy_file_exists(c->frag) ? airy_file_remove(c->frag) : 0;
        else
            rc = mr_save(&frag, c->frag);
        mr_free(&frag);
        return rc;
    }
    snprintf(model, sizeof(model), "%s/%s", c->dir, asset);
    snprintf(key, sizeof(key), "model.%s.args", name);
    mr_get(reg, key, args, sizeof(args));
    if (!mr_get(&frag, "AIRYRT_MANAGED_NAME", line, sizeof(line)) || !line[0])
        AIRY_MEMCPY(line, "local_llm", sizeof("local_llm"));
    rc = mr_set(&frag, "AIRYRT_MANAGED_NAME", line);
    rc |= mr_set(&frag, "AIRYRT_MANAGED_BIN", server);
    snprintf(line, sizeof(line), "tcp:%s", c->port);
    rc |= mr_set(&frag, "AIRYRT_MANAGED_EP", line);
    snprintf(line, sizeof(line), "-m %s --host 127.0.0.1 --port %s%s%s",
             model, c->port, args[0] ? " " : "", args);
    rc |= mr_set(&frag, "AIRYRT_MANAGED_ARGS", line);
    snprintf(key, sizeof(key), "model.%s.mem", name);
    if (mr_get(reg, key, mem, sizeof(mem)))
        rc |= mr_set(&frag, "AIRYRT_MANAGED_MEM", mem);
    else
        mr_del(&frag, "AIRYRT_MANAGED_MEM");
    if (rc == 0)
        rc = mr_save(&frag, c->frag);
    mr_free(&frag);
    return rc;
}
