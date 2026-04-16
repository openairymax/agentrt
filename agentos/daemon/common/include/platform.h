// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file platform.h
 * @brief 平台抽象兼容层
 * 
 * 本文件是 agentos/commons/platform 的兼容层，提供向后兼容的 API。
 * 新代码应直接使用 #include "agentos/platform/platform.h"
 * 
 * @see agentos/commons/platform/include/platform.h
 */

#ifndef AGENTOS_DAEMON_COMMON_PLATFORM_H
#define AGENTOS_DAEMON_COMMON_PLATFORM_H

/* 包含 commons 的统一平台抽象层 */
#include "agentos/platform/platform.h"

/* 系统头文件 - 用于服务器端 Socket 函数 */
#if AGENTOS_PLATFORM_POSIX
#include <sys/un.h>      /* Unix Domain Socket */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#elif AGENTOS_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

/* ==================== 兼容性别名 ==================== */

/*
 * 平台检测宏兼容层
 * 注意: 这些宏由 agentos/commons/platform 平台检测系统定义。
 * 此处仅提供向后兼容的守卫，防止重定义警告。
 * 实际值来自 commons 的编译器定义 (-DAGENTOS_PLATFORM_xxx)。
 */

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

/* 路径分隔符兼容 */
#ifndef AGENTOS_PATH_SEP
#define AGENTOS_PATH_SEP AGENTOS_PATH_SEP
#endif
#ifndef AGENTOS_PATH_SEP_STR
#define AGENTOS_PATH_SEP_STR AGENTOS_PATH_SEP_STR
#endif
#ifndef AGENTOS_PATH_MAX
#define AGENTOS_PATH_MAX AGENTOS_PATH_MAX
#endif

/* ==================== 类型兼容 ==================== */

/* commons 使用直接 typedef，daemon 使用句柄类型 */
/* 为保持兼容，这里提供句柄类型的别名 */

/**
 * @brief 互斥锁句柄类型（兼容层）
 * @note commons 使用 agentos_mutex_t 作为直接类型
 */
typedef agentos_mutex_t* agentos_mutex_handle_t;

/**
 * @brief 条件变量句柄类型（兼容层）
 */
typedef agentos_cond_t* agentos_cond_handle_t;

/**
 * @brief 线程句柄类型（兼容层）
 */
typedef agentos_thread_t* agentos_thread_handle_t;

/**
 * @brief 套接字句柄类型（兼容层）
 */
typedef agentos_socket_t* agentos_socket_handle_t;

/* ==================== 额外类型定义 ==================== */

/**
 * @brief 时间戳结构（纳秒精度）
 * @note commons 使用 uint64_t 毫秒/纳秒，这里提供结构体形式
 */
typedef struct {
    uint64_t seconds;      /**< 秒 */
    uint32_t nanoseconds;  /**< 纳秒 */
} agentos_timestamp_t;

/**
 * @brief 进程退出状态
 */
typedef struct {
    int exit_code;         /**< 退出码 */
    int signal;            /**< 终止信号（-1表示正常退出） */
} agentos_process_status_t;

/**
 * @brief 套接字地址族
 */
typedef enum {
    AGENTOS_AF_INET,    /**< IPv4 */
    AGENTOS_AF_INET6,   /**< IPv6 */
    AGENTOS_AF_UNIX     /**< Unix域套接字 */
} agentos_address_family_t;

/**
 * @brief 套接字类型
 */
typedef enum {
    AGENTOS_SOCK_STREAM,   /**< 流式套接字（TCP） */
    AGENTOS_SOCK_DGRAM,    /**< 数据报套接字（UDP） */
    AGENTOS_SOCK_SEQPACKET /**< 有序数据报 */
} agentos_socket_type_t;

/**
 * @brief 套接字地址
 */
