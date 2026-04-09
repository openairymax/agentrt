#include "include/logging_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

/**
 * @brief 全局日志配置
 */
static log_config_t g_log_config = {
    .min_level = LOG_LEVEL_INFO,
    .targets = LOG_TARGET_CONSOLE,
    .log_file = NULL,
    .max_file_size = 10 * 1024 * 1024, // 10MB
    .max_files = 5,
    .use_colors = true,
    .include_timestamp = true,
    .include_thread_id = false
};

/**
 * @brief 日志文件指针
 */
static FILE* g_log_file = NULL;

/**
 * @brief 日志级别名称
 */
static const char* g_log_level_names[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL"
};

/**
 * @brief 日志级别颜色
 */
static const char* g_log_level_colors[] = {
    "\033[36m", // DEBUG - Cyan
    "\033[32m", // INFO - Green
    "\033[33m", // WARNING - Yellow
    "\033[31m", // ERROR - Red
    "\033[35m"  // FATAL - Magenta
};

/**
 * @brief 重置颜色
 */
static const char* g_log_color_reset = "\033[0m";

/**
 * @brief 创建默认日志配置
 * @return 默认日志配置
 */
log_config_t log_create_default_config(void) {
    return g_log_config;
}

/**
 * @brief 初始化日志系统
 * @param manager 日志配置
 * @return 错误码
 */
agentos_error_t log_init(const log_config_t* manager) {
    if (manager) {
        g_log_config = *manager;
    }
    
    // 打开日志文件
    if (g_log_config.targets & LOG_TARGET_FILE && g_log_config.log_file) {
        g_log_file = fopen(g_log_config.log_file, "a");
        if (!g_log_file) {
            return AGENTOS_EIO;
        }
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理日志系统
 */
void log_cleanup(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

/**
 * @brief 设置日志配置
 * @param manager 日志配置
 * @return 错误码
 */
agentos_error_t log_set_config(const log_config_t* manager) {
    if (!manager) {
        return AGENTOS_EINVAL;
    }
    
    g_log_config = *manager;
    
    // 重新打开日志文件
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
    
    if (g_log_config.targets & LOG_TARGET_FILE && g_log_config.log_file) {
        g_log_file = fopen(g_log_config.log_file, "a");
        if (!g_log_file) {
            return AGENTOS_EIO;
        }
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取当前日志配置
 * @param manager 日志配置输出
 */
void log_get_config(log_config_t* manager) {
    if (manager) {
        *manager = g_log_config;
    }
}

/**
 * @brief 格式化日志时间戳
 * @param buffer 缓冲区
 * @param size 缓冲区大小
 */
static void log_format_timestamp(char* buffer, size_t size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * @brief 输出日志
 * @param level 日志级别
 * @param context 日志上下文
 * @param format 日志格式
 * @param args 可变参数
 */
static void log_output(log_level_t level, const log_context_t* context, const char* format, va_list args) {
    if (level < g_log_config.min_level) {
        return;
    }
    
    char timestamp[32] = {0};
    if (g_log_config.include_timestamp) {
        log_format_timestamp(timestamp, sizeof(timestamp));
    }
    
    // 构建日志前缀
    char prefix[256] = {0};
    if (g_log_config.include_timestamp) {
        snprintf(prefix, sizeof(prefix), "[%s] ", timestamp);
    }
    
    // 添加日志级别
    if (g_log_config.use_colors && (g_log_config.targets & LOG_TARGET_CONSOLE)) {
        snprintf(prefix + strlen(prefix), sizeof(prefix) - strlen(prefix), 
                 "%s[%s]%s ", g_log_level_colors[level], g_log_level_names[level], g_log_color_reset);
    } else {
        snprintf(prefix + strlen(prefix), sizeof(prefix) - strlen(prefix), 
                 "[%s] ", g_log_level_names[level]);
    }
    
    // 添加模块信息
    if (context && context->module) {
        snprintf(prefix + strlen(prefix), sizeof(prefix) - strlen(prefix), 
                 "[%s] ", context->module);
    }
    
    // 添加函数和行号
    if (context && context->function) {
        snprintf(prefix + strlen(prefix), sizeof(prefix) - strlen(prefix), 
                 "%s:%d: ", context->function, context->line);
    }
    
    // 输出到控制台
    if (g_log_config.targets & LOG_TARGET_CONSOLE) {
        fprintf(stdout, "%s", prefix);
        vfprintf(stdout, format, args);
        fprintf(stdout, "\n");
        fflush(stdout);
    }
    
    // 输出到文件
    if (g_log_config.targets & LOG_TARGET_FILE && g_log_file) {
        // 文件输出不使用颜色
        char file_prefix[256] = {0};
        if (g_log_config.include_timestamp) {
            snprintf(file_prefix, sizeof(file_prefix), "[%s] ", timestamp);
        }
        snprintf(file_prefix + strlen(file_prefix), sizeof(file_prefix) - strlen(file_prefix), 
                 "[%s] ", g_log_level_names[level]);
        if (context && context->module) {
            snprintf(file_prefix + strlen(file_prefix), sizeof(file_prefix) - strlen(file_prefix), 
                     "[%s] ", context->module);
        }
        if (context && context->function) {
            snprintf(file_prefix + strlen(file_prefix), sizeof(file_prefix) - strlen(file_prefix), 
                     "%s:%d: ", context->function, context->line);
        }
        fprintf(g_log_file, "%s", file_prefix);
        vfprintf(g_log_file, format, args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
    }
}

/**
 * @brief 输出调试日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_debug(const log_context_t* context, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_output(LOG_LEVEL_DEBUG, context, format, args);
    va_end(args);
}

/**
 * @brief 输出信息日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_info(const log_context_t* context, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_output(LOG_LEVEL_INFO, context, format, args);
    va_end(args);
}

/**
 * @brief 输出警告日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_warning(const log_context_t* context, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_output(LOG_LEVEL_WARNING, context, format, args);
    va_end(args);
}

/**
 * @brief 输出错误日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_error(const log_context_t* context, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_output(LOG_LEVEL_ERROR, context, format, args);
    va_end(args);
}

/**
 * @brief 输出致命日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_fatal(const log_context_t* context, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_output(LOG_LEVEL_FATAL, context, format, args);
    va_end(args);
}

/**
 * @brief 检查日志级别是否启用
 * @param level 日志级别
 * @return 是否启用
 */
bool log_is_enabled(log_level_t level) {
    return level >= g_log_config.min_level;
}

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void log_set_level(log_level_t level) {
    g_log_config.min_level = level;
}

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
log_level_t log_get_level(void) {
    return g_log_config.min_level;
}

/**
 * @brief 刷新日志缓冲区
 */
void log_flush(void) {
    if (g_log_config.targets & LOG_TARGET_CONSOLE) {
        fflush(stdout);
    }
    if (g_log_config.targets & LOG_TARGET_FILE && g_log_file) {
        fflush(g_log_file);
    }
}
