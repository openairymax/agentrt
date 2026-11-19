/* SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd. */
/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */

/**
 * @file cli_key.c
 * @brief 0.1.21 D-2：`agentrt key` 密钥装配引导。
 *
 * 流程：选 provider 行 → 填 key → 写入 secrets.env 正确变量位 →
 * 经 llm.probe 立即连通验证 → 失败给出可判读原因。写入规则与
 * `/apikey set` 同源（声明 api_key_env，否则 MODEL_<行号>_API_KEY），
 * 落盘为 tmp + fsync + rename 原子替换；中途取消不落任何字节。
 */

#include "airy_cli_cmd_internal.h"
#include "cli_internal.h"
#include "cli_gw.h"
#include "cli_gw_json.h"
#include "platform_misc.h"
#include "platform_paths.h"
#include "yaml_minimal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#else
#include <io.h>
#endif

#define KK_ROWS_MAX 64
#define KK_NAME_MAX 64
#define KK_ID_MAX 128
#define KK_VAR_MAX 64

typedef struct {
    char name[KK_NAME_MAX];
    char model_id[KK_ID_MAX];
    char key_env[KK_VAR_MAX];
    int local;
} kk_row_t;

typedef struct {
    char yaml[AIRY_PATH_MAX];
    char secrets[AIRY_PATH_MAX];
    kk_row_t row[KK_ROWS_MAX];
    size_t count;
} kk_ctx_t;

static const char *kk_str(const struct yaml_node *n, const char *key)
{
    return yaml_as_string(yaml_get(n, key), "");
}

/* 行号映射与 /apikey set 同规则：未声明 api_key_env 时取
 * MODEL_<行号>_API_KEY（行号 1 基）。 */
static void kk_env_of(const struct yaml_node *row, size_t idx, char *out, size_t cap)
{
    const char *env = kk_str(row, "api_key_env");

    if (env[0])
        snprintf(out, cap, "%s", env);
    else
        snprintf(out, cap, "MODEL_%zu_API_KEY", idx + 1);
}

static int kk_local_row(const struct yaml_node *row)
{
    const char *mode = kk_str(row, "mode");
    const char *prov = kk_str(row, "provision");

    return strstr(mode, "local") != NULL || strstr(prov, "local") != NULL;
}

static int kk_load(kk_ctx_t *ctx)
{
    snprintf(ctx->yaml, sizeof(ctx->yaml), "%s/model.yaml", airy_config_dir());
    snprintf(ctx->secrets, sizeof(ctx->secrets), "%s/secrets.env", airy_config_dir());

    yaml_document_t *doc = yaml_create();
    if (!doc || yaml_parse_file(doc, ctx->yaml) != AIRY_SUCCESS) {
        cli_outf("agentrt key: 无法解析 %s（%s）\n", ctx->yaml,
                 doc ? yaml_get_error(doc) : "内存不足");
        yaml_destroy(doc);
        return -1;
    }
    struct yaml_node *rows = yaml_get(yaml_root(doc), "models");
    size_t n = yaml_size(rows);
    ctx->count = 0;
    for (size_t i = 0; i < n && ctx->count < KK_ROWS_MAX; i++) {
        const struct yaml_node *row = yaml_get_index(rows, i);
        kk_row_t *r = &ctx->row[ctx->count];
        snprintf(r->name, sizeof(r->name), "%s", kk_str(row, "name"));
        snprintf(r->model_id, sizeof(r->model_id), "%s", kk_str(row, "model_id"));
        if (!r->model_id[0]) {
            size_t len = strlen(r->name);
            if (len >= sizeof(r->model_id))
                len = sizeof(r->model_id) - 1;
            AIRY_MEMCPY(r->model_id, r->name, len);
            r->model_id[len] = '\0';
        }
        kk_env_of(row, i, r->key_env, sizeof(r->key_env));
        r->local = kk_local_row(row);
        ctx->count++;
    }
    yaml_destroy(doc);
    if (ctx->count == 0) {
        cli_outf("agentrt key: %s 无 models 表（需 model.yaml v3 的 models: 列表）\n",
                 ctx->yaml);
        return -1;
    }
    return 0;
}

static int kk_has_key(const char *path, const char *var)
{
    FILE *f = fopen(path, "r");
    char line[512];
    size_t n = strlen(var);
    int found = 0;

    if (!f)
        return 0;
    while (!found && fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, var, n) != 0 || p[n] != '=')
            continue;
        p += n + 1;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '"')
            p++;
        found = *p && *p != '"' && *p != '\n' && *p != '\r';
    }
    fclose(f);
    return found;
}

static void kk_show(const kk_ctx_t *ctx)
{
    cli_outf("模型连接表 %s：\n", ctx->yaml);
    for (size_t i = 0; i < ctx->count; i++) {
        const kk_row_t *r = &ctx->row[i];
        const char *state;
        if (r->local)
            state = "本地模型，无需密钥";
        else
            state = kk_has_key(ctx->secrets, r->key_env) ? "已配置" : "未配置";
        cli_outf("  %zu) %-16s %-24s %s（%s）\n", i + 1, r->name, r->model_id,
                 state, r->key_env);
    }
}

