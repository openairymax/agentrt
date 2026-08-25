// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat.c
 * @brief airy_cli chat domain: GCCP callback, session history, intent split and replies.
 *
 * Handles all normal user chat: GCCP four-question interaction callback
 * (two conditional-compilation variants), FIFO chat history buffer,
 * heuristic + LLM task/chat intent classification, and direct replies
 * as the super agent (single t1-f B model generation, decision 2026-08-09).
 */

#include "cli_internal.h"

#include "memory.h" /* 对话记忆引擎 API（2.2.4 对话路径读写） */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#endif

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* 2.1.1.5/2.1.1.6：本轮对话真实 token/费用统计与思考链保留。
 *
 * llm_d 在响应的 usage/top-level 回填 total_tokens 与 cost_usd（含思考
 * token，DeepSeek/OpenAI 的 completion_tokens 已包含 reasoning_tokens），
 * 此处按轮累加；回合结束由 main.c 经 cli_chat_usage_get 读取展示，并在
 * 下一轮开始前清零。reasoning_content 全量累积后写日志（折叠展示在
 * 对话内，完整文本保留在日志，思考 token 不丢失）。 */
static uint64_t g_chat_tokens_total = 0;
static double g_chat_cost_total = 0.0;
static char *g_chat_reasoning_acc = NULL;

static void cli_chat_usage_add(const llm_response_t *resp)
{
    if (!resp)
        return;
    g_chat_tokens_total += resp->total_tokens;
    g_chat_cost_total += resp->cost_usd;
}

static void cli_chat_reasoning_add(const char *reasoning)
{
    if (!reasoning || !reasoning[0])
        return;
    size_t old = g_chat_reasoning_acc ? strlen(g_chat_reasoning_acc) : 0;
    size_t add = strlen(reasoning);
    char *np = (char *)AIRY_REALLOC(g_chat_reasoning_acc, old + add + 2);
    if (!np)
        return;
    g_chat_reasoning_acc = np;
    if (old > 0)
        g_chat_reasoning_acc[old++] = '\n';
    __builtin_memcpy(g_chat_reasoning_acc + old, reasoning, add);
    g_chat_reasoning_acc[old + add] = '\0';
}

/* 供 main.c 在回合结束时读取本轮消耗统计（随后由 cli_chat_reply 下一轮
 * 开始时清零）。 */
void cli_chat_usage_get(uint64_t *tokens, double *cost)
{
    if (tokens)
        *tokens = g_chat_tokens_total;
    if (cost)
        *cost = g_chat_cost_total;
}

/* 1.7 真实消耗会话差值：llm_d cost_tracker 是所有 LLM 请求（chat + task
 * 双思考路径）的持久化真相源。会话首读记录起点快照，此后每次返回与起点
 * 的差值 = 本会话真实消耗（含思考 token，completion_tokens 已含 reasoning）。
 * llm_d 不可用时返回 0（调用方回退 chat 累计 g_chat_tokens_total）。 */
static uint64_t g_llm_base_prompt = 0;
static uint64_t g_llm_base_completion = 0;
static double g_llm_base_cost = 0.0;
static int g_llm_base_set = 0;

static int cli_llm_d_usage_snapshot(uint64_t *out_prompt, uint64_t *out_completion,
                                    double *out_cost)
{
    const char *sock = airy_runtime_dir_socket("llm.sock");
    if (!sock || !sock[0])
        return -1;

    char *result = NULL;
    int rc = daemon_rpc_call(sock, "get_stats", "{}", &result, 6000);
    if (rc != 0 || !result)
        return -1;

    uint64_t prompt = 0, comp = 0;
    double cost = 0.0;

#ifdef AIRY_HAS_CJSON
    cJSON *root = cJSON_Parse(result);
    AIRY_FREE(result);
    if (!root)
        return -1;
    cJSON *costj = cJSON_GetObjectItemCaseSensitive(root, "cost");
    cJSON *arr = costj ? cJSON_GetObjectItemCaseSensitive(costj, "models") : NULL;
    if (cJSON_IsArray(arr)) {
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, arr) {
            cJSON *pt = cJSON_GetObjectItemCaseSensitive(item, "prompt_tokens");
            cJSON *ct = cJSON_GetObjectItemCaseSensitive(item, "completion_tokens");
            cJSON *cu = cJSON_GetObjectItemCaseSensitive(item, "cost_usd");
            if (cJSON_IsNumber(pt))
                prompt += (uint64_t)pt->valuedouble;
            if (cJSON_IsNumber(ct))
                comp += (uint64_t)ct->valuedouble;
            if (cJSON_IsNumber(cu))
                cost += cu->valuedouble;
        }
    }
    cJSON_Delete(root);
#else
    AIRY_FREE(result);
    return -1;
#endif

    if (out_prompt)
        *out_prompt = prompt;
    if (out_completion)
        *out_completion = comp;
    if (out_cost)
        *out_cost = cost;
    return 0;
}

/* 1.7：全链路真实消耗（会话差值）；llm_d 离线回退 chat 累计。 */
void cli_chat_usage_get_session(uint64_t *tokens, double *cost)
{
    uint64_t prompt = 0, comp = 0;
    double c = 0.0;
    if (cli_llm_d_usage_snapshot(&prompt, &comp, &c) != 0) {
        cli_chat_usage_get(tokens, cost);
        return;
    }

    if (!g_llm_base_set) {
        g_llm_base_prompt = prompt;
        g_llm_base_completion = comp;
        g_llm_base_cost = c;
        g_llm_base_set = 1;
        if (tokens)
            *tokens = 0;
        if (cost)
            *cost = 0.0;
        return;
    }

    if (tokens)
        *tokens = (prompt - g_llm_base_prompt) + (comp - g_llm_base_completion);
    if (cost)
        *cost = c - g_llm_base_cost;
}

/* 2.1.1.2：t1-p（PROF）模型缓存前向声明——GCCP 逐问确认（cli_gccp_interact
 * 在文件前部）使用该模型槽。实现见文件后部（与 cli_chat_t1f_cached 同构）。 */
static const char *cli_chat_t1p_cached(void);

#ifdef AIRY_HAS_CJSON

/* 对话记忆引擎（2.2.4）：main.c 注入，见 cli_internal.h */
airy_memory_engine_t *g_cli_memory_engine = NULL;

/* 2.2.2.1：无信息量寒暄启发式——此类对话不应进记忆，避免垃圾累积
 * 污染语义检索（匹配时返回 1）。仅作写入门槛，不做强制语义判断。 */
static int cli_chat_is_greeting(const char *s)
{
    if (!s)
        return 0;
    size_t n = strlen(s);
    if (n > 12)
        return 0;
    static const char *const greets[] = {
        "你好", "您好", "哈喽", "嗨", "hello", "hi", "hey",
        "谢谢", "感谢", "多谢", "thanks", "thank you", "thx",
        "再见", "拜拜", "bye", "goodbye", "ok", "好的", "嗯", "收到",
    };
    for (size_t i = 0; i < sizeof(greets) / sizeof(greets[0]); i++) {
        if (strcmp(s, greets[i]) == 0 || strncmp(s, greets[i], strlen(greets[i])) == 0)
            return 1;
    }
    return 0;
}

/* 检索相关历史记忆，拼成 system 追加段（最多 3 条、每条截断 200 字符） */
static void cli_chat_mem_inject_system(const char *input, char *out_buf, size_t out_size)
{
    out_buf[0] = '\0';
    if (!g_cli_memory_engine || !input || !input[0] || out_size < 16)
        return;

    airy_memory_query_t q;
    __builtin_memset(&q, 0, sizeof(q));
    q.memory_query_text = (char *)input;
    q.memory_query_text_len = strlen(input);
    q.memory_query_limit = 3;
    q.memory_query_include_raw = 1;

    airy_memory_result_ext_t *res = NULL;
    if (airy_memory_query(g_cli_memory_engine, &q, &res) != AIRY_EOK || !res)
        return;

    size_t off = 0;
    off += snprintf(out_buf + off, out_size - off, "\n\n[相关历史记忆]");
    /* 2.2.2.1：防自我回灌——跳过与当前输入相同/几乎相同的记忆（CLI 把
     * 整轮写为 "用户: <input>\nAgentRT: <reply>"，查询常命中上一条自身）。 */
    size_t input_prefix_len = strlen(input) + 7; /* "用户: " + input */
    for (size_t i = 0; i < res->memory_result_count; i++) {
        airy_memory_result_item_t *it =
            (res->memory_result_items && i < res->memory_result_count)
                ? res->memory_result_items[i]
                : NULL;
        if (!it || !it->memory_result_item_record)
            continue;
        airy_memory_record_t *r = it->memory_result_item_record;
        if (!r->memory_record_data || r->memory_record_data_len == 0)
            continue;
        const char *data = (const char *)r->memory_record_data;
        size_t dlen = r->memory_record_data_len;
        if (dlen >= input_prefix_len &&
            strncmp(data, "用户: ", 5) == 0 &&
            strncmp(data + 5, input, strlen(input)) == 0)
            continue;
        size_t n = dlen < 200 ? dlen : 200;
        off += snprintf(out_buf + off, out_size - off, "\n- %.*s", (int)n, data);
        if (off >= out_size - 1)
            break;
    }
    airy_memory_result_free(res);
}

/* 一轮对话完成后写入记忆：用户输入 + 回复（截断防噪声，只记事实）。
 * 2.1.1.6 修订：携带思考链（reasoning）——记忆检索按 content 匹配，
 * 拼接进记录后思考 token 可被下轮/下次会话召回（与 TUI 记忆的
 * reasoning 语义对齐），不再"只存档不可用"。 */
