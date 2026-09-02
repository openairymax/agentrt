// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_orch.c
 * @brief /orch 流程编排命令（M3，0.1.9 §4.2-1）
 *
 * 架构约束（2026-08-25）：CLI 不直连 orchestrator 实现，统一经 gateway
 * 派发 think.orchestrate RPC（think_d 承载七阶段编排：分解→规划→生成→
 * 批判→验证→审计→对齐）。本文件仅做 JSON-RPC 转发与结果渲染。
 */

#include "cli_internal.h"
#include "cli_gw.h"

#include "airy_memory.h"

#include <cjson/cJSON.h>

#include <stdio.h>

int cmd_orch(const char *arg, void *ctx)
{
    (void)ctx;
    if (!arg || !arg[0]) {
        cli_outf("  %s/orch%s 需要任务描述。用法：/orch <task>%s\n",
                 CLR_YELLOW, CLR_RESET, CLR_RESET);
        return 1;
    }

    cli_outf("  %s[编排]%s 启动七阶段管线（分解→规划→生成→批判→验证→审计→对齐）…\n",
             CLR_CYAN, CLR_RESET);

    cJSON *params = cJSON_CreateObject();
    if (!params)
        return 1;
    cJSON_AddStringToObject(params, "input", arg);
    char *ps = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!ps)
        return 1;

    char *result = NULL;
    int rc = cli_gw_call("think.orchestrate", ps, 120000, &result);
    AIRY_FREE(ps);
    if (rc != 0 || !result) {
        cli_outf("  %s[编排]%s gateway 不可达或 RPC 失败（rc=%d）\n", CLR_RED, CLR_RESET, rc);
        AIRY_FREE(result);
        return 1;
    }

    cJSON *root = cJSON_Parse(result);
    AIRY_FREE(result);
    if (!root) {
        cli_outf("  %s[编排]%s 响应解析失败\n", CLR_RED, CLR_RESET);
        return 1;
    }

    cJSON *phases = cJSON_GetObjectItem(root, "phases");
    size_t count = cJSON_GetArraySize(phases);
    for (size_t i = 0; i < count; i++) {
        cJSON *ph = cJSON_GetArrayItem(phases, (int)i);
        const char *phase = cJSON_GetStringValue(cJSON_GetObjectItem(ph, "phase"));
        const char *status = cJSON_GetStringValue(cJSON_GetObjectItem(ph, "status"));
        const char *output = cJSON_GetStringValue(cJSON_GetObjectItem(ph, "output"));
        const char *state = status ? status : "unknown";
        cli_outf("  ── 阶段 %zu/%zu [%s]\n", i + 1, count, state);
        if (phase)
            cli_outf("     %s\n", phase);
        if (output && output[0])
            cli_outf("     %.240s\n", output);
    }

    cJSON_Delete(root);
    return 0;
}
