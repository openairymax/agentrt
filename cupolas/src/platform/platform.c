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
 * Mutex Implementation
 * ============================================================================ */

/**
 * @brief Initialize mutex
 * @param[in] mutex Mutex handle to initialize
 * @return 0 on success, negative on failure
 */
int cupolas_mutex_init(cupolas_mutex_t* mutex) {
    return platform_mutex_init((platform_mutex_t*)mutex);
}

/**
 * @brief Destroy mutex
 * @param[in] mutex Mutex handle to destroy
 * @return 0 on success, negative on failure
 */
int cupolas_mutex_destroy(cupolas_mutex_t* mutex) {
    return platform_mutex_destroy((platform_mutex_t*)mutex);
}

/**
 * @brief Lock mutex
 * @param[in] mutex Mutex handle to lock
 * @return 0 on success, negative on failure
 */
int cupolas_mutex_lock(cupolas_mutex_t* mutex) {
    return platform_mutex_lock((platform_mutex_t*)mutex);
}

/**
 * @brief Try lock mutex (non-blocking)
 * @param[in] mutex Mutex handle
 * @return 0 on success, cupolas_ERROR_BUSY if locked, negative on failure
 */
int cupolas_mutex_trylock(cupolas_mutex_t* mutex) {
    return platform_mutex_trylock((platform_mutex_t*)mutex);
}

/**
 * @brief Unlock mutex
 * @param[in] mutex Mutex handle to unlock
 * @return 0 on success, negative on failure
 */
int cupolas_mutex_unlock(cupolas_mutex_t* mutex) {
    return platform_mutex_unlock((platform_mutex_t*)mutex);
}

/* ============================================================================
 * Read-Write Lock Implementation
 * ============================================================================ */

/**
 * @brief Initialize read-write lock
 * @param[in] rwlock Read-write lock handle to initialize
 * @return 0 on success, negative on failure
 */
int cupolas_rwlock_init(cupolas_rwlock_t* rwlock) {
    return platform_rwlock_init((platform_rwlock_t*)rwlock);
}

/**
 * @brief Destroy read-write lock
 * @param[in] rwlock Read-write lock handle to destroy
 * @return 0 on success, negative on failure
 */
int cupolas_rwlock_destroy(cupolas_rwlock_t* rwlock) {
    return platform_rwlock_destroy((platform_rwlock_t*)rwlock);
}

/**
 * @brief Acquire read lock
 * @param[in] rwlock Read-write lock handle
 * @return 0 on success, negative on failure
 */
int cupolas_rwlock_rdlock(cupolas_rwlock_t* rwlock) {
    return platform_rwlock_rdlock((platform_rwlock_t*)rwlock);
}

/**
 * @brief Acquire write lock
 * @param[in] rwlock Read-write lock handle
 * @return 0 on success, negative on failure
 */
int cupolas_rwlock_wrlock(cupolas_rwlock_t* rwlock) {
    return platform_rwlock_wrlock((platform_rwlock_t*)rwlock);
}

/**
 * @brief Try acquire read lock (non-blocking)
 * @param[in] rwlock Read-write lock handle
 * @return 0 on success, cupolas_ERROR_BUSY if locked, negative on failure
 */
int cupolas_rwlock_tryrdlock(cupolas_rwlock_t* rwlock) {
    return platform_rwlock_tryrdlock((platform_rwlock_t*)rwlock);
}

/**
 * @brief Try acquire write lock (non-blocking)
 * @param[in] rwlock Read-write lock handle
 * @return 0 on success, cupolas_ERROR_BUSY if locked, negative on failure
 */
int cupolas_rwlock_trywrlock(cupolas_rwlock_t* rwlock) {
    return platform_rwlock_trywrlock((platform_rwlock_t*)rwlock);
}

/**
 * @brief Unlock read-write lock
 * @param[in] rwlock Read-write lock handle to unlock
 * @return 0 on success, negative on failure
 */
