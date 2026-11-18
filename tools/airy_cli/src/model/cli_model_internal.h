// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_model_internal.h
 * @brief 本地模型注册表内部契约（0.1.21 D-6）。
 *
 * 三个实现件按数据源分治：cli_model_store.c 持有注册表 KV 与声明片段收敛，
 * cli_model_yaml.c 只读 model.yaml 视图，cli_model_reg.c 负责 list/use/remove
 * 子命令与参数判定。注册表与服务注册的键空间由 store 独占，yaml 视图只做
 * 引用判定，子命令不直接触碰文件。
 */
#ifndef CLI_MODEL_INTERNAL_H
#define CLI_MODEL_INTERNAL_H

#include "platform_paths.h"

#include <stddef.h>

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

void mr_ctx_init(mr_ctx_t *c);
void mr_free(mr_kv_t *kv);
int mr_load(mr_kv_t *kv, const char *path);
int mr_save(const mr_kv_t *kv, const char *path);
int mr_get(const mr_kv_t *kv, const char *key, char *buf, size_t cap);
int mr_set(mr_kv_t *kv, const char *key, const char *val);
void mr_del(mr_kv_t *kv, const char *key);
int mr_name_ok(const char *name);
size_t mr_names(const mr_kv_t *kv, char out[][MR_NAME_MAX], size_t cap);
int mr_asset(const mr_kv_t *kv, const char *name, char *out, size_t cap);
int mr_active(const mr_ctx_t *c, const mr_kv_t *kv, char *out, size_t cap);
int mr_sync(const mr_ctx_t *c, const mr_kv_t *reg);

void mr_yaml_load(const mr_ctx_t *c, mr_yaml_t *y);
void mr_stem(const char *asset, char *out, size_t cap);
int mr_same(const char *a, const char *name, const char *stem);
size_t mr_refs(const mr_yaml_t *y, const mr_kv_t *kv, const char *name,
               const char *asset, char *out, size_t cap);

#endif /* CLI_MODEL_INTERNAL_H */
