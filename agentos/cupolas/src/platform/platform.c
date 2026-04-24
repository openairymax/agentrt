/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * platform.c - Cross-Platform Abstraction Layer Implementation
 *
 * Self-contained implementation using OS primitives directly.
 * No dependency on agentos/commons/platform_adapter (which only provides
 * high-level file/path/env utilities, not sync primitives).
 */

#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#if cupolas_PLATFORM_WINDOWS
    #include <io.h>
    #include <direct.h>
    #include <process.h>
    #include <windows.h>
    #define getcwd _getcwd
    #define rmdir _rmdir
    #define unlink _unlink
    #define access _access /* flawfinder: ignore */
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
    #include <pthread.h>
#endif

/* ============================================================================
 * Mutex Implementation
 * ============================================================================ */

int cupolas_mutex_init(cupolas_mutex_t* mutex) {
#if cupolas_PLATFORM_WINDOWS
    InitializeCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_init(mutex, NULL) == 0 ? 0 : -1;
#endif
}

int cupolas_mutex_destroy(cupolas_mutex_t* mutex) {
#if cupolas_PLATFORM_WINDOWS
    DeleteCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_destroy(mutex) == 0 ? 0 : -1;
#endif
}

int cupolas_mutex_lock(cupolas_mutex_t* mutex) {
#if cupolas_PLATFORM_WINDOWS
    EnterCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_lock(mutex) == 0 ? 0 : -1;
#endif
}

int cupolas_mutex_trylock(cupolas_mutex_t* mutex) {
#if cupolas_PLATFORM_WINDOWS
    return TryEnterCriticalSection(mutex) ? 0 : cupolas_ERROR_BUSY;
#else
    int ret = pthread_mutex_trylock(mutex);
    if (ret == 0) return 0;
    if (ret == EBUSY) return cupolas_ERROR_BUSY;
    return -1;
#endif
}

int cupolas_mutex_unlock(cupolas_mutex_t* mutex) {
#if cupolas_PLATFORM_WINDOWS
    LeaveCriticalSection(mutex);
    return 0;
#else
    return pthread_mutex_unlock(mutex) == 0 ? 0 : -1;
#endif
}

/* ============================================================================
 * Read-Write Lock Implementation
 * ============================================================================ */

int cupolas_rwlock_init(cupolas_rwlock_t* rwlock) {
#if cupolas_PLATFORM_WINDOWS
    InitializeSRWLock(&rwlock->lock);
    rwlock->state = 0;
    return 0;
#else
    return pthread_rwlock_init(rwlock, NULL) == 0 ? 0 : -1;
#endif
}

int cupolas_rwlock_destroy(cupolas_rwlock_t* rwlock) {
#if cupolas_PLATFORM_WINDOWS
    rwlock->state = 0;
    (void)rwlock;
    return 0;
#else
    return pthread_rwlock_destroy(rwlock) == 0 ? 0 : -1;
#endif
}

int cupolas_rwlock_rdlock(cupolas_rwlock_t* rwlock) {
#if cupolas_PLATFORM_WINDOWS
    AcquireSRWLockShared(&rwlock->lock);
    InterlockedIncrement(&rwlock->state);
    return 0;
#else
    return pthread_rwlock_rdlock(rwlock) == 0 ? 0 : -1;
#endif
}

int cupolas_rwlock_wrlock(cupolas_rwlock_t* rwlock) {
#if cupolas_PLATFORM_WINDOWS
    AcquireSRWLockExclusive(&rwlock->lock);
    InterlockedExchange(&rwlock->state, -1);
    return 0;
#else
    return pthread_rwlock_wrlock(rwlock) == 0 ? 0 : -1;
#endif
}