typedef struct {
    agentos_address_family_t family;  /**< 地址族 */
    uint16_t port;                    /**< 端口号 */
    union {
        uint8_t ipv4[4];              /**< IPv4 地址 */
        uint8_t ipv6[16];             /**< IPv6 地址 */
        char path[108];               /**< Unix 域套接字路径 */
    } addr;
} agentos_sockaddr_t;

/* ==================== 兼容性函数包装 ==================== */

/**
 * @brief 获取当前时间戳
 * @param ts [out] 时间戳输出
 * @return 0成功，非0失败
 */
static inline int agentos_time_now(agentos_timestamp_t* ts) {
    if (!ts) return -1;
    uint64_t ns = agentos_time_ns();
    ts->seconds = ns / 1000000000ULL;
    ts->nanoseconds = (uint32_t)(ns % 1000000000ULL);
    return 0;
}

/**
 * @brief 获取单调时间
 * @param ts [out] 时间戳输出
 * @return 0成功，非0失败
 */
static inline int agentos_time_monotonic(agentos_timestamp_t* ts) {
    /* 使用 agentos_time_ns 作为单调时间 */
    return agentos_time_now(ts);
}

/**
 * @brief 时间戳转换为毫秒
 * @param ts [in] 时间戳
 * @return 毫秒数
 */
static inline uint64_t agentos_time_to_ms(const agentos_timestamp_t* ts) {
    if (!ts) return 0;
    return ts->seconds * 1000ULL + ts->nanoseconds / 1000000ULL;
}

/**
 * @brief 毫秒转换为时间戳
 * @param ms [in] 毫秒数
 * @param ts [out] 时间戳输出
 */
static inline void agentos_time_from_ms(uint64_t ms, agentos_timestamp_t* ts) {
    if (!ts) return;
    ts->seconds = ms / 1000ULL;
    ts->nanoseconds = (uint32_t)((ms % 1000ULL) * 1000000ULL);
}

/**
 * @brief 睡眠指定毫秒
 * @param ms [in] 毫秒数
 */
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

/**
 * @brief 获取当前进程ID
 * @return 进程ID
 */
