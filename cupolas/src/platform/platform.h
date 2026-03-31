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

#ifndef cupolas_PLATFORM_H
#define cupolas_PLATFORM_H

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
 * 导出宏
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
 * 线程原语
 * ============================================================================ */

/* 线程句柄类型 */
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

/* 互斥锁接口 */
int cupolas_mutex_init(cupolas_mutex_t* mutex);
int cupolas_mutex_destroy(cupolas_mutex_t* mutex);
int cupolas_mutex_lock(cupolas_mutex_t* mutex);
int cupolas_mutex_trylock(cupolas_mutex_t* mutex);
int cupolas_mutex_unlock(cupolas_mutex_t* mutex);

/* 读写锁接口 */
int cupolas_rwlock_init(cupolas_rwlock_t* rwlock);
int cupolas_rwlock_destroy(cupolas_rwlock_t* rwlock);
int cupolas_rwlock_rdlock(cupolas_rwlock_t* rwlock);
int cupolas_rwlock_wrlock(cupolas_rwlock_t* rwlock);
int cupolas_rwlock_tryrdlock(cupolas_rwlock_t* rwlock);
int cupolas_rwlock_trywrlock(cupolas_rwlock_t* rwlock);
int cupolas_rwlock_unlock(cupolas_rwlock_t* rwlock);

/* 条件变量接口 */
int cupolas_cond_init(cupolas_cond_t* cond);
int cupolas_cond_destroy(cupolas_cond_t* cond);
int cupolas_cond_wait(cupolas_cond_t* cond, cupolas_mutex_t* mutex);
int cupolas_cond_timedwait(cupolas_cond_t* cond, cupolas_mutex_t* mutex, uint32_t timeout_ms);
int cupolas_cond_signal(cupolas_cond_t* cond);
int cupolas_cond_broadcast(cupolas_cond_t* cond);

/* 线程接口 */
typedef void* (*cupolas_thread_func_t)(void* arg);
int cupolas_thread_create(cupolas_thread_t* thread, cupolas_thread_func_t func, void* arg);
int cupolas_thread_join(cupolas_thread_t thread, void** retval);
int cupolas_thread_detach(cupolas_thread_t thread);
cupolas_thread_id_t cupolas_thread_self(void);
bool cupolas_thread_equal(cupolas_thread_id_t t1, cupolas_thread_id_t t2);

/* ============================================================================
 * 进程原语
 * ============================================================================ */

/* 进程句柄类型 */
#if cupolas_PLATFORM_WINDOWS
    typedef HANDLE cupolas_process_t;
    typedef DWORD cupolas_pid_t;
    typedef HANDLE cupolas_pipe_t;
#else
    typedef pid_t cupolas_pid_t;
    typedef int cupolas_process_t;
    typedef int cupolas_pipe_t[2];
#endif

/* 进程退出状态 */
typedef struct cupolas_exit_status {
    int code;
    bool signaled;
    int signal;
} cupolas_exit_status_t;

/* 进程属性 */
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

/* 进程接口 */
int cupolas_process_spawn(cupolas_process_t* proc, 
                        const char* path, 
                        char* const argv[],
                        const cupolas_process_attr_t* attr);
int cupolas_process_wait(cupolas_process_t proc, cupolas_exit_status_t* status, uint32_t timeout_ms);
int cupolas_process_terminate(cupolas_process_t proc, int signal);
int cupolas_process_close(cupolas_process_t proc);
cupolas_pid_t cupolas_process_getpid(cupolas_process_t proc);

/* 管道接口 */
int cupolas_pipe_create(cupolas_pipe_t* pipe);
int cupolas_pipe_close(cupolas_pipe_t* pipe);
int cupolas_pipe_read(cupolas_pipe_t* pipe, void* buf, size_t count, size_t* bytes_read);
int cupolas_pipe_write(cupolas_pipe_t* pipe, const void* buf, size_t count, size_t* bytes_written);

/* ============================================================================
 * 时间原语
 * ============================================================================ */

/* 时间戳结构 */
typedef struct cupolas_timestamp {
    int64_t sec;
    int32_t nsec;
} cupolas_timestamp_t;

/* 时间接口 */
int cupolas_time_now(cupolas_timestamp_t* ts);
int cupolas_time_mono(cupolas_timestamp_t* ts);
uint64_t cupolas_time_ms(void);
void cupolas_sleep_ms(uint32_t ms);
void cupolas_sleep_us(uint32_t us);

/* ============================================================================
 * 文件系统原语
 * ============================================================================ */

/* 文件路径最大长度 */
#if cupolas_PLATFORM_WINDOWS
    #define cupolas_PATH_MAX  260
    #define cupolas_PATH_SEP  '\\'
    #define cupolas_PATH_SEP_STR "\\"
