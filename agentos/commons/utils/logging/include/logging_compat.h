/**
 * @file logging_compat.h
 * @brief 统一分层日志系统向后兼容�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块提供统一分层日志系统与现有日志API的向后兼容层�? * 确保现有代码无需修改即可继续工作，同时可以逐步迁移到新API�? * 
 * 兼容性策略：
 * 1. **完全兼容**: 保持现有函数签名和宏定义不变
 * 2. **自动映射**: 将旧API调用映射到新架构实现
 * 3. **增量迁移**: 允许模块逐步迁移到新API
 * 4. **配置透明**: 兼容层使用与新架构相同的配置
 * 
 * 支持的现有API�? * 1. `agentos/atoms/utils/observability/include/logger.h` 中的API
 * 2. `agentos/commons/utils/observability/include/logger.h` 中的API
 * 3. `agentos/daemon/agentos/commons/include/svc_logger.h` 中的API（部分）
 * 
 * 使用方式�? * 1. 现有代码：无需修改，继续包含原头文�? * 2. 新代码：建议直接使用新API（logging.h�? * 3. 迁移代码：可逐步替换旧API调用为新API
 * 
 * 注意：兼容层会增加少量性能开销，建议生产代码最终迁移到新API�? */

#ifndef AGENTOS_COMMON_LOGGING_COMPAT_H
#define AGENTOS_COMMON_LOGGING_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "logging.h"

/* ==================== 迁移辅助结构体定义（必须在函数声明之前） ==================== */

typedef struct logging_compat_stats logging_compat_stats_t;

typedef struct migration_module_info {
    char module_name[64];
    char current_api[64];
    char target_api[64];
    int migration_status;
    float completion_percent;
} migration_module_info_t;

typedef struct migration_options {
    int dry_run;
    int backup_enabled;
    int verbose;
    char backup_path[256];
} migration_options_t;

typedef struct migration_validation_result {
    char module_name[64];
    int status;
    int errors;
    int warnings;
    char details[256];
} migration_validation_result_t;

/* ==================== 兼容层统计结构体 ==================== */

/**
 * @brief 现有日志级别宏定义
 * 
 * 使用条件编译保护，避免与logger.h中的定义冲突。
 * logger.h 中已定义 AGENTOS_LOG_LEVEL_ERROR 等宏（值为1/2/3/4），
 * 此处仅在未被定义时才定义。
 */
#ifndef AGENTOS_LOG_LEVEL_ERROR
#define AGENTOS_LOG_LEVEL_ERROR LOG_LEVEL_ERROR
#endif

#ifndef AGENTOS_LOG_LEVEL_WARN
#define AGENTOS_LOG_LEVEL_WARN  LOG_LEVEL_WARN
#endif

#ifndef AGENTOS_LOG_LEVEL_INFO
#define AGENTOS_LOG_LEVEL_INFO  LOG_LEVEL_INFO
#endif

#ifndef AGENTOS_LOG_LEVEL_DEBUG
#define AGENTOS_LOG_LEVEL_DEBUG LOG_LEVEL_DEBUG
#endif

#ifndef AGENTOS_LOG_LEVEL
#define AGENTOS_LOG_LEVEL LOG_LEVEL_INFO
#endif

/* ==================== 现有API函数声明 ==================== */

/**
 * @brief 设置当前线程的追踪ID（兼容版本）
 * 
 * 兼容现有`agentos_log_set_trace_id()`函数�? * 
 * @param trace_id 追踪ID，若为NULL则自动生�? * @return 实际设置的追踪ID（静态内存，无需释放�? */
const char* agentos_log_set_trace_id(const char* trace_id);

/**
 * @brief 获取当前线程的追踪ID（兼容版本）
 * 
 * 兼容现有`agentos_log_get_trace_id()`函数�? * 
 * @return 追踪ID，可能为NULL
 */
const char* agentos_log_get_trace_id(void);

/**
 * @brief 记录日志（兼容版本）
 * 
 * 兼容现有`agentos_log_write()`函数�? * 
 * @param level 日志级别
 * @param file 文件名（通常�?__FILE__�? * @param line 行号
 * @param fmt 格式字符�? * @param ... 参数
 */
void agentos_log_write(int level, const char* file, int line, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 4, 5)))
#endif
;

