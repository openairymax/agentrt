// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_model_reg.c
 * @brief 本地模型注册表子命令（0.1.21 D-6）：list / use / remove。
 *
 * 数据面与 ops 的 airy-local 同源：注册表 $AIRY_HOME/local/models/installed.env，
 * 服务注册 $AIRY_HOME/local/registry/installed.env，声明片段
 * $AIRY_HOME/config/managed.d/llamacpp.env。写路一律「读全量 → 原位改单键 →
 * 临时文件 + fsync + rename」，只动本实例的 AIRYRT_MANAGED_* 键，保留同文件
 * 内其他实例/其他键（airy-local 的整文件覆写会静默抹掉第二实例，此处不复刻）。
 *
 * remove 默认 dry-run，只有 --yes 才落删除；被 active_model / model.yaml
 * 槽位或本地托管行引用的模型拒绝删除，需 --force 同时显式确认。list 合并
 * 「model.yaml 本地声明 + 注册表安装态」，并把注册表与 model.yaml 的失步
 * 显式报错并给修复命令，不静默取其一。
 */

#include "airy_cli_cmd_internal.h"
#include "cli_internal.h"
#include "platform_misc.h"
#include "platform_paths.h"
#include "yaml_minimal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#define MR_TTY() _isatty(_fileno(stdin))
#else
#include <unistd.h>
#define MR_TTY() isatty(fileno(stdin))
#endif

#define MR_NAME_MAX 128
#define MR_VAL_MAX 2048
#define MR_MAX_MODELS 64
#define MR_KEYS 6
#define MR_DECL_KEYS 5

typedef struct {
    char **ln;
    size_t n;
    size_t cap;
} mr_kv_t;

typedef struct {
    char name[MR_NAME_MAX];
    char id[MR_NAME_MAX];
    int local;
} mr_row_t;

typedef struct {
    int exists;
    int ok;
    char t2[MR_NAME_MAX];
    char t1f[MR_NAME_MAX];
    char t1p[MR_NAME_MAX];
    size_t nrow;
    mr_row_t row[MR_MAX_MODELS];
} mr_yaml_t;

typedef struct {
    char home[AIRY_PATH_MAX];
    char reg[AIRY_PATH_MAX];
    char dir[AIRY_PATH_MAX];
    char srv[AIRY_PATH_MAX];
    char frag[AIRY_PATH_MAX];
    char yaml[AIRY_PATH_MAX];
    char port[16];
} mr_ctx_t;

static void mr_ctx(mr_ctx_t *c)
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

static void mr_free(mr_kv_t *kv)
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
static int mr_load(mr_kv_t *kv, const char *path)
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
static int mr_save(const mr_kv_t *kv, const char *path)
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
static int mr_get(const mr_kv_t *kv, const char *key, char *buf, size_t cap)
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

static int mr_set(mr_kv_t *kv, const char *key, const char *val)
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

static void mr_del(mr_kv_t *kv, const char *key)
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
static int mr_name_ok(const char *name)
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

static size_t mr_names(const mr_kv_t *kv, char out[][MR_NAME_MAX], size_t cap)
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

static int mr_asset(const mr_ctx_t *c, const mr_kv_t *kv, const char *name,
                    char *out, size_t cap)
{
    char key[MR_NAME_MAX + 16];

    snprintf(key, sizeof(key), "model.%s.asset", name);
    return mr_get(kv, key, out, cap);
}

/* 与 airy_local_model_active 同语义：active 指向在位工件即用它，否则按注册表
 * 顺序取首个在位模型；两者都无则无服务模型。 */
