/**
 * @file error.c
 * @brief 统一错误处理框架实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "error.h"
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ==================== 错误码描述表 ==================== */

static const struct {
    agentos_error_t code;
    const char* name;
    const char* description;
} g_error_descriptions[] = {
    /* 通用错误 */
    { AGENTOS_OK,                  "OK",                  "成功" },
    { AGENTOS_ERR_UNKNOWN,         "UNKNOWN",             "未知错误" },
    { AGENTOS_ERR_INVALID_PARAM,   "INVALID_PARAM",       "无效参数" },
    { AGENTOS_ERR_NULL_POINTER,    "NULL_POINTER",        "空指针" },
    { AGENTOS_ERR_OUT_OF_MEMORY,   "OUT_OF_MEMORY",       "内存不足" },
    { AGENTOS_ERR_BUFFER_TOO_SMALL,"BUFFER_TOO_SMALL",    "缓冲区太小" },
    { AGENTOS_ERR_NOT_FOUND,       "NOT_FOUND",           "未找到" },
    { AGENTOS_ERR_ALREADY_EXISTS,  "ALREADY_EXISTS",      "已存在" },
    { AGENTOS_ERR_TIMEOUT,         "TIMEOUT",             "超时" },
    { AGENTOS_ERR_NOT_SUPPORTED,   "NOT_SUPPORTED",       "不支持" },
    { AGENTOS_ERR_PERMISSION_DENIED,"PERMISSION_DENIED",  "权限拒绝" },
    { AGENTOS_ERR_IO,              "IO",                  "IO错误" },
    { AGENTOS_ERR_PARSE_ERROR,     "PARSE_ERROR",         "解析错误" },
    { AGENTOS_ERR_STATE_ERROR,     "STATE_ERROR",         "状态错误" },
    { AGENTOS_ERR_OVERFLOW,        "OVERFLOW",            "溢出" },
    { AGENTOS_ERR_UNDERFLOW,       "UNDERFLOW",           "下溢" },
    
    /* 服务层错误 */
    { AGENTOS_ERR_SERVICE_NOT_READY, "SERVICE_NOT_READY", "服务未就绪" },
    { AGENTOS_ERR_SERVICE_BUSY,    "SERVICE_BUSY",        "服务繁忙" },
    { AGENTOS_ERR_SERVICE_STOPPED, "SERVICE_STOPPED",     "服务已停止" },
    { AGENTOS_ERR_SERVICE_CONFIG,  "SERVICE_CONFIG",      "服务配置错误" },
    
    /* IPC 错误 */
    { AGENTOS_ERR_IPC_CONNECT,     "IPC_CONNECT",         "IPC连接失败" },
    { AGENTOS_ERR_IPC_DISCONNECT,  "IPC_DISCONNECT",      "IPC断开连接" },
    { AGENTOS_ERR_IPC_TIMEOUT,     "IPC_TIMEOUT",         "IPC超时" },
    { AGENTOS_ERR_IPC_INVALID_MSG, "IPC_INVALID_MSG",     "IPC无效消息" },
    { AGENTOS_ERR_IPC_BUFFER_FULL, "IPC_BUFFER_FULL",     "IPC缓冲区满" },
    
    /* LLM 服务错误 */
    { AGENTOS_ERR_LLM_NO_PROVIDER, "LLM_NO_PROVIDER",     "无可用LLM提供商" },
    { AGENTOS_ERR_LLM_API_ERROR,   "LLM_API_ERROR",       "LLM API错误" },
    { AGENTOS_ERR_LLM_RATE_LIMIT,  "LLM_RATE_LIMIT",      "LLM速率限制" },
    { AGENTOS_ERR_LLM_CONTEXT_LEN, "LLM_CONTEXT_LEN",     "LLM上下文长度超限" },
    
    /* 工具服务错误 */
    { AGENTOS_ERR_TOOL_NOT_FOUND,  "TOOL_NOT_FOUND",      "工具未找到" },
    { AGENTOS_ERR_TOOL_EXEC_FAILED,"TOOL_EXEC_FAILED",    "工具执行失败" },
    { AGENTOS_ERR_TOOL_TIMEOUT,    "TOOL_TIMEOUT",        "工具执行超时" },
    { AGENTOS_ERR_TOOL_PERMISSION, "TOOL_PERMISSION",     "工具权限不足" },
    
    /* 调度服务错误 */
    { AGENTOS_ERR_SCHED_NO_AGENT,  "SCHED_NO_AGENT",      "无可用Agent" },
    { AGENTOS_ERR_SCHED_AGENT_BUSY,"SCHED_AGENT_BUSY",    "Agent繁忙" },
    { AGENTOS_ERR_SCHED_STRATEGY,  "SCHED_STRATEGY",      "调度策略错误" },
    
    /* 市场服务错误 */
    { AGENTOS_ERR_MARKET_NOT_FOUND,"MARKET_NOT_FOUND",    "市场项未找到" },
    { AGENTOS_ERR_MARKET_INSTALL,  "MARKET_INSTALL",      "安装失败" },
    { AGENTOS_ERR_MARKET_DEPENDENCY,"MARKET_DEPENDENCY",  "依赖解析失败" },
    
    /* 监控服务错误 */
    { AGENTOS_ERR_MONITOR_METRIC,  "MONITOR_METRIC",      "指标采集失败" },
    { AGENTOS_ERR_MONITOR_ALERT,   "MONITOR_ALERT",       "告警处理失败" },
    { AGENTOS_ERR_MONITOR_TRACE,   "MONITOR_TRACE",       "追踪失败" },
    
    { 0, NULL, NULL }
};

