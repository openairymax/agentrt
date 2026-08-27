// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_cmd_cognition.c
 * @brief Cognition-face commands: apikey, mem, a2a, metrics, alerts, tasks.
 *
 * Split from daemon_cmds.c.  Owns the model API key configuration
 * (/apikey), memory chain display (/mem), A2A discovery (/a2a),
 * observability queries (/metrics, /alerts), and task scheduler
 * inspection (/tasks).
 */

#include "daemon_cmds.h"
#include "airy_cli_cmd_internal.h"
#include "cli_gw.h"
#include "airy_memory.h"
#include "cli_render.h"
#include "platform.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/* ==================== /apikey ==================== */

static int cli_secrets_path(char *buf, size_t cap)
{
    const char *cdir = getenv("AIRY_CONFIG_DIR");
    if (cdir && cdir[0]) { snprintf(buf, cap, "%s/secrets.env", cdir); return 1; }
    const char *home = getenv("AIRY_HOME");
    if (home && home[0]) { snprintf(buf, cap, "%s/config/secrets.env", home); return 1; }
    return 0;
}

static void cli_secret_masked(const char *val, char *out, size_t cap)
{
    size_t len = val ? strlen(val) : 0;
    if (len == 0) { snprintf(out, cap, "(empty)"); return; }
    if (len <= 8) { snprintf(out, cap, "****"); return; }
    snprintf(out, cap, "%.*s…%s", 4, val, val + len - 4);
}

int cmd_apikey(const char *arg, void *ctx)
{
    (void)ctx;
    char path[AIRY_PATH_MAX];
    if (!cli_secrets_path(path, sizeof(path))) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey",
                             "无法定位 secrets.env（未设置 AIRY_HOME/AIRY_CONFIG_DIR）");
        return 0;
    }
    if (!arg || arg[0] == '\0' || strncmp(arg, "list", 4) == 0) {
        FILE *f = fopen(path, "r");
        if (!f) {
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey",
                                 "无法读取 secrets.env（文件不存在或不可读）");
            return 0;
        }
        char line[1024]; int found = 0;
        cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_SUB_AGENT, "apikey",
                             "已配置模型 Key 位（值打码，完整值见 secrets.env）:");
        while (fgets(line, sizeof(line), f)) {
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0'; char *val = eq + 1;
            size_t vlen = strlen(val);
            while (vlen > 0 && (val[vlen - 1] == '\n' || val[vlen - 1] == '\r')) val[--vlen] = '\0';
            if (strncmp(line, "MODEL_", 6) == 0 && strstr(line, "_API_KEY") != NULL) {
                char masked[64]; cli_secret_masked(val, masked, sizeof(masked));
                cli_outf("  %s %s %s= %s%s\n", cli_c(CLR_GREEN), line, cli_c(CLR_RESET),
                         cli_c(CLR_DIM), masked);
                cli_outf("%s", cli_c(CLR_RESET));
                found = 1;
            }
        }
        fclose(f);
        if (!found) cli_outf("  %s（无 Key 位；使用 /apikey set <N> <key> 添加）%s\n",
                             cli_c(CLR_DIM), cli_c(CLR_RESET));
        return 0;
    }
    if (strncmp(arg, "set ", 4) != 0) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                             "/apikey list | set <N> <key>（N = model.yaml 连接表行号）");
        return 0;
    }
    const char *rest = arg + 4;
    char *sp = strchr(rest, ' ');
    if (!sp) {
        cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage", "/apikey set <N> <key>");
        return 0;
    }
    size_t nlen = (size_t)(sp - rest);
    if (nlen == 0 || nlen > 4) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "行号 N 非法（1-64）");
        return 0;
    }
    char num[8]; __builtin_memcpy(num, rest, nlen); num[nlen] = '\0';
    for (size_t i = 0; i < nlen; i++) {
        if (num[i] < '0' || num[i] > '9') {
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "行号 N 非法（须为数字）");
            return 0;
        }
    }
    int idx = atoi(num);
    if (idx < 1 || idx > 64) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "行号 N 超出范围（1-64）");
        return 0;
    }
    const char *key = sp + 1;
    if (!key[0]) { cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "key 为空"); return 0; }

    FILE *f = fopen(path, "r");
    char **lines = NULL; size_t line_count = 0;
    if (f) {
        char lbuf[1024];
        while (fgets(lbuf, sizeof(lbuf), f)) {
            char **nl = (char **)AIRY_REALLOC(lines, (line_count + 1) * sizeof(char *));
            if (!nl) { for (size_t i = 0; i < line_count; i++) AIRY_FREE(lines[i]); AIRY_FREE(lines); fclose(f);
                       cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "OOM"); return 0; }
            lines = nl; lines[line_count++] = AIRY_STRDUP(lbuf);
        }
        fclose(f);
    }

    char var_name[32]; snprintf(var_name, sizeof(var_name), "MODEL_%d_API_KEY", idx);
    int replaced = 0;
    char tmp[AIRY_PATH_MAX]; snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    const char *tmp_path = tmp; FILE *wf = fopen(tmp_path, "w");
    if (!wf) {
        for (size_t i = 0; i < line_count; i++) AIRY_FREE(lines[i]);
        AIRY_FREE(lines);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "无法写入 secrets.env"); return 0;
    }
    for (size_t i = 0; i < line_count; i++) {
        const char *src = lines[i]; const char *p = src;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, var_name, strlen(var_name)) == 0 && p[strlen(var_name)] == '=') {
            fprintf(wf, "%s=%s\n", var_name, key); replaced = 1;
        } else { fputs(src, wf); }
    }
    if (!replaced) fprintf(wf, "%s=%s\n", var_name, key);
    if (fclose(wf) != 0) {
        for (size_t i = 0; i < line_count; i++) AIRY_FREE(lines[i]);
        AIRY_FREE(lines);
        remove(tmp_path); cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "写入失败"); return 0;
    }