int cupolas_rwlock_tryrdlock(cupolas_rwlock_t* rwlock) {
#if cupolas_PLATFORM_WINDOWS
    if (!TryAcquireSRWLockShared(&rwlock->lock)) return cupolas_ERROR_BUSY;
    InterlockedIncrement(&rwlock->state);
    return 0;
#else
    int ret = pthread_rwlock_tryrdlock(rwlock);
    if (ret == 0) return 0;
    if (ret == EBUSY) return cupolas_ERROR_BUSY;
    return -1;
#endif
}

int cupolas_rwlock_trywrlock(cupolas_rwlock_t* rwlock) {
#if cupolas_PLATFORM_WINDOWS
    if (!TryAcquireSRWLockExclusive(&rwlock->lock)) return cupolas_ERROR_BUSY;
    InterlockedExchange(&rwlock->state, -1);
    return 0;
#else
    int ret = pthread_rwlock_trywrlock(rwlock);
    if (ret == 0) return 0;
    if (ret == EBUSY) return cupolas_ERROR_BUSY;
    return -1;
#endif
}

int cupolas_rwlock_unlock(cupolas_rwlock_t* rwlock) {
#if cupolas_PLATFORM_WINDOWS
    long s = InterlockedExchangeAdd(&rwlock->state, 0);
    if (s < 0) {
        InterlockedExchange(&rwlock->state, 0);
        ReleaseSRWLockExclusive(&rwlock->lock);
    } else {
        InterlockedDecrement(&rwlock->state);
        ReleaseSRWLockShared(&rwlock->lock);
    }
    return 0;
#else
    return pthread_rwlock_unlock(rwlock) == 0 ? 0 : -1;
#endif
}

/* ============================================================================
 * Condition Variable Implementation
 * ============================================================================ */

int cupolas_cond_init(cupolas_cond_t* cond) {
#if cupolas_PLATFORM_WINDOWS
    InitializeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_init(cond, NULL) == 0 ? 0 : -1;
#endif
}

int cupolas_cond_destroy(cupolas_cond_t* cond) {
#if cupolas_PLATFORM_WINDOWS
    /* CONDITION_VARIABLE does not need destruction */
    (void)cond;
    return 0;
#else
    return pthread_cond_destroy(cond) == 0 ? 0 : -1;
#endif
}

int cupolas_cond_wait(cupolas_cond_t* cond, cupolas_mutex_t* mutex) {
#if cupolas_PLATFORM_WINDOWS
    SleepConditionVariableCS(cond, mutex, INFINITE) ? 0 : -1;
#else
    return pthread_cond_wait(cond, mutex) == 0 ? 0 : -1;
#endif
}

int cupolas_cond_timedwait(cupolas_cond_t* cond, cupolas_mutex_t* mutex,
                           uint32_t timeout_ms) {
#if cupolas_PLATFORM_WINDOWS
    if (!SleepConditionVariableCS(cond, mutex, timeout_ms))
        return cupolas_ERROR_TIMEOUT;
    return 0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += (timeout_ms / 1000);
    ts.tv_nsec += ((timeout_ms % 1000) * 1000000);
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_nsec -= 1000000000;
        ts.tv_sec++;
    }
    int ret = pthread_cond_timedwait(cond, mutex, &ts);
    if (ret == 0) return 0;
    if (ret == ETIMEDOUT) return cupolas_ERROR_TIMEOUT;
    return -1;
#endif
}

int cupolas_cond_signal(cupolas_cond_t* cond) {
#if cupolas_PLATFORM_WINDOWS
    WakeConditionVariable(cond);
    return 0;
#else
    return pthread_cond_signal(cond) == 0 ? 0 : -1;
#endif
}

int cupolas_cond_broadcast(cupolas_cond_t* cond) {
#if cupolas_PLATFORM_WINDOWS
    WakeAllConditionVariable(cond);
    return 0;
#else
    return pthread_cond_broadcast(cond) == 0 ? 0 : -1;
#endif
}

/* ============================================================================
 * Thread Implementation
 * ============================================================================ */

typedef struct thread_wrapper_arg {
    cupolas_thread_func_t func;
    void* arg;
} thread_wrapper_arg_t;

