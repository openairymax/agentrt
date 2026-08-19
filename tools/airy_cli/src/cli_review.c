// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file cli_review.c
 * @brief Cognition-stage parallel sub-agent review (item 4).
 *
 * Reviewer sub-agents (fact/risk + optional boundary/coverage on rich hosts)
 * run concurrently on separate threads; each spawns through agent_d and
 * invokes once with the task text + plan summary, returning a short JSON
 * verdict. The number of reviewers is probed from the host hardware
 * (cli_review_parallelism: 4 on rich, 2 mid-range, 1 constrained). The main
 * thread joins all and merges the outputs into one report JSON.
 *
 * Design notes:
 *  - Parallelism via airy_thread_create (cross-platform), never nested: the
 *    worker only performs agent.spawn/invoke RPCs.
 *  - The reviewer prompt asks for a strict JSON verdict, so the report stays
 *    small and machine-mergeable.
 *  - Every failure path degrades silently (return 0); the pipeline must not
 *    depend on the review stage.
 */

// @owner: team-B
#include "cli_review.h"

#include "cli_render.h"
#include "daemon_rpc_client.h"
#include "airy_memory.h"
#include "logging.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "platform.h"

/* Review topics are probed from the host (hardware-aware parallelism), so
 * the worker array is sized dynamically; these bounds keep memory bounded
 * even on many-core servers. */
#define CLI_REVIEW_MAX_TOPICS 4
#define CLI_REVIEW_DEFAULT_TOPICS 2
#define CLI_REVIEW_HEALTH_TIMEOUT_MS 3000
#define CLI_REVIEW_SPAWN_TIMEOUT_MS 90000
#define CLI_REVIEW_INVOKE_TIMEOUT_MS 180000
#define CLI_REVIEW_PROMPT_MAX 4096
#define CLI_REVIEW_SPEC_MAX 256

/* Topic table: each reviewer sub-agent audits one aspect of the plan.
 * Hardware-rich hosts enable all four; constrained hosts fall back to the
 * first ones only (order matters: fact/risk are the mandatory core). */
typedef struct {
    const char *name;
    const char *brief;
} cli_review_topic_t;

static const cli_review_topic_t cli_review_topics[CLI_REVIEW_MAX_TOPICS] = {
    {"fact", "审查计划的完整性与目标覆盖度"},
    {"risk", "审查计划的风险、边界与依赖正确性"},
    {"boundary", "审查计划的边界条件与资源约束"},
    {"coverage", "审查计划的验收标准与结果可验证性"},
};

/* Decide how many reviewer sub-agents to dispatch based on the host CPU and
 * memory. 4 on rich hosts, 2 on mid-range hosts, 1 (serial) on constrained
 * hosts; a probe failure degrades to the previous default of 2. */
static int cli_review_parallelism(void)
{
    airy_sysinfo_t si;
    if (airy_get_sysinfo(&si) != 0)
        return CLI_REVIEW_DEFAULT_TOPICS;
    const uint64_t gb = 1024ULL * 1024 * 1024;
    if (si.cpu_count >= 8 && si.memory_total >= 8 * gb)
        return 4;
    if (si.cpu_count >= 4 || si.memory_total >= 4 * gb)
        return 2;
    return 1;
}

typedef struct {
    const char *sock;
    const char *topic;
    const char *prompt; /* OWNER (built by caller) */
    char *output;       /* OWNER (filled by worker) */
} cli_review_job_t;

/* Resolve agent_d socket: AIRY_AGENT_SOCK -> $AIRY_HOME/run/agent.sock ->
 * agent.sock (same origin as the daemon commands). */
static const char *cli_review_agent_sock(const char *sock)
{
    if (sock && sock[0])
        return sock;
    static char buf[512];
    const char *env = getenv("AIRY_AGENT_SOCK");
    if (env && env[0]) {
        snprintf(buf, sizeof(buf), "%s", env);
        return buf;
    }
    const char *home = getenv("AIRY_HOME");
    if (home && home[0]) {
        snprintf(buf, sizeof(buf), "%s/run/agent.sock", home);
        return buf;
    }
    const char *uhome = getenv("HOME");
    if (uhome && uhome[0]) {
        snprintf(buf, sizeof(buf), "%s/.airymaxrt/run/agent.sock", uhome);
        return buf;
    }
    snprintf(buf, sizeof(buf), "%s", "agent.sock");
    return buf;
}

