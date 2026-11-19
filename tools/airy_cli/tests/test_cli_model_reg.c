// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_model_reg.c
 * @brief D-6 本地模型注册表子命令单测（真实临时 AIRY_HOME，无桩数据面）。
 *
 * 覆盖 §19.1 D-6 判据与 R-04~R-07：
 *  - use 校验在位工件、active_model 原子写；
 *  - remove 默认 dry-run 不改任何字节、未 --yes 不删工件；
 *  - 被 active_model / model.yaml 槽位或本地行引用时拒绝，--force 放行；
 *  - 声明片段只更新本实例键，同文件其他键（第二实例）保留；
 *  - list 合并注册表与 model.yaml 声明，两源失步报错并给修复命令。
 */

#include "airy_cli_cmd_internal.h"
#include "cli_internal.h"
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

void cli_json_esc(char *dst, size_t cap, const char *src)
{
    size_t n = 0;

    for (; src && *src && n + 7 < cap; src++) {
        unsigned char c = (unsigned char)*src;

        if (c == '"' || c == '\\') {
            dst[n++] = '\\';
            dst[n++] = (char)c;
        } else if (c < 0x20) {
            n += (size_t)snprintf(dst + n, cap - n, "\\u%04x", c);
        } else {
            dst[n++] = (char)c;
        }
    }
    dst[n] = '\0';
}

static char g_home[PATH_MAX];
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

static void setup(void)
{
    char dir[PATH_MAX];

    pth(g_home, sizeof(g_home), "/tmp/airy-modelreg-XXXXXX");
    if (!mkdtemp(g_home)) {
        printf("FAIL mkdtemp\n");
        exit(1);
    }
    setenv("AIRY_HOME", g_home, 1);
    setenv("AIRY_LOCAL_LLM_PORT", "11435", 1);
    pth(dir, sizeof(dir), "%s/local/models", g_home);
    airy_mkdir_p(dir);
    pth(dir, sizeof(dir), "%s/local/registry", g_home);
    airy_mkdir_p(dir);
    pth(dir, sizeof(dir), "%s/config/managed.d", g_home);
    airy_mkdir_p(dir);
}

static void reg_path(char *out, size_t cap)
{
    pth(out, cap, "%s/local/models/installed.env", g_home);
}

static void frag_path(char *out, size_t cap)
{
    pth(out, cap, "%s/config/managed.d/llamacpp.env", g_home);
}

static void yaml_path(char *out, size_t cap)
{
    pth(out, cap, "%s/config/model.yaml", g_home);
}

static void touch_asset(const char *asset)
{
    char path[PATH_MAX];

    pth(path, sizeof(path), "%s/local/models/%s", g_home, asset);
    wfile(path, "gguf");
}

static int run(const char *sub, const char *a1, const char *a2, const char *a3)
{
    char *av[4];
    int n = 0;

    av[n++] = (char *)sub;
    if (a1)
        av[n++] = (char *)a1;
    if (a2)
        av[n++] = (char *)a2;
    if (a3)
        av[n++] = (char *)a3;
    return cli_model_reg(n, av);
}

static void t_empty(void)
{
    char reg[PATH_MAX];

    setup();
    reg_path(reg, sizeof(reg));
    CHECK(run("list", NULL, NULL, NULL) == 0, "empty list rc");
    CHECK(!airy_file_exists(reg), "empty list must not create registry");
}