#if !cupolas_PLATFORM_WINDOWS
static void* thread_wrapper(void* arg) {
    thread_wrapper_arg_t* wrapper = (thread_wrapper_arg_t*)arg;
    cupolas_thread_func_t func = wrapper->func;
    void* user_arg = wrapper->arg;
    free(wrapper);
    return func(user_arg);
}
#endif

int cupolas_thread_create(cupolas_thread_t* thread, cupolas_thread_func_t func, void* arg) {
#if cupolas_PLATFORM_WINDOWS
    thread_wrapper_arg_t* wrapper = (thread_wrapper_arg_t*)malloc(sizeof(thread_wrapper_arg_t));
    if (!wrapper) return cupolas_ERROR_NO_MEMORY;
    wrapper->func = func;
    wrapper->arg = arg;

    *thread = CreateThread(NULL, 0,
                          (LPTHREAD_START_ROUTINE)(void(*)(void*))func,
                          arg, 0, NULL);
    if (*thread == NULL) { free(wrapper); return cupolas_ERROR_UNKNOWN; }
    free(wrapper);
    return 0;
#else
    thread_wrapper_arg_t* wrapper = (thread_wrapper_arg_t*)malloc(sizeof(thread_wrapper_arg_t));
    if (!wrapper) return cupolas_ERROR_NO_MEMORY;
    wrapper->func = func;
    wrapper->arg = arg;

    int ret = pthread_create(thread, NULL, thread_wrapper, wrapper);
    if (ret != 0) { free(wrapper); return -1; }
    return 0;
#endif
}

int cupolas_thread_join(cupolas_thread_t thread, void** retval) {
#if cupolas_PLATFORM_WINDOWS
    WaitForSingleObject(thread, INFINITE);
    DWORD exit_code;
    GetExitCodeThread(thread, &exit_code);
    CloseHandle(thread);
    if (retval) *retval = (void*)(uintptr_t)exit_code;
    return 0;
#else
    return pthread_join(thread, retval) == 0 ? 0 : -1;
#endif
}

int cupolas_thread_detach(cupolas_thread_t thread) {
#if cupolas_PLATFORM_WINDOWS
    CloseHandle(thread);
    return 0;
#else
    return pthread_detach(thread) == 0 ? 0 : -1;
#endif
}

cupolas_thread_id_t cupolas_thread_self(void) {
#if cupolas_PLATFORM_WINDOWS
    return GetCurrentThreadId();
#else
    return pthread_self();
#endif
}

bool cupolas_thread_equal(cupolas_thread_id_t t1, cupolas_thread_id_t t2) {
#if cupolas_PLATFORM_WINDOWS
    return t1 == t2;
#else
    return pthread_equal(t1, t2);
#endif
}

/* ============================================================================
 * Process Implementation
 * ============================================================================ */

int cupolas_process_spawn(cupolas_process_t* proc,
                        const char* path,
                        char* const argv[],
                        const cupolas_process_attr_t* attr) {
    if (!proc || !path || !argv) return cupolas_ERROR_INVALID_ARG;
    (void)attr;

#if cupolas_PLATFORM_WINDOWS
    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = { 0 };
    char cmdLine[4096] = { 0 };
    size_t offset = 0;

    for (int i = 0; argv[i] != NULL; i++) {
        size_t len = strlen(argv[i]);
        if (offset + len + 2 > sizeof(cmdLine)) return cupolas_ERROR_OVERFLOW;
        if (i > 0) cmdLine[offset++] = ' ';
        memcpy(cmdLine + offset, argv[i], len);
        offset += len;
    }
    cmdLine[offset] = '\0';

    BOOL ok = CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE,
                              CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (!ok) return cupolas_ERROR_IO;

    *proc = pi.hProcess;
    CloseHandle(pi.hThread);
    return 0;
#else
    pid_t pid = fork();
    if (pid < 0) return cupolas_ERROR_IO;
    if (pid == 0) {
        execvp(path, argv); /* flawfinder: ignore - path/argv are controlled by cupolas internals */
        _exit(127);
    }
    *proc = pid;
    return 0;
#endif
}

