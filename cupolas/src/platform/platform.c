/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * platform.c - Cross-platform Abstraction Layer Implementation
 */

/**
 * @file platform.c
 * @brief Cross-platform Abstraction Layer Implementation
 * @author Spharx
 * @date 2024
 */

#include "platform.h"
#include "../../../commons/utils/platform/include/platform_adapter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#if cupolas_PLATFORM_WINDOWS
    #include <io.h>
    #include <direct.h>
    #include <process.h>
    #define getcwd _getcwd
    #define rmdir _rmdir
    #define unlink _unlink
    #define access _access
    #define F_OK 0
    #define W_OK 2
    #define R_OK 4
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
    #include <signal.h>
    #include <time.h>
    #include <fcntl.h>
#endif

/* ============================================================================
 * 互斥锁实现
 * ============================================================================ */

int cupolas_mutex_init(cupolas_mutex_t* mutex) {\n    return platform_mutex_init((platform_mutex_t*)mutex);\n}

int cupolas_mutex_destroy(cupolas_mutex_t* mutex) {\n    return platform_mutex_destroy((platform_mutex_t*)mutex);\n}

int cupolas_mutex_lock(cupolas_mutex_t* mutex) {\n    return platform_mutex_lock((platform_mutex_t*)mutex);\n}

int cupolas_mutex_trylock(cupolas_mutex_t* mutex) {\n    return platform_mutex_trylock((platform_mutex_t*)mutex);\n}

int cupolas_mutex_unlock(cupolas_mutex_t* mutex) {\n    return platform_mutex_unlock((platform_mutex_t*)mutex);\n}

/* ============================================================================
 * 读写锁实现
 * ============================================================================ */

int cupolas_rwlock_init(cupolas_rwlock_t* rwlock) {\n    return platform_rwlock_init((platform_rwlock_t*)rwlock);\n}

int cupolas_rwlock_destroy(cupolas_rwlock_t* rwlock) {\n    return platform_rwlock_destroy((platform_rwlock_t*)rwlock);\n}

int cupolas_rwlock_rdlock(cupolas_rwlock_t* rwlock) {\n    return platform_rwlock_rdlock((platform_rwlock_t*)rwlock);\n}

int cupolas_rwlock_wrlock(cupolas_rwlock_t* rwlock) {\n    return platform_rwlock_wrlock((platform_rwlock_t*)rwlock);\n}

int cupolas_rwlock_tryrdlock(cupolas_rwlock_t* rwlock) {\n    return platform_rwlock_tryrdlock((platform_rwlock_t*)rwlock);\n}

int cupolas_rwlock_trywrlock(cupolas_rwlock_t* rwlock) {\n    return platform_rwlock_trywrlock((platform_rwlock_t*)rwlock);\n}

int cupolas_rwlock_unlock(cupolas_rwlock_t* rwlock) {\n    return platform_rwlock_unlock((platform_rwlock_t*)rwlock);\n}

/* ============================================================================
 * 条件变量实现
 * ============================================================================ */

int cupolas_cond_init(cupolas_cond_t* cond) {\n    return platform_cond_init((platform_cond_t*)cond);\n}

int cupolas_cond_destroy(cupolas_cond_t* cond) {\n    return platform_cond_destroy((platform_cond_t*)cond);\n}

int cupolas_cond_wait(cupolas_cond_t* cond, cupolas_mutex_t* mutex) {\n    return platform_cond_wait((platform_cond_t*)cond, (platform_mutex_t*)mutex);\n}

int cupolas_cond_timedwait(cupolas_cond_t* cond, cupolas_mutex_t* mutex, uint32_t timeout_ms) {\n    return platform_cond_timedwait((platform_cond_t*)cond, (platform_mutex_t*)mutex, timeout_ms);\n}

int cupolas_cond_signal(cupolas_cond_t* cond) {\n    return platform_cond_signal((platform_cond_t*)cond);\n}

int cupolas_cond_broadcast(cupolas_cond_t* cond) {\n    return platform_cond_broadcast((platform_cond_t*)cond);\n}

