/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file logging.h
 * @brief 统一分层日志系统核心层API
 * 
 * @details
 * 本模块提供统一的分层日志系统核心层接口，支持：
 * - 5级日志级别：DEBUG, INFO, WARN, ERROR, FATAL
 * - 多输出目标：控制台、文件、网络、Syslog、内存缓冲
 * - 多格式输出：文本、JSON、结构化、二进制
 * - 线程安全的追踪ID管理
 * - 运行时配置热重载
 * 
 * 架构层次：
 * 1. 核心层（本文件）：定义接口和数据类型的平台无关抽象
 * 2. 原子层：提供高性能、线程安全的日志写入实现
 * 3. 服务层：提供高级功能（轮转、过滤、传输、监控）
 * 
 * @author Spharx AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-2 可观测性原则
 * @see logging_format.md 日志格式规范
 */

/**
 * @details
 * 使用示例：
 * @code
 * // 初始化日志系统
 * log_config_t manager = {
 *     .level = LOG_LEVEL_INFO,
 *     .output = LOG_OUTPUT_CONSOLE,
 *     .format = LOG_FORMAT_JSON
 * };
 * log_init(&manager);
 * 
 * // 记录日志
 * LOG_INFO("系统启动成功，版本: %s", version);
 * LOG_ERROR("连接失败，错误码: %d", errno);
 * 
 * // 设置追踪ID
 * log_set_trace_id("req-123456");
 * 
 * // 清理日志系统
 * log_cleanup();
 * @endcode
 */

#ifndef AGENTOS_COMMON_LOGGING_H
#define AGENTOS_COMMON_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ==================== 日志级别定义 ==================== */

/**
 * @brief 日志级别枚举
 * 
 * 定义5级日志级别，遵循Syslog标准，支持细粒度日志控制。
 */
typedef enum {
    /** @brief 调试信息，用于开发调试，生产环境通常关闭 */
    LOG_LEVEL_DEBUG = 0,
    
    /** @brief 普通信息，记录系统正常运行状态 */
    LOG_LEVEL_INFO  = 1,
    
    /** @brief 警告信息，表示可能的问题但不会影响系统运行 */
    LOG_LEVEL_WARN  = 2,
    
    /** @brief 错误信息，表示功能错误但不影响系统继续运行 */
    LOG_LEVEL_ERROR = 3,
    
    /** @brief 致命错误，表示系统无法继续运行，通常会导致进程退出 */
    LOG_LEVEL_FATAL = 4,
    
    /** @brief 日志级别数量，用于数组边界检查 */
    LOG_LEVEL_COUNT = 5
} log_level_t;

/**
 * @brief 获取日志级别名称字符串
 * 
 * 将日志级别枚举转换为可读的字符串表示。
 * 
 * @param level 日志级别
 * @return 级别名称字符串，无效级别返回"UNKNOWN"
 */
const char* log_level_to_string(log_level_t level);

/**
 * @brief 从字符串解析日志级别
 * 
 * 将字符串（如"DEBUG"、"INFO"）转换为日志级别枚举。
 * 
 * @param str 日志级别字符串（不区分大小写）
 * @return 对应的日志级别，解析失败返回LOG_LEVEL_INFO
 */
log_level_t log_level_from_string(const char* str);

/* ==================== 输出目标定义 ==================== */

/**
 * @brief 日志输出目标枚举
 * 
 * 定义日志可以输出的目标位置，支持多目标同时输出。
 */
typedef enum {
    /** @brief 控制台输出（stdout/stderr） */
    LOG_OUTPUT_CONSOLE = 0,
    
    /** @brief 文件输出，支持轮转和压缩 */
    LOG_OUTPUT_FILE    = 1,
    
    /** @brief Syslog输出，遵循Syslog协议 */
    LOG_OUTPUT_SYSLOG  = 2,
    
    /** @brief 网络输出，支持TCP/UDP传输 */
    LOG_OUTPUT_NETWORK = 3,
    
    /** @brief 内存缓冲输出，用于调试和测试 */
    LOG_OUTPUT_BUFFER  = 4,
    
    /** @brief 输出目标数量，用于数组边界检查 */
    LOG_OUTPUT_COUNT   = 5
} log_output_t;

