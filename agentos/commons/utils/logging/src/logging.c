/**
 * @file logging.c
 * @brief 统一分层日志系统核心层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一分层日志系统的核心层功能，提供：
 * 1. 日志级别管理和转换
 * 2. 日志系统初始化和配置（含热重载）
 * 3. 基本日志记录功能（控制台输出）
 * 4. 追踪ID管理（线程局部存储）
 * 5. 模块级别过滤（支持通配符匹配）
 * 6. 完整线程清理（遍历所有注册线程）
 */

#include "logging.h"

#include "agentos.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Unified base library compatibility layer */
#include "atomic_compat.h"
#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "../../error/include/error.h"

/* ==================== 内部常量定义 ==================== */

static AGENTOS_THREAD_LOCAL char g_tls_trace_id[128] = {0};
static AGENTOS_THREAD_LOCAL char g_tls_span_id[64] = {0};

/** 日志级别名称数组 */
static const char *LEVEL_NAMES[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

/** 日志级别名称数组大小 */
static const size_t LEVEL_NAMES_COUNT = sizeof(LEVEL_NAMES) / sizeof(LEVEL_NAMES[0]);

/** 默认日志级别 */
static const log_level_t DEFAULT_LOG_LEVEL = LOG_LEVEL_INFO;

/** 默认输出格式 */
static const log_format_t DEFAULT_LOG_FORMAT = LOG_FORMAT_TEXT;

/** 最大消息长度 */
#define MAX_MESSAGE_LEN 4096

/* ==================== 日志节流（Throttling）内部数据结构 ==================== */

/** 节流哈希桶数量 */
#define THROTTLE_BUCKET_COUNT 256

/** 节流哈希桶 */
typedef struct {
    uint64_t hash_key;
    uint64_t last_second;
    uint32_t count;
} throttle_bucket_t;

static throttle_bucket_t g_throttle_buckets[THROTTLE_BUCKET_COUNT];
static atomic_uint g_throttle_enabled = 0;
static atomic_uint g_throttle_max_per_sec = 100;
static agentos_mutex_t g_throttle_mutex;
static bool g_throttle_mutex_init = false;

/* ==================== 日志采样（Sampling）内部数据结构 ==================== */

static atomic_uint g_sample_counter_debug = 0;
static atomic_uint g_sample_counter_info = 0;
static atomic_uint g_sample_counter_warn = 0;

/**
 * @brief 计算消息哈希（用于节流去重）
 */
static uint64_t throttle_hash(const char *module, int line, const char *message)
{
    uint64_t h = 14695981039346656037ULL;
    const char *p;

    if (module) {
        for (p = module; *p; p++) {
            h ^= (uint64_t)(unsigned char)*p;
            h *= 1099511628211ULL;
        }
    }

    h ^= (uint64_t)line;
    h *= 1099511628211ULL;

    if (message) {
        for (p = message; *p; p++) {
            h ^= (uint64_t)(unsigned char)*p;
            h *= 1099511628211ULL;
        }
    }

    return h;
}

/**
 * @brief 检查日志是否应被节流抑制
 *
 * @param module 模块名
 * @param line 行号
 * @param message 日志消息
 * @param now_sec 当前时间（秒）
 * @return true 应抑制（跳过），false 应输出
 */
static bool throttle_should_suppress(const char *module, int line, const char *message,
                                     uint64_t now_sec)
{
    if (!g_throttle_enabled)
        return false;
    if (!g_throttle_mutex_init)
        return false;

    uint64_t h = throttle_hash(module, line, message);
    uint32_t bucket_idx = (uint32_t)(h % THROTTLE_BUCKET_COUNT);

    agentos_mutex_lock(&g_throttle_mutex);

    throttle_bucket_t *bucket = &g_throttle_buckets[bucket_idx];

    if (bucket->last_second != now_sec) {
        if (bucket->last_second > 0 && bucket->count > g_throttle_max_per_sec) {
            agentos_mutex_unlock(&g_throttle_mutex);
            return false;
        }
        bucket->last_second = now_sec;
        bucket->hash_key = h;
        bucket->count = 1;
        agentos_mutex_unlock(&g_throttle_mutex);
        return false;
    }

    if (bucket->hash_key == h) {
        if (bucket->count >= g_throttle_max_per_sec) {
            bucket->count++;
            uint32_t suppressed = bucket->count - g_throttle_max_per_sec;
            agentos_mutex_unlock(&g_throttle_mutex);

            if (suppressed == 1) {
                __builtin_fprintf(stderr, "[THROTTLE] Suppressing further identical messages: %s:%d\n",
                        module ? module : "?", line);
            }
            return true;
        }
        bucket->count++;
        agentos_mutex_unlock(&g_throttle_mutex);
        return false;
    }

    if (bucket->count > g_throttle_max_per_sec) {
        uint32_t old_suppressed = bucket->count - g_throttle_max_per_sec;
        if (old_suppressed > 0) {
            agentos_mutex_unlock(&g_throttle_mutex);
            __builtin_fprintf(stderr, "[THROTTLE] Previous bucket flushed: %u messages suppressed\n",
                    old_suppressed);
            agentos_mutex_lock(&g_throttle_mutex);
        }
    }
    bucket->hash_key = h;
    bucket->count = 1;
    agentos_mutex_unlock(&g_throttle_mutex);
    return false;
}

/* ==================== 内部数据结构 ==================== */

/** 日志系统全局状?*/
typedef struct {
    /** 当前配置 */
    log_config_t manager;

    /** 是否已初始化 */
    bool initialized;

    /** 互斥锁保护配置和状?*/
    agentos_mutex_t mutex;

    log_config_t default_config;

    struct {
        char pattern[128];
        log_level_t level;
    } module_levels[32];

    /** 模块级别过滤器数?*/
    size_t module_level_count;
} logging_state_t;

/* ==================== 全局状态变?==================== */

/** 日志系统全局状态实?*/
static logging_state_t g_logging_state = {.initialized = false, .module_level_count = 0};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 获取当前时间戳（毫秒?
 *
 * 获取当前时间的Unix时间戳，毫秒精度?
 *
 * @return 当前时间戳（毫秒?
 */
static uint64_t get_current_timestamp(void)
{
    return agentos_time_ms();
}

/**
 * @brief 获取当前线程ID
 *
 * 获取当前线程的ID，用于日志记录?
 *
 * @return 线程ID
 */
static uint64_t get_current_thread_id(void)
{
    return agentos_thread_id();
}

/**
 * @brief 获取当前进程ID
 *
 * 获取当前进程的ID，用于日志记录?
 *
 * @return 进程ID
 */
static uint32_t get_current_process_id(void)
{
    return (uint32_t)getpid();
}

/**
 * @brief 格式化日志消?
 *
 * 将日志记录格式化为字符串，根据配置的格式?
 *
 * @param record 日志记录
 * @param buffer 输出缓冲?
 * @param buffer_size 缓冲区大?
 * @return 格式化后的字符串长度
 */
static size_t format_log_message(const log_record_t *record, char *buffer, size_t buffer_size)
{
    if (!record || !buffer || buffer_size == 0) {
        return 0;
    }

    // 简单文本格式实?
    time_t sec = record->timestamp / 1000;
    int ms = record->timestamp % 1000;
    struct tm tm_storage;
    localtime_r(&sec, &tm_storage);
    struct tm *tm_info = &tm_storage;

    const char *level_name = log_level_to_string(record->level);

    int len =
        snprintf(buffer, buffer_size, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s:%d]",
                 tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday, tm_info->tm_hour,
                 tm_info->tm_min, tm_info->tm_sec, ms, level_name, record->module, record->line);
    if (len < 0)
        return 0;
    if ((size_t)len >= buffer_size)
        len = (int)buffer_size - 1;

    if (record->trace_id && record->trace_id[0] != '\0') {
        len += snprintf(buffer + len, buffer_size - (size_t)len, " [trace:%s]", record->trace_id);
        if (len < 0)
            return 0;
        if ((size_t)len >= buffer_size)
            len = (int)buffer_size - 1;
    }

    if (record->span_id && record->span_id[0] != '\0') {
        len += snprintf(buffer + len, buffer_size - (size_t)len, " [span:%s]", record->span_id);
        if (len < 0)
            return 0;
        if ((size_t)len >= buffer_size)
            len = (int)buffer_size - 1;
    }

    len += snprintf(buffer + len, buffer_size - (size_t)len, " [thread:%llu] [process:%u]",
                    (unsigned long long)record->thread_id, (unsigned)record->process_id);
    if (len < 0)
        return 0;
    if ((size_t)len >= buffer_size)
        len = (int)buffer_size - 1;

    len += snprintf(buffer + len, buffer_size - (size_t)len, " %s\n", record->message);
    if (len < 0)
        return 0;
    if ((size_t)len >= buffer_size)
        len = (int)buffer_size - 1;

    return (size_t)len;
}