/* ============================================================================
 * 线程实现
 * ============================================================================ */

#if cupolas_PLATFORM_WINDOWS
typedef struct {
    cupolas_thread_func_t func;
    void* arg;
    void* result;
} thread_wrapper_ctx_t;

static DWORD WINAPI thread_wrapper(LPVOID param) {
    thread_wrapper_ctx_t* ctx = (thread_wrapper_ctx_t*)param;
    ctx->result = ctx->func(ctx->arg);
    free(ctx);
    return 0;
}
#endif

int cupolas_thread_create(cupolas_thread_t* thread, cupolas_thread_func_t func, void* arg) {\n    return platform_thread_create((platform_thread_func_t)func, arg, (platform_thread_t*)thread);\n}

int cupolas_thread_join(cupolas_thread_t thread, void** retval) {\n    return platform_join_thread((platform_thread_t)thread);\n}

int cupolas_thread_detach(cupolas_thread_t thread) {\n    return platform_detach_thread((platform_thread_t)thread);\n}

cupolas_thread_id_t cupolas_thread_self(void) {\n    return (cupolas_thread_id_t)platform_get_thread_id();\n}

bool cupolas_thread_equal(cupolas_thread_id_t t1, cupolas_thread_id_t t2) {
#if cupolas_PLATFORM_WINDOWS
    return t1 == t2;
#else
    return pthread_equal(t1, t2) != 0;
#endif
}

/* ============================================================================
 * 时间实现
 * ============================================================================ */

int cupolas_time_now(cupolas_timestamp_t* ts) {
    if (!ts) return cupolas_ERROR_INVALID_ARG;
    
#if cupolas_PLATFORM_WINDOWS
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart -= 116444736000000000ULL;
    ts->sec = (int64_t)(uli.QuadPart / 10000000);
    ts->nsec = (int32_t)((uli.QuadPart % 10000000) * 100);
    return cupolas_OK;
#else
    struct timespec t;
    if (clock_gettime(CLOCK_REALTIME, &t) != 0) {
        return cupolas_ERROR_UNKNOWN;
    }
    ts->sec = t.tv_sec;
    ts->nsec = (int32_t)t.tv_nsec;
    return cupolas_OK;
#endif
}

int cupolas_time_mono(cupolas_timestamp_t* ts) {
    if (!ts) return cupolas_ERROR_INVALID_ARG;
    
#if cupolas_PLATFORM_WINDOWS
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    ts->sec = count.QuadPart / freq.QuadPart;
    ts->nsec = (int32_t)((count.QuadPart % freq.QuadPart) * 1000000000 / freq.QuadPart);
    return cupolas_OK;
#else
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        return cupolas_ERROR_UNKNOWN;
    }
    ts->sec = t.tv_sec;
    ts->nsec = (int32_t)t.tv_nsec;
    return cupolas_OK;
#endif
}

uint64_t cupolas_time_ms(void) {\n    return platform_get_current_time_ms();\n}

void cupolas_sleep_ms(uint32_t ms) {
#if cupolas_PLATFORM_WINDOWS
    Sleep(ms);
#else
    struct timespec ts = { (time_t)(ms / 1000), (long)((ms % 1000) * 1000000) };
    nanosleep(&ts, NULL);
#endif
}

void cupolas_sleep_us(uint32_t us) {
#if cupolas_PLATFORM_WINDOWS
    Sleep((us + 999) / 1000);
#else
    struct timespec ts = { (time_t)(us / 1000000), (long)((us % 1000000) * 1000) };
    nanosleep(&ts, NULL);
#endif
}

/* ============================================================================
 * 文件系统实现
 * ============================================================================ */

