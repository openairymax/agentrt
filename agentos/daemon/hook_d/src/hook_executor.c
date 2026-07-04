/**
 * @file hook_executor.c
 * @brief P2.1.2: Hook 执行器实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "hook_executor.h"
#include "hook_timeout.h"
#include "svc_logger.h"
#include "memory_compat.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
/* hook_run_subprocess 的 fork/execve/execvp/pipe/select/waitpid 实现仅 POSIX
 * 可用；Windows 分支见 hook_run_subprocess 的 _WIN32 实现。 */
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef AGENTOS_HAS_CURL
#include <curl/curl.h>
#endif

/* ==================== 子进程执行常量 ==================== */

/* Shell/Python 脚本执行超时（对齐 HOOK_TIMEOUT_MAX_MS=30s，防止恶意脚本挂起） */
#define HOOK_SHELL_TIMEOUT_SEC    30
/* Webhook HTTP 请求超时（对齐原 curl --max-time 5） */
#define HOOK_WEBHOOK_TIMEOUT_SEC   5

/* ==================== 子进程执行基础设施（前向声明） ==================== */

/*
 * 安全的子进程执行：fork + execve/execvp + pipe(捕获输出) + select 超时 + waitpid。
 *
 * 设计要点（BAN-211/235 安全合规）：
 *   - 绝不使用 system() 或 /bin/sh -c 拼接命令字符串（命令注入风险）
 *   - 通过 argv[] 数组传参，环境变量通过 envp[] 传递（不拼入命令行）
 *   - envp != NULL 时使用 execve（完整路径 + 自定义环境）
 *   - envp == NULL 时使用 execvp（PATH 搜索 + 继承 environ，用于 curl 后备）
 *   - 捕获并丢弃子进程输出，防止管道阻塞导致挂起
 *   - 超时后 SIGKILL 终止并回收僵尸进程
 *
 * 返回值：子进程退出码(0-255)；-1=启动失败；-2=超时
 */
static int hook_run_subprocess(const char *const argv[], char **envp, int timeout_sec);

/* 构建环境数组 = 继承 environ + AGENTOS_HOOK_CONTEXT=<json>，返回 NULL 终止数组 */
static char **hook_build_env_with_context(const char *context_json);

/* 释放 hook_build_env_with_context 返回的环境数组 */
static void hook_free_env(char **env);

/*
 * 通用脚本执行：序列化上下文 → 构建环境 → fork+execve 运行解释器。
 * interpreter = "/bin/sh"（Shell Hook）或 "/usr/bin/python3"（Python Hook）。
 * 返回退出码（0=CONTINUE, 1=SKIP, 2=RETRY, 3=ABORT, 4=MODIFY），-1=失败。
 */
static int hook_run_script(const char *interpreter, const char *script_path,
                           hook_context_t *ctx, int timeout_sec);

/* ==================== 决策聚合 ==================== */

hook_decision_t hook_executor_merge_decision(hook_decision_t current,
                                              hook_decision_t new_dec)
{
    /* ABORT 优先级最高，一旦 ABORT 不再改变 */
    if (current == HOOK_DECISION_ABORT || new_dec == HOOK_DECISION_ABORT)
        return HOOK_DECISION_ABORT;

    /* RETRY 次之 */
    if (current == HOOK_DECISION_RETRY || new_dec == HOOK_DECISION_RETRY)
        return HOOK_DECISION_RETRY;

    /* MODIFY */
    if (current == HOOK_DECISION_MODIFY || new_dec == HOOK_DECISION_MODIFY)
        return HOOK_DECISION_MODIFY;

    /* SKIP */
    if (current == HOOK_DECISION_SKIP || new_dec == HOOK_DECISION_SKIP)
        return HOOK_DECISION_SKIP;

    return HOOK_DECISION_CONTINUE;
}

/* ==================== 上下文序列化 ==================== */