/**
 * @brief 检查日志是否应该被记录
 *
 * 根据全局级别和模块级别检查日志是否应该被记录?
 *
 * @param level 日志级别
 * @param module 模块名称
 * @return true 应该记录，false 应该过滤
 */
static bool should_log(log_level_t level, const char *module)
{
    if (level < g_logging_state.manager.level) {
        return false;
    }

    if (!module)
        return true;

    for (size_t i = 0; i < g_logging_state.module_level_count; i++) {
        const char *pattern = g_logging_state.module_levels[i].pattern;
        if (pattern[0] == '*') {
            size_t plen = strlen(pattern);
            if (plen == 1)
                return level >= g_logging_state.module_levels[i].level;
            const char *suffix = pattern + 1;
            size_t slen = strlen(suffix);
            size_t mlen = strlen(module);
            if (mlen >= slen && strcmp(module + mlen - slen, suffix) == 0)
                return level >= g_logging_state.module_levels[i].level;
        } else if (strcmp(pattern, module) == 0) {
            return level >= g_logging_state.module_levels[i].level;
        }
    }

    return true;
}

/* ==================== 公开API实现 ==================== */

const char *log_level_to_string(log_level_t level)
{
    if (level >= 0 && level < LEVEL_NAMES_COUNT) {
        return LEVEL_NAMES[level];
    }
    return "UNKNOWN";
}