/* use 链路 + 第二实例键保留 + 缺失工件拒绝。 */
static void t_use(void)
{
    char reg[PATH_MAX], frag[PATH_MAX], srv[PATH_MAX], bin[PATH_MAX];
    char text[4096];

    setup();
    reg_path(reg, sizeof(reg));
    frag_path(frag, sizeof(frag));
    touch_asset("a.gguf");
    pth(text, sizeof(text),
        "model.a.asset=a.gguf\nmodel.a.size=42\nmodel.a.sha256=dead\n"
        "model.a.args=--ctx-size 2048\nmodel.a.mem=512\n"
        "model.a.installed_at=2026-11-17T00:00:00Z\n"
        "model.b.asset=b.gguf\n");
    wfile(reg, text);
    wfile(frag, "OTHER_INSTANCE=keep\n");
    pth(srv, sizeof(srv), "%s/local/registry/installed.env", g_home);
    pth(bin, sizeof(bin), "%s/bin", g_home);
    airy_mkdir_p(bin);
    pth(bin, sizeof(bin), "%s/bin/llama-server", g_home);
    wfile(bin, "bin");
    pth(text, sizeof(text), "server=%s\n", bin);
    wfile(srv, text);

    CHECK(run("use", "a", NULL, NULL) == 0, "use a rc");
    CHECK(has(reg, "active_model=a"), "active_model written");
    CHECK(has(frag, "OTHER_INSTANCE=keep"), "other instance key kept");
    CHECK(has(frag, "AIRYRT_MANAGED_NAME=local_llm"), "decl name default");
    CHECK(has(frag, "AIRYRT_MANAGED_EP=tcp:11435"), "decl ep");
    CHECK(has(frag, "--ctx-size 2048"), "decl args from registry");
    CHECK(has(frag, "AIRYRT_MANAGED_MEM=512"), "decl mem");

    /* 实例名归政策层：已声明则沿用，不被 CLI 覆盖。 */
    wfile(frag, "OTHER_INSTANCE=keep\nAIRYRT_MANAGED_NAME=llama_server\n");
    CHECK(run("use", "a", NULL, NULL) == 0, "use a again rc");
    CHECK(has(frag, "AIRYRT_MANAGED_NAME=llama_server"), "policy name kept");

    CHECK(run("use", "b", NULL, NULL) == 2, "use b missing asset rc");
    CHECK(has(reg, "active_model=a"), "active unchanged after failed use");
    CHECK(run("use", "ghost", NULL, NULL) == 2, "use unknown rc");
}

/* 服务注册表双路径：安装器路径优先，旧 registry 路径回退。
 * 回归背景：安装器 airy-local 写 local/llamacpp/installed.env，CLI 只读
 * local/registry/installed.env，导致自托管服务在模型面板中永远缺席。 */
static void t_use_installer_path(void)
{
    char reg[PATH_MAX], frag[PATH_MAX], srv[PATH_MAX], bin[PATH_MAX], dir[PATH_MAX];
    char text[4096];

    setup();
    reg_path(reg, sizeof(reg));
    frag_path(frag, sizeof(frag));
    touch_asset("a.gguf");
    wfile(reg, "model.a.asset=a.gguf\nmodel.a.mem=512\nactive_model=a\n");
    pth(dir, sizeof(dir), "%s/local/llamacpp", g_home);
    airy_mkdir_p(dir);
    pth(bin, sizeof(bin), "%s/bin/llama-server", g_home);
    pth(dir, sizeof(dir), "%s/bin", g_home);
    airy_mkdir_p(dir);
    wfile(bin, "bin");
    pth(srv, sizeof(srv), "%s/local/llamacpp/installed.env", g_home);
    pth(text, sizeof(text), "server=%s\n", bin);
    wfile(srv, text);

    CHECK(run("use", "a", NULL, NULL) == 0, "use via installer registry rc");
    CHECK(has(frag, "AIRYRT_MANAGED_NAME=local_llm"), "installer path yields decl");

    /* 两处并存时以安装器路径为准：把旧路径指向不存在的二进制，
     * 声明仍应指向安装器登记的真实 server。 */
    pth(srv, sizeof(srv), "%s/local/registry/installed.env", g_home);
    wfile(srv, "server=/nonexistent/llama-server\n");
    CHECK(run("use", "a", NULL, NULL) == 0, "prefer installer registry rc");
    CHECK(has(frag, "AIRYRT_MANAGED_BIN="), "decl bin present");
    CHECK(!has(frag, "/nonexistent/llama-server"), "legacy path not used");
}

