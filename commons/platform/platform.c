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

#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if AGENTOS_PLATFORM_WINDOWS
    #include <io.h>
    #include <direct.h>
    #include <process.h>
    #include <bcrypt.h>
    #pragma comment(lib, "bcrypt.lib")
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <signal.h>
    #include <errno.h>
#endif

/* ==================== 网络初始化 ==================== */

int agentos_network_init(void) {
#if AGENTOS_PLATFORM_WINDOWS
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

void agentos_network_cleanup(void) {
#if AGENTOS_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

void agentos_ignore_sigpipe(void) {
#ifndef AGENTOS_PLATFORM_WINDOWS
    signal(SIGPIPE, SIG_IGN);
#endif
}

/* ==================== 线程实现 ==================== */

uint64_t agentos_thread_id(void) {
#if AGENTOS_PLATFORM_WINDOWS
    return (uint64_t)GetCurrentThreadId();
#else
    return (uint64_t)pthread_self();
#endif
}

#if AGENTOS_PLATFORM_WINDOWS

int agentos_thread_create(agentos_thread_t* thread, agentos_thread_func_t func, void* arg) {
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
    if (h == NULL) {
        return (int)GetLastError();
    }
    *thread = h;
    return 0;
}

int agentos_thread_join(agentos_thread_t thread, void** retval) {
    (void)retval;
    DWORD result = WaitForSingleObject(thread, INFINITE);
    if (result != WAIT_OBJECT_0) {
        return -1;
    }
    CloseHandle(thread);
    return 0;
}

#else

int agentos_thread_create(agentos_thread_t* thread, agentos_thread_func_t func, void* arg) {
    return pthread_create(thread, NULL, func, arg);
}

int agentos_thread_join(agentos_thread_t thread, void** retval) {
    return pthread_join(thread, retval);
}

#endif

/* ==================== 互斥锁实现 ==================== */

#if AGENTOS_PLATFORM_WINDOWS

int agentos_mutex_init(agentos_mutex_t* mutex) {
    InitializeCriticalSection(mutex);
    return 0;
}

int agentos_mutex_lock(agentos_mutex_t* mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    return TryEnterCriticalSection(mutex) ? 0 : -1;
}

int agentos_mutex_unlock(agentos_mutex_t* mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

void agentos_mutex_destroy(agentos_mutex_t* mutex) {
    DeleteCriticalSection(mutex);
}

#else

int agentos_mutex_init(agentos_mutex_t* mutex) {
    return pthread_mutex_init(mutex, NULL);
}

int agentos_mutex_lock(agentos_mutex_t* mutex) {
    return pthread_mutex_lock(mutex);
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    return pthread_mutex_trylock(mutex);
}

int agentos_mutex_unlock(agentos_mutex_t* mutex) {
    return pthread_mutex_unlock(mutex);
}

void agentos_mutex_destroy(agentos_mutex_t* mutex) {
    pthread_mutex_destroy(mutex);
}

#endif

/* ==================== 条件变量实现 ==================== */

#if AGENTOS_PLATFORM_WINDOWS

int agentos_cond_init(agentos_cond_t* cond) {
    InitializeConditionVariable(cond);
    return 0;
}

int agentos_cond_wait(agentos_cond_t* cond, agentos_mutex_t* mutex) {
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
}

int agentos_cond_timedwait(agentos_cond_t* cond, agentos_mutex_t* mutex, uint32_t timeout_ms) {
    BOOL result = SleepConditionVariableCS(cond, mutex, timeout_ms);
    if (!result) {
        DWORD err = GetLastError();
        if (err == ERROR_TIMEOUT) {
            return -2;
        }
        return -1;
    }
    return 0;
}

int agentos_cond_signal(agentos_cond_t* cond) {
    WakeConditionVariable(cond);
    return 0;
}

int agentos_cond_broadcast(agentos_cond_t* cond) {
    WakeAllConditionVariable(cond);
    return 0;
}

void agentos_cond_destroy(agentos_cond_t* cond) {
    (void)cond;
}

#else

int agentos_cond_init(agentos_cond_t* cond) {
    return pthread_cond_init(cond, NULL);
}

int agentos_cond_wait(agentos_cond_t* cond, agentos_mutex_t* mutex) {
    return pthread_cond_wait(cond, mutex);
}

int agentos_cond_timedwait(agentos_cond_t* cond, agentos_mutex_t* mutex, uint32_t timeout_ms) {
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
        return -2;
    }
    return ret;
}

int agentos_cond_signal(agentos_cond_t* cond) {
    return pthread_cond_signal(cond);
}

int agentos_cond_broadcast(agentos_cond_t* cond) {
    return pthread_cond_broadcast(cond);
}

void agentos_cond_destroy(agentos_cond_t* cond) {
    pthread_cond_destroy(cond);
}

#endif

/* ==================== Socket 实现 ==================== */

agentos_socket_t agentos_socket_tcp(void) {
#if AGENTOS_PLATFORM_WINDOWS
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#else
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif
}

agentos_socket_t agentos_socket_unix(void) {
#ifndef AGENTOS_PLATFORM_WINDOWS
    return socket(AF_UNIX, SOCK_STREAM, 0);
#else
    return AGENTOS_INVALID_SOCKET;
#endif
}

int agentos_socket_set_nonblock(agentos_socket_t sock, int nonblock) {
#if AGENTOS_PLATFORM_WINDOWS
    u_long mode = nonblock ? 1 : 0;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return -1;
    if (nonblock) {
        return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    } else {
        return fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
    }
#endif
}

int agentos_socket_set_reuseaddr(agentos_socket_t sock, int reuse) {
    int opt = reuse ? 1 : 0;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
}

void agentos_socket_close(agentos_socket_t sock) {
#if AGENTOS_PLATFORM_WINDOWS
    closesocket(sock);
#else
    close(sock);
#endif
}

/* ==================== 进程实现 ==================== */

#if AGENTOS_PLATFORM_WINDOWS

static HANDLE g_process_handle = NULL;
static HANDLE g_thread_handle = NULL;

int agentos_process_start(const char* executable, char* const argv[],
                         char* const envp[], agentos_process_info_t* proc) {
    (void)envp;

    if (!proc) return -1;
    memset(proc, 0, sizeof(agentos_process_info_t));

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    memset(&pi, 0, sizeof(pi));

    char cmdline[4096] = {0};
    snprintf(cmdline, sizeof(cmdline), "\"%s\"", executable);
    for (int i = 1; argv && argv[i]; i++) {
        size_t remaining = sizeof(cmdline) - strlen(cmdline);
        if (remaining > 0) {
            snprintf(cmdline + strlen(cmdline), remaining, " \"%s\"", argv[i]);
        }
    }

    BOOL success = CreateProcessA(
        NULL, cmdline, NULL, NULL, TRUE,
        CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
        NULL, NULL, &si, &pi
    );

    if (!success) {
        return -1;
    }

    g_process_handle = pi.hProcess;
    g_thread_handle = pi.hThread;
    proc->pid = pi.dwProcessId;

    return 0;
}

int agentos_process_wait(agentos_process_info_t* proc, uint32_t timeout_ms, int* exit_code) {
    (void)proc;

    if (!g_process_handle) return -1;

    DWORD result = WaitForSingleObject(g_process_handle, timeout_ms == 0 ? INFINITE : timeout_ms);
    if (result == WAIT_TIMEOUT) {
        return -2;
    }
    if (result != WAIT_OBJECT_0) {
        return -1;
    }

    DWORD code;
    if (!GetExitCodeProcess(g_process_handle, &code)) {
        return -1;
    }

    if (exit_code) {
        *exit_code = (int)code;
    }

    CloseHandle(g_process_handle);
    CloseHandle(g_thread_handle);
    g_process_handle = NULL;
    g_thread_handle = NULL;

    return 0;
}

int agentos_process_kill(agentos_process_info_t* proc) {
    (void)proc;
    if (g_process_handle) {
        return TerminateProcess(g_process_handle, 1) ? 0 : -1;
    }
    return -1;
}

void agentos_process_close_pipes(agentos_process_info_t* proc) {
    (void)proc;
    if (g_thread_handle) {
        CloseHandle(g_thread_handle);
        g_thread_handle = NULL;
    }
    if (g_process_handle) {
        CloseHandle(g_process_handle);
        g_process_handle = NULL;
    }
}

#else

int agentos_process_start(const char* executable, char* const argv[],
                         char* const envp[], agentos_process_info_t* proc) {
    if (!proc) return -1;
    memset(proc, 0, sizeof(agentos_process_info_t));

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        if (envp) {
            for (int i = 0; envp[i]; i++) {
                putenv(envp[i]);
            }
        }
        execvp(executable, argv);
        _exit(127);
    }

    proc->pid = pid;
    return 0;
}

int agentos_process_wait(agentos_process_info_t* proc, uint32_t timeout_ms, int* exit_code) {
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
            return -2;
        }
    }

    if (result < 0) {
        return -1;
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

int agentos_process_kill(agentos_process_info_t* proc) {
    return kill(proc->pid, SIGKILL);
}

void agentos_process_close_pipes(agentos_process_info_t* proc) {
    (void)proc;
}

#endif

/* ==================== 时间接口 ==================== */

uint64_t agentos_time_ns(void) {
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

uint64_t agentos_time_ms(void) {
    return agentos_time_ns() / 1000000ULL;
}

/* ==================== 随机数接口 ==================== */

static AGENTOS_THREAD_LOCAL unsigned int g_random_seed = 0;
static AGENTOS_THREAD_LOCAL int g_random_initialized = 0;

void agentos_random_init(void) {
    if (!g_random_initialized) {
        g_random_seed = (unsigned int)agentos_time_ns();
        g_random_initialized = 1;
    }
}

uint32_t agentos_random_uint32(uint32_t min, uint32_t max) {
    if (!g_random_initialized) {
        agentos_random_init();
    }

#if AGENTOS_PLATFORM_WINDOWS
    return min + (uint32_t)((double)rand() / (RAND_MAX + 1.0) * (max - min + 1));
#else
    return min + (uint32_t)((double)rand_r(&g_random_seed) / (RAND_MAX + 1.0) * (max - min + 1));
#endif
}

float agentos_random_float(void) {
    if (!g_random_initialized) {
        agentos_random_init();
    }

#if AGENTOS_PLATFORM_WINDOWS
    return (float)rand() / (float)RAND_MAX;
#else
    return (float)rand_r(&g_random_seed) / (float)RAND_MAX;
#endif
}

int agentos_random_bytes(void* buf, size_t len) {
#if AGENTOS_PLATFORM_WINDOWS
    NTSTATUS status = BCryptGenRandom(
        NULL,
        (PUCHAR)buf,
        (ULONG)len,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );
    return status == 0 ? 0 : -1;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, (char*)buf + total, len - total);
        if (n <= 0) {
            close(fd);
            return -1;
        }
        total += (size_t)n;
    }

    close(fd);
    return 0;
#endif
}

