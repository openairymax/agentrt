// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_cmd_system.c
 * @brief System-face daemon commands: shared table, namespace and socket
 *        resolution, generic RPC printer, /daemons view, /daemon control
 *        dispatch, /stats rendering.
 *
 * Daemon process lifecycle and the self-heal reconcile loop live in
 * cli_daemon_lifecycle.c (§458): spawning a process and waiting for its
 * readiness depend on a different platform surface than read-only views,
 * and keeping them here pushed the file past the 800-line budget.
 */

#include "daemon_cmds.h"
#include "airy_cli_cmd_internal.h"
#include "daemon_rpc_client.h"
#include "cli_gw.h"
#include "airy_defaults.h"
#include "airy_memory.h"
#include "cli_render.h"
#include "platform.h"
/* Windows 无 dirent.h：airy_dirent.h 在 _WIN32 下以 FindFirstFileA 提供
   DIR/opendir/readdir/closedir（#112 实证 cmd_system.c C2065 DIR） */
#include "airy_dirent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_GW_PROBE_TIMEOUT_MS 1000

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* ==================== shared infrastructure ==================== */

const cli_daemon_desc_t CLI_DAEMONS[] = {
    {"agent", "agent.sock", "health_check"},
    {"tool", "tool.sock", "health_check"},
    {"hook", "hook.sock", "health_check"},
    {"think", "think.sock", "health_check"},
    {"monit", "monit.sock", "health_check"},
    {"sched", "sched.sock", "health_check"},
    {"channel", "channel.sock", "health_check"},
    {"market", "market.sock", "health_check"},
    {"llm", "llm.sock", "health_check"},
    {"cupolas", "cupolas.sock", "health_check"},
    {"mem", "mem.sock", "health_check"},
    {"notify", "notify.sock", "health_check"},
    {"a2a", "a2a.sock", "health_check"},
};

/* 容量口径唯一来源：仅在表定义所在 TU 成立（见 airy_cli_cmd_internal.h）。 */
#define CLI_DAEMONS_COUNT (sizeof(CLI_DAEMONS) / sizeof(CLI_DAEMONS[0]))

size_t cli_daemons_count(void) { return CLI_DAEMONS_COUNT; }

/* 运行时自动发现（0.1.7）：枚举 $AIRY_HOME/bin 下所有 *_d 后缀可执行
   （排除 gateway_d 与 maths_d——前者为 HTTP 服务、后者为符号计算后端
   无 socket）动态生成 daemon 表。daemon 增删后 /daemons 与 /rpc 自动
   适配，无需改硬编码表。
   socket 名约定 = ns + ".sock"（与 CLI_DAEMONS 一致）。无 AIRY_HOME 或
   目录不可枚举时返回 0，调用方回退 CLI_DAEMONS 默认表。 */
static size_t cli_daemons_discover(cli_daemon_desc_t *out, size_t cap)
{
    size_t n = 0;
    const char *home = getenv("AIRY_HOME");
    if (!home || !*home) return 0;
    char bin[1024];
    snprintf(bin, sizeof(bin), "%s/bin", home);
    DIR *dir = opendir(bin);
    if (!dir) return 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && n < cap) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
#ifdef _WIN32
        /* FindFirstFileA 会列出 *.exe；剥离后缀后再按 *_d 约定匹配 */
        if (len > 4 && strcmp(name + len - 4, ".exe") == 0)
            len -= 4;
#endif
        char stem[33];
        if (len >= sizeof(stem)) len = sizeof(stem) - 1;
        __builtin_memcpy(stem, name, len);
        stem[len] = '\0';
        if (len < 3 || strcmp(stem + len - 2, "_d") != 0) continue;
        if (strcmp(stem, "gateway_d") == 0 || strcmp(stem, "maths_d") == 0) continue;
        static char ns_pool[32][32];
        static char sock_pool[32][64];
        size_t nlen = len - 2;
        if (nlen >= sizeof(ns_pool[n])) nlen = sizeof(ns_pool[n]) - 1;
        __builtin_memcpy(ns_pool[n], stem, nlen);
        ns_pool[n][nlen] = '\0';
        snprintf(sock_pool[n], sizeof(sock_pool[n]), "%s.sock", ns_pool[n]);
        out[n].ns = ns_pool[n];
        out[n].sock = sock_pool[n];
        out[n].health_method = "health_check";
        n++;
    }
    closedir(dir);
    return n;
}

const char *cli_rt_dir(void)
{
    return airy_runtime_dir();
}

