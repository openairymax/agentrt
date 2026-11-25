// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_brk.c
 * @brief 熔断读数视图（0.1.21 §33.2 #5）：/daemon、/daemons 与 breakers
 *        子命令共用同一取数口。
 *
 * 判据语义唯一位于 providers 的 prov_breaker 与 llm.health_check 的发布面
 * （circuit_breaks / breakers[]），本模块只做只读取出与呈现，不设阈值、
 * 不推算冷却、不缓存。旧版 daemon 未发布熔断字段时静默跳过，避免把
 * 「未实现」误报成「无熔断」。
 */

#include "cli_internal.h"
#include "airy_cli_cmd_internal.h"
#include "daemon_rpc_client.h"

#include <cjson/cJSON.h>

#include <stdio.h>
#include <string.h>

static int brk_is_open(const cJSON *b)
{
    const cJSON *st = cJSON_GetObjectItem(b, "state");
    return cJSON_IsString(st) && st->valuestring &&
           strcmp(st->valuestring, "closed") != 0;
}

/* 汇总 breakers[]：未闭合个数、总数与全部 breaks 之和（= rejects +
 * budget_tmo，由机制层保证口径）。字段缺失按 0 处理，不推断。 */
int cli_brk_read(const char *json, cli_brk_t *out)
{
    if (!out)
        return 0;
    AIRY_MEMSET(out, 0, sizeof(*out));
    if (!json || !json[0])
        return 0;

    cJSON *root = cJSON_Parse(json);
    if (!root)
        return 0;
    const cJSON *arr = cJSON_GetObjectItem(root, "breakers");
    const cJSON *brk = cJSON_GetObjectItem(root, "circuit_breaks");
    int seen = cJSON_IsArray(arr) || cJSON_IsNumber(brk);

    if (cJSON_IsArray(arr)) {
        const cJSON *b;
        cJSON_ArrayForEach(b, arr) {
            out->total++;
            if (brk_is_open(b))
                out->open++;
            const cJSON *v = cJSON_GetObjectItem(b, "breaks");
            if (cJSON_IsNumber(v))
                out->breaks += v->valuedouble;
        }
    } else if (cJSON_IsNumber(brk)) {
        out->breaks = brk->valuedouble;
    }
    cJSON_Delete(root);
    return seen;
}

void cli_brk_show(const char *ns, const char *json)
{
    cli_brk_t b;
    if (!cli_brk_read(json, &b))
        return;

    char line[96];
    snprintf(line, sizeof(line), "熔断 未闭合 %d/%d，累计跳闸 %.0f", b.open, b.total,
             b.breaks);
    if (g_cli_print_mode)
        cli_outf("%s breakers open=%d/%d breaks=%.0f\n", ns, b.open, b.total, b.breaks);
    else {
        char hdr[48];
        snprintf(hdr, sizeof(hdr), "%s:熔断", ns);
        cli_print_status(hdr, line);
    }
}

/* doctor 段数据源：单行 JSON（容器内未必有 python/jq）。llm 不可达时仍
 * 输出 online=false，供 doctor 判「服务未起」而非「无熔断」。 */
int cli_cmd_breakers(int argc, char *argv[])
{
    int json = g_cli_json_mode;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
            continue;
        }
        cli_outf("airy_cli: breakers 不认识参数 %s\n", argv[i]);
        return 2;
    }

    char *raw = NULL;
    int up = daemon_rpc_call(cli_ns_sock("llm"), "health_check", NULL, &raw, 6000) == 0 &&
             raw;
    cli_brk_t b;
    int seen = up ? cli_brk_read(raw, &b) : 0;
    AIRY_FREE(raw);

    if (json) {
        cli_outf("{\"schema\":1,\"online\":%s,\"total\":%d,\"open\":%d,"
                 "\"breaks\":%.0f}\n",
                 up ? "true" : "false", seen ? b.total : 0, seen ? b.open : 0,
                 seen ? b.breaks : 0.0);
    } else if (!up) {
        cli_print_status("breakers", "llm 不可达，熔断读数未知");
    } else if (!seen) {
        cli_print_status("breakers", "llm 未发布熔断读数");
    } else {
        char line[96];
        snprintf(line, sizeof(line), "未闭合 %d/%d，累计跳闸 %.0f", b.open, b.total,
                 b.breaks);
        cli_print_status("breakers", line);
    }
    return 0;
}
