#ifndef LOGGING_COMMON_H
#define LOGGING_COMMON_H

#include "../../../../corekern/include/error.h"

/**
 * @brief 日志级别枚举
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

/**
 * @brief 日志输出目标
 */
typedef enum {
    LOG_TARGET_CONSOLE = 1 << 0,
    LOG_TARGET_FILE = 1 << 1,
    LOG_TARGET_SYSLOG = 1 << 2
} log_target_t;

/**
 * @brief 日志配置结构体
 */
typedef struct {
    log_level_t min_level;
    log_target_t targets;
    const char* log_file;
    size_t max_file_size;
    int max_files;
    bool use_colors;
    bool include_timestamp;
    bool include_thread_id;
} log_config_t;

/**
 * @brief 日志上下文结构体
 */
typedef struct {
    const char* module;
    const char* function;
    int line;
} log_context_t;

/**
 * @brief 创建默认日志配置
 * @return 默认日志配置
 */
log_config_t log_create_default_config(void);

/**
 * @brief 初始化日志系统
 * @param manager 日志配置
 * @return 错误码
 */
agentos_error_t log_init(const log_config_t* manager);

/**
 * @brief 清理日志系统
 */
void log_cleanup(void);

/**
 * @brief 设置日志配置
 * @param manager 日志配置
 * @return 错误码
 */
agentos_error_t log_set_config(const log_config_t* manager);

/**
 * @brief 获取当前日志配置
 * @param manager 日志配置输出
 */
void log_get_config(log_config_t* manager);

/**
 * @brief 输出调试日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_debug(const log_context_t* context, const char* format, ...);

/**
 * @brief 输出信息日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_info(const log_context_t* context, const char* format, ...);

/**
 * @brief 输出警告日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_warning(const log_context_t* context, const char* format, ...);

/**
 * @brief 输出错误日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_error(const log_context_t* context, const char* format, ...);

/**
 * @brief 输出致命日志
 * @param context 日志上下文
 * @param format 日志格式
 * @param ... 可变参数
 */
void log_fatal(const log_context_t* context, const char* format, ...);

/**
 * @brief 检查日志级别是否启用
 * @param level 日志级别
 * @return 是否启用
 */
bool log_is_enabled(log_level_t level);

/**
 * @brief 设置日志级别
 * @param level 日志级别
 */
void log_set_level(log_level_t level);

/**
 * @brief 获取当前日志级别
 * @return 当前日志级别
 */
log_level_t log_get_level(void);

/**
 * @brief 刷新日志缓冲区
 */
void log_flush(void);

/**
 * @brief 日志上下文宏
 */
#define LOG_CONTEXT(module) (log_context_t){module, __FUNCTION__, __LINE__}

/**
 * @brief 日志宏
 */
#define LOG_DEBUG(module, format, ...) log_debug(&LOG_CONTEXT(module), format, ##__VA_ARGS__)
#define LOG_INFO(module, format, ...) log_info(&LOG_CONTEXT(module), format, ##__VA_ARGS__)
#define LOG_WARNING(module, format, ...) log_warning(&LOG_CONTEXT(module), format, ##__VA_ARGS__)
#define LOG_ERROR(module, format, ...) log_error(&LOG_CONTEXT(module), format, ##__VA_ARGS__)
#define LOG_FATAL(module, format, ...) log_fatal(&LOG_CONTEXT(module), format, ##__VA_ARGS__)

#endif // LOGGING_COMMON_H