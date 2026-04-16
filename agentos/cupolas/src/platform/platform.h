/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * platform.h - Cross-Platform Abstraction Layer: Unified Windows/POSIX Differences
 *
 * Design Principles:
 * - Single Responsibility: Only handles platform differences
 * - Zero Overhead: Inline functions + macro definitions
 * - Type Safety: Strong type encapsulation
 */

#ifndef cupolas_PLATFORM_H
#define cupolas_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #define cupolas_PLATFORM_WINDOWS  1
    #define cupolas_PLATFORM_POSIX    0
    #ifdef _WIN64
        #define cupolas_PLATFORM_64BIT 1
    #else
        #define cupolas_PLATFORM_64BIT 0
    #endif
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    #define cupolas_PLATFORM_WINDOWS  0
    #define cupolas_PLATFORM_POSIX    1
    #if defined(__x86_64__) || defined(__aarch64__)
        #define cupolas_PLATFORM_64BIT 1
    #else
        #define cupolas_PLATFORM_64BIT 0
    #endif
#else
    #error "Unsupported platform"
#endif

/* ============================================================================
 * Export Macros
 * ============================================================================ */

#if cupolas_PLATFORM_WINDOWS
    #ifdef cupolas_BUILD_DLL
        #define cupolas_API __declspec(dllexport)
    #else
        #define cupolas_API __declspec(dllimport)
    #endif
#else
    #define cupolas_API __attribute__((visibility("default")))
#endif

/* ============================================================================
 * Thread Primitives
 * ============================================================================ */

/* Thread Handle Types */
#if cupolas_PLATFORM_WINDOWS
    #include <windows.h>
    typedef HANDLE cupolas_thread_t;
    typedef DWORD cupolas_thread_id_t;
    typedef CRITICAL_SECTION cupolas_mutex_t;
    typedef SRWLOCK cupolas_rwlock_t;
    typedef CONDITION_VARIABLE cupolas_cond_t;
#else
    #include <pthread.h>
    #include <sys/types.h>
    typedef pthread_t cupolas_thread_t;
    typedef pthread_t cupolas_thread_id_t;
    typedef pthread_mutex_t cupolas_mutex_t;
    typedef pthread_rwlock_t cupolas_rwlock_t;
    typedef pthread_cond_t cupolas_cond_t;
#endif

/* Mutex Interface */
/**
 * @brief Initialize mutex
 * @param[out] mutex Mutex handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: N/A
 * @reentrant N/A
 * @ownership mutex: callee initializes
 */
int cupolas_mutex_init(cupolas_mutex_t* mutex);

/**
 * @brief Destroy mutex
 * @param[in] mutex Mutex handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No, ensure no threads hold the lock
 * @reentrant N/A
 * @ownership mutex: caller transfers ownership
 */
int cupolas_mutex_destroy(cupolas_mutex_t* mutex);

/**
 * @brief Lock mutex
 * @param[in] mutex Mutex handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant No (deadlock if same thread locks twice)
 */
int cupolas_mutex_lock(cupolas_mutex_t* mutex);

/**
 * @brief Try lock mutex
 * @param[in] mutex Mutex handle (must not be NULL)
 * @return 0 on success, cupolas_ERROR_BUSY if already locked, negative on failure
 * @note Thread-safe: Yes
 * @reentrant No
 */
int cupolas_mutex_trylock(cupolas_mutex_t* mutex);

/**
 * @brief Unlock mutex
 * @param[in] mutex Mutex handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant No (only owner should unlock)
 */
int cupolas_mutex_unlock(cupolas_mutex_t* mutex);

/* Read-Write Lock Interface */
/**
 * @brief Initialize read-write lock
 * @param[out] rwlock Read-write lock handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: N/A
 * @reentrant N/A
 * @ownership rwlock: callee initializes
 */
int cupolas_rwlock_init(cupolas_rwlock_t* rwlock);

/**
 * @brief Destroy read-write lock
 * @param[in] rwlock Read-write lock handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant N/A
 * @ownership rwlock: caller transfers ownership
 */
int cupolas_rwlock_destroy(cupolas_rwlock_t* rwlock);

/**
 * @brief Acquire read lock
 * @param[in] rwlock Read-write lock handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes (same thread can acquire multiple read locks)
 */
int cupolas_rwlock_rdlock(cupolas_rwlock_t* rwlock);

