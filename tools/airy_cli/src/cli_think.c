// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_think.c
 * @brief airy_cli dual-thinking domain: remote plan parsing via think_d.
 *
 * When AIRY_THINK_SOCK is set, cognition planning goes through daemon RPC
 * (process method on think.sock, 120s timeout); the response is parsed
 * twice and the plan segment is restored to airy_task_plan_t. GCCP
 * two-pass interaction (P-A): when think_d returns gccp_need_interaction=1,
 * the question set is rendered to the user via cli_gccp_interact and the
 * collected answers are re-sent as gccp_answers (second pass). On failure
 * no data is fabricated; the caller falls back to the embedded engine.
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* 单轮 think.process RPC：携带 prompt 与可选 gccp_answers 调用 think_d，
 * 返回解析后的内层 JSON 根（OWNER，调用方 cJSON_Delete）。
 * 失败返回 NULL，err_out（可选）带回错误码。 */
static cJSON *cli_think_rpc_round(const char *think_sock, const char *prompt,
                                  const char *gccp_answers, int *err_out)
{
    if (err_out)
        *err_out = AIRY_SUCCESS;

    cJSON *params = cJSON_CreateObject();
    if (!params) {
        if (err_out)
            *err_out = AIRY_ERR_OUT_OF_MEMORY;
        return NULL;
    }
    cJSON *prompt_str = cJSON_CreateString(prompt);
    if (!prompt_str) {
        cJSON_Delete(params);
        if (err_out)
            *err_out = AIRY_ERR_OUT_OF_MEMORY;
        return NULL;
    }
    cJSON_AddItemToObject(params, "prompt", prompt_str);
    if (gccp_answers && gccp_answers[0]) {
        cJSON *ans_str = cJSON_CreateString(gccp_answers);
        if (!ans_str) {
            cJSON_Delete(params);
            if (err_out)
                *err_out = AIRY_ERR_OUT_OF_MEMORY;
            return NULL;
        }
        cJSON_AddItemToObject(params, "gccp_answers", ans_str);
    }

    char *params_json = cJSON_PrintUnformatted(params);
    cJSON_Delete(params);
    if (!params_json) {
        if (err_out)
            *err_out = AIRY_ERR_OUT_OF_MEMORY;
        return NULL;
    }

    char *rpc_result = NULL;
    int rc = daemon_rpc_call(think_sock, "process", params_json, &rpc_result, 120000);
    AIRY_FREE(params_json);
    if (rc != AIRY_SUCCESS || !rpc_result) {
        AIRY_FREE(rpc_result);
        if (err_out)
            *err_out = rc;
        return NULL;
    }

    cJSON *outer = cJSON_Parse(rpc_result);
    AIRY_FREE(rpc_result);
    if (!outer) {
        if (err_out)
            *err_out = AIRY_ERR_PARSE_ERROR;
        return NULL;
    }
    cJSON *inner_root = NULL;
    if (cJSON_IsString(outer) && outer->valuestring)
        inner_root = cJSON_Parse(outer->valuestring);
    cJSON_Delete(outer);
    if (!inner_root) {
        if (err_out)
            *err_out = AIRY_ERR_PARSE_ERROR;
        return NULL;
    }
    return inner_root;
}

/* think_d 返回的 gccp_questions JSON 数组 -> airy_gccp_probe_t。
 * 问题集来自远端引擎（与本地 probe 同构），prefill.raw_prompt 回填原始
 * 指令供 cli_gccp_interact 的逐问追问使用。OWNER：airy_gccp_probe_free。 */