int hook_executor_ctx_to_json(const hook_context_t *ctx, char *buf, size_t bufsize)
{
    if (!ctx || !buf || bufsize == 0) return -1;

    const char *type_names[] = {
        "PRE_EXEC", "POST_EXEC", "PRE_LLM", "POST_LLM",
        "PRE_TOOL", "POST_TOOL", "ON_ERROR", "ON_MEMORY_EVOLVE"
    };

    const char *type_str = (ctx->type < HOOK_TYPE_COUNT)
        ? type_names[ctx->type] : "UNKNOWN";

    return snprintf(buf, bufsize,
        "{"
        "\"type\":\"%s\","
        "\"hook_name\":\"%s\","
        "\"source_daemon\":\"%s\","
        "\"operation\":\"%s\","
        "\"input_data_len\":%zu,"
        "\"output_data_len\":%zu,"
        "\"timestamp_ns\":%llu,"
        "\"trace_id\":\"%s\","
        "\"session_id\":\"%s\""
        "}",
        type_str,
        ctx->hook_name ? ctx->hook_name : "",
        ctx->source_daemon ? ctx->source_daemon : "",
        ctx->operation ? ctx->operation : "",
        ctx->input_data_len,
        ctx->output_data_len,
        (unsigned long long)ctx->timestamp_ns,
        ctx->trace_id,
        ctx->session_id);
}

/* ==================== 单个 Hook 执行 ==================== */

hook_decision_t hook_executor_run_one(const hook_entry_t *entry,
                                       hook_context_t *ctx,
                                       uint64_t *out_duration_ns)
{
    if (!entry || !ctx) {
        if (out_duration_ns) *out_duration_ns = 0;
        return HOOK_DECISION_CONTINUE;
    }

    /* P3.18 fix: 将 entry 的 hook_name 和 user_data 注入 ctx，使回调能访问。
     *
     * 根因：调用者（agentos_hook_trigger）传入的 ctx 通常不含 user_data
     * （user_data 是注册时与 callback 绑定的，存储在 entry->user_data 中）。
     * 多个 hook 共享同一 ctx 时，每个 hook 执行前必须更新这两个字段，
     * 否则回调从 ctx->user_data 读取的是 NULL 或上一个 hook 的 user_data。
     *
     * 同步更新 hook_name，使回调内可识别当前执行的 hook（审计/日志），
     * 且 hook_executor_ctx_to_json 序列化时能输出正确的 hook_name。 */
    ctx->hook_name = entry->name;
    ctx->user_data = entry->user_data;

    hook_decision_t decision = HOOK_DECISION_CONTINUE;

    switch (entry->impl_type) {
    case HOOK_IMPL_CALLBACK: {
        if (!entry->callback) break;

        /* 带超时保护执行 */
        decision = hook_timeout_run(entry, ctx, 0, out_duration_ns);
        break;
    }
    case HOOK_IMPL_SHELL: {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int exit_code = hook_executor_run_shell(entry->script_path, ctx);

        clock_gettime(CLOCK_MONOTONIC, &end);
        if (out_duration_ns) {
            *out_duration_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL
                             + (uint64_t)(end.tv_nsec - start.tv_nsec);
        }

        switch (exit_code) {
        case 0: decision = HOOK_DECISION_CONTINUE; break;
        case 1: decision = HOOK_DECISION_SKIP;     break;
        case 2: decision = HOOK_DECISION_RETRY;    break;
        case 3: decision = HOOK_DECISION_ABORT;    break;
        case 4: decision = HOOK_DECISION_MODIFY;   break;
        default: decision = HOOK_DECISION_CONTINUE; break;
        }
        break;
    }
    case HOOK_IMPL_PYTHON: {
        /* Python 脚本：fork + execve /usr/bin/python3（不经过 shell，无注入风险） */
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int exit_code = hook_run_script("/usr/bin/python3", entry->script_path,
                                        ctx, HOOK_SHELL_TIMEOUT_SEC);

        clock_gettime(CLOCK_MONOTONIC, &end);
        if (out_duration_ns) {
            *out_duration_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL
                             + (uint64_t)(end.tv_nsec - start.tv_nsec);
        }

        switch (exit_code) {
        case 0: decision = HOOK_DECISION_CONTINUE; break;
        case 1: decision = HOOK_DECISION_SKIP;     break;
        case 2: decision = HOOK_DECISION_RETRY;    break;
        case 3: decision = HOOK_DECISION_ABORT;    break;
        case 4: decision = HOOK_DECISION_MODIFY;   break;
        default: decision = HOOK_DECISION_CONTINUE; break;
        }
        break;
    }
    case HOOK_IMPL_WEBHOOK: {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        int result = hook_executor_run_webhook(entry->script_path, ctx);

        clock_gettime(CLOCK_MONOTONIC, &end);
        if (out_duration_ns) {
            *out_duration_ns = (uint64_t)(end.tv_sec - start.tv_sec) * 1000000000ULL
                             + (uint64_t)(end.tv_nsec - start.tv_nsec);
        }

        decision = (result == 0) ? HOOK_DECISION_CONTINUE : HOOK_DECISION_ABORT;
        break;
    }
    default:
        if (out_duration_ns) *out_duration_ns = 0;
        break;
    }

    /* 更新统计 */
    uint64_t duration = out_duration_ns ? *out_duration_ns : 0;
    hook_registry_update_stats(entry->name, decision, duration);

    return decision;
}

