// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_chat_classify.c
 * @brief 聊天域意图分辨（域拆分自 cli_chat.c，2026-08-27）。
 *
 * 用户输入是【任务指令】还是【普通对话】：启发式词表（cli_classify.c，
 * 2.5.x 纯函数）优先，未命中时 LLM 兜底判定；失败安全回退到对话集合。
 * 共享声明见 cli_chat_internal.h。
 */

#include "cli_chat_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_CLASSIFY_PROMPT                                                    \
    "判断用户输入属于【任务指令】还是【普通对话】。任务指令：要求执行具体工程" \
    "任务（实现、开发、构建、创建、编写、修复、重构、部署等），需要改动代码、" \
    "文件或系统；普通对话：寒暄、提问、解释、闲聊、搜索/查询/了解实时信息（如" \
    "新闻、天气、资料、概念解释）、读取/查看/编辑单个本地文件（读文件、列目录、" \
    "改文件内容等日常操作），只需回答或联网检索或调用文件工具即可完成，无需"   \
    "进入任务调度管线。只输出 JSON：{\"type\":\"task\"} 或 {\"type\":\"chat\"}"

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