int cli_ns_resolve(const char *in, char *out, size_t out_cap)
{
    size_t i, in_len = strlen(in);
    char trimmed[64];
    const char *probe = in;
    if (in_len > 2 && strcmp(in + in_len - 2, "_d") == 0) {
        size_t tlen = in_len - 2;
        if (tlen >= sizeof(trimmed)) tlen = sizeof(trimmed) - 1;
        __builtin_memcpy(trimmed, in, tlen);
        trimmed[tlen] = '\0';
        probe = trimmed;
    }
    for (i = 0; i < CLI_DAEMONS_COUNT; i++) {
        if (strcmp(probe, CLI_DAEMONS[i].ns) == 0) {
            AIRY_STRNCPY_TERM(out, CLI_DAEMONS[i].ns, out_cap);
            return 0;
        }
    }
    /* 0.1.6h：默认表未命中时运行时发现（daemon 增删后自动可寻址） */
    {
        cli_daemon_desc_t dyn[32];
        size_t n = cli_daemons_discover(dyn, 32);
        for (i = 0; i < n; i++) {
            if (strcmp(probe, dyn[i].ns) == 0) {
                AIRY_STRNCPY_TERM(out, dyn[i].ns, out_cap);
                return 0;
            }
        }
    }
    return 1;
}

const char *cli_ns_sock(const char *ns)
{
    static char buf[512];
#ifdef _WIN32
    /* Windows 无命名管道：daemon IPC 走 TCP 回环（daemon_main.h 平台分支
     * 强制），主机恒为 127.0.0.1，端口取自 airy_defaults.h 私有端口带
     * SSoT（唯一权威定义，此处禁止复刻字面量）。 */
    static const struct { const char *ns; unsigned port; } WIN_NS_TCP[] = {
        {"llm", AIRY_PORT_LLM_D},         {"tool", AIRY_PORT_TOOL_D},
        {"market", AIRY_PORT_MARKET_D},   {"sched", AIRY_PORT_SCHED_D},
        {"mem", AIRY_PORT_MEM_D},         {"agent", AIRY_PORT_AGENT_D},
        {"a2a", AIRY_PORT_A2A_D},         {"maths", AIRY_PORT_MATHS_D},
        {"cupolas", AIRY_PORT_CUPOLAS_D}, {"think", AIRY_PORT_THINK_D},
        {"hook", AIRY_PORT_HOOK},         {"channel", AIRY_PORT_CHANNEL_D},
        {"monit", AIRY_PORT_MONIT_D},     {"notify", AIRY_PORT_NOTIFY_D},
    };
    for (size_t i = 0; i < sizeof(WIN_NS_TCP) / sizeof(WIN_NS_TCP[0]); i++) {
        if (strcmp(ns, WIN_NS_TCP[i].ns) == 0) {
            snprintf(buf, sizeof(buf), "127.0.0.1:%u", WIN_NS_TCP[i].port);
            return buf;
        }
    }
    snprintf(buf, sizeof(buf), "%s\\%s.sock", cli_rt_dir(), ns);
#else
    snprintf(buf, sizeof(buf), "%s/%s.sock", cli_rt_dir(), ns);
#endif
    return buf;
}

/* 0.1.19 直印化：Sub <ns> Agent 动态头（ns 运行期可变，无法字面量直印）。
 * is_err 非零走 cli_print_line（-p 下保留），零走 cli_print_status（-p 下
 * 静默），与直印化前的 -p 语义逐点等价。 */
void cli_sub_print(int is_err, const char *ns, const char *text)
{
    char hdr[80];
    snprintf(hdr, sizeof(hdr), "Sub %s Agent", ns);
    if (is_err)
        cli_print_line(hdr, text);
    else
        cli_print_status(hdr, text);
}

void cli_rpc_print(const char *ns, const char *method, const char *params_json)
{
    char *result = NULL;
    char full_method[160];
    snprintf(full_method, sizeof(full_method), "%s.%s", ns, method);
    int rc = cli_gw_call(full_method, params_json, CLI_RPC_TIMEOUT_MS, &result);
    if (rc != 0 || !result) {
        char line[160];
        snprintf(line, sizeof(line), "%s.%s 调用失败：%s", ns, method, cli_err_desc(rc));
        cli_sub_print(1, ns, line);
        AIRY_FREE(result);
        return;
    }
    if (g_cli_print_mode) {
        cli_outf("%s\n", result);
        AIRY_FREE(result);
        return;
    }
    cli_sub_print(0, ns, result);
    AIRY_FREE(result);
}