static void cli_chat_mem_record(const char *input, const char *reply, const char *reasoning)
{
    if (!g_cli_memory_engine || !input || !input[0] || !reply || !reply[0])
        return;

    /* 2.2.2.1：寒暄/无信息量回复不写记忆（避免垃圾条目累积抬高检索噪声） */
    if (strlen(reply) < 8 || cli_chat_is_greeting(input))
        return;

    char content[1800];
    int n = snprintf(content, sizeof(content), "用户: %s\nAgentRT: %s", input, reply);
    if (n <= 0)
        return;
    if (reasoning && reasoning[0]) {
        int rn = snprintf(content + n, (size_t)(sizeof(content) - n), "\n[reasoning] %s", reasoning);
        if (rn > 0)
            n += (rn < (int)(sizeof(content) - n - 1)) ? rn : (int)(sizeof(content) - n - 1);
    }
    if (n > 1600)
        n = 1600;

    airy_memory_record_t rec;
    __builtin_memset(&rec, 0, sizeof(rec));
    rec.memory_record_type = AIRY_MEMTYPE_TEXT;
    rec.memory_record_data = content;
    rec.memory_record_data_len = (size_t)n;
    rec.memory_record_importance = 0.6f;

    char *rid = NULL;
    (void)airy_memory_write(g_cli_memory_engine, &rec, &rid);
    AIRY_FREE(rid);
}

/* LLM 上一步生成的追问（跨循环轮次保留；每次提问前清零，未生成则自然退出） */
static char g_last_step_q[512];
static char g_last_step_hint[256];

/**
  * @brief Ask the user four questions and collect answers (returns answer JSON, OWNER; freed by the engine)
  *
  * Question IDs (endpoint/start/bottleneck/audience) are the answer JSON keys,
  * matching the Q1-Q4 fields in gccp.h one-to-one.
  *
  * 逐问交互（2026-08-15）：不再一次性抛全部问题。每次只展示一个问题，
  * 用户回答后调用 airy_gccp_step() 让 LLM 对已答内容思考——决定是收敛
  * （done=1）还是根据已答内容生成下一个针对性追问。LLM 不可用时降级为
  * 逐问机械推进（至少不是批量）；用户跳过某问（空行）即视为意愿不足，
  * 直接收敛不纠缠。追问上限 = 问题数 + 4，防 LLM 无限追问。
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

    /* 待问队列：先按 probe 顺序走，之后由 LLM 动态生成追问。
     * 每轮索引 round 指向 probe->questions[round]（越界即表示进入 LLM 追问区）。 */
    for (size_t round = 0; round < max_rounds; round++) {
        /* 本轮问题：probe 原始问题 or LLM 上一步生成的追问 */
        airy_gccp_question_t local_q;
        __builtin_memset(&local_q, 0, sizeof(local_q));
        const airy_gccp_question_t *q = NULL;
        if (round < probe->question_count) {
            q = &probe->questions[round];
        } else {
            /* 进入 LLM 追问区：使用上一轮 step 生成的问题（question 非空才有意义） */
            if (!g_last_step_q[0])
                break; /* 没有可用追问：结束 */
            snprintf(local_q.id, sizeof(local_q.id), "followup%zu", round);
            AIRY_STRNCPY_TERM(local_q.question, g_last_step_q, sizeof(local_q.question));
            AIRY_STRNCPY_TERM(local_q.hint, g_last_step_hint, sizeof(local_q.hint));
            local_q.required = 0;
            q = &local_q;
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
         * 任何提示，模型响应慢时用户感知为"卡住"）——打印静态提示行，
         * 不碰 spinner 状态机（planning spinner 由 pause/resume 管理）。 */
        cli_outf("  %s◆%s %s意图收敛思考中…%s\n", cli_c(CLR_CYAN), cli_c(CLR_RESET),
                 cli_c(CLR_DIM), cli_c(CLR_RESET));
        fflush(stdout);
        airy_err_t serr = airy_gccp_step(g_chat_adapter, NULL,
                                         (t1p_model && t1p_model[0]) ? t1p_model : NULL, raw,
                                         raw_len, answers_json, 1, NULL, &step);
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

/* 聊天场景的错误描述：llm_d 是聊天回复的唯一 RPC 目标，NOT_FOUND 即
 * llm.sock 不存在（llm_d 未启动），与通用"目标不存在"相比给出可执行
 * 提示；其余错误沿用通用映射（cli_err_desc）。 */
static const char *cli_chat_err_desc(int err)
{
    if (err == AIRY_ERR_NOT_FOUND)
        return "LLM 服务未运行，请先启动 airymaxrt（llm_d 守护进程）";
    return cli_err_desc(err);
}

char *g_history_roles[CLI_HISTORY_MAX_MSGS];
char *g_history_contents[CLI_HISTORY_MAX_MSGS];
/* 2.1.1.6：历史携带每轮思考链——多轮对话上下文中前一轮的 reasoning 原样
 * 保留并随 assistant 消息回传（DeepSeek 续轮规范要求，缺省会语义断裂）。 */
char *g_history_reasonings[CLI_HISTORY_MAX_MSGS];
size_t g_history_count = 0;

static size_t cli_history_capacity(void)
{
    const char *env = getenv("AIRY_CHAT_HISTORY_ROUNDS");
    if (env && env[0] != '\0') {
        long rounds = strtol(env, NULL, 10);
        if (rounds >= 1 && rounds <= 30)
            return (size_t)rounds * 2;
    }
    return 30;
}

static void cli_history_add(const char *role, const char *content, const char *reasoning)
{
    if (!role || !content)
        return;
    size_t cap = cli_history_capacity();
    if (g_history_count >= cap) {

        AIRY_FREE(g_history_roles[0]);
        AIRY_FREE(g_history_contents[0]);
        AIRY_FREE(g_history_reasonings[0]);
        AIRY_FREE(g_history_roles[1]);
        AIRY_FREE(g_history_contents[1]);
        AIRY_FREE(g_history_reasonings[1]);
        AIRY_MEMMOVE(&g_history_roles[0], &g_history_roles[2],
                     (g_history_count - 2) * sizeof(char *));
        AIRY_MEMMOVE(&g_history_contents[0], &g_history_contents[2],
                     (g_history_count - 2) * sizeof(char *));
        AIRY_MEMMOVE(&g_history_reasonings[0], &g_history_reasonings[2],
                     (g_history_count - 2) * sizeof(char *));
        g_history_count -= 2;
    }
    g_history_roles[g_history_count] = AIRY_STRDUP(role);
    g_history_contents[g_history_count] = AIRY_STRDUP(content);
    g_history_reasonings[g_history_count] =
        (reasoning && reasoning[0]) ? AIRY_STRDUP(reasoning) : NULL;
    g_history_count++;
}

void cli_history_clear(void)
{
    for (size_t i = 0; i < g_history_count; i++) {
        AIRY_FREE(g_history_roles[i]);
        AIRY_FREE(g_history_contents[i]);
        AIRY_FREE(g_history_reasonings[i]);
    }
    g_history_count = 0;
}

/* 2.1.1.6：思考链全量落盘——交互模式 cli_trace 是 no-op（仅 -p 模式
 * 写 stderr），思考链此前只在内存折叠展示后即释放。这里独立追加写入
 * $AIRY_HOME/logs/airy_reasoning.log（所有模式生效），思考 token 不丢失。
 * 每轮带时间戳与角色前缀，便于按会话回溯。 */
static void cli_chat_reasoning_persist(const char *text)
{
    if (!text || !text[0])
        return;
    const char *logdir = airy_log_dir();
    if (!logdir || airy_mkdir_p(logdir) != 0)
        return;
    char logpath[512];
    int plen = snprintf(logpath, sizeof(logpath), "%s/airy_reasoning.log", logdir);
    if (plen < 0 || plen >= (int)sizeof(logpath))
        return;
    FILE *lf = fopen(logpath, "a");
    if (!lf)
        return;
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char ts[40];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(lf, "\n[%s] [assistant reasoning] %s\n", ts, text);
    fclose(lf);
}

#define CLI_SYSTEM_PROMPT                                                       \
    "你是 AgentRT，一个智能体操作系统与超级智能体助手。请用中文简洁、友好地"    \
    "回答用户的问题；需要执行具体工程任务时，请引导用户描述为任务指令。\n"      \
    "你具备本地文件工具：fs_read（读文件）、fs_write（写/覆盖文件）、fs_list"  \
    "（列目录）、fs_glob（通配符找文件）、fs_grep（正则搜文件内容）、fs_edit"  \
    "（精确字符串替换编辑）。用户要求查看、修改本地文件时，先 fs_read/fs_list" \
    " 确认现状再操作；fs_edit 需精确匹配原文，改完可 fs_read 复核。\n"          \
    "你具备两个联网工具：web_search（搜索引擎，参数 query/max_results）与 "    \
    "web_fetch（抓取网页正文，参数 url）。当问题涉及实时信息、最新新闻、时效"   \
    "性数据，或你知识截止日期（2025-05）之后发生的事件，必须调用 web_search "  \
    "获取最新结果，必要时再用 web_fetch 深入抓取；不要凭过时知识硬答。"        \
    "工具结果返回后，基于结果组织回答并标注信息时效。工具结果是真实抓取的"    \
    "内容，除非结果为空或明确报错，否则不得声称\"搜索失败\"\"结果无关\"或"      \
    "编造工具异常原因（如\"分词有问题\"）；应逐条核实返回的标题/摘要/链接，"    \
    "据实引用作答。\n"                                                           \
    "系统上下文已注入宿主机当前时间。用户问现在几点/今天几号/星期几等时间类"    \
    "问题时，直接依据注入的时间作答，不要为查询时间调用任何工具。"

/* 2.3.4 宿主机时间注入（2026-08-17 补强）：system prompt 声明"已注入宿主机
 * 当前时间"，但此前从未真正注入——LLM 只能靠知识截止日期猜测，问"今天几号"
 * 会答错。现在每次会话真实注入本地时间（含时区偏移）。静态缓冲够用（每个
 * 会话一条 system 消息，msgbuf 已 STRDUP 复制，生命周期安全）。 */
static const char *cli_system_prompt_now(void)
{
    static char s_sys[1536];
    time_t now = time(NULL);
    struct tm tmv;
    if (airy_localtime_r(&now, &tmv) != 0)
        return CLI_SYSTEM_PROMPT; /* 时间转换失败时回退无时间戳提示 */
    char ts[96];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S %z", &tmv);
    snprintf(s_sys, sizeof(s_sys),
             "当前宿主机时间：%s（本地时区）。\n%s", ts, CLI_SYSTEM_PROMPT);
    return s_sys;
}

#define CLI_CLASSIFY_PROMPT                                                    \
    "判断用户输入属于【任务指令】还是【普通对话】。任务指令：要求执行具体工程" \
    "任务（实现、开发、构建、创建、编写、修复、重构、部署等），需要改动代码、" \
    "文件或系统；普通对话：寒暄、提问、解释、闲聊、搜索/查询/了解实时信息（如" \
    "新闻、天气、资料、概念解释）、读取/查看/编辑单个本地文件（读文件、列目录、" \
    "改文件内容等日常操作），只需回答或联网检索或调用文件工具即可完成，无需"   \
    "进入任务调度管线。只输出 JSON：{\"type\":\"task\"} 或 {\"type\":\"chat\"}"

/* ============================================================================
 * 聊天工具回路（Claude Code / Codex tool-use 范式，2026-08-15）
 *
 * 日常对话同样可以调用工具：模型看到 web_search / web_fetch 的定义，回答时
 * 可返回 tool_calls；CLI 执行工具、把结果以 role="tool" 消息回填，再请求
 * 一轮，直到模型不再调用工具，最后流式渲染最终回复。工具行为（Bing/DDG 抓
 * 取）由 tool_d 的 builtin_net.c 提供（真实实现，非桩），CLI 只做协议编排：
 * 工具执行细节在 tool_d，对话策略在 CLI（机制/策略分离，Linux 哲学）。
 *
 * 安全护栏：工具轮上限 CLI_CHAT_TOOL_MAX_ROUNDS（防模型无限工具循环）；
 * 工具结果按折叠摘要渲染到屏幕（全量文本仍回填模型上下文）。
 * ============================================================================ */
#define CLI_CHAT_TOOL_MAX_ROUNDS 8
#define CLI_CHAT_TOOL_RESULT_CAP 12000 /* 单工具结果回填模型的最大字节数 */

#ifdef AIRY_HAS_CJSON
/* tool_result_t 定义于 daemons/tool_d/include/tool_service.h（CLI 已含该
 * include 路径）；web_search_tool / web_fetch_tool 声明于 tool_d/src/
 * tool_builtin_internal.h（src 目录不对外），已编译进 airy_tool_service
 * 库（builtin_net.c），此处 extern 声明直接链接调用。 */
#include "tool_service.h"

#ifdef _WIN32
/* Windows：builtin_net.c 的 web_search_tool 仅在 POSIX 编译（#ifndef
 * _WIN32），此处提供与 web_fetch_tool 平台降级一致的实现——真实报告平台
 * 能力边界（网络工具在 Windows 暂缺 curl 子进程实现），非桩函数。 */
int web_search_tool(const char *params_json, tool_result_t *res)
{
    (void)params_json;
    if (!res)
        return AIRY_ERR_INVALID_PARAM;
    res->error = AIRY_STRDUP("web_search is not supported on this platform");
    return AIRY_ERR_NOT_SUPPORTED;
}
#else
int web_search_tool(const char *params_json, tool_result_t *res);
#endif
int web_fetch_tool(const char *params_json, tool_result_t *res);

/* OpenAI function-calling schema（聊天工具回路）。工具行为 SSoT 在 tool_d
 * （builtin.c 真实实现）；本 schema 与 gateway_tools_schema.h 保持同构
 * （2026-08-16 对齐）。本地文件读写 + 联网检索构成超级智能体的日常能力：
 * fs_read/fs_write/fs_list/fs_glob/fs_grep/fs_edit/fs_delete + web_search/web_fetch。
 * shell_run / git_* 不入聊天回路（高危，留给任务管线审批链）。 */
static const char *CLI_CHAT_TOOLS_JSON =
    "["
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_read\","
    "\"description\":\"Read a file's content from the local filesystem\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"}},\"required\":[\"path\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_write\","
    "\"description\":\"Write content to a local file (creates or overwrites)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},\"content\":{\"type\":\"string\"}},"
    "\"required\":[\"path\",\"content\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_list\","
    "\"description\":\"List entries of a local directory (JSON array)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"}},\"required\":[]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_glob\","
    "\"description\":\"List files matching a glob pattern (supports * ? and **)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"pattern\":{\"type\":\"string\"},\"base\":{\"type\":\"string\"}},"
    "\"required\":[\"pattern\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_grep\","
    "\"description\":\"Search file contents with a regular expression (relpath:line:text)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"pattern\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},"
    "\"glob\":{\"type\":\"string\"},\"max_results\":{\"type\":\"integer\"}},"
    "\"required\":[\"pattern\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_edit\","
    "\"description\":\"Replace an exact string in a file (search-and-replace edit)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},\"old\":{\"type\":\"string\"},"
    "\"new\":{\"type\":\"string\"},\"count\":{\"type\":\"integer\"}},"
    "\"required\":[\"path\",\"old\",\"new\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"fs_delete\","
    "\"description\":\"Delete a local file, or a directory (recursive=1 for "
    "non-empty trees; destructive)\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"path\":{\"type\":\"string\"},\"recursive\":{\"type\":\"boolean\"}},"
    "\"required\":[\"path\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"web_search\","
    "\"description\":\"Search the web for up-to-date information relevant to "
    "the user's question. Returns ranked result titles, URLs and snippets.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"query\":{\"type\":\"string\",\"description\":\"Search query\"},"
    "\"max_results\":{\"type\":\"integer\",\"description\":\"Max result count, "
    "1-8\",\"minimum\":1,\"maximum\":8}},"
    "\"required\":[\"query\"]}}},"
    "{\"type\":\"function\",\"function\":{"
    "\"name\":\"web_fetch\","
    "\"description\":\"Fetch and read the text content of a web page by URL.\","
    "\"parameters\":{\"type\":\"object\",\"properties\":{"
    "\"url\":{\"type\":\"string\",\"description\":\"Full http(s) URL\"}},"
    "\"required\":[\"url\"]}}}"
    "]";

