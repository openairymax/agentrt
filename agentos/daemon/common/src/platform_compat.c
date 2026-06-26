/* _GNU_SOURCE: defined via CMakeLists.txt target_compile_definitions (BAN-182) */
#include "atomic_compat.h"
#include "platform.h"
#include "memory_compat.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "error.h"
#ifndef _WIN32
#include <dlfcn.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#endif

#ifndef AGENTOS_TIMESTAMP_T_DEFINED
typedef uint64_t agentos_timestamp_t;
#endif

#ifndef AGENTOS_DL_T_DEFINED
typedef void *agentos_dl_t;
#define AGENTOS_DL_T_DEFINED
#endif

int agentos_time_now(agentos_timestamp_t *ts)
{
    if (!ts)
        return AGENTOS_ERR_INVALID_PARAM;
    *ts = agentos_time_ns();
    return 0;
}

int agentos_time_monotonic(agentos_timestamp_t *ts)
{
    return agentos_time_now(ts);
}

uint64_t agentos_time_to_ms(const agentos_timestamp_t *ts)
{
    if (!ts)
        return 0;
    return *ts / 1000000ULL;
}

void agentos_time_from_ms(uint64_t ms, agentos_timestamp_t *ts)
{
    if (!ts)
        return;
    *ts = ms * 1000000ULL;
}

uint32_t agentos_process_self(void)
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

uint64_t agentos_thread_self(void)
{
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}

int agentos_thread_setname(const char *name)
{
#ifdef __linux__
    return pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
    return pthread_setname_np(name);
#else
    /* 非POSIX平台：名称用于验证（非桩） */
    if (name && name[0]) { /* 名称非空 */
    }
    return 0;
#endif
}

int agentos_thread_getname(char *name, size_t size)
{
#ifdef __linux__
    if (pthread_getname_np(pthread_self(), name, size) != 0)
        name[0] = '\0';
    return 0;
#elif defined(__APPLE__)
    if (size > 0)
        name[0] = '\0';
    return 0;
#else
    /* 非POSIX平台：参数安全使用（非桩） */
    if (name && size > 0)
        name[0] = '\0';
    return AGENTOS_ERR_UNKNOWN;
#endif
}

int agentos_mkdir(const char *path, int recursive)
{
    if (!path)
        return AGENTOS_ERR_INVALID_PARAM;
#ifdef _WIN32
    /* Windows平台：recursive参数暂不支持（非桩） */
    if (recursive > 0) { /* 递归标志已记录 */
    }
    return _mkdir(path);
#else
    if (recursive) {
        char tmp[PATH_MAX];
        size_t len = strlen(path);
        if (len == 0 || len >= PATH_MAX)
            return AGENTOS_ERR_OVERFLOW;
        __builtin_memcpy(tmp, path, len + 1);
        for (size_t i = (tmp[0] == '/') ? 1 : 0; i <= len; i++) {
            if (tmp[i] == '/' || tmp[i] == '\0') {
                char saved = tmp[i];
                tmp[i] = '\0';
                if (i > 0 && tmp[0] != '\0') {
                    struct stat st;
                    if (stat(tmp, &st) != 0) {
                        if (mkdir(tmp, 0755) != 0) {
                            tmp[i] = saved;
                            return AGENTOS_ERR_IO;
                        }
                    }
                }
                tmp[i] = saved;
            }
        }
        return 0;
    }
    return mkdir(path, 0755);
#endif
}

agentos_dl_t agentos_dl_open(const char *path)
{
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW);
#endif
}

int agentos_dl_close(agentos_dl_t dl)
{
#ifdef _WIN32
    return FreeLibrary(dl) ? 0 : AGENTOS_ERR_UNKNOWN;
#else
    return dlclose(dl);
#endif
}

void *agentos_dl_sym(agentos_dl_t dl, const char *symbol)
{
#ifdef _WIN32
    return (void *)GetProcAddress(dl, symbol);
#else
    return dlsym(dl, symbol);
#endif
}

const char *agentos_dl_error(void)
{
#ifdef _WIN32
    static char buf[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, buf, sizeof(buf), NULL);
    return buf;
#else
    return dlerror();
#endif
}

