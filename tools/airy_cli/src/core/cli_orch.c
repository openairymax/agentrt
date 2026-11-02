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
#include "cli_gw_json.h"

#include "airy_memory.h"

#include <stdio.h>

/* 续期轮询（T-19）：服务端七阶段各自限时、单步超时即收尾应答，总时长可
 * 达预算量子的有限倍数。客户端按「量子 + 续期轮询」等待而非定长斩首：
 * 7 量子 = 七阶段硬上界；量子取 cli_think_timeout_ms（与 think_d 同链）。 */
#define CLI_ORCH_WAIT_RENEWALS 6

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

    cJSON *root = NULL;
    int rc = cli_gw_json_q("think.orchestrate", params, (int)cli_think_timeout_ms(),
                           CLI_ORCH_WAIT_RENEWALS, &root);
    if (rc != 0) {
        /* C-5 收敛：经 cli_err_desc 统一渲染，契约外码值不进用户面 */
        cli_outf("  %s[编排]%s 编排请求失败：%s\n", CLR_RED, CLR_RESET, cli_err_desc(rc));
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