/* 动态消息缓冲：工具轮需要逐步追加 assistant tool_calls 与 role="tool"
 * 结果消息。content / tool_call_id / tool_calls_json 的副本由 owned 数组
 * 统一管理，msgbuf_free 时一并释放。 */
typedef struct {
    llm_message_t *msgs;
    size_t count;
    size_t cap;
    char **owned;
    size_t owned_count;
    size_t owned_cap;
} cli_chat_msgbuf_t;

static void cli_msgbuf_free(cli_chat_msgbuf_t *b)
{
    if (!b)
        return;
    for (size_t i = 0; i < b->owned_count; i++)
        AIRY_FREE(b->owned[i]);
    AIRY_FREE(b->owned);
    AIRY_FREE(b->msgs);
    AIRY_MEMSET(b, 0, sizeof(*b));
}

static const char *cli_msgbuf_own(cli_chat_msgbuf_t *b, const char *s)
{
    if (!s)
        return NULL;
    char *copy = AIRY_STRDUP(s);
    if (!copy)
        return NULL;
    if (b->owned_count >= b->owned_cap) {
        size_t new_cap = b->owned_cap ? b->owned_cap * 2 : 16;
        char **grown = (char **)AIRY_REALLOC(b->owned, new_cap * sizeof(char *));
        if (!grown) {
            AIRY_FREE(copy);
            return NULL;
        }
        b->owned = grown;
        b->owned_cap = new_cap;
    }
    b->owned[b->owned_count++] = copy;
    return copy;
}

static void cli_msgbuf_push(cli_chat_msgbuf_t *b, const char *role, const char *content,
                            const char *tool_call_id, const char *tool_calls_json,
                            const char *reasoning_content)
{
    if (b->count >= b->cap) {
        size_t new_cap = b->cap ? b->cap * 2 : 16;
        llm_message_t *grown = (llm_message_t *)AIRY_REALLOC(b->msgs, new_cap * sizeof(llm_message_t));
        if (!grown)
            return;
        b->msgs = grown;
        b->cap = new_cap;
    }
    llm_message_t *m = &b->msgs[b->count++];
    m->role = role;
    m->content = cli_msgbuf_own(b, content);
    m->tool_call_id = cli_msgbuf_own(b, tool_call_id);
    m->tool_calls_json = cli_msgbuf_own(b, tool_calls_json);
    /* DeepSeek thinking mode requires the assistant turn's reasoning_content
     * to be echoed verbatim on tool-loop re-send (else HTTP 400). */
    m->reasoning_content = cli_msgbuf_own(b, reasoning_content);
}

/* 工具执行主体：CLI 聊天回路的默认身份（ACL permission_rules.yaml 的
 * coding_v1 标准工作集）。tool_d 的 execute RPC 按该主体做 fail-closed
 * 权限判定，未显式 allow 的工具一律拒绝。 */
#define CLI_TOOL_AGENT "coding_v1"
#define CLI_TOOL_RPC_TIMEOUT_MS 30000