#else
    #define cupolas_PATH_MAX  4096
    #define cupolas_PATH_SEP  '/'
    #define cupolas_PATH_SEP_STR "/"
#endif

/* 文件属性 */
typedef struct cupolas_file_stat {
    uint64_t size;
    cupolas_timestamp_t mtime;
    bool is_dir;
    bool is_regular;
    bool exists;
} cupolas_file_stat_t;

/* 文件系统接口 */
int cupolas_file_stat(const char* path, cupolas_file_stat_t* stat);
int cupolas_file_exists(const char* path);
int cupolas_file_mkdir(const char* path, bool recursive);
int cupolas_file_remove(const char* path);
int cupolas_file_rename(const char* old_path, const char* new_path);
char* cupolas_file_abspath(const char* path, char* buf, size_t size);
char* cupolas_file_dirname(const char* path, char* buf, size_t size);

/* ============================================================================
 * 内存原语
 * ============================================================================ */

/* 对齐内存分配 */
void* cupolas_mem_alloc(size_t size);
void* cupolas_mem_alloc_aligned(size_t size, size_t alignment);
void cupolas_mem_free(void* ptr);
void* cupolas_mem_realloc(void* ptr, size_t size);

/* 安全内存操作 */
void cupolas_mem_zero(void* ptr, size_t size);
void cupolas_mem_lock(void* ptr, size_t size);
void cupolas_mem_unlock(void* ptr, size_t size);

/* ============================================================================
 * 工具宏
 * ============================================================================ */

/* 未使用参数标记 */
#ifndef cupolas_UNUSED
#define cupolas_UNUSED(x) ((void)(x))
#endif

/* ============================================================================
 * 原子操作
 * ============================================================================ */

typedef volatile int32_t cupolas_atomic32_t;
typedef volatile int64_t cupolas_atomic64_t;
typedef volatile void* cupolas_atomic_ptr_t;

int32_t cupolas_atomic_load32(cupolas_atomic32_t* ptr);
void cupolas_atomic_store32(cupolas_atomic32_t* ptr, int32_t val);
int32_t cupolas_atomic_add32(cupolas_atomic32_t* ptr, int32_t delta);
int32_t cupolas_atomic_sub32(cupolas_atomic32_t* ptr, int32_t delta);
int32_t cupolas_atomic_inc32(cupolas_atomic32_t* ptr);
int32_t cupolas_atomic_dec32(cupolas_atomic32_t* ptr);
bool cupolas_atomic_cas32(cupolas_atomic32_t* ptr, int32_t expected, int32_t desired);

int64_t cupolas_atomic_load64(cupolas_atomic64_t* ptr);
void cupolas_atomic_store64(cupolas_atomic64_t* ptr, int64_t val);
int64_t cupolas_atomic_add64(cupolas_atomic64_t* ptr, int64_t delta);
int64_t cupolas_atomic_sub64(cupolas_atomic64_t* ptr, int64_t delta);
bool cupolas_atomic_cas64(cupolas_atomic64_t* ptr, int64_t expected, int64_t desired);

void* cupolas_atomic_load_ptr(cupolas_atomic_ptr_t* ptr);
void cupolas_atomic_store_ptr(cupolas_atomic_ptr_t* ptr, void* val);
bool cupolas_atomic_cas_ptr(cupolas_atomic_ptr_t* ptr, void* expected, void* desired);

/* ============================================================================
 * 错误处理
 * ============================================================================ */

/* 错误码 */
#define cupolas_OK                    0
#define cupolas_ERROR_UNKNOWN         -1
#define cupolas_ERROR_INVALID_ARG     -2
#define cupolas_ERROR_NO_MEMORY       -3
#define cupolas_ERROR_NOT_FOUND       -4
#define cupolas_ERROR_PERMISSION      -5
#define cupolas_ERROR_BUSY            -6
#define cupolas_ERROR_TIMEOUT         -7
#define cupolas_ERROR_WOULD_BLOCK     -8
#define cupolas_ERROR_OVERFLOW        -9
#define cupolas_ERROR_NOT_SUPPORTED   -10
#define cupolas_ERROR_IO              -11

/* 获取最后错误 */
int cupolas_get_last_error(void);
const char* cupolas_strerror(int error);

/* ============================================================================
 * 字符串工具
 * ============================================================================ */

char* cupolas_strdup(const char* str);
char* cupolas_strndup(const char* str, size_t n);
int cupolas_strcasecmp(const char* s1, const char* s2);
int cupolas_strncasecmp(const char* s1, const char* s2, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* cupolas_PLATFORM_H */