int cupolas_rwlock_unlock(cupolas_rwlock_t* rwlock) {
    return platform_rwlock_unlock((platform_rwlock_t*)rwlock);
}

/* ============================================================================
 * Condition Variable Implementation
 * ============================================================================ */

/**
 * @brief Initialize condition variable
 * @param[in] cond Condition variable to initialize
 * @return 0 on success, negative on failure
 */
int cupolas_cond_init(cupolas_cond_t* cond) {
    return platform_cond_init((platform_cond_t*)cond);
}

/**
 * @brief Destroy condition variable
 * @param[in] cond Condition variable to destroy
 * @return 0 on success, negative on failure
 */
int cupolas_cond_destroy(cupolas_cond_t* cond) {
    return platform_cond_destroy((platform_cond_t*)cond);
}

/**
 * @brief Wait for condition variable
 * @param[in] cond Condition variable handle
 * @param[in] mutex Associated mutex handle
 * @return 0 on success, negative on failure
 */
int cupolas_cond_wait(cupolas_cond_t* cond, cupolas_mutex_t* mutex) {
    return platform_cond_wait((platform_cond_t*)cond, (platform_mutex_t*)mutex);
}

/**
 * @brief Wait for condition with timeout
 * @param[in] cond Condition variable handle
 * @param[in] mutex Associated mutex handle
 * @param[in] timeout_ms Timeout in milliseconds
 * @return 0 on success, cupolas_ERROR_TIMEOUT on timeout, negative on failure
 */
int cupolas_cond_timedwait(cupolas_cond_t* cond, cupolas_mutex_t* mutex, uint32_t timeout_ms) {
    return platform_cond_timedwait((platform_cond_t*)cond, (platform_mutex_t*)mutex, timeout_ms);
}

/**
 * @brief Signal condition variable (wake one thread)
 * @param[in] cond Condition variable handle
 * @return 0 on success, negative on failure
 */
int cupolas_cond_signal(cupolas_cond_t* cond) {
    return platform_cond_signal((platform_cond_t*)cond);
}

/**
 * @brief Broadcast condition variable (wake all threads)
 * @param[in] cond Condition variable handle
 * @return 0 on success, negative on failure
 */
int cupolas_cond_broadcast(cupolas_cond_t* cond) {
    return platform_cond_broadcast((platform_cond_t*)cond);
}

/* ============================================================================
 * Thread Implementation
 * ============================================================================ */

/**
 * @brief Create new thread
 * @param[out] thread Thread handle output
 * @param[in] func Thread function to execute
 * @param[in] arg Argument to pass to thread function
 * @return 0 on success, negative on failure
 */
int cupolas_thread_create(cupolas_thread_t* thread, cupolas_thread_func_t func, void* arg) {
    return platform_thread_create((platform_thread_t*)thread, func, arg);
}

/**
 * @brief Join thread (wait for completion)
 * @param[in] thread Thread handle to join
 * @param[out] retval Return value from thread function
 * @return 0 on success, negative on failure
 */
int cupolas_thread_join(cupolas_thread_t thread, void** retval) {
    return platform_thread_join((platform_thread_t)thread, retval);
}

/**
 * @brief Detach thread (allow independent execution)
 * @param[in] thread Thread handle to detach
 * @return 0 on success, negative on failure
 */
int cupolas_thread_detach(cupolas_thread_t thread) {
    return platform_thread_detach((platform_thread_t)thread);
}

/**
 * @brief Get current thread ID
 * @return Current thread ID
 */
cupolas_thread_id_t cupolas_thread_self(void) {
    return platform_thread_self();
}

/**
 * @brief Compare two thread IDs
 * @param[in] t1 First thread ID
 * @param[in] t2 Second thread ID
 * @return true if equal, false otherwise
 */
bool cupolas_thread_equal(cupolas_thread_id_t t1, cupolas_thread_id_t t2) {
    return platform_thread_equal(t1, t2);
}