int cupolas_process_wait(cupolas_process_t proc, cupolas_exit_status_t* status,
                        uint32_t timeout_ms) {
    if (!status) return cupolas_ERROR_INVALID_ARG;

#if cupolas_PLATFORM_WINDOWS
    DWORD ms = (timeout_ms == 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(proc, ms);
    if (result == WAIT_TIMEOUT) return cupolas_ERROR_TIMEOUT;
    if (result != WAIT_OBJECT_0) return cupolas_ERROR_IO;

    DWORD exit_code;
    GetExitCodeProcess(proc, &exit_code);
    status->code = (int)exit_code;
    status->signaled = false;
    status->signal = 0;
    return 0;
#else
    if (timeout_ms == 0) {
        int s;
        pid_t ret = waitpid(proc, &s, 0);
        if (ret < 0) return cupolas_ERROR_IO;
        status->signaled = WIFSIGNALED(s);
        status->signal = WTERMSIG(s);
        status->code = WEXITSTATUS(s);
        return 0;
    } else {
        for (uint32_t elapsed = 0; elapsed < timeout_ms; elapsed += 10) {
            int s;
            pid_t ret = waitpid(proc, &s, WNOHANG);
            if (ret > 0) {
                status->signaled = WIFSIGNALED(s);
                status->signal = WTERMSIG(s);
                status->code = WEXITSTATUS(s);
                return 0;
            }
            if (ret < 0 && errno != ECHILD) return cupolas_ERROR_IO;
            usleep(10000);
        }
        return cupolas_ERROR_TIMEOUT;
    }
#endif
}

int cupolas_process_terminate(cupolas_process_t proc, int signal) {
#if cupolas_PLATFORM_WINDOWS
    (void)signal;
    return TerminateProcess(proc, 1) ? 0 : -1;
#else
    return kill(proc, signal) == 0 ? 0 : -1;
#endif
}

int cupolas_process_close(cupolas_process_t proc) {
#if cupolas_PLATFORM_WINDOWS
    return CloseHandle(proc) ? 0 : -1;
#else
    (void)proc;
    return 0;
#endif
}

cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc) {
#if cupolas_PLATFORM_WINDOWS
    return GetProcessId(proc);
#else
    return proc;
#endif
}

/* ============================================================================
 * Pipe Implementation
 * ============================================================================ */

int cupolas_pipe_create(cupolas_pipe_t* pfd) {
#if cupolas_PLATFORM_WINDOWS
    HANDLE readHandle, writeHandle;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    if (!CreatePipe(&readHandle, &writeHandle, &sa, 0)) return cupolas_ERROR_IO;
    pfd[0] = readHandle;
    pfd[1] = writeHandle;
    return 0;
#else
    return pipe(*pfd) == 0 ? 0 : cupolas_ERROR_IO;
#endif
}

int cupolas_pipe_close(cupolas_pipe_t* pipe) {
#if cupolas_PLATFORM_WINDOWS
    CloseHandle(pipe[0]);
    CloseHandle(pipe[1]);
    return 0;
#else
    close((*pipe)[0]);
    close((*pipe)[1]);
    return 0;
#endif
}

int cupolas_pipe_read(cupolas_pipe_t* pipe, void* buf, size_t count,
                     size_t* bytes_read) {
#if cupolas_PLATFORM_WINDOWS
    DWORD bytesRead = 0;
    BOOL ok = ReadFile(pipe[0], buf, (DWORD)count, &bytesRead, NULL);
    if (bytes_read) *bytes_read = bytesRead;
    return ok ? 0 : cupolas_ERROR_IO;
#else
    ssize_t n = read((*pipe)[0], buf, count);
    if (n < 0) return cupolas_ERROR_IO;
    if (bytes_read) *bytes_read = (size_t)n;
    return 0;
#endif
}

