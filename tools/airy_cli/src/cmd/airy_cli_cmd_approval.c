// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_cmd_approval.c
 * @brief 审批面命令：/pending（待批工具调用只读视图）、/approve（决议回传）。
 *
 * 权威状态在 tool_d（gateway 转发），本层只提供用户面只读视图与决议回传，
 * 不持有任何审批状态，与 TUI 前端同源同契约。
 */

#include "daemon_cmds.h"
#include "airy_cli_cmd_internal.h"
#include "cli_gw.h"
#include "airy_memory.h"
#include "cli_render.h"
#include "platform.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* created_at 为毫秒时间戳（tool_d interactive_approval_block）。 */
static void cli_approval_time(uint64_t ts, char *out, size_t cap)
{
    time_t t = (time_t)(ts / 1000u);
    struct tm ltm;
    if (airy_localtime_r(&t, &ltm) == 0 && strftime(out, cap, "%m-%d %H:%M:%S", &ltm) > 0)
        return;
    snprintf(out, cap, "%llu", (unsigned long long)ts);
}

/* result 可能被再包一层字符串（与 TUI 客户端同容错）。 */
static cJSON *cli_approval_unwrap(cJSON *root)
{
    cJSON *inner = cJSON_IsString(root) ? cJSON_Parse(root->valuestring) : NULL;
    if (!inner)
        return root;
    cJSON_Delete(root);
    return inner;
}

static const char *cli_approval_str(cJSON *obj, const char *key, const char *fallback)
{
    cJSON *v = cJSON_GetObjectItem(obj, key);
    return (cJSON_IsString(v) && v->valuestring) ? v->valuestring : fallback;
}

static void cli_approval_item(size_t idx, cJSON *item)
{
    const char *id = cli_approval_str(item, "request_id", "?");
    const char *tool = cli_approval_str(item, "tool", "?");
    const char *agent = cli_approval_str(item, "agent_id", "-");
    const char *params = cli_approval_str(item, "params", "");
    cJSON *ts = cJSON_GetObjectItem(item, "created_at");
    char timestr[32];
    cli_approval_time(cJSON_IsNumber(ts) ? (uint64_t)ts->valuedouble : 0, timestr,
                      sizeof(timestr));

    cli_out(cli_c(CLR_DIM));
    cli_outf("  %-2zu%s", idx, CLI_ICON_TODO);
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_outf(" [%s] ", timestr);
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_CYAN));
    cli_out(tool);
    cli_out(cli_c(CLR_RESET));
    cli_out(cli_c(CLR_DIM));
    cli_outf("  (%s)\n", agent);
    cli_out(cli_c(CLR_RESET));

    cli_outf("     %s%s\n", CLI_ICON_BRANCH, id);
    if (params[0]) {
        size_t plen = cli_utf8_safe_len(params, 88);
        cli_out(cli_c(CLR_DIM));
        cli_outf("     %s %.*s%s\n", CLI_ICON_BRANCH, (int)plen, params,
                 params[plen] ? "…" : "");
        cli_out(cli_c(CLR_RESET));
    }
}

static void cli_approval_usage(void)
{
    cli_render_role_line(CLI_ROLE_STATUS, CLI_ACTOR_SUB_AGENT, "usage",
                         "/approve <request_id> allow|always|deny");
    cli_outf("    allow=允许本次 · always=始终允许 · deny=拒绝\n");
}

int cmd_pending(const char *arg, void *ctx)
{
    (void)arg;
    (void)ctx;

    char *res = NULL;
    if (cli_gw_call("tool.pending", NULL, CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "approval", "审批服务不可用（tool.pending）");
        return 0;
    }
    cJSON *root = cli_approval_unwrap(cJSON_Parse(res));
    AIRY_FREE(res);
    if (!root) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "approval", "审批响应解析失败");
        return 0;
    }

    cJSON *arr = cJSON_GetObjectItem(root, "pending");
    if (!cJSON_IsArray(arr) && cJSON_IsArray(root))
        arr = root;
    size_t n = cJSON_IsArray(arr) ? (size_t)cJSON_GetArraySize(arr) : 0;

    char hdr[128];
    snprintf(hdr, sizeof(hdr), "待批工具调用 · %zu 项", n);
    cli_render_sub_agent_line(CLI_ROLE_STATUS, "approval", hdr);

    if (n == 0) {
        cli_outf("  %s 当前无待审批请求\n", CLI_ICON_INFO);
        cJSON_Delete(root);
        return 0;
    }
    for (size_t i = 0; i < n; i++)
        cli_approval_item(i + 1, cJSON_GetArrayItem(arr, (int)i));
    cli_outf("  %s 决议：/approve <request_id> allow|always|deny\n", CLI_ICON_INFO);
    cJSON_Delete(root);
    return 0;
}

/* 决议回显标签与 TUI 前端一致（always/allow/deny）。 */
static const char *cli_approval_label(const char *decision)
{
    if (strcmp(decision, "always") == 0)
        return "始终允许";
    if (strcmp(decision, "allow") == 0)
        return "允许本次";
    return "拒绝";
}

int cmd_approve(const char *arg, void *ctx)
{
    (void)ctx;

    if (!arg || arg[0] == '\0') {
        cli_approval_usage();
        return 0;
    }
    const char *sp = strchr(arg, ' ');
    if (!sp) {
        cli_approval_usage();
        return 0;
    }

    char request_id[192];
    size_t idlen = (size_t)(sp - arg);
    if (idlen == 0 || idlen >= sizeof(request_id)) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "approval", "request_id 非法");
        return 0;
    }
    AIRY_MEMCPY(request_id, arg, idlen);
    request_id[idlen] = '\0';

    char decision[16];
    const char *tok = sp + 1;
    size_t dlen = 0;
    while (tok[dlen] && tok[dlen] != ' ' && dlen < sizeof(decision) - 1)
        dlen++;
    AIRY_MEMCPY(decision, tok, dlen);
    decision[dlen] = '\0';

    if (strcmp(decision, "allow") != 0 && strcmp(decision, "always") != 0 &&
        strcmp(decision, "deny") != 0) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "approval",
                                  "决议非法（allow / always / deny）");
        return 0;
    }

    char params[320];
    snprintf(params, sizeof(params), "{\"request_id\":\"%s\",\"decision\":\"%s\"}",
             request_id, decision);

    char *res = NULL;
    if (cli_gw_call("tool.approve", params, CLI_RPC_TIMEOUT_MS, &res) != 0 || !res) {
        AIRY_FREE(res);
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "approval", "审批决议回传失败（tool.approve）");
        return 0;
    }
    cJSON *root = cli_approval_unwrap(cJSON_Parse(res));
    AIRY_FREE(res);
    if (!root) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "approval", "审批决议响应解析失败");
        return 0;
    }
    cJSON *resolved = cJSON_GetObjectItem(root, "resolved");
    cJSON *rid = cJSON_GetObjectItem(root, "request_id");
    int ok = cJSON_IsTrue(resolved);
    char line[256];
    snprintf(line, sizeof(line), "决议%s：%s（%s）", ok ? "已回传" : "未生效",
             cli_approval_label(decision),
             (cJSON_IsString(rid) && rid->valuestring) ? rid->valuestring : request_id);
    cli_render_sub_agent_line(ok ? CLI_ROLE_STATUS : CLI_ROLE_ERROR, "approval", line);
    cJSON_Delete(root);
    return 0;
}
