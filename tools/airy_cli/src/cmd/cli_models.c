// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_models.c
 * @brief 模型连接表自检视图（0.1.21 A-7）：供 airymaxrt doctor 区分
 *        「本地模型 / 缺 key / 已配 key」，不做联网探测。
 *
 * 解析仍走 commons yaml_minimal（与 llm_d 同一解析件），输出 NDJSON：
 * 一行一个连接，只报告 env 名与是否存在，绝不输出密钥值。密钥存在性
 * 与运行期一致——先看进程环境，再看 secrets.env 的非空值。
 */

#include "cli_internal.h"
#include "platform_paths.h"
#include "yaml_minimal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_MODEL_ENV_MAX 64

static int env_present(const char *name)
{
    const char *v = getenv(name);

    return v != NULL && v[0] != '\0';
}

/* secrets.env 的值语义与运行期 sec_resolve_key 一致：允许两侧空白，
 * 双引号包裹视为字面值，引号内为空即视为未配置。 */
static int file_has_key(const char *path, const char *name)
{
    FILE *f;
    char line[512];
    size_t nlen = strlen(name);
    int found = 0;

    f = fopen(path, "r");
    if (!f)
        return 0;
    while (!found && fgets(line, sizeof(line), f)) {
        char *p = line;
        char *q;

        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#')
            continue;
        if (strncmp(p, name, nlen) != 0)
            continue;
        q = p + nlen;
        while (*q == ' ' || *q == '\t')
            q++;
        if (*q != '=')
            continue;
        q++;
        while (*q == ' ' || *q == '\t')
            q++;
        if (*q == '"') {
            q++;
            found = *q != '\0' && *q != '"' && *q != '\n' && *q != '\r';
        } else {
            found = *q != '\0' && *q != '\n' && *q != '\r';
        }
    }
    fclose(f);
    return found;
}

static const char *node_str(const struct yaml_node *n, const char *key)
{
    return yaml_as_string(yaml_get(n, key), "");
}

/* 行号映射与 /apikey set 同规则：未声明 api_key_env 时取 MODEL_<行号>_API_KEY。 */
static void key_env_of(const struct yaml_node *row, size_t idx, const char *mode,
                       char *out, size_t cap)
{
    const char *decl = node_str(row, "api_key_env");

    out[0] = '\0';
    if (decl[0] != '\0') {
        snprintf(out, cap, "%s", decl);
    } else if (strcmp(mode, "api") == 0) {
        snprintf(out, cap, "MODEL_%zu_API_KEY", idx + 1);
    }
}

static void emit_row(const struct yaml_node *row, size_t idx, const char *secrets,
                     int json)
{
    const char *mode = node_str(row, "mode");
    char env[CLI_MODEL_ENV_MAX];
    char esc[4][CLI_MODEL_ENV_MAX * 2];
    int has_key;

    key_env_of(row, idx, mode, env, sizeof(env));
    has_key = env[0] != '\0' &&
              (env_present(env) || file_has_key(secrets, env));
    if (!json) {
        cli_outf("name=%s mode=%s base_url=%s key_env=%s has_key=%d\n",
                 node_str(row, "name"), mode, node_str(row, "base_url"), env,
                 has_key);
        return;
    }
    cli_json_esc(esc[0], sizeof(esc[0]), node_str(row, "name"));
    cli_json_esc(esc[1], sizeof(esc[1]), mode);
    cli_json_esc(esc[2], sizeof(esc[2]), node_str(row, "base_url"));
    cli_json_esc(esc[3], sizeof(esc[3]), env);
    cli_outf("{\"schema\":1,\"name\":\"%s\",\"mode\":\"%s\",\"base_url\":\"%s\","
             "\"key_env\":\"%s\",\"has_key\":%d}\n",
             esc[0], esc[1], esc[2], esc[3], has_key);
}

int cli_cmd_models(int argc, char *argv[])
{
    char path[AIRY_PATH_MAX];
    char secrets[AIRY_PATH_MAX];
    const char *file = NULL;
    yaml_document_t *doc;
    struct yaml_node *rows;
    size_t n;
    int json = g_cli_json_mode;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            file = argv[++i];
        } else {
            cli_outf("airy_cli: models 不认识参数 %s\n", argv[i]);
            return 2;
        }
    }
    if (file)
        snprintf(path, sizeof(path), "%s", file);
    else
        snprintf(path, sizeof(path), "%s/model.yaml", airy_config_dir());
    snprintf(secrets, sizeof(secrets), "%s/secrets.env", airy_config_dir());

    doc = yaml_create();
    if (!doc || yaml_parse_file(doc, path) != AIRY_SUCCESS) {
        cli_outf("airy_cli: 无法解析 %s（%s）\n", path,
                 doc ? yaml_get_error(doc) : "内存不足");
        yaml_destroy(doc);
        return 2;
    }
    rows = yaml_get(yaml_root(doc), "models");
    n = yaml_size(rows);
    if (n == 0) {
        cli_outf("airy_cli: %s 无 models 表（需 model.yaml v3 的 models: 列表）\n",
                 path);
        yaml_destroy(doc);
        return 2;
    }
    for (size_t i = 0; i < n; i++)
        emit_row(yaml_get_index(rows, i), i, secrets, json);
    yaml_destroy(doc);
    return 0;
}