/* ==================== /rpc + /daemons ==================== */

int cmd_rpc(const char *arg, void *ctx)
{
    (void)ctx;
    if (!arg || arg[0] == '\0') {
        cli_print_status("Sub Agent:usage", "/rpc <ns>.<method> [json参数]");
        cli_outf("    例: /rpc mem.search {\"query\":\"hello\"}\n");
        cli_outf("        /rpc cupolas.check_permission {\"agent_id\":\"a\",\"action\":\"read\","
               "\"resource\":\"fs:///tmp/x\"}\n");
        return 0;
    }
    const char *dot = strchr(arg, '.');
    if (!dot) {
        cli_print_line("Sub Agent:用法", "需要 <ns>.<method> 形式");
        return 0;
    }
    char ns[64];
    size_t ns_len = (size_t)(dot - arg);
    if (ns_len >= sizeof(ns)) ns_len = sizeof(ns) - 1;
    __builtin_memcpy(ns, arg, ns_len);
    ns[ns_len] = '\0';

    char ns_resolved[64];
    if (cli_ns_resolve(ns, ns_resolved, sizeof(ns_resolved)) != 0) {
        cli_print_line("Sub Agent:RPC",
                       "未知 daemon 命名空间，/daemons 查看在线列表");
        return 0;
    }

    const char *rest = dot + 1;
    char method[128];
    size_t mlen = 0;
    while (rest[mlen] && rest[mlen] != ' ') mlen++;
    if (mlen >= sizeof(method)) mlen = sizeof(method) - 1;
    __builtin_memcpy(method, rest, mlen);
    method[mlen] = '\0';

    const char *params = NULL;
    if (rest[mlen] == ' ') params = rest + mlen + 1;

    cli_rpc_print(ns_resolved, method, params);
    return 0;
}

/* 一次 health_check 探测：rc 与结果体双条件，与 /daemon status 的
 * cli_daemon_online 保持同一判据来源（daemon_rpc_client 的返回约定）。 */
static int daemon_state(const char *ns, const char *method)
{
    char *result = NULL;
    int rc = daemon_rpc_call(cli_ns_sock(ns), method, NULL, &result, 6000);
    int up = (rc == 0 && result) ? 1 : 0;
    AIRY_FREE(result);
    return up;
}

int cmd_daemons(const char *arg, void *ctx)
{
    (void)arg; (void)ctx;
    int online = 0;
    /* 0.1.7：运行时发现 daemon 表（daemon 增删自动适配），失败回退默认表 */
    cli_daemon_desc_t dyn[32];
    size_t dcount = cli_daemons_discover(dyn, 32);
    size_t total = dcount ? dcount : CLI_DAEMONS_COUNT;
    const cli_daemon_desc_t *tab = dcount ? dyn : CLI_DAEMONS;
    cli_print_status("Super Agent:daemon", "health check");
    for (size_t i = 0; i < total; i++) {
        int up = daemon_state(tab[i].ns, tab[i].health_method);
        if (g_cli_print_mode) cli_outf("%s %s\n", tab[i].ns, up ? "online" : "offline");
        else cli_print_status(tab[i].ns, up ? "在线 100%" : "离线 0%");
        online += up;
    }
    /* gateway 在线判据唯一化（0.1.19 §458）：此前此处手写 TCP 连通性探测，
     * 与 main.c 的 cli_gw_health 构成两套定义——TCP 可连而 HTTP /health 未
     * 就绪时误报在线。现统一取 cli_gw_health（GET /health + healthy 判定，
     * 端口经 cli_gw_endpoint 解析，兼容端口漂移）。 */
    int gw_ok = cli_gw_health(CLI_GW_PROBE_TIMEOUT_MS);
    if (g_cli_print_mode) cli_outf("gateway %s\n", gw_ok ? "online" : "offline");
    else cli_print_status("gateway", gw_ok ? "在线 100%" : "离线 0%");
    if (gw_ok) online++;
    {
        char line[64];
        snprintf(line, sizeof(line), "online %d/%zu", online, total + 1);
        if (g_cli_print_mode) cli_outf("%s\n", line);
        else cli_print_status("Super Agent:daemon", line);
    }
    return 0;
}

/* ==================== /daemon + /stats + simple wrappers ==================== */

/* 动作与命名空间共享同一套空白分词；解析结果一律持有堆副本，
 * 释放路径不再依赖「指针是否等于静态表项」这一脆弱判定。 */
