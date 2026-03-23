/**
 * @file platform.h
 * @brief 跨平台抽象层 - 统一封装 Windows/POSIX 差异
 * @author Spharx
 * @date 2024
 * 
 * 设计原则：
 * - 单一职责：仅处理平台差异
 * - 零开销：内联函数 + 宏定义
 * - 类型安全：强类型封装
 */

#ifndef DOMES_PLATFORM_H
#define DOMES_PLATFORM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 平台检测
 * ============================================================================ */

#if defined(_WIN32) || defined(_WIN64)
    #define DOMES_PLATFORM_WINDOWS  1
    #define DOMES_PLATFORM_POSIX    0
    #ifdef _WIN64
        #define DOMES_PLATFORM_64BIT 1
    #else
        #define DOMES_PLATFORM_64BIT 0
    #endif
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    #define DOMES_PLATFORM_WINDOWS  0
    #define DOMES_PLATFORM_POSIX    1
    #if defined(__x86_64__) || defined(__aarch64__)
        #define DOMES_PLATFORM_64BIT 1
    #else
        #define DOMES_PLATFORM_64BIT 0
    #endif
#else
    #error "Unsupported platform"
#endif

/* ============================================================================
 * 导出宏
 * ============================================================================ */

#if DOMES_PLATFORM_WINDOWS
    #ifdef DOMES_BUILD_DLL
        #define DOMES_API __declspec(dllexport)
    #else
        #define DOMES_API __declspec(dllimport)
    #endif
#else
    #define DOMES_API __attribute__((visibility("default")))
#endif

/* ============================================================================
 * 线程原语
 * ============================================================================ */

/* 线程句柄类型 */
#if DOMES_PLATFORM_WINDOWS
    #include <windows.h>
    typedef HANDLE domes_thread_t;
    typedef DWORD domes_thread_id_t;
    typedef CRITICAL_SECTION domes_mutex_t;
    typedef SRWLOCK domes_rwlock_t;
    typedef CONDITION_VARIABLE domes_cond_t;
#else
    #include <pthread.h>
    #include <sys/types.h>
    typedef pthread_t domes_thread_t;
    typedef pthread_t domes_thread_id_t;
    typedef pthread_mutex_t domes_mutex_t;
    typedef pthread_rwlock_t domes_rwlock_t;
    typedef pthread_cond_t domes_cond_t;
#endif

/* 互斥锁接口 */
int domes_mutex_init(domes_mutex_t* mutex);
int domes_mutex_destroy(domes_mutex_t* mutex);
int domes_mutex_lock(domes_mutex_t* mutex);
int domes_mutex_trylock(domes_mutex_t* mutex);
int domes_mutex_unlock(domes_mutex_t* mutex);

/* 读写锁接口 */
int domes_rwlock_init(domes_rwlock_t* rwlock);
int domes_rwlock_destroy(domes_rwlock_t* rwlock);
int domes_rwlock_rdlock(domes_rwlock_t* rwlock);
int domes_rwlock_wrlock(domes_rwlock_t* rwlock);
int domes_rwlock_tryrdlock(domes_rwlock_t* rwlock);
int domes_rwlock_trywrlock(domes_rwlock_t* rwlock);
int domes_rwlock_unlock(domes_rwlock_t* rwlock);

/* 条件变量接口 */
int domes_cond_init(domes_cond_t* cond);
int domes_cond_destroy(domes_cond_t* cond);
int domes_cond_wait(domes_cond_t* cond, domes_mutex_t* mutex);
int domes_cond_timedwait(domes_cond_t* cond, domes_mutex_t* mutex, uint32_t timeout_ms);
int domes_cond_signal(domes_cond_t* cond);
int domes_cond_broadcast(domes_cond_t* cond);

/* 线程接口 */
typedef void* (*domes_thread_func_t)(void* arg);
int domes_thread_create(domes_thread_t* thread, domes_thread_func_t func, void* arg);
int domes_thread_join(domes_thread_t thread, void** retval);
int domes_thread_detach(domes_thread_t thread);
domes_thread_id_t domes_thread_self(void);
bool domes_thread_equal(domes_thread_id_t t1, domes_thread_id_t t2);

/* ============================================================================
 * 进程原语
 * ============================================================================ */

/* 进程句柄类型 */
#if DOMES_PLATFORM_WINDOWS
    typedef HANDLE domes_process_t;
    typedef DWORD domes_pid_t;
    typedef HANDLE domes_pipe_t;
#else
    typedef pid_t domes_pid_t;
    typedef int domes_process_t;
    typedef int domes_pipe_t[2];
#endif

/* 进程退出状态 */
typedef struct domes_exit_status {
    int code;
    bool signaled;
    int signal;
} domes_exit_status_t;

/* 进程属性 */
typedef struct domes_process_attr {
    const char* working_dir;
    const char** env;
    bool redirect_stdin;
    bool redirect_stdout;
    bool redirect_stderr;
    domes_pipe_t stdin_pipe;
    domes_pipe_t stdout_pipe;
    domes_pipe_t stderr_pipe;
} domes_process_attr_t;

/* 进程接口 */
int domes_process_spawn(domes_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const domes_process_attr_t* attr);
int domes_process_wait(domes_process_t proc, domes_exit_status_t* status, uint32_t timeout_ms);
int domes_process_terminate(domes_process_t proc, int signal);
int domes_process_close(domes_process_t proc);
domes_pid_t domes_process_getpid(domes_process_t proc);

