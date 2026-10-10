// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_gccp.c
 * @brief airy_cli GCCP intent-confirmation interaction sub-module.
 *
 * CLI 是引擎壳（0.1.9 方案 §2.3，M5-4 收口）：GCCP 的策略推理——LLM
 * 逐问追问与收敛判定——已收拢到 products/cognition 策略载荷，经 daemon
 * 注入面（are_ops_set_gccp）在 think_d 进程内执行；CLI 进程不再持有
 * 任何策略调用（本地 airy_gccp_step 退役）。本模块降纯为机制面：对
 * 远端问题集（cli_think.c 两段式 P-A）逐问展示并收集答案，序列化为
 * 答案 JSON（OWNER，所有权移交引擎）供第二段重发完成收敛。
 *
 * 交互纪律：用户跳过某问（空行）即视为意愿不足，直接收敛不纠缠；
 * 中断指令（quit/exit 等）放弃意图确认，任务按默认约束继续。
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

#ifdef AIRY_HAS_CJSON

/**
  * @brief Ask the user the probe questions and collect answers (returns
  *        answer JSON, OWNER; freed by the engine)
  *
  * Question IDs are the answer JSON keys, matching the probe fields in
  * gccp.h one-to-one.
  */
char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data)
{
    (void)user_data;
    if (!probe || probe->question_count == 0)
        return NULL;

    /* One-shot server mode (-p): no interactive confirmation; return empty
     * answers so the engine proceeds with its defaults (non-blocking).
     * 注意：先序列化再释放对象，避免 cJSON 对象泄漏。 */
    if (g_cli_print_mode) {
        cJSON *empty = cJSON_CreateObject();
        if (!empty)
            return NULL;
        char *empty_json = cJSON_PrintUnformatted(empty);
        cJSON_Delete(empty);
        return empty_json;
    }

    /* GCCP 意图确认是对用户需求的结构化约束验证（目标/起点/瓶颈/受众），
     * 属 t1-p 验证者（PROF）职责，而非 t1-f 仲裁者的对话生成链。 */
    cli_render_role_line(CLI_ROLE_DUAL_THINK, CLI_ACTOR_DUAL_PROF_THINK, "意图确认",
                         "我将逐问确认意图（Enter 跳过当前问题）：");
    /* The planning spinner may be animating; pause it so the questions
     * render on clean lines, then resume after the answers. */
    cli_spinner_pause();
    cJSON *answers = cJSON_CreateObject();
    if (!answers) {
        cli_spinner_resume();
        return NULL;
    }

    for (size_t i = 0; i < probe->question_count; i++) {
        const airy_gccp_question_t *q = &probe->questions[i];
        cli_outf("  %sQ%zu%s [%s]%s %s\n", cli_c(CLR_CYAN), i + 1, cli_c(CLR_RESET), q->id,
                 q->required ? "（建议回答）" : "", q->question);
        if (q->hint[0])
            cli_outf("      %s提示：%s%s\n", cli_c(CLR_GREEN), q->hint, cli_c(CLR_RESET));
        cli_outf("  %s>%s ", cli_c(CLR_GREEN), cli_c(CLR_RESET));
        fflush(stdout);

        char line[1024];
        size_t line_len = 0;
        int rl = cli_tui_readline(cli_tui_get_default(), line, sizeof(line), &line_len);
        if (rl == 0)
            break;
        if (line_len == 0)
            break; /* 用户跳过：收敛，不再追问（不纠缠） */
        /* 中断指令：放弃意图确认（视为意愿不足），任务按默认约束
         * 继续；避免把 quit/stop 等当答案写入 Q 字段。 */
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 ||
            strcmp(line, "abort") == 0 || strcmp(line, "stop") == 0 ||
            strcmp(line, "cancel") == 0 || strcmp(line, "打断") == 0 ||
            strcmp(line, "停止") == 0 || strcmp(line, "取消") == 0) {
            cJSON_Delete(answers);
            cli_spinner_resume();
            cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_THINK, "意图确认",
                                 "已放弃意图确认，任务按默认约束继续执行。");
            return NULL;
        }
        cJSON_AddStringToObject(answers, q->id, line);
    }

    char *json = cJSON_PrintUnformatted(answers);
    cJSON_Delete(answers);
    cli_spinner_resume();
    /* 阶段 4：GCCP 意图确认 → 决策链事件（preflight，cognition 角色）。
     * 仅记录结构化信号（问题数），用户回答原文不进事件流（隐私 + JSON 转义安全）。 */
    cli_hall_emit("preflight", AIRY_HALL_CAT_CHAIN,
                  "{\"event\":\"gccp_confirm\",\"question_count\":%zu}",
                  probe->question_count);
    return json;
}

#else /* !AIRY_HAS_CJSON */
char *cli_gccp_interact(const airy_gccp_probe_t *probe, void *user_data)
{
    (void)user_data;
    if (!probe || probe->question_count == 0)
        return NULL;

    /* One-shot server mode (-p): non-blocking, empty answers (defaults). */
    if (g_cli_print_mode) {
        char *json = (char *)AIRY_MALLOC(3);
        if (!json)
            return NULL;
        json[0] = '{';
        json[1] = '}';
        json[2] = '\0';
        return json;
    }

    cli_spinner_pause();
    size_t cap = 512;
    for (size_t i = 0; i < probe->question_count; i++)
        cap += strlen(probe->questions[i].id) + 1024;
    char *json = (char *)AIRY_MALLOC(cap);
    if (!json) {
        cli_spinner_resume();
        return NULL;
    }
    char *p = json;
    int n = snprintf(p, cap, "{");
    p += n;
    for (size_t i = 0; i < probe->question_count; i++) {
        const airy_gccp_question_t *q = &probe->questions[i];
        cli_outf("  Q%zu [%s]%s %s\n", i + 1, q->id, q->required ? "（建议回答）" : "", q->question);
        if (q->hint[0])
            cli_outf("      提示：%s\n", q->hint);
        cli_outf("  > ");
        fflush(stdout);
        char line[1024];
        size_t line_len = 0;
        int rl = cli_tui_readline(cli_tui_get_default(), line, sizeof(line), &line_len);
        if (rl == 0)
            break;
        /* 中断指令：放弃意图确认（同 cJSON 分支，避免 quit/stop 当答案） */
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 ||
            strcmp(line, "abort") == 0 || strcmp(line, "stop") == 0 ||
            strcmp(line, "cancel") == 0 || strcmp(line, "打断") == 0 ||
            strcmp(line, "停止") == 0 || strcmp(line, "取消") == 0) {
            AIRY_FREE(json);
            cli_spinner_resume();
            cli_render_role_line(CLI_ROLE_TRACE, CLI_ACTOR_DUAL_THINK, "意图确认",
                                 "已放弃意图确认，任务按默认约束继续执行。");
            return NULL;
        }
        if (i > 0)
            *p++ = ',';
        n = snprintf(p, cap - (size_t)(p - json), "\"%s\":\"%s\"", q->id, line);
        p += n;
    }
    snprintf(p, cap - (size_t)(p - json), "}");
    cli_spinner_resume();
    /* 阶段 4：GCCP 意图确认 → 决策链事件（同 cJSON 分支，仅记录结构化信号） */
    cli_hall_emit("preflight", AIRY_HALL_CAT_CHAIN,
                  "{\"event\":\"gccp_confirm\",\"question_count\":%zu}",
                  probe->question_count);
    return json;
}

#endif /* AIRY_HAS_CJSON */