int cupolas_file_stat(const char* path, cupolas_file_stat_t* stat) {
    if (!path || !stat) return cupolas_ERROR_INVALID_ARG;
    
    memset(stat, 0, sizeof(*stat));
    
#if cupolas_PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        stat->exists = false;
        return cupolas_ERROR_NOT_FOUND;
    }
    
    stat->exists = true;
    stat->is_dir = (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    stat->is_regular = !stat->is_dir;
    
    ULARGE_INTEGER size;
    size.LowPart = attrs.nFileSizeLow;
    size.HighPart = attrs.nFileSizeHigh;
    stat->size = size.QuadPart;
    
    ULARGE_INTEGER mtime;
    mtime.LowPart = attrs.ftLastWriteTime.dwLowDateTime;
    mtime.HighPart = attrs.ftLastWriteTime.dwHighDateTime;
    mtime.QuadPart -= 116444736000000000ULL;
    stat->mtime.sec = (int64_t)(mtime.QuadPart / 10000000);
    stat->mtime.nsec = (int32_t)((mtime.QuadPart % 10000000) * 100);
    
    return cupolas_OK;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        stat->exists = false;
        return cupolas_ERROR_NOT_FOUND;
    }
    
    stat->exists = true;
    stat->is_dir = S_ISDIR(st.st_mode);
    stat->is_regular = S_ISREG(st.st_mode);
    stat->size = (uint64_t)st.st_size;
    stat->mtime.sec = st.st_mtime;
    stat->mtime.nsec = 0;
    
    return cupolas_OK;
#endif
}

int cupolas_file_exists(const char* path) {\n    return platform_path_exists(path) ? 1 : 0;\n}

int cupolas_file_mkdir(const char* path, bool recursive) {
    if (!path) return cupolas_ERROR_INVALID_ARG;
    
    if (cupolas_file_exists(path)) {
        return cupolas_OK;
    }
    
    if (!recursive) {
#if cupolas_PLATFORM_WINDOWS
        return CreateDirectoryA(path, NULL) ? cupolas_OK : cupolas_ERROR_PERMISSION;
#else
        return mkdir(path, 0755) == 0 ? cupolas_OK : cupolas_ERROR_PERMISSION;
#endif
    }
    
    char buf[cupolas_PATH_MAX];
    strncpy(buf, path, cupolas_PATH_MAX - 1);
    buf[cupolas_PATH_MAX - 1] = '\0';
    
    char* p = buf;
    while (*p) {
        if (*p == cupolas_PATH_SEP && p != buf) {
            *p = '\0';
            if (!cupolas_file_exists(buf)) {
#if cupolas_PLATFORM_WINDOWS
                if (!CreateDirectoryA(buf, NULL)) {
                    return cupolas_ERROR_PERMISSION;
                }
#else
                if (mkdir(buf, 0755) != 0) {
                    return cupolas_ERROR_PERMISSION;
                }
#endif
            }
            *p = cupolas_PATH_SEP;
        }
        p++;
    }
    
    if (!cupolas_file_exists(buf)) {
#if cupolas_PLATFORM_WINDOWS
        return CreateDirectoryA(buf, NULL) ? cupolas_OK : cupolas_ERROR_PERMISSION;
#else
        return mkdir(buf, 0755) == 0 ? cupolas_OK : cupolas_ERROR_PERMISSION;
#endif
    }
    
    return cupolas_OK;
}

int cupolas_file_remove(const char* path) {
    if (!path) return cupolas_ERROR_INVALID_ARG;
    
#if cupolas_PLATFORM_WINDOWS
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return cupolas_ERROR_NOT_FOUND;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        return RemoveDirectoryA(path) ? cupolas_OK : cupolas_ERROR_PERMISSION;
    }
    return DeleteFileA(path) ? cupolas_OK : cupolas_ERROR_PERMISSION;
#else
    cupolas_file_stat_t st;
    if (cupolas_file_stat(path, &st) != cupolas_OK) {
        return cupolas_ERROR_NOT_FOUND;
    }
    if (st.is_dir) {
        return rmdir(path) == 0 ? cupolas_OK : cupolas_ERROR_PERMISSION;
    }
    return unlink(path) == 0 ? cupolas_OK : cupolas_ERROR_PERMISSION;
#endif
}

int cupolas_file_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return cupolas_ERROR_INVALID_ARG;
    
#if cupolas_PLATFORM_WINDOWS
    return MoveFileExA(old_path, new_path, MOVEFILE_REPLACE_EXISTING) ? cupolas_OK : cupolas_ERROR_PERMISSION;