static const char *dm_take(const char *p, char *buf, size_t cap)
{
    while (*p == ' ')
        p++;
    if (*p == '\0')
        return NULL;
    size_t n = 0;
    while (p[n] != '\0' && p[n] != ' ')
        n++;
    size_t c = n < cap - 1 ? n : cap - 1;
    __builtin_memcpy(buf, p, c);
    buf[c] = '\0';
    return p + n;
}

static size_t dm_targets(const char *p, const char **out, size_t cap)
{
    size_t n = 0;
    char tok[64];
    while (n < cap) {
        p = dm_take(p, tok, sizeof(tok));
        if (!p)
            break;
        char resolved[64];
        if (cli_ns_resolve(tok, resolved, sizeof(resolved)) != 0) {
            char hdr[80];
            snprintf(hdr, sizeof(hdr), "Sub Agent:%s", tok);
            cli_print_line(hdr, "未知 daemon 命名空间，已忽略");
            continue;
        }
        out[n] = AIRY_STRDUP(resolved);
        if (out[n])
            n++;
    }
    return n;
}

static void dm_defaults(const char **out, size_t *n)
{
    size_t i;
    for (i = 0; i < CLI_DAEMONS_COUNT; i++) {
        out[i] = AIRY_STRDUP(CLI_DAEMONS[i].ns);
        if (!out[i])
            break;
    }
    *n = i;
}

static void dm_free(const char **ns, size_t n)
{
    for (size_t i = 0; i < n; i++)
        AIRY_FREE((void *)ns[i]);
}

static void dm_status(const char **ns, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int up = cli_daemon_online(ns[i]);
        if (g_cli_print_mode)
            cli_outf("%s %s\n", ns[i], up ? "online" : "offline");
        else
            cli_print_status(ns[i], up ? "在线 100%" : "离线 0%");
    }
}

static void dm_start(const char **ns, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (cli_daemon_start(ns[i]) != 0)
            continue;
        const char *st = cli_daemon_wait_online(ns[i], 8000) ? "started" : "starting";
        if (g_cli_print_mode)
            cli_outf("%s %s\n", ns[i], st);
        else
            cli_sub_print(0, ns[i], st);
    }
}

static void dm_stop(const char **ns, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int rc = cli_daemon_stop(ns[i]);
        const char *st = rc == 0 ? "stopped" : "stop failed (timeout)";
        if (g_cli_print_mode)
            cli_outf("%s %s\n", ns[i], st);
        else
            cli_sub_print(rc != 0, ns[i], st);
    }
}

static void dm_restart(const char **ns, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        cli_daemon_stop(ns[i]);
        if (cli_daemon_start(ns[i]) != 0)
            continue;
        const char *st = cli_daemon_wait_online(ns[i], 8000) ? "restarted" : "restarting";
        if (g_cli_print_mode)
            cli_outf("%s %s\n", ns[i], st);
        else
            cli_sub_print(0, ns[i], st);
    }
}

int cmd_daemon(const char *arg, void *ctx)
{
    (void)ctx;
    char action[16] = "status";
    const char *ns[CLI_DAEMONS_COUNT];
    size_t n = 0;

    if (arg) {
        const char *p = dm_take(arg, action, sizeof(action));
        if (p)
            n = dm_targets(p, ns, CLI_DAEMONS_COUNT);
    }
    if (n == 0)
        dm_defaults(ns, &n);

    if (strcmp(action, "status") == 0)
        dm_status(ns, n);
    else if (strcmp(action, "start") == 0)
        dm_start(ns, n);
    else if (strcmp(action, "stop") == 0)
        dm_stop(ns, n);
    else if (strcmp(action, "restart") == 0)
        dm_restart(ns, n);
    else
        cli_print_line("Sub Agent:usage",
                       "/daemon start|stop|restart|status [ns...]（默认全部）");

    dm_free(ns, n);
    return 0;
}

#ifdef AIRY_HAS_CJSON
/* mem_d 统计含语义缓存命中率与上下文台账两层子对象，默认视图下拆行可读渲染；
 * --print / --json 仍输出原始 JSON 供脚本消费。 */
