// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_exec_review.c
 * @brief CLI execution-review semantic delegates (t2 / t1-f).
 *
 * Policy side of the improvement-6 review pipeline. The mechanism (gate ->
 * t2 -> t1-f orchestration + degradation chain) lives in
 * execution_review.c; here we supply the LLM judgment: t2 decides whether
 * the execution artifact deviates from the blueprint node contract (DRIFT),
 * t1-f makes the contextual final accept/reject call. Both go through the
 * shared chat adapter (g_chat_adapter, llm_d). Every failure path returns
 * -1 so the pipeline degrades deterministically (gate-only / adopt t2).
 */

#include "cli_exec_review.h"

#include "cli_internal.h"
#include "cli_render.h"
#include "llm_svc_adapter.h"
#include "logging.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <string.h>

#define CLI_EXEC_REVIEW_MAX_TOKENS 128
#define CLI_EXEC_REVIEW_PROMPT_MAX 4096
#define CLI_EXEC_REVIEW_REASON_MAX 128

/* t2 (A) semantic review: artifact vs blueprint node contract. */
static const char CLI_EXEC_T2_SYSTEM[] =
    "你是 AgentRT 执行中复核的 t2 语义审查器。判断执行产物 output_json 是否满足"
    "蓝图节点的目标 node_goal。注意：output_signatures 中形如 artifact:xxx 的键"
    "是 GRAD 因果引用签名（描述图上下游因果链接），不是产物必须包含的内容键，"
    "不要据此判定偏离；请以 node_goal 达成度为主要判据。只输出一行 JSON："
    "{\"drift\":0或1,\"reason\":\"不超过60字\"}。drift=1 表示产物偏离目标或存在"
    "明显缺陷，drift=0 表示产物满足目标。";

/* t1-f (B) final adjudication: combine gate result and t2 drift. */
static const char CLI_EXEC_T1F_SYSTEM[] =
    "你是 AgentRT 执行中复核的 t1-f 终裁者。综合确定性门禁结果 gate_reason 与 t2 "
    "语义偏离结论 drift，对执行产物 output_json 做出最终接受/拒绝决定。"
    "只输出一行 JSON：{\"accept\":0或1,\"reason\":\"不超过60字\"}。"
    "accept=0 表示拒绝（产物不合格，需要重做或人工介入），accept=1 表示接受。";

/* Extract the value of "key":"..." from a JSON-ish string into buf
 * (lenient: works even when a thinking model prefixes reasoning text;
 * JSON escapes are copied verbatim). Returns 1 on match, 0 otherwise. */
static int cli_exec_json_str(const char *json, const char *key, char *buf, size_t sz)
{
    if (!json || !key || !buf || sz == 0)
        return 0;
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\":\"", key);
    const char *p = strstr(json, pat);
    if (!p)
        return 0;
    p += strlen(pat);
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < sz) {
        buf[o++] = *p++;
    }
    buf[o] = '\0';
    return o > 0;
}

static int cli_exec_t2_review(const airy_review_input_t *in, const char *fact_base_json,
                              char *reason_out, size_t reason_sz, void *user_data)
{
    (void)fact_base_json;
    (void)user_data;
    if (!g_chat_adapter || !in || !in->output_json)
        return -1;

    /* 只把真实内容契约键交给 LLM（artifact:* 引用签名过滤掉） */
    char sigbuf[256] = "(无)";
    if (in->output_signatures && in->output_signature_count > 0) {
        size_t o = 0;
        for (size_t i = 0; i < in->output_signature_count && o < sizeof(sigbuf) - 2; i++) {
            const char *s = in->output_signatures[i];
            if (!s || !s[0] || strncmp(s, "artifact:", 9) == 0)
                continue;
            o += (size_t)snprintf(sigbuf + o, sizeof(sigbuf) - o, "%s%s", o ? "," : "", s);
        }
        if (o == 0)
            snprintf(sigbuf, sizeof(sigbuf), "(无)");
    }

    char user[CLI_EXEC_REVIEW_PROMPT_MAX];
    snprintf(user, sizeof(user),
             "node_goal: %s\noutput_signatures: %s\noutput_json: %s",
             in->node_goal ? in->node_goal : "(无)", sigbuf, in->output_json);

    llm_message_t msgs[2];
    __builtin_memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = "system";
    msgs[0].content = CLI_EXEC_T2_SYSTEM;
    msgs[1].role = "user";
    msgs[1].content = user;

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    /* t2 语义复核属 A 角色（生成器/复核），按 GRAD 三模型命名取
     * AIRY_MODEL_T2（2026-08-17 审计修复：此前误用 T1F，仅配 T2 时
     * 复核会回落 provider 默认，角色分离失效）；t1-f 终裁仍取 T1F。
     * 2026-08-17 审计修复：T2 未配置时显式返回 -1 走机制层降级
     * （gate-only），而非静默回落 provider 默认——否则与主生成同模型
     * 自审自签，双思考独立性失效且 n_degraded 不递增。 */
    /* t2x/t1fx/t1px 必须为函数作用域：若在 if 块内声明，t2m 指向的
     * 栈数组随块结束失效，后台线程（cli_task_wait_worker）中
     * llm_svc_adapter_complete 读取 cfg.model 触发 ASan
     * stack-use-after-scope（2026-08-22 冒烟实测复现）。 */
    char t2x[128], t1fx[128], t1px[128];
    const char *t2m = getenv("AIRY_MODEL_T2");
    if (!t2m || !t2m[0]) {
        /* 2026-08-19：模型配置统一自 cli_think_cfg_*（env > model.yaml
         * think 段 > llm.model 默认）。复核须显式配置（explicit，不
         * 回填默认）——llm.model 默认与主生成同模型，采用即自审自签。 */
        if (cli_think_cfg_explicit(t2x, sizeof(t2x), t1fx, sizeof(t1fx),
                                   t1px, sizeof(t1px)) && t2x[0]) {
            t2m = t2x;
        } else {
            cli_trace("review", "exec t2: AIRY_MODEL_T2 not set, degrade to gate-only");
            return -1;
        }
    }
    cfg.model = t2m;
    cfg.messages = msgs;
    cfg.message_count = 2;
    cfg.temperature = 0.0f;
    cfg.max_tokens = CLI_EXEC_REVIEW_MAX_TOKENS;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
    if (ret != 0 || !resp || resp->choice_count == 0 || !resp->choices[0].content) {
        if (resp)
            llm_response_free(resp);
        cli_trace("review", "exec t2: llm unavailable, degrade to gate-only");
        return -1;
    }
    const char *content = resp->choices[0].content;
    int rc = -1;
    if (strstr(content, "\"drift\":1") || strstr(content, "\"drift\": 1"))
        rc = 1;
    else if (strstr(content, "\"drift\":0") || strstr(content, "\"drift\": 0"))
        rc = 0;
    if (rc == 1 && reason_out && reason_sz > 0) {
        char rb[CLI_EXEC_REVIEW_REASON_MAX];
        if (cli_exec_json_str(content, "reason", rb, sizeof(rb)))
            snprintf(reason_out, reason_sz, "%s", rb);
    }
    llm_response_free(resp);
    return rc;
}

