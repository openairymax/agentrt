// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_orch.c
 * @brief /orch 流程编排命令（S-5 编排管线用户入口，2026-08-24）
 *
 * 将 orchestrator（七阶段流程编排器）接入 airy_cli 作为用户入口：
 * 分解→规划→生成→批判→验证→审计→对齐。llm_d 在线时各阶段经
 * daemon_rpc 直连 $AIRY_HOME/run/llm.sock 真实执行；离线时走认知引擎
 * /降级链（与 orchestrator 内部设计一致，不依赖外部状态）。
 */

#include "cli_internal.h"

#include "airy_memory.h"
#include "orchestrator.h"

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

    orch_config_t cfg;
    orch_config_get_defaults(&cfg);
    orchestrator_t *orch = orchestrator_create(&cfg);
    if (!orch) {
        cli_outf("  %s[编排]%s 创建编排器失败\n", CLR_RED, CLR_RESET);
        return 1;
    }

    orch_result_t *results = NULL;
    size_t count = 0;
    int rc = orchestrator_execute(orch, arg, &results, &count);
    if (rc != 0) {
        cli_outf("  %s[编排]%s 管线中止（rc=%d）\n", CLR_RED, CLR_RESET, rc);
    }

    for (size_t i = 0; i < count && results; i++) {
        const char *state = results[i].status == ORCH_TASK_COMPLETED
                                ? "完成"
                                : results[i].status == ORCH_TASK_FAILED
                                      ? "失败"
                                      : results[i].status == ORCH_TASK_TIMEOUT ? "超时"
                                                                               : "未完成";
        cli_outf("  ── 阶段 %zu/%zu [%s]\n", i + 1, count, state);
        if (results[i].output && results[i].output[0])
            cli_outf("     %.240s\n", results[i].output);
        orchestrator_result_free(&results[i]);
    }
    if (results)
        AIRY_FREE(results);

    orchestrator_destroy(orch);
    orchestrator_global_cleanup();
    return 0;
}
