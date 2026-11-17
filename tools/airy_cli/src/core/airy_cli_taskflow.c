// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_taskflow.c
 * @brief 任务回合统一入口（C-15）。
 *
 * 任务回合不再由 CLI 本地裁决链路（本地分类 → think_d 计划 → sched_d DAG），
 * 而是与 TUI/SDK 一样提交 gateway `task.submit`：模式裁决、GCCP 门禁、计划
 * 宣告与执行全部在 agent_d 内完成，CLI 只负责参数装配与结果渲染。
 *
 * -p 单次模式不会自动确认目标：不发送 gccp_confirmed / gccp_answers 时，
 * 服务端对 task 模式返回 needs_intent，本次回合零工具调用。返回 1 表示
 * 本轮未执行（与「已执行但失败」区分，-p 的进程退出码由此决定）。
 */

#include "cli_internal.h"
#include "cli_gw.h"
#include "cli_gw_json.h"
#include "airy_cli_exec.h"

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

#include <stdio.h>
#include <string.h>

/* 一次 task.submit 覆盖规划 + 执行；执行轮次由引擎预算约束，传输层给足
 * 余量，避免长任务被 CLI 侧先掐断。 */
#define CLI_TASK_SUBMIT_TIMEOUT_MS 600000

#ifdef AIRY_HAS_CJSON
/* 只读字符串字段（缺失/类型不符统一返回空串，渲染侧不必判 NULL）。 */
static const char *task_str(const cJSON *obj, const char *key)
{
    const cJSON *v = cJSON_GetObjectItem(obj, key);
    return (cJSON_IsString(v) && v->valuestring) ? v->valuestring : "";
}

/* needs_intent 渲染：模式裁决原因 + 缺失槽位（服务端唯一权威来源）。 */
static void task_print_intent(const cJSON *res)
{
    const char *msg = task_str(res, "message");
    const char *mode = task_str(res, "mode");
    const char *reason = task_str(res, "mode_reason");
    char line[512];

    cli_print_line("GCCP", msg[0] ? msg
                                  : "任务目标尚不完整，需补齐 goal/context/constraints/plan。");
    snprintf(line, sizeof(line), "模式：%s%s%s%s", mode[0] ? mode : "task",
             reason[0] ? "（" : "", reason[0] ? reason : "", reason[0] ? "）" : "");
    cli_print_line("GCCP", line);

    const cJSON *missing = cJSON_GetObjectItem(res, "gccp_missing");
    if (cJSON_IsArray(missing)) {
        size_t off = 0;
        line[0] = '\0';
        for (int i = 0; i < cJSON_GetArraySize(missing); i++) {
            const cJSON *m = cJSON_GetArrayItem(missing, i);
            if (!cJSON_IsString(m) || !m->valuestring)
                continue;
            int n = snprintf(line + off, sizeof(line) - off, "%s%s",
                             off ? ", " : "缺少：", m->valuestring);
            if (n < 0)
                break;
            off += (size_t)n;
            if (off >= sizeof(line))
                break;
        }
        if (off)
            cli_print_line("GCCP", line);
    }
}
#endif /* AIRY_HAS_CJSON */

int cli_run_task_pipeline(cli_runtime_ctx_t *rt, const char *input, uint64_t turn_start)
{
#ifdef AIRY_HAS_CJSON
    (void)rt;

    cJSON *params = cJSON_CreateObject();
    cJSON *res = NULL;
    char line[256];

    if (!params) {
        cli_print_line("Super Agent", "任务提交失败：内存不足");
        return 1;
    }
    cJSON_AddStringToObject(params, "goal", input ? input : "");
    /* "auto" 交给服务端裁决；客户端不得自行决定 chat/task。 */
    cJSON_AddStringToObject(params, "mode", "auto");
    cJSON_AddStringToObject(params, "client", "cli");
    /* 不发送 gccp_confirmed / gccp_answers：确认只能来自用户交互。 */

    cli_print_status("Sub agent_d Agent", "task.submit（plan-execute）");
    if (cli_gw_json("task.submit", params, CLI_TASK_SUBMIT_TIMEOUT_MS, &res) != 0 || !res) {
        const char *err = cli_gw_err_take();
        snprintf(line, sizeof(line), "任务提交失败：%s",
                 (err && err[0]) ? err : "网关不可达或任务服务未就绪");
        cli_print_line("Super Agent", line);
        cJSON_Delete(res);
        return 1;
    }

    const char *status = task_str(res, "status");
    const char *task_id = task_str(res, "task_id");
    const char *mode = task_str(res, "mode");
    const char *reason = task_str(res, "mode_reason");
    cli_trace("task", "id=%s mode=%s reason=%s status=%s", task_id, mode, reason, status);

    int failed = 0;
    if (strcmp(status, "needs_intent") == 0) {
        task_print_intent(res);
        failed = 1;
    } else {
        snprintf(line, sizeof(line), "task=%s mode=%s%s%s", task_id, mode,
                 reason[0] ? " · " : "", reason[0] ? reason : "");
        cli_print_status("Sub agent_d Agent", line);

        const cJSON *resp = cJSON_GetObjectItem(res, "response");
        if (g_cli_json_mode) {
            char *out = cJSON_PrintUnformatted(res);
            if (out) {
                cli_outf("%s\n", out);
                AIRY_FREE(out);
            }
        } else if (cJSON_IsString(resp) && resp->valuestring && resp->valuestring[0]) {
            cli_print_line("Super Agent", resp->valuestring);
        } else {
            cli_print_line("Super Agent", "任务已完成（无文本输出）。");
        }
    }

    cJSON_Delete(res);
    cli_print_turn(cli_now_ms() - turn_start, NULL);
    return failed;
#else
    (void)rt;
    (void)input;
    (void)turn_start;
    cli_print_line("Super Agent", "任务提交失败：当前构建未启用 cJSON");
    return 1;
#endif
}
