/**
 * @file platform.c
 * @brief 跨平台兼容层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../../../commons/utils/platform/include/platform_adapter.h"
#include "platform.h"

/* ==================== 线程实现 ==================== */

int agentos_thread_create(agentos_thread_t* thread, void* (*func)(void*), void* arg) {
    return platform_thread_create((platform_thread_func_t)func, arg, (platform_thread_t*)thread);
}

int agentos_thread_join(agentos_thread_t thread, void** retval) {
    (void)retval;
    return platform_join_thread((platform_thread_t)thread);
}

int agentos_thread_detach(agentos_thread_t thread) {
    return platform_detach_thread((platform_thread_t)thread);
}

agentos_thread_t agentos_thread_self(void) {
    return (agentos_thread_t)platform_get_thread_id();
}

/* ==================== 互斥锁实�?==================== */

int agentos_mutex_init(agentos_mutex_t* mutex) {
    return platform_mutex_init((platform_mutex_t*)mutex);
}

int agentos_mutex_lock(agentos_mutex_t* mutex) {
    return platform_mutex_lock((platform_mutex_t*)mutex);
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    return platform_mutex_trylock((platform_mutex_t*)mutex);
}

int agentos_mutex_unlock(agentos_mutex_t* mutex) {
    return platform_mutex_unlock((platform_mutex_t*)mutex);
}

int agentos_mutex_destroy(agentos_mutex_t* mutex) {
    return platform_mutex_destroy((platform_mutex_t*)mutex);
}

/* ==================== 条件变量实现 ==================== */

int agentos_cond_init(agentos_cond_t* cond) {
    return platform_cond_init((platform_cond_t*)cond);
}

int agentos_cond_wait(agentos_cond_t* cond, agentos_mutex_t* mutex) {
    return platform_cond_wait((platform_cond_t*)cond, (platform_mutex_t*)mutex);
}

int agentos_cond_timedwait(agentos_cond_t* cond, agentos_mutex_t* mutex, uint32_t timeout_ms) {
    return platform_cond_wait((platform_cond_t*)cond, (platform_mutex_t*)mutex);
}

int agentos_cond_signal(agentos_cond_t* cond) {
    return platform_cond_signal((platform_cond_t*)cond);
}

int agentos_cond_broadcast(agentos_cond_t* cond) {
    return platform_cond_broadcast((platform_cond_t*)cond);
}

int agentos_cond_destroy(agentos_cond_t* cond) {
    return platform_cond_destroy((platform_cond_t*)cond);
}

/* ==================== Socket 实现 ==================== */

int agentos_socket_init(void) {
    return 0;
}

void agentos_socket_cleanup(void) {
}

agentos_socket_t agentos_socket_create(int domain, int type, int protocol) {
    return (agentos_socket_t)platform_exec("socket", NULL, NULL);
}

int agentos_socket_connect(agentos_socket_t sock, const char* host, uint16_t port) {
    (void)sock;
    (void)host;
    (void)port;
    return 0;
}

int agentos_socket_bind(agentos_socket_t sock, const char* addr, uint16_t port) {
    (void)sock;
    (void)addr;
    (void)port;
    return 0;
}

int agentos_socket_listen(agentos_socket_t sock, int backlog) {
    (void)sock;
    (void)backlog;
    return 0;
}

agentos_socket_t agentos_socket_accept(agentos_socket_t sock) {
    (void)sock;
    return 0;
}

int agentos_socket_set_nonblock(agentos_socket_t sock) {
    (void)sock;
    return 0;
}

int agentos_socket_set_reuseaddr(agentos_socket_t sock) {
    (void)sock;
    return 0;
}

int agentos_socket_close(agentos_socket_t sock) {
    (void)sock;
    return 0;
}

int agentos_socket_read(agentos_socket_t sock, void* buf, size_t len) {
    (void)sock;
    (void)buf;
    (void)len;
    return 0;
}

int agentos_socket_write(agentos_socket_t sock, const void* buf, size_t len) {
    (void)sock;
    (void)buf;
    (void)len;
    return 0;
}

/* ==================== 管道实现 ==================== */

int agentos_pipe_create(agentos_pipe_t* pipe) {
    (void)pipe;
    return 0;
}

int agentos_pipe_read(agentos_pipe_t* pipe, void* buf, size_t len) {
    (void)pipe;
    (void)buf;
    (void)len;
    return 0;
}

int agentos_pipe_write(agentos_pipe_t* pipe, const void* buf, size_t len) {
    (void)pipe;
    (void)buf;
    (void)len;
    return 0;
}

int agentos_pipe_close_read(agentos_pipe_t* pipe) {
    (void)pipe;
    return 0;
}

int agentos_pipe_close_write(agentos_pipe_t* pipe) {
    (void)pipe;
    return 0;
}

/* ==================== 进程实现 ==================== */

int agentos_process_create(const char* executable, char* const argv[],
                           const char* working_dir,
                           agentos_pipe_t* stdin_pipe,
                           agentos_pipe_t* stdout_pipe,
                           agentos_pipe_t* stderr_pipe,
                           agentos_process_t* proc) {
    (void)executable;
    (void)argv;
    (void)working_dir;
    (void)stdin_pipe;
    (void)stdout_pipe;
    (void)stderr_pipe;
    (void)proc;
    return 0;
}

int agentos_process_wait(agentos_process_t* proc, uint32_t timeout_ms, int* exit_code) {
    (void)proc;
    (void)timeout_ms;
    (void)exit_code;
    return 0;
}

int agentos_process_kill(agentos_process_t* proc) {
    (void)proc;
    return 0;
}

void agentos_process_close_pipes(agentos_process_t* proc) {
    (void)proc;
}

/* ==================== 时间接口 ==================== */

uint64_t agentos_time_ns(void) {
    return platform_get_current_time_ms() * 1000000ULL;
}

uint64_t agentos_time_ms(void) {
    return platform_get_current_time_ms();
}

/* ==================== 随机数接�?==================== */

static AGENTOS_THREAD_LOCAL unsigned int g_random_seed = 0;
static AGENTOS_THREAD_LOCAL int g_random_initialized = 0;

void agentos_random_init(void) {
    if (!g_random_initialized) {
        g_random_seed = (unsigned int)platform_get_current_time_ms();
        g_random_initialized = 1;
    }
}

uint32_t agentos_random_uint32(uint32_t min, uint32_t max) {
    if (!g_random_initialized) {
        agentos_random_init();
    }
    if (min >= max) {
        return min;
    }
    uint32_t range = max - min + 1;
    uint32_t divisor = RAND_MAX / range;
    uint32_t retval;
    do {
        retval = (uint32_t)rand() / divisor;
    } while (retval >= range);
    return min + retval;
}

float agentos_random_float(void) {
    if (!g_random_initialized) {
        agentos_random_init();
    }
    return (float)rand() / (float)RAND_MAX;
}

int agentos_random_bytes(void* buf, size_t len) {
    (void)buf;
    (void)len;
    return 0;
}

/* ==================== 文件系统接口 ==================== */

int agentos_file_exists(const char* path) {
    return platform_path_exists(path) ? 1 : 0;
}

int agentos_mkdir_p(const char* path) {
    (void)path;
    return 0;
}

int64_t agentos_file_size(const char* path) {
    (void)path;
    return 0;
}

/* ==================== 字符串工�?==================== */

char* agentos_strlcpy(char* dest, size_t dest_size, const char* src) {
    if (dest_size == 0) {
        return dest;
    }
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    
    return dest;
}

char* agentos_strlcat(char* dest, size_t dest_size, const char* src) {
    if (dest_size == 0) {
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