static void cli_mem_stats_show(const char *ns, const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        cli_sub_print(0, ns, json);
        return;
    }

    char line[200];
    const cJSON *records = cJSON_GetObjectItem(root, "records");
    const cJSON *max_records = cJSON_GetObjectItem(root, "max_records");
    if (cJSON_IsNumber(records)) {
        snprintf(line, sizeof(line), "records %d / %d",
                 records->valueint, cJSON_IsNumber(max_records) ? max_records->valueint : 0);
        cli_sub_print(0, ns, line);
    }

    const cJSON *cache = cJSON_GetObjectItem(root, "cache");
    if (cJSON_IsObject(cache)) {
        const cJSON *e = cJSON_GetObjectItem(cache, "entries");
        const cJSON *h = cJSON_GetObjectItem(cache, "hits");
        const cJSON *m = cJSON_GetObjectItem(cache, "misses");
        const cJSON *r = cJSON_GetObjectItem(cache, "hit_rate");
        const cJSON *v = cJSON_GetObjectItem(cache, "evictions");
        /* 无查询样本时 hit_rate 为负：呈现"不可用"，不得伪零（§2.1-6） */
        char rate[16];
        if (cJSON_IsNumber(r) && r->valuedouble >= 0.0)
            snprintf(rate, sizeof(rate), "%.1f%%", r->valuedouble * 100.0);
        else
            snprintf(rate, sizeof(rate), "unavailable");
        snprintf(line, sizeof(line),
                 "semantic cache: entries=%d hits=%d misses=%d hit_rate=%s evictions=%d",
                 e ? e->valueint : 0, h ? h->valueint : 0, m ? m->valueint : 0, rate,
                 v ? v->valueint : 0);
        cli_sub_print(0, ns, line);
    }

    const cJSON *ledger = cJSON_GetObjectItem(root, "ledger");
    if (cJSON_IsObject(ledger)) {
        const cJSON *s = cJSON_GetObjectItem(ledger, "sessions");
        const cJSON *e = cJSON_GetObjectItem(ledger, "entries");
        const cJSON *t = cJSON_GetObjectItem(ledger, "total_tokens");
        snprintf(line, sizeof(line), "ledger: sessions=%d entries=%d total_tokens=%d",
                 s ? s->valueint : 0, e ? e->valueint : 0, t ? t->valueint : 0);
        cli_sub_print(0, ns, line);
    }

    cJSON_Delete(root);
}

/* llm_d 统计含进程内缓存命中率与逐模型 token/成本累计。 */
static void cli_llm_stats_show(const char *ns, const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        cli_sub_print(0, ns, json);
        return;
    }

    char line[200];
    const cJSON *size = cJSON_GetObjectItem(root, "llm_cache_size");
    const cJSON *cap = cJSON_GetObjectItem(root, "llm_cache_capacity");
    const cJSON *h = cJSON_GetObjectItem(root, "llm_cache_hits");
    const cJSON *m = cJSON_GetObjectItem(root, "llm_cache_misses");
    const cJSON *r = cJSON_GetObjectItem(root, "llm_cache_hit_rate");
    if (size || h) {
        snprintf(line, sizeof(line),
                 "llm cache: size=%d/%d hits=%d misses=%d hit_rate=%.1f%%",
                 size ? size->valueint : 0, cap ? cap->valueint : 0, h ? h->valueint : 0,
                 m ? m->valueint : 0, r ? r->valuedouble * 100.0 : 0.0);
        cli_sub_print(0, ns, line);
    }

    const cJSON *cost = cJSON_GetObjectItem(root, "cost");
    const cJSON *models = cJSON_IsObject(cost) ? cJSON_GetObjectItem(cost, "models") : NULL;
    if (cJSON_IsArray(models)) {
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, models) {
            const cJSON *name = cJSON_GetObjectItem(entry, "model");
            const cJSON *in = cJSON_GetObjectItem(entry, "prompt_tokens");
            const cJSON *out = cJSON_GetObjectItem(entry, "completion_tokens");
            const cJSON *usd = cJSON_GetObjectItem(entry, "cost_usd");
            snprintf(line, sizeof(line), "usage %s: in=%d out=%d cost=$%.6f",
                     cJSON_IsString(name) ? name->valuestring : "?", in ? in->valueint : 0,
                     out ? out->valueint : 0, usd ? usd->valuedouble : 0.0);
            cli_sub_print(0, ns, line);
        }
    }

    cJSON_Delete(root);
}

/* think_d 推理语言网关统计：Tokenizer 特征校准状态与逐模型语言画像。
 * 默认视图拆分呈现校准漂移；--print / --json 输出原始 JSON。 */
