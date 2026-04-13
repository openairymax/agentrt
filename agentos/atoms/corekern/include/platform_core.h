/*
 * platform_core.h - Core platform type definitions for corekern
 * Extracted from platform.h to avoid function declaration conflicts
 */

#ifndef AGENTOS_PLATFORM_CORE_H
#define AGENTOS_PLATFORM_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

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
#if defined(_WIN32) || defined(_WIN64)
    #ifdef AGENTOS_BUILDING_DLL
        #define AGENTOS_API __declspec(dllexport)
    #else
        #define AGENTOS_API __declspec(dllimport)
    #endif
#else
    #define AGENTOS_API __attribute__((visibility("default")))
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
#define AGENTOS_INVALID_MUTEX ((agentos_mutex_t)0)
#define AGENTOS_INVALID_SOCKET (-1)
#define AGENTOS_INVALID_PROCESS ((agentos_process_t)0)

#endif /* AGENTOS_PLATFORM_CORE_H */