/* ============================================================================
 * Process Implementation
 * ============================================================================ */

/**
 * @brief Spawn child process
 * @param[out] proc Process handle output
 * @param[in] path Path to executable
 * @param[in] argv Argument vector (NULL-terminated)
 * @param[in] attr Process attributes (may be NULL)
 * @return 0 on success, negative on failure
 */
int cupolas_process_spawn(cupolas_process_t* proc,
                        const char* path,
                        char* const argv[],
                        const cupolas_process_attr_t* attr) {
    return platform_process_spawn((platform_process_t*)proc, path, argv,
                                  (const platform_process_attr_t*)attr);
}

/**
 * @brief Wait for process completion
 * @param[in] proc Process handle
 * @param[out] status Exit status output
 * @param[in] timeout_ms Timeout in milliseconds (0 = infinite)
 * @return 0 on success, cupolas_ERROR_TIMEOUT on timeout, negative on failure
 */
int cupolas_process_wait(cupolas_process_t proc, cupolas_exit_status_t* status, uint32_t timeout_ms) {
    return platform_process_wait((platform_process_t)proc,
                                 (platform_exit_status_t*)status, timeout_ms);
}

/**
 * @brief Terminate process
 * @param[in] proc Process handle
 * @param[in] signal Signal number (platform-specific)
 * @return 0 on success, negative on failure
 */
int cupolas_process_terminate(cupolas_process_t proc, int signal) {
    return platform_process_terminate((platform_process_t)proc, signal);
}

/**
 * @brief Close process handle
 * @param[in] proc Process handle to close
 * @return 0 on success, negative on failure
 */
int cupolas_process_close(cupolas_process_t proc) {
    return platform_process_close((platform_process_t)proc);
}

/**
 * @brief Get process ID
 * @param[in] proc Process handle
 * @return Process ID
 */
cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc) {
    return platform_process_getpid((platform_process_t)proc);
}

/* ============================================================================
 * Pipe Implementation
 * ============================================================================ */

/**
 * @brief Create pipe
 * @param[out] pipe Pipe handles output
 * @return 0 on success, negative on failure
 */
int cupolas_pipe_create(cupolas_pipe_t* pipe) {
    return platform_pipe_create((platform_pipe_t*)pipe);
}

/**
 * @brief Close pipe
 * @param[in] pipe Pipe handles to close
 * @return 0 on success, negative on failure
 */
int cupolas_pipe_close(cupolas_pipe_t* pipe) {
    return platform_pipe_close((platform_pipe_t*)pipe);
}

/**
 * @brief Read from pipe
 * @param[in] pipe Pipe handle
 * @param[out] buf Buffer to read into
 * @param[in] count Number of bytes to read
 * @param[out] bytes_read Actual bytes read
 * @return 0 on success, negative on failure
 */
int cupolas_pipe_read(cupolas_pipe_t* pipe, void* buf, size_t count, size_t* bytes_read) {
    return platform_pipe_read((platform_pipe_t*)pipe, buf, count, bytes_read);
}

/**
 * @brief Write to pipe
 * @param[in] pipe Pipe handle
 * @param[in] buf Data buffer to write
 * @param[in] count Number of bytes to write
 * @param[out] bytes_written Actual bytes written
 * @return 0 on success, negative on failure
 */
int cupolas_pipe_write(cupolas_pipe_t* pipe, const void* buf, size_t count, size_t* bytes_written) {
    return platform_pipe_write((platform_pipe_t*)pipe, buf, count, bytes_written);
}

/* ============================================================================
 * Time Implementation
 * ============================================================================ */

/**
 * @brief Get current wall clock timestamp
 * @param[out] ts Timestamp output
 * @return 0 on success, negative on failure
 */
int cupolas_time_now(cupolas_timestamp_t* ts) {
    return platform_time_now((platform_timestamp_t*)ts);
}

