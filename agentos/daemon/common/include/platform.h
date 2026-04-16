// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file platform.h
 * @brief 平台抽象兼容层（daemon 专用）
 *
 * 本文件是 agentos/commons/platform 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include <platform.h>
 *
 * @see agentos/commons/platform/include/platform.h
 */

#ifndef AGENTOS_DAEMON_COMMON_PLATFORM_H
#define AGENTOS_DAEMON_COMMON_PLATFORM_H

/* ==================== 系统头文件（必须在 commons platform.h 之前） ==================== */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(_WIN32) || defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define AGENTOS_PLATFORM_WINDOWS 1
#define AGENTOS_PLATFORM_POSIX   0
#else
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#define AGENTOS_PLATFORM_WINDOWS 0
#define AGENTOS_PLATFORM_POSIX   1
#endif

/* ==================== 包含 commons 的统一平台抽象层 ==================== */
#include <platform.h>
#include <compat.h>
#include <atomic_compat.h>

/* ==================== 兼容性别名 ==================== */

/* 旧 API 命名兼容映射（daemon 代码使用 agentos_platform_ 前缀） */
typedef agentos_mutex_t agentos_platform_mutex_t;
#define agentos_platform_mutex_init    agentos_mutex_init
#define agentos_platform_mutex_lock    agentos_mutex_lock
#define agentos_platform_mutex_unlock  agentos_mutex_unlock
#define agentos_platform_mutex_destroy agentos_mutex_destroy
#define agentos_platform_get_time_ms   agentos_time_ms

/* 线程本地存储兼容 */
#ifndef AGENTOS_THREAD_LOCAL
#define AGENTOS_THREAD_LOCAL _Thread_local
#endif

/* 内联函数兼容 */
#ifndef AGENTOS_INLINE
#define AGENTOS_INLINE inline
#endif

/* 未使用参数标记 */
#ifndef AGENTOS_UNUSED
#define AGENTOS_UNUSED(x) (void)(x)
#endif

/* ==================== 类型兼容 ==================== */

typedef agentos_mutex_t* agentos_mutex_handle_t;
typedef agentos_cond_t* agentos_cond_handle_t;
typedef agentos_cond_t* agentos_platform_cond_t;
typedef agentos_thread_t* agentos_thread_handle_t;
typedef agentos_socket_t* agentos_socket_handle_t;

/* ==================== 额外类型定义 ==================== */

typedef struct {
    uint64_t seconds;
    uint32_t nanoseconds;
} agentos_timestamp_t;

typedef struct {
    int exit_code;
    int signal;
} agentos_process_status_t;

typedef enum {
    AGENTOS_AF_INET,
    AGENTOS_AF_INET6,
    AGENTOS_AF_UNIX
} agentos_address_family_t;

typedef enum {
    AGENTOS_SOCK_STREAM,
    AGENTOS_SOCK_DGRAM,
    AGENTOS_SOCK_SEQPACKET
} agentos_socket_type_t;

typedef struct {
    agentos_address_family_t family;
    uint16_t port;
    union {
        uint8_t ipv4[4];
        uint8_t ipv6[16];
        char path[108];
    } addr;
} agentos_sockaddr_t;

/* ==================== 兼容性函数包装 ==================== */

static inline int agentos_time_now(agentos_timestamp_t* ts) {
    if (!ts) return -1;
    uint64_t ns = agentos_time_ns();
    ts->seconds = ns / 1000000000ULL;
    ts->nanoseconds = (uint32_t)(ns % 1000000000ULL);
    return 0;
}

static inline int agentos_time_monotonic(agentos_timestamp_t* ts) {
    return agentos_time_now(ts);
}

static inline uint64_t agentos_time_to_ms(const agentos_timestamp_t* ts) {
    if (!ts) return 0;
    return ts->seconds * 1000ULL + ts->nanoseconds / 1000000ULL;
}

static inline void agentos_time_from_ms(uint64_t ms, agentos_timestamp_t* ts) {
    if (!ts) return;
    ts->seconds = ms / 1000ULL;
    ts->nanoseconds = (uint32_t)((ms % 1000ULL) * 1000000ULL);
}