int cupolas_pipe_write(cupolas_pipe_t* pipe, const void* buf, size_t count,
                      size_t* bytes_written) {
#if cupolas_PLATFORM_WINDOWS
    DWORD written = 0;
    BOOL ok = WriteFile(pipe[1], buf, (DWORD)count, &written, NULL);
    if (bytes_written) *bytes_written = written;
    return ok ? 0 : cupolas_ERROR_IO;
#else
    ssize_t n = write((*pipe)[1], buf, count);
    if (n < 0) return cupolas_ERROR_IO;
    if (bytes_written) *bytes_written = (size_t)n;
    return 0;
#endif
}

/* ============================================================================
 * Time Implementation
 * ============================================================================ */

int cupolas_time_now(cupolas_timestamp_t* ts) {
    if (!ts) return cupolas_ERROR_INVALID_ARG;
#if cupolas_PLATFORM_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    uint64_t ns100 = uli.QuadPart;
    ts->sec = (int64_t)(ns100 / 10000000ULL - 11644473600ULL);
    ts->nsec = (int32_t)((ns100 % 10000000ULL) * 100);
    return 0;
#else
    struct timespec tp;
    if (clock_gettime(CLOCK_REALTIME, &tp) != 0) return cupolas_ERROR_IO;
    ts->sec = tp.tv_sec;
    ts->nsec = tp.tv_nsec;
    return 0;
#endif
}

int cupolas_time_mono(cupolas_timestamp_t* ts) {
    if (!ts) return cupolas_ERROR_INVALID_ARG;
#if cupolas_PLATFORM_WINDOWS
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    uint64_t ns = (uint64_t)(counter.QuadPart * 1000000000ULL / freq.QuadPart);
    ts->sec = (int64_t)(ns / 1000000000ULL);
    ts->nsec = (int32_t)(ns % 1000000000ULL);
    return 0;
#else
    struct timespec tp;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) != 0) return cupolas_ERROR_IO;
    ts->sec = tp.tv_sec;
    ts->nsec = tp.tv_nsec;
    return 0;
#endif
}

uint64_t cupolas_time_ms(void) {
#if cupolas_PLATFORM_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uint64_t)((uli.QuadPart / 10000ULL) - 11644473600000ULL);
#else
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    return (uint64_t)tp.tv_sec * 1000ULL + (uint64_t)tp.tv_nsec / 1000000ULL;
#endif
}

void cupolas_sleep_ms(uint32_t ms) {
#if cupolas_PLATFORM_WINDOWS
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void cupolas_sleep_us(uint32_t us) {
#if cupolas_PLATFORM_WINDOWS
    if (us < 1000) {
        Sleep(1);
    } else {
        Sleep(us / 1000);
    }
#else
    usleep(us);
#endif
}

/* ============================================================================
 * File System Implementation
 * ============================================================================ */

int cupolas_file_stat(const char* path, cupolas_file_stat_t* file_stat) {
    if (!path || !file_stat) return cupolas_ERROR_INVALID_ARG;
    memset(file_stat, 0, sizeof(*file_stat));

#if cupolas_PLATFORM_WINDOWS
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &fad)) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND ||
            GetLastError() == ERROR_PATH_NOT_FOUND) {
            file_stat->exists = false;
            return 0;
        }
        return cupolas_ERROR_IO;
    }

    file_stat->exists = true;
    file_stat->is_dir = (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    file_stat->is_regular = !file_stat->is_dir;
    file_stat->size = ((uint64_t)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;

    ULARGE_INTEGER uli;
    uli.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    uli.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    uint64_t ns100 = uli.QuadPart;
    file_stat->mtime.sec = (int64_t)(ns100 / 10000000ULL - 11644473600ULL);
    file_stat->mtime.nsec = (int32_t)((ns100 % 10000000ULL) * 100);
    return 0;
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        if (errno == ENOENT) { file_stat->exists = false; return 0; }
        return cupolas_ERROR_IO;
    }

    file_stat->exists = true;
    file_stat->is_dir = S_ISDIR(st.st_mode) != 0;
    file_stat->is_regular = S_ISREG(st.st_mode) != 0;
    file_stat->size = (uint64_t)st.st_size;
    file_stat->mtime.sec = st.st_mtime;
    file_stat->mtime.nsec = 0;
    return 0;
#endif
}

