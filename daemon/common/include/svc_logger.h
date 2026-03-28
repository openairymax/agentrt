/**
 * @file svc_logger.h
 * @brief AgentOS 日志服务接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 设计原则：
 * 1. 统一日志格式：时间戳 + 级别 + 模块 + 消息
 * 2. 多级别支持：DEBUG/INFO/WARN/ERROR/FATAL
 * 3. 多输出目标：控制台/文件/syslog
 * 4. 线程安全
 */

#ifndef AGENTOS_SVC_LOGGER_H
#define AGENTOS_SVC_LOGGER_H

#include <stdint.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 日志级别 ==================== */

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_FATAL = 4
} log_level_t;

/* ==================== 日志输出目标 ==================== */

typedef enum {
    LOG_TARGET_CONSOLE = (1 << 0),
    LOG_TARGET_FILE     = (1 << 1),
    LOG_TARGET_SYSLOG  = (1 << 2),
    LOG_TARGET_JSON    = (1 << 3)
} log_target_t;

/* ==================== 日志配置 ==================== */

typedef struct {
    log_level_t min_level;
    int targets;
    const char* log_dir;
    const char* log_prefix;
    uint64_t max_file_size;
    int max_backup_files;
    int async_mode;
    int use_colors;
} logger_config_t;

/* ==================== 初始化与清理 ==================== */

/**
 * @brief 初始化日志系统
 * @param manager 日志配置（可为 NULL 使用默认配置）
 * @return 0 成功，非0 失败
 */
int svc_logger_init(const logger_config_t* manager);

/**
 * @brief 关闭日志系统
 */
void svc_logger_shutdown(void);

/**
 * @brief 刷新日志缓冲区
 */
void svc_logger_flush(void);

/* ==================== 日志记录接口 ==================== */

/**
 * @brief 设置日志级别
 * @param level 最小日志级别
 */
void svc_logger_set_level(log_level_t level);

/**
 * @brief 设置日志模块
 * @param module 模块名称（如 "llm_d", "tool_d"）
 * @param level 该模块的日志级别
 */
void svc_logger_set_module_level(const char* module, log_level_t level);

/**
 * @brief 获取当前日志级别
 * @return 当前最小日志级别
 */
log_level_t svc_logger_get_level(void);

/* ==================== 日志宏 ==================== */

/**
 * @brief 获取模块名
 */
#define SVC_LOG_MODULE svc_logger_get_module()

/* DEBUG 级别日志 */
#define SVC_LOG_DEBUG(fmt, ...) \
    svc_logger_log_impl(LOG_LEVEL_DEBUG, SVC_LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* INFO 级别日志 */
#define SVC_LOG_INFO(fmt, ...) \
    svc_logger_log_impl(LOG_LEVEL_INFO, SVC_LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* WARN 级别日志 */
#define SVC_LOG_WARN(fmt, ...) \
    svc_logger_log_impl(LOG_LEVEL_WARN, SVC_LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* ERROR 级别日志 */
#define SVC_LOG_ERROR(fmt, ...) \
    svc_logger_log_impl(LOG_LEVEL_ERROR, SVC_LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/* FATAL 级别日志 */
#define SVC_LOG_FATAL(fmt, ...) \
    svc_logger_log_impl(LOG_LEVEL_FATAL, SVC_LOG_MODULE, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 可变参数日志记录实现
 * @param level 日志级别
 * @param module 模块名
 * @param file 源文件
 * @param line 行号
 * @param fmt 格式字符串
 * @param ... 可变参数
 */
void svc_logger_log_impl(log_level_t level,
                         const char* module,
                         const char* file,
                         int line,
                         const char* fmt,
                         ...);

/**
 * @brief 获取当前模块名（线程本地）
 */
const char* svc_logger_get_module(void);

/**
 * @brief 设置当前模块名（线程本地）
 * @param module 模块名
 */
void svc_logger_set_module(const char* module);

/* ==================== 条件日志 ==================== */

/**
 * @brief 调试模式下才输出的日志
 */
#ifdef DEBUG
    #define SVC_LOG_DEBUG_IF(cond, fmt, ...) \
        do { if (cond) SVC_LOG_DEBUG(fmt, ##__VA_ARGS__); } while(0)
#else
    #define SVC_LOG_DEBUG_IF(cond, fmt, ...) ((void)0)
#endif

/**
 * @brief 性能日志（INFO 级别）
 */
#define SVC_LOG_PERF(fmt, ...) SVC_LOG_INFO("[PERF] " fmt, ##__VA_ARGS__)

/**
 * @brief 安全日志（WARN 级别）
 */
#define SVC_LOG_SECURITY(fmt, ...) SVC_LOG_WARN("[SECURITY] " fmt, ##__VA_ARGS__)

/* ==================== 追踪接口 ==================== */

/**
 * @brief 获取当前追踪 ID
 * @return 追踪 ID（64 位十六进制字符串）
 */
const char* svc_logger_get_trace_id(void);

/**
 * @brief 设置当前追踪 ID
 * @param trace_id 追踪 ID
 */
void svc_logger_set_trace_id(const char* trace_id);

/**
 * @brief 生成新的追踪 ID
 * @return 新追踪 ID（调用者需释放）
 */
char* svc_logger_gen_trace_id(void);

/* ==================== 统计接口 ==================== */

/**
 * @brief 日志统计信息
 */
typedef struct {
    uint64_t total_messages;
    uint64_t by_level[5];     /* 按级别统计 */
    uint64_t by_module[16];   /* 按模块统计（假设最多 16 个模块） */
    uint64_t dropped_messages;
} logger_stats_t;

/**
 * @brief 获取日志统计
 * @param stats 输出统计信息
 */
void svc_logger_get_stats(logger_stats_t* stats);

/**
 * @brief 重置日志统计
 */
void svc_logger_reset_stats(void);

/* ==================== 高级接口 ==================== */

/**
 * @brief 日志回调函数类型
 */
typedef void (*log_callback_t)(log_level_t level,
                               const char* module,
                               const char* file,
                               int line,
                               const char* message,
                               void* user_data);

/**
 * @brief 注册日志回调
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
int svc_logger_register_callback(log_callback_t callback, void* user_data);

/**
 * @brief 注销日志回调
 * @param callback 回调函数
 */
void svc_logger_unregister_callback(log_callback_t callback);

/* ==================== 便捷函数 ==================== */

/**
 * @brief 将日志级别转换为字符串
 * @param level 日志级别
 * @return 级别字符串
 */
const char* svc_logger_level_to_string(log_level_t level);

/**
 * @brief 将字符串转换为日志级别
 * @param str 字符串
 * @return 日志级别，失败返回 -1
 */
int svc_logger_string_to_level(const char* str);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SVC_LOGGER_H */
