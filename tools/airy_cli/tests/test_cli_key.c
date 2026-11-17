// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_key.c
 * @brief D-2 密钥装配引导单测（真实临时 AIRY_HOME，无桩数据面）。
 *
 * 覆盖 §19.1 D-2：行选择（声明 api_key_env / MODEL_<N>_API_KEY 映射）、
 * secrets.env 原子写与替换、本地行不需要密钥、取消/未知行不落盘，
 * 以及 llm.probe 结果三态（通过 / 失败给原因 / 网关不可达）。
 */

#include "airy_cli_cmd_internal.h"
#include "cli_internal.h"
#include "cli_gw_json.h"
#include "platform_misc.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int g_cli_json_mode = 0;

void cli_outf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

/* llm.probe 测试替身：仅在网关边界替换，CLI 写盘与选择逻辑保持真实。 */
static int g_probe_rc = 0;
static const char *g_probe_reason = "ok";
static int g_probe_calls = 0;

int cli_gw_json(const char *method, cJSON *params, int timeout_ms, cJSON **out_root)
{
    (void)timeout_ms;
    (void)method;
    cJSON_Delete(params);
    g_probe_calls++;
    if (g_probe_rc != 0)
        return -1;
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "reachable", strcmp(g_probe_reason, "ok") == 0);
    cJSON_AddStringToObject(root, "reason", g_probe_reason);
    cJSON_AddNumberToObject(root, "http_code", 200);
    cJSON_AddNumberToObject(root, "latency_ms", 8);
    *out_root = root;
    return 0;
}

const char *cli_gw_last_err(void)
{
    return "gateway 不可达（测试）";
}

static char g_home[PATH_MAX];
static char g_yaml[PATH_MAX];
static char g_secret[PATH_MAX];
static int g_fail;

#define CHECK(cond, what)                                                       \
    do {                                                                        \
        if (!(cond)) {                                                          \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, what);                \
            g_fail = 1;                                                         \
        }                                                                       \
    } while (0)

static void pth(char *out, size_t cap, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(out, cap, fmt, ap);
    va_end(ap);
}

static void wfile(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");

    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static int has(const char *path, const char *needle)
{
    char buf[8192];
    FILE *f = fopen(path, "r");
    size_t n;

    if (!f)
        return 0;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

static int lines(const char *path)
{
    char buf[8192];
    FILE *f = fopen(path, "r");
    int n = 0;

    if (!f)
        return 0;
    while (fgets(buf, sizeof(buf), f))
        n++;
    fclose(f);
    return n;
}

static void setup(void)
{
    char dir[PATH_MAX];

    pth(g_home, sizeof(g_home), "/tmp/airy-key-XXXXXX");
    if (!mkdtemp(g_home)) {
        printf("FAIL mkdtemp\n");
        exit(1);
    }
    setenv("AIRY_HOME", g_home, 1);
    pth(dir, sizeof(dir), "%s/config", g_home);
    airy_mkdir_p(dir);
    pth(g_yaml, sizeof(g_yaml), "%s/model.yaml", dir);
    pth(g_secret, sizeof(g_secret), "%s/secrets.env", dir);

    wfile(g_yaml,
          "models:\n"
          "  - name: openai-main\n"
          "    mode: api\n"
          "    model_id: gpt-4o\n"
          "    api_key_env: OPENAI_API_KEY\n"
          "  - name: claude-main\n"
          "    mode: api\n"
          "    model_id: claude-3\n"
          "  - name: local-qwen\n"
          "    mode: local\n"
          "    model_id: qwen2.5\n");
    wfile(g_secret, "EXISTING_VAR=keep\n");
}

static void test_store_atomic(void)
{
    int replaced = -1;

    CHECK(airy_cli_secret_store(g_secret, "OPENAI_API_KEY", "sk-one", &replaced) == 0 &&
              replaced == 0,
          "store adds new var");
    CHECK(has(g_secret, "OPENAI_API_KEY=sk-one") && has(g_secret, "EXISTING_VAR=keep"),
          "store preserves other vars");
    CHECK(airy_cli_secret_store(g_secret, "OPENAI_API_KEY", "sk-two", &replaced) == 0 &&
              replaced == 1,
          "store replaces existing var");
    CHECK(has(g_secret, "OPENAI_API_KEY=sk-two") && has(g_secret, "sk-one") == 0,
          "store rewrites single line");
    CHECK(lines(g_secret) == 2, "store leaves no duplicated line");
    CHECK(has(g_secret, "OPENAI_API_KEY=sk-one") == 0, "no stale tmp content");
}

static void test_list_no_write(void)
{
    char *argv[] = {"--list"};

    CHECK(cli_cmd_key(1, argv) == 0, "key --list ok");
    CHECK(has(g_secret, "CLAUDE") == 0, "list writes nothing");
}

static void test_unknown_row(void)
{
    char *argv[] = {"--model", "not-exist", "--key", "sk-x"};

    CHECK(cli_cmd_key(4, argv) == 2, "unknown model rejected");
    CHECK(has(g_secret, "sk-x") == 0, "unknown model writes nothing");
}

static void test_local_row(void)
{
    char *argv[] = {"--model", "local-qwen", "--key", "sk-x"};
    int before = g_probe_calls;

    CHECK(cli_cmd_key(4, argv) == 0, "local row skipped");
    CHECK(g_probe_calls == before, "local row not probed");
    CHECK(has(g_secret, "sk-x") == 0, "local row writes nothing");
}

static void test_probe_ok(void)
{
    char *argv[] = {"--model", "openai-main", "--key", "sk-live"};

    g_probe_rc = 0;
    g_probe_reason = "ok";
    CHECK(cli_cmd_key(4, argv) == 0, "probe ok returns 0");
    CHECK(has(g_secret, "OPENAI_API_KEY=sk-live"), "declared env written");
    CHECK(g_probe_calls >= 1, "probe invoked after write");
}

static void test_probe_reason(void)
{
    char *argv[] = {"--index", "2", "--key", "sk-bad"};

    g_probe_rc = 0;
    g_probe_reason = "auth_failed";
    CHECK(cli_cmd_key(4, argv) == 2, "probe failure returns 2");
    CHECK(has(g_secret, "MODEL_2_API_KEY=sk-bad"), "undeclared env maps by row");
}

static void test_gateway_down(void)
{
    char *argv[] = {"--model", "openai-main", "--key", "sk-off"};

    g_probe_rc = -1;
    CHECK(cli_cmd_key(4, argv) == 2, "gateway down returns 2");
    CHECK(has(g_secret, "OPENAI_API_KEY=sk-off"), "key kept for later verify");
    g_probe_rc = 0;
}

static void test_dry_run(void)
{
    char *argv[] = {"--model", "claude-main", "--key", "sk-dry", "--dry-run"};

    CHECK(cli_cmd_key(5, argv) == 0, "dry-run ok");
    CHECK(has(g_secret, "sk-dry") == 0, "dry-run writes nothing");
}

int main(void)
{
    setup();
    test_store_atomic();
    test_list_no_write();
    test_unknown_row();
    test_local_row();
    test_probe_ok();
    test_probe_reason();
    test_gateway_down();
    test_dry_run();

    char cmd[PATH_MAX + 32];
    pth(cmd, sizeof(cmd), "rm -rf %s", g_home);
    if (system(cmd) != 0)
        printf("warn: cleanup failed\n");

    printf("%s\n", g_fail ? "cli_key tests FAILED" : "cli_key tests passed");
    return g_fail;
}
