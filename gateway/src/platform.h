/**
 * @file platform.h
 * @brief 跨平台兼容层
 * 
 * 提供统一的线程、互斥锁、条件变量等原语，
 * 支持 Windows 和 POSIX (Linux/macOS) 平台。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef DYNAMIC_PLATFORM_H
#define DYNAMIC_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <errno.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 线程类型 ========== */

#ifdef _WIN32
typedef HANDLE platform_thread_t;
typedef DWORD platform_thread_id_t;
#define PLATFORM_THREAD_INVALID NULL
#else
typedef pthread_t platform_thread_t;
typedef pthread_t platform_thread_id_t;
#define PLATFORM_THREAD_INVALID 0
#endif

/* ========== 互斥锁类型 ========== */

#ifdef _WIN32
typedef CRITICAL_SECTION platform_mutex_t;
#else
typedef pthread_mutex_t platform_mutex_t;
#endif

/* ========== 条件变量类型 ========== */

#ifdef _WIN32
typedef CONDITION_VARIABLE platform_cond_t;
#else
typedef pthread_cond_t platform_cond_t;
#endif

/* ========== 时间类型 ========== */

#ifdef _WIN32
typedef struct {
    LARGE_INTEGER interval;
} platform_timespec_t;
#else
typedef struct timespec platform_timespec_t;
#endif

/* ========== 互斥锁操作 ========== */

/**
 * @brief 初始化互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
static inline int platform_mutex_init(platform_mutex_t* mutex) {
#ifdef _WIN32
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL);
#endif
}

/**
 * @brief 销毁互斥锁
 * @param mutex 互斥锁指针
 */
static inline void platform_mutex_destroy(platform_mutex_t* mutex) {
#ifdef _WIN32
    DeleteCriticalSection(mutex);
#else
    pthread_mutex_destroy(mutex);
#endif
}

/**
 * @brief 加锁
 * @param mutex 互斥锁指针
 */
static inline void platform_mutex_lock(platform_mutex_t* mutex) {
#ifdef _WIN32
    EnterCriticalSection(mutex);
#else
    pthread_mutex_lock(mutex);
#endif
}

/**
 * @brief 解锁
 * @param mutex 互斥锁指针
 */
static inline void platform_mutex_unlock(platform_mutex_t* mutex) {
#ifdef _WIN32
    LeaveCriticalSection(mutex);
#else
    pthread_mutex_unlock(mutex);
#endif
}

/* ========== 条件变量操作 ========== */

/**
 * @brief 初始化条件变量
 * @param cond 条件变量指针
 * @return 0 成功
 */
static inline int platform_cond_init(platform_cond_t* cond) {
#ifdef _WIN32
    InitializeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_init(cond, NULL);
#endif
}

/**
 * @brief 销毁条件变量
 * @param cond 条件变量指针
 */
static inline void platform_cond_destroy(platform_cond_t* cond) {
#ifdef _WIN32
    /* Windows CONDITION_VARIABLE 不需要显式销毁 */
    (void)cond;
#else
    pthread_cond_destroy(cond);
#endif
}

/**
 * @brief 等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 */
static inline void platform_cond_wait(platform_cond_t* cond, platform_mutex_t* mutex) {
#ifdef _WIN32
    SleepConditionVariableCS(cond, mutex, INFINITE);
#else
    pthread_cond_wait(cond, mutex);
#endif
}

/**
 * @brief 带超时的等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @param timeout_ms 超时时间（毫秒），0 表示无限等待
 * @return 0 成功，ETIMEDOUT 超时
 */
static inline int platform_cond_timedwait(platform_cond_t* cond, 
    platform_mutex_t* mutex, uint32_t timeout_ms) {
#ifdef _WIN32
    DWORD ms = (timeout_ms == 0) ? INFINITE : timeout_ms;
    BOOL success = SleepConditionVariableCS(cond, mutex, ms);
    if (!success && GetLastError() == ERROR_TIMEOUT) {
        return ETIMEDOUT;
    }
    return success ? 0 : -1;
#else
    if (timeout_ms == 0) {
        return pthread_cond_wait(cond, mutex);
    }
    
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    return pthread_cond_timedwait(cond, mutex, &ts);
#endif
}