/**
 * @brief Get monotonic timestamp
 * @param[out] ts Timestamp output
 * @return 0 on success, negative on failure
 */
int cupolas_time_mono(cupolas_timestamp_t* ts) {
    return platform_time_mono((platform_timestamp_t*)ts);
}

/**
 * @brief Get current time in milliseconds since epoch
 * @return Time in milliseconds
 */
uint64_t cupolas_time_ms(void) {
    return platform_get_current_time_ms();
}

/**
 * @brief Sleep for specified milliseconds
 * @param[in] ms Milliseconds to sleep
 */
void cupolas_sleep_ms(uint32_t ms) {
    platform_sleep_ms(ms);
}

/**
 * @brief Sleep for specified microseconds
 * @param[in] us Microseconds to sleep
 */
void cupolas_sleep_us(uint32_t us) {
    platform_sleep_us(us);
}

/* ============================================================================
 * File System Implementation
 * ============================================================================ */

/**
 * @brief Get file statistics
 * @param[in] path File path
 * @param[out] stat Statistics output
 * @return 0 on success, negative on failure
 */
int cupolas_file_stat(const char* path, cupolas_file_stat_t* stat) {
    return platform_file_stat(path, (platform_file_stat_t*)stat);
}

/**
 * @brief Check if file exists
 * @param[in] path File path
 * @return Non-zero if exists, 0 otherwise
 */
int cupolas_file_exists(const char* path) {
    return platform_file_exists(path);
}

/**
 * @brief Create directory
 * @param[in] path Directory path
 * @param[in] recursive Create parent directories if needed
 * @return 0 on success, negative on failure
 */
int cupolas_file_mkdir(const char* path, bool recursive) {
    return platform_file_mkdir(path, recursive);
}

/**
 * @brief Remove file or empty directory
 * @param[in] path Path to remove
 * @return 0 on success, negative on failure
 */
int cupolas_file_remove(const char* path) {
    return platform_file_remove(path);
}

/**
 * @brief Rename file
 * @param[in] old_path Original path
 * @param[in] new_path New path
 * @return 0 on success, negative on failure
 */
int cupolas_file_rename(const char* old_path, const char* new_path) {
    return platform_file_rename(old_path, new_path);
}

/**
 * @brief Get absolute path
 * @param[in] path Input path
 * @param[out] buf Output buffer
 * @param[in] size Buffer size
 * @return Pointer to buffer on success, NULL on failure
 */
char* cupolas_file_abspath(const char* path, char* buf, size_t size) {
    return platform_file_abspath(path, buf, size);
}

/**
 * @brief Get directory name from path
 * @param[in] path Input path
 * @param[out] buf Output buffer
 * @param[in] size Buffer size
 * @return Pointer to buffer on success, NULL on failure
 */
char* cupolas_file_dirname(const char* path, char* buf, size_t size) {
    return platform_file_dirname(path, buf, size);
}

/* ============================================================================
 * Memory Implementation
 * ============================================================================ */

/**
 * @brief Allocate memory
 * @param[in] size Number of bytes to allocate
 * @return Pointer to allocated memory, NULL on failure
 */
void* cupolas_mem_alloc(size_t size) {
    return platform_mem_alloc(size);
}

/**
 * @brief Allocate aligned memory
 * @param[in] size Number of bytes
 * @param[in] alignment Alignment requirement (power of 2)
 * @return Pointer to allocated memory, NULL on failure
 */
void* cupolas_mem_alloc_aligned(size_t size, size_t alignment) {
    return platform_mem_alloc_aligned(size, alignment);
}

/**
 * @brief Free memory
 * @param[in] ptr Pointer to free (NULL safe)
 */
void cupolas_mem_free(void* ptr) {
    platform_mem_free(ptr);
}

/**
 * @brief Reallocate memory
 * @param[in] ptr Original pointer (NULL safe for alloc)
 * @param[in] size New size
 * @return Pointer to reallocated memory, NULL on failure
 */