/* ==================== Hook 链执行 ==================== */

hook_decision_t hook_executor_run(hook_context_t *ctx, hook_exec_mode_t mode)
{
    if (!ctx) return HOOK_DECISION_CONTINUE;

    /* 获取该类型的所有已启用 Hook */
    hook_entry_t *entries[HOOK_REGISTRY_MAX];
    size_t count = 0;

    if (hook_registry_get_by_type(ctx->type, entries,
                                   HOOK_REGISTRY_MAX, &count) != 0) {
        return HOOK_DECISION_CONTINUE;
    }

    if (count == 0)
        return HOOK_DECISION_CONTINUE;

    hook_decision_t final_decision = HOOK_DECISION_CONTINUE;

    /* 顺序执行 */
    for (size_t i = 0; i < count; i++) {
        uint64_t duration_ns = 0;
        hook_decision_t decision = hook_executor_run_one(entries[i], ctx,
                                                          &duration_ns);

        final_decision = hook_executor_merge_decision(final_decision, decision);

        /* 如果 ABORT，立即停止执行后续 Hook */
        if (final_decision == HOOK_DECISION_ABORT)
            break;
    }

    (void)mode; /* 并行模式在 Phase 3 实现 */

    return final_decision;
}

/* ==================== 子进程执行基础设施 ==================== */

