// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file airy_cli_frontend.c
 * @brief TUI 前端子进程生命周期（0.1.17 R5-G6 入口唯一化）。
 *
 * --tui 模式与 /tui 切换的唯一实现：airy_cli 调起 agentrt-tui 子进程
 * （继承终端 stdio），父进程等待并回传退出码。TUI 异常退出只渲染可判读
 * 错误，不做任何接力降级；gateway 地址经 AIRY_GATEWAY_URL 环境变量以
 * --gateway-url 下沉（TUI 缺省自读 run/gateway.port）。
 */

#include "cli_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* TUI 二进制存在性判定（不区分平台的差异只在此处）。 */
static int tui_bin_present(const char *path)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(path, X_OK) == 0;
#endif
}

/* 退出码约定：126 = 调起失败，128+N = 信号终止，其余透传 TUI 退出码。 */
int cli_run_tui_frontend(int resume)
{
    char tui_bin[AIRY_PATH_MAX];
    snprintf(tui_bin, sizeof(tui_bin), "%s/agentrt-tui", airy_bin_dir());
    if (!tui_bin_present(tui_bin)) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui",
                             "agentrt-tui 未安装（$AIRY_HOME/bin/agentrt-tui 缺失），"
                             "请重新执行安装。");
        return 126;
    }

    const char *gw = getenv("AIRY_GATEWAY_URL");
    const char *agf = getenv("AIRY_AGENT_FILE");

#ifndef _WIN32
    char *argv[8];
    int n = 0;
    argv[n++] = tui_bin;
    if (gw && gw[0]) {
        argv[n++] = (char *)"--gateway-url";
        argv[n++] = (char *)gw;
    }
    if (agf && agf[0]) {
        argv[n++] = (char *)"--agent-file";
        argv[n++] = (char *)agf;
    }
    if (resume)
        argv[n++] = (char *)"--resume";
    argv[n] = NULL;

    fflush(NULL);
    pid_t pid = fork();
    if (pid < 0) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui",
                             "无法创建 TUI 子进程（fork 失败）。");
        return 126;
    }
    if (pid == 0) {
        extern char **environ;
        execve(tui_bin, argv, environ);
        _exit(126);
    }

    int st = 0;
    while (waitpid(pid, &st, 0) < 0) {
        if (errno != EINTR) {
            cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui",
                                 "等待 TUI 子进程失败。");
            return 126;
        }
    }
    if (WIFSIGNALED(st)) {
        char msg[128];
        snprintf(msg, sizeof(msg), "TUI 异常终止（信号 %d），日志: logs/agentrt-tui.log",
                 WTERMSIG(st));
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui", msg);
        return 128 + WTERMSIG(st);
    }
    int rc = WIFEXITED(st) ? WEXITSTATUS(st) : 126;
    if (rc != 0) {
        char msg[160];
        snprintf(msg, sizeof(msg), "TUI 退出码 %d，日志: logs/agentrt-tui.log", rc);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui", msg);
    }
    return rc;
#else
    char cmd[AIRY_PATH_MAX + 256];
    snprintf(cmd, sizeof(cmd), "\"%s\"", tui_bin);
    if (gw && gw[0])
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd),
                 " --gateway-url \"%s\"", gw);
    if (agf && agf[0])
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd),
                 " --agent-file \"%s\"", agf);
    if (resume)
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " --resume");

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessA(tui_bin, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui",
                             "无法创建 TUI 子进程（CreateProcess 失败）。");
        return 126;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 126;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code != 0 && code != STILL_ACTIVE) {
        char msg[160];
        snprintf(msg, sizeof(msg), "TUI 退出码 %lu，日志: logs/agentrt-tui.log",
                 (unsigned long)code);
        cli_render_role_line(CLI_ROLE_ERROR, CLI_ACTOR_SUPER_AGENT, "tui", msg);
    }
    return (int)code;
#endif
}