static int cli_exec_t1f_adjudicate(const airy_review_input_t *in, int drift,
                                   const char *gate_reason, char *verdict_out,
                                   size_t verdict_sz, void *user_data)
{
    (void)user_data;
    if (!g_chat_adapter || !in)
        return -1;

    char user[CLI_EXEC_REVIEW_PROMPT_MAX];
    snprintf(user, sizeof(user),
             "gate_reason: %s\ndrift: %d\nnode_goal: %s\noutput_json: %s",
             (gate_reason && gate_reason[0]) ? gate_reason : "gate ok", drift,
             in->node_goal ? in->node_goal : "(无)",
             in->output_json ? in->output_json : "(无)");

    llm_message_t msgs[2];
    __builtin_memset(&msgs, 0, sizeof(msgs));
    msgs[0].role = "system";
    msgs[0].content = CLI_EXEC_T1F_SYSTEM;
    msgs[1].role = "user";
    msgs[1].content = user;

    llm_request_config_t cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    /* 2026-08-17 审计修复：T1F 未配置时显式返回 -1，由机制层走既定降级
     * （adopt t2 verdict）——而非静默回落 provider 默认，否则终裁与
     * 主生成同模型自我终裁（自审自签），双思考独立性失效。 */
    /* 同 t2：t1fx 必须函数作用域，避免栈数组随 if 块失效（ASan
     * stack-use-after-scope，2026-08-22 冒烟实测复现）。 */
    char t2x[128], t1fx[128], t1px[128];
    const char *t1fm = getenv("AIRY_MODEL_T1F");
    if (!t1fm || !t1fm[0]) {
        /* 同 t2：终裁须显式配置（explicit），不回填 llm.model 默认 */
        if (cli_think_cfg_explicit(t2x, sizeof(t2x), t1fx, sizeof(t1fx),
                                   t1px, sizeof(t1px)) && t1fx[0]) {
            t1fm = t1fx;
        } else {
            cli_trace("review", "exec t1-f: AIRY_MODEL_T1F not set, adopt t2 verdict");
            return -1;
        }
    }
    cfg.model = t1fm;
    cfg.messages = msgs;
    cfg.message_count = 2;
    cfg.temperature = 0.0f;
    cfg.max_tokens = CLI_EXEC_REVIEW_MAX_TOKENS;

    llm_response_t *resp = NULL;
    int ret = llm_svc_adapter_complete(g_chat_adapter, &cfg, &resp);
    if (ret != 0 || !resp || resp->choice_count == 0 || !resp->choices[0].content) {
        if (resp)
            llm_response_free(resp);
        cli_trace("review", "exec t1-f: llm unavailable, adopt t2 verdict");
        return -1;
    }
    const char *content = resp->choices[0].content;
    int rc = -1;
    if (strstr(content, "\"accept\":0") || strstr(content, "\"accept\": 0"))
        rc = 1; /* reject */
    else if (strstr(content, "\"accept\":1") || strstr(content, "\"accept\": 1"))
        rc = 0; /* accept */
    /* 无论 accept/reject，都把 LLM 的终裁理由带回（verify 事件信息完整） */
    if (rc >= 0 && verdict_out && verdict_sz > 0) {
        char rb[CLI_EXEC_REVIEW_REASON_MAX];
        if (cli_exec_json_str(content, "reason", rb, sizeof(rb)) && rb[0])
            snprintf(verdict_out, verdict_sz, "%s", rb);
    }
    llm_response_free(resp);
    return rc;
}

airy_execution_review_t *cli_exec_review_create(void)
{
    airy_execution_review_t *rv = airy_execution_review_create(NULL);
    if (!rv)
        return NULL;

    airy_review_semantic_ops_t ops;
    __builtin_memset(&ops, 0, sizeof(ops));
    ops.t2_review = cli_exec_t2_review;
    ops.t1f_adjudicate = cli_exec_t1f_adjudicate;
    ops.user_data = NULL;
    airy_execution_review_set_semantic_ops(rv, &ops);
    return rv;
}
