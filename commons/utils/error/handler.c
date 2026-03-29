/**
 * @file handler.c
 * @brief 统一错误处理模块实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本模块提供统一的错误处理功能，包括�? * - 错误码描述和严重程度管理
 * - 错误链追踪和上下文管�? * - 多语言错误描述支持
 * - 错误统计和报�? */

#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../commons/utils/memory/include/memory_compat.h"
#include "../commons/utils/string/include/string_compat.h"
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

/* ==================== 平台相关定义 ==================== */

#ifdef _WIN32
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif

/* ==================== 全局状�?==================== */

/* 当前语言环境 */
static agentos_language_t g_current_language = AGENTOS_LANG_EN_US;

/* 自定义多语言错误描述条目 */
static agentos_error_i18n_entry_t* g_i18n_entries = NULL;
static size_t g_i18n_entry_count = 0;

/* 错误统计信息 */
static struct {
    uint64_t total_errors;
    uint64_t errors_by_severity[4]; /* 按严重程度统�?*/
    uint64_t last_error_time;
    agentos_error_t last_error;
} g_error_stats;

#ifdef _WIN32
static CRITICAL_SECTION g_error_stats_mutex;
static volatile LONG g_error_stats_initialized = 0;

static void ensure_stats_init(void) {
    if (InterlockedCompareExchange(&g_error_stats_initialized, 1, 0) == 0) {
        InitializeCriticalSection(&g_error_stats_mutex);
    }
}

#define STATS_LOCK() \
    do { \
        ensure_stats_init(); \
        EnterCriticalSection(&g_error_stats_mutex); \
    } while (0)

#define STATS_UNLOCK() \
    do { \
        LeaveCriticalSection(&g_error_stats_mutex); \
    } while (0)

#else
static pthread_mutex_t g_error_stats_mutex = PTHREAD_MUTEX_INITIALIZER;

#define STATS_LOCK() \
    do { \
        pthread_mutex_lock(&g_error_stats_mutex); \
    } while (0)

#define STATS_UNLOCK() \
    do { \
        pthread_mutex_unlock(&g_error_stats_mutex); \
    } while (0)
#endif

/* ==================== 线程本地错误�?==================== */

typedef struct {
    agentos_error_chain_t chain;
    int initialized;
} thread_error_state_t;

#ifdef _WIN32
static DWORD g_tls_index = TLS_OUT_OF_INDEXES;

static thread_error_state_t* get_thread_error_state(void) {
    if (g_tls_index == TLS_OUT_OF_INDEXES) {
        g_tls_index = TlsAlloc();
        if (g_tls_index == TLS_OUT_OF_INDEXES) {
            return NULL;
        }
    }
    
    thread_error_state_t* state = (thread_error_state_t*)TlsGetValue(g_tls_index);
    if (state == NULL) {
        state = (thread_error_state_t*)AGENTOS_CALLOC(1, sizeof(thread_error_state_t));
        if (state != NULL) {
            state->initialized = 1;
            TlsSetValue(g_tls_index, state);
        }
    }
    return state;
}
#else
static pthread_key_t g_tls_key;
static pthread_once_t g_tls_once = PTHREAD_ONCE_INIT;

static void create_tls_key(void) {
    pthread_key_create(&g_tls_key, free);
}

static thread_error_state_t* get_thread_error_state(void) {
    pthread_once(&g_tls_once, create_tls_key);
    
    thread_error_state_t* state = (thread_error_state_t*)pthread_getspecific(g_tls_key);
    if (state == NULL) {
        state = (thread_error_state_t*)AGENTOS_CALLOC(1, sizeof(thread_error_state_t));
        if (state != NULL) {
            state->initialized = 1;
            pthread_setspecific(g_tls_key, state);
        }
    }
    return state;
}
#endif

/* ==================== 错误码描述表 ==================== */

typedef struct {
    agentos_error_t code;
    const char* name;
    const char* description_en;
    const char* description_zh_cn;
    agentos_error_severity_t severity;
} error_info_t;