#else
    return rename(old_path, new_path) == 0 ? cupolas_OK : cupolas_ERROR_PERMISSION;
#endif
}

char* cupolas_file_abspath(const char* path, char* buf, size_t size) {
    if (!path || !buf || size == 0) return NULL;
    
#if cupolas_PLATFORM_WINDOWS
    DWORD len = GetFullPathNameA(path, (DWORD)size, buf, NULL);
    if (len == 0 || len >= size) {
        return NULL;
    }
    return buf;
#else
    if (realpath(path, buf) == NULL) {
        if (path[0] == '/') {
            strncpy(buf, path, size - 1);
            buf[size - 1] = '\0';
        } else {
            char cwd[cupolas_PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                return NULL;
            }
            snprintf(buf, size, "%s/%s", cwd, path);
        }
    }
    return buf;
#endif
}

char* cupolas_file_dirname(const char* path, char* buf, size_t size) {
    if (!path || !buf || size == 0) return NULL;
    
    strncpy(buf, path, size - 1);
    buf[size - 1] = '\0';
    
    char* last_sep = strrchr(buf, cupolas_PATH_SEP);
    if (last_sep == NULL) {
        buf[0] = '.';
        buf[1] = '\0';
    } else if (last_sep == buf) {
        buf[1] = '\0';
    } else {
        *last_sep = '\0';
    }
    
    return buf;
}

/* ============================================================================
 * 内存实现
 * ============================================================================ */

void* cupolas_mem_alloc(size_t size) {\n    return platform_alloc(size);\n}

void* cupolas_mem_alloc_aligned(size_t size, size_t alignment) {\n    return platform_alloc_aligned(size, alignment);\n}

void cupolas_mem_free(void* ptr) {\n    platform_free(ptr);\n}

void* cupolas_mem_realloc(void* ptr, size_t size) {\n    return platform_realloc(ptr, size);\n}

void cupolas_mem_zero(void* ptr, size_t size) {
    if (ptr && size > 0) {
#if cupolas_PLATFORM_WINDOWS
        SecureZeroMemory(ptr, size);
#else
        memset(ptr, 0, size);
        __asm__ __volatile__ ("" : : "r"(ptr) : "memory");
#endif
    }
}

void cupolas_mem_lock(void* ptr, size_t size) {
    if (ptr && size > 0) {
#if cupolas_PLATFORM_WINDOWS
        VirtualLock(ptr, size);
#else
        mlock(ptr, size);
#endif
    }
}

void cupolas_mem_unlock(void* ptr, size_t size) {
    if (ptr && size > 0) {
#if cupolas_PLATFORM_WINDOWS
        VirtualUnlock(ptr, size);
#else
        munlock(ptr, size);
#endif
    }
}

/* ============================================================================
 * 原子操作实现
 * ============================================================================ */

int32_t cupolas_atomic_load32(cupolas_atomic32_t* ptr) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchange((volatile LONG*)ptr, 0, 0);
#else
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#endif
}

void cupolas_atomic_store32(cupolas_atomic32_t* ptr, int32_t val) {
#if cupolas_PLATFORM_WINDOWS
    InterlockedExchange((volatile LONG*)ptr, val);
#else
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#endif
}

int32_t cupolas_atomic_add32(cupolas_atomic32_t* ptr, int32_t delta) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedExchangeAdd((volatile LONG*)ptr, delta);
#else
    return __atomic_fetch_add(ptr, delta, __ATOMIC_SEQ_CST);
#endif
}

int32_t cupolas_atomic_sub32(cupolas_atomic32_t* ptr, int32_t delta) {
    return cupolas_atomic_add32(ptr, -delta);
}

int32_t cupolas_atomic_inc32(cupolas_atomic32_t* ptr) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedIncrement((volatile LONG*)ptr) - 1;
#else
    return __atomic_fetch_add(ptr, 1, __ATOMIC_SEQ_CST);
#endif
}

