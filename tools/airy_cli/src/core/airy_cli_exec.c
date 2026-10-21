// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_exec.c
 * @brief Task execution helpers extracted from main.c.
 *
 * Background wait worker (gateway → sched_d, the only execution path), stdin
 * polling during execution, decision-chain submission recording, and task
 * result rendering (JSON success/failure determination).
 */

#include "airy_cli_exec.h"
#include "cli_internal.h"
#include "cli_render.h"

/* 0.1.16 B2：corekern 总伞头已摘除，错误码契约由 airy_types.h 提供
 * （经 airy_cli_exec.h/cli_internal.h 传入）。 */
#include "hall_store.h"
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
    c->err = cli_dag_wait_remote(c->exec_id, &c->result);
    c->done = 1;
    return NULL;
}

int cli_task_poll_input(void)
{
    if (g_cli_print_mode || g_cli_json_mode)
        return 0;
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
    cli_print_line("For Thee", line);
    cli_chat_reply(line);
    return 1;
#else
    (void)0;
    return 0;
#endif
}

void cli_chain_record_submit(const char *exec_id, const airy_task_plan_t *plan)
{
    if (!exec_id || !exec_id[0])
        return;
    cli_hall_emit(exec_id, AIRY_HALL_CAT_COMMAND,
                  "{\"dag_id\":\"%s\",\"plan_id\":\"%s\",\"nodes\":%zu,\"edges\":%zu}",
                  exec_id,
                  (plan && plan->task_plan_id) ? plan->task_plan_id : "",
                  plan ? plan->task_plan_node_count : (size_t)0,
                  cli_plan_deps_count(plan));
}

#ifdef AIRY_HAS_CJSON
/* 结果体自述的成败：status 优先，缺失时看 success / error。 */
static int body_ok(const cJSON *obj)
{
    const cJSON *st = cJSON_GetObjectItem(obj, "status");
    const cJSON *ok;
    const cJSON *errj;

    if (cJSON_IsString(st) && st->valuestring)
        return strcmp(st->valuestring, "completed") == 0;
    ok = cJSON_GetObjectItem(obj, "success");
    errj = cJSON_GetObjectItem(obj, "error");
    if (cJSON_IsBool(ok) && !cJSON_IsTrue(ok))
        return 0;
    if (cJSON_IsString(ok) && strcmp(ok->valuestring, "true") != 0)
        return 0;
    return !(cJSON_IsString(errj) && errj->valuestring[0]);
}
#endif

/* 结果体自述的成败：解析失败按成功处理（无自述即不否决）。 */
static int task_ok_probe(const char *result)
{
#ifdef AIRY_HAS_CJSON
    cJSON *rstat = cJSON_Parse(result);
    int ok;

    if (!rstat)
        return 1;
    ok = body_ok(rstat);
    cJSON_Delete(rstat);
    return ok;
#else
    return !(strstr(result, "\"failed\"") || strstr(result, "\"canceled\"") ||
             strstr(result, "\"success\":false") || strstr(result, "\"error\":"));
#endif
}

/* 单次 JSON 信封：成功携带 result，失败携带错误码。 */
static void json_emit(const char *exec_id, int ok, const char *result, airy_err_t err)
{
    const char *dag_id = exec_id ? exec_id : "";

#ifdef AIRY_HAS_CJSON
    cJSON *jroot = cJSON_CreateObject();
    char *js;

    if (!jroot)
        return;
    cJSON_AddStringToObject(jroot, "role", "super_agent");
    cJSON_AddStringToObject(jroot, "type", "task");
    cJSON_AddBoolToObject(jroot, "success", ok);
    cJSON_AddStringToObject(jroot, "dag_id", dag_id);
    if (ok)
        cJSON_AddStringToObject(jroot, "result", result ? result : "");
    else
        cJSON_AddNumberToObject(jroot, "code", (double)(int)err);
    js = cJSON_PrintUnformatted(jroot);
    if (js) {
        cli_outf("%s\n", js);
        cJSON_free(js);
    }
    cJSON_Delete(jroot);
#else
    if (ok)
        cli_outf("{\"role\":\"super_agent\",\"type\":\"task\",\"success\":true,"
                 "\"dag_id\":\"%s\",\"result\":\"%s\"}\n",
                 dag_id, result ? result : "");
    else
        cli_outf("{\"role\":\"super_agent\",\"type\":\"task\",\"success\":false,"
                 "\"dag_id\":\"%s\",\"code\":%d}\n",
                 dag_id, (int)err);
#endif
}

int cli_task_result_render(const char *result, airy_err_t err, const char *exec_id, int canceled)
{
    int ok = (err == AIRY_EOK && result) ? task_ok_probe(result) : 0;

    if (canceled) {
        cli_print_line("Sub cancel Agent", "Task aborted (stopped after the current node).");
        cli_trace("result", "%s canceled", CLI_ICON_CROSS);
        return ok;
    }
    if (err == AIRY_EOK && result) {
        cli_print_status("phase", "结果汇总");
        if (g_cli_json_mode)
            json_emit(exec_id, ok, result, err);
        else
            cli_print_result(result);
        cli_trace("result", "%s success=%d", ok ? CLI_ICON_CHECK : CLI_ICON_CROSS, ok ? 1 : 0);
    } else if (g_cli_json_mode) {
        json_emit(exec_id, 0, NULL, err);
    } else {
        char line[128];

        snprintf(line, sizeof(line), "任务执行无结果：%s", cli_err_desc((int)err));
        cli_print_line("Super Agent:执行结果", line);
    }
    return ok;
}