static void cli_lang_stats_show(const char *ns, const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        cli_sub_print(0, ns, json);
        return;
    }

    char line[200];
    const cJSON *pc = cJSON_GetObjectItem(root, "process_count");
    const cJSON *cc = cJSON_GetObjectItem(root, "calibrate_count");
    const cJSON *dd = cJSON_GetObjectItem(root, "drift_detected");
    const cJSON *ri = cJSON_GetObjectItem(root, "recalibrate_interval");
    snprintf(line, sizeof(line), "lang gateway: process=%d calibrate=%d drift=%d interval=%d",
             pc ? pc->valueint : 0, cc ? cc->valueint : 0,
             dd ? dd->valueint : 0, ri ? ri->valueint : 0);
    cli_sub_print(0, ns, line);

    const cJSON *profiles = cJSON_GetObjectItem(root, "profiles");
    if (cJSON_IsArray(profiles)) {
        cJSON *p = NULL;
        cJSON_ArrayForEach(p, profiles) {
            const cJSON *mid = cJSON_GetObjectItem(p, "model_id");
            const cJSON *fam = cJSON_GetObjectItem(p, "family");
            const cJSON *nl = cJSON_GetObjectItem(p, "native_lang");
            const cJSON *ratio = cJSON_GetObjectItem(p, "lang_ratio");
            const cJSON *conf = cJSON_GetObjectItem(p, "lang_confidence");
            snprintf(line, sizeof(line),
                     "profile %s: family=%s native=%s ratio=%.3f conf=%.3f",
                     cJSON_IsString(mid) ? mid->valuestring : "?",
                     cJSON_IsString(fam) ? fam->valuestring : "?",
                     cJSON_IsString(nl) ? nl->valuestring : "?",
                     ratio ? ratio->valuedouble : 0.0,
                     conf ? conf->valuedouble : 0.0);
            cli_sub_print(0, ns, line);
        }
    }

    cJSON_Delete(root);
}
#endif /* AIRY_HAS_CJSON */

static void cli_stats_print(const char *ns, const char *method)
{
    if (strcmp(ns, "lang") == 0) {
#ifdef AIRY_HAS_CJSON
        if (!g_cli_print_mode && !g_cli_json_mode) {
            char *result = NULL;
            if (cli_gw_call("think.lang_stats", NULL, CLI_RPC_TIMEOUT_MS, &result) == 0 &&
                result) {
                cli_lang_stats_show(ns, result);
                AIRY_FREE(result);
                return;
            }
            AIRY_FREE(result);
        }
#endif
        cli_rpc_print("think", "lang_stats", NULL);
        return;
    }
#ifdef AIRY_HAS_CJSON
    if (!g_cli_print_mode && !g_cli_json_mode &&
        (strcmp(ns, "mem") == 0 || strcmp(ns, "llm") == 0)) {
        char method_buf[32];
        snprintf(method_buf, sizeof(method_buf), "%s.get_stats", ns);
        char *result = NULL;
        if (cli_gw_call(method_buf, NULL, CLI_RPC_TIMEOUT_MS, &result) == 0 && result) {
            if (strcmp(ns, "mem") == 0)
                cli_mem_stats_show(ns, result);
            else
                cli_llm_stats_show(ns, result);
            AIRY_FREE(result);
            return;
        }
        AIRY_FREE(result);
    }
#endif
    cli_rpc_print(ns, method, NULL);
}

int cmd_stats(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && arg[0]) cli_stats_print(arg, "get_stats");
    else { for (size_t i = 0; i < CLI_DAEMONS_COUNT; i++) cli_stats_print(CLI_DAEMONS[i].ns, "get_stats"); }
    return 0;
}

int cmd_agents(const char *arg, void *ctx)    { (void)ctx; cli_rpc_print("agent", arg && arg[0] ? arg : "list", NULL); return 0; }
int cmd_tools(const char *arg, void *ctx)     { (void)ctx; cli_rpc_print("tool", "list_tools", NULL); return 0; }
int cmd_hooks(const char *arg, void *ctx)     { (void)ctx; cli_rpc_print("hook", "list", NULL); return 0; }
int cmd_plugins(const char *arg, void *ctx)   { (void)ctx; cli_rpc_print("plugin", "list", NULL); return 0; }
int cmd_channels(const char *arg, void *ctx)  { (void)ctx; cli_rpc_print("channel", "list", NULL); return 0; }
int cmd_models(const char *arg, void *ctx)    { (void)ctx; cli_rpc_print("llm", "list_models", NULL); return 0; }