/* ==================== 输出格式定义 ==================== */

/**
 * @brief 日志输出格式枚举
 * 
 * 定义日志的格式化方式，支持多种结构化格式。
 */
typedef enum {
    /** @brief 简单文本格式，人类可读，易于调试 */
    LOG_FORMAT_TEXT       = 0,
    
    /** @brief JSON格式，机器可读，支持结构化查询 */
    LOG_FORMAT_JSON       = 1,
    
    /** @brief 结构化格式，键值对分隔，兼顾可读性和机器解析 */
    LOG_FORMAT_STRUCTURED = 2,
    
    /** @brief 二进制格式，高性能，节省存储空间 */
    LOG_FORMAT_BINARY     = 3,
    
    /** @brief 输出格式数量，用于数组边界检查 */
    LOG_FORMAT_COUNT      = 4
} log_format_t;

/* ==================== 日志记录结构体 ==================== */

/**
 * @brief 日志记录结构体
 * 
 * 表示一条完整的日志记录，包含所有元数据和消息内容。
 * 用于在日志系统的不同层次之间传递。
 */
typedef struct {
    /** @brief 时间戳（Unix时间，毫秒精度） */
    uint64_t timestamp;
    
    /** @brief 日志级别 */
    log_level_t level;
    
    /** @brief 模块名称（来源文件或组件） */
    const char* module;
    
    /** @brief 源代码行号 */
    int line;
    
    /** @brief 线程追踪ID，用于请求跟踪 */
    const char* trace_id;
    
    /** @brief 日志消息内容 */
    const char* message;
    
    /** @brief 线程ID，标识产生日志的线程 */
    uint64_t thread_id;
    
    /** @brief 进程ID，标识产生日志的进程 */
    uint32_t process_id;
} log_record_t;

/* ==================== 配置结构体 ==================== */

/**
 * @brief 日志配置结构体
 * 
 * 配置日志系统的行为，支持运行时动态修改。
 */
typedef struct {
    /** @brief 全局日志级别，低于此级别的日志将被过滤 */
    log_level_t level;
    
    /** @brief 输出目标，支持多个目标（使用位掩码组合） */
    uint32_t outputs;
    
    /** @brief 输出格式 */
    log_format_t format;
    
    /** @brief 文件输出配置 - 文件路径（仅当outputs包含FILE时有效） */
    const char* file_path;
    
    /** @brief 文件输出配置 - 最大文件大小（字节），超过则轮转 */
    size_t max_file_size;
    
    /** @brief 文件输出配置 - 最大保留文件数 */
    int max_backup_count;
    
    /** @brief 网络输出配置 - 服务器地址（仅当outputs包含NETWORK时有效） */
    const char* network_host;
    
    /** @brief 网络输出配置 - 服务器端口 */
    uint16_t network_port;
    
    /** @brief Syslog配置 - 设施（facility） */
    int syslog_facility;
    
    /** @brief 异步模式开关，启用后日志写入不会阻塞调用线程 */
    bool async_mode;
    
    /** @brief 异步缓冲大小（字节），仅当async_mode为true时有效 */
    size_t async_buffer_size;
    
    /** @brief 是否启用性能统计 */
    bool enable_statistics;
} log_config_t;

/* ==================== 核心API函数 ==================== */

/**
 * @brief 初始化日志系统
 * 
 * 初始化日志系统，应用指定的配置。
 * 必须在调用任何其他日志函数之前调用（除log_set_default_config外）。
 * 
 * @param manager 日志配置，为NULL时使用默认配置
 * @return 0 成功，负值表示错误（错误码定义在log_error.h）
 */
int log_init(const log_config_t* manager);

/**
 * @brief 设置默认日志配置
 * 
 * 设置日志系统的默认配置，这些配置将在log_init()未提供配置时使用。
 * 可以在log_init()之前调用，用于预设配置。
 * 
 * @param manager 默认日志配置
 * @return 0 成功，负值表示错误
 */