int cupolas_file_exists(const char* path) {
    if (!path) return 0;
#if cupolas_PLATFORM_WINDOWS
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) ? 1 : 0;
#else
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
#endif
}

int cupolas_file_mkdir(const char* path, bool recursive) {
    if (!path) return cupolas_ERROR_INVALID_ARG;
#if cupolas_PLATFORM_WINDOWS
    if (recursive) {
        char tmp[cupolas_PATH_MAX];
        strncpy(tmp, path, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        for (char* p = tmp + 1; *p; p++) {
            if (*p == '\\' || *p == '/') {
                *p = '\0';
                CreateDirectoryA(tmp, NULL);
                *p = '\\';
            }
        }
    }
    return CreateDirectoryA(path, NULL) ? 0 : cupolas_ERROR_IO;
#else
    if (recursive) {
        char tmp[cupolas_PATH_MAX];
        strncpy(tmp, path, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        for (char* p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(tmp, 0755);
                *p = '/';
            }
        }
    }
    return mkdir(path, 0755) == 0 ? 0 : cupolas_ERROR_IO;
#endif
}

int cupolas_file_remove(const char* path) {
    if (!path) return cupolas_ERROR_INVALID_ARG;
#if cupolas_PLATFORM_WINDOWS
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return cupolas_ERROR_NOT_FOUND;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return RemoveDirectoryA(path) ? 0 : cupolas_ERROR_IO;
    return DeleteFileA(path) ? 0 : cupolas_ERROR_IO;
#else
    return unlink(path) == 0 ? 0 : cupolas_ERROR_IO;
#endif
}

int cupolas_file_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) return cupolas_ERROR_INVALID_ARG;
#if cupolas_PLATFORM_WINDOWS
    return MoveFileExA(old_path, new_path, MOVEFILE_REPLACE_EXISTING) ? 0 : cupolas_ERROR_IO;
#else
    return rename(old_path, new_path) == 0 ? 0 : cupolas_ERROR_IO;
#endif
}

char* cupolas_file_abspath(const char* path, char* buf, size_t size) {
    if (!path || !buf || size == 0) return NULL;
#if cupolas_PLATFORM_WINDOWS
    DWORD ret = GetFullPathNameA(path, (DWORD)size, buf, NULL);
    return (ret > 0 && ret < size) ? buf : NULL;
#else
    if (realpath(path, buf)) return buf;
    if (strlen(path) < size) {
        strncpy(buf, path, size - 1);
        buf[size - 1] = '\0';
        return buf;
    }
    return NULL;
#endif
}

char* cupolas_file_dirname(const char* path, char* buf, size_t size) {
    if (!path || !buf || size == 0) return NULL;

    size_t len = strlen(path);
    if (len >= size) return NULL;

    strncpy(buf, path, size);
    buf[size - 1] = '\0';

    char* last_slash = strrchr(buf, '/');
#if cupolas_PLATFORM_WINDOWS
    char* last_bs = strrchr(buf, '\\');
    if (last_bs > last_slash) last_slash = last_bs;
#endif

    if (last_slash) {
        *last_slash = '\0';
    } else {
        buf[0] = '.';
        buf[1] = '\0';
    }
    return buf;
}

/* ============================================================================
 * Memory Implementation
 * ============================================================================ */

void* cupolas_mem_alloc(size_t size) {
    if (size == 0) return NULL;
#if cupolas_PLATFORM_WINDOWS
    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size);
#else
    void* ptr = malloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
#endif
}