#if defined(_WIN32)
    _chmod(tmp_path, _S_IREAD | _S_IWRITE);
#else
    chmod(tmp_path, 0600);
#endif
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        for (size_t i = 0; i < line_count; i++) AIRY_FREE(lines[i]);
        AIRY_FREE(lines);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUB_AGENT, "apikey", "写回失败"); return 0;
    }
    for (size_t i = 0; i < line_count; i++) AIRY_FREE(lines[i]);
    AIRY_FREE(lines);

    char ok[128];
    snprintf(ok, sizeof(ok), "%s 已%s（%s，权限 600）", var_name, replaced ? "更新" : "添加", path);
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "apikey", ok);
    return 0;
}

/* ==================== /mem ==================== */

static void cli_mem_time(uint64_t ts, char *out, size_t cap)
{
    time_t t = (time_t)ts; struct tm ltm;
#if defined(_WIN32)
    if (localtime_s(&ltm, &t) == 0) strftime(out, cap, "%m-%d %H:%M", &ltm);
    else snprintf(out, cap, "%llu", (unsigned long long)ts);
#else
    if (localtime_r(&t, &ltm)) strftime(out, cap, "%m-%d %H:%M", &ltm);
    else snprintf(out, cap, "%llu", (unsigned long long)ts);
#endif
}

static void cli_mem_summary(const char *data, char *out, size_t cap)
{
    const char *p = data ? data : "";
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    if (strncmp(p, "user: ", 6) == 0) { p += 6; len = (len > 6) ? len - 6 : 0; }
    if (len > 60) len = 60;
    while (len > 0 && ((unsigned char)p[len] & 0xC0) == 0x80) len--;
    if (len >= cap) len = cap - 1;
    __builtin_memcpy(out, p, len); out[len] = '\0';
}