static inline void agentos_sleep_ms(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static inline uint32_t agentos_process_self(void) {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

static inline uint64_t agentos_thread_self(void) {
    return agentos_thread_id();
}

static inline int agentos_thread_setname(const char* name) {
    (void)name;
    return 0;
}

static inline int agentos_thread_getname(char* name, size_t size) {
    if (name && size > 0) {
        name[0] = '\0';
    }
    return 0;
}

/* ==================== 文件系统兼容 ==================== */

static inline int agentos_mkdir(const char* path, int recursive) {
    if (recursive) {
        return agentos_mkdir_p(path);
    }
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) ? 0 : -1;
#else
    return mkdir(path, 0755);
#endif
}

/* ==================== 动态库兼容 ==================== */

typedef void* agentos_dl_t;

static inline agentos_dl_t agentos_dl_open(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static inline int agentos_dl_close(agentos_dl_t dl) {
#ifdef _WIN32
    return FreeLibrary((HMODULE)dl) ? 0 : -1;
#else
    return dlclose(dl);
#endif
}

static inline void* agentos_dl_sym(agentos_dl_t dl, const char* name) {
#ifdef _WIN32
    return GetProcAddress((HMODULE)dl, name);
#else
    return dlsym(dl, name);
#endif
}

static inline const char* agentos_dl_error(void) {
#ifdef _WIN32
    static char error_buf[256];
    DWORD err = GetLastError();
    if (err) {
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, error_buf, sizeof(error_buf), NULL);
        return error_buf;
    }
    return NULL;
#else
    return dlerror();
#endif
}

/* ==================== 系统信息兼容 ==================== */

typedef struct {
    char os_name[64];
    char os_version[64];
    char hostname[64];
    uint32_t cpu_count;
    uint64_t memory_total;
    uint64_t memory_free;
} agentos_sysinfo_t;

static inline int agentos_get_sysinfo(agentos_sysinfo_t* info) {
    if (!info) return -1;
    memset(info, 0, sizeof(agentos_sysinfo_t));
#ifdef _WIN32
    strncpy(info->os_name, "Windows", sizeof(info->os_name) - 1);
    info->os_name[sizeof(info->os_name) - 1] = '\0';
    DWORD size = sizeof(info->hostname);
    GetComputerNameA(info->hostname, &size);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info->cpu_count = si.dwNumberOfProcessors;
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    info->memory_total = ms.ullTotalPhys;
    info->memory_free = ms.ullAvailPhys;
#else
    struct utsname uts;
    uname(&uts);
    strncpy(info->os_name, uts.sysname, sizeof(info->os_name) - 1);
    info->os_name[sizeof(info->os_name) - 1] = '\0';
    strncpy(info->os_version, uts.release, sizeof(info->os_version) - 1);
    info->os_version[sizeof(info->os_version) - 1] = '\0';
    gethostname(info->hostname, sizeof(info->hostname) - 1);
    info->cpu_count = (uint32_t)sysconf(_SC_NPROCESSORS_ONLN);
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info->memory_total = (uint64_t)si.totalram * si.mem_unit;
        info->memory_free = (uint64_t)si.freeram * si.mem_unit;
    }
#endif
    return 0;
}

/* ==================== 原子操作兼容 ==================== */

typedef struct {
    volatile int value;
} agentos_atomic_int_t;

#ifndef ATOMIC_COMPAT_HAS_32
#define ATOMIC_COMPAT_HAS_32
static inline long atomic_load_32(volatile long* ptr, long order) {
    (void)order;
    return __sync_val_compare_and_swap(ptr, 0, 0);
}
static inline void atomic_store_32(volatile long* ptr, long val, long order) {
    (void)order;
    __sync_lock_test_and_set(ptr, val);
}
static inline long atomic_fetch_add_32(volatile long* ptr, long val, long order) {
    (void)order;
    return __sync_add_and_fetch(ptr, val);
}
static inline long atomic_fetch_sub_32(volatile long* ptr, long val, long order) {
    (void)order;
    return __sync_sub_and_fetch(ptr, val);
}
#endif

static inline int agentos_atomic_load(agentos_atomic_int_t* atomic) {
    if (!atomic) return 0;
    return atomic_load_32((long*)&atomic->value, memory_order_seq_cst);
}

static inline void agentos_atomic_store(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return;
    atomic_store_32((long*)&atomic->value, (long)value, memory_order_seq_cst);
}