/* Compact plan summary JSON (OWNER, caller AIRY_FREE). */
static char *cli_review_plan_summary(const airy_task_plan_t *plan)
{
    cJSON *o = cJSON_CreateObject();
    if (!o)
        return NULL;
    cJSON_AddStringToObject(o, "plan_id", plan->task_plan_id ? plan->task_plan_id : "");
    cJSON *nodes = cJSON_CreateArray();
    if (nodes) {
        for (size_t i = 0; i < plan->task_plan_node_count; i++) {
            const airy_task_node_t *n = plan->task_plan_nodes[i];
            if (!n)
                continue;
            cJSON *no = cJSON_CreateObject();
            if (!no)
                continue;
            cJSON_AddStringToObject(no, "id", n->task_node_id ? n->task_node_id : "");
            cJSON_AddStringToObject(no, "goal", n->task_node_goal ? n->task_node_goal : "");
            cJSON_AddStringToObject(no, "handler",
                                    (n->task_node_handler_name && n->task_node_handler_name[0])
                                        ? n->task_node_handler_name
                                        : (n->task_node_agent_role ? n->task_node_agent_role : ""));
            cJSON_AddItemToArray(nodes, no);
        }
        cJSON_AddItemToObject(o, "nodes", nodes);
    }
    char *s = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    return s;
}

/* Build the reviewer sub-agent input prompt (OWNER, caller AIRY_FREE). */
static char *cli_review_build_prompt(const char *topic, const char *task, const char *plan_json)
{
    const char *brief = "";
    for (size_t i = 0; i < CLI_REVIEW_MAX_TOPICS; i++) {
        if (strcmp(topic, cli_review_topics[i].name) == 0) {
            brief = cli_review_topics[i].brief;
            break;
        }
    }
    char *prompt = (char *)AIRY_MALLOC(CLI_REVIEW_PROMPT_MAX + 1);
    if (!prompt)
        return NULL;
    snprintf(prompt, CLI_REVIEW_PROMPT_MAX,
             "你是 AgentRT 认知阶段的%s审查子 agent。%s。\n"
             "任务：%s\n计划：%s\n"
             "请只输出一行 JSON：{\"verdict\":\"pass\"或\"flag\",\"reason\":\"不超过50字\","
             "\"suggestion\":\"不超过50字\"}",
             topic, brief, task ? task : "", plan_json ? plan_json : "");
    return prompt;
}

/* Extract "<field>" (or "result.<field>") from a daemon RPC response; OWNER
 * via AIRY_STRDUP. daemon_rpc_call returns the serialized result object
 * directly (no JSON-RPC "result" wrapper); keep the wrapped form as a
 * fallback for callers that pass the raw response. */
static char *cli_review_rpc_field(const char *resp, const char *field)
{
    if (!resp)
        return NULL;
    cJSON *root = cJSON_Parse(resp);
    if (!root)
        return NULL;
    const char *val = NULL;
    cJSON *f = cJSON_GetObjectItem(root, field);
    if (cJSON_IsString(f) && f->valuestring) {
        val = f->valuestring;
    } else {
        cJSON *result = cJSON_GetObjectItem(root, "result");
        if (result) {
            f = cJSON_GetObjectItem(result, field);
            if (cJSON_IsString(f) && f->valuestring)
                val = f->valuestring;
        }
    }
    char *out = val ? AIRY_STRDUP(val) : NULL;
    cJSON_Delete(root);
    return out;
}

/* Worker: spawn a reviewer agent and invoke it once with the prompt. */
static void *cli_review_worker(void *arg)
{
    cli_review_job_t *job = (cli_review_job_t *)arg;
    if (!job || !job->sock || !job->sock[0] || !job->prompt)
        return NULL;

    char spec[CLI_REVIEW_SPEC_MAX];
    snprintf(spec, sizeof(spec), "{\"agent_spec\":{\"role\":\"reviewer\",\"topic\":\"%s\"}}",
             job->topic);

    char *resp = NULL;
    int rc = daemon_rpc_call(job->sock, "spawn", spec, &resp, CLI_REVIEW_SPAWN_TIMEOUT_MS);
    if (rc != 0 || !resp) {
        AIRY_FREE(resp);
        return NULL;
    }
    char *agent_id = cli_review_rpc_field(resp, "agent_id");
    AIRY_FREE(resp);
    if (!agent_id)
        return NULL;

    cJSON *params = cJSON_CreateObject();
    if (!params) {
        AIRY_FREE(agent_id);
        return NULL;
    }
    cJSON_AddStringToObject(params, "agent_id", agent_id);
    cJSON_AddStringToObject(params, "input", job->prompt);
    char *params_str = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    AIRY_FREE(agent_id);
    if (!params_str)
        return NULL;

    resp = NULL;
    rc = daemon_rpc_call(job->sock, "invoke", params_str, &resp, CLI_REVIEW_INVOKE_TIMEOUT_MS);
    AIRY_FREE(params_str);
    if (rc == 0 && resp)
        job->output = cli_review_rpc_field(resp, "output");
    if (job->output && job->output[0])
        cli_trace("review", "sub-agent[%s] verdict: %.*s", job->topic, (int)cli_utf8_safe_len(job->output, 180), job->output);
    AIRY_FREE(resp);
    return NULL;
}