/**
 * @brief Acquire write lock
 * @param[in] rwlock Read-write lock handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant No
 */
int cupolas_rwlock_wrlock(cupolas_rwlock_t* rwlock);

/**
 * @brief Try acquire read lock
 * @param[in] rwlock Read-write lock handle (must not be NULL)
 * @return 0 on success, cupolas_ERROR_BUSY if write locked, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_rwlock_tryrdlock(cupolas_rwlock_t* rwlock);

/**
 * @brief Try acquire write lock
 * @param[in] rwlock Read-write lock handle (must not be NULL)
 * @return 0 on success, cupolas_ERROR_BUSY if locked, negative on failure
 * @note Thread-safe: Yes
 * @reentrant No
 */
int cupolas_rwlock_trywrlock(cupolas_rwlock_t* rwlock);

/**
 * @brief Unlock read-write lock
 * @param[in] rwlock Read-write lock handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_rwlock_unlock(cupolas_rwlock_t* rwlock);

/* Condition Variable Interface */
/**
 * @brief Initialize condition variable
 * @param[out] cond Condition variable handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: N/A
 * @reentrant N/A
 * @ownership cond: callee initializes
 */
int cupolas_cond_init(cupolas_cond_t* cond);

/**
 * @brief Destroy condition variable
 * @param[in] cond Condition variable handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant N/A
 * @ownership cond: caller transfers ownership
 */
int cupolas_cond_destroy(cupolas_cond_t* cond);

/**
 * @brief Wait for condition
 * @param[in] cond Condition variable handle (must not be NULL)
 * @param[in] mutex Associated mutex (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant No
 * @post Atomically releases mutex and waits, then reacquires mutex on wake
 */
int cupolas_cond_wait(cupolas_cond_t* cond, cupolas_mutex_t* mutex);

/**
 * @brief Wait for condition with timeout
 * @param[in] cond Condition variable handle (must not be NULL)
 * @param[in] mutex Associated mutex (must not be NULL)
 * @param[in] timeout_ms Timeout in milliseconds
 * @return 0 on success, cupolas_ERROR_TIMEOUT on timeout, negative on failure
 * @note Thread-safe: Yes
 * @reentrant No
 */
int cupolas_cond_timedwait(cupolas_cond_t* cond, cupolas_mutex_t* mutex, uint32_t timeout_ms);

/**
 * @brief Signal condition variable (wake one)
 * @param[in] cond Condition variable handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_cond_signal(cupolas_cond_t* cond);

/**
 * @brief Broadcast condition variable (wake all)
 * @param[in] cond Condition variable handle (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_cond_broadcast(cupolas_cond_t* cond);

/* Thread Interface */
typedef void* (*cupolas_thread_func_t)(void* arg);

/**
 * @brief Create thread
 * @param[out] thread Thread handle output (must not be NULL)
 * @param[in] func Thread function (must not be NULL)
 * @param[in] arg Argument passed to thread function
 * @return 0 on success, negative on failure
 * @note Thread-safe: N/A
 * @reentrant N/A
 * @ownership thread: callee initializes, caller owns
 */
int cupolas_thread_create(cupolas_thread_t* thread, cupolas_thread_func_t func, void* arg);

/**
 * @brief Join thread
 * @param[in] thread Thread handle
 * @param[out] retval Return value from thread function (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant N/A
 * @ownership retval: callee writes if not NULL, caller owns
 */
int cupolas_thread_join(cupolas_thread_t thread, void** retval);

/**
 * @brief Detach thread
 * @param[in] thread Thread handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: No
 * @reentrant N/A
 */
int cupolas_thread_detach(cupolas_thread_t thread);

/**
 * @brief Get current thread ID
 * @return Current thread ID
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_thread_id_t cupolas_thread_self(void);

/**
 * @brief Compare thread IDs
 * @param[in] t1 First thread ID
 * @param[in] t2 Second thread ID
 * @return true if equal, false otherwise
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
bool cupolas_thread_equal(cupolas_thread_id_t t1, cupolas_thread_id_t t2);

/* ============================================================================
 * Process Primitives
 * ============================================================================ */

/* Process Handle Types */
#if cupolas_PLATFORM_WINDOWS
    typedef HANDLE cupolas_process_t;
    typedef DWORD cupolas_pid_t;
    typedef HANDLE cupolas_pipe_t;