void* cupolas_mem_alloc_aligned(size_t size, size_t alignment) {
    if (size == 0 || alignment == 0) return NULL;
#if cupolas_PLATFORM_WINDOWS
    return _aligned_malloc(size, alignment);
#else
    void* ptr = NULL;
    (void)posix_memalign(&ptr, alignment, size);
    return ptr;
#endif
}

void cupolas_mem_free(void* ptr) {
    if (!ptr) return;
#if cupolas_PLATFORM_WINDOWS
    HeapFree(GetProcessHeap(), 0, ptr);
#else
    free(ptr);
#endif
}

void* cupolas_mem_realloc(void* ptr, size_t size) {
#if cupolas_PLATFORM_WINDOWS
    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, size);
#else
    return realloc(ptr, size);
#endif
}

void cupolas_mem_zero(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
#if cupolas_PLATFORM_WINDOWS
    SecureZeroMemory(ptr, size);
#else
    volatile unsigned char* p = (volatile unsigned char*)ptr;
    while (size--) *p++ = 0;
#endif
}

void cupolas_mem_lock(void* ptr, size_t size) {
    (void)ptr;
    (void)size;
#if cupolas_PLATFORM_WINDOWS
    VirtualLock(ptr, size);
#endif
}

void cupolas_mem_unlock(void* ptr, size_t size) {
    (void)ptr;
    (void)size;
#if cupolas_PLATFORM_WINDOWS
    VirtualUnlock(ptr, size);
#endif
}

/* ============================================================================
 * Atomic Operations Implementation
 * ============================================================================ */

int32_t cupolas_atomic_load32(volatile int32_t* ptr) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchange((volatile LONG*)ptr, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return (int32_t)__atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#else
    return *ptr;
#endif
}

void cupolas_atomic_store32(volatile int32_t* ptr, int32_t val) {
#if cupolas_PLATFORM_WINDOWS
    InterlockedExchange((volatile LONG*)ptr, val);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#else
    *ptr = val;
#endif
}

int32_t cupolas_atomic_add32(volatile int32_t* ptr, int32_t delta) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedExchangeAdd((volatile LONG*)ptr, delta) + delta;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(ptr, delta, __ATOMIC_SEQ_CST);
#else
    return (*ptr += delta);
#endif
}

int32_t cupolas_atomic_sub32(volatile int32_t* ptr, int32_t delta) {
    return cupolas_atomic_add32(ptr, -delta);
}

int32_t cupolas_atomic_inc32(volatile int32_t* ptr) {
    return cupolas_atomic_add32(ptr, 1);
}

int32_t cupolas_atomic_dec32(volatile int32_t* ptr) {
    return cupolas_atomic_sub32(ptr, 1);
}

bool cupolas_atomic_cas32(volatile int32_t* ptr, int32_t expected, int32_t desired) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchange((volatile LONG*)ptr, desired, expected) == expected;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_compare_exchange_n(ptr, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    if (*ptr == expected) { *ptr = desired; return true; }
    return false;
#endif
}

int64_t cupolas_atomic_load64(volatile int64_t* ptr) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchange64((volatile LONGLONG*)ptr, 0, 0);
#elif defined(__GNUC__) || defined(__clang__)
    return (int64_t)__atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#else
    return *ptr;
#endif
}

void cupolas_atomic_store64(volatile int64_t* ptr, int64_t val) {
#if cupolas_PLATFORM_WINDOWS
    InterlockedExchange64((volatile LONGLONG*)ptr, val);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#else
    *ptr = val;
#endif
}

int64_t cupolas_atomic_add64(volatile int64_t* ptr, int64_t delta) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedExchangeAdd64((volatile LONGLONG*)ptr, delta) + delta;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_add_fetch(ptr, delta, __ATOMIC_SEQ_CST);
#else
    return (*ptr += delta);
#endif
}

int64_t cupolas_atomic_sub64(volatile int64_t* ptr, int64_t delta) {
    return cupolas_atomic_add64(ptr, -delta);
}