/* 相对路径绝对化：tool_d 在 daemon 启动目录解析相对路径，与 CLI 的 cwd
 * 不一致（2026-08-16 实测：模型传 "test_edit.txt" 被 tool_d 按自己的 cwd
 * 解析失败）。CLI 是用户会话的语义边界——用户说"当前目录"就是 CLI 的
 * cwd，故将 fs 工具的路径参数基于 CLI cwd 转为绝对路径（并规范化
 * "." / ".." 段）。 */
static void cli_tool_absolutize_path(cJSON *args, const char *key)
{
    cJSON *v = cJSON_GetObjectItem(args, key);
    if (!v || !cJSON_IsString(v) || !v->valuestring || !v->valuestring[0])
        return;
    const char *p = v->valuestring;
    if (p[0] == '/')
        return;
#if !defined(_WIN32)
    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd)))
        return;
    size_t cl = strlen(cwd);
    size_t pl = strlen(p);
    if (cl + pl + 2 > sizeof(cwd))
        return;
    char out[4096];
    snprintf(out, sizeof(out), "%s/%s", cwd, p);
    /* 逐段规范化：空段与 "." 跳过；".." 回退上一段（cwd 为绝对路径）；*/
    /* 其余段原样保留。dst 指向当前写入位置，回退即移回上一 '/'。 */
    char *dst = out;
    const char *src = out;
    for (;;) {
        const char *seg = src;
        const char *slash = strchr(seg, '/');
        size_t seg_len = slash ? (size_t)(slash - seg) : strlen(seg);
        if (seg_len == 0 || (seg_len == 1 && seg[0] == '.')) {
            src = slash ? slash + 1 : seg + seg_len;
            if (!slash)
                break;
            continue;
        }
        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.') {
            if (dst > out + 1) {
                dst -= 1;
                while (dst > out && dst[-1] != '/')
                    dst -= 1;
            }
            src = slash ? slash + 1 : seg + seg_len;
            if (!slash)
                break;
            continue;
        }
        dst[0] = '/';
        __builtin_memcpy(dst + 1, seg, seg_len);
        dst += 1 + seg_len;
        src = slash ? slash + 1 : seg + seg_len;
        if (!slash)
            break;
    }
    *dst = '\0';
    if (out[0] == '\0')
        snprintf(out, sizeof(out), "%s/", cwd);
    cJSON_SetValuestring(v, out);
#else
    (void)p; /* Windows 平台：保持原样（daemon 与 CLI 同盘时语义一致） */
#endif
}

/* 执行一个工具调用，返回回填模型的 result JSON（OWNER，AIRY_FREE）。
 * 统一走 tool_d daemon RPC（tool.sock execute，带 ACL fail-closed），与
 * gateway agent.run 同一路径；不直连 builtin 库（那会绕过权限边界）。
 * tool_d 离线或 RPC 失败时返回可诊断的错误 JSON。 */
static char *cli_chat_exec_tool(const char *tool_id, const char *args_json, int *out_ok)
{
    if (!tool_id || !args_json) {
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"missing tool arguments\"}");
    }

    char sock[512];
    snprintf(sock, sizeof(sock), "%s/tool.sock", cli_rt_dir());

    cJSON *params = cJSON_CreateObject();
    if (!params) {
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"out of memory\"}");
    }
    cJSON_AddStringToObject(params, "tool_id", tool_id);
    cJSON *pargs = cJSON_Parse(args_json);
    if (!pargs) {
        cJSON_Delete(params);
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"invalid tool arguments\"}");
    }
    /* 相对路径绝对化（基于 CLI cwd；tool_d 按自身 cwd 解析）：
     * fs 工具的 path 参数与 fs_glob 的 base 参数。 */
    if (strcmp(tool_id, "fs_read") == 0 || strcmp(tool_id, "fs_write") == 0 ||
        strcmp(tool_id, "fs_list") == 0 || strcmp(tool_id, "fs_edit") == 0 ||
        strcmp(tool_id, "fs_grep") == 0 || strcmp(tool_id, "fs_delete") == 0) {
        cli_tool_absolutize_path(pargs, "path");
    } else if (strcmp(tool_id, "fs_glob") == 0) {
        cli_tool_absolutize_path(pargs, "base");
        if (!cJSON_GetObjectItem(pargs, "base")) {
            char cwd[4096];
#if !defined(_WIN32)
            if (getcwd(cwd, sizeof(cwd)))
                cJSON_AddStringToObject(pargs, "base", cwd);
#else
            (void)cwd;
#endif
        }
    }
    cJSON_AddItemToObject(params, "params", pargs);
    cJSON_AddStringToObject(params, "agent_id", CLI_TOOL_AGENT);
    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json) {
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"out of memory\"}");
    }

    char *result_json = NULL;
    int rpc_ret = daemon_rpc_call(sock, "execute", params_json, &result_json,
                                  CLI_TOOL_RPC_TIMEOUT_MS);
    AIRY_FREE(params_json);
    if (rpc_ret != 0 || !result_json) {
        cli_trace("chat", "%s tool rpc fail tool=%s ret=%d sock=%s", CLI_ICON_CROSS, tool_id,
                  rpc_ret, sock);
        AIRY_FREE(result_json);
        if (out_ok)
            *out_ok = 0;
        return AIRY_STRDUP("{\"ok\":false,\"error\":\"tool_d unreachable\"}");
    }

    /* daemon_rpc_call 已解包 JSON-RPC 的 result 字段：result_json 即
     * {"success":..,"output":"..","error":"..","exit_code":..}（2026-08-16
     * 实测：误以为带外层 "result" 导致解析失败，工具全部报 ✗）。 */
    cJSON *root = cJSON_Parse(result_json);
    AIRY_FREE(result_json);
    int ok = 0;
    char *out_json = NULL;
    if (root) {
        cJSON *succ = cJSON_GetObjectItem(root, "success");
        /* tool_d 返回 "success":1（数字），cJSON_IsTrue 只认 true 字面量；
         * 数字与布尔都按非 0 判定（2026-08-16 实测误判 ok=0）。 */
        ok = (succ && (cJSON_IsTrue(succ) || (cJSON_IsNumber(succ) && succ->valueint != 0))) ? 1
                                                                                            : 0;
        cJSON *out = cJSON_GetObjectItem(root, "output");
        cJSON *err = cJSON_GetObjectItem(root, "error");
        cJSON *r = cJSON_CreateObject();
        cJSON_AddBoolToObject(r, "ok", ok);
        if (ok && out && cJSON_IsString(out) && out->valuestring) {
            cJSON_AddStringToObject(r, "output", out->valuestring);
        } else {
            cJSON_AddStringToObject(r, "error",
                                    (err && cJSON_IsString(err) && err->valuestring)
                                        ? err->valuestring
                                        : "tool failed");
        }
        out_json = cJSON_PrintUnformatted(r);
        cJSON_Delete(r);
        cJSON_Delete(root);
    }
    if (!out_json)
        out_json = AIRY_STRDUP("{\"ok\":false,\"error\":\"tool response parse failed\"}");
    cli_trace("chat", "%s tool exec tool=%s ok=%d resp=%.*s", ok ? CLI_ICON_CHECK : CLI_ICON_CROSS,
              tool_id, ok, (int)cli_utf8_safe_len(out_json, 120), out_json ? out_json : "");
    if (out_ok)
        *out_ok = ok;
    return out_json;
}

/* 解析一轮 LLM 返回的 tool_calls，渲染 + 执行 + 回填消息缓冲。
 * 返回 0 = 本轮已消费（调用方应继续下一轮）；-1 = 终止（解析失败/无工具）。 */