static void cli_mem_render_item(size_t idx, cJSON *item)
{
    cJSON *rid = cJSON_GetObjectItem(item, "record_id");
    cJSON *ts = cJSON_GetObjectItem(item, "created_at");
    cJSON *data = cJSON_GetObjectItem(item, "data");
    cJSON *lenj = cJSON_GetObjectItem(item, "len");
    const char *id = cJSON_IsString(rid) ? rid->valuestring : "?";
    uint64_t created = cJSON_IsNumber(ts) ? (uint64_t)ts->valuedouble : 0;
    const char *d = cJSON_IsString(data) ? data->valuestring : "";
    long dlen = cJSON_IsNumber(lenj) ? (long)lenj->valuedouble : (long)strlen(d);
    char timestr[32], sum[64];
    cli_mem_time(created, timestr, sizeof(timestr));
    cli_mem_summary(d, sum, sizeof(sum));
    cli_out(cli_c(CLR_DIM));
    cli_outf("  %-2zu%s", idx, CLI_ICON_BULLET);
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_outf(" [%s] ", timestr);
    cli_out(cli_c(CLR_RESET));
    cli_out(sum);
    if (dlen > 60) cli_outf("…");
    cli_out(cli_c(CLR_DIM));
    cli_outf("  (%ldB %.*s)", dlen, 8, id);
    cli_out(cli_c(CLR_RESET));
    cli_outc('\n');
}

