/**
 * @file platform.c
 * @brief 跨平台兼容层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供统一的跨平台抽象层：
 * - 线程与互斥锁
 * - 条件变量
 * - Socket 网络通信
 * - 进程管理
 * - 时间与随机数
 * - 文件系统操作
 */

/* 1. POSIX标准头文件（必须最先包含，确保特性测试宏生效） */
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/* 2. C标准库头文件 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#include <bcrypt.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#define strdup _strdup
#define access _access /* flawfinder: ignore */
#ifndef EEXIST
#define EEXIST 17
#endif
#pragma comment(lib, "bcrypt.lib")
#else
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

/* 2. 项目头文件（最后包含，避免覆盖系统定义） */
#include "error.h"
#include "platform.h"

#include <string.h>
#include "memory_compat.h"



/* ==================== 网络初始化 ==================== */

int agentos_network_init(void)
{
#if AGENTOS_PLATFORM_WINDOWS
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

void agentos_network_cleanup(void)
{
#if AGENTOS_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

void agentos_ignore_sigpipe(void)
{
#ifndef AGENTOS_PLATFORM_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif
}

/* ==================== 线程实现 ==================== */

uint64_t agentos_thread_id(void)
{
#if AGENTOS_PLATFORM_WINDOWS
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

#if AGENTOS_PLATFORM_WINDOWS

int agentos_platform_thread_create(agentos_thread_t *thread, agentos_thread_func_t func, void *arg)
{
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    if (h == NULL) {
        return (int)GetLastError();
    }
    *thread = h;
    return 0;
}

int agentos_platform_thread_join(agentos_thread_t thread, void **retval)
{
    (void)retval;
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return AGENTOS_EINVAL;
    }
    CloseHandle(thread);
    return 0;
}

#else

int agentos_platform_thread_create(agentos_thread_t *thread, agentos_thread_func_t func, void *arg)
{
    return pthread_create(thread, NULL, func, arg);
}

int agentos_platform_thread_join(agentos_thread_t thread, void **retval)
{
    return pthread_join(thread, retval);
}

int agentos_platform_thread_detach(agentos_thread_t thread)
{
    return pthread_detach(thread);
}

#endif

/* ==================== 互斥锁实现 ==================== */

#if AGENTOS_PLATFORM_WINDOWS

int agentos_mutex_init(agentos_mutex_t *mutex)
{
    InitializeCriticalSection(mutex);
    return 0;
}

int agentos_mutex_lock(agentos_mutex_t *mutex)
{
    EnterCriticalSection(mutex);
    return 0;
}

int agentos_mutex_trylock(agentos_mutex_t *mutex)
{
    return TryEnterCriticalSection(mutex) ? 0 : -1;
}

int agentos_mutex_unlock(agentos_mutex_t *mutex)
{
    LeaveCriticalSection(mutex);
    return 0;
}

void agentos_mutex_destroy(agentos_mutex_t *mutex)
{
    DeleteCriticalSection(mutex);
}

agentos_mutex_t *agentos_mutex_create(void)
{
    agentos_mutex_t *mutex = (agentos_mutex_t *)AGENTOS_MALLOC(sizeof(agentos_mutex_t));
    if (mutex) {
        InitializeCriticalSection(mutex);
    }
    return mutex;
}

void agentos_mutex_free(agentos_mutex_t *mutex)
{
    if (mutex) {
        DeleteCriticalSection(mutex);
        AGENTOS_FREE(mutex);
    }
}

#else

int agentos_mutex_init(agentos_mutex_t *mutex)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    int ret = pthread_mutex_init(mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    return ret;
}

int agentos_mutex_lock(agentos_mutex_t *mutex)
{
    return pthread_mutex_lock(mutex);
}

int agentos_mutex_trylock(agentos_mutex_t *mutex)
{
    return pthread_mutex_trylock(mutex);
}

int agentos_mutex_unlock(agentos_mutex_t *mutex)
{
    return pthread_mutex_unlock(mutex);
}

void agentos_mutex_destroy(agentos_mutex_t *mutex)
{
    pthread_mutex_destroy(mutex);
}

agentos_mutex_t *agentos_mutex_create(void)
{
    agentos_mutex_t *mutex = (agentos_mutex_t *)AGENTOS_MALLOC(sizeof(agentos_mutex_t));
    if (mutex) {
        pthread_mutex_init(mutex, NULL);
    }
    return mutex;
}

void agentos_mutex_free(agentos_mutex_t *mutex)
{
    if (mutex) {
        pthread_mutex_destroy(mutex);
        AGENTOS_FREE(mutex);
    }
}

#endif

/* ==================== 条件变量实现 ==================== */

#if AGENTOS_PLATFORM_WINDOWS

int agentos_cond_init(agentos_cond_t *cond)
{
    InitializeConditionVariable(cond);
    return 0;
}

int agentos_cond_wait(agentos_cond_t *cond, agentos_mutex_t *mutex)
{
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

int agentos_cond_timedwait(agentos_cond_t *cond, agentos_mutex_t *mutex, uint32_t timeout_ms)
{
    BOOL result = SleepConditionVariableCS(cond, mutex, timeout_ms);
    if (!result) {
        DWORD err = GetLastError();
        if (err == ERROR_TIMEOUT) {
            return AGENTOS_ERR_TIMEOUT;
        }
        return AGENTOS_EINVAL;
    }
    return 0;
}

int agentos_cond_signal(agentos_cond_t *cond)
{
    WakeConditionVariable(cond);
    return 0;
}

int agentos_cond_broadcast(agentos_cond_t *cond)
{
    WakeAllConditionVariable(cond);
    return 0;
}

void agentos_cond_destroy(agentos_cond_t *cond)
{
    (void)cond;
}

agentos_cond_t *agentos_cond_create(void)
{
    agentos_cond_t *cond = (agentos_cond_t *)AGENTOS_MALLOC(sizeof(agentos_cond_t));
    if (cond) {
        InitializeConditionVariable(cond);
    }
    return cond;
}

void agentos_cond_free(agentos_cond_t *cond)
{
    if (cond) {
        AGENTOS_FREE(cond);
    }
}

#else

int agentos_cond_init(agentos_cond_t *cond)
{
    return pthread_cond_init(cond, NULL);
}

int agentos_cond_wait(agentos_cond_t *cond, agentos_mutex_t *mutex)
{
    return pthread_cond_wait(cond, mutex);
}

int agentos_cond_timedwait(agentos_cond_t *cond, agentos_mutex_t *mutex, uint32_t timeout_ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    int ret = pthread_cond_timedwait(cond, mutex, &ts);
    if (ret == ETIMEDOUT) {
        return AGENTOS_ERR_TIMEOUT;
    }
    return ret;
}

int agentos_cond_signal(agentos_cond_t *cond)
{
    return pthread_cond_signal(cond);
}

int agentos_cond_broadcast(agentos_cond_t *cond)
{
    return pthread_cond_broadcast(cond);
}

void agentos_cond_destroy(agentos_cond_t *cond)
{
    pthread_cond_destroy(cond);
}

agentos_cond_t *agentos_cond_create(void)
{
    agentos_cond_t *cond = (agentos_cond_t *)AGENTOS_MALLOC(sizeof(agentos_cond_t));
    if (cond) {
        pthread_cond_init(cond, NULL);
    }
    return cond;
}

void agentos_cond_free(agentos_cond_t *cond)
{
    if (cond) {
        pthread_cond_destroy(cond);
        AGENTOS_FREE(cond);
    }
}

#endif

/* ==================== Socket 实现 ==================== */

agentos_socket_t agentos_socket_tcp(void)
{
#if AGENTOS_PLATFORM_WINDOWS
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
}

agentos_socket_t agentos_socket_unix(void)
{
#ifndef AGENTOS_PLATFORM_WINDOWS
    return socket(AF_UNIX, SOCK_STREAM, 0);
#else
    return AGENTOS_INVALID_SOCKET;
#endif
}

int agentos_socket_set_nonblock(agentos_socket_t sock, int nonblock)
{
#if AGENTOS_PLATFORM_WINDOWS
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
        return AGENTOS_EINVAL;
    if (nonblock) {
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    } else {
        return fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
}

int agentos_socket_set_reuseaddr(agentos_socket_t sock, int reuse)
{
    int opt = reuse ? 1 : 0;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
}

void agentos_socket_close(agentos_socket_t sock)
{
#if AGENTOS_PLATFORM_WINDOWS
    closesocket(sock);
#else
    close(sock);
#endif
}

/* ==================== 进程实现 ==================== */

#if AGENTOS_PLATFORM_WINDOWS

int agentos_process_start(const char *executable, char *const argv[], char *const envp[],
                          agentos_process_info_t *proc)
{
    (void)envp;

    if (!proc)
        return AGENTOS_EINVAL;
    AGENTOS_MEMSET(proc, 0, sizeof(agentos_process_info_t));

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    AGENTOS_MEMSET(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    AGENTOS_MEMSET(&pi, 0, sizeof(pi));

    char cmdline[4096] = {0};
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", executable);
    for (int i = 1; argv && argv[i]; i++) {
        size_t remaining = sizeof(cmdline) - strlen(cmdline);
        if (remaining > 0) {
            snprintf(cmdline + strlen(cmdline), remaining, " \"%s\"", argv[i]);
        }
    }

    BOOL success =
        CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
                       NULL, NULL, &si, &pi);

    if (!success) {
        return AGENTOS_EINVAL;
    }

    /* 将句柄存入 proc 而非全局变量，支持同时跟踪多个子进程且线程安全 */
    proc->process_handle = (void *)pi.hProcess;
    proc->thread_handle = (void *)pi.hThread;
    proc->pid = pi.dwProcessId;

    return 0;
}

int agentos_process_wait(agentos_process_info_t *proc, uint32_t timeout_ms, int *exit_code)
{
    if (!proc || !proc->process_handle)
        return AGENTOS_EINVAL;

    HANDLE h_process = (HANDLE)proc->process_handle;
    DWORD result = WaitForSingleObject(h_process, timeout_ms == 0 ? INFINITE : timeout_ms);
    if (result == WAIT_TIMEOUT) {
        return AGENTOS_ERR_TIMEOUT;
    }
    if (result != WAIT_OBJECT_0) {
        return AGENTOS_EINVAL;
    }

    DWORD code;
    if (!GetExitCodeProcess(h_process, &code)) {
        return AGENTOS_EINVAL;
    }

    if (exit_code) {
        *exit_code = (int)code;
    }

    CloseHandle(h_process);
    if (proc->thread_handle) {
        CloseHandle((HANDLE)proc->thread_handle);
        proc->thread_handle = NULL;
    }
    proc->process_handle = NULL;

    return 0;
}

int agentos_process_kill(agentos_process_info_t *proc)
{
    if (!proc || !proc->process_handle)
        return AGENTOS_EINVAL;
    return TerminateProcess((HANDLE)proc->process_handle, 1) ? 0 : -1;
}

void agentos_process_close_pipes(agentos_process_info_t *proc)
{
    if (!proc)
        return;
    if (proc->thread_handle) {
        CloseHandle((HANDLE)proc->thread_handle);
        proc->thread_handle = NULL;
    }
    if (proc->process_handle) {
        CloseHandle((HANDLE)proc->process_handle);
        proc->process_handle = NULL;
    }
}

int agentos_process_run_capture(const char *executable, char *const argv[],
                                char *const envp[], uint32_t timeout_ms,
                                char *output, size_t output_size)
{
    (void)envp;
    (void)timeout_ms;
    /* Windows 无 fork/execvp，重建命令字符串并用 _popen 执行（与现有 Windows 代码模式一致）。
     * executable 和 argv 由调用者控制（非任意用户输入），注入风险可控。 */
    char cmdline[4096] = {0};
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", executable);
    for (int i = 1; argv && argv[i]; i++) {
        size_t remaining = sizeof(cmdline) - strlen(cmdline);
        if (remaining > 0)
            snprintf(cmdline + strlen(cmdline), remaining, " \"%s\"", argv[i]);
    }

    FILE *pipe = _popen(cmdline, "r");
    if (!pipe)
        return -1;

    size_t offset = 0;
    if (output && output_size > 0) {
        char buf[4096];
        while (offset + 1 < output_size && fgets(buf, sizeof(buf), pipe)) {
            size_t len = strlen(buf);
            if (offset + len >= output_size)
                len = output_size - 1 - offset;
            __builtin_memcpy(output + offset, buf, len);
            offset += len;
        }
        output[offset] = '\0';
    }

    int result = _pclose(pipe);
    return result;
}

#else

int agentos_process_start(const char *executable, char *const argv[], char *const envp[],
                          agentos_process_info_t *proc)
{
    if (!proc)
        return AGENTOS_EINVAL;
    AGENTOS_MEMSET(proc, 0, sizeof(agentos_process_info_t));
    proc->stdin_fd = -1;
    proc->stdout_fd = -1;
    proc->stderr_fd = -1;

    /* 创建 stdout/stderr 管道以捕获子进程输出（补全 agentos_process_info_t 设计意图） */
    int stdout_pipe[2];
    int stderr_pipe[2];
    if (pipe(stdout_pipe) < 0)
        return AGENTOS_EINVAL;
    if (pipe(stderr_pipe) < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return AGENTOS_EINVAL;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return AGENTOS_EINVAL;
    }

    if (pid == 0) {
        /* 子进程：重定向 stdout/stderr 到管道写端，关闭所有读端 */
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        if (envp) {
            for (int i = 0; envp[i]; i++) {
                putenv(envp[i]);
            }
        }
        /* flawfinder: ignore - executable and argv are caller-controlled, not arbitrary user input
         */
        execvp(executable, argv);
        _exit(127);
    }

    /* 父进程：关闭写端，保留读端供调用者读取输出 */
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    proc->pid = pid;
    proc->stdout_fd = stdout_pipe[0];
    proc->stderr_fd = stderr_pipe[0];
    return 0;
}

int agentos_process_wait(agentos_process_info_t *proc, uint32_t timeout_ms, int *exit_code)
{
    int status;
    pid_t result;

    if (timeout_ms == 0) {
        result = waitpid(proc->pid, &status, 0);
    } else {
        struct timespec ts;
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;

        sigset_t mask, old_mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGCHLD);
        sigprocmask(SIG_BLOCK, &mask, &old_mask);

        do {
            result = waitpid(proc->pid, &status, WNOHANG);
            if (result == 0) {
                nanosleep(&ts, NULL);
            }
        } while (result == 0 && ts.tv_sec > 0);

        sigprocmask(SIG_SETMASK, &old_mask, NULL);

        if (result == 0) {
            return AGENTOS_ERR_TIMEOUT;
        }
    }

    if (result < 0) {
        return AGENTOS_EINVAL;
    }

    if (WIFEXITED(status)) {
        if (exit_code) {
            *exit_code = WEXITSTATUS(status);
        }
    } else if (WIFSIGNALED(status)) {
        if (exit_code) {
            *exit_code = -WTERMSIG(status);
        }
    }

    return 0;
}

int agentos_process_kill(agentos_process_info_t *proc)
{
    return kill(proc->pid, SIGKILL);
}

void agentos_process_close_pipes(agentos_process_info_t *proc)
{
    if (!proc)
        return;
    if (proc->stdout_fd >= 0) {
        close(proc->stdout_fd);
        proc->stdout_fd = -1;
    }
    if (proc->stderr_fd >= 0) {
        close(proc->stderr_fd);
        proc->stderr_fd = -1;
    }
    if (proc->stdin_fd >= 0) {
        close(proc->stdin_fd);
        proc->stdin_fd = -1;
    }
}

int agentos_process_run_capture(const char *executable, char *const argv[],
                                char *const envp[], uint32_t timeout_ms,
                                char *output, size_t output_size)
{
    agentos_process_info_t proc;
    if (agentos_process_start(executable, argv, envp, &proc) != 0)
        return -1;

    /* 读取 stdout + stderr 合并输出（select 多路复用，防止管道写满阻塞子进程） */
    size_t offset = 0;
    if (output && output_size > 0)
        output[0] = '\0';

    time_t start = time(NULL);
    int timed_out = 0;

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int max_fd = -1;
        if (proc.stdout_fd >= 0) {
            FD_SET(proc.stdout_fd, &rfds);
            if (proc.stdout_fd > max_fd) max_fd = proc.stdout_fd;
        }
        if (proc.stderr_fd >= 0) {
            FD_SET(proc.stderr_fd, &rfds);
            if (proc.stderr_fd > max_fd) max_fd = proc.stderr_fd;
        }
        if (max_fd < 0)
            break; /* 所有管道已 EOF，子进程已退出 */

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        int retval = select(max_fd + 1, &rfds, NULL, NULL, &tv);
        if (retval > 0) {
            char buf[4096];
            if (proc.stdout_fd >= 0 && FD_ISSET(proc.stdout_fd, &rfds)) {
                ssize_t n = read(proc.stdout_fd, buf, sizeof(buf));
                if (n <= 0) {
                    close(proc.stdout_fd);
                    proc.stdout_fd = -1;
                } else if (output && offset + 1 < output_size) {
                    size_t copy = (offset + (size_t)n < output_size) ? (size_t)n
                                                                     : output_size - 1 - offset;
                    __builtin_memcpy(output + offset, buf, copy);
                    offset += copy;
                }
            }
            if (proc.stderr_fd >= 0 && FD_ISSET(proc.stderr_fd, &rfds)) {
                ssize_t n = read(proc.stderr_fd, buf, sizeof(buf));
                if (n <= 0) {
                    close(proc.stderr_fd);
                    proc.stderr_fd = -1;
                } else if (output && offset + 1 < output_size) {
                    size_t copy = (offset + (size_t)n < output_size) ? (size_t)n
                                                                     : output_size - 1 - offset;
                    __builtin_memcpy(output + offset, buf, copy);
                    offset += copy;
                }
            }
        } else if (retval == 0) {
            /* 1 秒无数据：检查总超时 */
            if (timeout_ms > 0 &&
                (uint32_t)((time(NULL) - start) * 1000) >= timeout_ms) {
                timed_out = 1;
                agentos_process_kill(&proc);
                /* SIGKILL 后子进程 fd 关闭，select 将返回 EOF，循环自然退出 */
            }
        } else if (errno != EINTR) {
            break; /* select 出错 */
        }
    }

    agentos_process_close_pipes(&proc);

    int exit_code = -1;
    /* 阻塞等待回收子进程（此时子进程已退出或被 SIGKILL） */
    agentos_process_wait(&proc, 0, &exit_code);

    if (output && output_size > 0)
        output[offset] = '\0';
    return timed_out ? -2 : exit_code;
}

#endif

/* ==================== 时间接口 ==================== */

uint64_t agentos_time_ns(void)
{
#if AGENTOS_PLATFORM_WINDOWS
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

uint64_t agentos_time_ms(void)
{
    return agentos_time_ns() / 1000000ULL;
}

void agentos_sleep_ms(uint32_t ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

/* ==================== 随机数接口 ==================== */

static AGENTOS_THREAD_LOCAL unsigned int g_random_seed = 0;
static AGENTOS_THREAD_LOCAL int g_random_initialized = 0;

void agentos_random_init(void)
{
    if (!g_random_initialized) {
        g_random_seed = (unsigned int)agentos_time_ns();
        g_random_initialized = 1;
    }
}

uint32_t agentos_random_uint32(uint32_t min, uint32_t max)
{
    if (!g_random_initialized) {
        agentos_random_init();
    }

#if AGENTOS_PLATFORM_WINDOWS
    uint32_t rnd;
    BCryptGenRandom(NULL, (PUCHAR)&rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return min + (uint32_t)((rnd / 4294967296.0) * (max - min + 1));
#else
    return min + (uint32_t)((double)rand_r(&g_random_seed) / (RAND_MAX + 1.0) * (max - min + 1));
#endif
}

float agentos_random_float(void)
{
    if (!g_random_initialized) {
        agentos_random_init();
    }

#if AGENTOS_PLATFORM_WINDOWS
    uint32_t rnd;
    BCryptGenRandom(NULL, (PUCHAR)&rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return rnd / 4294967296.0f;
#else
    return (float)rand_r(&g_random_seed) / (float)RAND_MAX;
#endif
}

int agentos_random_bytes(void *buf, size_t len)
{
#if AGENTOS_PLATFORM_WINDOWS
    NTSTATUS status =
        BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return status == 0 ? 0 : -1;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return AGENTOS_EINVAL;
    }

    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (char *)buf + total, len - total);
        if (n <= 0) {
            close(fd);
            return AGENTOS_EINVAL;
        }
        total += (size_t)n;
    }

    close(fd);
    return 0;
#endif
}

/* ==================== 文件系统接口 ==================== */

int agentos_file_exists(const char *path)
{
    if (!path)
        return 0;
#if AGENTOS_PLATFORM_WINDOWS
    struct _stat st;
    return _stat(path, &st) == 0 ? 1 : 0;
#else
    struct stat st;
    return stat(path, &st) == 0 ? 1 : 0;
#endif
}

int agentos_mkdir_p(const char *path)
{
    if (!path)
        return AGENTOS_EINVAL;

    char *tmp = AGENTOS_STRDUP(path);
    if (!tmp)
        return AGENTOS_EINVAL;

    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
    }

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';

#if AGENTOS_PLATFORM_WINDOWS
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif

            *p = saved;
        }
    }

#if AGENTOS_PLATFORM_WINDOWS
    int ret = _mkdir(tmp);
#else
    int ret = mkdir(tmp, 0755);
#endif

    AGENTOS_FREE(tmp);
    return (ret == 0 || errno == EEXIST) ? 0 : -1;
}