static int cli_chat_tool_round(cli_chat_msgbuf_t *b, const llm_response_t *resp)
{
    if (!resp || resp->choice_count == 0 || !resp->choices[0].tool_calls_json)
        return -1;

    cJSON *calls = cJSON_Parse(resp->choices[0].tool_calls_json);
    if (!calls || !cJSON_IsArray(calls)) {
        if (calls)
            cJSON_Delete(calls);
        return -1;
    }
    size_t n = (size_t)cJSON_GetArraySize(calls);
    if (n == 0) {
        cJSON_Delete(calls);
        return -1;
    }

    /* assistant 消息：保留原 content 与 reasoning_content，附 tool_calls
     * （OpenAI 要求续轮必需 tool_calls；DeepSeek thinking 要求回传
     * reasoning_content，否则 tool 续轮 HTTP 400） */
    cli_trace("chat", "tool-round reasoning=%s tools=%zu",
              resp->choices[0].reasoning_content ? "yes" : "no",
              strlen(resp->choices[0].tool_calls_json));
    cli_msgbuf_push(b, "assistant", resp->choices[0].content, NULL,
                    resp->choices[0].tool_calls_json,
                    resp->choices[0].reasoning_content);

    cJSON *call = NULL;
    cJSON_ArrayForEach(call, calls)
    {
        cJSON *fn = cJSON_GetObjectItem(call, "function");
        cJSON *id = cJSON_GetObjectItem(call, "id");
        const char *name = fn && cJSON_IsObject(fn) ? cJSON_GetObjectItem(fn, "name")->valuestring : NULL;
        const char *args = fn && cJSON_IsObject(fn) ? cJSON_GetObjectItem(fn, "arguments")->valuestring : NULL;
        const char *call_id = (id && cJSON_IsString(id)) ? id->valuestring : "call_unknown";
        if (!name)
            name = "unknown";

        /* 工具调用过程卡片：⚙ 动作名…（参数/返回内容不暴露在对话中；
         * 操作细节经 cli_trace 留档供 -p 管道与日志诊断）。 */
        cli_trace("chat", "tool call %s args=%.*s", name, (int)cli_utf8_safe_len(args, 160), args ? args : "{}");
        cli_render_tool_use(name, args);

        int ok = 0;
        char *result_json = cli_chat_exec_tool(name, args ? args : "{}", &ok);
        if (!result_json)
            result_json = AIRY_STRDUP("{\"ok\":false,\"error\":\"tool failed\"}");

        /* 折叠摘要渲染（全量文本已回填模型上下文） */
        const char *detail = NULL;
#ifdef AIRY_HAS_CJSON
        cJSON *rroot = cJSON_Parse(result_json);
        if (rroot) {
            cJSON *out = cJSON_GetObjectItem(rroot, "output");
            cJSON *err = cJSON_GetObjectItem(rroot, "error");
            detail = (ok && out && cJSON_IsString(out)) ? out->valuestring
                     : (err && cJSON_IsString(err)) ? err->valuestring
                                                    : NULL;
        }
#else
        detail = ok ? NULL : result_json;
#endif
        /* UAF 修复（2026-08-16，ASan heap-use-after-free）：detail 指向
         * cJSON 树内字符串（out->valuestring），必须在渲染消费完之后再
         * 释放树；此前 cJSON_Delete(rroot) 先于 cli_render_tool_result
         * 执行，工具回路偶发崩溃。 */
        cli_render_tool_result(name, detail, ok);
#ifdef AIRY_HAS_CJSON
        if (rroot)
            cJSON_Delete(rroot);
#endif

        /* role="tool" 消息回填：携带匹配的 tool_call_id；超长结果在 UTF-8
         * 字符边界截断，防止上下文无界膨胀（web_fetch 页面可能很大）。 */
        char trunc_buf[CLI_CHAT_TOOL_RESULT_CAP + 1];
        const char *feed = result_json;
        size_t rl = strlen(result_json);
        if (rl > CLI_CHAT_TOOL_RESULT_CAP) {
            size_t n = CLI_CHAT_TOOL_RESULT_CAP;
            while (n > 0 && ((unsigned char)result_json[n] & 0xC0) == 0x80)
                n--;
            AIRY_MEMCPY(trunc_buf, result_json, n);
            trunc_buf[n] = '\0';
            feed = trunc_buf;
        }
        cli_msgbuf_push(b, "tool", feed, call_id, NULL, NULL);
        AIRY_FREE(result_json);
    }
    cJSON_Delete(calls);
    return 0;
}

#endif /* AIRY_HAS_CJSON */

/**
  * @brief Ask the LLM to classify input as task or chat (returns 1=task 0=chat -1=failure)
  *
  * Reasoning-model note: the classifier must not be starved of output tokens.
  * With a tiny max_tokens a thinking model (e.g. DeepSeek) spends the whole
  * budget on reasoning_content and emits an empty content, which parses as a
  * failure here and degrades intent routing (2026-08-16: "北京今天天气怎么样"
  * misrouted to the task pipeline this way). 64 tokens leaves room for the
  * JSON verdict after the chain of thought.
  */
static int cli_llm_classify(const char *input)
{
    if (!g_chat_adapter || !input)
        return -1;

    /* llm_message_t carries optional fields (reasoning_content/tool_call_id/
     * tool_calls_json) that build_llm_request_json dereferences; zero the
     * array so unset fields are never garbage pointers (2026-08-16). */
    llm_message_t msgs[2];
    __builtin_memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = "system";
    msgs[0].content = CLI_CLASSIFY_PROMPT;
    msgs[1].role = "user";
    msgs[1].content = input;

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    cfg.model = getenv("AIRY_MODEL_T1F");
    cfg.messages = msgs;
    cfg.message_count = 2;
    cfg.temperature = 0.0f;
    cfg.max_tokens = 64;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
    if (ret != 0 || !resp || !resp->choices || resp->choice_count == 0 ||
        !resp->choices[0].content) {
        if (resp)
            llm_response_free(resp);
        return -1;
    }

    const char *content = resp->choices[0].content;
    /* The classifier prompt mandates {"type":"task"} / {"type":"chat"}; the
     * JSON verdict key with optional spacing still carries the literal
     * "task" token, so a plain substring match is sufficient and robust.
     * Only the emitted content counts: the chain-of-thought may mention
     * "task" while concluding "chat" (thinking models), so never consult
     * reasoning_content — an empty content degrades to the chat default. */
    int is_task = (content[0] != '\0' && strstr(content, "\"task\"") != NULL);
    llm_response_free(resp);
    return is_task ? 1 : 0;
}

/**
  * @brief Intent routing: fast heuristic check + LLM confirmation
  *
  * Clear task words -> task set; clear chat words -> chat set; ambiguous -> LLM;
  * fails SAFE to the chat set (the super-agent default): a misrouted "task"
  * sent into the task pipeline can stall/act, whereas a chat reply is always
  * harmless and lets the user rephrase. The explicit task_marks fast path
  * still catches unambiguous task commands without any LLM round trip.
  *
  * 启发式词表与优先级（consult > task > chat）见 cli_classify_heuristic
  * （2.5.x 意图分辨，独立纯函数可单测）。
  */
int cli_classify_input(const char *input)
{
    int h = cli_classify_heuristic(input);
    if (h >= 0)
        return h;

    int cls = cli_llm_classify(input);
    return cls >= 0 ? cls : 0;
}

/**
  * @brief Chat-set handling: reply to the user directly as the super agent
  *
  * Decision A (2026-08-09): daily chat is generated and routed by the B model
  * (t1-f); no full dual-thinking loop (t2/t1-f/t1-p critique). Single t1-f
  * model replies (AIRY_MODEL_T1F; falls back to the provider default).
  *
  * Decision C (2026-08-15): the chat turn is a tool loop. The model sees the
  * web_search / web_fetch tools and may return tool_calls; the CLI executes
  * them (real implementations in tool_d builtin_net), feeds the results back
  * as role="tool" messages, and repeats until the model answers without
  * tools. Rendering follows the Claude Code tool-use convention: tool cards
  * (⛏ name + folded result) during the loop, then the final reply rendered
  * as markdown. -p 模式整轮走 complete_stream（增量文本实时直出 stdout）；
  * tool_calls 由 provider 以控制帧暴露，流式结束后的响应与非流式同构，
  * 工具轮逻辑完全复用（2026-08-16）。交互模式仍非流式（spinner + markdown
  * 完整渲染），避免流式下 markdown 标记裸露。
  */

/* -p 模式流式渲染回调：把增量文本直写 stdout（stdout 保持纯净可管道；
 * 工具/进度 chrome 走 stderr，见 cli_trace）。交互模式仍走 spinner +
 * markdown 完整渲染，避免流式下 markdown 标记裸露。
 *
 * 流式归一化：部分模型用 [code]/[/code] 包裹代码而非 markdown 围栏
 * ``` ```。交互模式由 cli_render_markdown 统一识别；-p 流式直出时必须
 * 在此归一化（[code] → ```），否则管道输出裸露标签。跨 chunk 边界
 * （token 被 provider 截断成多个增量）用静态 carry 缓冲拼接。 */
static char s_code_carry[8];
static size_t s_code_carry_len = 0;

/* 交互 TTY 流式：首片到达时擦除 "Connecting..." 提示行（stderr 上的
 * 光标行），避免残留。仅在交互 TTY 流式模式下有意义（-p 不打印提示）。 */
static int s_stream_first_chunk = 0;

/* 交互 TTY 流式状态（2026-08-17）：流式预览直出后记录其物理行数，
 * 完成后擦除预览并完整重绘最终形态（结果不折叠，仅思考链折叠）。
 * 仅最终轮有意义（工具轮的预览保留为过程展示，不擦除）。 */
static size_t g_chat_fold_phys = 0;
/* 预览末尾是否无换行（光标停在最后一行行尾）：擦除时上移行数须少 1，
 * 否则会把预览起点上方那行（消息行）一并覆盖。 */
static int g_chat_fold_tail_no_nl = 0;

static void cli_stream_norm_emit(const char *s, size_t n)
{
    for (size_t i = 0; i < n; i++)
        cli_outc(s[i]);
    fflush(stdout);
}

static void cli_stream_norm_flush_carry(void)
{
    if (s_code_carry_len) {
        cli_stream_norm_emit(s_code_carry, s_code_carry_len);
        s_code_carry_len = 0;
    }
}

/* s[0..n) 是否是 [code] 或 [/code] 的严格前缀（不含完整标签本身）？ */
static int cli_code_prefix(const char *s, size_t n)
{
    static const char *const tags[] = {"[code]", "[/code]"};
    if (n == 0 || n >= 7)
        return 0;
    for (size_t t = 0; t < sizeof(tags) / sizeof(tags[0]); t++) {
        if (n < strlen(tags[t]) && strncmp(s, tags[t], n) == 0)
            return 1;
    }
    return 0;
}

/* 2.3.5/2.3.6：thinking 模型（DeepSeek/Kimi 等）先产思考链再产正文，
 * 思考阶段可长达数十秒。原始推理碎片逐块上屏无阅读价值（用户反馈
 * "太长看不懂"），因此思考期间只显示轻量进度行
 * （[Dual Fast Think] 思考中… N 字 · 耗时，\r 原地刷新），首个
 * content 分片到达即擦除；完整思考链在回复完成后折叠展示。
 * 通道说明：交互模式 stderr 被重定向到日志，实时 UI 反馈（Connecting /
 * 思考进度）与流式正文**同流直写 stdout**（同一 fd 光标同步，\r 覆盖
 * 不留痕）；-p / --json / TUI 下这些反馈被各自前置条件禁用。 */
static cli_actor_t cli_chat_think_actor(void);
static int s_reasoning_progress = 0;      /* 进度行当前可见 */
static int s_reasoning_chars = 0;         /* 思考字数累计 */
static uint64_t s_reasoning_start_ms = 0; /* 思考阶段开始时刻 */

