/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file platform.h
 * @brief 跨平台兼容层 - 统一不同操作系统的API差异
 *
 * 支持平台：
 * - Linux (POSIX)
 * - macOS (Darwin)
 * - Windows (Win32/Win64)
 *
 * 设计原则：
 * - 单一职责：仅处理平台差异
 * - 零开销：内联函数 + 宏定义
 * - 类型安全：强类型封装
 *
 * @author Spharx AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 *
 * @note 线程安全：平台抽象层本身不涉及线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-4 跨平台一致性原则
 */

#ifndef AGENTOS_PLATFORM_H
#define AGENTOS_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
/* 注意：不在此处包含 <time.h>，因为项目的 corekern/include/time.h 会覆盖系统 time.h */
/* 需要使用 time.h 定义的代码（如 clockid_t, CLOCK_MONOTONIC）应在 .c 文件中 */
/* 在包含 platform.h 之前先包含系统的 <time.h> */

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 平台检测 ==================== */
#if defined(_WIN32) || defined(_WIN64)
    #define AGENTOS_PLATFORM_WINDOWS 1
    #define AGENTOS_PLATFORM_NAME "Windows"
    #if defined(_WIN64)
        #define AGENTOS_PLATFORM_BITS 64
    #else
        #define AGENTOS_PLATFORM_BITS 32
    #endif
    #define AGENTOS_PLATFORM_POSIX 0
#elif defined(__APPLE__) && defined(__MACH__)
    #define AGENTOS_PLATFORM_MACOS 1
    #define AGENTOS_PLATFORM_NAME "macOS"
    #define AGENTOS_PLATFORM_BITS 64
    #define AGENTOS_PLATFORM_POSIX 1
#elif defined(__linux__)
    #define AGENTOS_PLATFORM_LINUX 1
    #define AGENTOS_PLATFORM_NAME "Linux"
    #if defined(__x86_64__) || defined(__aarch64__)
        #define AGENTOS_PLATFORM_BITS 64
    #else
        #define AGENTOS_PLATFORM_BITS 32
    #endif
    #define AGENTOS_PLATFORM_POSIX 1
#else
    #error "Unsupported platform"
#endif

/* ==================== 导出宏定义 ==================== */
#include "export.h"

/* ==================== 线程局部存储 ==================== */
#if defined(_WIN32) || defined(_WIN64)
    #define AGENTOS_THREAD_LOCAL __declspec(thread)
#else
    #define AGENTOS_THREAD_LOCAL __thread
#endif

/* ==================== 内联函数 ==================== */
#if defined(_WIN32) || defined(_WIN64)
    #define AGENTOS_INLINE __forceinline
#else
    #define AGENTOS_INLINE static inline __attribute__((always_inline))
#endif

/* ==================== 未使用参数标记 ==================== */
#ifndef AGENTOS_UNUSED
#define AGENTOS_UNUSED(x) ((void)(x))
#endif

/* ==================== 路径分隔符 ==================== */
#if AGENTOS_PLATFORM_WINDOWS
    #define AGENTOS_PATH_SEP '\\'
    #define AGENTOS_PATH_SEP_STR "\\"
    #define AGENTOS_PATH_MAX 260
#else
    #define AGENTOS_PATH_SEP '/'
    #define AGENTOS_PATH_SEP_STR "/"
    #define AGENTOS_PATH_MAX 4096
#endif

/* ==================== 平台头文件包含 ==================== */
#if AGENTOS_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <time.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/un.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <signal.h>
#endif

/* ==================== 基础类型定义 ==================== */

#if AGENTOS_PLATFORM_WINDOWS
    typedef HANDLE agentos_thread_t;
    typedef DWORD agentos_thread_id_t;
    typedef CRITICAL_SECTION agentos_mutex_t;
    typedef CONDITION_VARIABLE agentos_cond_t;
    typedef DWORD agentos_pid_t;
    typedef SOCKET agentos_socket_t;
    typedef HANDLE agentos_process_t;
#else
    typedef pthread_t agentos_thread_t;
    typedef pthread_t agentos_thread_id_t;
    typedef pthread_mutex_t agentos_mutex_t;
    typedef pthread_cond_t agentos_cond_t;
    typedef pid_t agentos_pid_t;
    typedef int agentos_socket_t;
    typedef pid_t agentos_process_t;
#endif

#define AGENTOS_INVALID_THREAD ((agentos_thread_t)0)
#define AGENTOS_INVALID_MUTEX ((agentos_mutex_t){0})
#define AGENTOS_INVALID_SOCKET (-1)
#define AGENTOS_INVALID_PROCESS ((agentos_process_t)0)

/* ==================== 互斥锁接口 ==================== */