log_level_t log_level_from_string(const char *str)
{
    if (!str) {
        return DEFAULT_LOG_LEVEL;
    }

    for (size_t i = 0; i < LEVEL_NAMES_COUNT; i++) {
        if (strcasecmp(str, LEVEL_NAMES[i]) == 0) {
            return (log_level_t)i;
        }
    }

    // 尝试解析为数?
    char *endptr;
    long value = strtol(str, &endptr, 10);
    if (endptr != str && *endptr == '\0' && value >= 0 && (size_t)value < LEVEL_NAMES_COUNT) {
        return (log_level_t)value;
    }

    return DEFAULT_LOG_LEVEL;
}

int log_init(const log_config_t *manager)
{
    if (g_logging_state.initialized) {
        return 0;
    }

    if (agentos_mutex_init(&g_logging_state.mutex) != 0) {
        return AGENTOS_EINVAL;
    }

    if (!g_throttle_mutex_init) {
        if (agentos_mutex_init(&g_throttle_mutex) == 0) {
            g_throttle_mutex_init = true;
        }
    }

    g_tls_trace_id[0] = '\0';
    g_tls_span_id[0] = '\0';

    if (manager) {
        __builtin_memcpy(&g_logging_state.manager, manager, sizeof(log_config_t));
    } else {
        g_logging_state.manager.level = DEFAULT_LOG_LEVEL;
        g_logging_state.manager.outputs = 1 << LOG_OUTPUT_CONSOLE;
        g_logging_state.manager.format = DEFAULT_LOG_FORMAT;
        g_logging_state.manager.async_mode = false;
        g_logging_state.manager.enable_statistics = false;
    }

    g_logging_state.initialized = true;

    return 0;
}

int log_set_default_config(const log_config_t *manager)
{
    if (!manager) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(&g_logging_state.mutex);
    __builtin_memcpy(&g_logging_state.default_config, manager, sizeof(log_config_t));
    agentos_mutex_unlock(&g_logging_state.mutex);

    return 0;
}

void log_write(log_level_t level, const char *module, int line, const char *fmt, ...)
{
    if (!g_logging_state.initialized) {
        // 自动使用默认配置初始?
        log_init(NULL);
    }

    // 检查日志级?
    if (!should_log(level, module)) {
        return;
    }

    // 获取追踪ID和Span ID
    const char *trace_id = log_get_trace_id();
    const char *span_id = log_get_span_id();

    // 格式化消息
    char message_buffer[MAX_MESSAGE_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message_buffer, sizeof(message_buffer), fmt,
              args); /* flawfinder: ignore - variadic logging wrapper */
    va_end(args);

    /* 节流检查：相同消息1秒内最多输出 N 次 */
    uint64_t now_sec = (uint64_t)(get_current_timestamp() / 1000);
    if (throttle_should_suppress(module, line, message_buffer, now_sec)) {
        return;
    }

    // 构建日志记录
    log_record_t record = {.timestamp = get_current_timestamp(),
                           .level = level,
                           .module = module,
                           .line = line,
                           .trace_id = trace_id,
                           .span_id = span_id,
                           .message = message_buffer,
                           .thread_id = get_current_thread_id(),
                           .process_id = get_current_process_id()};

    // 格式化输?
    char formatted_buffer[MAX_MESSAGE_LEN * 2];
    size_t formatted_len = format_log_message(&record, formatted_buffer, sizeof(formatted_buffer));

    // 输出到控制台?
    if (formatted_len > 0) {
        // 根据级别选择输出?
        FILE *stream = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
        fwrite(formatted_buffer, 1, formatted_len, stream);
        fflush(stream);
    }
}