char **hook_build_env_with_context(const char *context_json)
{
    if (!context_json)
        return NULL;

    /* 统计 environ 条目数（继承父进程环境，保持与原 system() 行为一致） */
    extern char **environ;
    char **ep = environ;
    size_t env_count = 0;
    while (ep && *ep) { env_count++; ep++; }

    /* 分配 env_count + 2（AGENTOS_HOOK_CONTEXT + NULL 终止符） */
    char **env = (char **)AGENTOS_MALLOC(sizeof(char *) * (env_count + 2));
    if (!env) return NULL;

    size_t i = 0;
    for (ep = environ; ep && *ep; ep++) {
        env[i] = AGENTOS_STRDUP(*ep);
        if (!env[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(env[j]);
            AGENTOS_FREE(env);
            return NULL;
        }
        i++;
    }

    /* 追加 AGENTOS_HOOK_CONTEXT=<json>（环境变量传递，不拼入命令行 — 无注入风险） */
    static const char prefix[] = "AGENTOS_HOOK_CONTEXT=";
    const size_t prefix_len = sizeof(prefix) - 1;
    const size_t json_len = strlen(context_json);
    env[i] = (char *)AGENTOS_MALLOC(prefix_len + json_len + 1);
    if (!env[i]) {
        for (size_t j = 0; j < i; j++) AGENTOS_FREE(env[j]);
        AGENTOS_FREE(env);
        return NULL;
    }
    __builtin_memcpy(env[i], prefix, prefix_len);
    __builtin_memcpy(env[i] + prefix_len, context_json, json_len);
    env[i][prefix_len + json_len] = '\0';
    i++;

    env[i] = NULL;
    return env;
}

void hook_free_env(char **env)
{
    if (!env) return;
    for (size_t i = 0; env[i]; i++) AGENTOS_FREE(env[i]);
    AGENTOS_FREE(env);
}

int hook_run_subprocess(const char *const argv[], char **envp, int timeout_sec)
{
    if (!argv || !argv[0]) return -1;

#ifndef _WIN32
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        /* 子进程：重定向 stdout/stderr 到管道写端，关闭读端 */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        /* envp != NULL → execve（自定义环境 + 完整路径）
         * envp == NULL → execvp（PATH 搜索 + 继承 environ） */
        if (envp) {
            execve(argv[0], (char *const *)argv, envp);
        } else {
            execvp(argv[0], (char *const *)argv);
        }
        /* exec 失败：退出码 127（对齐 shell 约定） */
        _exit(127);
    }

    /* 父进程：关闭写端，持续读取输出直到 EOF 或超时（防止管道阻塞致挂起） */
    close(pipefd[1]);

    time_t start = time(NULL);
    char discard[4096];
    int timed_out = 0;
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(pipefd[0], &rfds);
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int retval = select(pipefd[0] + 1, &rfds, NULL, NULL, &tv);
        if (retval > 0) {
            ssize_t n = read(pipefd[0], discard, sizeof(discard));
            if (n <= 0) break; /* EOF（子进程退出）或错误 */
        } else if (retval == 0) {
            /* 1 秒无数据：检查总超时 */
            if (timeout_sec > 0 && (time(NULL) - start) >= timeout_sec) {
                timed_out = 1;
                kill(pid, SIGKILL);
                /* 排空管道以避免子进程写端阻塞（已 SIGKILL，fd 即将关闭） */
                while (read(pipefd[0], discard, sizeof(discard)) > 0) { /* drain */ }
                break;
            }
            /* 继续等待 */
        } else {
            if (errno == EINTR) continue;
            break; /* select 出错 */
        }
    }

    close(pipefd[0]);

    /* 回收子进程（阻塞等待，子进程已被 EOF 或 SIGKILL 终止） */
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) break;
    }

    if (timed_out) return -2;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return -1;
#else  /* _WIN32 */
    /* Windows 无 fork/execve/pipe/select/waitpid，且无安全的轻量方案可等价实现
     * "超时 + 输出捕获 + SIGKILL 强杀" 语义（需 overlapped I/O 或读线程排空
     * 管道以防子进程写满阻塞，复杂度高且易引入句柄泄漏/死锁）。
     * 为避免引入不安全的桩实现（违反 BAN-211/235 安全合规与"禁桩函数"约束），
     * 此处明确拒绝执行并在日志中指明替代路径。调用方（hook_run_script /
     * hook_run_shell）会处理 -1 返回值（记录失败并返回错误）。 */
    (void)envp;
    (void)timeout_sec;
    SVC_LOG_ERROR("hook_executor: subprocess execution not supported on Windows, "
                  "use tool_d IPC instead (fork/execve unavailable)");
    return -1;
#endif /* _WIN32 */
}