#else
    typedef pid_t cupolas_pid_t;
    typedef int cupolas_process_t;
    typedef int cupolas_pipe_t[2];
#endif

/* Process Exit Status */
typedef struct cupolas_exit_status {
    int code;
    bool signaled;
    int signal;
} cupolas_exit_status_t;

/* Process Attributes */
typedef struct cupolas_process_attr {
    const char* working_dir;
    const char** env;
    bool redirect_stdin;
    bool redirect_stdout;
    bool redirect_stderr;
    cupolas_pipe_t stdin_pipe;
    cupolas_pipe_t stdout_pipe;
    cupolas_pipe_t stderr_pipe;
} cupolas_process_attr_t;

/* Process Interface */
/**
 * @brief Spawn child process
 * @param[out] proc Process handle output (must not be NULL)
 * @param[in] path Path to executable (must not be NULL)
 * @param[in] argv Argument vector (NULL-terminated, must not be NULL)
 * @param[in] attr Process attributes (may be NULL for defaults)
 * @return 0 on success, negative on failure
 * @note Thread-safe: N/A
 * @reentrant N/A
 * @ownership proc: callee initializes, caller owns
 */
int cupolas_process_spawn(cupolas_process_t* proc,
                        const char* path,
                        char* const argv[],
                        const cupolas_process_attr_t* attr);

/**
 * @brief Wait for process
 * @param[in] proc Process handle
 * @param[out] status Exit status output (must not be NULL)
 * @param[in] timeout_ms Timeout in milliseconds (0 for infinite)
 * @return 0 on success, cupolas_ERROR_TIMEOUT on timeout, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership status: callee writes, caller owns
 */
int cupolas_process_wait(cupolas_process_t proc, cupolas_exit_status_t* status, uint32_t timeout_ms);

/**
 * @brief Terminate process
 * @param[in] proc Process handle
 * @param[in] signal Signal to send (platform-specific)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_process_terminate(cupolas_process_t proc, int signal);

/**
 * @brief Close process handle
 * @param[in] proc Process handle
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_process_close(cupolas_process_t proc);

/**
 * @brief Get process ID
 * @param[in] proc Process handle
 * @return Process ID
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc);

/* Pipe Interface */
/**
 * @brief Create pipe
 * @param[out] pipe Pipe handles output (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: N/A
 * @reentrant N/A
 * @ownership pipe: callee initializes, caller owns
 */
int cupolas_pipe_create(cupolas_pipe_t* pipe);

/**
 * @brief Close pipe
 * @param[in] pipe Pipe handles
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_pipe_close(cupolas_pipe_t* pipe);

/**
 * @brief Read from pipe
 * @param[in] pipe Pipe handle
 * @param[out] buf Buffer to read into (must not be NULL)
 * @param[in] count Number of bytes to read
 * @param[out] bytes_read Bytes actually read (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership buf: caller owns; bytes_read: callee writes if not NULL
 */
int cupolas_pipe_read(cupolas_pipe_t* pipe, void* buf, size_t count, size_t* bytes_read);

/**
 * @brief Write to pipe
 * @param[in] pipe Pipe handle
 * @param[in] buf Data to write (must not be NULL)
 * @param[in] count Number of bytes to write
 * @param[out] bytes_written Bytes actually written (may be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership buf: caller retains; bytes_written: callee writes if not NULL
 */
int cupolas_pipe_write(cupolas_pipe_t* pipe, const void* buf, size_t count, size_t* bytes_written);

/* ============================================================================
 * Time Primitives
 * ============================================================================ */

/* Timestamp Structure */
typedef struct cupolas_timestamp {
    int64_t sec;
    int32_t nsec;
} cupolas_timestamp_t;

/* Time Interface */
/**
 * @brief Get current timestamp (wall clock)
 * @param[out] ts Timestamp output (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership ts: callee writes, caller owns
 */
int cupolas_time_now(cupolas_timestamp_t* ts);

/**
 * @brief Get monotonic timestamp
 * @param[out] ts Timestamp output (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership ts: callee writes, caller owns
 */
int cupolas_time_mono(cupolas_timestamp_t* ts);

