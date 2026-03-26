/**
 * @file platform.c
 * @brief 跨平台抽象层实现
 * @author Spharx
 * @date 2024
 */

#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#if DOMES_PLATFORM_WINDOWS
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

int domes_mutex_init(domes_mutex_t* mutex) {
    if (!mutex) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    InitializeCriticalSection(mutex);
    return DOMES_OK;
#else
    int ret = pthread_mutex_init(mutex, NULL);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_mutex_destroy(domes_mutex_t* mutex) {
    if (!mutex) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    DeleteCriticalSection(mutex);
    return DOMES_OK;
#else
    int ret = pthread_mutex_destroy(mutex);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_mutex_lock(domes_mutex_t* mutex) {
    if (!mutex) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    EnterCriticalSection(mutex);
    return DOMES_OK;
#else
    int ret = pthread_mutex_lock(mutex);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_mutex_trylock(domes_mutex_t* mutex) {
    if (!mutex) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    return TryEnterCriticalSection(mutex) ? DOMES_OK : DOMES_ERROR_BUSY;
#else
    int ret = pthread_mutex_trylock(mutex);
    if (ret == EBUSY) return DOMES_ERROR_BUSY;
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_mutex_unlock(domes_mutex_t* mutex) {
    if (!mutex) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    LeaveCriticalSection(mutex);
    return DOMES_OK;
#else
    int ret = pthread_mutex_unlock(mutex);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

/* ============================================================================
 * 读写锁实现
 * ============================================================================ */

int domes_rwlock_init(domes_rwlock_t* rwlock) {
    if (!rwlock) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    InitializeSRWLock(rwlock);
    return DOMES_OK;
#else
    int ret = pthread_rwlock_init(rwlock, NULL);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_rwlock_destroy(domes_rwlock_t* rwlock) {
    if (!rwlock) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    (void)rwlock;
    return DOMES_OK;
#else
    int ret = pthread_rwlock_destroy(rwlock);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_rwlock_rdlock(domes_rwlock_t* rwlock) {
    if (!rwlock) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    AcquireSRWLockShared(rwlock);
    return DOMES_OK;
#else
    int ret = pthread_rwlock_rdlock(rwlock);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_rwlock_wrlock(domes_rwlock_t* rwlock) {
    if (!rwlock) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    AcquireSRWLockExclusive(rwlock);
    return DOMES_OK;
#else
    int ret = pthread_rwlock_wrlock(rwlock);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_rwlock_tryrdlock(domes_rwlock_t* rwlock) {
    if (!rwlock) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    return TryAcquireSRWLockShared(rwlock) ? DOMES_OK : DOMES_ERROR_BUSY;
#else
    int ret = pthread_rwlock_tryrdlock(rwlock);
    if (ret == EBUSY) return DOMES_ERROR_BUSY;
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_rwlock_trywrlock(domes_rwlock_t* rwlock) {
    if (!rwlock) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    return TryAcquireSRWLockExclusive(rwlock) ? DOMES_OK : DOMES_ERROR_BUSY;
#else
    int ret = pthread_rwlock_trywrlock(rwlock);
    if (ret == EBUSY) return DOMES_ERROR_BUSY;
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_rwlock_unlock(domes_rwlock_t* rwlock) {
    if (!rwlock) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    if (rwlock->Ptr && ((ULONG_PTR)rwlock->Ptr & 0x7) != 0) {
        ReleaseSRWLockExclusive(rwlock);
    } else {
        ReleaseSRWLockShared(rwlock);
    }
    return DOMES_OK;
#else
    int ret = pthread_rwlock_unlock(rwlock);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

/* ============================================================================
 * 条件变量实现
 * ============================================================================ */

int domes_cond_init(domes_cond_t* cond) {
    if (!cond) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    InitializeConditionVariable(cond);
    return DOMES_OK;
#else
    int ret = pthread_cond_init(cond, NULL);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_cond_destroy(domes_cond_t* cond) {
    if (!cond) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    (void)cond;
    return DOMES_OK;
#else
    int ret = pthread_cond_destroy(cond);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_cond_wait(domes_cond_t* cond, domes_mutex_t* mutex) {
    if (!cond || !mutex) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    return SleepConditionVariableCS(cond, mutex, INFINITE) ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#else
    int ret = pthread_cond_wait(cond, mutex);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_cond_timedwait(domes_cond_t* cond, domes_mutex_t* mutex, uint32_t timeout_ms) {
    if (!cond || !mutex) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    return SleepConditionVariableCS(cond, mutex, timeout_ms) ? DOMES_OK : DOMES_ERROR_TIMEOUT;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    int ret = pthread_cond_timedwait(cond, mutex, &ts);
    if (ret == ETIMEDOUT) return DOMES_ERROR_TIMEOUT;
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_cond_signal(domes_cond_t* cond) {
    if (!cond) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    WakeConditionVariable(cond);
    return DOMES_OK;
#else
    int ret = pthread_cond_signal(cond);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_cond_broadcast(domes_cond_t* cond) {
    if (!cond) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    WakeAllConditionVariable(cond);
    return DOMES_OK;
#else
    int ret = pthread_cond_broadcast(cond);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

/* ============================================================================
 * 线程实现
 * ============================================================================ */

#if DOMES_PLATFORM_WINDOWS
typedef struct {
    domes_thread_func_t func;
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

int domes_thread_create(domes_thread_t* thread, domes_thread_func_t func, void* arg) {
    if (!thread || !func) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    thread_wrapper_ctx_t* ctx = (thread_wrapper_ctx_t*)malloc(sizeof(thread_wrapper_ctx_t));
    if (!ctx) return DOMES_ERROR_NO_MEMORY;
    ctx->func = func;
    ctx->arg = arg;
    ctx->result = NULL;
    
    *thread = CreateThread(NULL, 0, thread_wrapper, ctx, 0, NULL);
    if (*thread == NULL) {
        free(ctx);
        return DOMES_ERROR_UNKNOWN;
    }
    return DOMES_OK;
#else
    int ret = pthread_create(thread, NULL, func, arg);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_thread_join(domes_thread_t thread, void** retval) {
#if DOMES_PLATFORM_WINDOWS
    DWORD wait_result = WaitForSingleObject(thread, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        return DOMES_ERROR_UNKNOWN;
    }
    if (retval) {
        *retval = NULL;
    }
    CloseHandle(thread);
    return DOMES_OK;
#else
    int ret = pthread_join(thread, retval);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

int domes_thread_detach(domes_thread_t thread) {
#if DOMES_PLATFORM_WINDOWS
    CloseHandle(thread);
    return DOMES_OK;
#else
    int ret = pthread_detach(thread);
    return ret == 0 ? DOMES_OK : DOMES_ERROR_UNKNOWN;
#endif
}

domes_thread_id_t domes_thread_self(void) {
#if DOMES_PLATFORM_WINDOWS
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}

bool domes_thread_equal(domes_thread_id_t t1, domes_thread_id_t t2) {
#if DOMES_PLATFORM_WINDOWS
    return t1 == t2;
#else
    return pthread_equal(t1, t2) != 0;
#endif
}

/* ============================================================================
 * 时间实现
 * ============================================================================ */

int domes_time_now(domes_timestamp_t* ts) {
    if (!ts) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uli.QuadPart -= 116444736000000000ULL;
    ts->sec = (int64_t)(uli.QuadPart / 10000000);
    ts->nsec = (int32_t)((uli.QuadPart % 10000000) * 100);
    return DOMES_OK;
#else
    struct timespec t;
    if (clock_gettime(CLOCK_REALTIME, &t) != 0) {
        return DOMES_ERROR_UNKNOWN;
    }
    ts->sec = t.tv_sec;
    ts->nsec = (int32_t)t.tv_nsec;
    return DOMES_OK;
#endif
}

int domes_time_mono(domes_timestamp_t* ts) {
    if (!ts) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    ts->sec = count.QuadPart / freq.QuadPart;
    ts->nsec = (int32_t)((count.QuadPart % freq.QuadPart) * 1000000000 / freq.QuadPart);
    return DOMES_OK;
#else
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) {
        return DOMES_ERROR_UNKNOWN;
    }
    ts->sec = t.tv_sec;
    ts->nsec = (int32_t)t.tv_nsec;
    return DOMES_OK;
#endif
}

uint64_t domes_time_ms(void) {
    domes_timestamp_t ts;
    domes_time_now(&ts);
    return (uint64_t)ts.sec * 1000 + ts.nsec / 1000000;
}

void domes_sleep_ms(uint32_t ms) {
#if DOMES_PLATFORM_WINDOWS
    Sleep(ms);
#else
    struct timespec ts = { (time_t)(ms / 1000), (long)((ms % 1000) * 1000000) };
    nanosleep(&ts, NULL);
#endif
}

void domes_sleep_us(uint32_t us) {
#if DOMES_PLATFORM_WINDOWS
    Sleep((us + 999) / 1000);
#else
    struct timespec ts = { (time_t)(us / 1000000), (long)((us % 1000000) * 1000) };
    nanosleep(&ts, NULL);
#endif
}

/* ============================================================================
 * 文件系统实现
 * ============================================================================ */

int domes_file_stat(const char* path, domes_file_stat_t* stat) {
    if (!path || !stat) return DOMES_ERROR_INVALID_ARG;
    
    memset(stat, 0, sizeof(*stat));
    
#if DOMES_PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        stat->exists = false;
        return DOMES_ERROR_NOT_FOUND;
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
    
    return DOMES_OK;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        stat->exists = false;
        return DOMES_ERROR_NOT_FOUND;
    }
    
    stat->exists = true;
    stat->is_dir = S_ISDIR(st.st_mode);
    stat->is_regular = S_ISREG(st.st_mode);
    stat->size = (uint64_t)st.st_size;
    stat->mtime.sec = st.st_mtime;
    stat->mtime.nsec = 0;
    
    return DOMES_OK;
#endif
}

int domes_file_exists(const char* path) {
    if (!path) return 0;
    
#if DOMES_PLATFORM_WINDOWS
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

int domes_file_mkdir(const char* path, bool recursive) {
    if (!path) return DOMES_ERROR_INVALID_ARG;
    
    if (domes_file_exists(path)) {
        return DOMES_OK;
    }
    
    if (!recursive) {
#if DOMES_PLATFORM_WINDOWS
        return CreateDirectoryA(path, NULL) ? DOMES_OK : DOMES_ERROR_PERMISSION;
#else
        return mkdir(path, 0755) == 0 ? DOMES_OK : DOMES_ERROR_PERMISSION;
#endif
    }
    
    char buf[DOMES_PATH_MAX];
    strncpy(buf, path, DOMES_PATH_MAX - 1);
    buf[DOMES_PATH_MAX - 1] = '\0';
    
    char* p = buf;
    while (*p) {
        if (*p == DOMES_PATH_SEP && p != buf) {
            *p = '\0';
            if (!domes_file_exists(buf)) {
#if DOMES_PLATFORM_WINDOWS
                if (!CreateDirectoryA(buf, NULL)) {
                    return DOMES_ERROR_PERMISSION;
                }
#else
                if (mkdir(buf, 0755) != 0) {
                    return DOMES_ERROR_PERMISSION;
                }
#endif
            }
            *p = DOMES_PATH_SEP;
        }
        p++;
    }
    
    if (!domes_file_exists(buf)) {
#if DOMES_PLATFORM_WINDOWS
        return CreateDirectoryA(buf, NULL) ? DOMES_OK : DOMES_ERROR_PERMISSION;
#else
        return mkdir(buf, 0755) == 0 ? DOMES_OK : DOMES_ERROR_PERMISSION;
#endif
    }
    
    return DOMES_OK;
}

int domes_file_remove(const char* path) {
    if (!path) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return DOMES_ERROR_NOT_FOUND;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        return RemoveDirectoryA(path) ? DOMES_OK : DOMES_ERROR_PERMISSION;
    }
    return DeleteFileA(path) ? DOMES_OK : DOMES_ERROR_PERMISSION;
#else
    domes_file_stat_t st;
    if (domes_file_stat(path, &st) != DOMES_OK) {
        return DOMES_ERROR_NOT_FOUND;
    }
    if (st.is_dir) {
        return rmdir(path) == 0 ? DOMES_OK : DOMES_ERROR_PERMISSION;
    }
    return unlink(path) == 0 ? DOMES_OK : DOMES_ERROR_PERMISSION;
#endif
}

int domes_file_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    return MoveFileExA(old_path, new_path, MOVEFILE_REPLACE_EXISTING) ? DOMES_OK : DOMES_ERROR_PERMISSION;
#else
    return rename(old_path, new_path) == 0 ? DOMES_OK : DOMES_ERROR_PERMISSION;
#endif
}

char* domes_file_abspath(const char* path, char* buf, size_t size) {
    if (!path || !buf || size == 0) return NULL;
    
#if DOMES_PLATFORM_WINDOWS
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
            char cwd[DOMES_PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == NULL) {
                return NULL;
            }
            snprintf(buf, size, "%s/%s", cwd, path);
        }
    }
    return buf;
#endif
}

char* domes_file_dirname(const char* path, char* buf, size_t size) {
    if (!path || !buf || size == 0) return NULL;
    
    strncpy(buf, path, size - 1);
    buf[size - 1] = '\0';
    
    char* last_sep = strrchr(buf, DOMES_PATH_SEP);
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

void* domes_mem_alloc(size_t size) {
    if (size == 0) return NULL;
    return malloc(size);
}

void* domes_mem_alloc_aligned(size_t size, size_t alignment) {
    if (size == 0) return NULL;
    
#if DOMES_PLATFORM_WINDOWS
    return _aligned_malloc(size, alignment);
#else
    void* ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
#endif
}

void domes_mem_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

void* domes_mem_realloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

void domes_mem_zero(void* ptr, size_t size) {
    if (ptr && size > 0) {
#if DOMES_PLATFORM_WINDOWS
        SecureZeroMemory(ptr, size);
#else
        memset(ptr, 0, size);
        __asm__ __volatile__ ("" : : "r"(ptr) : "memory");
#endif
    }
}

void domes_mem_lock(void* ptr, size_t size) {
    if (ptr && size > 0) {
#if DOMES_PLATFORM_WINDOWS
        VirtualLock(ptr, size);
#else
        mlock(ptr, size);
#endif
    }
}

void domes_mem_unlock(void* ptr, size_t size) {
    if (ptr && size > 0) {
#if DOMES_PLATFORM_WINDOWS
        VirtualUnlock(ptr, size);
#else
        munlock(ptr, size);
#endif
    }
}

/* ============================================================================
 * 原子操作实现
 * ============================================================================ */

int32_t domes_atomic_load32(domes_atomic32_t* ptr) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedCompareExchange((volatile LONG*)ptr, 0, 0);
#else
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#endif
}

void domes_atomic_store32(domes_atomic32_t* ptr, int32_t val) {
#if DOMES_PLATFORM_WINDOWS
    InterlockedExchange((volatile LONG*)ptr, val);
#else
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#endif
}

int32_t domes_atomic_add32(domes_atomic32_t* ptr, int32_t delta) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedExchangeAdd((volatile LONG*)ptr, delta);
#else
    return __atomic_fetch_add(ptr, delta, __ATOMIC_SEQ_CST);
#endif
}

int32_t domes_atomic_sub32(domes_atomic32_t* ptr, int32_t delta) {
    return domes_atomic_add32(ptr, -delta);
}

int32_t domes_atomic_inc32(domes_atomic32_t* ptr) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedIncrement((volatile LONG*)ptr) - 1;
#else
    return __atomic_fetch_add(ptr, 1, __ATOMIC_SEQ_CST);
#endif
}

int32_t domes_atomic_dec32(domes_atomic32_t* ptr) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedDecrement((volatile LONG*)ptr) + 1;
#else
    return __atomic_fetch_sub(ptr, 1, __ATOMIC_SEQ_CST);
#endif
}

bool domes_atomic_cas32(domes_atomic32_t* ptr, int32_t expected, int32_t desired) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedCompareExchange((volatile LONG*)ptr, desired, expected) == expected;
#else
    return __atomic_compare_exchange_n(ptr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

int64_t domes_atomic_load64(domes_atomic64_t* ptr) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedCompareExchange64((volatile LONGLONG*)ptr, 0, 0);
#else
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#endif
}

void domes_atomic_store64(domes_atomic64_t* ptr, int64_t val) {
#if DOMES_PLATFORM_WINDOWS
    InterlockedExchange64((volatile LONGLONG*)ptr, val);
#else
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#endif
}

int64_t domes_atomic_add64(domes_atomic64_t* ptr, int64_t delta) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedExchangeAdd64((volatile LONGLONG*)ptr, delta);
#else
    return __atomic_fetch_add(ptr, delta, __ATOMIC_SEQ_CST);
#endif
}

int64_t domes_atomic_sub64(domes_atomic64_t* ptr, int64_t delta) {
    return domes_atomic_add64(ptr, -delta);
}

bool domes_atomic_cas64(domes_atomic64_t* ptr, int64_t expected, int64_t desired) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedCompareExchange64((volatile LONGLONG*)ptr, desired, expected) == expected;
#else
    return __atomic_compare_exchange_n(ptr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

void* domes_atomic_load_ptr(domes_atomic_ptr_t* ptr) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedCompareExchangePointer((volatile PVOID*)ptr, NULL, NULL);
#else
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#endif
}

void domes_atomic_store_ptr(domes_atomic_ptr_t* ptr, void* val) {
#if DOMES_PLATFORM_WINDOWS
    InterlockedExchangePointer((volatile PVOID*)ptr, val);
#else
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#endif
}

bool domes_atomic_cas_ptr(domes_atomic_ptr_t* ptr, void* expected, void* desired) {
#if DOMES_PLATFORM_WINDOWS
    return InterlockedCompareExchangePointer((volatile PVOID*)ptr, desired, expected) == expected;
#else
    return __atomic_compare_exchange_n(ptr, &expected, desired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#endif
}

/* ============================================================================
 * 错误处理实现
 * ============================================================================ */

int domes_get_last_error(void) {
#if DOMES_PLATFORM_WINDOWS
    DWORD err = GetLastError();
    switch (err) {
        case ERROR_SUCCESS: return DOMES_OK;
        case ERROR_INVALID_PARAMETER: return DOMES_ERROR_INVALID_ARG;
        case ERROR_NOT_ENOUGH_MEMORY: return DOMES_ERROR_NO_MEMORY;
        case ERROR_FILE_NOT_FOUND: return DOMES_ERROR_NOT_FOUND;
        case ERROR_ACCESS_DENIED: return DOMES_ERROR_PERMISSION;
        case ERROR_BUSY: return DOMES_ERROR_BUSY;
        case ERROR_TIMEOUT: return DOMES_ERROR_TIMEOUT;
        default: return DOMES_ERROR_UNKNOWN;
    }
#else
    int err = errno;
    switch (err) {
        case 0: return DOMES_OK;
        case EINVAL: return DOMES_ERROR_INVALID_ARG;
        case ENOMEM: return DOMES_ERROR_NO_MEMORY;
        case ENOENT: return DOMES_ERROR_NOT_FOUND;
        case EACCES: return DOMES_ERROR_PERMISSION;
        case EBUSY: return DOMES_ERROR_BUSY;
        case ETIMEDOUT: return DOMES_ERROR_TIMEOUT;
        case EAGAIN: return DOMES_ERROR_WOULD_BLOCK;
        default: return DOMES_ERROR_UNKNOWN;
    }
#endif
}

const char* domes_strerror(int error) {
    switch (error) {
        case DOMES_OK: return "Success";
        case DOMES_ERROR_UNKNOWN: return "Unknown error";
        case DOMES_ERROR_INVALID_ARG: return "Invalid argument";
        case DOMES_ERROR_NO_MEMORY: return "Out of memory";
        case DOMES_ERROR_NOT_FOUND: return "Not found";
        case DOMES_ERROR_PERMISSION: return "Permission denied";
        case DOMES_ERROR_BUSY: return "Resource busy";
        case DOMES_ERROR_TIMEOUT: return "Timeout";
        case DOMES_ERROR_WOULD_BLOCK: return "Operation would block";
        case DOMES_ERROR_OVERFLOW: return "Buffer overflow";
        case DOMES_ERROR_NOT_SUPPORTED: return "Operation not supported";
        case DOMES_ERROR_IO: return "I/O error";
        default: return "Unknown error";
    }
}

/* ============================================================================
 * 字符串工具实现
 * ============================================================================ */

char* domes_strdup(const char* str) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* dup = (char*)malloc(len);
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

char* domes_strndup(const char* str, size_t n) {
    if (!str) return NULL;
    
    size_t len = strnlen(str, n);
    char* dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}

int domes_strcasecmp(const char* s1, const char* s2) {
#if DOMES_PLATFORM_WINDOWS
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

int domes_strncasecmp(const char* s1, const char* s2, size_t n) {
#if DOMES_PLATFORM_WINDOWS
    return _strnicmp(s1, s2, n);
#else
    return strncasecmp(s1, s2, n);
#endif
}

/* ============================================================================
 * 进程实现（简化版，仅支持基本功能）
 * ============================================================================ */

int domes_pipe_create(domes_pipe_t* pipe) {
    if (!pipe) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE read_hdl, write_hdl;
    if (!CreatePipe(&read_hdl, &write_hdl, &sa, 0)) {
        return DOMES_ERROR_UNKNOWN;
    }
    pipe[0] = read_hdl;
    pipe[1] = write_hdl;
    return DOMES_OK;
#else
    if (pipe2((int*)pipe, O_CLOEXEC) != 0) {
        return DOMES_ERROR_UNKNOWN;
    }
    return DOMES_OK;
#endif
}

int domes_pipe_close(domes_pipe_t* pipe) {
    if (!pipe) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    if (pipe[0]) CloseHandle(pipe[0]);
    if (pipe[1]) CloseHandle(pipe[1]);
    pipe[0] = pipe[1] = NULL;
#else
    if (pipe[0] >= 0) close(pipe[0]);
    if (pipe[1] >= 0) close(pipe[1]);
    pipe[0] = pipe[1] = -1;
#endif
    return DOMES_OK;
}

int domes_pipe_read(domes_pipe_t* pipe, void* buf, size_t count, size_t* bytes_read) {
    if (!pipe || !buf || !bytes_read) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    DWORD dw_read;
    if (!ReadFile(pipe[0], buf, (DWORD)count, &dw_read, NULL)) {
        return DOMES_ERROR_IO;
    }
    *bytes_read = dw_read;
    return DOMES_OK;
#else
    ssize_t n = read(pipe[0], buf, count);
    if (n < 0) return DOMES_ERROR_IO;
    *bytes_read = (size_t)n;
    return DOMES_OK;
#endif
}

int domes_pipe_write(domes_pipe_t* pipe, const void* buf, size_t count, size_t* bytes_written) {
    if (!pipe || !buf || !bytes_written) return DOMES_ERROR_INVALID_ARG;
    
#if DOMES_PLATFORM_WINDOWS
    DWORD dw_written;
    if (!WriteFile(pipe[1], buf, (DWORD)count, &dw_written, NULL)) {
        return DOMES_ERROR_IO;
    }
    *bytes_written = dw_written;
    return DOMES_OK;
#else
    ssize_t n = write(pipe[1], buf, count);
    if (n < 0) return DOMES_ERROR_IO;
    *bytes_written = (size_t)n;
    return DOMES_OK;
#endif
}