static inline int agentos_atomic_fetch_add(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return 0;
    return (int)atomic_fetch_add_32((long*)&atomic->value, (long)value, memory_order_seq_cst);
}

static inline int agentos_atomic_fetch_sub(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return 0;
    return (int)atomic_fetch_sub_32((long*)&atomic->value, (long)value, memory_order_seq_cst);
}

/* ==================== 服务器端 Socket 兼容 ==================== */

static inline int agentos_socket_init(void) {
#if AGENTOS_PLATFORM_WINDOWS
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0 ? 0 : -1;
#else
    return 0;
#endif
}

static inline void agentos_socket_cleanup(void) {
#if AGENTOS_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

static inline agentos_socket_t agentos_socket_create_tcp_server(const char* host, uint16_t port) {
    agentos_socket_t sock = agentos_socket_tcp();
    if (sock == AGENTOS_INVALID_SOCKET) return AGENTOS_INVALID_SOCKET;
    agentos_socket_set_reuseaddr(sock, 1);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (host && strlen(host) > 0) {
        inet_pton(AF_INET, host, &addr.sin_addr);
    } else {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        agentos_socket_close(sock);
        return AGENTOS_INVALID_SOCKET;
    }
    if (listen(sock, SOMAXCONN) != 0) {
        agentos_socket_close(sock);
        return AGENTOS_INVALID_SOCKET;
    }
    return sock;
}

#if AGENTOS_PLATFORM_POSIX
static inline agentos_socket_t agentos_socket_create_unix_server(const char* path) {
    agentos_socket_t sock = agentos_socket_unix();
    if (sock == AGENTOS_INVALID_SOCKET) return AGENTOS_INVALID_SOCKET;
    unlink(path);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    addr.sun_path[sizeof(addr.sun_path) - 1] = '\0';
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        agentos_socket_close(sock);
        return AGENTOS_INVALID_SOCKET;
    }
    if (listen(sock, SOMAXCONN) != 0) {
        agentos_socket_close(sock);
        return AGENTOS_INVALID_SOCKET;
    }
    return sock;
}
#endif

static inline agentos_socket_t agentos_socket_accept(agentos_socket_t server_fd, uint32_t timeout_ms) {
#if AGENTOS_PLATFORM_WINDOWS
    if (!ConnectNamedPipe((HANDLE)server_fd, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_LISTENING && timeout_ms > 0) {
            Sleep(timeout_ms);
            if (ConnectNamedPipe((HANDLE)server_fd, NULL)) {
                return server_fd;
            }
        }
        return AGENTOS_INVALID_SOCKET;
    }
    return server_fd;
#else
    if (timeout_ms == 0) {
        return accept(server_fd, NULL, NULL);
    }
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int sel = select((int)(server_fd + 1), &read_fds, NULL, NULL, &tv);
    if (sel <= 0) return AGENTOS_INVALID_SOCKET;
    return accept(server_fd, NULL, NULL);
#endif
}

static inline ssize_t agentos_socket_recv(agentos_socket_t sock, void* buf, size_t len) {
#if AGENTOS_PLATFORM_WINDOWS
    if (GetFileType((HANDLE)sock) == FILE_TYPE_PIPE) {
        DWORD bytesRead = 0;
        BOOL success = ReadFile((HANDLE)sock, buf, (DWORD)len, &bytesRead, NULL);
        return success ? (ssize_t)bytesRead : -1;
    }
    return recv(sock, (char*)buf, (int)len, 0);
#else
    return recv(sock, buf, len, 0);
#endif
}

static inline ssize_t agentos_socket_send(agentos_socket_t sock, const void* buf, size_t len) {
#if AGENTOS_PLATFORM_WINDOWS
    if (GetFileType((HANDLE)sock) == FILE_TYPE_PIPE) {
        DWORD bytesWritten = 0;
        BOOL success = WriteFile((HANDLE)sock, buf, (DWORD)len, &bytesWritten, NULL);
        return success ? (ssize_t)bytesWritten : -1;
    }
    return send(sock, (const char*)buf, (int)len, 0);
#else
    return send(sock, buf, len, 0);
#endif
}

#endif /* AGENTOS_DAEMON_COMMON_PLATFORM_H */