void* cupolas_mem_realloc(void* ptr, size_t size) {
    return platform_mem_realloc(ptr, size);
}

/**
 * @brief Zero memory securely
 * @param[in] ptr Memory pointer
 * @param[in] size Number of bytes to zero
 */
void cupolas_mem_zero(void* ptr, size_t size) {
    platform_mem_zero(ptr, size);
}

/**
 * @brief Lock memory (prevent swapping)
 * @param[in] ptr Memory pointer
 * @param[in] size Number of bytes
 */
void cupolas_mem_lock(void* ptr, size_t size) {
    platform_mem_lock(ptr, size);
}

/**
 * @brief Unlock memory
 * @param[in] ptr Memory pointer
 * @param[in] size Number of bytes
 */
void cupolas_mem_unlock(void* ptr, size_t size) {
    platform_mem_unlock(ptr, size);
}

/* ============================================================================
 * Atomic Operations Implementation
 * ============================================================================ */

/**
 * @brief Load 32-bit atomic value
 * @param[in] ptr Atomic variable
 * @return Value at ptr
 */
int32_t cupolas_atomic_load32(cupolas_atomic32_t* ptr) {
    return platform_atomic_load32((platform_atomic32_t*)ptr);
}

/**
 * @brief Store 32-bit atomic value
 * @param[out] ptr Atomic variable
 * @param[in] val Value to store
 */
void cupolas_atomic_store32(cupolas_atomic32_t* ptr, int32_t val) {
    platform_atomic_store32((platform_atomic32_t*)ptr, val);
}

/**
 * @brief Add to 32-bit atomic value
 * @param[inout] ptr Atomic variable
 * @param[in] delta Value to add
 * @return New value after addition
 */
int32_t cupolas_atomic_add32(cupolas_atomic32_t* ptr, int32_t delta) {
    return platform_atomic_add32((platform_atomic32_t*)ptr, delta);
}

/**
 * @brief Subtract from 32-bit atomic value
 * @param[inout] ptr Atomic variable
 * @param[in] delta Value to subtract
 * @return New value after subtraction
 */
int32_t cupolas_atomic_sub32(cupolas_atomic32_t* ptr, int32_t delta) {
    return platform_atomic_sub32((platform_atomic32_t*)ptr, delta);
}

/**
 * @brief Increment 32-bit atomic value
 * @param[inout] ptr Atomic variable
 * @return Value after increment
 */
int32_t cupolas_atomic_inc32(cupolas_atomic32_t* ptr) {
    return platform_atomic_inc32((platform_atomic32_t*)ptr);
}

/**
 * @brief Decrement 32-bit atomic value
 * @param[inout] ptr Atomic variable
 * @return Value after decrement
 */
int32_t cupolas_atomic_dec32(cupolas_atomic32_t* ptr) {
    return platform_atomic_dec32((platform_atomic32_t*)ptr);
}

/**
 * @brief Compare and swap 32-bit atomic value
 * @param[inout] ptr Atomic variable
 * @param[in] expected Expected current value
 * @param[in] desired Desired new value
 * @return true if swapped, false otherwise
 */
bool cupolas_atomic_cas32(cupolas_atomic32_t* ptr, int32_t expected, int32_t desired) {
    return platform_atomic_cas32((platform_atomic32_t*)ptr, expected, desired);
}

/**
 * @brief Load 64-bit atomic value
 * @param[in] ptr Atomic variable
 * @return Value at ptr
 */
int64_t cupolas_atomic_load64(cupolas_atomic64_t* ptr) {
    return platform_atomic_load64((platform_atomic64_t*)ptr);
}

/**
 * @brief Store 64-bit atomic value
 * @param[out] ptr Atomic variable
 * @param[in] val Value to store
 */
void cupolas_atomic_store64(cupolas_atomic64_t* ptr, int64_t val) {
    platform_atomic_store64((platform_atomic64_t*)ptr, val);
}