/**
 * @brief 初始化互斥锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int agentos_mutex_init(agentos_mutex_t* mutex);

/**
 * @brief 销毁互斥锁
 * @param mutex 互斥锁指针
 */
void agentos_mutex_destroy(agentos_mutex_t* mutex);

/**
 * @brief 加锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int agentos_mutex_lock(agentos_mutex_t* mutex);

/**
 * @brief 尝试加锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败或已锁定
 */
int agentos_mutex_trylock(agentos_mutex_t* mutex);

/**
 * @brief 解锁
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int agentos_mutex_unlock(agentos_mutex_t* mutex);

/**
 * @brief 动态创建互斥锁（分配内存并初始化）
 * @return 互斥锁指针，失败返回NULL
 */
agentos_mutex_t* agentos_mutex_create(void);

/**
 * @brief 动态销毁互斥锁（销毁并释放内存）
 * @param mutex 互斥锁指针
 */
void agentos_mutex_free(agentos_mutex_t* mutex);

/* ==================== 条件变量接口 ==================== */

/**
 * @brief 初始化条件变量
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int agentos_cond_init(agentos_cond_t* cond);

/**
 * @brief 销毁条件变量
 * @param cond 条件变量指针
 */
void agentos_cond_destroy(agentos_cond_t* cond);

/**
 * @brief 等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @return 0 成功，非0 失败
 */
int agentos_cond_wait(agentos_cond_t* cond, agentos_mutex_t* mutex);

/**
 * @brief 超时等待条件变量
 * @param cond 条件变量指针
 * @param mutex 互斥锁指针
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，非0 失败或超时
 */
int agentos_cond_timedwait(agentos_cond_t* cond, agentos_mutex_t* mutex, uint32_t timeout_ms);

/**
 * @brief 唤醒一个等待线程
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int agentos_cond_signal(agentos_cond_t* cond);

/**
 * @brief 唤醒所有等待线程
 * @param cond 条件变量指针
 * @return 0 成功，非0 失败
 */
int agentos_cond_broadcast(agentos_cond_t* cond);

/**
 * @brief 动态创建条件变量（分配内存并初始化）
 * @return 条件变量指针，失败返回NULL
 */
agentos_cond_t* agentos_cond_create(void);

/**
 * @brief 动态销毁条件变量（销毁并释放内存）
 * @param cond 条件变量指针
 */
void agentos_cond_free(agentos_cond_t* cond);

/* ==================== 线程接口 ==================== */

/**
 * @brief 线程函数类型
 */
typedef void* (*agentos_thread_func_t)(void* arg);

/**
 * @brief 创建线程
 * @param thread 线程句柄指针
 * @param func 线程函数
 * @param arg 线程参数
 * @return 0 成功，非0 失败
 */
int agentos_thread_create(agentos_thread_t* thread, agentos_thread_func_t func, void* arg);

/**
 * @brief 等待线程结束
 * @param thread 线程句柄
 * @param retval 返回值指针（可为NULL）
 * @return 0 成功，非0 失败
 */
int agentos_thread_join(agentos_thread_t thread, void** retval);

/**
 * @brief 获取当前线程ID
 * @return 线程ID
 */
uint64_t agentos_thread_id(void);

/* ==================== Socket 接口 ==================== */

/**
 * @brief 创建 TCP Socket
 * @return Socket 句柄，失败返回 AGENTOS_INVALID_SOCKET
 */
agentos_socket_t agentos_socket_tcp(void);

/**
 * @brief 创建 Unix Domain Socket（仅 POSIX）
 * @return Socket 句柄，失败返回 AGENTOS_INVALID_SOCKET
 */
agentos_socket_t agentos_socket_unix(void);

/**
 * @brief 关闭 Socket
 * @param sock Socket 句柄
 */
void agentos_socket_close(agentos_socket_t sock);

/**
 * @brief 设置 Socket 非阻塞模式
 * @param sock Socket 句柄
 * @param nonblock 是否非阻塞
 * @return 0 成功，非0 失败
 */
int agentos_socket_set_nonblock(agentos_socket_t sock, int nonblock);

/**
 * @brief 设置 Socket 复用地址
 * @param sock Socket 句柄
 * @param reuse 是否复用
 * @return 0 成功，非0 失败
 */
int agentos_socket_set_reuseaddr(agentos_socket_t sock, int reuse);

/* ==================== 进程接口 ==================== */

/**
 * @brief 进程信息结构
 */
typedef struct {
    agentos_pid_t pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
} agentos_process_info_t;

/**
 * @brief 启动进程
 * @param executable 可执行文件路径
 * @param argv 参数数组（以NULL结尾）
 * @param envp 环境变量数组（以NULL结尾，可为NULL）
 * @param proc 输出进程信息
 * @return 0 成功，非0 失败
 */
