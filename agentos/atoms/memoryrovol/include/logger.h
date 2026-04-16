/**
 * @file logger.h
 * @brief 日志系统接口定义（用于memoryrovol模块）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供统一的日志记录接口，支持不同日志级别和输出目标。
 * 此模块使用corekern模块的错误处理基础设施。
 */

#ifndef AGENTOS_LOGGER_H
#define AGENTOS_LOGGER_H

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 日志级别定义 ==================== */

/**
 * @brief 日志级别枚举
 */
#ifndef AGENTOS_LOG_LEVEL_T_DEFINED
#define AGENTOS_LOG_LEVEL_T_DEFINED
typedef enum agentos_log_level_e {
    AGENTOS_LOG_DEBUG = 0,    /**< 调试信息，仅开发阶段使用 */
    AGENTOS_LOG_INFO  = 1,    /**< 一般信息，正常运行状态 */
    AGENTOS_LOG_WARN  = 2,    /**< 警告信息，可能需要关注 */
    AGENTOS_LOG_ERROR = 3     /**< 错误信息，需要立即处理 */
} agentos_log_level_t;
#endif

/* ==================== 日志宏接口 ==================== */

/**
 * @brief 输出调试日志
 * @param fmt 格式字符串
 * @param ... 可变参数列表
 *
 * 仅在调试构建中输出，生产环境不产生任何代码。
 */
#ifndef NDEBUG
#define LOG_DEBUG(fmt, ...) \
    AGENTOS_LOG_DEBUG(fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

/**
 * @brief 输出信息日志
 * @param fmt 格式字符串
 * @param ... 可变参数列表
 */
#define LOG_INFO(fmt, ...) \
    AGENTOS_LOG_INFO(fmt, ##__VA_ARGS__)

/**
 * @brief 输出警告日志
 * @param fmt 格式字符串
 * @param ... 可变参数列表
 */
#define LOG_WARN(fmt, ...) \
    AGENTOS_LOG_WARN(fmt, ##__VA_ARGS__)

/**
 * @brief 输出错误日志
 * @param fmt 格式字符串
 * @param ... 可变参数列表
 */
#define LOG_ERROR(fmt, ...) \
    AGENTOS_LOG_ERROR(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LOGGER_H */