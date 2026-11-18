// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_cmd_jobs.c
 * @brief D-9 作业历史视图：/jobs（列表）、/jobs show <job_id>（详情）。
 *
 * 权威记录在 tool_d（tool.jobs / tool.job），本层只提供用户面只读视图，
 * 不持有任何作业状态，与 TUI 前端同源同契约。
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

/* created_ms/finished_ms 为墙钟毫秒时间戳。 */
static void cli_job_time(uint64_t ms, char *out, size_t cap)
{
    time_t t = (time_t)(ms / 1000u);
    struct tm ltm;

    if (ms == 0) {
        snprintf(out, cap, "-");
        return;
    }
    if (airy_localtime_r(&t, &ltm) == 0 && strftime(out, cap, "%m-%d %H:%M:%S", &ltm) > 0)
        return;
    snprintf(out, cap, "%llu", (unsigned long long)ms);
}

/* result 可能被再包一层字符串（与 TUI 客户端同容错）。 */
static cJSON *cli_job_unwrap(cJSON *root)
{
    cJSON *inner = cJSON_IsString(root) ? cJSON_Parse(root->valuestring) : NULL;
    if (!inner)
        return root;
    cJSON_Delete(root);
    return inner;
}

static const char *cli_job_str(cJSON *obj, const char *key, const char *fallback)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return (cJSON_IsString(v) && v->valuestring) ? v->valuestring : fallback;
}

static int cli_job_num(cJSON *obj, const char *key)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return cJSON_IsNumber(v) ? v->valueint : 0;
}

static uint64_t cli_job_ts(cJSON *obj, const char *key)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return cJSON_IsNumber(v) ? (uint64_t)v->valuedouble : 0;
}

static void cli_job_usage(void)
{
    cli_print_status("Sub jobs Agent", "usage");
    cli_outf("    /jobs [n]               列出最近作业（缺省 20 条，最新在前）\n");
    cli_outf("    /jobs show <job_id>     查看单条作业的审计留痕\n");
}

static void cli_job_row(size_t idx, cJSON *item)
{
    char timestr[32];
    const char *id = cli_job_str(item, "job_id", "?");
    const char *tool = cli_job_str(item, "tool_id", "?");
    const char *state = cli_job_str(item, "state", "?");
    const char *agent = cli_job_str(item, "agent_id", "-");
    const char *approval = cli_job_str(item, "approval", "none");
    int risk = cli_job_num(item, "risk");

    cli_job_time(cli_job_ts(item, "created_ms"), timestr, sizeof(timestr));
    cli_outf("  %-2zu [%s] %-9s %-11s %s  agent=%s  approval=%s\n", idx, timestr, state, tool,
             risk ? "HIGH" : "low", agent, approval);
    cli_outf("     %s\n", id);
}