static int kk_pick(const kk_ctx_t *ctx, const char *model, int index, size_t *out)
{
    if (model && model[0]) {
        for (size_t i = 0; i < ctx->count; i++) {
            if (strcmp(ctx->row[i].name, model) == 0 ||
                strcmp(ctx->row[i].model_id, model) == 0) {
                *out = i;
                return 0;
            }
        }
        cli_outf("agentrt key: 未找到模型行 %s\n", model);
        return -1;
    }
    if (index >= 1 && (size_t)index <= ctx->count) {
        *out = (size_t)index - 1;
        return 0;
    }
    if (index != 0) {
        cli_outf("agentrt key: 行号越界（1-%zu）\n", ctx->count);
        return -1;
    }
    if (ctx->count == 1) {
        *out = 0;
        return 0;
    }
    kk_show(ctx);
    cli_outf("选择要配置密钥的行号（回车取消）: ");
    fflush(stdout);
    char buf[32];
    if (!fgets(buf, sizeof(buf), stdin)) {
        cli_outf("\n已取消，未写入任何内容\n");
        return -1;
    }
    char *end = NULL;
    long v = strtol(buf, &end, 10);
    if (end == buf || v < 1 || (size_t)v > ctx->count) {
        cli_outf("已取消，未写入任何内容\n");
        return -1;
    }
    *out = (size_t)v - 1;
    return 0;
}

static int kk_readkey(char *out, size_t cap)
{
    cli_outf("输入 API Key（不回显，留空取消）: ");
    fflush(stdout);

    int ok = 0;
    int have = 0;
#if !defined(_WIN32)
    struct termios old;
    have = isatty(fileno(stdin)) && tcgetattr(fileno(stdin), &old) == 0;
    if (have) {
        struct termios no = old;
        no.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(fileno(stdin), TCSANOW, &no);
    }
#endif
    ok = fgets(out, (int)cap, stdin) != NULL;
#if !defined(_WIN32)
    if (have)
        tcsetattr(fileno(stdin), TCSANOW, &old);
    if (have)
        cli_outf("\n");
#endif

    if (!ok) {
        cli_outf("已取消，未写入任何内容\n");
        return -1;
    }
    size_t n = strlen(out);
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || isspace((unsigned char)out[n - 1])))
        out[--n] = '\0';
    if (n == 0) {
        cli_outf("已取消，未写入任何内容\n");
        return -1;
    }
    return 0;
}

/* 逐行重写 secrets：命中 var= 即替换，返回 1 表示发生替换。 */
static int kk_rewrite(FILE *rf, FILE *wf, const char *var, const char *key)
{
    char buf[1024];
    int hit = 0;
    while (fgets(buf, sizeof(buf), rf)) {
        char *p = buf;
        while (*p == ' ' || *p == '\t')
            p++;
        if (strncmp(p, var, strlen(var)) == 0 && p[strlen(var)] == '=') {
            fprintf(wf, "%s=%s\n", var, key);
            hit = 1;
        } else {
            fputs(buf, wf);
        }
    }
    return hit;
}

/* A-7 同源文案：secrets.env 变量位写入（tmp + fsync + rename，权限 600）。 */
int airy_cli_secret_store(const char *path, const char *var, const char *key, int *replaced)
{
    char tmp[AIRY_PATH_MAX];
    FILE *rf = fopen(path, "r");
    int hit = 0;

    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    FILE *wf = fopen(tmp, "w");
    if (!wf) {
        if (rf)
            fclose(rf);
        return -1;
    }
    if (rf) {
        hit = kk_rewrite(rf, wf, var, key);
        fclose(rf);
    }
    if (!hit)
        fprintf(wf, "%s=%s\n", var, key);
    if (fflush(wf) != 0) {
        fclose(wf);
        remove(tmp);
        return -1;
    }
#if !defined(_WIN32)
    if (fsync(fileno(wf)) != 0) {
        fclose(wf);
        remove(tmp);
        return -1;
    }
#endif
    if (fclose(wf) != 0) {
        remove(tmp);
        return -1;
    }
#if defined(_WIN32)
    _chmod(tmp, _S_IREAD | _S_IWRITE);
#else
    chmod(tmp, 0600);
#endif
    if (airy_file_rename(tmp, path) != 0) {
        remove(tmp);
        return -1;
    }
    if (replaced)
        *replaced = hit;
    return 0;
}

static const char *kk_reason(const char *reason)
{
    if (!reason)
        return "原因未知";
    if (strcmp(reason, "api_base_missing") == 0)
        return "后端未配置 api_base，先检查 model.yaml";
    if (strcmp(reason, "api_key_missing") == 0)
        return "密钥未生效（变量名不匹配或值为空）";
    if (strcmp(reason, "auth_failed") == 0)
        return "密钥无效或权限不足（HTTP 401/403）";
    if (strcmp(reason, "endpoint_not_found") == 0)
        return "服务地址不可用（/models 返回 404），检查 base_url";
    if (strcmp(reason, "rate_limited") == 0)
        return "上游限流（HTTP 429），密钥可用，稍后重试";
    if (strcmp(reason, "upstream_error") == 0)
        return "上游服务错误（HTTP 5xx），稍后重试";
    if (strcmp(reason, "timeout") == 0)
        return "连接超时，检查网络或 base_url";
    if (strcmp(reason, "unreachable") == 0)
        return "网络不可达，检查 base_url 与网络";
    return "非预期响应，检查 base_url 与密钥";
}