bool cupolas_atomic_cas64(volatile int64_t* ptr, int64_t expected, int64_t desired) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchange64((volatile LONGLONG*)ptr, desired, expected) == expected;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_compare_exchange_n(ptr, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    if (*ptr == expected) { *ptr = desired; return true; }
    return false;
#endif
}

void* cupolas_atomic_load_ptr(volatile void** ptr) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchangePointer((volatile PVOID*)ptr, NULL, NULL);
#elif defined(__GNUC__) || defined(__clang__)
    return (void*)__atomic_load_n(ptr, __ATOMIC_SEQ_CST);
#else
    return *(void**)ptr;
#endif
}

void cupolas_atomic_store_ptr(volatile void** ptr, void* val) {
#if cupolas_PLATFORM_WINDOWS
    InterlockedExchangePointer((volatile PVOID*)ptr, val);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_store_n(ptr, val, __ATOMIC_SEQ_CST);
#else
    *(void**)ptr = val;
#endif
}

bool cupolas_atomic_cas_ptr(volatile void** ptr, void* expected, void* desired) {
#if cupolas_PLATFORM_WINDOWS
    return InterlockedCompareExchangePointer((volatile PVOID*)ptr, desired, expected) == expected;
#elif defined(__GNUC__) || defined(__clang__)
    return __atomic_compare_exchange_n(ptr, &expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
#else
    if (*(void**)ptr == expected) { *(void**)ptr = desired; return true; }
    return false;
#endif
}

/* ============================================================================
 * Error Handling Implementation
 * ============================================================================ */

int cupolas_get_last_error(void) {
#if cupolas_PLATFORM_WINDOWS
    return (int)GetLastError();
#else
    return errno;
#endif
}

const char* cupolas_strerror(int error) {
    switch (error) {
        case 0:              return "Success";
        case cupolas_ERROR_UNKNOWN:      return "Unknown error";
        case cupolas_ERROR_INVALID_ARG:  return "Invalid argument";
        case cupolas_ERROR_NO_MEMORY:    return "Out of memory";
        case cupolas_ERROR_NOT_FOUND:    return "Not found";
        case cupolas_ERROR_PERMISSION:   return "Permission denied";
        case cupolas_ERROR_BUSY:         return "Resource busy";
        case cupolas_ERROR_TIMEOUT:      return "Operation timed out";
        case cupolas_ERROR_WOULD_BLOCK:   return "Operation would block";
        case cupolas_ERROR_OVERFLOW:     return "Overflow";
        case cupolas_ERROR_NOT_SUPPORTED: return "Not supported";
        case cupolas_ERROR_IO:           return "I/O error";
        default:
#if cupolas_PLATFORM_WINDOWS
            static char msg[256];
            FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, (DWORD)error,
                         0, msg, sizeof(msg), NULL);
            return msg;
#else
            return strerror(error);
#endif
    }
}

/* ============================================================================
 * String Utilities Implementation
 * ============================================================================ */

char* cupolas_strdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* dup = (char*)cupolas_mem_alloc(len);
    if (dup) memcpy(dup, str, len);
    return dup;
}

char* cupolas_strndup(const char* str, size_t n) {
    if (!str) return NULL;
    size_t len = strlen(str);
    if (len > n) len = n;
    char* dup = (char*)cupolas_mem_alloc(len + 1);
    if (dup) {
        memcpy(dup, str, len);
        dup[len] = '\0';
    }
    return dup;
}

int cupolas_strcasecmp(const char* s1, const char* s2) {
    if (!s1 || !s2) return (s1 ? 1 : (s2 ? -1 : 0));
#if cupolas_PLATFORM_WINDOWS
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

int cupolas_strncasecmp(const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2) return (s1 ? 1 : (s2 ? -1 : 0));
#if cupolas_PLATFORM_WINDOWS
    return _strnicmp(s1, s2, n);
#else
    return strncasecmp(s1, s2, n);
#endif
}