/* ==================== 线程本地错误链 ==================== */

#define TLS_CHAIN_SIZE 16

typedef struct {
    agentos_error_chain_t chain;
    int initialized;
} tls_error_storage_t;

static AGENTOS_THREAD_LOCAL tls_error_storage_t g_tls_error = {0};

/* ==================== 全局统计 ==================== */

static struct {
    uint64_t total_errors;
    uint64_t errors_by_category[16];
    agentos_mutex_t lock;
    int initialized;
} g_error_stats = {0};

/* ==================== 内部函数 ==================== */

static void init_tls_if_needed(void) {
    if (!g_tls_error.initialized) {
        memset(&g_tls_error, 0, sizeof(g_tls_error));
        g_tls_error.chain.depth = 0;
        g_tls_error.initialized = 1;
    }
}

static void init_stats_if_needed(void) {
    if (!g_error_stats.initialized) {
        agentos_mutex_init(&g_error_stats.lock);
        g_error_stats.initialized = 1;
    }
}

static int get_error_category(agentos_error_t code) {
    if (code >= 0) return 0;
    code = -code;
    
    if (code < 1000) return 0;        /* 通用 */
    if (code < 2000) return 1;        /* 服务 */
    if (code < 3000) return 2;        /* IPC */
    if (code < 4000) return 3;        /* LLM */
    if (code < 5000) return 4;        /* 工具 */
    if (code < 6000) return 5;        /* 调度 */
    if (code < 7000) return 6;        /* 市场 */
    if (code < 8000) return 7;        /* 监控 */
    return 15;
}

/* ==================== 公共接口实现 ==================== */

const char* agentos_strerror(agentos_error_t code) {
    for (int i = 0; g_error_descriptions[i].name != NULL; i++) {
        if (g_error_descriptions[i].code == code) {
            return g_error_descriptions[i].description;
        }
    }
    
    static char unknown_buf[64];
    snprintf(unknown_buf, sizeof(unknown_buf), "未知错误 (%d)", code);
    return unknown_buf;
}

const char* agentos_error_name(agentos_error_t code) {
    for (int i = 0; g_error_descriptions[i].name != NULL; i++) {
        if (g_error_descriptions[i].code == code) {
            return g_error_descriptions[i].name;
        }
    }
    return "UNKNOWN";
}

agentos_error_chain_t* agentos_error_get_chain(void) {
    init_tls_if_needed();
    return &g_tls_error.chain;
}

void agentos_error_clear(void) {
    init_tls_if_needed();
    
    for (int i = 0; i < g_tls_error.chain.depth; i++) {
        if (g_tls_error.chain.contexts[i].file) {
            free((void*)g_tls_error.chain.contexts[i].file);
        }
        if (g_tls_error.chain.contexts[i].func) {
            free((void*)g_tls_error.chain.contexts[i].func);
        }
        if (g_tls_error.chain.contexts[i].message) {
            free((void*)g_tls_error.chain.contexts[i].message);
        }
    }
    
    g_tls_error.chain.depth = 0;
    g_tls_error.chain.code = AGENTOS_OK;
}