int log_set_default_config(const log_config_t* manager);

/**
 * @brief 记录日志
 * 
 * 记录一条日志消息，这是日志系统的核心写入函数。
 * 根据配置，日志可能会被输出到多个目标。
 * 
 * @param level 日志级别
 * @param module 模块名称（通常使用__FILE__或组件名）
 * @param line 源代码行号（通常使用__LINE__）
 * @param fmt 格式化字符串，遵循printf格式
 * @param ... 格式化参数
 */
void log_write(log_level_t level, const char* module, int line, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 4, 5)))
#endif
;

/**
 * @brief 记录可变参数日志
 * 
 * log_write()的可变参数版本，用于需要传递va_list的场景。
 * 
 * @param level 日志级别
 * @param module 模块名称
 * @param line 源代码行号
 * @param fmt 格式化字符串
 * @param args 可变参数列表
 */
void log_write_va(log_level_t level, const char* module, int line, const char* fmt, va_list args);

/**
 * @brief 设置当前线程的追踪ID
 * 
 * 为当前线程设置追踪ID，所有后续日志都会包含此ID。
 * 用于分布式系统跟踪请求流程。
 * 
 * @param trace_id 追踪ID字符串，为NULL时自动生成
 * @return 实际设置的追踪ID（内部存储，无需释放）
 */
const char* log_set_trace_id(const char* trace_id);

/**
 * @brief 获取当前线程的追踪ID
 * 
 * 获取当前线程设置的追踪ID。
 * 
 * @return 追踪ID字符串，未设置返回NULL
 */
const char* log_get_trace_id(void);

/**
 * @brief 设置模块日志级别
 * 
 * 为特定模块设置独立的日志级别，覆盖全局级别。
 * 支持通配符匹配（如"network.*"匹配所有network模块）。
 * 
 * @param module_pattern 模块名称模式（支持通配符*）
 * @param level 日志级别
 * @return 0 成功，负值表示错误
 */
int log_set_module_level(const char* module_pattern, log_level_t level);

/**
 * @brief 重新加载日志配置
 * 
 * 从配置文件重新加载日志配置，支持热重载。
 * 
 * @param config_path 配置文件路径，为NULL时使用默认路径
 * @return 0 成功，负值表示错误
 */
int log_reload_config(const char* config_path);

/**
 * @brief 刷新日志缓冲
 * 
 * 强制刷新所有缓冲的日志到输出目标。
 * 在程序退出前调用以确保所有日志都被写入。
 */
void log_flush(void);

/**
 * @brief 清理日志系统
 * 
 * 清理日志系统资源，刷新所有缓冲的日志。
 * 程序退出前应该调用此函数。
 */
void log_cleanup(void);

/* ==================== 便捷日志宏 ==================== */

/**
 * @brief 调试级别日志宏
 * 
 * 记录DEBUG级别的日志，通常在开发调试时使用。
 */
#define LOG_DEBUG(fmt, ...) \
    log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 信息级别日志宏
 * 
 * 记录INFO级别的日志，用于记录系统正常运行状态。
 */
#define LOG_INFO(fmt, ...) \
    log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 警告级别日志宏
 * 
 * 记录WARN级别的日志，表示可能的问题但不影响系统运行。
 */
#define LOG_WARN(fmt, ...) \
    log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 错误级别日志宏
 * 
 * 记录ERROR级别的日志，表示功能错误但不导致系统崩溃。
 */
#define LOG_ERROR(fmt, ...) \
    log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 致命错误级别日志宏
 * 
 * 记录FATAL级别的日志，表示系统无法继续运行。
 * 记录日志后通常会调用abort()终止程序。
 */
#define LOG_FATAL(fmt, ...) \
    do { \
        log_write(LOG_LEVEL_FATAL, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        log_flush(); \
        abort(); \
    } while(0)

/**
 * @brief 条件日志宏
 * 
 * 仅当条件成立时记录日志，避免不必要的字符串格式化开销。
 */
#define LOG_IF(condition, level, fmt, ...) \
    do { \
        if (condition) { \
            log_write(level, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMMON_LOGGING_H */