/**
 * @brief 唤醒一个等待线程
 * @param cond 条件变量指针
 */
static inline void platform_cond_signal(platform_cond_t* cond) {
#ifdef _WIN32
    WakeConditionVariable(cond);
#else
    pthread_cond_signal(cond);
#endif
}

/**
 * @brief 唤醒所有等待线程
 * @param cond 条件变量指针
 */
static inline void platform_cond_broadcast(platform_cond_t* cond) {
#ifdef _WIN32
    WakeAllConditionVariable(cond);
#else
    pthread_cond_broadcast(cond);
#endif
}

/* ========== 线程操作 ========== */

/**
 * @brief 线程函数类型
 */
#ifdef _WIN32
typedef DWORD WINAPI platform_thread_func_t(LPVOID arg);
#else
typedef void* platform_thread_func_t(void* arg);
#endif

/**
 * @brief 创建线程
 * @param thread 线程句柄指针
 * @param func 线程函数
 * @param arg 线程参数
 * @return 0 成功，非0 失败
 */
static inline int platform_thread_create(platform_thread_t* thread,
    platform_thread_func_t* func, void* arg) {
#ifdef _WIN32
    *thread = CreateThread(NULL, 0, func, arg, 0, NULL);
    return (*thread != NULL) ? 0 : -1;
#else
    return pthread_create(thread, NULL, func, arg);
#endif
}

/**
 * @brief 等待线程结束
 * @param thread 线程句柄
 * @return 0 成功
 */
static inline int platform_thread_join(platform_thread_t thread) {
#ifdef _WIN32
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    return 0;
#else
    return pthread_join(thread, NULL);
#endif
}

/**
 * @brief 获取当前线程ID
 * @return 线程ID
 */
static inline platform_thread_id_t platform_thread_self(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}

/* ========== Socket 兼容性 ========== */

#ifdef _WIN32
typedef SOCKET platform_socket_t;
#define PLATFORM_SOCKET_INVALID INVALID_SOCKET
#define PLATFORM_SOCKET_ERROR SOCKET_ERROR
#define platform_close_socket(s) closesocket(s)
#else
typedef int platform_socket_t;
#define PLATFORM_SOCKET_INVALID (-1)
#define PLATFORM_SOCKET_ERROR (-1)
#define platform_close_socket(s) close(s)
#endif

/**
 * @brief 初始化网络库（Windows需要）
 * @return 0 成功
 */
static inline int platform_network_init(void) {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

/**
 * @brief 清理网络库（Windows需要）
 */
static inline void platform_network_cleanup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

/* ========== 信号处理 ========== */

#ifdef _WIN32
/* Windows 不支持 SIGPIPE，无需处理 */
#define platform_ignore_sigpipe() ((void)0)
#else
#include <signal.h>
static inline void platform_ignore_sigpipe(void) {
    signal(SIGPIPE, SIG_IGN);
}
#endif

/* ========== 字符串函数兼容性 ========== */

#ifdef _WIN32
#define strcasestr _strnicmp_strstr

static inline char* _strnicmp_strstr(const char* haystack, const char* needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0) return (char*)haystack;
    
    size_t haystack_len = strlen(haystack);
    while (haystack_len >= needle_len) {
        if (_strnicmp(haystack, needle, needle_len) == 0) {
            return (char*)haystack;
        }
        haystack++;
        haystack_len--;
    }
    return NULL;
}
#endif

/* ========== getopt 兼容性（Windows） ========== */

#ifdef _WIN32
/* Windows 需要手动实现或使用第三方库 */
extern char* optarg;
extern int optind, opterr, optopt;

int getopt_long(int argc, char* const argv[], const char* optstring,
    const struct option* longopts, int* longindex);

struct option {
    const char* name;
    int has_arg;
    int* flag;
    int val;
};

#define no_argument       0
#define required_argument 1
#define optional_argument 2
#endif

/* ========== 时间工具函数 ========== */

/**
 * @brief 获取当前时间（纳秒）
 * @return 当前时间纳秒数
 */
uint64_t time_ns(void);

#ifdef __cplusplus
}
#endif

#endif /* DYNAMIC_PLATFORM_H */
