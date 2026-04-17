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
#ifdef _WIN32
    (void)recursive;
    return _mkdir(path);
#else
    if (recursive) {
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof(tmp), "mkdir -p \"%s\"", path);
        return system(tmp);
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

/* ==================== Socket 兼容层（桩实现） ==================== */

int agentos_socket_init(void) { return 0; }
void agentos_socket_cleanup(void) {}

agentos_socket_t agentos_socket_create_tcp_server(const char* host, uint16_t port) {
    (void)host; (void)port; return -1;
}

#if AGENTOS_PLATFORM_POSIX
agentos_socket_t agentos_socket_create_unix_server(const char* path) {
    (void)path; return -1;
}
#endif

agentos_socket_t agentos_socket_accept(agentos_socket_t server_fd, uint32_t timeout_ms) {
    (void)server_fd; (void)timeout_ms; return -1;
}

ssize_t agentos_socket_recv(agentos_socket_t sock, void* buf, size_t len) {
    (void)sock; (void)buf; (void)len; return -1;
}

ssize_t agentos_socket_send(agentos_socket_t sock, const void* buf, size_t len) {
    (void)sock; (void)buf; (void)len; return -1;
}
