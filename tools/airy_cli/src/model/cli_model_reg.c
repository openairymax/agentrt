// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_model_reg.c
 * @brief 本地模型注册表子命令（0.1.21 D-6）：list / use / remove。
 *
 * 数据面由 cli_model_store.c 独占，model.yaml 视图由 cli_model_yaml.c 提供，
 * 本件只做参数判定、状态编排与输出。remove 默认 dry-run，只有 --yes 才落
 * 删除；被 active_model / model.yaml 槽位或本地托管行引用的模型拒绝删除，
 * 需 --force 同时显式确认。list 合并「model.yaml 本地声明 + 注册表安装态」，
 * 并把注册表与 model.yaml 的失步显式报错并给修复命令，不静默取其一。
 */
#include "cli_model_internal.h"

#include "airy_cli_cmd_internal.h"
#include "cli_internal.h"
#include "platform_misc.h"

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

/* 已列出的注册表名字中是否含 stem。 */
static int mr_listed(const char *stem, char names[][MR_NAME_MAX], size_t n)
{
    for (size_t j = 0; j < n; j++) {
        if (mr_same(stem, names[j], names[j]))
            return 1;
    }
    return 0;
}

/* 打印 model.yaml 本地声明但未安装的行。 */
static void mr_list_declared(const mr_yaml_t *y, char names[][MR_NAME_MAX], size_t n,
                             int json)
{
    char stem[MR_NAME_MAX];

    for (size_t i = 0; i < y->nrow; i++) {
        if (!y->row[i].local)
            continue;
        mr_stem(y->row[i].id, stem, sizeof(stem));
        if (mr_listed(stem, names, n))
            continue;
        if (json)
            cli_outf("{\"schema\":1,\"name\":\"%s\",\"state\":\"declared\","
                     "\"asset\":\"\",\"active\":0}\n", y->row[i].id);
        else
            cli_outf("%s [declared] model.yaml 本地声明（未安装）\n", y->row[i].id);
    }
}

/* 注册表有在位模型但 active_model 无效：报失步并给修复命令（返回 1 表示 bad）。 */
static int mr_list_incons(size_t n, int active, const char *first)
{
    if (n == 0 || active)
        return 0;
    cli_outf("inconsistent: 注册表有 %zu 个在位模型但 active_model 无效\n", n);
    cli_outf("repair: airy_cli model use %s\n", first);
    return 1;
}

/* 校验 model.yaml 本地行与注册表 active 是否一致（返回 1 表示 bad）。 */
static int mr_list_verify(mr_kv_t *reg, const mr_yaml_t *y, const char *act, int active)
{
    char asset[MR_VAL_MAX];
    char stem[MR_NAME_MAX];
    int bad = 0;

    for (size_t i = 0; active && i < y->nrow; i++) {
        if (!y->row[i].local)
            continue;
        mr_asset(reg, act, asset, sizeof(asset));
        mr_stem(asset, stem, sizeof(stem));
        if (!mr_same(y->row[i].id, act, stem)) {
            cli_outf("inconsistent: model.yaml 本地行 model_id=%s 与注册表 "
                     "active=%s（%s）不一致\n", y->row[i].id, act, asset);
            cli_outf("repair: 把 model.yaml 该行 model_id 改为 %s，"
                     "或 airy_cli model use %s\n", stem, y->row[i].id);
            bad = 1;
        }
    }
    return bad;
}