static int cli_think_gccp_probe_build(const char *questions_json, const char *raw_prompt,
                                      airy_gccp_probe_t **out_probe)
{
    if (!out_probe)
        return AIRY_ERR_INVALID_PARAM;
    *out_probe = NULL;
    if (!questions_json || !questions_json[0] || !raw_prompt)
        return AIRY_ERR_INVALID_PARAM;

    cJSON *arr = cJSON_Parse(questions_json);
    if (!cJSON_IsArray(arr)) {
        if (arr)
            cJSON_Delete(arr);
        return AIRY_ERR_PARSE_ERROR;
    }
    int n = cJSON_GetArraySize(arr);

    airy_gccp_probe_t *probe = (airy_gccp_probe_t *)AIRY_CALLOC(1, sizeof(airy_gccp_probe_t));
    if (!probe) {
        cJSON_Delete(arr);
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    probe->prefill = (airy_gccp_goal_t *)AIRY_CALLOC(1, sizeof(airy_gccp_goal_t));
    if (!probe->prefill) {
        AIRY_FREE(probe);
        cJSON_Delete(arr);
        return AIRY_ERR_OUT_OF_MEMORY;
    }
    probe->prefill->raw_prompt = AIRY_STRDUP(raw_prompt);
    probe->question_count = (size_t)n;
    probe->questions = (airy_gccp_question_t *)AIRY_CALLOC(
        n > 0 ? (size_t)n : 1u, sizeof(airy_gccp_question_t));
    if (!probe->questions) {
        airy_gccp_probe_free(probe);
        cJSON_Delete(arr);
        return AIRY_ERR_OUT_OF_MEMORY;
    }

    for (int i = 0; i < n; i++) {
        cJSON *qj = cJSON_GetArrayItem(arr, i);
        if (!qj)
            continue;
        cJSON *f = cJSON_GetObjectItem(qj, "id");
        if (cJSON_IsString(f) && f->valuestring)
            AIRY_STRNCPY_TERM(probe->questions[i].id, f->valuestring,
                              sizeof(probe->questions[i].id));
        f = cJSON_GetObjectItem(qj, "question");
        if (cJSON_IsString(f) && f->valuestring)
            AIRY_STRNCPY_TERM(probe->questions[i].question, f->valuestring,
                              sizeof(probe->questions[i].question));
        f = cJSON_GetObjectItem(qj, "hint");
        if (cJSON_IsString(f) && f->valuestring)
            AIRY_STRNCPY_TERM(probe->questions[i].hint, f->valuestring,
                              sizeof(probe->questions[i].hint));
        f = cJSON_GetObjectItem(qj, "required");
        if (f && cJSON_IsTrue(f))
            probe->questions[i].required = 1;
    }

    cJSON_Delete(arr);
    *out_probe = probe;
    return AIRY_SUCCESS;
}

/* think.process plan segment -> airy_task_plan_t (nodes/goal/depends filled,
  * then the usual plan -> workflow -> hall chain) */
static int cli_think_plan_from_json(cJSON *plan_json, airy_task_plan_t **out_plan)
{
    if (!plan_json || !out_plan)
        return AIRY_ERR_INVALID_PARAM;
    *out_plan = NULL;

    cJSON *nodes = cJSON_GetObjectItem(plan_json, "nodes");
    int node_n = (nodes && cJSON_IsArray(nodes)) ? cJSON_GetArraySize(nodes) : 0;
    if (node_n <= 0)
        return AIRY_ERR_INVALID_PARAM;

    airy_task_plan_t *plan = (airy_task_plan_t *)AIRY_CALLOC(1, sizeof(airy_task_plan_t));
    if (!plan)
        return AIRY_ERR_OUT_OF_MEMORY;

    cJSON *pid = cJSON_GetObjectItem(plan_json, "task_plan_id");
    if (cJSON_IsString(pid) && pid->valuestring && pid->valuestring[0]) {
        plan->task_plan_id = AIRY_STRDUP(pid->valuestring);
        plan->task_plan_id_len = plan->task_plan_id ? strlen(plan->task_plan_id) : 0;
    }

    plan->task_plan_node_count = (size_t)node_n;
    plan->task_plan_nodes =
        (airy_task_node_t **)AIRY_CALLOC((size_t)node_n, sizeof(airy_task_node_t *));
    if (!plan->task_plan_nodes) {
        plan->task_plan_node_count = 0;
        goto fail;
    }

    for (int i = 0; i < node_n; i++) {
        cJSON *nj = cJSON_GetArrayItem(nodes, i);
        if (!nj)
            continue;
        airy_task_node_t *nd = (airy_task_node_t *)AIRY_CALLOC(1, sizeof(airy_task_node_t));
        if (!nd)
            goto fail;
        plan->task_plan_nodes[i] = nd;

        cJSON *f = cJSON_GetObjectItem(nj, "id");
        if (cJSON_IsString(f) && f->valuestring && f->valuestring[0]) {
            nd->task_node_id = AIRY_STRDUP(f->valuestring);
            nd->task_node_id_len = nd->task_node_id ? strlen(nd->task_node_id) : 0;
        }
        f = cJSON_GetObjectItem(nj, "goal");
        if (cJSON_IsString(f) && f->valuestring)
            nd->task_node_goal = AIRY_STRDUP(f->valuestring);
        f = cJSON_GetObjectItem(nj, "handler");
        if (cJSON_IsString(f) && f->valuestring)
            nd->task_node_handler_name = AIRY_STRDUP(f->valuestring);
        f = cJSON_GetObjectItem(nj, "role");
        if (cJSON_IsString(f) && f->valuestring) {
            nd->task_node_agent_role = AIRY_STRDUP(f->valuestring);
            nd->task_node_role_len =
                nd->task_node_agent_role ? strlen(nd->task_node_agent_role) : 0;
        }

        cJSON *deps = cJSON_GetObjectItem(nj, "depends");
        int dep_n = (deps && cJSON_IsArray(deps)) ? cJSON_GetArraySize(deps) : 0;
        if (dep_n > 0) {
            nd->task_node_depends_on = (char **)AIRY_CALLOC((size_t)dep_n, sizeof(char *));
            if (!nd->task_node_depends_on)
                goto fail;
            for (int d = 0; d < dep_n; d++) {
                cJSON *dj = cJSON_GetArrayItem(deps, d);
                if (!cJSON_IsString(dj) || !dj->valuestring)
                    continue;
                nd->task_node_depends_on[nd->task_node_depends_count] =
                    AIRY_STRDUP(dj->valuestring);
                if (!nd->task_node_depends_on[nd->task_node_depends_count])
                    goto fail;
                nd->task_node_depends_count++;
            }
        }

        f = cJSON_GetObjectItem(nj, "cost_time_ms");
        if (cJSON_IsNumber(f))
            nd->task_node_cost_time_ms = (int64_t)f->valuedouble;
        f = cJSON_GetObjectItem(nj, "cost_mem_mb");
        if (cJSON_IsNumber(f))
            nd->task_node_cost_mem_mb = (int64_t)f->valuedouble;
        f = cJSON_GetObjectItem(nj, "invariant_guard");
        if (f && cJSON_IsTrue(f))
            nd->task_node_invariant_guard = 1;
    }

    cJSON *entry = cJSON_GetObjectItem(plan_json, "entry_points");
    int entry_n = (entry && cJSON_IsArray(entry)) ? cJSON_GetArraySize(entry) : 0;
    if (entry_n > 0) {
        plan->task_plan_entry_points = (char **)AIRY_CALLOC((size_t)entry_n, sizeof(char *));
        if (!plan->task_plan_entry_points)
            goto fail;
        for (int e = 0; e < entry_n; e++) {
            cJSON *ej = cJSON_GetArrayItem(entry, e);
            if (!cJSON_IsString(ej) || !ej->valuestring)
                continue;
            plan->task_plan_entry_points[plan->task_plan_entry_count] =
                AIRY_STRDUP(ej->valuestring);
            if (!plan->task_plan_entry_points[plan->task_plan_entry_count])
                goto fail;
            plan->task_plan_entry_count++;
        }
    } else {
        plan->task_plan_entry_points = (char **)AIRY_CALLOC((size_t)node_n, sizeof(char *));
        if (!plan->task_plan_entry_points)
            goto fail;
        for (int i = 0; i < node_n; i++) {
            const airy_task_node_t *nd = plan->task_plan_nodes[i];
            if (!nd || !nd->task_node_id || nd->task_node_depends_count > 0)
                continue;
            plan->task_plan_entry_points[plan->task_plan_entry_count] =
                AIRY_STRDUP(nd->task_node_id);
            if (!plan->task_plan_entry_points[plan->task_plan_entry_count])
                goto fail;
            plan->task_plan_entry_count++;
        }
    }

    *out_plan = plan;
    return AIRY_SUCCESS;

fail:
    airy_task_plan_free(plan);
    return AIRY_ERR_OUT_OF_MEMORY;
}

/* Remote dual-thinking via AIRY_THINK_SOCK: daemon_rpc_call(think.sock, "process", ..., 120s).
 * GCCP 两段式交互（P-A）：第一段无 gccp_answers，think_d 判定指令不完整时返回
 * gccp_need_interaction=1 + gccp_questions；本函数转成 airy_gccp_probe_t 交给
 * cli_gccp_interact 逐问收集答案，第二段携带 gccp_answers 重发，引擎完成目标
 * 确认进入后续 Phase。两次挂起（第二段仍要求交互）视为失败，调用方回退内置
 * 引擎。失败不伪造数据。 */
airy_err_t cli_think_process_remote(const char *think_sock, const char *input,
                                    size_t input_len, airy_task_plan_t **out_plan)
{
    (void)input_len;
    if (!think_sock || !think_sock[0] || !input || !out_plan)
        return AIRY_ERR_INVALID_PARAM;
    *out_plan = NULL;

    /* 第一段：无 gccp_answers。 */
    int rerr = AIRY_SUCCESS;
    cJSON *inner = cli_think_rpc_round(think_sock, input, NULL, &rerr);
    if (!inner)
        return (airy_err_t)rerr;

    /* 第二段：think_d 请求交互——向用户展示远端问题集并收集答案后重发。 */
    cJSON *interact = cJSON_GetObjectItem(inner, "gccp_need_interaction");
    if (interact && cJSON_IsTrue(interact)) {
        cJSON *qjson = cJSON_GetObjectItem(inner, "gccp_questions");
        const char *qstr = (cJSON_IsString(qjson) && qjson->valuestring) ?
                               qjson->valuestring :
                               "";
        airy_gccp_probe_t *probe = NULL;
        int perr = cli_think_gccp_probe_build(qstr, input, &probe);
        cJSON_Delete(inner);
        inner = NULL;
        if (perr != AIRY_SUCCESS || !probe)
            return (airy_err_t)perr;

        /* 逐问收集答案。用户放弃（NULL）→ 空对象：引擎按默认约束收敛，
         * 避免重发空答案导致再次挂起形成交互死循环。 */
        char *answers = cli_gccp_interact(probe, NULL);
        if (!answers) {
            answers = AIRY_STRDUP("{}");
            if (!answers) {
                airy_gccp_probe_free(probe);
                return AIRY_ERR_OUT_OF_MEMORY;
            }
        }
        airy_gccp_probe_free(probe);

        rerr = AIRY_SUCCESS;
        inner = cli_think_rpc_round(think_sock, input, answers, &rerr);
        AIRY_FREE(answers);
        if (!inner)
            return (airy_err_t)rerr;

        /* 二次挂起（远端仍要求交互）：放弃远端，回退内置引擎。 */
        cJSON *again = cJSON_GetObjectItem(inner, "gccp_need_interaction");
        if (again && cJSON_IsTrue(again)) {
            cJSON_Delete(inner);
            return AIRY_ERR_GCCP_INTERACTION;
        }
    }

    cJSON *plan_json = cJSON_GetObjectItem(inner, "plan");
    int perr = AIRY_SUCCESS;
    if (!cJSON_IsObject(plan_json))
        perr = AIRY_ERR_PARSE_ERROR;
    else
        perr = cli_think_plan_from_json(plan_json, out_plan);
    if (perr == AIRY_SUCCESS && !*out_plan)
        perr = AIRY_ERR_PARSE_ERROR;

    cJSON_Delete(inner);
    return (airy_err_t)perr;
}

/* ==================== 双思考三模型配置统一读取 ==================== */

/* 从 model.yaml 提取标量字段（BAN 合规：手写解析，禁 sscanf）。
 *
 * 结构约定（ecosystem/manager/model/model.yaml）：
 *   llm:
 *     model: "deepseek-v4-flash"
 *   think:
 *     think2_slow_model: ""
 *     think1_fast_model: ""
 *     think1_prof_model: ""
 * 无缩进段标题（"name:"）切换当前段；目标段内缩进的 "key: \"value\""
 * 行命中即返回。返回 0 找到（含空串），-1 未找到/文件不可读。 */
static int cli_think_cfg_yaml_get(const char *path, const char *section,
                                  const char *key, char *out, size_t cap)
{
    if (!path || !section || !key || !out || cap == 0)
        return -1;
    out[0] = '\0';

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    char line[512];
    char cur[32] = "";
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        const char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        const char *kstart = p;
        while (*p && *p != ':' && *p != ' ' && *p != '\t')
            p++;
        if (*p != ':' || p == kstart)
            continue;

        const char *after = p + 1;
        while (*after == ' ' || *after == '\t')
            after++;
        size_t kl = (size_t)(p - kstart);

        if (*after == '\0') {
            /* 段标题行：key:（后无其他非空白） */
            if (kl < sizeof(cur)) {
                AIRY_MEMCPY(cur, kstart, kl);
                cur[kl] = '\0';
            }
            continue;
        }
        if (strcmp(cur, section) != 0)
            continue;
        if (strlen(key) != kl || strncmp(kstart, key, kl) != 0)
            continue;

        if (*after == '"') {
            /* 引号包裹值：截到闭引号 */
            const char *v = after + 1;
            size_t vl = 0;
            while (v[vl] && v[vl] != '"' && vl + 1 < cap)
                vl++;
            AIRY_MEMCPY(out, v, vl);
            out[vl] = '\0';
        } else {
            /* 无引号标量（timeout_ms 等）：仅确认键存在，值为空 */
            out[0] = '\0';
        }
        found = 1;
        break;
    }
    fclose(f);
    return found ? 0 : -1;
}

