/**
 * @file handler.c
 * @brief 错误处理实现（跨平台）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include "../observability/include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
    #include <time.h>
#endif

/* ==================== 线程本地存储 ==================== */

#ifdef _WIN32
    #define ERROR_TLS DWORD
    #define ERROR_TLS_INDEX TlsAlloc()
    #define error_tls_get() TlsGetValue(ERROR_TLS_INDEX)
    #define error_tls_set(v) TlsSetValue(ERROR_TLS_INDEX, (v))
#else
    static pthread_once_t g_error_tls_once = PTHREAD_ONCE_INIT;
    static pthread_key_t g_error_tls_key;

    static void error_tls_once_init(void) {
        pthread_key_create(&g_error_tls_key, free);
    }

    static agentos_error_chain_t* error_tls_get(void) {
        pthread_once(&g_error_tls_once, error_tls_once_init);
        return pthread_getspecific(g_error_tls_key);
    }

    static void error_tls_set(agentos_error_chain_t* chain) {
        pthread_once(&g_error_tls_once, error_tls_once_init);
        pthread_setspecific(g_error_tls_key, chain);
    }
#endif

/* ==================== 互斥锁 ==================== */

#ifdef _WIN32
    static CRITICAL_SECTION g_error_mutex;
    static volatile LONG g_error_initialized = 0;

    static void ensure_error_init(void) {
        if (InterlockedCompareExchange(&g_error_initialized, 1, 0) == 0) {
            InitializeCriticalSection(&g_error_mutex);
        }
    }
    #define ERROR_LOCK() \
        do { ensure_error_init(); EnterCriticalSection(&g_error_mutex); } while (0)
    #define ERROR_UNLOCK() \
        do { LeaveCriticalSection(&g_error_mutex); } while (0)
#else
    static pthread_mutex_t g_error_mutex = PTHREAD_MUTEX_INITIALIZER;
    #define ensure_error_init() ((void)0)
    #define ERROR_LOCK() \
        do { pthread_mutex_lock(&g_error_mutex); } while (0)
    #define ERROR_UNLOCK() \
        do { pthread_mutex_unlock(&g_error_mutex); } while (0)
#endif

/* ==================== 时间戳 ==================== */

static uint64_t get_monotonic_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/* ==================== 错误码字符串映射 ==================== */

typedef struct {
    int code;
    const char* name;
    const char* description;
    agentos_error_severity_t severity;
} error_info_entry_t;