static int mr_list(const mr_ctx_t *c, int json)
{
    mr_kv_t reg;
    mr_yaml_t y;
    char names[MR_MAX_MODELS][MR_NAME_MAX];
    char asset[MR_VAL_MAX];
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
        mr_asset(&reg, names[i], asset, sizeof(asset));
        mr_show(names[i], asset, active && strcmp(act, names[i]) == 0, json);
    }
    mr_list_declared(&y, names, n, json);
    bad = mr_list_incons(n, active, names[0]);
    if (mr_list_verify(&reg, &y, act, active))
        bad = 1;
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
    if (!mr_asset(&reg, name, asset, sizeof(asset))) {
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

/* dry-run 报告：工件、注册表键与引用。 */
static void mr_remove_report(const char *name, const char *path, const char *refs,
                             int has_ref)
{
    cli_outf("remove: model=%s asset=%s\n", name, path);
    cli_outf("  %s 删除工件\n", airy_file_exists(path) ? "将" : "工件缺失，跳过");
    cli_outf("  将删除注册表 model.%s.{asset,size,sha256,args,mem,installed_at}\n",
             name);
    if (has_ref)
        cli_outf("引用: %s\n", refs);
}

/* 确认后执行删除：工件 + 注册表键 + active_model 清理（返回 0/2）。 */
static int mr_remove_apply(const mr_ctx_t *c, mr_kv_t *reg, const char *name,
                           const char *asset, const char *path, const char *stems)
{
    static const char *sub[MR_KEYS] = { "asset", "size", "sha256",
                                        "args", "mem", "installed_at" };
    char key[MR_NAME_MAX + 16];

    if (airy_file_exists(path) && airy_file_remove(path) != 0) {
        cli_outf("airy_cli: 工件删除失败 %s\n", path);
        return 2;
    }
    for (size_t i = 0; i < MR_KEYS; i++) {
        snprintf(key, sizeof(key), "model.%s.%s", name, sub[i]);
        mr_del(reg, key);
    }
    {
        char act[MR_NAME_MAX];

        if (mr_get(reg, "active_model", act, sizeof(act)) &&
            (strcmp(act, name) == 0 || strcmp(act, stems) == 0))
            mr_del(reg, "active_model");
    }
    if (mr_save(reg, c->reg) != 0 || mr_sync(c, reg) != 0) {
        cli_outf("airy_cli: 注册表/声明片段写回失败\n");
        return 2;
    }
    cli_outf("removed model: %s (%s)\n", name, asset);
    return 0;
}

static int mr_remove(const mr_ctx_t *c, const char *name, int yes, int force)
{
    mr_kv_t reg;
    mr_yaml_t y;
    char asset[MR_VAL_MAX];
    char path[AIRY_PATH_MAX];
    char stems[MR_NAME_MAX];
    char refs[256];
    int has_ref;
    int rc;

    if (!mr_name_ok(name)) {
        cli_outf("airy_cli: 非法模型名 %s\n", name ? name : "");
        return 2;
    }
    if (mr_load(&reg, c->reg) != 0) {
        cli_outf("airy_cli: 无法读取 %s\n", c->reg);
        return 2;
    }
    if (!mr_asset(&reg, name, asset, sizeof(asset))) {
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
    mr_remove_report(name, path, refs, has_ref);
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
    rc = mr_remove_apply(c, &reg, name, asset, path, stems);
    mr_free(&reg);
    return rc;
}

/* list 子命令：解析 --json 并输出。 */
static int mr_cmd_list(const mr_ctx_t *c, int argc, char *argv[])
{
    int json = g_cli_json_mode;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0)
            json = 1;
        else {
            cli_outf("airy_cli: model list 不认识参数 %s\n", argv[i]);
            return 2;
        }
    }
    return mr_list(c, json);
}

/* remove 子命令：解析 --yes/--force/--dry-run 与模型名。 */
static int mr_cmd_remove(const mr_ctx_t *c, int argc, char *argv[])
{
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
    return mr_remove(c, name, yes, force);
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
    mr_ctx_init(&c);
    if (strcmp(sub, "list") == 0)
        return mr_cmd_list(&c, argc, argv);
    if (strcmp(sub, "use") == 0) {
        if (argc != 2) {
            cli_outf("用法: airy_cli model use <name>\n");
            return 2;
        }
        return mr_use(&c, argv[1]);
    }
    if (strcmp(sub, "remove") == 0)
        return mr_cmd_remove(&c, argc, argv);
    cli_outf("用法: airy_cli model list [--json] | use <name> | "
             "remove <name> [--yes] [--force]\n");
    return 2;
}