int hook_run_script(const char *interpreter, const char *script_path,
                    hook_context_t *ctx, int timeout_sec)
{
    if (!interpreter || !script_path || !ctx) return -1;

    /* 序列化上下文为 JSON（通过环境变量传递，不拼入命令行） */
    char json_buf[4096];
    int json_len = hook_executor_ctx_to_json(ctx, json_buf, sizeof(json_buf));
    if (json_len < 0) return -1;

    char **env = hook_build_env_with_context(json_buf);
    if (!env) {
        SVC_LOG_ERROR("hook_executor: failed to build env for script '%s'", script_path);
        return -1;
    }

    const char *const argv[] = {interpreter, script_path, NULL};
    int exit_code = hook_run_subprocess(argv, env, timeout_sec);
    hook_free_env(env);

    if (exit_code == -2) {
        SVC_LOG_WARN("hook_executor: script hook '%s' timed out (%ds)", script_path, timeout_sec);
        return -1;
    }
    if (exit_code < 0) {
        SVC_LOG_ERROR("hook_executor: failed to launch interpreter '%s' for '%s'",
                      interpreter, script_path);
        return -1;
    }
    return exit_code;
}

/* ==================== Shell 执行 ==================== */

int hook_executor_run_shell(const char *script_path, hook_context_t *ctx)
{
    /* fork + execve /bin/sh，上下文经 AGENTOS_HOOK_CONTEXT 环境变量传递 */
    return hook_run_script("/bin/sh", script_path, ctx, HOOK_SHELL_TIMEOUT_SEC);
}

/* ==================== Webhook 执行 ==================== */

#ifdef AGENTOS_HAS_CURL
/* libcurl 响应丢弃回调（仅消费响应体，webhook 不需要返回数据） */
static size_t hook_webhook_discard_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    (void)ptr;
    (void)userdata;
    return size * nmemb;
}

/* libcurl 真实 HTTP POST 实现 */
static int hook_webhook_post_curl(const char *url, const char *json_body)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        SVC_LOG_ERROR("hook_executor: curl_easy_init failed for webhook '%s'", url);
        return -1;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, hook_webhook_discard_cb);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)HOOK_WEBHOOK_TIMEOUT_SEC);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)HOOK_WEBHOOK_TIMEOUT_SEC);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        SVC_LOG_WARN("hook_executor: webhook POST '%s' failed: %s", url, curl_easy_strerror(res));
        return -1;
    }
    if (http_code < 200 || http_code >= 300) {
        SVC_LOG_WARN("hook_executor: webhook '%s' returned HTTP %ld", url, http_code);
        return -1;
    }
    return 0;
}
#endif /* AGENTOS_HAS_CURL */

int hook_executor_run_webhook(const char *url, hook_context_t *ctx)
{
    if (!url || !ctx) return -1;

    /* 序列化上下文为 JSON（作为 POST body） */
    char json_buf[4096];
    int json_len = hook_executor_ctx_to_json(ctx, json_buf, sizeof(json_buf));
    if (json_len < 0) return -1;

#ifdef AGENTOS_HAS_CURL
    /* 首选：libcurl 真实 HTTP POST（进程内，无外部二进制依赖） */
    return hook_webhook_post_curl(url, json_buf);
#else
    /* 后备：fork + execvp curl 二进制（argv 数组传参，无 shell 注入风险） */
    const char *const argv[] = {
        "curl", "-s", "-S",
        "-X", "POST",
        "-H", "Content-Type: application/json",
        "-d", json_buf,
        "--max-time", "5",
        "-o", "/dev/null",
        url,
        NULL
    };
    int exit_code = hook_run_subprocess(argv, NULL, HOOK_WEBHOOK_TIMEOUT_SEC + 5);
    if (exit_code == -2) {
        SVC_LOG_WARN("hook_executor: webhook curl '%s' timed out", url);
        return -1;
    }
    if (exit_code < 0) {
        SVC_LOG_ERROR("hook_executor: failed to launch curl for webhook '%s'", url);
        return -1;
    }
    if (exit_code != 0) {
        SVC_LOG_WARN("hook_executor: webhook curl '%s' exit=%d", url, exit_code);
        return -1;
    }
    return 0;
#endif
}