/**
 * @brief Add to 64-bit atomic value
 * @param[inout] ptr Atomic variable
 * @param[in] delta Value to add
 * @return New value after addition
 */
int64_t cupolas_atomic_add64(cupolas_atomic64_t* ptr, int64_t delta) {
    return platform_atomic_add64((platform_atomic64_t*)ptr, delta);
}

/**
 * @brief Subtract from 64-bit atomic value
 * @param[inout] ptr Atomic variable
 * @param[in] delta Value to subtract
 * @return New value after subtraction
 */
int64_t cupolas_atomic_sub64(cupolas_atomic64_t* ptr, int64_t delta) {
    return platform_atomic_sub64((platform_atomic64_t*)ptr, delta);
}

/**
 * @brief Compare and swap 64-bit atomic value
 * @param[inout] ptr Atomic variable
 * @param[in] expected Expected current value
 * @param[in] desired Desired new value
 * @return true if swapped, false otherwise
 */
bool cupolas_atomic_cas64(cupolas_atomic64_t* ptr, int64_t expected, int64_t desired) {
    return platform_atomic_cas64((platform_atomic64_t*)ptr, expected, desired);
}

/**
 * @brief Load pointer atomic value
 * @param[in] ptr Atomic pointer variable
 * @return Value at ptr
 */
void* cupolas_atomic_load_ptr(cupolas_atomic_ptr_t* ptr) {
    return platform_atomic_load_ptr((platform_atomic_ptr_t*)ptr);
}

/**
 * @brief Store pointer atomic value
 * @param[out] ptr Atomic pointer variable
 * @param[in] val Value to store
 */
void cupolas_atomic_store_ptr(cupolas_atomic_ptr_t* ptr, void* val) {
    platform_atomic_store_ptr((platform_atomic_ptr_t*)ptr, val);
}

/**
 * @brief Compare and swap pointer atomic value
 * @param[inout] ptr Atomic pointer variable
 * @param[in] expected Expected current value
 * @param[in] desired Desired new value
 * @return true if swapped, false otherwise
 */
bool cupolas_atomic_cas_ptr(cupolas_atomic_ptr_t* ptr, void* expected, void* desired) {
    return platform_atomic_cas_ptr((platform_atomic_ptr_t*)ptr, expected, desired);
}

/* ============================================================================
 * String Utilities Implementation
 * ============================================================================ */

/**
 * @brief Duplicate string
 * @param[in] str String to duplicate
 * @return Duplicated string (caller owns), NULL on failure
 */
char* cupolas_strdup(const char* str) {
    return platform_strdup(str);
}

/**
 * @brief Duplicate string with length limit
 * @param[in] str String to duplicate
 * @param[in] n Maximum length
 * @return Duplicated string (caller owns), NULL on failure
 */
char* cupolas_strndup(const char* str, size_t n) {
    return platform_strndup(str, n);
}

/**
 * @brief Case-insensitive string comparison
 * @param[in] s1 First string
 * @param[in] s2 Second string
 * @return Comparison result (like strcmp)
 */
int cupolas_strcasecmp(const char* s1, const char* s2) {
    return platform_strcasecmp(s1, s2);
}

/**
 * @brief Case-insensitive string comparison with length limit
 * @param[in] s1 First string
 * @param[in] s2 Second string
 * @param[in] n Maximum length to compare
 * @return Comparison result (like strncmp)
 */
int cupolas_strncasecmp(const char* s1, const char* s2, size_t n) {
    return platform_strncasecmp(s1, s2, n);
}

/* ============================================================================
 * Error Handling Implementation
 * ============================================================================ */

/**
 * @brief Get last error code (per-thread)
 * @return Last error code
 */
int cupolas_get_last_error(void) {
    return platform_get_last_error();
}

/**
 * @brief Get error description string
 * @param[in] error Error code
 * @return Static error description string
 */
const char* cupolas_strerror(int error) {
    return platform_strerror(error);
}
