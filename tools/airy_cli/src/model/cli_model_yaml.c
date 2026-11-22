// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_model_yaml.c
 * @brief model.yaml 声明视图与引用判定（0.1.21 D-6）。
 *
 * 只读不写：把 model.yaml 的 slots 与本地托管行投影成 mr_yaml_t，供 list 的
 * 「声明 vs 安装态」对账和 remove 的引用拒绝使用。解析失败必须让上层显式
 * 报错，不静默退化成空视图——两源失步被静默取其一正是要避免的故障。
 */
#include "cli_model_internal.h"

#include "airy_cli_cmd_internal.h"
#include "cli_internal.h"
#include "platform_misc.h"
#include "yaml_minimal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mr_node(const struct yaml_node *n, const char *key)
{
    return yaml_as_string(yaml_get(n, key), "");
}

/* token 字面量解析（4k/16k/128k/1M/2M 与纯十进制）；0 = 非法或未声明。
 * 与 llm_d 的 svc_tokens_parse 同口径，CLI 侧不链接 daemons 故本地持有。 */
static int mr_tokens(const char *s)
{
    char *end = NULL;
    long v;
    long mul = 1;

    if (!s || !*s)
        return 0;
    v = strtol(s, &end, 10);
    if (end == s || v <= 0)
        return 0;
    if (*end == 'k' || *end == 'K')
        mul = 1024;
    else if (*end == 'm' || *end == 'M')
        mul = 1024 * 1024;
    else if (*end)
        return 0;
    if (v > 2147483647L / mul)
        return 0;
    return (int)(v * mul);
}

void mr_yaml_load(const mr_ctx_t *c, mr_yaml_t *y)
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
        r->ctx = mr_tokens(mr_node(row, "context_window"));
        if (r->id[0])
            y->nrow++;
    }
    yaml_destroy(doc);
}

/* 托管工件对应的 context_window（token 数）；未声明/解析失败返回 0，
 * 由调用方决定「不写 -c」而不是猜一个窗口。 */
int mr_ctx_tokens(const mr_ctx_t *c, const char *asset)
{
    mr_yaml_t y;
    char stem[MR_NAME_MAX];
    int ctx = 0;

    mr_stem(asset, stem, sizeof(stem));
    mr_yaml_load(c, &y);
    if (!y.ok)
        return 0;
    for (size_t i = 0; i < y.nrow; i++) {
        if (y.row[i].local && mr_same(y.row[i].id, "", stem)) {
            ctx = y.row[i].ctx;
            break;
        }
    }
    return ctx;
}

/* 工件名去目录与 .gguf 后缀，用于和 model.yaml 的 model_id 对齐。 */
void mr_stem(const char *asset, char *out, size_t cap)
{
    const char *base = strrchr(asset, '/');
    size_t n;

    base = base ? base + 1 : asset;
    snprintf(out, cap, "%s", base);
    n = strlen(out);
    if (n > 5 && strcmp(out + n - 5, ".gguf") == 0)
        out[n - 5] = '\0';
}

int mr_same(const char *a, const char *name, const char *stem)
{
    return a[0] && (strcmp(a, name) == 0 || strcmp(a, stem) == 0);
}

size_t mr_refs(const mr_yaml_t *y, const mr_kv_t *kv, const char *name,
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