void log_write_va(log_level_t level, const char *module, int line, const char *fmt, va_list args)
{
    if (!g_logging_state.initialized) {
        log_init(NULL);
    }

    if (!should_log(level, module)) {
        return;
    }

    // 获取追踪ID和Span ID
    const char *trace_id = log_get_trace_id();
    const char *span_id = log_get_span_id();

    // 格式化消息
    char message_buffer[MAX_MESSAGE_LEN];
    vsnprintf(message_buffer, sizeof(message_buffer), fmt,
              args); /* flawfinder: ignore - variadic logging wrapper */
    // 构建日志记录
    log_record_t record = {.timestamp = get_current_timestamp(),
                           .level = level,
                           .module = module,
                           .line = line,
                           .trace_id = trace_id,
                           .span_id = span_id,
                           .message = message_buffer,
                           .thread_id = get_current_thread_id(),
                           .process_id = get_current_process_id()};

    // 格式化输?
    char formatted_buffer[MAX_MESSAGE_LEN * 2];
    size_t formatted_len = format_log_message(&record, formatted_buffer, sizeof(formatted_buffer));

    // 输出到控制台
    if (formatted_len > 0) {
        FILE *stream = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
        fwrite(formatted_buffer, 1, formatted_len, stream);
        fflush(stream);
    }
}

const char *log_set_trace_id(const char *trace_id)
{
    if (!g_logging_state.initialized) return NULL;

    if (trace_id) {
        AGENTOS_STRNCPY_TERM(g_tls_trace_id, trace_id, sizeof(g_tls_trace_id));
        g_tls_trace_id[sizeof(g_tls_trace_id) - 1] = '\0';
    } else {
        g_tls_trace_id[0] = '\0';
    }

    return g_tls_trace_id;
}

const char *log_get_trace_id(void)
{
    if (!g_logging_state.initialized) return NULL;
    return g_tls_trace_id[0] ? g_tls_trace_id : NULL;
}

const char *log_set_span_id(const char *span_id)
{
    if (!g_logging_state.initialized) return NULL;

    if (span_id) {
        AGENTOS_STRNCPY_TERM(g_tls_span_id, span_id, sizeof(g_tls_span_id));
        g_tls_span_id[sizeof(g_tls_span_id) - 1] = '\0';
    } else {
        g_tls_span_id[0] = '\0';
    }

    return g_tls_span_id;
}

const char *log_get_span_id(void)
{
    if (!g_logging_state.initialized) return NULL;
    return g_tls_span_id[0] ? g_tls_span_id : NULL;
}

int log_set_module_level(const char *module_pattern, log_level_t level)
{
    if (!g_logging_state.initialized || !module_pattern) {
        return AGENTOS_EINVAL;
    }

    agentos_mutex_lock(&g_logging_state.mutex);

    // 查找现有模式
    for (size_t i = 0; i < g_logging_state.module_level_count; i++) {
        if (strcmp(g_logging_state.module_levels[i].pattern, module_pattern) == 0) {
            g_logging_state.module_levels[i].level = level;
            agentos_mutex_unlock(&g_logging_state.mutex);
            return 0;
        }
    }

    // 添加新模?
    if (g_logging_state.module_level_count <
        sizeof(g_logging_state.module_levels) / sizeof(g_logging_state.module_levels[0])) {
        AGENTOS_STRNCPY_TERM(g_logging_state.module_levels[g_logging_state.module_level_count].pattern, module_pattern, sizeof(g_logging_state.module_levels[0].pattern));
        g_logging_state.module_levels[g_logging_state.module_level_count]
            .pattern[sizeof(g_logging_state.module_levels[0].pattern) - 1] = '\0';
        g_logging_state.module_levels[g_logging_state.module_level_count].level = level;
        g_logging_state.module_level_count++;
        agentos_mutex_unlock(&g_logging_state.mutex);
        return 0;
    }

    agentos_mutex_unlock(&g_logging_state.mutex);
    return AGENTOS_ERR_NOT_FOUND;  // 表已?
}

