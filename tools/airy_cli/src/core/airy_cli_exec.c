// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_exec.c
 * @brief Task execution helpers extracted from main.c.
 *
 * Background wait worker, stdin polling during execution, decision-chain
 * submission recording, and task result rendering (JSON success/failure
 * determination + validation gate annotation).
 */

#include "airy_cli_exec.h"
#include "cli_internal.h"
#include "cli_render.h"
#include "cli_exec_review.h"

#include "airy_rt.h"
#include "work_hall.h"
#include "hall_store.h"
#include "plan_to_dag.h"
#include "taskflow_advanced.h"
#include "logger.h"
#include "airy_memory.h"
#include "string_compat.h"

#ifdef AIRY_HAS_CJSON
#include <cjson/cJSON.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <poll.h>
#endif

void *cli_task_wait_worker(void *arg)
{
    cli_task_wait_ctx_t *c = (cli_task_wait_ctx_t *)arg;
    if (!c)
        return NULL;
    if (c->sched_remote) {
        c->err = cli_dag_wait_remote(c->sched_sock, c->exec_id, &c->result);
    } else {
        c->err = airy_work_hall_wait(c->hall, c->exec_id, 0, &c->result);
        airy_work_hall_set_blueprint(c->hall, NULL);
    }
    c->done = 1;
    return NULL;
}

int cli_task_poll_input(void)
{
    if (g_cli_print_mode || g_cli_json_mode)
        return 0;
    cli_tui_t *tui = cli_tui_get_default();
    if (tui && cli_tui_active(tui)) {
        int eof = 0;
        int key = cli_tui_poll_key(tui, &eof);
        if (eof) {
            g_cli_cancel = 1;
            return -1;
        }
        if (key == 0x03) {
            g_cli_cancel = 1;
            return -1;
        }
        return 0;
    }
#ifndef _WIN32
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int r = poll(&pfd, 1, 0);
    if (r <= 0 || !(pfd.revents & POLLIN))
        return 0;
    char line[4096];
    if (!fgets(line, sizeof(line), stdin))
        return 0;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
    if (n == 0)
        return 0;
    {
        size_t nz = 0;
        while (nz < n && (line[nz] == ' ' || line[nz] == '\t'))
            nz++;
        if (nz == n)
            return 0;
    }
    if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0 ||
        strcmp(line, "abort") == 0 || strcmp(line, "stop") == 0 ||
        strcmp(line, "cancel") == 0 || strcmp(line, "打断") == 0 ||
        strcmp(line, "停止") == 0 || strcmp(line, "取消") == 0) {
        g_cli_cancel = 1;
        return -1;
    }
    cli_spinner_pause();
    cli_render_user_message(line);
    cli_chat_reply(line);
    cli_spinner_resume();
    return 1;
#else
    (void)0;
    return 0;
#endif
}

void cli_chain_record_submit(const char *exec_id, const airy_task_plan_t *plan,
                              const taskflow_workflow_t *wf)
{
    if (!g_cli_hall_store || !exec_id || !exec_id[0])
        return;
    char ev[384];
    snprintf(ev, sizeof(ev),
             "{\"dag_id\":\"%s\",\"plan_id\":\"%s\",\"nodes\":%zu,\"edges\":%zu}",
             exec_id, (plan && plan->task_plan_id) ? plan->task_plan_id : "",
             wf ? wf->node_count : 0, wf ? wf->edge_count : 0);
    airy_hall_store_write(g_cli_hall_store, "default", exec_id, NULL, AIRY_HALL_CAT_COMMAND,
                          "cognition", ev, NULL, 0);
}