static inline uint32_t agentos_process_self(void) {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

/**
 * @brief 获取当前线程ID
 * @return 线程ID
 */
static inline uint64_t agentos_thread_self(void) {
    return agentos_thread_id();
}

/**
 * @brief 设置线程名称
 * @param name [in] 线程名称
 * @return 0成功，非0失败
 */
static inline int agentos_thread_setname(const char* name) {
    /* commons 暂不支持，返回成功 */
    (void)name;
    return 0;
}

/**
 * @brief 获取线程名称
 * @param name [out] 名称输出缓冲区
 * @param size [in] 缓冲区大小
 * @return 0成功，非0失败
 */
static inline int agentos_thread_getname(char* name, size_t size) {
    /* commons 暂不支持，返回空字符串 */
    if (name && size > 0) {
        name[0] = '\0';
    }
    return 0;
}

/* ==================== 文件系统兼容 ==================== */

/**
 * @brief 创建目录（递归）
 * @param path [in] 目录路径
 * @param recursive [in] 是否递归创建
 * @return 0成功，非0失败
 */
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

/**
 * @brief 动态库句柄类型
 */
typedef void* agentos_dl_t;

/**
 * @brief 加载动态库
 * @param path [in] 库文件路径
 * @return 动态库句柄，失败返回 NULL
 */
static inline agentos_dl_t agentos_dl_open(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

/**
 * @brief 关闭动态库
 * @param dl [in] 动态库句柄
 * @return 0成功，非0失败
 */
static inline int agentos_dl_close(agentos_dl_t dl) {
#ifdef _WIN32
    return FreeLibrary((HMODULE)dl) ? 0 : -1;
#else
    return dlclose(dl);
#endif
}

/**
 * @brief 获取符号地址
 * @param dl [in] 动态库句柄
 * @param name [in] 符号名称
 * @return 符号地址，失败返回 NULL
 */
static inline void* agentos_dl_sym(agentos_dl_t dl, const char* name) {
#ifdef _WIN32
    return GetProcAddress((HMODULE)dl, name);
#else
    return dlsym(dl, name);
#endif
}

/**
 * @brief 获取动态库错误信息
 * @return 错误信息字符串
 */
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

/**
 * @brief 系统信息结构
 */
typedef struct {
    char os_name[64];       /**< 操作系统名称 */
    char os_version[64];    /**< 操作系统版本 */
    char hostname[64];      /**< 主机名 */
    uint32_t cpu_count;     /**< CPU核心数 */
    uint64_t memory_total;  /**< 总内存（字节） */
    uint64_t memory_free;   /**< 可用内存（字节） */
} agentos_sysinfo_t;

/**
 * @brief 获取系统信息
 * @param info [out] 系统信息输出
 * @return 0成功，非0失败
 */
static inline int agentos_get_sysinfo(agentos_sysinfo_t* info) {
    if (!info) return -1;
    
    memset(info, 0, sizeof(agentos_sysinfo_t));
    
#ifdef _WIN32
    OSVERSIONINFOA osvi;
    osvi.dwOSVersionInfoSize = sizeof(osvi);
#pragma warning(push)
#pragma warning(disable: 4996)
    GetVersionExA(&osvi);
#pragma warning(pop)
    
    strncpy(info->os_name, "Windows", sizeof(info->os_name) - 1);
    info->os_name[sizeof(info->os_name) - 1] = '\0';
    snprintf(info->os_version, sizeof(info->os_version), "%lu.%lu", 
             osvi.dwMajorVersion, osvi.dwMinorVersion);
    
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
    
    info->cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info->memory_total = si.totalram * si.mem_unit;
        info->memory_free = si.freeram * si.mem_unit;
    }
#endif
    
    return 0;
}

/* ==================== 原子操作兼容 ==================== */

/*
 * @note 原子操作已迁移至 <agentos/atomic_compat.h>
 * 以下函数保留向后兼容，新代码请使用 atomic_compat.h
 */
#include <agentos/atomic_compat.h>

/** @brief 原子整数类型（兼容层，建议使用 atomic_compat.h 中的类型） */
typedef struct {
    volatile int value;
} agentos_atomic_int_t;

/** @deprecated 使用 atomic_compat.h 中的 atomic_load 替代 */
static inline int agentos_atomic_load(agentos_atomic_int_t* atomic) {
    if (!atomic) return 0;
    return atomic_load_32((long*)&atomic->value, memory_order_seq_cst);
}

/** @deprecated 使用 atomic_compat.h 中的 atomic_store 替代 */
static inline void agentos_atomic_store(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return;
    atomic_store_32((long*)&atomic->value, (long)value, memory_order_seq_cst);
}

/** @deprecated 使用 atomic_compat.h 中的 atomic_fetch_add 替代 */
static inline int agentos_atomic_fetch_add(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return 0;
    return (int)atomic_fetch_add_32((long*)&atomic->value, (long)value, memory_order_seq_cst);
}

/** @deprecated 使用 atomic_compat.h 中的 atomic_fetch_sub 替代 */
static inline int agentos_atomic_fetch_sub(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return 0;
    return (int)atomic_fetch_sub_32((long*)&atomic->value, (long)value, memory_order_seq_cst);
}

/* ==================== 服务器端 Socket 兼容 ==================== */

/**
 * @brief 初始化网络库（Windows 需要）
 * @return 0 成功，非0 失败
 *
 * @note 在使用任何 socket 函数前必须调用此函数
 */
static inline int agentos_socket_init(void) {
#if AGENTOS_PLATFORM_WINDOWS
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    return result == 0 ? 0 : -1;
#elif AGENTOS_PLATFORM_POSIX
    /* POSIX 不需要初始化 */
    (void)0;
    return 0;
#else
    return -1;
#endif
}

/**
 * @brief 清理网络库（Windows 需要）
 */
static inline void agentos_socket_cleanup(void) {
#if AGENTOS_PLATFORM_WINDOWS
    WSACleanup();
#endif
    (void)0;
}

/**
 * @brief 创建 TCP 服务器 Socket
 * @param host 绑定地址（NULL 表示 INADDR_ANY）
 * @param port 监听端口
 * @return Socket 句柄，失败返回 AGENTOS_INVALID_SOCKET
 */
static inline agentos_socket_t agentos_socket_create_tcp_server(const char* host, uint16_t port) {
    agentos_socket_t sock = agentos_socket_tcp();
    if (sock == AGENTOS_INVALID_SOCKET) return AGENTOS_INVALID_SOCKET;

    /* 设置 SO_REUSEADDR */
    agentos_socket_set_reuseaddr(sock, 1);

    /* 绑定地址 */
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

    /* 开始监听 */
    if (listen(sock, SOMAXCONN) != 0) {
        agentos_socket_close(sock);
        return AGENTOS_INVALID_SOCKET;
    }

    return sock;
}

/**
 * @brief 创建 Unix Domain Socket 服务器（仅 POSIX）
 * @param path Socket 路径
 * @return Socket 句柄，失败返回 AGENTOS_INVALID_SOCKET
 */
static inline agentos_socket_t agentos_socket_create_unix_server(const char* path) {
#if AGENTOS_PLATFORM_POSIX
    agentos_socket_t sock = agentos_socket_unix();
    if (sock == AGENTOS_INVALID_SOCKET) return AGENTOS_INVALID_SOCKET;

    /* 删除已存在的 socket 文件 */
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
#else
    (void)path;
    return AGENTOS_INVALID_SOCKET;
#endif
}

/**
 * @brief 创建 Windows Named Pipe 服务器（仅 Windows）
 * @param pipe_path 管道路径
 * @return Socket/句柄，失败返回 AGENTOS_INVALID_SOCKET
 */
static inline agentos_socket_t agentos_socket_create_named_pipe_server(const char* pipe_path) {
#if AGENTOS_PLATFORM_WINDOWS
    HANDLE hPipe = CreateNamedPipeA(
        pipe_path,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT,
        PIPE_UNLIMITED_INSTANCES,
        4096,
        4096,
        0,
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        return AGENTOS_INVALID_SOCKET;
    }
    return (agentos_socket_t)hPipe;
#else
    (void)pipe_path;
    return AGENTOS_INVALID_SOCKET;
#endif
}

/**
 * @brief 等待并接受客户端连接
 * @param server_fd 服务器 Socket
 * @param timeout_ms 超时时间（毫秒），0 表示无限等待
 * @return 客户端 Socket，超时或错误返回 AGENTOS_INVALID_SOCKET
 */
static inline agentos_socket_t agentos_socket_accept(agentos_socket_t server_fd, uint32_t timeout_ms) {
#if AGENTOS_PLATFORM_WINDOWS
    /* Named Pipe 使用 ConnectNamedPipe */
    if (!ConnectNamedPipe((HANDLE)server_fd, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_LISTENING && timeout_ms > 0) {
            Sleep(timeout_ms);
            if (ConnectNamedPipe((HANDLE)server_fd, NULL)) {
                return server_fd;  /* 对于 Named Pipe，返回同一个句柄 */
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

    int sel = select((int)(server_fd + 1), &read_fds, NULL, NULL, 
                     timeout_ms > 0 ? &tv : NULL);
    
    if (sel <= 0) {
        return AGENTOS_INVALID_SOCKET;
    }

    return accept(server_fd, NULL, NULL);
#endif
}

/**
 * @brief 从 Socket 接收数据
 * @param sock Socket 句柄
 * @param buf 缓冲区
 * @param len 缓冲区长度
 * @return 接收的字节数，错误返回负值或 0
 */
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

/**
 * @brief 向 Socket 发送数据
 * @param sock Socket 句柄
 * @param buf 数据缓冲区
 * @param len 数据长度
 * @return 发送的字节数，错误返回负值或 0
 */
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