static void cli_chat_reasoning_cb(const char *delta, void *user_data)
{
    (void)user_data;
    /* 仅交互 TTY 流式显示进度；-p / --json / TUI 走各自无进度路径。 */
    if (!delta || g_cli_print_mode || g_cli_json_mode)
        return;
    /* 正文已开始（首片到达）：思考阶段结束，不再刷新进度行——上游
     * reasoning 增量与 content 首片可能交错到达（DeepSeek 流式分块），
     * 否则进度行 \r 覆盖会压到正文首行（2026-08-19 实测竞态）。 */
    if (!s_stream_first_chunk)
        return;
    if (!s_reasoning_start_ms)
        s_reasoning_start_ms = cli_now_ms();
    s_reasoning_chars += (int)strlen(delta);
    s_reasoning_progress = 1;
    uint64_t ms = cli_now_ms() - s_reasoning_start_ms;
    /* 角色名与全站一致：[Dual Slow/Fast/Prof Think]（2.3.14） */
    fprintf(stdout, "\r    %s[%s]%s 思考中… %d 字 · %lu.%lus   ",
            cli_c(CLR_YELLOW), cli_render_actor_name(cli_chat_think_actor()),
            cli_c(CLR_RESET), s_reasoning_chars, (unsigned long)(ms / 1000),
            (unsigned long)(ms % 1000) / 100);
    fflush(stdout);
}

static void cli_chat_reasoning_clear(void)
{
    if (s_reasoning_progress) {
        s_reasoning_progress = 0;
        /* 2K erases the whole line (not just cursor-to-end): a partially
         * rendered progress row must not leak "… 字 · 0.6s" onto the first
         * streaming-reply line (reported debris). */
        fputs("\r\033[2K", stdout);
        fflush(stdout);
    }
}

static void cli_chat_stream_cb(const char *chunk, void *user_data)
{
    (void)user_data;
    if (!chunk)
        return;
    size_t n = strlen(chunk);
    if (n == 0)
        return;

    /* 首片到达：擦除 "Connecting..." / 思考进度提示行（同位置） */
    if (s_stream_first_chunk) {
        s_stream_first_chunk = 0;
        s_reasoning_progress = 0; /* 进度行已被首片擦除，避免收尾重复擦除 */
        fputs("\r\033[2K", stdout);
        fflush(stdout);
    }

    /* 跨 chunk：上一片末尾的标签前缀先与本次开头拼接判断。 */
    if (s_code_carry_len) {
        size_t take = 7 - s_code_carry_len;
        if (take > n)
            take = n;
        AIRY_MEMCPY(s_code_carry + s_code_carry_len, chunk, take);
        s_code_carry_len += take;
        chunk += take;
        n -= take;
        if (strncmp(s_code_carry, "[code]", 6) == 0 ||
            strncmp(s_code_carry, "[/code]", 7) == 0) {
            cli_stream_norm_emit("```", 3);
            s_code_carry_len = 0;
        } else {
            cli_stream_norm_flush_carry();
        }
        if (n == 0)
            return;
    }

    size_t i = 0;
    while (i < n) {
        if (n - i >= 6 && strncmp(chunk + i, "[code]", 6) == 0) {
            cli_stream_norm_emit("```", 3);
            i += 6;
            continue;
        }
        if (n - i >= 7 && strncmp(chunk + i, "[/code]", 7) == 0) {
            cli_stream_norm_emit("```", 3);
            i += 7;
            continue;
        }
        /* chunk 末尾的标签前缀（被 token 截断）→ 缓存，不提前输出。 */
        if (i + 6 >= n && cli_code_prefix(chunk + i, n - i)) {
            AIRY_MEMCPY(s_code_carry, chunk + i, n - i);
            s_code_carry_len = n - i;
            return;
        }
        cli_outc(chunk[i]);
        i++;
    }
    fflush(stdout);
}

/* 2.3.14 (2026-08-17)：对话思考链来自 t1-f（context arbiter）模型，
 * 实时思考标签为 [Dual Fast Think]；未配置（走 llm_d 默认模型）时
 * 回落通用 [Dual Think]。2026-08-19：t1-f 配置统一来自
 * cli_think_cfg_load（env > model.yaml think 段 > llm.model 默认），
 * 与 think_d 实际生效模型一致；静态缓存避免每帧进度行重复读文件。 */
static const char *cli_chat_t1f_cached(void)
{
    static char s_t1f[128];
    static int s_loaded = 0;
    if (!s_loaded) {
        s_loaded = 1;
        char t2[128], t1p[128];
        cli_think_cfg_load(t2, sizeof(t2), s_t1f, sizeof(s_t1f), t1p, sizeof(t1p));
    }
    return s_t1f;
}

/* 2.1.1.2 修复：t1-p（PROF）模型缓存——GCCP 意图确认（[Dual Prof Think]）
 * 使用该模型槽，与 CLI 渲染标签一致。配置源与 t1f 同（cli_think_cfg_load：
 * env > model.yaml think 段 > 默认）。 */
static const char *cli_chat_t1p_cached(void)
{
    static char s_t1p[128];
    static int s_loaded = 0;
    if (!s_loaded) {
        s_loaded = 1;
        char t2[128], t1f[128];
        cli_think_cfg_load(t2, sizeof(t2), t1f, sizeof(t1f), s_t1p, sizeof(s_t1p));
    }
    return s_t1p;
}

static cli_actor_t cli_chat_think_actor(void)
{
    const char *t1f = cli_chat_t1f_cached();
    return (t1f && t1f[0]) ? CLI_ACTOR_DUAL_FAST_THINK : CLI_ACTOR_DUAL_THINK;
}

void cli_chat_reply(const char *input)
{
    if (!g_chat_adapter) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "对话",
                             "对话服务不可用（模型服务未连接）。");
        return;
    }

    /* 2.1.1.5：新一轮开始清零统计（main.c 在上一轮结束后已读取展示）。 */
    g_chat_tokens_total = 0;
    g_chat_cost_total = 0.0;
    if (g_chat_reasoning_acc) {
        AIRY_FREE(g_chat_reasoning_acc);
        g_chat_reasoning_acc = NULL;
    }

    const char *t1f_model = cli_chat_t1f_cached();
    cli_trace("chat", "%s start model=%s", CLI_ICON_DIAMOND,
              (t1f_model && t1f_model[0]) ? t1f_model : "default");

    /* Decision B (2026-08-09): config reminder - t1-f (B model) activates first.
      * If unset, hint the three config points and order without blocking (provider default).
      * The hint prints once per session only. 2026-08-17: 改为 cli_trace 输出
      * （日志/stderr），不再渲染进对话列——配置提示是内部运维信息，出现在
      * 会话正文会污染对话（用户要求对话只展示过程，不暴露内部细节）。
      * 2026-08-19: model.yaml 的 llm.model 默认可满足 t1-f，此时不再提示。 */
    static int s_t1f_hint_shown = 0;
    if (!s_t1f_hint_shown && (!t1f_model || !t1f_model[0])) {
        s_t1f_hint_shown = 1;
        cli_trace("config",
                  "t1-f (B model) not configured; set AIRY_MODEL_T1F (local Ollama/vLLM or "
                  "cloud API), then AIRY_MODEL_T2 (A) and AIRY_MODEL_T1P (C) as needed. "
                  "Chat will use the llm_d default model for now.");
    }