/**
 * @brief Get current time in milliseconds
 * @return Current time in milliseconds since epoch
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
uint64_t cupolas_time_ms(void);

/**
 * @brief Sleep for milliseconds
 * @param[in] ms Milliseconds to sleep
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_sleep_ms(uint32_t ms);

/**
 * @brief Sleep for microseconds
 * @param[in] us Microseconds to sleep
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_sleep_us(uint32_t us);

/* ============================================================================
 * File System Primitives
 * ============================================================================ */

/* File Path Maximum Length */
#if cupolas_PLATFORM_WINDOWS
    #define cupolas_PATH_MAX  260
    #define cupolas_PATH_SEP  '\\'
    #define cupolas_PATH_SEP_STR "\\"
#else
    #define cupolas_PATH_MAX  4096
    #define cupolas_PATH_SEP  '/'
    #define cupolas_PATH_SEP_STR "/"
#endif

/* File Attributes */
typedef struct cupolas_file_stat {
    uint64_t size;
    cupolas_timestamp_t mtime;
    bool is_dir;
    bool is_regular;
    bool exists;
} cupolas_file_stat_t;

/* File System Interface */
/**
 * @brief Get file statistics
 * @param[in] path File path (must not be NULL)
 * @param[out] stat Statistics output (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership stat: callee writes, caller owns
 */
int cupolas_file_stat(const char* path, cupolas_file_stat_t* stat);

/**
 * @brief Check if file exists
 * @param[in] path File path (must not be NULL)
 * @return true if exists, false otherwise
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_file_exists(const char* path);

/**
 * @brief Create directory
 * @param[in] path Directory path (must not be NULL)
 * @param[in] recursive Create parent directories if needed
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_file_mkdir(const char* path, bool recursive);

/**
 * @brief Remove file or empty directory
 * @param[in] path Path to remove (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_file_remove(const char* path);

/**
 * @brief Rename file
 * @param[in] old_path Old path (must not be NULL)
 * @param[in] new_path New path (must not be NULL)
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_file_rename(const char* old_path, const char* new_path);

/**
 * @brief Get absolute path
 * @param[in] path Input path (must not be NULL)
 * @param[out] buf Output buffer (must not be NULL)
 * @param[in] size Buffer size in bytes
 * @return Buffer on success, NULL on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership buf: caller owns
 */
char* cupolas_file_abspath(const char* path, char* buf, size_t size);

/**
 * @brief Get directory name
 * @param[in] path Input path (must not be NULL)
 * @param[out] buf Output buffer (must not be NULL)
 * @param[in] size Buffer size in bytes
 * @return Buffer on success, NULL on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership buf: caller owns
 */
char* cupolas_file_dirname(const char* path, char* buf, size_t size);

/* ============================================================================
 * Memory Primitives
 * ============================================================================ */

/* Aligned Memory Allocation */
/**
 * @brief Allocate memory
 * @param[in] size Number of bytes to allocate
 * @return Pointer to allocated memory, NULL on failure
 * @note Thread-safe: Yes (heap operations are atomic)
 * @reentrant Yes
 */
void* cupolas_mem_alloc(size_t size);

/**
 * @brief Allocate aligned memory
 * @param[in] size Number of bytes to allocate
 * @param[in] alignment Alignment requirement (must be power of 2)
 * @return Pointer to allocated memory, NULL on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void* cupolas_mem_alloc_aligned(size_t size, size_t alignment);

/**
 * @brief Free memory
 * @param[in] ptr Pointer to memory (NULL is safe)
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_mem_free(void* ptr);

/**
 * @brief Reallocate memory
 * @param[in] ptr Original pointer (NULL is safe for alloc)
 * @param[in] size New size in bytes
 * @return Pointer to reallocated memory, NULL on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @post On failure, original pointer remains valid
 */
void* cupolas_mem_realloc(void* ptr, size_t size);

/* Secure Memory Operations */
/**
 * @brief Zero memory (secure erase)
 * @param[in] ptr Pointer to memory (must not be NULL)
 * @param[in] size Number of bytes to zero
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_mem_zero(void* ptr, size_t size);

/**
 * @brief Lock memory (prevent swapping)
 * @param[in] ptr Pointer to memory (must not be NULL)
 * @param[in] size Number of bytes to lock
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_mem_lock(void* ptr, size_t size);

/**
 * @brief Unlock memory
 * @param[in] ptr Pointer to memory (must not be NULL)
 * @param[in] size Number of bytes to unlock
 * @return 0 on success, negative on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_mem_unlock(void* ptr, size_t size);

/* ============================================================================
 * Utility Macros
 * ============================================================================ */