/* remove 三态：默认 dry-run / 被引用拒绝 / --force 生效。 */
static void t_remove(void)
{
    char reg[PATH_MAX], frag[PATH_MAX], path[PATH_MAX];

    setup();
    reg_path(reg, sizeof(reg));
    frag_path(frag, sizeof(frag));
    touch_asset("a.gguf");
    wfile(reg, "model.a.asset=a.gguf\nmodel.a.size=42\n"
               "model.a.sha256=dead\nmodel.a.args=\n"
               "model.a.mem=512\nmodel.a.installed_at=t\n"
               "active_model=a\n");
    wfile(frag, "OTHER_INSTANCE=keep\nAIRYRT_MANAGED_NAME=llama_server\n");
    pth(path, sizeof(path), "%s/local/models/a.gguf", g_home);

    CHECK(run("remove", "a", NULL, NULL) == 0, "remove dry-run rc");
    CHECK(airy_file_exists(path), "dry-run keeps artifact");
    CHECK(has(reg, "model.a.asset=a.gguf"), "dry-run keeps registry");

    CHECK(run("remove", "a", "--yes", NULL) == 2, "remove referenced rc");
    CHECK(airy_file_exists(path), "referenced keeps artifact");

    CHECK(run("remove", "a", "--yes", "--force") == 0, "remove force rc");
    CHECK(!airy_file_exists(path), "artifact removed");
    CHECK(!has(reg, "model.a.asset"), "registry key removed");
    CHECK(!has(reg, "active_model"), "active cleared");
    CHECK(has(frag, "OTHER_INSTANCE=keep"), "other instance survives remove");
    CHECK(!has(frag, "AIRYRT_MANAGED_NAME"), "own decl cleared");

    CHECK(run("remove", "ghost", "--yes", NULL) == 0, "remove unknown rc");
}

/* model.yaml 槽位引用：未 --force 拒绝。 */
static void t_slot_ref(void)
{
    char reg[PATH_MAX], yaml[PATH_MAX];

    setup();
    reg_path(reg, sizeof(reg));
    yaml_path(yaml, sizeof(yaml));
    touch_asset("s.gguf");
    wfile(reg, "model.s.asset=s.gguf\nactive_model=s\n");
    wfile(yaml, "slots:\n  t2: s\nmodels:\n  - name: local\n    mode: local\n"
                "    model_id: s\n");
    CHECK(run("remove", "s", "--yes", NULL) == 2, "slot ref rc");
    CHECK(run("remove", "s", "--yes", "--force") == 0, "slot force rc");
}

/* list：合并声明 + 双 SSoT 失步判定。 */
static void t_list_drift(void)
{
    char reg[PATH_MAX], yaml[PATH_MAX];

    setup();
    reg_path(reg, sizeof(reg));
    yaml_path(yaml, sizeof(yaml));
    touch_asset("a.gguf");
    wfile(reg, "model.a.asset=a.gguf\n");
    CHECK(run("list", NULL, NULL, NULL) == 0, "list w/o yaml rc");

    wfile(yaml, "models:\n  - name: local\n    mode: local\n    model_id: zzz\n");
    CHECK(run("list", NULL, NULL, NULL) == 2, "drift detected rc");

    wfile(yaml, "models:\n  - name: local\n    mode: local\n    model_id: a\n");
    CHECK(run("list", NULL, NULL, NULL) == 0, "consistent rc");
    CHECK(run("list", "--json", NULL, NULL) == 0, "json list rc");
}

int main(void)
{
    t_empty();
    t_use();
    t_use_installer_path();
    t_remove();
    t_slot_ref();
    t_list_drift();
    printf("test_cli_model_reg: %s\n", g_fail ? "FAIL" : "PASS");
    return g_fail;
}