/* 立即连通验证：经 gateway → llm.probe，与业务请求同源（热解析密钥）。 */
static int kk_probe(const char *model_id, const char *prov, char *reason, size_t rcap,
                    long *code, double *ms)
{
    cJSON *params = cJSON_CreateObject();
    cJSON *root = NULL;
    int rc;

    if (!params)
        return -2;
    if (model_id[0])
        cJSON_AddStringToObject(params, "model", model_id);
    if (prov[0])
        cJSON_AddStringToObject(params, "provider", prov);
    rc = cli_gw_json("llm.probe", params, 12000, &root);
    if (rc != 0) {
        snprintf(reason, rcap, "%s", cli_gw_last_err());
        return -2;
    }
    cJSON *rj = cJSON_GetObjectItem(root, "reason");
    cJSON *cj = cJSON_GetObjectItem(root, "http_code");
    cJSON *mj = cJSON_GetObjectItem(root, "latency_ms");
    int reachable = cJSON_IsTrue(cJSON_GetObjectItem(root, "reachable"));
    snprintf(reason, rcap, "%s", cJSON_IsString(rj) && rj->valuestring ? rj->valuestring : "");
    if (code)
        *code = cJSON_IsNumber(cj) ? (long)cj->valuedouble : 0;
    if (ms)
        *ms = cJSON_IsNumber(mj) ? mj->valuedouble : 0.0;
    cJSON_Delete(root);
    return reachable ? 0 : 1;
}

/* 解析 key 子命令参数；返回 0 成功，2 为不认识参数。 */
static int kk_parse_args(int argc, char *argv[], const char **model, const char **key,
                         int *index, int *list, int *dry)
{
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            *model = argv[++i];
        else if (strcmp(argv[i], "--provider") == 0 && i + 1 < argc)
            *model = argv[++i];
        else if (strcmp(argv[i], "--index") == 0 && i + 1 < argc)
            *index = atoi(argv[++i]);
        else if (strcmp(argv[i], "--key") == 0 && i + 1 < argc)
            *key = argv[++i];
        else if (strcmp(argv[i], "--list") == 0)
            *list = 1;
        else if (strcmp(argv[i], "--dry-run") == 0)
            *dry = 1;
        else {
            cli_outf("agentrt key: 不认识参数 %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

int cli_cmd_key(int argc, char *argv[])
{
    kk_ctx_t ctx;
    const char *model = NULL;
    const char *key = NULL;
    int index = 0, list = 0, dry = 0;

    if (kk_parse_args(argc, argv, &model, &key, &index, &list, &dry) != 0)
        return 2;

    AIRY_MEMSET(&ctx, 0, sizeof(ctx));
    if (kk_load(&ctx) != 0)
        return 2;
    if (list) {
        kk_show(&ctx);
        return 0;
    }

    size_t sel = 0;
    if (kk_pick(&ctx, model, index, &sel) != 0)
        return 2;
    kk_row_t *row = &ctx.row[sel];
    if (row->local) {
        cli_outf("agentrt key: %s 为本地模型，无需密钥\n", row->name);
        return 0;
    }

    char secret[1024];
    if (key) {
        snprintf(secret, sizeof(secret), "%s", key);
    } else if (kk_readkey(secret, sizeof(secret)) != 0) {
        return 2;
    }

    if (dry) {
        cli_outf("将写入 %s=%s（%s），不实际落盘\n", row->key_env, "***", ctx.secrets);
        return 0;
    }
    int replaced = 0;
    if (airy_cli_secret_store(ctx.secrets, row->key_env, secret, &replaced) != 0) {
        cli_outf("agentrt key: 无法写入 %s（检查目录权限）\n", ctx.secrets);
        return 2;
    }
    AIRY_MEMSET(secret, 0, sizeof(secret));
    cli_outf("%s 已%s（%s，权限 600）\n", row->key_env, replaced ? "更新" : "添加",
             ctx.secrets);

    char reason[256] = "";
    long code = 0;
    double ms = 0.0;
    int prc = kk_probe(row->model_id, row->model_id[0] ? "" : row->name, reason,
                       sizeof(reason), &code, &ms);
    if (prc == -2) {
        cli_outf("agentrt key: 无法经 gateway 立即验证（%s）；密钥已写入，"
                 "可稍后运行 airy_cli models 复核\n", reason[0] ? reason : "gateway 不可达");
        return 2;
    }
    if (prc != 0) {
        cli_outf("agentrt key: 连通验证失败：%s（reason=%s，HTTP %ld）\n",
                 kk_reason(reason), reason, code);
        return 2;
    }
    cli_outf("连通验证通过：%s（HTTP %ld，%.0f ms）\n", row->name, code, ms);
    return 0;
}