int agentos_process_start(const char* executable, char* const argv[], char* const envp[], agentos_process_info_t* proc);

/**
 * @brief 等待进程结束
 * @param proc 进程信息
 * @param timeout_ms 超时时间（毫秒），0表示无限等待
 * @param exit_code 输出退出码（可为NULL）
 * @return 0 成功，非0 失败或超时
 */
int agentos_process_wait(agentos_process_info_t* proc, uint32_t timeout_ms, int* exit_code);

/**
 * @brief 终止进程
 * @param proc 进程信息
 * @return 0 成功，非0 失败
 */
int agentos_process_kill(agentos_process_info_t* proc);

/**
 * @brief 关闭进程管道
 * @param proc 进程信息
 */
void agentos_process_close_pipes(agentos_process_info_t* proc);

/* ==================== 时间接口 ==================== */

/**
 * @brief 获取高精度时间戳（纳秒）
 * @return 时间戳
 */
uint64_t agentos_time_ns(void);

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 时间戳
 */
uint64_t agentos_time_ms(void);

/* ==================== 随机数接口 ==================== */

/**
 * @brief 初始化随机数生成器（线程安全）
 */
void agentos_random_init(void);

/**
 * @brief 生成随机数（线程安全）
 * @param min 最小值
 * @param max 最大值
 * @return 随机数
 */
uint32_t agentos_random_uint32(uint32_t min, uint32_t max);

/**
 * @brief 生成随机浮点数（线程安全）
 * @return 0.0 到 1.0 之间的随机数
 */
float agentos_random_float(void);

/**
 * @brief 生成随机字节（线程安全）
 * @param buf 缓冲区
 * @param len 长度
 * @return 0 成功，非0 失败
 */
int agentos_random_bytes(void* buf, size_t len);

/* ==================== 文件系统接口 ==================== */

/**
 * @brief 检查文件是否存在
 * @param path 文件路径
 * @return 1 存在，0 不存在
 */
int agentos_file_exists(const char* path);

/**
 * @brief 创建目录（递归）
 * @param path 目录路径
 * @return 0 成功，非0 失败
 */
int agentos_mkdir_p(const char* path);

/**
 * @brief 获取文件大小
 * @param path 文件路径
 * @return 文件大小，失败返回 -1
 */
int64_t agentos_file_size(const char* path);

/* ==================== 网络初始化接口 ==================== */

/**
 * @brief 初始化网络库（Windows需要）
 * @return 0 成功
 */
int agentos_network_init(void);

/**
 * @brief 清理网络库（Windows需要）
 */
void agentos_network_cleanup(void);

/* ==================== 信号处理接口 ==================== */

/**
 * @brief 忽略 SIGPIPE 信号
 */
void agentos_ignore_sigpipe(void);

/* ==================== 字符串工具 ==================== */

/**
 * @brief 安全的字符串复制
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src 源字符串
 * @return 目标字符串指针
 */
char* agentos_strlcpy(char* dest, size_t dest_size, const char* src);

/**
 * @brief 安全的字符串连接
 * @param dest 目标缓冲区
 * @param dest_size 目标缓冲区大小
 * @param src 源字符串
 * @return 目标字符串指针
 */
char* agentos_strlcat(char* dest, size_t dest_size, const char* src);

/* ==================== 错误处理接口 ==================== */

/**
 * @brief 获取最后错误的错误码
 * @return 错误码
 */
int agentos_get_last_error(void);

/**
 * @brief 获取错误描述字符串
 * @param error 错误码
 * @return 错误描述字符串
 */
const char* agentos_strerror(int error);

/* ==================== 系统信息类型 (UNI-01: 唯一定义) ==================== */

#ifndef AGENTOS_SYSINFO_T_DEFINED
#define AGENTOS_SYSINFO_T_DEFINED
typedef struct {
    char os_name[64];
    char os_version[64];
    char hostname[64];
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} agentos_sysinfo_t;
#endif

int agentos_get_sysinfo(agentos_sysinfo_t* info);

/* ==================== 原子操作类型 (UNI-01: 唯一定义) ==================== */

#ifndef AGENTOS_ATOMIC_INT_T_DEFINED
#define AGENTOS_ATOMIC_INT_T_DEFINED
typedef struct {
    volatile int value;
} agentos_atomic_int_t;
#endif

int agentos_atomic_load(agentos_atomic_int_t* atomic);
void agentos_atomic_store(agentos_atomic_int_t* atomic, int value);
int agentos_atomic_fetch_add(agentos_atomic_int_t* atomic, int value);
int agentos_atomic_fetch_sub(agentos_atomic_int_t* atomic, int value);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_PLATFORM_H */