int cli_task_result_render(const char *result, airy_err_t err, const char *exec_id,
                            int canceled, airy_work_hall_t *hall, uint32_t vf_before)
{
    int task_succeeded = (err == AIRY_EOK && result) ? 1 : 0;
    if (task_succeeded && result) {
#ifdef AIRY_HAS_CJSON
        cJSON *rstat = cJSON_Parse(result);
        if (rstat) {
            cJSON *st = cJSON_GetObjectItem(rstat, "status");
            if (cJSON_IsString(st) && st->valuestring) {
                if (strcmp(st->valuestring, "completed") != 0)
                    task_succeeded = 0;
            } else {
                cJSON *ok = cJSON_GetObjectItem(rstat, "success");
                if (cJSON_IsBool(ok) && !cJSON_IsTrue(ok))
                    task_succeeded = 0;
                else if (cJSON_IsString(ok) && strcmp(ok->valuestring, "true") != 0)
                    task_succeeded = 0;
                cJSON *errj = cJSON_GetObjectItem(rstat, "error");
                if (errj && cJSON_IsString(errj) && errj->valuestring &&
                    errj->valuestring[0])
                    task_succeeded = 0;
            }
            cJSON_Delete(rstat);
        }
#else
        if (strstr(result, "\"failed\"") || strstr(result, "\"canceled\"") ||
            strstr(result, "\"success\":false") || strstr(result, "\"error\":"))
            task_succeeded = 0;
#endif
    }
    if (canceled) {
        cli_render_sub_agent_line(CLI_ROLE_ERROR, "cancel",
                                  "Task aborted (stopped after the current node).");
        cli_trace("result", "%s canceled", CLI_ICON_CROSS);
    } else if (err == AIRY_EOK && result) {
        cli_render_phase("结果汇总");
        if (g_cli_json_mode) {
#ifdef AIRY_HAS_CJSON
            cJSON *jroot = cJSON_CreateObject();
            cJSON_AddStringToObject(jroot, "role", "super_agent");
            cJSON_AddStringToObject(jroot, "type", "task");
            cJSON_AddBoolToObject(jroot, "success", task_succeeded);
            cJSON_AddStringToObject(jroot, "dag_id", exec_id ? exec_id : "");
            cJSON_AddStringToObject(jroot, "result", result);
            char *js = cJSON_PrintUnformatted(jroot);
            if (js) {
                cli_outf("%s\n", js);
                cJSON_free(js);
            }
            cJSON_Delete(jroot);
#else
            cli_outf("{\"role\":\"super_agent\",\"type\":\"task\",\"success\":%s,"
                     "\"dag_id\":\"%s\",\"result\":\"%s\"}\n",
                     task_succeeded ? "true" : "false",
                     exec_id ? exec_id : "", result);
#endif
        } else {
            cli_print_result(result);
        }
        cli_trace("result", "%s success=%d", task_succeeded ? CLI_ICON_CHECK : CLI_ICON_CROSS,
                  task_succeeded ? 1 : 0);
    } else {
        if (g_cli_json_mode) {
#ifdef AIRY_HAS_CJSON
            cJSON *jroot = cJSON_CreateObject();
            cJSON_AddStringToObject(jroot, "role", "super_agent");
            cJSON_AddStringToObject(jroot, "type", "task");
            cJSON_AddBoolToObject(jroot, "success", 0);
            cJSON_AddStringToObject(jroot, "dag_id", exec_id ? exec_id : "");
            cJSON_AddNumberToObject(jroot, "code", (double)(int)err);
            char *js = cJSON_PrintUnformatted(jroot);
            if (js) {
                cli_outf("%s\n", js);
                cJSON_free(js);
            }
            cJSON_Delete(jroot);
#else
            cli_outf("{\"role\":\"super_agent\",\"type\":\"task\",\"success\":false,"
                     "\"dag_id\":\"%s\",\"code\":%d}\n",
                     exec_id ? exec_id : "", (int)err);
#endif
        } else {
            char line[128];
            snprintf(line, sizeof(line), "任务执行无结果：%s", cli_err_desc((int)err));
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "执行结果", line);
        }
    }
    if (!canceled) {
        uint32_t vf_after = 0;
        airy_work_hall_verify_stats(hall, NULL, &vf_after, NULL);
        if (vf_after > vf_before)
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_DUAL_PROF_THINK, "校验",
                                 "Artifact validation failed - replan or retry the task.");
    }
    return task_succeeded;
}