static int mr_active(const mr_ctx_t *c, const mr_kv_t *kv, char *out, size_t cap)
{
    char names[MR_MAX_MODELS][MR_NAME_MAX];
    char asset[MR_VAL_MAX];
    char path[AIRY_PATH_MAX];
    char act[MR_NAME_MAX];
    size_t n;

    if (mr_get(kv, "active_model", act, sizeof(act)) &&
        mr_asset(c, kv, act, asset, sizeof(asset))) {
        snprintf(path, sizeof(path), "%s/%s", c->dir, asset);
        if (airy_file_exists(path)) {
            snprintf(out, cap, "%s", act);
            return 1;
        }
    }
    n = mr_names(kv, names, MR_MAX_MODELS);
    for (size_t i = 0; i < n; i++) {
        if (!mr_asset(c, kv, names[i], asset, sizeof(asset)))
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

static const char *mr_node(const struct yaml_node *n, const char *key)
{
    return yaml_as_string(yaml_get(n, key), "");
}

static void mr_yaml_load(const mr_ctx_t *c, mr_yaml_t *y)
{
    yaml_document_t *doc;
    struct yaml_node *rows;
    struct yaml_node *slots;
    size_t n;

    AIRY_MEMSET(y, 0, sizeof(*y));
    if (!airy_file_exists(c->yaml)) {
        y->ok = 1;
        return;
    }
    y->exists = 1;
    doc = yaml_create();
    if (!doc || yaml_parse_file(doc, c->yaml) != AIRY_SUCCESS) {
        yaml_destroy(doc);
        return;
    }
    y->ok = 1;
    slots = yaml_get(yaml_root(doc), "slots");
    snprintf(y->t2, sizeof(y->t2), "%s", mr_node(slots, "t2"));
    snprintf(y->t1f, sizeof(y->t1f), "%s", mr_node(slots, "t1f"));
    snprintf(y->t1p, sizeof(y->t1p), "%s", mr_node(slots, "t1p"));
    rows = yaml_get(yaml_root(doc), "models");
    n = yaml_size(rows);
    for (size_t i = 0; i < n && y->nrow < MR_MAX_MODELS; i++) {
        const struct yaml_node *row = yaml_get_index(rows, i);
        mr_row_t *r = &y->row[y->nrow];

        snprintf(r->name, sizeof(r->name), "%s", mr_node(row, "name"));
        snprintf(r->id, sizeof(r->id), "%s", mr_node(row, "model_id"));
        r->local = strcmp(mr_node(row, "mode"), "local") == 0 ||
                   strcmp(mr_node(row, "provision"), "managed") == 0;
        if (r->id[0])
            y->nrow++;
    }
    yaml_destroy(doc);
}

/* 工件名去目录与 .gguf 后缀，用于和 model.yaml 的 model_id 对齐。 */
static void mr_stem(const char *asset, char *out, size_t cap)
{
    const char *base = strrchr(asset, '/');
    size_t n;

    base = base ? base + 1 : asset;
    snprintf(out, cap, "%s", base);
    n = strlen(out);
    if (n > 5 && strcmp(out + n - 5, ".gguf") == 0)
        out[n - 5] = '\0';
}

static int mr_same(const char *a, const char *name, const char *stem)
{
    return a[0] && (strcmp(a, name) == 0 || strcmp(a, stem) == 0);
}

static size_t mr_refs(const mr_yaml_t *y, const mr_kv_t *kv, const char *name,
                      const char *asset, char *out, size_t cap)
{
    char active[MR_NAME_MAX];
    char stem[MR_NAME_MAX];
    size_t used = 0;
    int cnt = 0;

    out[0] = '\0';
    mr_stem(asset, stem, sizeof(stem));
    if (mr_get(kv, "active_model", active, sizeof(active)) &&
        mr_same(active, name, stem))
        cnt += snprintf(out + used, cap - used, " active_model");
    used = strlen(out);
    if (mr_same(y->t2, name, stem))
        cnt += snprintf(out + used, cap - used, " slots.t2");
    used = strlen(out);
    if (mr_same(y->t1f, name, stem))
        cnt += snprintf(out + used, cap - used, " slots.t1f");
    used = strlen(out);
    if (mr_same(y->t1p, name, stem))
        cnt += snprintf(out + used, cap - used, " slots.t1p");
    used = strlen(out);
    for (size_t i = 0; i < y->nrow; i++) {
        if (y->row[i].local && mr_same(y->row[i].id, name, stem))
            cnt += snprintf(out + used, cap - used, " model.yaml:%s",
                            y->row[i].name);
        used = strlen(out);
    }
    return (size_t)cnt;
}

/* 声明片段按当前安装态收敛：只更新本实例的 5 个键，同文件内其他键原样保留；
 * 实例名归政策层——片段已声明 NAME 时沿用（ops 侧 airy-local 的声明），只在
 * 空片段上给中性缺省，机制核不写死厂商实例名；无服务模型时只清本实例键，
 * 清空后才删文件。 */
static int mr_sync(const mr_ctx_t *c, const mr_kv_t *reg)
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
        !mr_asset(c, reg, name, asset, sizeof(asset))) {
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

static void mr_show(const char *name, const char *asset, int active, int json)
{
    char esc[2][MR_NAME_MAX * 2];

    if (json) {
        cli_json_esc(esc[0], sizeof(esc[0]), name);
        cli_json_esc(esc[1], sizeof(esc[1]), asset);
        cli_outf("{\"schema\":1,\"name\":\"%s\",\"state\":\"installed\","
                 "\"asset\":\"%s\",\"active\":%d}\n",
                 esc[0], esc[1], active);
        return;
    }
    cli_outf("%s [installed] %s%s\n", name, asset, active ? " *active" : "");
}

static int mr_list(const mr_ctx_t *c, int json)
{
    mr_kv_t reg;
    mr_yaml_t y;
    char names[MR_MAX_MODELS][MR_NAME_MAX];
    char asset[MR_VAL_MAX];
    char stem[MR_NAME_MAX];
    char act[MR_NAME_MAX];
    size_t n;
    int active, bad = 0;

    if (mr_load(&reg, c->reg) != 0) {
        cli_outf("airy_cli: 无法读取 %s\n", c->reg);
        return 2;
    }
    mr_yaml_load(c, &y);
    if (!y.ok) {
        cli_outf("airy_cli: %s 解析失败；两源状态不可判定，不静默取注册表\n",
                 c->yaml);
        mr_free(&reg);
        return 2;
    }
    n = mr_names(&reg, names, MR_MAX_MODELS);
    active = mr_active(c, &reg, act, sizeof(act));
    for (size_t i = 0; i < n; i++) {
        mr_asset(c, &reg, names[i], asset, sizeof(asset));
        mr_show(names[i], asset, active && strcmp(act, names[i]) == 0, json);
    }
    for (size_t i = 0; i < y.nrow; i++) {
        int seen = 0;

        if (!y.row[i].local)
            continue;
        mr_stem(y.row[i].id, stem, sizeof(stem));
        for (size_t j = 0; j < n; j++)
            if (mr_same(stem, names[j], names[j]))
                seen = 1;
        if (seen)
            continue;
        if (json)
            cli_outf("{\"schema\":1,\"name\":\"%s\",\"state\":\"declared\","
                     "\"asset\":\"\",\"active\":0}\n", y.row[i].id);
        else
            cli_outf("%s [declared] model.yaml 本地声明（未安装）\n", y.row[i].id);
    }
    if (n > 0 && !active) {
        cli_outf("inconsistent: 注册表有 %zu 个在位模型但 active_model 无效\n", n);
        cli_outf("repair: airy_cli model use %s\n", names[0]);
        bad = 1;
    }
    for (size_t i = 0; active && i < y.nrow; i++) {
        if (!y.row[i].local)
            continue;
        mr_asset(c, &reg, act, asset, sizeof(asset));
        mr_stem(asset, stem, sizeof(stem));
        if (!mr_same(y.row[i].id, act, stem)) {
            cli_outf("inconsistent: model.yaml 本地行 model_id=%s 与注册表 "
                     "active=%s（%s）不一致\n", y.row[i].id, act, asset);
            cli_outf("repair: 把 model.yaml 该行 model_id 改为 %s，"
                     "或 airy_cli model use %s\n", stem, y.row[i].id);
            bad = 1;
        }
    }
    mr_free(&reg);
    return bad ? 2 : 0;
}

static int mr_use(const mr_ctx_t *c, const char *name)
{
    mr_kv_t reg;
    char asset[MR_VAL_MAX];
    char path[AIRY_PATH_MAX];

    if (!mr_name_ok(name)) {
        cli_outf("airy_cli: 非法模型名 %s\n", name ? name : "");
        return 2;
    }
    if (mr_load(&reg, c->reg) != 0) {
        cli_outf("airy_cli: 无法读取 %s\n", c->reg);
        return 2;
    }
    if (!mr_asset(c, &reg, name, asset, sizeof(asset))) {
        cli_outf("airy_cli: 模型 %s 未安装；先 airy-local model install %s\n",
                 name, name);
        mr_free(&reg);
        return 2;
    }
    snprintf(path, sizeof(path), "%s/%s", c->dir, asset);
    if (!airy_file_exists(path)) {
        cli_outf("airy_cli: 模型 %s 工件缺失（%s）；重新 airy-local model install\n",
                 name, path);
        mr_free(&reg);
        return 2;
    }
    if (mr_set(&reg, "active_model", name) != 0 || mr_save(&reg, c->reg) != 0) {
        cli_outf("airy_cli: active_model 原子写入失败（%s）\n", c->reg);
        mr_free(&reg);
        return 2;
    }
    if (mr_sync(c, &reg) != 0) {
        cli_outf("airy_cli: 声明片段重建失败（%s）\n", c->frag);
        mr_free(&reg);
        return 2;
    }
    cli_outf("active model: %s (%s)\n", name, asset);
    mr_free(&reg);
    return 0;
}

static int mr_confirm(const char *name)
{
    char ans[16];

    if (!MR_TTY())
        return 1;
    cli_outf("删除模型 %s 及其工件？[y/N] ", name);
    if (!fgets(ans, sizeof(ans), stdin))
        return 0;
    return ans[0] == 'y' || ans[0] == 'Y';
}

static int mr_remove(const mr_ctx_t *c, const char *name, int yes, int force)
{
    mr_kv_t reg;
    mr_yaml_t y;
    char asset[MR_VAL_MAX];
    char path[AIRY_PATH_MAX];
    char stems[MR_NAME_MAX];
    char refs[256];
    char key[MR_NAME_MAX + 16];
    static const char *sub[MR_KEYS] = { "asset", "size", "sha256",
                                        "args", "mem", "installed_at" };
    int has_ref;

    if (!mr_name_ok(name)) {
        cli_outf("airy_cli: 非法模型名 %s\n", name ? name : "");
        return 2;
    }
    if (mr_load(&reg, c->reg) != 0) {
        cli_outf("airy_cli: 无法读取 %s\n", c->reg);
        return 2;
    }
    if (!mr_asset(c, &reg, name, asset, sizeof(asset))) {
        cli_outf("airy_cli: 未安装：%s（无需删除）\n", name);
        mr_free(&reg);
        return 0;
    }
    mr_yaml_load(c, &y);
    if (!y.ok) {
        cli_outf("airy_cli: %s 解析失败；引用关系不可判定，拒绝删除\n", c->yaml);
        mr_free(&reg);
        return 2;
    }
    mr_stem(asset, stems, sizeof(stems));
    has_ref = mr_refs(&y, &reg, name, asset, refs, sizeof(refs)) > 0;
    snprintf(path, sizeof(path), "%s/%s", c->dir, asset);
    cli_outf("remove: model=%s asset=%s\n", name, path);
    cli_outf("  %s 删除工件\n", airy_file_exists(path) ? "将" : "工件缺失，跳过");
    cli_outf("  将删除注册表 model.%s.{asset,size,sha256,args,mem,installed_at}\n",
             name);
    if (has_ref)
        cli_outf("引用: %s\n", refs);
    if (!yes) {
        cli_outf("dry-run: 未做任何变更（确认后加 --yes 执行）\n");
        mr_free(&reg);
        return 0;
    }
    if (has_ref && !force) {
        cli_outf("airy_cli: 模型被引用，拒绝删除；确认影响后加 --force\n");
        mr_free(&reg);
        return 2;
    }
    if (!mr_confirm(name)) {
        cli_outf("已取消，未做任何变更\n");
        mr_free(&reg);
        return 0;
    }
    if (airy_file_exists(path) && airy_file_remove(path) != 0) {
        cli_outf("airy_cli: 工件删除失败 %s\n", path);
        mr_free(&reg);
        return 2;
    }
    for (size_t i = 0; i < MR_KEYS; i++) {
        snprintf(key, sizeof(key), "model.%s.%s", name, sub[i]);
        mr_del(&reg, key);
    }
    {
        char act[MR_NAME_MAX];

        if (mr_get(&reg, "active_model", act, sizeof(act)) &&
            (strcmp(act, name) == 0 || strcmp(act, stems) == 0))
            mr_del(&reg, "active_model");
    }
    if (mr_save(&reg, c->reg) != 0 || mr_sync(c, &reg) != 0) {
        cli_outf("airy_cli: 注册表/声明片段写回失败\n");
        mr_free(&reg);
        return 2;
    }
    cli_outf("removed model: %s (%s)\n", name, asset);
    mr_free(&reg);
    return 0;
}

int cli_model_reg(int argc, char *argv[])
{
    const char *sub = argc > 0 ? argv[0] : NULL;
    mr_ctx_t c;

    if (!sub) {
        cli_outf("用法: airy_cli model list [--json] | use <name> | "
                 "remove <name> [--yes] [--force]\n");
        return 2;
    }
    mr_ctx(&c);
    if (strcmp(sub, "list") == 0) {
        int json = g_cli_json_mode;

        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--json") == 0)
                json = 1;
            else {
                cli_outf("airy_cli: model list 不认识参数 %s\n", argv[i]);
                return 2;
            }
        }
        return mr_list(&c, json);
    }
    if (strcmp(sub, "use") == 0) {
        if (argc != 2) {
            cli_outf("用法: airy_cli model use <name>\n");
            return 2;
        }
        return mr_use(&c, argv[1]);
    }
    if (strcmp(sub, "remove") == 0) {
        int yes = 0, force = 0;
        const char *name = NULL;

        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "-y") == 0)
                yes = 1;
            else if (strcmp(argv[i], "--force") == 0)
                force = 1;
            else if (strcmp(argv[i], "--dry-run") == 0)
                yes = 0;
            else if (argv[i][0] == '-') {
                cli_outf("airy_cli: model remove 不认识参数 %s\n", argv[i]);
                return 2;
            } else if (!name) {
                name = argv[i];
            } else {
                cli_outf("airy_cli: model remove 只接受一个模型名\n");
                return 2;
            }
        }
        if (!name) {
            cli_outf("用法: airy_cli model remove <name> [--yes] [--force] "
                     "[--dry-run]\n");
            return 2;
        }
        return mr_remove(&c, name, yes, force);
    }
    cli_outf("用法: airy_cli model list [--json] | use <name> | "
             "remove <name> [--yes] [--force]\n");
    return 2;
}