/* ==================== 文件系统接口 ==================== */

int agentos_file_exists(const char* path) {
    if (!path) return 0;
#if AGENTOS_PLATFORM_WINDOWS
    return _access(path, 0) == 0 ? 1 : 0;
#else
    return access(path, F_OK) == 0 ? 1 : 0;
#endif
}

int agentos_mkdir_p(const char* path) {
    if (!path) return -1;

    char* tmp = strdup(path);
    if (!tmp) return -1;

    size_t len = strlen(tmp);
    if (len > 0 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
    }

    for (char* p = tmp + 1; *p; p++) {
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

    free(tmp);
    return (ret == 0 || errno == EEXIST) ? 0 : -1;
}

int64_t agentos_file_size(const char* path) {
    if (!path) return -1;
#if AGENTOS_PLATFORM_WINDOWS
    struct _stat st;
    if (_stat(path, &st) != 0) {
        return -1;
    }
    return st.st_size;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return st.st_size;
#endif
}

/* ==================== 字符串工具 ==================== */

char* agentos_strlcpy(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0 || !src) {
        return dest;
    }

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return dest;
}

char* agentos_strlcat(char* dest, size_t dest_size, const char* src) {
    if (!dest || dest_size == 0 || !src) {
        return dest;
    }

    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size - 1) {
        return dest;
    }

    size_t src_len = strlen(src);
    size_t remaining = dest_size - dest_len - 1;
    size_t copy_len = (src_len < remaining) ? src_len : remaining;

    memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';

    return dest;
}

/* ==================== 错误处理 ==================== */

int agentos_get_last_error(void) {
#if AGENTOS_PLATFORM_WINDOWS
    return (int)GetLastError();
#else
    return errno;
#endif
}

const char* agentos_strerror(int error) {
#if AGENTOS_PLATFORM_WINDOWS
    static char msg[256];
    memset(msg, 0, sizeof(msg));
    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, (DWORD)error, 0, msg, sizeof(msg), NULL
    );
    return msg;
#else
    return strerror(error);
#endif
}