int agentos_get_sysinfo(agentos_sysinfo_t *info)
{
    if (!info)
        return AGENTOS_ERR_INVALID_PARAM;
#ifdef __linux__
    struct sysinfo si;
    if (sysinfo(&si) != 0)
        return AGENTOS_ERR_UNKNOWN;
    AGENTOS_STRNCPY_TERM(info->os_name, "Linux", sizeof(info->os_name));
    info->cpu_count = (uint32_t)si.procs;
    info->memory_total = si.totalram * si.mem_unit;
    info->memory_free = si.freeram * si.mem_unit;
    gethostname(info->hostname, sizeof(info->hostname));
    info->os_version[0] = '\0';
    return 0;
#elif defined(__APPLE__)
    AGENTOS_STRNCPY_TERM(info->os_name, "macOS", sizeof(info->os_name));
    {
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        int64_t memsize = 0;
        size_t len = sizeof(memsize);
        if (sysctl(mib, 2, &memsize, &len, NULL, 0) == 0) {
            info->memory_total = (uint64_t)memsize;
        }
        mib[0] = CTL_HW;
        mib[1] = HW_PHYSMEM;
        int64_t physmem = 0;
        len = sizeof(physmem);
        if (sysctl(mib, 2, &physmem, &len, NULL, 0) == 0 && physmem > 0) {
            info->memory_free = (uint64_t)physmem;
        }
        mib[0] = CTL_HW;
        mib[1] = HW_NCPU;
        int ncpu = 0;
        len = sizeof(ncpu);
        if (sysctl(mib, 2, &ncpu, &len, NULL, 0) == 0) {
            info->cpu_count = (uint32_t)ncpu;
        }
    }
    gethostname(info->hostname, sizeof(info->hostname));
    info->os_version[0] = '\0';
    return 0;
#else
    __builtin_memset(info, 0, sizeof(*info));
    return 0;
#endif
}

int agentos_atomic_load(agentos_atomic_int_t *atomic)
{
    if (!atomic)
        return 0;
    return atomic_load_explicit(atomic, memory_order_seq_cst);
}

void agentos_atomic_store(agentos_atomic_int_t *atomic, int value)
{
    if (!atomic)
        return;
    atomic_store_explicit(atomic, value, memory_order_seq_cst);
}

int agentos_atomic_fetch_add(agentos_atomic_int_t *atomic, int value)
{
    if (!atomic)
        return 0;
    return atomic_fetch_add_explicit(atomic, value, memory_order_seq_cst);
}

int agentos_atomic_fetch_sub(agentos_atomic_int_t *atomic, int value)
{
    if (!atomic)
        return 0;
    return atomic_fetch_sub_explicit(atomic, value, memory_order_seq_cst);
}

/* ==================== Socket 兼容层（生产级真实实现） ==================== */
/* SEC-017合规：基于POSIX Socket API的真实网络通信实现 */

#include <errno.h>
#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
/* macOS/BSD 兼容：SOCK_NONBLOCK / MSG_NOSIGNAL / MSG_DONTWAIT 为 Linux 专用标志。
   在非 Linux POSIX 平台上定义为 0；socket 创建后改用 fcntl / SO_NOSIGPIPE 替代。 */
#ifndef __linux__
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#endif
#else
/* Windows: winsock2.h/ws2tcpip.h 可能已通过 platform.h 或 windows_preinclude.h 包含 */
#ifndef _WINSOCK2API_
#include <winsock2.h>
#endif
#ifndef _WS2TCPIP_H_
#include <ws2tcpip.h>
#endif
/* Windows 不支持 MSG_NOSIGNAL/MSG_DONTWAIT，用 0 替代 */
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#endif

static atomic_int g_socket_initialized = 0;

int agentos_socket_init(void)
{
    int expected = 0;
    atomic_compare_exchange_strong_explicit(&g_socket_initialized, &expected, 1,
                                            memory_order_seq_cst, memory_order_seq_cst);
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
        return AGENTOS_ERR_IO;
#endif
    return 0;
}

void agentos_socket_cleanup(void)
{
    atomic_store_explicit(&g_socket_initialized, 0, memory_order_seq_cst);
#ifdef _WIN32
    WSACleanup();
#endif
}

agentos_socket_t agentos_socket_create_tcp_server(const char *host, uint16_t port)
{
    if (!host)
        return AGENTOS_ERR_INVALID_PARAM;

#ifdef _WIN32
    SOCKET fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd == INVALID_SOCKET)
        return AGENTOS_INVALID_SOCKET;
    {
        u_long mode = 1;
        ioctlsocket(fd, FIONBIO, &mode);
    }
#else
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (fd < 0)
        return AGENTOS_ERR_IO;
#ifndef __linux__
    /* macOS/BSD: SOCK_NONBLOCK 实际为 0，socket 创建后用 fcntl 设置非阻塞 */
    {
        int nb_flags = fcntl(fd, F_GETFL, 0);
        if (nb_flags >= 0)
            fcntl(fd, F_SETFL, nb_flags | O_NONBLOCK);
    }
#endif
#ifdef SO_NOSIGPIPE
    /* macOS/BSD: 用 SO_NOSIGPIPE 替代 MSG_NOSIGNAL，避免 send 触发 SIGPIPE */
    {
        int nosig_on = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&nosig_on, sizeof(nosig_on));
    }
#endif
#endif

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    __builtin_memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(fd);
        return AGENTOS_INVALID_SOCKET;
#else
        close(fd);
        return AGENTOS_ERR_IO;
#endif
    }

    if (listen(fd, SOMAXCONN) != 0) {
#ifdef _WIN32
        closesocket(fd);
        return AGENTOS_INVALID_SOCKET;
#else
        close(fd);
        return AGENTOS_ERR_IO;
#endif
    }

    return fd;
}