static const error_info_t g_error_info[] = {
    /* 成功 */
    {AGENTOS_OK, "OK", "Success", "成功", AGENTOS_ERR_SEVERITY_INFO},
    
    /* 通用基础错误 */
    {AGENTOS_ERR_UNKNOWN, "ERR_UNKNOWN", "Unknown error", "未知错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_INVALID_PARAM, "ERR_INVALID_PARAM", "Invalid parameter", "无效参数", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_NULL_POINTER, "ERR_NULL_POINTER", "Null pointer", "空指�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_OUT_OF_MEMORY, "ERR_OUT_OF_MEMORY", "Out of memory", "内存不足", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_BUFFER_TOO_SMALL, "ERR_BUFFER_TOO_SMALL", "Buffer too small", "缓冲区太�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_NOT_FOUND, "ERR_NOT_FOUND", "Not found", "未找�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_ALREADY_EXISTS, "ERR_ALREADY_EXISTS", "Already exists", "已存�?, AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_TIMEOUT, "ERR_TIMEOUT", "Timeout", "超时", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_NOT_SUPPORTED, "ERR_NOT_SUPPORTED", "Not supported", "不支�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_PERMISSION_DENIED, "ERR_PERMISSION_DENIED", "Permission denied", "权限不足", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_IO, "ERR_IO", "I/O error", "I/O错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_PARSE_ERROR, "ERR_PARSE_ERROR", "Parse error", "解析错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_STATE_ERROR, "ERR_STATE_ERROR", "State error", "状态错�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_OVERFLOW, "ERR_OVERFLOW", "Overflow", "溢出", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_UNDERFLOW, "ERR_UNDERFLOW", "Underflow", "下溢", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_CANCELED, "ERR_CANCELED", "Canceled", "已取�?, AGENTOS_ERR_SEVERITY_INFO},
    {AGENTOS_ERR_BUSY, "ERR_BUSY", "Busy", "忙碌", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_WOULD_BLOCK, "ERR_WOULD_BLOCK", "Would block", "将阻�?, AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_INTERRUPTED, "ERR_INTERRUPTED", "Interrupted", "被中�?, AGENTOS_ERR_SEVERITY_WARNING},
    
    /* 系统与平台错�?*/
    {AGENTOS_ERR_SYS_NOT_INIT, "ERR_SYS_NOT_INIT", "System not initialized", "系统未初始化", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_RESOURCE, "ERR_SYS_RESOURCE", "System resource error", "系统资源错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_DEADLOCK, "ERR_SYS_DEADLOCK", "Deadlock", "死锁", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_SYS_THREAD, "ERR_SYS_THREAD", "Thread error", "线程错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_MUTEX, "ERR_SYS_MUTEX", "Mutex error", "互斥锁错�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_SEMAPHORE, "ERR_SYS_SEMAPHORE", "Semaphore error", "信号量错�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_CONDITION, "ERR_SYS_CONDITION", "Condition variable error", "条件变量错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_ATOMIC, "ERR_SYS_ATOMIC", "Atomic operation error", "原子操作错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_SOCKET, "ERR_SYS_SOCKET", "Socket error", "套接字错�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_PIPE, "ERR_SYS_PIPE", "Pipe error", "管道错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_PROCESS, "ERR_SYS_PROCESS", "Process error", "进程错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_FILE, "ERR_SYS_FILE", "File error", "文件错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_TIME, "ERR_SYS_TIME", "Time error", "时间错误", AGENTOS_ERR_SEVERITY_ERROR},
    
    /* 内核层错�?*/
    {AGENTOS_ERR_KERN_IPC, "ERR_KERN_IPC", "IPC error", "IPC错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_TASK, "ERR_KERN_TASK", "Task error", "任务错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_SYNC, "ERR_KERN_SYNC", "Synchronization error", "同步错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_LOCK, "ERR_KERN_LOCK", "Lock error", "锁错�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_MEM, "ERR_KERN_MEM", "Memory error", "内存错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_SCHED, "ERR_KERN_SCHED", "Scheduler error", "调度器错�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_TIMER, "ERR_KERN_TIMER", "Timer error", "定时器错�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_INTERRUPT, "ERR_KERN_INTERRUPT", "Interrupt error", "中断错误", AGENTOS_ERR_SEVERITY_ERROR},
    
    /* 服务层错�?*/
    {AGENTOS_ERR_SVC_NOT_READY, "ERR_SVC_NOT_READY", "Service not ready", "服务未就�?, AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_SVC_BUSY, "ERR_SVC_BUSY", "Service busy", "服务忙碌", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_SVC_STOPPED, "ERR_SVC_STOPPED", "Service stopped", "服务已停�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SVC_CONFIG, "ERR_SVC_CONFIG", "Service configuration error", "服务配置错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SVC_DEPENDENCY, "ERR_SVC_DEPENDENCY", "Service dependency error", "服务依赖错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SVC_HEALTH, "ERR_SVC_HEALTH", "Service health error", "服务健康错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SVC_LOADBALANCE, "ERR_SVC_LOADBALANCE", "Load balance error", "负载均衡错误", AGENTOS_ERR_SEVERITY_ERROR},
    
    /* LLM/AI服务错误 */
    {AGENTOS_ERR_LLM_NO_PROVIDER, "ERR_LLM_NO_PROVIDER", "No LLM provider", "无LLM提供�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_PROVIDER_FAIL, "ERR_LLM_PROVIDER_FAIL", "LLM provider failure", "LLM提供商失�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_RATE_LIMIT, "ERR_LLM_RATE_LIMIT", "Rate limit exceeded", "超出速率限制", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_LLM_CONTEXT_LEN, "ERR_LLM_CONTEXT_LEN", "Context length exceeded", "超出上下文长�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_INVALID_MODEL, "ERR_LLM_INVALID_MODEL", "Invalid model", "无效模型", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_AUTH_FAIL, "ERR_LLM_AUTH_FAIL", "Authentication failed", "认证失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_TOKEN_LIMIT, "ERR_LLM_TOKEN_LIMIT", "Token limit exceeded", "超出Token限制", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_PARSE_RESP, "ERR_LLM_PARSE_RESP", "Failed to parse response", "解析响应失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_EMPTY_RESP, "ERR_LLM_EMPTY_RESP", "Empty response", "空响�?, AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_COST_EXCEED, "ERR_LLM_COST_EXCEED", "Cost exceeded", "超出成本限制", AGENTOS_ERR_SEVERITY_WARNING},
    
    /* 执行/工具错误 */
    {AGENTOS_ERR_EXEC_NOT_FOUND, "ERR_EXEC_NOT_FOUND", "Executor not found", "执行器未找到", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_FAIL, "ERR_EXEC_FAIL", "Execution failed", "执行失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_TIMEOUT, "ERR_EXEC_TIMEOUT", "Execution timeout", "执行超时", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_EXEC_VALIDATION, "ERR_EXEC_VALIDATION", "Execution validation failed", "执行验证失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_SANDBOX, "ERR_EXEC_SANDBOX", "Sandbox error", "沙箱错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_PERMISSION, "ERR_EXEC_PERMISSION", "Execution permission denied", "执行权限不足", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_ARGS, "ERR_EXEC_ARGS", "Invalid execution arguments", "无效执行参数", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_ENV, "ERR_EXEC_ENV", "Execution environment error", "执行环境错误", AGENTOS_ERR_SEVERITY_ERROR},
    
    /* 记忆/存储错误 */
    {AGENTOS_ERR_MEM_WRITE, "ERR_MEM_WRITE", "Memory write error", "内存写入错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_MEM_READ, "ERR_MEM_READ", "Memory read error", "内存读取错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_MEM_QUERY, "ERR_MEM_QUERY", "Memory query error", "内存查询错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_MEM_EVOLVE, "ERR_MEM_EVOLVE", "Memory evolution error", "内存演进错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_MEM_FULL, "ERR_MEM_FULL", "Memory full", "内存已满", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_MEM_CORRUPT, "ERR_MEM_CORRUPT", "Memory corruption", "内存损坏", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_MEM_NOT_INIT, "ERR_MEM_NOT_INIT", "Memory not initialized", "内存未初始化", AGENTOS_ERR_SEVERITY_ERROR},
    
    /* 安全/沙箱错误 */
    {AGENTOS_ERR_SEC_VIOLATION, "ERR_SEC_VIOLATION", "Security violation", "安全违规", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_SEC_SANITIZE, "ERR_SEC_SANITIZE", "Sanitization error", "清理错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SEC_AUDIT, "ERR_SEC_AUDIT", "Audit error", "审计错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SEC_PERMISSION, "ERR_SEC_PERMISSION", "Security permission error", "安全权限错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SEC_VALIDATION, "ERR_SEC_VALIDATION", "Security validation error", "安全验证错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SEC_QUOTA, "ERR_SEC_QUOTA", "Quota exceeded", "超出配额", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_SEC_TEMP_DIR, "ERR_SEC_TEMP_DIR", "Temporary directory error", "临时目录错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SEC_SYMLINK, "ERR_SEC_SYMLINK", "Symbolic link error", "符号链接错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SEC_PATH_TRAV, "ERR_SEC_PATH_TRAV", "Path traversal detected", "检测到路径遍历", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_ESECURITY, "ERR_ESECURITY", "Security error", "安全错误", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_ESANITIZE, "ERR_ESANITIZE", "Sanitization error", "清理错误", AGENTOS_ERR_SEVERITY_ERROR},
    
    /* 协调/规划错误 */
    {AGENTOS_ERR_COORD_PLAN_FAIL, "ERR_COORD_PLAN_FAIL", "Planning failed", "规划失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_COORD_SYNC_FAIL, "ERR_COORD_SYNC_FAIL", "Synchronization failed", "同步失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_COORD_DISPATCH, "ERR_COORD_DISPATCH", "Dispatch error", "调度错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_COORD_INTENT, "ERR_COORD_INTENT", "Intent error", "意图错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_COORD_COMPENSATE, "ERR_COORD_COMPENSATE", "Compensation error", "补偿错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_COORD_RETRY_EXCEED, "ERR_COORD_RETRY_EXCEED", "Retry limit exceeded", "超出重试限制", AGENTOS_ERR_SEVERITY_ERROR},
};

static const size_t g_error_info_count = sizeof(g_error_info) / sizeof(g_error_info[0]);

/* ==================== 核心错误处理函数 ==================== */

const char* agentos_error_str(agentos_error_t code) {
    for (size_t i = 0; i < g_error_info_count; i++) {
        if (g_error_info[i].code == code) {
            return g_error_info[i].description_en;
        }
    }
    return "Unknown error";
}

agentos_error_severity_t agentos_error_get_severity(agentos_error_t code) {
    for (size_t i = 0; i < g_error_info_count; i++) {
        if (g_error_info[i].code == code) {
            return g_error_info[i].severity;
        }
    }
    return AGENTOS_ERR_SEVERITY_ERROR;
}

agentos_error_chain_t* agentos_error_get_chain(void) {
    thread_error_state_t* state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        return NULL;
    }
    return &state->chain;
}

void agentos_error_clear(void) {
    thread_error_state_t* state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        return;
    }
    state->chain.code = AGENTOS_OK;
    state->chain.depth = 0;
}

/* ==================== 时间获取函数 ==================== */

static uint64_t get_current_time_ns(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return uli.QuadPart * 100;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* ==================== 错误上下文添加函�?==================== */

void agentos_error_push_ex(agentos_error_t code,
                           const char* file,
                           int line,
                           const char* func,
                           const char* fmt, ...) {
    thread_error_state_t* state = get_thread_error_state();
    if (state == NULL || !state->initialized) {
        return;
    }
    
    agentos_error_chain_t* chain = &state->chain;
    
    /* 更新错误统计 */
    STATS_LOCK();
    g_error_stats.total_errors++;
    agentos_error_severity_t severity = agentos_error_get_severity(code);
    if (severity >= 0 && severity < 4) {
        g_error_stats.errors_by_severity[severity]++;
    }
    g_error_stats.last_error_time = get_current_time_ns();
    g_error_stats.last_error = code;
    STATS_UNLOCK();
    
    /* 格式化错误消�?*/
    char message_buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message_buffer, sizeof(message_buffer), fmt, args);
    va_end(args);
    
    /* 添加到错误链 */
    if (chain->depth < AGENTOS_ERROR_CONTEXT_MAX_DEPTH) {
        agentos_error_context_entry_t* entry = &chain->contexts[chain->depth];
        entry->file = file;
        entry->line = line;
        entry->function = func;
        entry->message = AGENTOS_STRDUP(message_buffer); /* 需要释放，但错误链生命周期内保�?*/
        entry->error_code = code;
        entry->timestamp_ns = get_current_time_ns();
        chain->depth++;
    } else {
        /* 链已满，移除最旧的条目 */
        AGENTOS_FREE((void*)chain->contexts[0].message);
        for (int i = 0; i < AGENTOS_ERROR_CONTEXT_MAX_DEPTH - 1; i++) {
            chain->contexts[i] = chain->contexts[i + 1];
        }
        agentos_error_context_entry_t* entry = &chain->contexts[AGENTOS_ERROR_CONTEXT_MAX_DEPTH - 1];
        entry->file = file;
        entry->line = line;
        entry->function = func;
        entry->message = AGENTOS_STRDUP(message_buffer);
        entry->error_code = code;
        entry->timestamp_ns = get_current_time_ns();
    }
    
    /* 更新链的最新错误码 */
    chain->code = code;
}

void agentos_error_print_chain(const agentos_error_chain_t* chain) {
    if (chain == NULL) {
        printf("Error chain is NULL\n");
        return;
    }
    
    printf("Error chain (depth: %d, latest error: %d)\n", chain->depth, chain->code);
    for (int i = 0; i < chain->depth; i++) {
        const agentos_error_context_entry_t* ctx = &chain->contexts[i];
        printf("  [%d] %s:%d in %s() - %d: %s\n",
               i + 1,
               ctx->file ? ctx->file : "(unknown)",
               ctx->line,
               ctx->function ? ctx->function : "(unknown)",
               ctx->error_code,
               ctx->message ? ctx->message : "");
    }
}

char* agentos_error_chain_to_json(const agentos_error_chain_t* chain) {
    if (chain == NULL) {
        return AGENTOS_STRDUP("{\"error\": \"null chain\"}");
    }
    
    /* 调用多语言版本，使用当前语言 */
    return agentos_error_chain_to_json_i18n(chain, -1);
}

void agentos_error_get_stats(agentos_error_stats_t* stats) {
    if (stats == NULL) {
        return;
    }
    
    STATS_LOCK();
    stats->total_errors = g_error_stats.total_errors;
    for (int i = 0; i < 4; i++) {
        stats->errors_by_code[i] = g_error_stats.errors_by_severity[i];
    }
    stats->last_error_time = g_error_stats.last_error_time;
    stats->last_error = g_error_stats.last_error;
    STATS_UNLOCK();
}

void agentos_error_reset_stats(void) {
    STATS_LOCK();
    g_error_stats.total_errors = 0;
    for (int i = 0; i < 4; i++) {
        g_error_stats.errors_by_severity[i] = 0;
    }
    g_error_stats.last_error_time = 0;
    g_error_stats.last_error = AGENTOS_OK;
    STATS_UNLOCK();
}

/* ==================== 多语言支持函数实现 ==================== */

agentos_error_t agentos_error_set_language(agentos_language_t lang) {
    if ((int)lang < 0 || (int)lang > 7) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    g_current_language = lang;
    return AGENTOS_OK;
}

agentos_language_t agentos_error_get_language(void) {
    return g_current_language;
}

const char* agentos_error_str_i18n(agentos_error_t code, agentos_language_t lang) {
    agentos_language_t use_lang = lang;
    if ((int)lang < 0) {
        use_lang = g_current_language;
    }
    
    if ((int)use_lang < 0 || (int)use_lang > 7) {
        use_lang = AGENTOS_LANG_EN_US;
    }
    
    /* 首先检查自定义i18n条目 */
    for (size_t i = 0; i < g_i18n_entry_count; i++) {
        if (g_i18n_entries[i].error_code == code) {
            const char* desc = g_i18n_entries[i].descriptions[use_lang];
            if (desc != NULL) {
                return desc;
            }
        }
    }
    
    /* 回退到内置多语言描述 */
    for (size_t i = 0; i < g_error_info_count; i++) {
        if (g_error_info[i].code == code) {
            switch (use_lang) {
                case AGENTOS_LANG_ZH_CN:
                    return g_error_info[i].description_zh_cn ? g_error_info[i].description_zh_cn : g_error_info[i].description_en;
                default:
                    return g_error_info[i].description_en;
            }
        }
    }
    
    /* 最后回退到默认错误描�?*/
    return agentos_error_str(code);
}

agentos_error_t agentos_error_register_i18n(
    const agentos_error_i18n_entry_t* entries,
    size_t count) {
    
    if (entries == NULL || count == 0) {
        return AGENTOS_ERR_INVALID_PARAM;
    }
    
    /* 释放现有条目 */
    if (g_i18n_entries != NULL) {
        for (size_t i = 0; i < g_i18n_entry_count; i++) {
            /* 注意：这里假设descriptions是静态字符串，不需要释�?*/
        }
        AGENTOS_FREE(g_i18n_entries);
        g_i18n_entries = NULL;
        g_i18n_entry_count = 0;
    }
    
    /* 分配新内�?*/
    g_i18n_entries = (agentos_error_i18n_entry_t*)AGENTOS_MALLOC(count * sizeof(agentos_error_i18n_entry_t));
    if (g_i18n_entries == NULL) {
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    
    /* 复制条目 */
    memcpy(g_i18n_entries, entries, count * sizeof(agentos_error_i18n_entry_t));
    g_i18n_entry_count = count;
    
    return AGENTOS_OK;
}

char* agentos_error_chain_to_json_i18n(
    const agentos_error_chain_t* chain,
    agentos_language_t lang) {
    
    if (!chain) {
        return AGENTOS_STRDUP("{\"error\": \"null\"}");
    }
    
    agentos_language_t use_lang = lang;
    if ((int)lang < 0) {
        use_lang = g_current_language;
    }
    
    size_t buf_size = 4096;
    char* buf = (char*)AGENTOS_MALLOC(buf_size);
    if (!buf) {
        return NULL;
    }
    
    size_t offset = snprintf(buf, buf_size, 
        "{\"code\": %d, \"message\": \"%s\", \"depth\": %d, \"contexts\": [",
        chain->code, 
        agentos_error_str_i18n(chain->code, use_lang),
        chain->depth);
    
    for (int i = 0; i < chain->depth; i++) {
        const agentos_error_context_entry_t* ctx = &chain->contexts[i];
        
        /* 转义消息字符�?*/
        char escaped_msg[2048] = {0};
        const char* msg = ctx->message ? ctx->message : "";
        for (size_t j = 0, k = 0; j < strlen(msg) && k < sizeof(escaped_msg) - 1; j++) {
            if (msg[j] == '"' || msg[j] == '\\') {
                escaped_msg[k++] = '\\';
            }
            escaped_msg[k++] = msg[j];
        }
        
        offset += snprintf(buf + offset, buf_size - offset,
            "%s{\"file\": \"%s\", \"line\": %d, \"function\": \"%s\", \"code\": %d, \"message\": \"%s\"}",
            i > 0 ? ", " : "",
            ctx->file ? ctx->file : "",
            ctx->line,
            ctx->function ? ctx->function : "",
            ctx->error_code,
            escaped_msg);
    }
    
    offset += snprintf(buf + offset, buf_size - offset, "]}");
    
    return buf;
}

/* ==================== 错误链增强功能实�?==================== */

void agentos_error_chain_iter_init(
    const agentos_error_chain_t* chain,
    agentos_error_chain_iterator_t* iter) {
    
    if (!iter) return;
    
    iter->chain = chain;
    iter->current_index = 0;
}

const agentos_error_context_entry_t* agentos_error_chain_iter_next(
    agentos_error_chain_iterator_t* iter) {
    
    if (!iter || !iter->chain) {
        return NULL;
    }
    
    if (iter->current_index >= iter->chain->depth) {
        return NULL;
    }
    
    const agentos_error_context_entry_t* ctx = &iter->chain->contexts[iter->current_index];
    iter->current_index++;
    
    return ctx;
}

void agentos_error_chain_iter_reset(agentos_error_chain_iterator_t* iter) {
    if (!iter) return;
    
    iter->current_index =