// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/*
 * @file cli_review.c
 * @brief Cognition-stage parallel sub-agent review (item 4).
 *
 * Two reviewer sub-agents (fact/coverage + risk/boundary) run concurrently on
 * separate threads; each spawns through agent_d and invokes once with the
 * task text + plan summary, returning a short JSON verdict. The main thread
 * joins both and merges the outputs into one report JSON.
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

#define CLI_REVIEW_TOPIC_COUNT 2
#define CLI_REVIEW_HEALTH_TIMEOUT_MS 3000
#define CLI_REVIEW_SPAWN_TIMEOUT_MS 90000
#define CLI_REVIEW_INVOKE_TIMEOUT_MS 180000
#define CLI_REVIEW_PROMPT_MAX 4096
#define CLI_REVIEW_SPEC_MAX 256

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
    char *prompt = (char *)AIRY_MALLOC(CLI_REVIEW_PROMPT_MAX + 1);
    if (!prompt)
        return NULL;
    const char *brief = (strcmp(topic, "fact") == 0) ?
                            "审查计划的完整性与目标覆盖度" :
                            "审查计划的风险、边界与依赖正确性";
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
        cli_trace("review", "sub-agent[%s] verdict: %.180s", job->topic, job->output);
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
    cli_trace("review", "parallel cognition review (fact+risk) started, plan=%s nodes=%zu",
              plan->task_plan_id ? plan->task_plan_id : "?", plan->task_plan_node_count);

    static const char *topics[CLI_REVIEW_TOPIC_COUNT] = {"fact", "risk"};
    cli_review_job_t jobs[CLI_REVIEW_TOPIC_COUNT];
    airy_thread_t threads[CLI_REVIEW_TOPIC_COUNT];
    AIRY_MEMSET(jobs, 0, sizeof(jobs));
    for (int i = 0; i < CLI_REVIEW_TOPIC_COUNT; i++) {
        threads[i] = AIRY_INVALID_THREAD;
        jobs[i].sock = sock;
        jobs[i].topic = topics[i];
        jobs[i].prompt = cli_review_build_prompt(topics[i], task, plan_json);
        if (jobs[i].prompt)
            airy_platform_thread_create(&threads[i], cli_review_worker, &jobs[i]);
    }

    for (int i = 0; i < CLI_REVIEW_TOPIC_COUNT; i++) {
        if (threads[i] != AIRY_INVALID_THREAD)
            airy_platform_thread_join(threads[i], NULL);
        AIRY_FREE(jobs[i].prompt);
    }
    AIRY_FREE(plan_json);

    int produced = 0;
    for (int i = 0; i < CLI_REVIEW_TOPIC_COUNT; i++) {
        if (jobs[i].output)
            produced++;
    }
    if (produced == 0)
        return 0;

    cJSON *rep = cJSON_CreateObject();
    if (!rep)
        return 0;
    cJSON_AddStringToObject(rep, "reviewer", "cognition-sub-agents");
    cJSON *items = cJSON_CreateArray();
    if (items) {
        for (int i = 0; i < CLI_REVIEW_TOPIC_COUNT; i++) {
            if (!jobs[i].output)
                continue;
            cJSON *it = cJSON_CreateObject();
            if (!it)
                continue;
            cJSON_AddStringToObject(it, "topic", topics[i]);
            cJSON_AddStringToObject(it, "output", jobs[i].output);
            cJSON_AddItemToArray(items, it);
        }
        cJSON_AddItemToObject(rep, "reviews", items);
    }
    char *report = cJSON_PrintUnformatted(rep);
    cJSON_Delete(rep);

    for (int i = 0; i < CLI_REVIEW_TOPIC_COUNT; i++)
        AIRY_FREE(jobs[i].output);

    if (!report)
        return 0;
    if (out_report) {
        *out_report = report;
    } else {
        AIRY_FREE(report);
    }
    return 1;
}