static const error_info_entry_t g_error_entries[] = {
    {AGENTOS_OK, "OK", "成功", AGENTOS_ERR_SEVERITY_INFO},
    {AGENTOS_ERR_UNKNOWN, "UNKNOWN", "未知错误", AGENTOS_ERR_SEVERITY_ERROR},

    {AGENTOS_ERR_INVALID_PARAM, "INVALID_PARAM", "无效参数", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_NULL_POINTER, "NULL_POINTER", "空指针", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_OUT_OF_MEMORY, "OUT_OF_MEMORY", "内存不足", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_BUFFER_TOO_SMALL, "BUFFER_TOO_SMALL", "缓冲区太小", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_NOT_FOUND, "NOT_FOUND", "未找到", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_ALREADY_EXISTS, "ALREADY_EXISTS", "已存在", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_TIMEOUT, "TIMEOUT", "超时", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_NOT_SUPPORTED, "NOT_SUPPORTED", "不支持", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_PERMISSION_DENIED, "PERMISSION_DENIED", "权限不足", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_IO, "IO", "I/O错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_PARSE_ERROR, "PARSE_ERROR", "解析错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_STATE_ERROR, "STATE_ERROR", "状态错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_CANCELED, "CANCELED", "已取消", AGENTOS_ERR_SEVERITY_INFO},
    {AGENTOS_ERR_BUSY, "BUSY", "资源忙碌", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_WOULD_BLOCK, "WOULD_BLOCK", "会阻塞", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_INTERRUPTED, "INTERRUPTED", "被中断", AGENTOS_ERR_SEVERITY_WARNING},

    {AGENTOS_ERR_SYS_BASE, "SYS_BASE", "系统错误基址", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_NOT_INIT, "SYS_NOT_INIT", "未初始化", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_RESOURCE, "SYS_RESOURCE", "系统资源不足", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_SOCKET, "SYS_SOCKET", "Socket错误", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SYS_FILE, "SYS_FILE", "文件错误", AGENTOS_ERR_SEVERITY_ERROR},

    {AGENTOS_ERR_KERN_BASE, "KERN_BASE", "内核错误基址", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_IPC, "KERN_IPC", "IPC通信失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_TASK, "KERN_TASK", "任务失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_KERN_MEM, "KERN_MEM", "内核内存错误", AGENTOS_ERR_SEVERITY_ERROR},

    {AGENTOS_ERR_SVC_BASE, "SVC_BASE", "服务错误基址", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SVC_NOT_READY, "SVC_NOT_READY", "服务未就绪", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SVC_BUSY, "SVC_BUSY", "服务忙碌", AGENTOS_ERR_SEVERITY_WARNING},

    {AGENTOS_ERR_LLM_BASE, "LLM_BASE", "LLM错误基址", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_NO_PROVIDER, "LLM_NO_PROVIDER", "无LLM提供商", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_RATE_LIMIT, "LLM_RATE_LIMIT", "LLM限流", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_LLM_CONTEXT_LEN, "LLM_CONTEXT_LEN", "上下文超长", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_LLM_COST_EXCEED, "LLM_COST_EXCEED", "成本超限", AGENTOS_ERR_SEVERITY_ERROR},

    {AGENTOS_ERR_EXEC_BASE, "EXEC_BASE", "执行错误基址", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_NOT_FOUND, "EXEC_NOT_FOUND", "执行单元未找到", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_FAIL, "EXEC_FAIL", "执行失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_EXEC_TIMEOUT, "EXEC_TIMEOUT", "执行超时", AGENTOS_ERR_SEVERITY_WARNING},
    {AGENTOS_ERR_EXEC_SANDBOX, "EXEC_SANDBOX", "沙箱执行失败", AGENTOS_ERR_SEVERITY_ERROR},

    {AGENTOS_ERR_MEM_BASE, "MEM_BASE", "记忆错误基址", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_MEM_WRITE, "MEM_WRITE", "记忆写入失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_MEM_READ, "MEM_READ", "记忆读取失败", AGENTOS_ERR_SEVERITY_ERROR},

    {AGENTOS_ERR_SEC_BASE, "SEC_BASE", "安全错误基址", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_SEC_VIOLATION, "SEC_VIOLATION", "安全违规", AGENTOS_ERR_SEVERITY_CRITICAL},
    {AGENTOS_ERR_SEC_SANITIZE, "SEC_SANITIZE", "输入净化失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_SEC_PATH_TRAV, "SEC_PATH_TRAV", "路径遍历攻击", AGENTOS_ERR_SEVERITY_CRITICAL},

    {AGENTOS_ERR_COORD_BASE, "COORD_BASE", "协调错误基址", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_COORD_PLAN_FAIL, "COORD_PLAN_FAIL", "规划失败", AGENTOS_ERR_SEVERITY_ERROR},
    {AGENTOS_ERR_COORD_SYNC_FAIL, "COORD_SYNC_FAIL", "同步失败", AGENTOS_ERR_SEVERITY_ERROR},
};

#define ERROR_ENTRIES_COUNT (sizeof(g_error_entries) / sizeof(g_error_entries[0]))

/* ==================== 错误统计 ==================== */

static struct {
    uint64_t total_errors;
    uint64_t errors_by_code[32];
    uint64_t last_error_time;
    agentos_error_t last_error;
} g_error_stats = {0};

/* ==================== 核心接口实现 ==================== */

const char* agentos_error_str(agentos_error_t code) {
    for (size_t i = 0; i < ERROR_ENTRIES_COUNT; i++) {
        if (g_error_entries[i].code == code) {
            return g_error_entries[i].description;
        }
    }
    return "未知错误码";
}

agentos_error_severity_t agentos_error_get_severity(agentos_error_t code) {
    for (size_t i = 0; i < ERROR_ENTRIES_COUNT; i++) {
        if (g_error_entries[i].code == code) {
            return g_error_entries[i].severity;
        }
    }
    if (code < 0) {
        return AGENTOS_ERR_SEVERITY_ERROR;
    }
    return AGENTOS_ERR_SEVERITY_INFO;
}

/* ==================== 错误链接口 ==================== */

agentos_error_chain_t* agentos_error_get_chain(void) {
    agentos_error_chain_t* chain = (agentos_error_chain_t*)error_tls_get();
    if (!chain) {
        chain = (agentos_error_chain_t*)calloc(1, sizeof(agentos_error_chain_t));
        if (chain) {
            error_tls_set(chain);
        }
    }
    return chain;
}

void agentos_error_clear(void) {
    agentos_error_chain_t* chain = agentos_error_get_chain();
    if (chain) {
        chain->code = AGENTOS_OK;
        chain->depth = 0;
        memset(chain->contexts, 0, sizeof(chain->contexts));
    }
}

/* ==================== 向后兼容接口实现 ==================== */

static agentos_error_handler_t g_legacy_handler = NULL;

void agentos_error_set_handler(agentos_error_handler_t handler) {
    g_legacy_handler = handler;
}

static void invoke_legacy_handler(agentos_error_t code,
                                  const char* file,
                                  int line,
                                  const char* func,
                                  const char* message) {
    if (g_legacy_handler) {
        agentos_error_context_t ctx;
        ctx.error_code = code;
        ctx.file = file;
        ctx.line = line;
        ctx.function = func;
        ctx.message = message;
        g_legacy_handler(code, &ctx);
    }
}

void agentos_error_push_ex(agentos_error_t code,
                           const char* file,
                           int line,
                           const char* func,
                           const char* fmt, ...) {
    if (code == AGENTOS_OK) {
        return;
    }

    agentos_error_chain_t* chain = agentos_error_get_chain();
    if (!chain) {
        return;
    }

    char message_buf[512] = {0};
    if (fmt && fmt[0]) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(message_buf, sizeof(message_buf), fmt, args);
        va_end(args);
    }

    invoke_legacy_handler(code, file, line, func, message_buf);

    ERROR_LOCK();

    chain->code = code;
    if (chain->depth < AGENTOS_ERROR_CONTEXT_MAX_DEPTH) {
        agentos_error_context_entry_t* ctx = &chain->contexts[chain->depth];
        ctx->file = file;
        ctx->line = line;
        ctx->function = func;
        ctx->error_code = code;
        ctx->timestamp_ns = get_monotonic_ns();
        snprintf(ctx->message, sizeof(ctx->message), "%s", message_buf);
        chain->depth++;
    }

    g_error_stats.total_errors++;
    g_error_stats.last_error_time = get_monotonic_ns();
    g_error_stats.last_error = code;

    ERROR_UNLOCK();
}

void agentos_error_print_chain(const agentos_error_chain_t* chain) {
    if (!chain) {
        return;
    }

    printf("Error chain (code=%d):\n", chain->code);
    for (int i = 0; i < chain->depth; i++) {
        const agentos_error_context_entry_t* ctx = &chain->contexts[i];
        printf("  [%d] %s:%d (%s): %s\n",
               ctx->error_code,
               ctx->file ? ctx->file : "?",
               ctx->line,
               ctx->function ? ctx->function : "?",
               ctx->message);
    }
}

char* agentos_error_chain_to_json(const agentos_error_chain_t* chain) {
    if (!chain) {
        return strdup("{\"error\": \"null\"}");
    }

    size_t buf_size = 2048;
    char* buf = (char*)malloc(buf_size);
    if (!buf) {
        return NULL;
    }

    size_t offset = snprintf(buf, buf_size, "{\"code\": %d, \"depth\": %d, \"contexts\": [",
                            chain->code, chain->depth);

    for (int i = 0; i < chain->depth; i++) {
        const agentos_error_context_entry_t* ctx = &chain->contexts[i];
        const char* msg_escaped = ctx->message;
        char escaped_msg[1024] = {0};
        for (size_t j = 0, k = 0; j < strlen(msg_escaped) && k < sizeof(escaped_msg) - 1; j++) {
            if (msg_escaped[j] == '"') {
                escaped_msg[k++] = '\\';
            }
            escaped_msg[k++] = msg_escaped[j];
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

/* ==================== 错误统计接口 ==================== */

void agentos_error_get_stats(agentos_error_stats_t* stats) {
    if (!stats) return;

    ERROR_LOCK();
    stats->total_errors = g_error_stats.total_errors;
    stats->last_error_time = g_error_stats.last_error_time;
    stats->last_error = g_error_stats.last_error;
    for (int i = 0; i < 32 && i < (int)(sizeof(g_error_stats.errors_by_code) / sizeof(g_error_stats.errors_by_code[0])); i++) {
        stats->errors_by_code[i] = g_error_stats.errors_by_code[i];
    }
    ERROR_UNLOCK();
}

void agentos_error_reset_stats(void) {
    ERROR_LOCK();
    g_error_stats.total_errors = 0;
    g_error_stats.last_error_time = 0;
    g_error_stats.last_error = AGENTOS_OK;
    memset(g_error_stats.errors_by_code, 0, sizeof(g_error_stats.errors_by_code));
    ERROR_UNLOCK();
}