void agentos_error_push_ex(agentos_error_t code,
                           const char* file,
                           int line,
                           const char* func,
                           const char* fmt, ...) {
    init_tls_if_needed();
    init_stats_if_needed();
    
    /* 更新统计 */
    agentos_mutex_lock(&g_error_stats.lock);
    g_error_stats.total_errors++;
    g_error_stats.errors_by_category[get_error_category(code)]++;
    agentos_mutex_unlock(&g_error_stats.lock);
    
    /* 设置主错误码 */
    if (g_tls_error.chain.depth == 0) {
        g_tls_error.chain.code = code;
    }
    
    /* 添加上下文 */
    if (g_tls_error.chain.depth < AGENTOS_ERROR_CONTEXT_MAX_DEPTH) {
        agentos_error_context_t* ctx = &g_tls_error.chain.contexts[g_tls_error.chain.depth];
        
        ctx->file = file ? strdup(file) : NULL;
        ctx->line = line;
        ctx->func = func ? strdup(func) : NULL;
        
        /* 格式化消息 */
        if (fmt) {
            va_list args;
            va_start(args, fmt);
            
            size_t needed = vsnprintf(NULL, 0, fmt, args) + 1;
            va_end(args);
            
            char* msg = malloc(needed);
            if (msg) {
                va_start(args, fmt);
                vsnprintf(msg, needed, fmt, args);
                va_end(args);
                ctx->message = msg;
            }
        } else {
            ctx->message = strdup(agentos_strerror(code));
        }
        
        g_tls_error.chain.depth++;
    }
}

void agentos_error_print_chain(const agentos_error_chain_t* chain) {
    if (!chain || chain->depth == 0) {
        printf("No error\n");
        return;
    }
    
    printf("Error Chain (depth=%d, code=%d):\n", chain->depth, chain->code);
    printf("  Main Error: %s\n", agentos_strerror(chain->code));
    
    for (int i = 0; i < chain->depth; i++) {
        const agentos_error_context_t* ctx = &chain->contexts[i];
        printf("  [%d] %s:%d in %s(): %s\n",
               i,
               ctx->file ? ctx->file : "unknown",
               ctx->line,
               ctx->func ? ctx->func : "unknown",
               ctx->message ? ctx->message : "");
    }
}

char* agentos_error_chain_to_json(const agentos_error_chain_t* chain) {
    if (!chain) {
        return strdup("{\"error\":null}");
    }
    
    size_t buf_size = 4096;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    int pos = snprintf(buf, buf_size,
        "{\n"
        "  \"code\": %d,\n"
        "  \"name\": \"%s\",\n"
        "  \"message\": \"%s\",\n"
        "  \"depth\": %d,\n"
        "  \"contexts\": [\n",
        chain->code,
        agentos_error_name(chain->code),
        agentos_strerror(chain->code),
        chain->depth
    );
    
    for (int i = 0; i < chain->depth && pos < (int)buf_size - 256; i++) {
        const agentos_error_context_t* ctx = &chain->contexts[i];
        pos += snprintf(buf + pos, buf_size - pos,
            "    {\n"
            "      \"file\": \"%s\",\n"
            "      \"line\": %d,\n"
            "      \"function\": \"%s\",\n"
            "      \"message\": \"%s\"\n"
            "    }%s\n",
            ctx->file ? ctx->file : "unknown",
            ctx->line,
            ctx->func ? ctx->func : "unknown",
            ctx->message ? ctx->message : "",
            (i < chain->depth - 1) ? "," : ""
        );
    }
    
    pos += snprintf(buf + pos, buf_size - pos,
        "  ]\n"
        "}"
    );
    
    return buf;
}

void agentos_error_get_stats(agentos_error_stats_t* stats) {
    if (!stats) return;
    
    init_stats_if_needed();
    
    agentos_mutex_lock(&g_error_stats.lock);
    stats->total_errors = g_error_stats.total_errors;
    memcpy(stats->errors_by_category, g_error_stats.errors_by_category, 
           sizeof(g_error_stats.errors_by_category));
    agentos_mutex_unlock(&g_error_stats.lock);
}

void agentos_error_reset_stats(void) {
    init_stats_if_needed();
    
    agentos_mutex_lock(&g_error_stats.lock);
    g_error_stats.total_errors = 0;
    memset(g_error_stats.errors_by_category, 0, sizeof(g_error_stats.errors_by_category));
    agentos_mutex_unlock(&g_error_stats.lock);
}