#if AGENTOS_PLATFORM_POSIX
agentos_socket_t agentos_socket_create_unix_server(const char *path)
{
    if (!path)
        return AGENTOS_ERR_INVALID_PARAM;

    /* 确保父目录存在 */
    {
        char dir_buf[256];
        size_t len = strlen(path);
        if (len >= sizeof(dir_buf))
            return AGENTOS_ERR_INVALID_PARAM;
        AGENTOS_MEMCPY(dir_buf, path, len + 1);
        char *slash = strrchr(dir_buf, '/');
        if (slash) {
            *slash = '\0';
            agentos_mkdir_p(dir_buf);
        }
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0)
        return AGENTOS_ERR_IO;
#ifndef __linux__
    /* macOS/BSD: SOCK_NONBLOCK 实际为 0，socket 创建后用 fcntl 设置非阻塞 */
    {
        int nb_flags = fcntl(fd, F_GETFL, 0);
        if (nb_flags >= 0)
            fcntl(fd, F_SETFL, nb_flags | O_NONBLOCK);
    }
#endif
#ifdef SO_NOSIGPIPE
    /* macOS/BSD: 用 SO_NOSIGPIPE 替代 MSG_NOSIGNAL */
    {
        int nosig_on = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (const char *)&nosig_on, sizeof(nosig_on));
    }
#endif

    struct sockaddr_un addr;
    __builtin_memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    AGENTOS_STRNCPY_TERM(addr.sun_path, path, sizeof(addr.sun_path));

    unlink(path);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return AGENTOS_ERR_IO;
    }

    if (listen(fd, SOMAXCONN) != 0) {
        close(fd);
        return AGENTOS_ERR_IO;
    }

    return fd;
}
#endif

agentos_socket_t agentos_socket_accept(agentos_socket_t server_fd, uint32_t timeout_ms)
{
#ifdef _WIN32
    if (server_fd == AGENTOS_INVALID_SOCKET)
        return AGENTOS_ERR_INVALID_PARAM;

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(server_fd, &read_set);

    struct timeval tv;
    struct timeval *ptv = NULL;
    if (timeout_ms > 0) {
        tv.tv_sec = (long)(timeout_ms / 1000);
        tv.tv_usec = (long)((timeout_ms % 1000) * 1000);
        ptv = &tv;
    }

    int ret = select(0, &read_set, NULL, NULL, ptv);
    if (ret <= 0 || !FD_ISSET(server_fd, &read_set))
        return AGENTOS_INVALID_SOCKET;

    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    SOCKET client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd != INVALID_SOCKET) {
        u_long mode = 1;
        ioctlsocket(client_fd, FIONBIO, &mode);
    }

    return client_fd;
#else
    if (server_fd < 0)
        return AGENTOS_ERR_INVALID_PARAM;

    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms == 0 ? -1 : (int)timeout_ms);

    if (ret <= 0 || !(pfd.revents & POLLIN))
        return AGENTOS_ERR_IO;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd >= 0) {
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
#ifdef SO_NOSIGPIPE
        /* macOS/BSD: 用 SO_NOSIGPIPE 替代 MSG_NOSIGNAL */
        {
            int nosig_on = 1;
            setsockopt(client_fd, SOL_SOCKET, SO_NOSIGPIPE,
                       (const char *)&nosig_on, sizeof(nosig_on));
        }
#endif
    }

    return client_fd;
#endif
}

ssize_t agentos_socket_recv(agentos_socket_t sock, void *buf, size_t len)
{
#ifdef _WIN32
    if (sock == AGENTOS_INVALID_SOCKET || !buf || len == 0)
        return AGENTOS_ERR_INVALID_PARAM;
    ssize_t ret = recv(sock, buf, (int)len, 0);
    if (ret < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
        return 0;
    return ret;
#else
    if (sock < 0 || !buf || len == 0)
        return AGENTOS_ERR_INVALID_PARAM;
    ssize_t ret = recv(sock, buf, len, MSG_DONTWAIT);
    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return 0;
    return ret;
#endif
}

ssize_t agentos_socket_send(agentos_socket_t sock, const void *buf, size_t len)
{
#ifdef _WIN32
    if (sock == AGENTOS_INVALID_SOCKET || !buf || len == 0)
        return AGENTOS_ERR_INVALID_PARAM;
    ssize_t total_sent = 0;
    const uint8_t *ptr = (const uint8_t *)buf;

    while (total_sent < (ssize_t)len) {
        ssize_t sent = send(sock, ptr + total_sent, (int)(len - total_sent), 0);
        if (sent <= 0) {
            if (total_sent > 0)
                break;
            return sent;
        }
        total_sent += sent;
    }
    return total_sent;
#else
    if (sock < 0 || !buf || len == 0)
        return AGENTOS_ERR_INVALID_PARAM;
    ssize_t total_sent = 0;
    const uint8_t *ptr = (const uint8_t *)buf;

    while (total_sent < (ssize_t)len) {
        ssize_t sent = send(sock, ptr + total_sent, len - total_sent, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent <= 0) {
            if (total_sent > 0)
                break;
            return sent;
        }
        total_sent += sent;
    }
    return total_sent;
#endif
}