/* Unused Parameter Marker */
#ifndef cupolas_UNUSED
#define cupolas_UNUSED(x) ((void)(x))
#endif

/* ============================================================================
 * Atomic Operations
 * ============================================================================ */

typedef volatile int32_t cupolas_atomic32_t;
typedef volatile int64_t cupolas_atomic64_t;
typedef volatile void* cupolas_atomic_ptr_t;

/**
 * @brief Load 32-bit atomic value
 * @param[in] ptr Atomic variable (must not be NULL)
 * @return Value at ptr
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int32_t cupolas_atomic_load32(cupolas_atomic32_t* ptr);

/**
 * @brief Store 32-bit atomic value
 * @param[out] ptr Atomic variable (must not be NULL)
 * @param[in] val Value to store
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_atomic_store32(cupolas_atomic32_t* ptr, int32_t val);

/**
 * @brief Add to 32-bit atomic value
 * @param[inout] ptr Atomic variable (must not be NULL)
 * @param[in] delta Value to add
 * @return New value after addition
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int32_t cupolas_atomic_add32(cupolas_atomic32_t* ptr, int32_t delta);

/**
 * @brief Subtract from 32-bit atomic value
 * @param[inout] ptr Atomic variable (must not be NULL)
 * @param[in] delta Value to subtract
 * @return New value after subtraction
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int32_t cupolas_atomic_sub32(cupolas_atomic32_t* ptr, int32_t delta);

/**
 * @brief Increment 32-bit atomic value
 * @param[inout] ptr Atomic variable (must not be NULL)
 * @return Value after increment
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int32_t cupolas_atomic_inc32(cupolas_atomic32_t* ptr);

/**
 * @brief Decrement 32-bit atomic value
 * @param[inout] ptr Atomic variable (must not be NULL)
 * @return Value after decrement
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int32_t cupolas_atomic_dec32(cupolas_atomic32_t* ptr);

/**
 * @brief Compare and swap 32-bit atomic value
 * @param[inout] ptr Atomic variable (must not be NULL)
 * @param[in] expected Expected current value
 * @param[in] desired Desired new value
 * @return true if swapped, false if current value != expected
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
bool cupolas_atomic_cas32(cupolas_atomic32_t* ptr, int32_t expected, int32_t desired);

/**
 * @brief Load 64-bit atomic value
 * @param[in] ptr Atomic variable (must not be NULL)
 * @return Value at ptr
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int64_t cupolas_atomic_load64(cupolas_atomic64_t* ptr);

/**
 * @brief Store 64-bit atomic value
 * @param[out] ptr Atomic variable (must not be NULL)
 * @param[in] val Value to store
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_atomic_store64(cupolas_atomic64_t* ptr, int64_t val);

/**
 * @brief Add to 64-bit atomic value
 * @param[inout] ptr Atomic variable (must not be NULL)
 * @param[in] delta Value to add
 * @return New value after addition
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int64_t cupolas_atomic_add64(cupolas_atomic64_t* ptr, int64_t delta);

/**
 * @brief Subtract from 64-bit atomic value
 * @param[inout] ptr Atomic variable (must not be NULL)
 * @param[in] delta Value to subtract
 * @return New value after subtraction
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int64_t cupolas_atomic_sub64(cupolas_atomic64_t* ptr, int64_t delta);

/**
 * @brief Compare and swap 64-bit atomic value
 * @param[inout] ptr Atomic variable (must not be NULL)
 * @param[in] expected Expected current value
 * @param[in] desired Desired new value
 * @return true if swapped, false if current value != expected
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
bool cupolas_atomic_cas64(cupolas_atomic64_t* ptr, int64_t expected, int64_t desired);

/**
 * @brief Load pointer atomic value
 * @param[in] ptr Atomic pointer variable (must not be NULL)
 * @return Value at ptr
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void* cupolas_atomic_load_ptr(cupolas_atomic_ptr_t* ptr);

/**
 * @brief Store pointer atomic value
 * @param[out] ptr Atomic pointer variable (must not be NULL)
 * @param[in] val Value to store
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
void cupolas_atomic_store_ptr(cupolas_atomic_ptr_t* ptr, void* val);

/**
 * @brief Compare and swap pointer atomic value
 * @param[inout] ptr Atomic pointer variable (must not be NULL)
 * @param[in] expected Expected current value
 * @param[in] desired Desired new value
 * @return true if swapped, false if current value != expected
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
bool cupolas_atomic_cas_ptr(cupolas_atomic_ptr_t* ptr, void* expected, void* desired);

/* ============================================================================
 * Error Handling
 * ============================================================================ */