/* 定位 model.yaml：AIRY_MODEL_CONFIG > $AIRY_HOME/config/model.yaml */
static void cli_think_cfg_path(char *path, size_t cap)
{
    const char *cfg = getenv("AIRY_MODEL_CONFIG");
    if (cfg && cfg[0]) {
        snprintf(path, cap, "%s", cfg);
        return;
    }
    snprintf(path, cap, "%s/model.yaml", airy_config_dir());
}

/* 双思考三模型配置统一读取，CLI 侧真相源对齐 think_d。
 *
 * 优先级：env AIRY_MODEL_T2/T1F/T1P > $AIRY_MODEL_CONFIG（或
 * $AIRY_HOME/config/model.yaml）think 段 > 该文件 llm.model 默认
 * （model.yaml 语义：think 段留空 = 使用默认模型）。
 *
 * 2026-08-19：CLI 此前只读 env，与 think_d 配置脱节——think 段留空
 * 时 CLI 显示"默认"而 think_d 实际用 llm.model；现统一为真实生效
 * 模型。调用方对空串按原语义处理（显示"默认"/传 NULL 走 provider
 * default）。 */
void cli_think_cfg_load(char *t2, size_t t2c, char *t1f, size_t t1fc,
                        char *t1p, size_t t1pc)
{
    if (!t2 || !t1f || !t1p || t2c == 0 || t1fc == 0 || t1pc == 0)
        return;
    t2[0] = t1f[0] = t1p[0] = '\0';

    const char *e2 = getenv("AIRY_MODEL_T2");
    const char *e1f = getenv("AIRY_MODEL_T1F");
    const char *e1p = getenv("AIRY_MODEL_T1P");
    if (e2 && e2[0])
        snprintf(t2, t2c, "%s", e2);
    if (e1f && e1f[0])
        snprintf(t1f, t1fc, "%s", e1f);
    if (e1p && e1p[0])
        snprintf(t1p, t1pc, "%s", e1p);
    if (t2[0] && t1f[0] && t1p[0])
        return;

    char path[AIRY_PATH_MAX];
    cli_think_cfg_path(path, sizeof(path));

    if (t2[0] == '\0')
        cli_think_cfg_yaml_get(path, "think", "think2_slow_model", t2, t2c);
    if (t1f[0] == '\0')
        cli_think_cfg_yaml_get(path, "think", "think1_fast_model", t1f, t1fc);
    if (t1p[0] == '\0')
        cli_think_cfg_yaml_get(path, "think", "think1_prof_model", t1p, t1pc);

    /* think 段留空 = llm.model 默认 */
    if (t2[0] && t1f[0] && t1p[0])
        return;
    char def_model[128] = "";
    cli_think_cfg_yaml_get(path, "llm", "model", def_model, sizeof(def_model));
    if (def_model[0]) {
        if (t2[0] == '\0')
            snprintf(t2, t2c, "%s", def_model);
        if (t1f[0] == '\0')
            snprintf(t1f, t1fc, "%s", def_model);
        if (t1p[0] == '\0')
            snprintf(t1p, t1pc, "%s", def_model);
    }
}