int32_t cupolas_atomic_dec32(cupolas_atomic32_t* ptr) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedDecrement((volatile LONG*)ptr) + 1;
#else
    return __atomic_fetch_sub(ptr, 1, __ATOMIC_SEQ_CST);
#endif
}

bool cupolas_atomic_cas32(cupolas_atomic32_t* ptr, int32_t expected, int32_t desired) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchange((volatile LONG*)ptr, desired, expected) == expected;
#else
    return __atomic_compare_exchange_n(ptr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

int64_t cupolas_atomic_load64(cupolas_atomic64_t* ptr) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchange64((volatile LONGLONG*)ptr, 0, 0);
#else
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#endif
}

void cupolas_atomic_store64(cupolas_atomic64_t* ptr, int64_t val) {
#if cupolas_PLATFORM_WINDOWS
    InterlockedExchange64((volatile LONGLONG*)ptr, val);
#else
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#endif
}

int64_t cupolas_atomic_add64(cupolas_atomic64_t* ptr, int64_t delta) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedExchangeAdd64((volatile LONGLONG*)ptr, delta);
#else
    return __atomic_fetch_add(ptr, delta, __ATOMIC_SEQ_CST);
#endif
}

int64_t cupolas_atomic_sub64(cupolas_atomic64_t* ptr, int64_t delta) {
    return cupolas_atomic_add64(ptr, -delta);
}

bool cupolas_atomic_cas64(cupolas_atomic64_t* ptr, int64_t expected, int64_t desired) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchange64((volatile LONGLONG*)ptr, desired, expected) == expected;
#else
    return __atomic_compare_exchange_n(ptr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

void* cupolas_atomic_load_ptr(cupolas_atomic_ptr_t* ptr) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchangePointer((volatile PVOID*)ptr, NULL, NULL);
#else
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#endif
}

void cupolas_atomic_store_ptr(cupolas_atomic_ptr_t* ptr, void* val) {
#if cupolas_PLATFORM_WINDOWS
    InterlockedExchangePointer((volatile PVOID*)ptr, val);
#else
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#endif
}