#ifndef cupolas_OK
#define cupolas_OK                    0
#endif
#ifndef cupolas_ERROR_UNKNOWN
#define cupolas_ERROR_UNKNOWN         -1
#endif
#ifndef cupolas_ERROR_INVALID_ARG
#define cupolas_ERROR_INVALID_ARG     -2
#endif
#ifndef cupolas_ERROR_NO_MEMORY
#define cupolas_ERROR_NO_MEMORY       -3
#endif
#ifndef cupolas_ERROR_NOT_FOUND
#define cupolas_ERROR_NOT_FOUND       -4
#endif
#ifndef cupolas_ERROR_PERMISSION
#define cupolas_ERROR_PERMISSION      -5
#endif
#ifndef cupolas_ERROR_BUSY
#define cupolas_ERROR_BUSY            -6
#endif
#ifndef cupolas_ERROR_TIMEOUT
#define cupolas_ERROR_TIMEOUT         -7
#endif
#ifndef cupolas_ERROR_WOULD_BLOCK
#define cupolas_ERROR_WOULD_BLOCK     -8
#endif
#ifndef cupolas_ERROR_OVERFLOW
#define cupolas_ERROR_OVERFLOW        -9
#endif
#ifndef cupolas_ERROR_NOT_SUPPORTED
#define cupolas_ERROR_NOT_SUPPORTED   -10
#endif
#ifndef cupolas_ERROR_IO
#define cupolas_ERROR_IO              -11
#endif

/**
 * @brief Get last error code
 * @return Last error code
 * @note Thread-safe: Yes (per-thread errno-style)
 * @reentrant Yes
 */
int cupolas_get_last_error(void);

/**
 * @brief Get error string
 * @param[in] error Error code
 * @return Error description string (static, do not free)
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
const char* cupolas_strerror(int error);

/* ============================================================================
 * String Utilities
 * ============================================================================ */

/**
 * @brief Duplicate string
 * @param[in] str String to duplicate (must not be NULL)
 * @return Duplicated string (caller owns, must free), NULL on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership Returned string: caller owns, must call cupolas_mem_free
 */
char* cupolas_strdup(const char* str);

/**
 * @brief Duplicate string with length limit
 * @param[in] str String to duplicate (must not be NULL)
 * @param[in] n Maximum length
 * @return Duplicated string (caller owns, must free), NULL on failure
 * @note Thread-safe: Yes
 * @reentrant Yes
 * @ownership Returned string: caller owns, must call cupolas_mem_free
 */
char* cupolas_strndup(const char* str, size_t n);

/**
 * @brief Case-insensitive string comparison
 * @param[in] s1 First string (must not be NULL)
 * @param[in] s2 Second string (must not be NULL)
 * @return Comparison result (like strcmp)
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_strcasecmp(const char* s1, const char* s2);

/**
 * @brief Case-insensitive string comparison with length limit
 * @param[in] s1 First string (must not be NULL)
 * @param[in] s2 Second string (must not be NULL)
 * @param[in] n Maximum length to compare
 * @return Comparison result (like strncmp)
 * @note Thread-safe: Yes
 * @reentrant Yes
 */
int cupolas_strncasecmp(const char* s1, const char* s2, size_t n);

/* ============================================================================
 * One-Time Initialization
 * ============================================================================ */

#if cupolas_PLATFORM_WINDOWS
typedef INIT_ONCE cupolas_once_t;
#define CUPOLAS_ONCE_INIT INIT_ONCE_STATIC_INIT
#else
typedef pthread_once_t cupolas_once_t;
#define CUPOLAS_ONCE_INIT PTHREAD_ONCE_INIT
#endif

static inline void cupolas_call_once(cupolas_once_t* once, void (*func)(void)) {
#if cupolas_PLATFORM_WINDOWS
    InitOnceExecuteOnce(once, (PINIT_ONCE_FN)(void*)func, NULL, NULL);
#else
    pthread_once(once, func);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* cupolas_PLATFORM_H */