#ifdef AIRY_HAS_CJSON
    /* 消息缓冲：[system] + history + [current input]；工具轮会动态扩展
     * （assistant tool_calls + role="tool" 结果），所有内容副本由缓冲统一管理。 */
    cli_chat_msgbuf_t buf;
    AIRY_MEMSET(&buf, 0, sizeof(buf));
    /* 2.2.4 对话记忆读取：相关历史记忆注入 system 上下文（此前对话
     * 路径零记忆，用户"记不住/想不准"的直接根因） */
    char mem_sys[768];
    cli_chat_mem_inject_system(input, mem_sys, sizeof(mem_sys));
    cli_msgbuf_push(&buf, "system", cli_system_prompt_now(), NULL, NULL, NULL);
    /* 1.3 推理语言网关：语言约束 System Prompt 注入（首条 system 之后）。
     * 约束模型内部推理语言与最终输出语言，从源头抑制语言漂移。 */
    if (g_cli_lang_sys_prompt && g_cli_lang_sys_prompt[0])
        cli_msgbuf_push(&buf, "system", g_cli_lang_sys_prompt, NULL, NULL, NULL);
    if (mem_sys[0])
        cli_msgbuf_push(&buf, "system", mem_sys, NULL, NULL, NULL);
    for (size_t hi = 0; hi < g_history_count; hi++)
        cli_msgbuf_push(&buf, g_history_roles[hi], g_history_contents[hi], NULL, NULL,
                        g_history_reasonings[hi]);
    cli_msgbuf_push(&buf, "user", input, NULL, NULL, NULL);

    /* 交互 TTY 走流式（打字机预览，完成后折叠/重绘最终形态）；
     * TUI、--json 与 -p 保持非流式（markdown 完整渲染进历史 / 结构化
     * 输出 / 纯 stdout 最终答案）。-p 非流式还避免工具轮之间模型的
     * 过程叙述混入 stdout——脚本模式只消费最终回答（2026-08-17）。
     * 交互流式不再使用 spinner——打字机即进度指示。 */
    cli_tui_t *tui = cli_tui_get_default();
    int tui_active = tui && cli_tui_active(tui);
    int stream_mode = !g_cli_json_mode && !g_cli_print_mode && !tui_active;

    /* 交互模式的"思考中"状态行（spinner；流式/-p/--json 抑制 chrome）。 */
    int spinner_on = !g_cli_print_mode && !g_cli_json_mode && !stream_mode;
    if (spinner_on) {
        char think_title[128];
        /* 2.3.14：思考角色细分——t1-f 思考中显示 [Dual Fast Think] */
        snprintf(think_title, sizeof(think_title), "%s (%s)",
                 cli_render_actor_name(cli_chat_think_actor()),
                 t1f_model ? t1f_model : "default");
        cli_spinner_start(think_title);
    }

    /* 工具回路：流式模式走 complete_stream（增量文本实时直出，tool_calls
     * 经控制帧暴露）；TUI/--json 走非流式 complete（markdown / 结构化）。
     * 模型返回 tool_calls → 渲染过程卡片 + 执行 + 回填 → 续轮；
     * 不再调用工具 → final_resp 即最终回复。护栏：轮次上限。 */
    llm_response_t *final_resp = NULL;
    int tool_rounds = 0;
    int force_summary = 0; /* 工具轮次用尽：撤下工具定义，强制基于已有结果总结 */
    for (;;) {
        llm_request_config_t cfg;
        __builtin_memset(&cfg, 0, sizeof(cfg));
        cfg.model = t1f_model;
        cfg.messages = buf.msgs;
        cfg.message_count = buf.count;
        cfg.temperature = 0.7f;
        cfg.max_tokens = 2048;
        cfg.tools_json = force_summary ? NULL : CLI_CHAT_TOOLS_JSON;
        cfg.stream = stream_mode ? 1 : 0;

        llm_response_t *resp = NULL;
        int ret;
        if (stream_mode) {
            /* 交互 TTY 流式：计量本轮直出预览的行数，完成后擦除重绘
             * 最终形态（短回复 markdown 精修 / 长回复折叠）。-p 流式
             * 直出供管道消费，不计量不重绘。 */
            cli_line_meter_t meter;
            int folding = !g_cli_print_mode;
            if (folding)
                cli_render_meter_begin(&meter);
            /* 连接反馈：流式前显示连接状态（与正文同流 stdout），首片
             * 到达时擦除。避免连接阶段（RPC 握手 + 模型推理首 token）
             * 用户无任何反馈。 */
            s_stream_first_chunk = !g_cli_print_mode;
            if (s_stream_first_chunk) {
                fprintf(stdout, "    %s●%s Connecting…\r",
                        cli_c(CLR_DIM), cli_c(CLR_RESET));
                fflush(stdout);
            }
            /* 思考阶段进度：每轮重置计数，reasoning 增量经回调实时上屏 */
            s_reasoning_start_ms = 0;
            s_reasoning_chars = 0;
            s_reasoning_progress = 0;
            ret = llm_svc_adapter_complete_stream(g_chat_adapter, &cfg,
                                                  cli_chat_stream_cb, NULL,
                                                  cli_chat_reasoning_cb, NULL,
                                                  &resp);
            /* 思考进度行残留清理（reasoning-only 或异常中断场景） */
            cli_chat_reasoning_clear();
            /* 流式收尾：flush 可能残留的 [code] 前缀 carry（流提前结束时
             * 最后一片未触达标签判定的字节）。 */
            cli_stream_norm_flush_carry();
            /* 流结束但无首片到达（连接失败/空响应）：擦除 Connecting 行 */
            if (s_stream_first_chunk) {
                s_stream_first_chunk = 0;
                fputs("\r\033[2K", stdout);
                fflush(stdout);
            }
            if (folding) {
                g_chat_fold_phys = cli_render_meter_phys(&meter);
                g_chat_fold_tail_no_nl = (meter.row_len > 0) ? 1 : 0;
                cli_render_meter_end(&meter);
            }
        } else {
            ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
        }
        if (ret != 0 || !resp || resp->choice_count == 0) {
            if (resp)
                llm_response_free(resp);
            if (spinner_on)
                cli_spinner_stop(0, "reply failed");
            /* 人类可读的错误描述（数字码对用户无意义） */
            const char *err_desc = cli_chat_err_desc((int)ret);
            if (ret == 0 && (!resp || resp->choice_count == 0))
                err_desc = "模型未返回文本（可能仅生成了思考内容）";
            char line[256];
            snprintf(line, sizeof(line), "回复失败：%s", err_desc);
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "对话", line);
            cli_msgbuf_free(&buf);
            return;
        }
        /* 2.1.1.5/2.1.1.6：累计本轮真实 token/费用与思考链（工具轮与
         * 最终轮都计入；reasoning 全量保留，折叠展示之外完整进日志）。 */
        cli_chat_usage_add(resp);
        if (resp->choices[0].reasoning_content)
            cli_chat_reasoning_add(resp->choices[0].reasoning_content);
        int has_tools =
            resp->choices[0].tool_calls_json && resp->choices[0].tool_calls_json[0];
        if (has_tools && !force_summary && tool_rounds < CLI_CHAT_TOOL_MAX_ROUNDS) {
            /* 层级修复（2026-08-20）：工具轮中间叙述（"第一次搜索不太
             * 直接…"）流式直出后与工具卡片拼接在同一行，层级混乱。
             * 先擦除本轮流式直出的中间叙述预览，折叠为一行弱化摘要，
             * 工具卡片从新行独立渲染——过程叙述 / 工具卡片 / 最终
             * 结果三层清晰分隔（2.3.12 层级编排）。 */
            if (stream_mode && g_chat_fold_phys > 0 && cli_term_is_tty()) {
                fflush(stdout);
                size_t up = g_chat_fold_phys;
                if (g_chat_fold_tail_no_nl && up > 0)
                    up -= 1;
                /* CUU 参数 0 在 ANSI 中等于默认值 1（上移 1 行）！
                 * up=0 时必须避免 `\033[0A`（会误删上一行内容），
                 * 改用回车 + 清当前行。 */
                char erase[32];
                int en;
                if (up > 0)
                    en = snprintf(erase, sizeof(erase), "\033[%zuA\r\033[J", up);
                else
                    en = snprintf(erase, sizeof(erase), "\r\033[2K");
                if (en > 0)
                    fwrite(erase, 1, (size_t)en, stdout);
                fflush(stdout);
                g_chat_fold_phys = 0;
                g_chat_fold_tail_no_nl = 0;
                /* 中间叙述折叠为一行弱化摘要（保留首行 + 折叠尾） */
                const char *mid = resp->choices[0].content;
                if (mid && mid[0]) {
                    cli_render_role_line(CLI_ROLE_TRACE, cli_chat_think_actor(),
                                         "分析", NULL);
                    cli_render_collapsed(mid, 2, 1, 1);
                }
            }
            if (cli_chat_tool_round(&buf, resp) == 0) {
                tool_rounds++;
                llm_response_free(resp);
                continue;
            }
        }
        /* 工具轮次用尽但模型仍想调用工具：不再放行工具，追加一条
         * 总结提示并进入最终轮，保证用户拿到基于已获取信息的完整回答
         * （此前直接采纳该过渡响应，用户只能看到一行半截文本）。 */
        if (has_tools && !force_summary) {
            force_summary = 1;
            cli_msgbuf_push(&buf, "user",
                            "（工具调用轮次已用尽。请仅基于以上已获取的信息给出最终回答，"
                            "不要再调用任何工具。）",
                            NULL, NULL, NULL);
            llm_response_free(resp);
            continue;
        }
        final_resp = resp;
        break;
    }

    if (spinner_on)
        cli_spinner_stop(1, NULL);

    /* 2.2.4 对话记忆写入：一轮对话完成且有回复时落盘（用户输入+回复+
     * 思考链，供下轮/下次会话检索注入；2.1.1.6 起携带 reasoning）。 */
    if (final_resp && final_resp->choice_count > 0 && final_resp->choices[0].content) {
        const char *mem_reasoning =
            (final_resp->choices[0].reasoning_content && final_resp->choices[0].reasoning_content[0])
                ? final_resp->choices[0].reasoning_content
                : (g_chat_reasoning_acc ? g_chat_reasoning_acc : NULL);
        cli_chat_mem_record(input, final_resp->choices[0].content, mem_reasoning);
    }
#else
    /* 无 cJSON 平台：固定消息数组 + 单轮非流式（不带工具），保持纯对话。
     * 消息数 = 历史 + 2（首 system + 当前 user）+ 1（语言约束 system，可能缺省）。 */
    int stream_mode = 0;
    int tool_rounds = 0;
    size_t msg_n = g_history_count + 3;
    llm_message_t *msgs = (llm_message_t *)AIRY_CALLOC(msg_n, sizeof(llm_message_t));
    if (!msgs) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "对话",
                             "内存不足，无法生成回复。");
        return;
    }
    size_t mi = 0;
    msgs[mi].role = "system";
    msgs[mi].content = cli_system_prompt_now();
    mi++;
    /* 1.3 推理语言网关：语言约束 System Prompt 注入（非 cJSON 平台） */
    if (g_cli_lang_sys_prompt && g_cli_lang_sys_prompt[0] && mi < msg_n) {
        msgs[mi].role = "system";
        msgs[mi].content = g_cli_lang_sys_prompt;
        mi++;
    }
    for (size_t hi = 0; hi < g_history_count; hi++) {
        msgs[mi].role = g_history_roles[hi];
        msgs[mi].content = g_history_contents[hi];
        msgs[mi].reasoning_content = g_history_reasonings[hi];
        mi++;
    }
    msgs[mi].role = "user";
    msgs[mi].content = input;
    mi++;

    int spinner_on = !g_cli_print_mode && !g_cli_json_mode;
    if (spinner_on) {
        char think_title[128];
        /* 2.3.14：思考角色细分——t1-f 思考中显示 [Dual Fast Think] */
        snprintf(think_title, sizeof(think_title), "%s (%s)",
                 cli_render_actor_name(cli_chat_think_actor()),
                 t1f_model ? t1f_model : "default");
        cli_spinner_start(think_title);
    }

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    cfg.model = t1f_model;
    cfg.messages = msgs;
    cfg.message_count = msg_n;
    cfg.temperature = 0.7f;
    cfg.max_tokens = 2048;

    llm_response_t *final_resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &final_resp);
    if (ret != 0 || !final_resp || final_resp->choice_count == 0) {
        if (final_resp)
            llm_response_free(final_resp);
        if (spinner_on)
            cli_spinner_stop(0, "reply failed");
        char line[128];
        snprintf(line, sizeof(line), "回复失败：%s", cli_chat_err_desc(ret));
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "对话", line);
        AIRY_FREE(msgs);
        return;
    }
    /* 2.1.1.5/2.1.1.6：累计本轮真实 token/费用与思考链（无 cJSON 平台） */
    cli_chat_usage_add(final_resp);
    if (final_resp->choices[0].reasoning_content)
        cli_chat_reasoning_add(final_resp->choices[0].reasoning_content);
    if (spinner_on)
        cli_spinner_stop(1, NULL);