/* 管道接口 */
int domes_pipe_create(domes_pipe_t* pipe);
int domes_pipe_close(domes_pipe_t* pipe);
int domes_pipe_read(domes_pipe_t* pipe, void* buf, size_t count, size_t* bytes_read);
int domes_pipe_write(domes_pipe_t* pipe, const void* buf, size_t count, size_t* bytes_written);

/* ============================================================================
 * 时间原语
 * ============================================================================ */

/* 时间戳结构 */
typedef struct domes_timestamp {
    int64_t sec;
    int32_t nsec;
} domes_timestamp_t;

/* 时间接口 */
int domes_time_now(domes_timestamp_t* ts);
int domes_time_mono(domes_timestamp_t* ts);
uint64_t domes_time_ms(void);
void domes_sleep_ms(uint32_t ms);
void domes_sleep_us(uint32_t us);

/* ============================================================================
 * 文件系统原语
 * ============================================================================ */

/* 文件路径最大长度 */
#if DOMES_PLATFORM_WINDOWS
    #define DOMES_PATH_MAX  260
    #define DOMES_PATH_SEP  '\\'
    #define DOMES_PATH_SEP_STR "\\"
#else
    #define DOMES_PATH_MAX  4096
    #define DOMES_PATH_SEP  '/'
    #define DOMES_PATH_SEP_STR "/"
#endif

/* 文件属性 */
typedef struct domes_file_stat {
    uint64_t size;
    domes_timestamp_t mtime;
    bool is_dir;
    bool is_regular;
    bool exists;
} domes_file_stat_t;

/* 文件系统接口 */
int domes_file_stat(const char* path, domes_file_stat_t* stat);
int domes_file_exists(const char* path);
int domes_file_mkdir(const char* path, bool recursive);
int domes_file_remove(const char* path);
int domes_file_rename(const char* old_path, const char* new_path);
char* domes_file_abspath(const char* path, char* buf, size_t size);
char* domes_file_dirname(const char* path, char* buf, size_t size);

/* ============================================================================
 * 内存原语
 * ============================================================================ */

/* 对齐内存分配 */
void* domes_mem_alloc(size_t size);
void* domes_mem_alloc_aligned(size_t size, size_t alignment);
void domes_mem_free(void* ptr);
void* domes_mem_realloc(void* ptr, size_t size);

/* 安全内存操作 */
void domes_mem_zero(void* ptr, size_t size);
void domes_mem_lock(void* ptr, size_t size);
void domes_mem_unlock(void* ptr, size_t size);

/* ============================================================================
 * 原子操作
 * ============================================================================ */

typedef volatile int32_t domes_atomic32_t;
typedef volatile int64_t domes_atomic64_t;
typedef volatile void* domes_atomic_ptr_t;

int32_t domes_atomic_load32(domes_atomic32_t* ptr);
void domes_atomic_store32(domes_atomic32_t* ptr, int32_t val);
int32_t domes_atomic_add32(domes_atomic32_t* ptr, int32_t delta);
int32_t domes_atomic_sub32(domes_atomic32_t* ptr, int32_t delta);
int32_t domes_atomic_inc32(domes_atomic32_t* ptr);
int32_t domes_atomic_dec32(domes_atomic32_t* ptr);
bool domes_atomic_cas32(domes_atomic32_t* ptr, int32_t expected, int32_t desired);

int64_t domes_atomic_load64(domes_atomic64_t* ptr);
void domes_atomic_store64(domes_atomic64_t* ptr, int64_t val);
int64_t domes_atomic_add64(domes_atomic64_t* ptr, int64_t delta);
bool domes_atomic_cas64(domes_atomic64_t* ptr, int64_t expected, int64_t desired);

void* domes_atomic_load_ptr(domes_atomic_ptr_t* ptr);
void domes_atomic_store_ptr(domes_atomic_ptr_t* ptr, void* val);
bool domes_atomic_cas_ptr(domes_atomic_ptr_t* ptr, void* expected, void* desired);

/* ============================================================================
 * 错误处理
 * ============================================================================ */

/* 错误码 */
#define DOMES_OK                    0
#define DOMES_ERROR_UNKNOWN         -1
#define DOMES_ERROR_INVALID_ARG     -2
#define DOMES_ERROR_NO_MEMORY       -3
#define DOMES_ERROR_NOT_FOUND       -4
#define DOMES_ERROR_PERMISSION      -5
#define DOMES_ERROR_BUSY            -6
#define DOMES_ERROR_TIMEOUT         -7
#define DOMES_ERROR_WOULD_BLOCK     -8
#define DOMES_ERROR_OVERFLOW        -9
#define DOMES_ERROR_NOT_SUPPORTED   -10
#define DOMES_ERROR_IO              -11

/* 获取最后错误 */
int domes_get_last_error(void);
const char* domes_strerror(int error);

/* ============================================================================
 * 字符串工具
 * ============================================================================ */

char* domes_strdup(const char* str);
char* domes_strndup(const char* str, size_t n);
int domes_strcasecmp(const char* s1, const char* s2);
int domes_strncasecmp(const char* s1, const char* s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* DOMES_PLATFORM_H */
