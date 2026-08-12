// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file cli_display.c
 * @brief airy_cli display domain: banner, result formatting, node progress and board bars.
 *
 * All terminal-facing display logic: startup banner, result pretty-printing
 * (prefer extracting the output field from JSON), node-level progress
 * callbacks and generic board progress lines.
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

/**
 * @brief Pretty-print execution results (prefer parsing JSON for the output field).
 *
 * result is the output JSON from dag_wait; when it contains output/agent_id
 * fields, print them separately, otherwise truncate the raw output.
 * Colors are enabled on POSIX terminals only.
 */
void cli_print_result(const char *result)
{
    if (!result) {
        printf("  %s[结果]%s （未获取到结果）\n", CLR_YELLOW, CLR_RESET);
        return;
    }

#ifdef AIRY_HAS_CJSON
    cJSON *root = cJSON_Parse(result);
    if (root) {
        cJSON *output = cJSON_GetObjectItem(root, "output");
        cJSON *agent = cJSON_GetObjectItem(root, "agent_id");
        if (output && cJSON_IsString(output)) {
            printf("  %s[结果]%s 执行完成\n", CLR_GREEN, CLR_RESET);
            if (agent && cJSON_IsString(agent))
                printf("    %s执行体%s %s\n", CLR_CYAN, CLR_RESET, agent->valuestring);
            printf("    %s产出%s\n", CLR_CYAN, CLR_RESET);
            printf("    %s%s%s\n", CLR_GREEN, output->valuestring, CLR_RESET);
            cJSON_Delete(root);
            return;
        }
        cJSON_Delete(root);
    }
#endif /* AIRY_HAS_CJSON */

    printf("  %s[结果]%s %.600s%s\n", CLR_GREEN, CLR_RESET, result,
           strlen(result) > 600 ? "..." : "");
}

/**
  * @brief Per-node progress callback for task sets (work hall forwards taskflow progress)
 *
  * Fired per node while run_to_completion drives them (node_id set),
  * and once more at completion (node_id empty, state is the overall final state).
  * Lets users watch per-node progress instead of a frozen-looking screen.
 */
void cli_progress_cb(const char *execution_id, const char *node_id, taskflow_state_t state,
                            double progress, void *user_data)
{
    (void)user_data;
    if (!node_id) {
        if (state == TASKFLOW_STATE_COMPLETED)
            printf("  %s[看板]%s %s 执行完成 %3.0f%%\n", CLR_GREEN, CLR_RESET, execution_id,
                   progress * 100.0);
        else if (state == TASKFLOW_STATE_FAILED)
            printf("  %s[看板]%s %s 执行失败\n", CLR_RED, CLR_RESET, execution_id);
        return;
    }
    int filled = (int)(progress * 10.0f);
    if (filled < 0)
        filled = 0;
    if (filled > 10)
        filled = 10;
    char bar[32];
    size_t bo = 0;
    for (int b = 0; b < 10; b++) {
        const char *seg = (b < filled) ? "█" : "░";
        bo += (size_t)snprintf(bar + bo, sizeof(bar) - bo, "%s", seg);
    }
    printf("  %s[执行]%s 节点 %s 完成 %s %3.0f%%\n", CLR_GREEN, CLR_RESET, node_id, bar,
           progress * 100.0);
}

void cli_board_line(const char *tag, const char *id, const char *state, double progress)
{
    int filled = (int)(progress * 10.0f);
    if (filled < 0)
        filled = 0;
    if (filled > 10)
        filled = 10;
    char bar[32];
    size_t bo = 0;
    for (int b = 0; b < 10; b++) {
        const char *seg = (b < filled) ? "█" : "░";
        bo += (size_t)snprintf(bar + bo, sizeof(bar) - bo, "%s", seg);
    }
    printf("  %s[%s]%s %s%s%s %s%-6s%s %s[%s]%s %3.0f%%\n", CLR_GREEN, tag, CLR_RESET, CLR_YELLOW,
           id ? id : "?", CLR_RESET, CLR_CYAN, state ? state : "?", CLR_RESET, CLR_GREEN, bar,
           CLR_RESET, progress * 100.0);
}

void cli_print_banner(void)
{
    printf("\n"
           "  %s╔══════════════════════════════════════════════════╗%s\n"
           "  %s║          AgentRT 智能体运行时 — 交互入口          ║%s\n"
           "  %s╚══════════════════════════════════════════════════╝%s\n",
           CLR_CYAN, CLR_RESET, CLR_CYAN, CLR_RESET, CLR_CYAN, CLR_RESET);
    printf("  %s版本%s v%s | 超级智能体对话 + GCCP 意图确认 + WorkHall 调度\n", CLR_GREEN,
           CLR_RESET, AIRY_CLI_VERSION);
    printf("  %s说明%s 普通对话直接回复；任务指令（如：\n"
           "        \"为项目实现登录模块，包含前端页面、后端接口与单元测试\"）\n"
           "       将启动完备四问确认后规划执行。\n"
           "  %s提示%s 输入 %s/help%s 查看命令，%squit%s/%sexit%s 退出。\n",
           CLR_GREEN, CLR_RESET, CLR_GREEN, CLR_RESET, CLR_YELLOW, CLR_RESET, CLR_YELLOW, CLR_RESET,
           CLR_YELLOW, CLR_RESET);
}