#endif /* AIRY_HAS_CJSON */

    const char *final_content = (final_resp->choices && final_resp->choice_count > 0 &&
                                 final_resp->choices[0].content)
                                    ? final_resp->choices[0].content
                                    : "";

    /* 1.3 推理语言网关：输出后处理（语言漂移检测 + 术语一致性 + 润色）。
     * 期望输出语言取路由决策 output_lang；仅作用于渲染与历史写入，
     * 不修改 llm_d 原始响应（推理链条证据保留）。 */
    char *lg_final = NULL;
    const char *render_content = final_content;
    if (g_cli_lang_gateway && final_content[0]) {
        if (airy_lang_gateway_post_process(g_cli_lang_gateway, final_content,
                                           g_cli_lang_output,
                                           &lg_final) == AIRY_EOK &&
            lg_final && lg_final[0])
            render_content = lg_final;
    }

    /* 最终回复渲染：
     *   --json  结构化 JSON（Codex exec 约定）
     *   -p      纯文本（Claude Code -p / Codex exec 约定；流式已直出）
     *   交互    TTY 流式：擦除预览后 markdown 精修 / 长回复折叠；
     *           TUI/非流式：markdown 渲染 + 折叠区（浏览展开） */
    if (g_cli_json_mode) {
#ifdef AIRY_HAS_CJSON
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "role", "super_agent");
        cJSON_AddStringToObject(root, "type", "chat");
        if (final_content[0])
            cJSON_AddStringToObject(root, "content", render_content);
        else
            cJSON_AddStringToObject(root, "error", "reply failed");
        /* 2.1.1.6：--json 结构化输出携带思考链（TUI RunResponse.thinking
         * 已有先例），思考 token 不随 JSON 输出丢失。 */
        const char *json_reasoning = (final_resp->choices && final_resp->choice_count > 0 &&
                                      final_resp->choices[0].reasoning_content)
                                         ? final_resp->choices[0].reasoning_content
                                         : (g_chat_reasoning_acc ? g_chat_reasoning_acc : "");
        if (json_reasoning && json_reasoning[0])
            cJSON_AddStringToObject(root, "reasoning", json_reasoning);
        if (final_resp) {
            cJSON *usage = cJSON_CreateObject();
            cJSON_AddNumberToObject(usage, "prompt_tokens", final_resp->prompt_tokens);
            cJSON_AddNumberToObject(usage, "completion_tokens", final_resp->completion_tokens);
            cJSON_AddNumberToObject(usage, "total_tokens", final_resp->total_tokens);
            cJSON_AddNumberToObject(usage, "reasoning_tokens", final_resp->reasoning_tokens);
            cJSON_AddNumberToObject(usage, "cost_usd", final_resp->cost_usd);
            cJSON_AddItemToObject(root, "usage", usage);
        }
        char *js = cJSON_PrintUnformatted(root);
        if (js) {
            cli_outf("%s\n", js);
            cJSON_free(js);
        }
        cJSON_Delete(root);
#else
        cli_outf("{\"role\":\"super_agent\",\"type\":\"chat\"");
        if (final_content[0])
            cli_outf(",\"content\":\"%s\"", render_content);
        else
            cli_outf(",\"error\":\"reply failed\"");
        cli_outf("}\n");
#endif /* AIRY_HAS_CJSON */
    } else if (g_cli_print_mode) {
        /* 流式模式：final_content 已随块实时直出，不再重复打印。
         * 空返回诊断（2026-08-17）：流式未输出任何字节且模型无文本
         * 回复（thinking 模型可能只产生 reasoning_content）→ stderr
         * 明确告警，stdout 保持空串可解析（脚本不被打断）。 */
        if (!stream_mode)
            cli_outf("%s\n", render_content);
        else if (final_content[0] == '\0' && g_chat_fold_phys == 0)
            fprintf(stderr,
                    "[chat] warning: empty reply (model returned no text; "
                    "reasoning-only or provider error)\n");
    } else {
        cli_tui_t *r_tui = cli_tui_get_default();
        (void)r_tui;
        if (stream_mode) {
            /* 交互 TTY 流式：擦除打字机预览，重绘最终形态。
             * 上移行数：末尾无换行（光标在最后一行行尾）时 = phys-1，
             * 否则 = phys；\r 回行首再 \033[J 清屏（CUU 只移行不移列，
             * 直接清会残留列尾内容）。擦除前强制 flush：预览/进度行
             * 全部落盘后再移动光标，避免 stdio 缓冲重排造成擦除错位。 */
            if (g_chat_fold_phys > 0 && cli_term_is_tty()) {
                fflush(stdout);
                size_t up = g_chat_fold_phys;
                if (g_chat_fold_tail_no_nl && up > 0)
                    up -= 1;
                /* CUU 参数 0 = 默认值 1（ANSI），up=0 时避免 `\033[0A`
                 * 误删上一行，改用回车 + 清当前行。 */
                char erase[32];
                int en;
                if (up > 0)
                    en = snprintf(erase, sizeof(erase), "\033[%zuA\r\033[J", up);
                else
                    en = snprintf(erase, sizeof(erase), "\r\033[2K");
                if (en > 0)
                    fwrite(erase, 1, (size_t)en, stdout);
                fflush(stdout);
            }
            /* 2.3.5/2.3.14：thinking 模型的思考过程以 [Dual Think] 折叠
             * 呈现（前几行 + 折叠尾），避免碎片刷屏，又让用户看到模型
             * 确实在思考；浏览/日志可看全量。 */
            if (final_resp->choices && final_resp->choice_count > 0 &&
                final_resp->choices[0].reasoning_content &&
                final_resp->choices[0].reasoning_content[0]) {
                cli_render_role_line(CLI_ROLE_DUAL_THINK, cli_chat_think_actor(),
                                     "思考", NULL);
                cli_render_collapsed(final_resp->choices[0].reasoning_content,
                                     4, CLI_REPLY_FOLD_KEEP, 1);
            }
            if (final_content[0] != '\0') {
                /* 结果完整渲染，不折叠（2026-08-19：仅折叠思考链，
                 * 结果必须完整展示；长结果靠终端滚动/TUI 视口浏览）。 */
                cli_render_super_agent(render_content);
            } else {
                cli_render_super_agent(CLI_REPLY_EMPTY_HINT);
            }
        } else {
            /* TUI / 非流式交互：思考链折叠展示（进历史）；结果完整渲染
             * 进历史（用户要求结果不折叠，长结果经视口滚动浏览）。 */
            if (final_resp->choices && final_resp->choice_count > 0 &&
                final_resp->choices[0].reasoning_content &&
                final_resp->choices[0].reasoning_content[0]) {
                cli_render_role_line(CLI_ROLE_DUAL_THINK, cli_chat_think_actor(),
                                     "思考", NULL);
                cli_render_collapsed(final_resp->choices[0].reasoning_content,
                                     4, CLI_REPLY_FOLD_KEEP, 1);
            }
            if (final_content[0] != '\0') {
                cli_render_super_agent(render_content);
            } else {
                cli_render_super_agent(CLI_REPLY_EMPTY_HINT);
            }
        }
    }

    /* 2.1.1.6：思考链全量保留——历史携带 reasoning（跨轮回传 DeepSeek
     * 续轮规范）+ 独立日志落盘（所有模式），折叠展示之外的完整文本不丢失。 */
    const char *round_reasoning = (final_resp->choices && final_resp->choice_count > 0 &&
                                   final_resp->choices[0].reasoning_content)
                                      ? final_resp->choices[0].reasoning_content
                                      : (g_chat_reasoning_acc ? g_chat_reasoning_acc : "");
    cli_history_add("user", input, NULL);
    cli_history_add("assistant", render_content, round_reasoning);
    cli_chat_reasoning_persist(round_reasoning);
    /* -p 模式保持既有 stderr trace 通道（供脚本消费进度） */
    if (g_chat_reasoning_acc && g_chat_reasoning_acc[0])
        cli_trace("reasoning", "%s", g_chat_reasoning_acc);
    AIRY_FREE(g_chat_reasoning_acc);
    g_chat_reasoning_acc = NULL;
    cli_trace("chat", "%s done rounds=%d bytes=%zu", CLI_ICON_CHECK, tool_rounds,
              strlen(final_content));

    llm_response_free(final_resp);
    AIRY_FREE(lg_final);
#ifdef AIRY_HAS_CJSON
    cli_msgbuf_free(&buf);
#else
    AIRY_FREE(msgs);
#endif
}