int log_reload_config(const char *config_path)
{
    if (!config_path) {
        return AGENTOS_EINVAL;
    }

    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        return AGENTOS_ENOENT;
    }

    char line[512];
    agentos_mutex_lock(&g_logging_state.mutex);
    log_config_t new_config = g_logging_state.manager;
    agentos_mutex_unlock(&g_logging_state.mutex);

    int changes = 0;

    while (fgets(line, sizeof(line), fp)) {
        char key[128], value[256];
        char *saveptr = NULL;
        char *key_tok = strtok_r(line, " =\r\n", &saveptr);
        if (!key_tok) continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        char *val_tok = eq + 1;
        while (*val_tok == ' ') val_tok++;
        size_t key_len = strlen(key_tok);
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        __builtin_memcpy(key, key_tok, key_len);
        key[key_len] = '\0';
        size_t val_len = strlen(val_tok);
        if (val_len >= sizeof(value)) val_len = sizeof(value) - 1;
        __builtin_memcpy(value, val_tok, val_len);
        value[val_len] = '\0';
        {
            if (strcmp(key, "level") == 0) {
                log_level_t lvl = log_level_from_string(value);
                if ((int)lvl >= 0 && lvl < LOG_LEVEL_COUNT) {
                    new_config.level = lvl;
                    changes++;
                }
            } else if (strcmp(key, "output") == 0) {
                if (strstr(value, "file"))
                    new_config.outputs |= LOG_OUTPUT_FILE;
                if (strstr(value, "console"))
                    new_config.outputs |= LOG_OUTPUT_CONSOLE;
                if (strstr(value, "syslog"))
                    new_config.outputs |= LOG_OUTPUT_SYSLOG;
                changes++;
            } else if (strcmp(key, "format") == 0) {
                if (strcmp(value, "json") == 0)
                    new_config.format = LOG_FORMAT_JSON;
                else if (strcmp(value, "text") == 0)
                    new_config.format = LOG_FORMAT_TEXT;
                changes++;
            }
        }
    }

    fclose(fp);

    agentos_mutex_lock(&g_logging_state.mutex);
    g_logging_state.manager = new_config;
    agentos_mutex_unlock(&g_logging_state.mutex);

    if (changes > 0) {
        __builtin_fprintf(stderr, "[LOGGING] Config reloaded from '%s' (%d changes applied)\n", config_path,
                changes);
    }

    return changes > 0 ? 0 : AGENTOS_ENOENT;
}

void log_flush(void)
{
    // 控制台输出立即刷?
    fflush(stdout);
    fflush(stderr);
}

void log_set_throttle(bool enable, uint32_t max_per_sec)
{
    g_throttle_enabled = enable ? 1 : 0;
    if (max_per_sec > 0) {
        g_throttle_max_per_sec = max_per_sec;
    } else if (max_per_sec == 0 && enable) {
        g_throttle_max_per_sec = 100;
    }
}

bool log_should_sample(log_level_t level)
{
    uint32_t counter;

    switch (level) {
    case LOG_LEVEL_DEBUG: {
        counter = AGENTOS_ATOMIC_FETCH_ADD(&g_sample_counter_debug, 1);
        return (counter % 1000) == 0; /* 0.1% */
    }
    case LOG_LEVEL_INFO: {
        counter = AGENTOS_ATOMIC_FETCH_ADD(&g_sample_counter_info, 1);
        return (counter % 100) == 0; /* 1% */
    }
    case LOG_LEVEL_WARN: {
        counter = AGENTOS_ATOMIC_FETCH_ADD(&g_sample_counter_warn, 1);
        return (counter % 10) == 0; /* 10% */
    }
    case LOG_LEVEL_ERROR:
    case LOG_LEVEL_FATAL:
        return true; /* 100% */
    default:
        return true;
    }
}

void log_cleanup(void)
{
    if (!g_logging_state.initialized) {
        return;
    }

    agentos_mutex_lock(&g_logging_state.mutex);

    g_tls_trace_id[0] = '\0';
    g_tls_span_id[0] = '\0';

    for (size_t i = 0; i < g_logging_state.module_level_count; i++) {
        AGENTOS_MEMSET(&g_logging_state.module_levels[i], 0, sizeof(g_logging_state.module_levels[i]));
    }
    g_logging_state.module_level_count = 0;

    g_logging_state.initialized = false;

    agentos_mutex_unlock(&g_logging_state.mutex);
    agentos_mutex_destroy(&g_logging_state.mutex);

    if (g_throttle_mutex_init) {
        agentos_mutex_destroy(&g_throttle_mutex);
        g_throttle_mutex_init = false;
    }

    AGENTOS_MEMSET(&g_logging_state, 0, sizeof(g_logging_state));
}