bool cupolas_atomic_cas_ptr(cupolas_atomic_ptr_t* ptr, void* expected, void* desired) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchangePointer((volatile PVOID*)ptr, desired, expected) == expected;
#else
    return __atomic_compare_exchange_n(ptr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

/* ============================================================================
 * 错误处理实现
 * ============================================================================ */

int cupolas_get_last_error(void) {
#if cupolas_PLATFORM_WINDOWS
    DWORD err = GetLastError();
    switch (err) {
        case ERROR_SUCCESS: return cupolas_OK;
        case ERROR_INVALID_PARAMETER: return cupolas_ERROR_INVALID_ARG;
        case ERROR_NOT_ENOUGH_MEMORY: return cupolas_ERROR_NO_MEMORY;
        case ERROR_FILE_NOT_FOUND: return cupolas_ERROR_NOT_FOUND;
        case ERROR_ACCESS_DENIED: return cupolas_ERROR_PERMISSION;
        case ERROR_BUSY: return cupolas_ERROR_BUSY;
        case ERROR_TIMEOUT: return cupolas_ERROR_TIMEOUT;
        default: return cupolas_ERROR_UNKNOWN;
    }
#else
    int err = errno;
    switch (err) {
        case 0: return cupolas_OK;
        case EINVAL: return cupolas_ERROR_INVALID_ARG;
        case ENOMEM: return cupolas_ERROR_NO_MEMORY;
        case ENOENT: return cupolas_ERROR_NOT_FOUND;
        case EACCES: return cupolas_ERROR_PERMISSION;
        case EBUSY: return cupolas_ERROR_BUSY;
        case ETIMEDOUT: return cupolas_ERROR_TIMEOUT;
        case EAGAIN: return cupolas_ERROR_WOULD_BLOCK;
        default: return cupolas_ERROR_UNKNOWN;
    }
#endif
}

const char* cupolas_strerror(int error) {
    switch (error) {
        case cupolas_OK: return "Success";
        case cupolas_ERROR_UNKNOWN: return "Unknown error";
        case cupolas_ERROR_INVALID_ARG: return "Invalid argument";
        case cupolas_ERROR_NO_MEMORY: return "Out of memory";
        case cupolas_ERROR_NOT_FOUND: return "Not found";
        case cupolas_ERROR_PERMISSION: return "Permission denied";
        case cupolas_ERROR_BUSY: return "Resource busy";
        case cupolas_ERROR_TIMEOUT: return "Timeout";
        case cupolas_ERROR_WOULD_BLOCK: return "Operation would block";
        case cupolas_ERROR_OVERFLOW: return "Buffer overflow";
        case cupolas_ERROR_NOT_SUPPORTED: return "Operation not supported";
        case cupolas_ERROR_IO: return "I/O error";
        default: return "Unknown error";
    }
}

/* ============================================================================
 * 字符串工具实现
 * ============================================================================ */

char* cupolas_strdup(const char* str) {\n    return platform_strdup(str);\n}

char* cupolas_strndup(const char* str, size_t n) {\n    return platform_strndup(str, n);\n}

int cupolas_strcasecmp(const char* s1, const char* s2) {\n    return platform_strcasecmp(s1, s2);\n}

int cupolas_strncasecmp(const char* s1, const char* s2, size_t n) {\n    return platform_strncasecmp(s1, s2, n);\n}

/* ============================================================================
 * 进程实现（简化版，仅支持基本功能）
 * ============================================================================ */

int cupolas_pipe_create(cupolas_pipe_t* pipe) {
    if (!pipe) return cupolas_ERROR_INVALID_ARG;
    
#if cupolas_PLATFORM_WINDOWS
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE read_hdl, write_hdl;
    if (!CreatePipe(&read_hdl, &write_hdl, &sa, 0)) {
        return cupolas_ERROR_UNKNOWN;
    }
    pipe[0] = read_hdl;
    pipe[1] = write_hdl;
    return cupolas_OK;
#else
    if (pipe2((int*)pipe, O_CLOEXEC) != 0) {
        return cupolas_ERROR_UNKNOWN;
    }
    return cupolas_OK;
#endif
}

int cupolas_pipe_close(cupolas_pipe_t* pipe) {
    if (!pipe) return cupolas_ERROR_INVALID_ARG;
    
#if cupolas_PLATFORM_WINDOWS
    if (pipe[0]) CloseHandle(pipe[0]);
    if (pipe[1]) CloseHandle(pipe[1]);
    pipe[0] = pipe[1] = NULL;
#else
    if (pipe[0] >= 0) close(pipe[0]);
    if (pipe[1] >= 0) close(pipe[1]);
    pipe[0] = pipe[1] = -1;
#endif
    return cupolas_OK;
}

int cupolas_pipe_read(cupolas_pipe_t* pipe, void* buf, size_t count, size_t* bytes_read) {
    if (!pipe || !buf || !bytes_read) return cupolas_ERROR_INVALID_ARG;
    
#if cupolas_PLATFORM_WINDOWS
    DWORD dw_read;
    if (!ReadFile(pipe[0], buf, (DWORD)count, &dw_read, NULL)) {
        return cupolas_ERROR_IO;
    }
    *bytes_read = dw_read;
    return cupolas_OK;
#else
    ssize_t n = read(pipe[0], buf, count);
    if (n < 0) return cupolas_ERROR_IO;
    *bytes_read = (size_t)n;
    return cupolas_OK;
#endif
}

int cupolas_pipe_write(cupolas_pipe_t* pipe, const void* buf, size_t count, size_t* bytes_written) {
    if (!pipe || !buf || !bytes_written) return cupolas_ERROR_INVALID_ARG;
    
#if cupolas_PLATFORM_WINDOWS
    DWORD dw_written;
    if (!WriteFile(pipe[1], buf, (DWORD)count, &dw_written, NULL)) {
        return cupolas_ERROR_IO;
    }
    *bytes_written = dw_written;
    return cupolas_OK;
#else
    ssize_t n = write(pipe[1], buf, count);
    if (n < 0) return cupolas_ERROR_IO;
    *bytes_written = (size_t)n;
    return cupolas_OK;
#endif
}