/**
 * @brief 记录可变参数日志（兼容版本）
 * 
 * 兼容现有`agentos_log_write_va()`函数（如果存在）�? * 
 * @param level 日志级别
 * @param file 文件�? * @param line 行号
 * @param fmt 格式字符�? * @param args 可变参数列表
 */
void agentos_log_write_va(int level, const char* file, int line, const char* fmt, va_list args);

/* ==================== 现有API宏定义兼�?==================== */

/**
 * @brief 错误级别日志宏（兼容版本�? * 
 * 兼容现有`AGENTOS_LOG_ERROR`宏�? */
#define AGENTOS_LOG_ERROR(fmt, ...) \
    agentos_log_write(AGENTOS_LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 警告级别日志宏（兼容版本�? * 
 * 兼容现有`AGENTOS_LOG_WARN`宏�? */
#define AGENTOS_LOG_WARN(fmt, ...) \
    agentos_log_write(AGENTOS_LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 信息级别日志宏（兼容版本�? * 
 * 兼容现有`AGENTOS_LOG_INFO`宏�? */
#define AGENTOS_LOG_INFO(fmt, ...) \
    agentos_log_write(AGENTOS_LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief 调试级别日志宏（兼容版本�? * 
 * 兼容现有`AGENTOS_LOG_DEBUG`宏�? * 注意：根据原common版本的行为，DEBUG日志可能被条件编译禁用�? */
#ifdef AGENTOS_DEBUG
#define AGENTOS_LOG_DEBUG(fmt, ...) \
    agentos_log_write(AGENTOS_LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define AGENTOS_LOG_DEBUG(fmt, ...) ((void)0)
#endif

/* ==================== 服务日志API兼容（部分） ==================== */

/**
 * @brief 服务日志初始化（兼容版本�? * 
 * 兼容现有服务日志初始化函数（如果存在）�? * 将服务日志配置映射到统一日志架构�? * 
 * @param manager 服务日志配置
 * @return 0 成功，非0 失败
 */
int svc_logger_init(const void* manager);

/**
 * @brief 服务日志清理（兼容版本）
 * 
 * 兼容现有服务日志清理函数（如果存在）�? */
void svc_logger_cleanup(void);

/**
 * @brief 设置服务日志级别（兼容版本）
 * 
 * 兼容现有服务日志级别设置函数（如果存在）�? * 
 * @param level 日志级别
 * @return 0 成功，非0 失败
 */
int svc_logger_set_level(int level);

/**
 * @brief 服务日志记录（兼容版本）
 * 
 * 兼容现有服务日志记录函数（如果存在）�? * 
 * @param level 日志级别
 * @param module 模块名称
 * @param fmt 格式字符�? * @param ... 参数
 */
void svc_logger_log(int level, const char* module, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
;

/* ==================== 兼容层配�?==================== */

/**
 * @brief 兼容层配置结构体
 * 
 * 配置兼容层的行为，优化兼容性和性能�? */
typedef struct {
    /** @brief 是否启用完全兼容模式，true时严格模拟旧行为 */
    bool strict_compatibility;
    
    /** @brief 是否启用性能优化，true时减少兼容层开销 */
    bool enable_perf_optimization;
    
    /** @brief 是否启用自动迁移检测，true时记录旧API使用情况 */
    bool enable_migration_detection;
    
    /** @brief 是否启用API映射日志，true时记录API调用映射 */
    bool enable_api_mapping_log;
    
    /** @brief 兼容层行为配�?*/
    struct {
        /** @brief 是否模拟旧的时间戳格�?*/
        bool emulate_old_timestamp;
        
        /** @brief 是否模拟旧的消息转义规则 */
        bool emulate_old_escaping;
        
        /** @brief 是否模拟旧的输出格式 */
        bool emulate_old_format;
        
        /** @brief 是否模拟旧的追踪ID生成算法 */
        bool emulate_old_trace_id;
    } behavior;
} logging_compat_config_t;

/**
 * @brief 初始化兼容层
 * 
 * 初始化兼容层内部状态，应用兼容配置�? * 通常在日志系统初始化时自动调用�? * 
 * @param manager 兼容层配置，为NULL时使用默认配�? * @return 0 成功，负值表示错�? */
int logging_compat_init(const logging_compat_config_t* manager);

/**
 * @brief 获取兼容层统计信息 * 
 * 获取兼容层的运行时统计信息，用于监控迁移进度�? * 
 * @param out_stats 输出参数，接收统计信�? * @return 0 成功，负值表示错�? */
int logging_compat_get_stats(logging_compat_stats_t* out_stats);

/**
 * @brief 获取待迁移API列表
 * 
 * 获取项目中仍在使用旧API的模块列表，用于迁移规划�? * 
 * @param out_modules 输出数组，接收模块信�? * @param max_modules 最大模块数�? * @return 实际返回的模块数量，负值表示错�? */
int logging_compat_get_migration_list(migration_module_info_t* out_modules, int max_modules);

/**
 * @brief 清理兼容�? * 
 * 清理兼容层资源�? */
void logging_compat_cleanup(void);

/* ==================== 迁移辅助工具 ==================== */

/**
 * @brief 迁移模块到新API
 * 
 * 辅助工具，帮助将指定模块从旧API迁移到新API�? * 
 * @param module_name 模块名称
 * @param options 迁移选项
 * @return 0 成功，负值表示错�? */
int logging_migrate_module(const char* module_name, const migration_options_t* options);

/**
 * @brief 生成迁移报告
 * 
 * 生成详细的迁移报告，包括兼容性分析、迁移建议等�? * 
 * @param report_path 报告文件路径
 * @return 0 成功，负值表示错�? */
int logging_generate_migration_report(const char* report_path);

/**
 * @brief 验证迁移结果
 * 
 * 验证模块迁移后的正确性，确保功能不变�? * 
 * @param module_name 模块名称
 * @return 验证结果结构体指针，失败返回NULL
 */
const migration_validation_result_t* logging_validate_migration(const char* module_name);

/* ==================== 兼容层统计结构体 ==================== */

/**
 * @brief 兼容层统计信息
 * 
 * 兼容层的运行时统计信息。
 */
struct logging_compat_stats {
    /** @brief 旧API调用次数统计 */
    struct {
        /** @brief agentos_log_write调用次数 */
        uint64_t agentos_log_write_calls;
        
        /** @brief agentos_log_set_trace_id调用次数 */
        uint64_t agentos_log_set_trace_id_calls;
        
        /** @brief agentos_log_get_trace_id调用次数 */
        uint64_t agentos_log_get_trace_id_calls;
        
        /** @brief AGENTOS_LOG_DEBUG宏使用次�?*/
        uint64_t agentos_log_debug_calls;
        
        /** @brief AGENTOS_LOG_INFO宏使用次�?*/
        uint64_t agentos_log_info_calls;
        
        /** @brief AGENTOS_LOG_WARN宏使用次�?*/
        uint64_t agentos_log_warn_calls;
        
        /** @brief AGENTOS_LOG_ERROR宏使用次�?*/
        uint64_t agentos_log_error_calls;
        
        /** @brief 服务日志API调用次数 */
        uint64_t svc_logger_calls;
    } api_calls;
    
    /** @brief 迁移进度统计 */
    struct {
        /** @brief 已迁移模块数�?*/
        int migrated_modules;
        
        /** @brief 待迁移模块数�?*/
        int pending_modules;
        
        /** @brief 迁移中模块数�?*/
        int migrating_modules;
        
        /** @brief 总模块数�?*/
        int total_modules;
    } migration_progress;
    
    /** @brief 性能统计 */
    struct {
        /** @brief 兼容层调用平均开销（纳秒） */
        uint64_t avg_overhead_ns;
        
        /** @brief 最大兼容层调用开销（纳秒） */
        uint64_t max_overhead_ns;
        
        /** @brief 内存使用量（字节�?*/
        size_t memory_usage;
        
        /** @brief 缓存命中率（百分比） */
        float cache_hit_rate;
    } performance;
    
    /** @brief 错误统计 */
    struct {
        /** @brief API映射错误次数 */
        uint64_t api_mapping_errors;
        
        /** @brief 参数转换错误次数 */
        uint64_t param_conversion_errors;
        
        /** @brief 兼容性错误次�?*/
        uint64_t compatibility_errors;
        
        /** @brief 迁移错误次数 */
        uint64_t migration_errors;
    } errors;
};

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMMON_LOGGING_COMPAT_H */