static void cli_mem_render_chain(void)
{
    char *res = NULL; int total = 0;
    if (cli_gw_call("mem.count", NULL, CLI_RPC_TIMEOUT_MS, &res) == 0 && res) {
        cJSON *r = cJSON_Parse(res);
        if (r) { cJSON *c = cJSON_GetObjectItem(r, "count"); if (cJSON_IsNumber(c)) total = (int)c->valuedouble; cJSON_Delete(r); }
        AIRY_FREE(res);
    } else { AIRY_FREE(res); cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆服务不可用（mem.count）"); return; }

    char hdr[160]; snprintf(hdr, sizeof(hdr), "记忆链 · 共 %d 条（最近 %d 条）", total, 6);
    cli_render_sub_agent_line(CLI_ROLE_STATUS, "mem", hdr);

    if (cli_gw_call("mem.recent", "{\"limit\":6}", CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res); cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆链读取失败（mem.recent）"); return;
    }
    cJSON *root = cJSON_Parse(res); AIRY_FREE(res);
    if (!root) { cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆链响应解析失败"); return; }
    cJSON *records = cJSON_GetObjectItem(root, "records");
    size_t n = cJSON_IsArray(records) ? (size_t)cJSON_GetArraySize(records) : 0;
    if (n == 0) { cli_outf("  %s 暂无记忆记录，对话一次即可生成记忆\n", CLI_ICON_INFO); cJSON_Delete(root); return; }
    for (size_t i = 0; i < n; i++) cli_mem_render_item(i + 1, cJSON_GetArrayItem(records, (int)i));
    cJSON_Delete(root);
}

static void cli_mem_render_search(const char *query)
{
    char params[1024]; snprintf(params, sizeof(params), "{\"query\":\"%s\",\"limit\":5}", query);
    char *res = NULL;
    if (cli_gw_call("mem.search", params, CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res); cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆检索失败（mem.search）"); return;
    }
    cJSON *root = cJSON_Parse(res); AIRY_FREE(res);
    if (!root) { cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆检索响应解析失败"); return; }
    cJSON *results = cJSON_GetObjectItem(root, "results");
    size_t n = cJSON_IsArray(results) ? (size_t)cJSON_GetArraySize(results) : 0;
    char hdr[192]; snprintf(hdr, sizeof(hdr), "检索记忆 '%s' · %zu 条命中", query, n);
    cli_render_sub_agent_line(CLI_ROLE_STATUS, "mem", hdr);

    for (size_t i = 0; i < n; i++) {
        cJSON *hit = cJSON_GetArrayItem(results, (int)i);
        const char *id = cJSON_IsString(cJSON_GetObjectItem(hit, "record_id")) ? cJSON_GetObjectItem(hit, "record_id")->valuestring : "";
        char gp[256]; snprintf(gp, sizeof(gp), "{\"record_id\":\"%s\"}", id);
        char *g = NULL; cJSON *detail = NULL;
        if (cli_gw_call("mem.get", gp, CLI_RPC_TIMEOUT_MS, &g) == 0 && g) { detail = cJSON_Parse(g); AIRY_FREE(g); } else { AIRY_FREE(g); }
        char dcopy[8192]; dcopy[0] = '\0'; long dlen = 0;
        if (detail) {
            cJSON *dj = cJSON_GetObjectItem(detail, "data"); cJSON *lj = cJSON_GetObjectItem(detail, "length");
            if (cJSON_IsString(dj) && dj->valuestring) {
                size_t dl = strlen(dj->valuestring); if (dl >= sizeof(dcopy)) dl = sizeof(dcopy) - 1;
                __builtin_memcpy(dcopy, dj->valuestring, dl); dcopy[dl] = '\0';
                dlen = cJSON_IsNumber(lj) ? (long)lj->valuedouble : (long)dl;
            }
            cJSON_Delete(detail);
        }
        char sum[64]; cli_mem_summary(dcopy, sum, sizeof(sum));
        cli_out(cli_c(CLR_DIM)); cli_outf("  %-2zu%s", i + 1, CLI_ICON_BULLET); cli_out(cli_c(CLR_RESET));
        cli_outf(" %.3f  ", cJSON_IsNumber(cJSON_GetObjectItem(hit, "score")) ? cJSON_GetObjectItem(hit, "score")->valuedouble : 0.0);
        cli_out(sum); if (dlen > 60) cli_outf("…");
        cli_out(cli_c(CLR_DIM)); cli_outf("  (%.*s)", 8, id); cli_out(cli_c(CLR_RESET)); cli_outc('\n');
    }
    cJSON_Delete(root);
}

static void cli_mem_render_get(const char *record_id)
{
    char params[512]; snprintf(params, sizeof(params), "{\"record_id\":\"%s\"}", record_id);
    char *res = NULL;
    if (cli_gw_call("mem.get", params, CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res); cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆读取失败（mem.get）"); return;
    }
    cJSON *root = cJSON_Parse(res); AIRY_FREE(res);
    if (!root) { cli_render_sub_agent_line(CLI_ROLE_ERROR, "mem", "记忆响应解析失败"); return; }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *lenj = cJSON_GetObjectItem(root, "length");
    cJSON *meta = cJSON_GetObjectItem(root, "metadata");
    char hdr[160]; snprintf(hdr, sizeof(hdr), "记忆 %s · %ldB", record_id,
                            cJSON_IsNumber(lenj) ? (long)lenj->valuedouble : 0L);
    cli_render_sub_agent_line(CLI_ROLE_STATUS, "mem", hdr);
    if (cJSON_IsString(meta)) { cli_out(cli_c(CLR_DIM)); cli_outf("  元数据: %s\n", meta->valuestring); cli_out(cli_c(CLR_RESET)); }
    if (cJSON_IsString(data) && data->valuestring) { cli_out(cli_c(CLR_RESET)); cli_outf("%s\n", data->valuestring); }
    else { cli_outf("  %s 记录为空或已删除\n", CLI_ICON_INFO); }
    cJSON_Delete(root);
}

int cmd_mem(const char *arg, void *ctx)
{
    (void)ctx;
    if (arg && strncmp(arg, "get ", 4) == 0) { cli_mem_render_get(arg + 4); return 0; }
    if (arg && arg[0] != '\0') { cli_mem_render_search(arg); return 0; }
    cli_mem_render_chain(); return 0;
}

/* ==================== simple cognition wrappers ==================== */

int cmd_a2a(const char *arg, void *ctx)      { (void)ctx; cli_rpc_print("a2a", "discover_agents", NULL); return 0; }
int cmd_metrics(const char *arg, void *ctx)   { (void)ctx; cli_rpc_print("observe", "query_metrics", NULL); return 0; }
int cmd_alerts(const char *arg, void *ctx)    { (void)ctx; cli_rpc_print("monit", "get_alerts", NULL); return 0; }
int cmd_tasks(const char *arg, void *ctx)     { (void)ctx; cli_rpc_print("sched", "get_stats", NULL); cli_rpc_print("sched", "checkpoint_save", NULL); return 0; }