/* 仅显式配置：env AIRY_MODEL_T2/T1F/T1P + model.yaml think 段，不回填
 * llm.model 默认。执行复核等场景须区分"用户显式指定"与"默认回填"——
 * 默认回填意味着与主生成同模型，复核会自审自签，必须降级而非采用。
 * 返回是否至少一个字段显式配置（输出为对应模型名，未配置为空串）。 */
int cli_think_cfg_explicit(char *t2, size_t t2c, char *t1f, size_t t1fc,
                           char *t1p, size_t t1pc)
{
    if (!t2 || !t1f || !t1p || t2c == 0 || t1fc == 0 || t1pc == 0)
        return 0;
    t2[0] = t1f[0] = t1p[0] = '\0';

    const char *e2 = getenv("AIRY_MODEL_T2");
    const char *e1f = getenv("AIRY_MODEL_T1F");
    const char *e1p = getenv("AIRY_MODEL_T1P");
    if (e2 && e2[0])
        snprintf(t2, t2c, "%s", e2);
    if (e1f && e1f[0])
        snprintf(t1f, t1fc, "%s", e1f);
    if (e1p && e1p[0])
        snprintf(t1p, t1pc, "%s", e1p);
    if (t2[0] && t1f[0] && t1p[0])
        return 1;

    char path[AIRY_PATH_MAX];
    cli_think_cfg_path(path, sizeof(path));
    if (t2[0] == '\0')
        cli_think_cfg_yaml_get(path, "think", "think2_slow_model", t2, t2c);
    if (t1f[0] == '\0')
        cli_think_cfg_yaml_get(path, "think", "think1_fast_model", t1f, t1fc);
    if (t1p[0] == '\0')
        cli_think_cfg_yaml_get(path, "think", "think1_prof_model", t1p, t1pc);
    return (t2[0] || t1f[0] || t1p[0]) ? 1 : 0;
}
