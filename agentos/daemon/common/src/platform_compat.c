#include "platform.h"
#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>

int agentos_time_now(agentos_timestamp_t* ts) {
    if (!ts) return -1;
    *ts = agentos_time_ns();
    return 0;
}

int agentos_time_monotonic(agentos_timestamp_t* ts) {
    return agentos_time_now(ts);
}

uint64_t agentos_time_to_ms(const agentos_timestamp_t* ts) {
    if (!ts) return 0;
    return *ts / 1000000ULL;
}

void agentos_time_from_ms(uint64_t ms, agentos_timestamp_t* ts) {
    if (!ts) return;
    *ts = ms * 1000000ULL;
}

void agentos_sleep_ms(uint32_t ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

uint32_t agentos_process_self(void) {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return (uint32_t)getpid();
#endif
}

uint64_t agentos_thread_self(void) {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return (uint64_t)(uintptr_t)pthread_self();
#endif
}

int agentos_thread_setname(const char* name) {
#ifdef __linux__
    return pthread_setname_np(pthread_self(), name);
#elif defined(__APPLE__)
    return pthread_setname_np(name);
#else
    (void)name;
    return 0;
#endif
}

int agentos_thread_getname(char* name, size_t size) {
#ifdef __linux__
    if (pthread_getname_np(pthread_self(), name, size) != 0)
        name[0] = '\0';
    return 0;
#elif defined(__APPLE__)
    if (size > 0) name[0] = '\0';
    return 0;
#else
    (void)name; (void)size;
    return -1;
#endif
}

int agentos_mkdir(const char* path, int recursive) {
    if (!path) return -1;
#ifdef _WIN32
    (void)recursive;
    return _mkdir(path);
#else
    if (recursive) {
        char tmp[PATH_MAX];
        size_t len = strlen(path);
        if (len == 0 || len >= PATH_MAX) return -1;
        memcpy(tmp, path, len + 1);
        for (size_t i = (tmp[0] == '/') ? 1 : 0; i <= len; i++) {
            if (tmp[i] == '/' || tmp[i] == '\0') {
                char saved = tmp[i];
                tmp[i] = '\0';
                if (i > 0 && tmp[0] != '\0') {
                    struct stat st;
                    if (stat(tmp, &st) != 0) {
                        if (mkdir(tmp, 0755) != 0) { tmp[i] = saved; return -1; }
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

agentos_dl_t agentos_dl_open(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW);
#endif
}

int agentos_dl_close(agentos_dl_t dl) {
#ifdef _WIN32
    return FreeLibrary(dl) ? 0 : -1;
#else
    return dlclose(dl);
#endif
}

void* agentos_dl_sym(agentos_dl_t dl, const char* symbol) {
#ifdef _WIN32
    return (void*)GetProcAddress(dl, symbol);
#else
    return dlsym(dl, symbol);
#endif
}

const char* agentos_dl_error(void) {
#ifdef _WIN32
    static char buf[256];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
                   0, buf, sizeof(buf), NULL);
    return buf;
#else
    return dlerror();
#endif
}

int agentos_get_sysinfo(agentos_sysinfo_t* info) {
    if (!info) return -1;
#ifdef __linux__
    struct sysinfo si;
    if (sysinfo(&si) != 0) return -1;
    strncpy(info->os_name, "Linux", sizeof(info->os_name));
    info->cpu_count = (uint32_t)si.procs;
    info->memory_total = si.totalram * si.mem_unit;
    info->memory_free = si.freeram * si.mem_unit;
    gethostname(info->hostname, sizeof(info->hostname));
    info->os_version[0] = '\0';
    return 0;
#elif defined(__APPLE__)
    strncpy(info->os_name, "macOS", sizeof(info->os_name));
    info->memory_total = 0;
    info->memory_free = 0;
    info->cpu_count = 0;
    gethostname(info->hostname, sizeof(info->hostname));
    info->os_version[0] = '\0';
    return 0;
#else
    memset(info, 0, sizeof(*info));
    return 0;
#endif
}

int agentos_atomic_load(agentos_atomic_int_t* atomic) {
    if (!atomic) return 0;
    return __atomic_load_n(&atomic->value, __ATOMIC_SEQ_CST);
}

void agentos_atomic_store(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return;
    __atomic_store_n(&atomic->value, value, __ATOMIC_SEQ_CST);
}

int agentos_atomic_fetch_add(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return 0;
    return __atomic_fetch_add(&atomic->value, value, __ATOMIC_SEQ_CST);
}

int agentos_atomic_fetch_sub(agentos_atomic_int_t* atomic, int value) {
    if (!atomic) return 0;
    return __atomic_fetch_sub(&atomic->value, value, __ATOMIC_SEQ_CST);
}

/* ==================== Socket 兼容层（生产级真实实现） ==================== */
/* SEC-017合规：基于POSIX Socket API的真实网络通信实现 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static int g_socket_initialized = 0;

int agentos_socket_init(void) {
    if (g_socket_initialized) return 0;
    g_socket_initialized = 1;
    return 0;
}

void agentos_socket_cleanup(void) {
    g_socket_initialized = 0;
}

agentos_socket_t agentos_socket_create_tcp_server(const char* host, uint16_t port) {
    if (!host) return -1;

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
    }

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

#if AGENTOS_PLATFORM_POSIX
agentos_socket_t agentos_socket_create_unix_server(const char* path) {
    if (!path) return -1;

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    unlink(path);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, SOMAXCONN) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}
#endif

agentos_socket_t agentos_socket_accept(agentos_socket_t server_fd, uint32_t timeout_ms) {
    if (server_fd < 0) return -1;

    struct pollfd pfd;
    pfd.fd = server_fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1,
        timeout_ms == 0 ? -1 : (int)timeout_ms);

    if (ret <= 0 || !(pfd.revents & POLLIN)) return -1;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

    if (client_fd >= 0) {
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    }

    return client_fd;
}

ssize_t agentos_socket_recv(agentos_socket_t sock, void* buf, size_t len) {
    if (sock < 0 || !buf || len == 0) return -1;
    ssize_t ret = recv(sock, buf, len, MSG_DONTWAIT);
    if (ret < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    return ret;
}

ssize_t agentos_socket_send(agentos_socket_t sock, const void* buf, size_t len) {
    if (sock < 0 || !buf || len == 0) return -1;
    ssize_t total_sent = 0;
    const uint8_t* ptr = (const uint8_t*)buf;

    while (total_sent < (ssize_t)len) {
        ssize_t sent = send(sock, ptr + total_sent, len - total_sent, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (sent <= 0) {
            if (total_sent > 0) break;
            return sent;
        }
        total_sent += sent;
    }
    return total_sent;
}
