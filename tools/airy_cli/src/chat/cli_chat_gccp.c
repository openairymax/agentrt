// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_gccp.c
 * @brief airy_cli GCCP intent-confirmation interaction sub-module.
 *
 * GCCP 逐问交互（2026-08-15）：不再一次性抛全部问题。每次只展示一个
 * 问题，用户回答后调用 airy_gccp_step() 让 LLM 对已答内容思考——决定是
 * 收敛（done=1）还是根据已答内容生成下一个针对性追问。LLM 不可用时降级
 * 为逐问机械推进（至少不是批量）；用户跳过某问（空行）即视为意愿不足，
 * 直接收敛不纠缠。追问上限 = 问题数 + 4，防 LLM 无限追问。
 *
 * 交互完成后把用户回答序列化为答案 JSON（OWNER，由引擎释放），供
 * cli_think.c 第二段重发完成 GCCP 收敛。
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* LLM 上一步生成的追问（跨循环轮次保留；每次提问前清零，未生成则自然退出） */
static char g_last_step_q[512];
static char g_last_step_hint[256];

#ifdef AIRY_HAS_CJSON

/**
  * @brief Ask the user four questions and collect answers (returns answer JSON, OWNER; freed by the engine)
  *
  * Question IDs (endpoint/start/bottleneck/audience) are the answer JSON keys,
  * matching the Q1-Q4 fields in gccp.h one-to-one.
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
    if (!answers)
        return NULL;

    /* 交互开始清零：防止上次交互（用户 Ctrl-C / 异常中断）残留的
     * 追问污染本次 probe 列表后的 LLM 追问区。 */
    g_last_step_q[0] = '\0';
    g_last_step_hint[0] = '\0';

    /* 原指令从 prefill 的 raw_prompt 取（probe 阶段已保存） */
    const char *raw = (probe->prefill && probe->prefill->raw_prompt) ? probe->prefill->raw_prompt :
                                                                       "（无原始指令）";
    size_t raw_len = strlen(raw);

    /* 追问上限：基础问题数 + 4，防止 LLM 无限追问（用户主导交互的护栏） */
    size_t max_rounds = probe->question_count + 4;

    /* 待问队列：固定问题（probe 顺序）与 LLM 动态追问交错。
     * q8a 修复：此前每轮 round < question_count 时无条件用固定问题，
     * LLM 经 airy_gccp_step 生成的追问要等全部固定问题问完才被消费——
     * 用户回答 Q1 后 LLM 判断"最不确定维度是 X"并生成追问，循环却继续
     * 问固定 Q2，LLM 每轮推理被丢弃。现改为：step 返回追问时下一轮
     * 优先消费（llm_q_pending），固定问题退为追问用尽后的兜底——真正
     * 实现"每个问题用户回答后，模型推理后再据推理结果问下一问"。 */
    int llm_q_pending = 0;
    size_t base_round = 0; /* 固定问题区游标（独立于总轮次） */
    for (size_t round = 0; round < max_rounds; round++) {
        /* 本轮问题：LLM 追问 > 固定问题 > 上轮遗留追问 */
        airy_gccp_question_t local_q;
        __builtin_memset(&local_q, 0, sizeof(local_q));
        const airy_gccp_question_t *q = NULL;
        if (llm_q_pending) {
            /* 优先消费 LLM 上一步生成的针对性追问 */
            snprintf(local_q.id, sizeof(local_q.id), "followup%zu", round);
            AIRY_STRNCPY_TERM(local_q.question, g_last_step_q, sizeof(local_q.question));
            AIRY_STRNCPY_TERM(local_q.hint, g_last_step_hint, sizeof(local_q.hint));
            local_q.required = 0;
            q = &local_q;
            llm_q_pending = 0;
        } else if (base_round < probe->question_count) {
            q = &probe->questions[base_round++];
        } else if (g_last_step_q[0]) {
            /* 固定问题问完：消费 LLM 遗留追问 */
            snprintf(local_q.id, sizeof(local_q.id), "followup%zu", round);
            AIRY_STRNCPY_TERM(local_q.question, g_last_step_q, sizeof(local_q.question));
            AIRY_STRNCPY_TERM(local_q.hint, g_last_step_hint, sizeof(local_q.hint));
            local_q.required = 0;
            q = &local_q;
        } else {
            break; /* 没有可用追问：结束 */
        }

        cli_outf("  %sQ%zu%s [%s]%s %s\n", cli_c(CLR_CYAN), round + 1, cli_c(CLR_RESET), q->id,
                 q->required ? "（建议回答）" : "", q->question);
        if (q->hint[0])
            cli_outf("      %s提示：%s%s\n", cli_c(CLR_GREEN), q->hint, cli_c(CLR_RESET));
        if (!cli_tui_active(cli_tui_get_default())) {
            /* Line-oriented mode: print an inline prompt. In full-screen TUI
             * mode the bottom input line is the prompt itself. */
            cli_outf("  %s>%s ", cli_c(CLR_GREEN), cli_c(CLR_RESET));
            fflush(stdout);
        }

        char line[1024];
        size_t line_len = 0;
        int rl = cli_tui_readline(cli_tui_get_default(), line, sizeof(line), &line_len);
        if (rl == 0)
            break;
        if (rl != 1)
            continue; /* F8 切换请求：重试当前问题（切换在主循环处理） */
        int answered = (line_len > 0);
        if (answered) {
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
        } else {
            break; /* 用户跳过：收敛，不再追问（不纠缠） */
        }

        /* 让 LLM 对已答内容思考，决定收敛或生成下一个追问。无 LLM 时
         * airy_gccp_step 降级为"继续下一题"，交互仍逐问推进。 */
        g_last_step_q[0] = '\0';
        g_last_step_hint[0] = '\0';
        char *answers_json = cJSON_PrintUnformatted(answers);
        if (!answers_json)
            break;
        airy_gccp_step_t step;
        /* 2.1.1.2 修复：GCCP 逐问确认走 t1-p（PROF）模型槽 */
        const char *t1p_model = cli_chat_t1p_cached();
        /* 用户回答后 LLM 思考期间必须有可见反馈（此前 spinner 暂停且无
         * 其他指示，长推理时屏幕静止数秒，用户误以为卡死） */
        cli_outf("  %s◆%s %s意图收敛思考中…%s\n", cli_c(CLR_CYAN), cli_c(CLR_RESET),
                 cli_c(CLR_DIM), cli_c(CLR_RESET));
        fflush(stdout);
        airy_err_t serr = airy_gccp_step(g_chat_adapter, NULL,
                                         (t1p_model && t1p_model[0]) ? t1p_model : NULL, raw,
                                         raw_len, answers_json, 1, q, &step);
        cJSON_free(answers_json);
        if (serr != AIRY_SUCCESS)
            break;
        /* 展示 LLM 对上一答的思考（渐进披露：折叠为前 4 行，避免多轮
         * 推理刷屏；完整文本保留在日志）。 */
        if (step.reasoning[0])
            cli_render_collapsed(step.reasoning, 4, 4, 1);
        if (step.done)
            break; /* 已收敛 */
        if (step.question[0]) {
            AIRY_STRNCPY_TERM(g_last_step_q, step.question, sizeof(g_last_step_q));
            AIRY_STRNCPY_TERM(g_last_step_hint, step.hint, sizeof(g_last_step_hint));
            llm_q_pending = 1; /* 下一轮优先消费 LLM 针对性追问 */
        }
    }

    char *json = cJSON_PrintUnformatted(answers);
    cJSON_Delete(answers);
    cli_spinner_resume();
    /* 阶段 4：GCCP 意图确认 → 决策链事件（preflight，cognition 角色）。
     * 仅记录结构化信号（问题数），用户回答原文不进事件流（隐私 + JSON 转义安全）。 */
    if (g_cli_hall_store) {
        char ev[256];
        snprintf(ev, sizeof(ev), "{\"event\":\"gccp_confirm\",\"question_count\":%zu}",
                 probe->question_count);
        airy_hall_store_write(g_cli_hall_store, "default", "preflight", NULL,
                              AIRY_HALL_CAT_CHAIN, "cognition", ev, NULL, 0);
    }
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
    if (!json)
        return NULL;
    char *p = json;
    int n = snprintf(p, cap, "{");
    p += n;
    for (size_t i = 0; i < probe->question_count; i++) {
        const airy_gccp_question_t *q = &probe->questions[i];
        cli_outf("  Q%zu [%s]%s %s\n", i + 1, q->id, q->required ? "（建议回答）" : "", q->question);
        if (q->hint[0])
            cli_outf("      提示：%s\n", q->hint);
        if (!cli_tui_active(cli_tui_get_default()))
            cli_outf("  > ");
        fflush(stdout);
        char line[1024];
        size_t line_len = 0;
        int rl = cli_tui_readline(cli_tui_get_default(), line, sizeof(line), &line_len);
        if (rl == 0)
            break;
        if (rl != 1)
            continue; /* F8 切换请求：重试当前问题（切换在主循环处理） */
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
    if (g_cli_hall_store) {
        char ev[256];
        snprintf(ev, sizeof(ev), "{\"event\":\"gccp_confirm\",\"question_count\":%zu}",
                 probe->question_count);
        airy_hall_store_write(g_cli_hall_store, "default", "preflight", NULL,
                              AIRY_HALL_CAT_CHAIN, "cognition", ev, NULL, 0);
    }
    return json;
}

#endif /* AIRY_HAS_CJSON */