int cli_cognition_review(const char *agent_sock, const char *task, const airy_task_plan_t *plan,
                         char **out_report)
{
    if (out_report)
        *out_report = NULL;

    const char *env = getenv("AIRY_COGNITION_REVIEW");
    if (env && env[0] == '0')
        return 0;
    if (!plan)
        return 0;

    const char *sock = cli_review_agent_sock(agent_sock);
    if (!sock[0])
        return 0;

    /* agent_d must be reachable; a failed probe degrades silently. */
    char *hc = NULL;
    int hc_rc = daemon_rpc_call(sock, "health_check", NULL, &hc, CLI_REVIEW_HEALTH_TIMEOUT_MS);
    if (hc_rc != 0) {
        AIRY_FREE(hc);
        return 0;
    }
    AIRY_FREE(hc);

    char *plan_json = cli_review_plan_summary(plan);
    if (!plan_json)
        return 0;

    const int n = cli_review_parallelism();
    if (n <= 0 || n > CLI_REVIEW_MAX_TOPICS)
        return 0;

    cli_review_job_t *jobs = (cli_review_job_t *)AIRY_CALLOC((size_t)n, sizeof(cli_review_job_t));
    airy_thread_t *threads = (airy_thread_t *)AIRY_CALLOC((size_t)n, sizeof(airy_thread_t));
    if (!jobs || !threads) {
        AIRY_FREE(jobs);
        AIRY_FREE(threads);
        AIRY_FREE(plan_json);
        return 0;
    }
    cli_trace("review", "parallel cognition review (%d sub-agents) started, plan=%s nodes=%zu",
              n, plan->task_plan_id ? plan->task_plan_id : "?", plan->task_plan_node_count);

    for (int i = 0; i < n; i++) {
        threads[i] = AIRY_INVALID_THREAD;
        jobs[i].sock = sock;
        jobs[i].topic = cli_review_topics[i].name;
        jobs[i].prompt = cli_review_build_prompt(cli_review_topics[i].name, task, plan_json);
        if (jobs[i].prompt)
            airy_platform_thread_create(&threads[i], cli_review_worker, &jobs[i]);
    }

    for (int i = 0; i < n; i++) {
        if (threads[i] != AIRY_INVALID_THREAD)
            airy_platform_thread_join(threads[i], NULL);
        AIRY_FREE(jobs[i].prompt);
    }
    AIRY_FREE(plan_json);

    int produced = 0;
    for (int i = 0; i < n; i++) {
        if (jobs[i].output)
            produced++;
    }
    if (produced == 0) {
        AIRY_FREE(jobs);
        AIRY_FREE(threads);
        return 0;
    }

    cJSON *rep = cJSON_CreateObject();
    if (!rep) {
        for (int i = 0; i < n; i++)
            AIRY_FREE(jobs[i].output);
        AIRY_FREE(jobs);
        AIRY_FREE(threads);
        return 0;
    }
    cJSON_AddStringToObject(rep, "reviewer", "cognition-sub-agents");
    cJSON *items = cJSON_CreateArray();
    if (items) {
        for (int i = 0; i < n; i++) {
            if (!jobs[i].output)
                continue;
            cJSON *it = cJSON_CreateObject();
            if (!it)
                continue;
            cJSON_AddStringToObject(it, "topic", cli_review_topics[i].name);
            cJSON_AddStringToObject(it, "output", jobs[i].output);
            cJSON_AddItemToArray(items, it);
        }
        cJSON_AddItemToObject(rep, "reviews", items);
    }
    char *report = cJSON_PrintUnformatted(rep);
    cJSON_Delete(rep);

    for (int i = 0; i < n; i++)
        AIRY_FREE(jobs[i].output);
    AIRY_FREE(jobs);
    AIRY_FREE(threads);

    if (!report)
        return 0;
    if (out_report) {
        *out_report = report;
    } else {
        AIRY_FREE(report);
    }
    return 1;
}
