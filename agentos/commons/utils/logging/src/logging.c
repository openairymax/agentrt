/**
 * @file logging.c
 * @brief 统一分层日志系统核心层实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一分层日志系统的核心层功能，提供：
 * 1. 日志级别管理和转�?
 * 2. 日志系统初始化和配置
 * 3. 基本日志记录功能（控制台输出�?
 * 4. 追踪ID管理
 * 5. 线程安全的基础操作
 *
 * 注意：这是一个基础实现，高级功能（文件轮转、网络传输、异步缓冲等�?
 * 由原子层和服务层提供，可通过条件编译启用�?
 */

#include "logging.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../utils/memory/include/memory_compat.h"
#include "../../utils/string/include/string_compat.h"
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

/* ==================== 内部常量定义 ==================== */

/** 日志级别名称数组 */
static const char* LEVEL_NAMES[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

/** 日志级别名称数组大小 */
static const size_t LEVEL_NAMES_COUNT = sizeof(LEVEL_NAMES) / sizeof(LEVEL_NAMES[0]);

/** 默认日志级别 */
static const log_level_t DEFAULT_LOG_LEVEL = LOG_LEVEL_INFO;

/** 默认输出格式 */
static const log_format_t DEFAULT_LOG_FORMAT = LOG_FORMAT_TEXT;

/** 最大模块名称长�?*/
static const size_t MAX_MODULE_NAME_LEN = 256;

/** 最大消息长�?*/
static const size_t MAX_MESSAGE_LEN = 4096;

/* ==================== 内部数据结构 ==================== */

/** 日志系统全局状�?*/
typedef struct {
    /** 当前配置 */
    log_config_t manager;
    
    /** 是否已初始化 */
    bool initialized;
    
    /** 互斥锁保护配置和状�?*/
    pthread_mutex_t mutex;
    
    /** 默认配置 */
    log_config_t default_config;
    
    /** 线程局部存储的追踪ID */
    pthread_key_t trace_id_key;
    
    /** 模块级别过滤器表（简化实现） */
    struct {
        char pattern[128];
        log_level_t level;
    } module_levels[32];
    
    /** 模块级别过滤器数�?*/
    size_t module_level_count;
} logging_state_t;

/* ==================== 全局状态变�?==================== */

/** 日志系统全局状态实�?*/
static logging_state_t g_logging_state = {
    .initialized = false,
    .module_level_count = 0
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 获取当前时间戳（毫秒�?
 * 
 * 获取当前时间的Unix时间戳，毫秒精度�?
 * 
 * @return 当前时间戳（毫秒�?
 */
static uint64_t get_current_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

/**
 * @brief 获取当前线程ID
 * 
 * 获取当前线程的ID，用于日志记录�?
 * 
 * @return 线程ID
 */
static uint64_t get_current_thread_id(void) {
    return (uint64_t)pthread_self();
}

/**
 * @brief 获取当前进程ID
 * 
 * 获取当前进程的ID，用于日志记录�?
 * 
 * @return 进程ID
 */
static uint32_t get_current_process_id(void) {
    return (uint32_t)getpid();
}

/**
 * @brief 格式化日志消�?
 * 
 * 将日志记录格式化为字符串，根据配置的格式�?
 * 
 * @param record 日志记录
 * @param buffer 输出缓冲�?
 * @param buffer_size 缓冲区大�?
 * @return 格式化后的字符串长度
 */
static size_t format_log_message(const log_record_t* record, char* buffer, size_t buffer_size) {
    if (!record || !buffer || buffer_size == 0) {
        return 0;
    }
    
    // 简单文本格式实�?
    time_t sec = record->timestamp / 1000;
    int ms = record->timestamp % 1000;
    struct tm* tm_info = localtime(&sec);
    
    const char* level_name = log_level_to_string(record->level);
    
    // 构建日志消息
    int len = snprintf(buffer, buffer_size,
        "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s:%d]",
        tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, ms,
        level_name, record->module, record->line);
    
    // 添加追踪ID（如果存在）
    if (record->trace_id && record->trace_id[0] != '\0') {
        len += snprintf(buffer + len, buffer_size - len,
            " [trace:%s]", record->trace_id);
    }
    
    // 添加线程/进程ID
    len += snprintf(buffer + len, buffer_size - len,
        " [thread:%llu] [process:%u]",
        (unsigned long long)record->thread_id,
        (unsigned)record->process_id);
    
    // 添加消息内容
    len += snprintf(buffer + len, buffer_size - len,
        " %s\n", record->message);
    
    return (size_t)len;
}

/**
 * @brief 检查日志是否应该被记录
 * 
 * 根据全局级别和模块级别检查日志是否应该被记录�?
 * 
 * @param level 日志级别
 * @param module 模块名称
 * @return true 应该记录，false 应该过滤
 */
static bool should_log(log_level_t level, const char* module) {
    // 检查全局级别
    if (level < g_logging_state.manager.level) {
        return false;
    }
    
    // 检查模块级别（简化实现：精确匹配�?
    for (size_t i = 0; i < g_logging_state.module_level_count; i++) {
        if (strcmp(g_logging_state.module_levels[i].pattern, module) == 0) {
            return level >= g_logging_state.module_levels[i].level;
        }
    }
    
    return true;
}

/* ==================== 公开API实现 ==================== */

const char* log_level_to_string(log_level_t level) {
    if (level >= 0 && level < LEVEL_NAMES_COUNT) {
        return LEVEL_NAMES[level];
    }
    return "UNKNOWN";
}

log_level_t log_level_from_string(const char* str) {
    if (!str) {
        return DEFAULT_LOG_LEVEL;
    }
    
    for (size_t i = 0; i < LEVEL_NAMES_COUNT; i++) {
        if (strcasecmp(str, LEVEL_NAMES[i]) == 0) {
            return (log_level_t)i;
        }
    }
    
    // 尝试解析为数�?
    char* endptr;
    long value = strtol(str, &endptr, 10);
    if (endptr != str && *endptr == '\0' && value >= 0 && value < LEVEL_NAMES_COUNT) {
        return (log_level_t)value;
    }
    
    return DEFAULT_LOG_LEVEL;
}

int log_init(const log_config_t* manager) {
    if (g_logging_state.initialized) {
        // 已经初始化，可以重新初始�?
        log_cleanup();
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_logging_state.mutex, NULL) != 0) {
        return -1;
    }
    
    // 初始化线程局部存�?
    if (pthread_key_create(&g_logging_state.trace_id_key, free) != 0) {
        pthread_mutex_destroy(&g_logging_state.mutex);
        return -2;
    }
    
    // 设置配置
    if (manager) {
        memcpy(&g_logging_state.manager, manager, sizeof(log_config_t));
    } else {
        // 使用默认配置
        g_logging_state.manager.level = DEFAULT_LOG_LEVEL;
        g_logging_state.manager.outputs = 1 << LOG_OUTPUT_CONSOLE; // 控制台输�?
        g_logging_state.manager.format = DEFAULT_LOG_FORMAT;
        g_logging_state.manager.async_mode = false;
        g_logging_state.manager.enable_statistics = false;
    }
    
    g_logging_state.initialized = true;
    
    // 记录初始化日�?
    LOG_INFO("日志系统初始化完成，级别: %s", 
             log_level_to_string(g_logging_state.manager.level));
    
    return 0;
}

int log_set_default_config(const log_config_t* manager) {
    if (!manager) {
        return -1;
    }
    
    pthread_mutex_lock(&g_logging_state.mutex);
    memcpy(&g_logging_state.default_config, manager, sizeof(log_config_t));
    pthread_mutex_unlock(&g_logging_state.mutex);
    
    return 0;
}

void log_write(log_level_t level, const char* module, int line, const char* fmt, ...) {
    if (!g_logging_state.initialized) {
        // 自动使用默认配置初始�?
        log_init(NULL);
    }
    
    // 检查日志级�?
    if (!should_log(level, module)) {
        return;
    }
    
    // 获取追踪ID
    const char* trace_id = log_get_trace_id();
    
    // 格式化消�?
    char message_buffer[MAX_MESSAGE_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message_buffer, sizeof(message_buffer), fmt, args);
    va_end(args);
    
    // 构建日志记录
    log_record_t record = {
        .timestamp = get_current_timestamp(),
        .level = level,
        .module = module,
        .line = line,
        .trace_id = trace_id,
        .message = message_buffer,
        .thread_id = get_current_thread_id(),
        .process_id = get_current_process_id()
    };
    
    // 格式化输�?
    char formatted_buffer[MAX_MESSAGE_LEN * 2];
    size_t formatted_len = format_log_message(&record, formatted_buffer, sizeof(formatted_buffer));
    
    // 输出到控制台（基础实现�?
    if (formatted_len > 0) {
        // 根据级别选择输出�?
        FILE* stream = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
        fwrite(formatted_buffer, 1, formatted_len, stream);
        fflush(stream);
    }
}

void log_write_va(log_level_t level, const char* module, int line, const char* fmt, va_list args) {
    if (!g_logging_state.initialized) {
        log_init(NULL);
    }
    
    if (!should_log(level, module)) {
        return;
    }
    
    // 获取追踪ID
    const char* trace_id = log_get_trace_id();
    
    // 格式化消�?
    char message_buffer[MAX_MESSAGE_LEN];
    vsnprintf(message_buffer, sizeof(message_buffer), fmt, args);
    
    // 构建日志记录
    log_record_t record = {
        .timestamp = get_current_timestamp(),
        .level = level,
        .module = module,
        .line = line,
        .trace_id = trace_id,
        .message = message_buffer,
        .thread_id = get_current_thread_id(),
        .process_id = get_current_process_id()
    };
    
    // 格式化输�?
    char formatted_buffer[MAX_MESSAGE_LEN * 2];
    size_t formatted_len = format_log_message(&record, formatted_buffer, sizeof(formatted_buffer));
    
    // 输出到控制台
    if (formatted_len > 0) {
        FILE* stream = (level >= LOG_LEVEL_ERROR) ? stderr : stdout;
        fwrite(formatted_buffer, 1, formatted_len, stream);
        fflush(stream);
    }
}

const char* log_set_trace_id(const char* trace_id) {
    if (!g_logging_state.initialized) {
        return NULL;
    }
    
    // 获取当前线程的追踪ID存储
    char* stored_id = pthread_getspecific(g_logging_state.trace_id_key);
    
    // 释放旧的追踪ID
    if (stored_id) {
        AGENTOS_FREE(stored_id);
    }
    
    // 设置新的追踪ID
    if (trace_id) {
        stored_id = AGENTOS_STRDUP(trace_id);
    } else {
        // 生成默认追踪ID
        static uint64_t counter = 0;
        char default_id[64];
        snprintf(default_id, sizeof(default_id), "trace-%llu-%llu",
                (unsigned long long)get_current_process_id(),
                (unsigned long long)counter++);
        stored_id = AGENTOS_STRDUP(default_id);
    }
    
    if (stored_id) {
        pthread_setspecific(g_logging_state.trace_id_key, stored_id);
    }
    
    return stored_id;
}

const char* log_get_trace_id(void) {
    if (!g_logging_state.initialized) {
        return NULL;
    }
    
    char* stored_id = pthread_getspecific(g_logging_state.trace_id_key);
    return stored_id;
}

int log_set_module_level(const char* module_pattern, log_level_t level) {
    if (!g_logging_state.initialized || !module_pattern) {
        return -1;
    }
    
    pthread_mutex_lock(&g_logging_state.mutex);
    
    // 查找现有模式
    for (size_t i = 0; i < g_logging_state.module_level_count; i++) {
        if (strcmp(g_logging_state.module_levels[i].pattern, module_pattern) == 0) {
            g_logging_state.module_levels[i].level = level;
            pthread_mutex_unlock(&g_logging_state.mutex);
            return 0;
        }
    }
    
    // 添加新模�?
    if (g_logging_state.module_level_count < sizeof(g_logging_state.module_levels) / sizeof(g_logging_state.module_levels[0])) {
        strncpy(g_logging_state.module_levels[g_logging_state.module_level_count].pattern,
                module_pattern,
                sizeof(g_logging_state.module_levels[0].pattern) - 1);
        g_logging_state.module_levels[g_logging_state.module_level_count].pattern[
            sizeof(g_logging_state.module_levels[0].pattern) - 1] = '\0';
        g_logging_state.module_levels[g_logging_state.module_level_count].level = level;
        g_logging_state.module_level_count++;
        pthread_mutex_unlock(&g_logging_state.mutex);
        return 0;
    }
    
    pthread_mutex_unlock(&g_logging_state.mutex);
    return -2; // 表已�?
}

int log_reload_config(const char* config_path) {
    // 简化实现：暂时不支持配置文件重�?
    (void)config_path;
    return 0;
}

void log_flush(void) {
    // 控制台输出立即刷�?
    fflush(stdout);
    fflush(stderr);
}

void log_cleanup(void) {
    if (!g_logging_state.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_logging_state.mutex);
    
    // 清理所有线程的追踪ID
    // 注意：这里简化处理，实际应该遍历所有线�?
    char* stored_id = pthread_getspecific(g_logging_state.trace_id_key);
    if (stored_id) {
        AGENTOS_FREE(stored_id);
        pthread_setspecific(g_logging_state.trace_id_key, NULL);
    }
    
    // 清理线程局部存�?
    pthread_key_delete(g_logging_state.trace_id_key);
    
    // 销毁互斥锁
    pthread_mutex_unlock(&g_logging_state.mutex);
    pthread_mutex_destroy(&g_logging_state.mutex);
    
    // 重置状�?
    memset(&g_logging_state, 0, sizeof(g_logging_state));
    
    LOG_INFO("日志系统清理完成");
}