static void cli_job_show(const char *job_id)
{
    char params[192];
    char *res = NULL;

    snprintf(params, sizeof(params), "{\"job_id\":\"%s\"}", job_id);
    if (cli_gw_call("tool.job", params, CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res);
        cli_print_line("Sub jobs Agent", "作业查询不可用（tool.job）");
        return;
    }
    cJSON *root = cli_job_unwrap(cJSON_Parse(res));
    AIRY_FREE(res);
    if (!root) {
        cli_print_line("Sub jobs Agent", "作业响应解析失败");
        return;
    }

    char created[32];
    char finished[32];
    const char *id = cli_job_str(root, "job_id", job_id);
    const char *state = cli_job_str(root, "state", "?");
    const char *params_sum = cli_job_str(root, "params_sum", "");
    const char *output_sum = cli_job_str(root, "output_sum", "");
    const char *error = cli_job_str(root, "error", "");
    int risk = cli_job_num(root, "risk");

    cli_job_time(cli_job_ts(root, "created_ms"), created, sizeof(created));
    cli_job_time(cli_job_ts(root, "finished_ms"), finished, sizeof(finished));

    cli_print_status("Sub jobs Agent", id);
    cli_outf("  tool      %s\n", cli_job_str(root, "tool_id", "?"));
    cli_outf("  agent     %s\n", cli_job_str(root, "agent_id", "-"));
    cli_outf("  state     %s\n", state);
    cli_outf("  created   %s\n", created);
    cli_outf("  finished  %s\n", finished);
    cli_outf("  exit/err  %d / %d\n", cli_job_num(root, "exit_code"), cli_job_num(root, "err_code"));
    cli_outf("  risk      %s%s%s\n", risk ? "HIGH" : "low",
             risk && cli_job_str(root, "risk_reason", "")[0] ? " · " : "",
             risk ? cli_job_str(root, "risk_reason", "") : "");
    cli_outf("  approval  %s\n", cli_job_str(root, "approval", "none"));
    cli_outf("  sandbox   %s\n", cli_job_str(root, "sandbox", "-"));
    if (params_sum[0])
        cli_outf("  params    %s\n", params_sum);
    if (output_sum[0])
        cli_outf("  output    %s\n", output_sum);
    if (error[0]) {
        size_t elen = cli_utf8_safe_len(error, 200);
        cli_outf("  error     %.*s%s\n", (int)elen, error, error[elen] ? "…" : "");
    }
    cJSON_Delete(root);
}

static void cli_job_list(int limit)
{
    char params[64];
    char *res = NULL;

    snprintf(params, sizeof(params), "{\"limit\":%d}", limit);
    if (cli_gw_call("tool.jobs", params, CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res);
        cli_print_line("Sub jobs Agent", "作业历史不可用（tool.jobs）");
        return;
    }
    cJSON *root = cli_job_unwrap(cJSON_Parse(res));
    AIRY_FREE(res);
    if (!root) {
        cli_print_line("Sub jobs Agent", "作业历史解析失败");
        return;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "jobs");
    if (!cJSON_IsArray(arr) && cJSON_IsArray(root))
        arr = root;
    size_t n = cJSON_IsArray(arr) ? (size_t)cJSON_GetArraySize(arr) : 0;

    char hdr[96];
    snprintf(hdr, sizeof(hdr), "作业历史 · %zu 项", n);
    cli_print_status("Sub jobs Agent", hdr);
    if (n == 0) {
        cli_outf("  暂无作业记录\n");
        cJSON_Delete(root);
        return;
    }
    for (size_t i = 0; i < n; i++)
        cli_job_row(i + 1, cJSON_GetArrayItem(arr, (int)i));
    cli_outf("  详情：/jobs show <job_id> · 高危=shell_run/tool_create/包管理类命令\n");
    cJSON_Delete(root);
}

int cmd_jobs(const char *arg, void *ctx)
{
    (void)ctx;

    if (!arg || arg[0] == '\0') {
        cli_job_list(20);
        return 0;
    }
    if (strncmp(arg, "show", 4) == 0 && (arg[4] == '\0' || arg[4] == ' ')) {
        const char *job_id = arg[4] ? arg + 5 : NULL;
        if (!job_id || job_id[0] == '\0') {
            cli_print_line("Sub jobs Agent", "缺少 job_id");
            cli_job_usage();
            return 0;
        }
        cli_job_show(job_id);
        return 0;
    }
    if (strncmp(arg, "list", 4) == 0 && (arg[4] == '\0' || arg[4] == ' ')) {
        int limit = 20;
        const char *num = arg[4] ? arg + 5 : NULL;
        if (num && num[0])
            limit = atoi(num);
        cli_job_list(limit > 0 ? limit : 20);
        return 0;
    }
    /* 纯数字视为条数，兼容 /jobs 10。 */
    if (arg[0] >= '0' && arg[0] <= '9') {
        int limit = atoi(arg);
        cli_job_list(limit > 0 ? limit : 20);
        return 0;
    }
    cli_job_usage();
    return 0;
}