int64_t agentos_file_size(const char *path)
{
    if (!path)
        return AGENTOS_EINVAL;
#if AGENTOS_PLATFORM_WINDOWS
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return AGENTOS_EINVAL;
    }
    return st.st_size;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return AGENTOS_EINVAL;
    }
    return st.st_size;
#endif
}

/* ==================== 字符串工具 ==================== */

int agentos_strlcpy(char *dest, const char *src, size_t dest_size)
{
    if (!dest || dest_size == 0 || !src) {
        return AGENTOS_EINVAL;
    }

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

__builtin_memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return (int)copy_len;
}

int agentos_strlcat(char *dest, const char *src, size_t dest_size)
{
    if (!dest || dest_size == 0 || !src) {
        return AGENTOS_EINVAL;
    }

    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) {
        return AGENTOS_EINVAL;
    }

    size_t src_len = strlen(src);
    size_t remaining = dest_size - dest_len - 1;
    size_t copy_len = (src_len < remaining) ? src_len : remaining;

__builtin_memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';

    return (int)copy_len;
}

/* ==================== 错误处理 ==================== */

int agentos_get_last_error(void)
{
#if AGENTOS_PLATFORM_WINDOWS
    return (int)GetLastError();
#else
    return errno;
#endif
}

const char *agentos_strerror(int error)
{
#if AGENTOS_PLATFORM_WINDOWS
    static char msg[256];
    AGENTOS_MEMSET(msg, 0, sizeof(msg));
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, (DWORD)error,
                   0, msg, sizeof(msg), NULL);
    return msg;
#else
    return strerror(error);
#endif
}
