/**
 * @file service_logging.h
 * @brief 统一分层日志系统服务层API
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块提供统一的分层日志系统服务层接口，提供高级功能：
 * - 日志轮转：基于大小/时间自动轮转，支持压缩和归档
 * - 日志过滤：级别过滤、关键字过滤、正则表达式过滤
 * - 日志传输：网络传输（TCP/UDP）、Syslog集成、远程收集
 * - 监控统计：吞吐量统计、延迟监控、错误率告警
 * - 管理接口：热重载、运行时调整、查询检索
 * 
 * 服务层设计原则：
 * 1. **功能丰富**：提供生产环境所需的所有高级日志功能
 * 2. **配置灵活**：支持多种配置方式和运行时调整
 * 3. **可扩展性**：插件化架构，支持自定义输出器和过滤器
 * 4. **监控完备**：全面的性能指标和健康状态监控
 * 
 * 架构角色：
 * - 从原子层获取日志记录
 * - 应用过滤规则和格式化
 * - 将日志输出到多个目标（文件、网络、Syslog等）
 * - 提供管理接口和监控数据
 * 
 * 注意：服务层是可选的，简单应用可以只使用核心层和原子层。
 * 服务层功能可通过条件编译禁用，以减少资源消耗。
 */

#ifndef AGENTOS_COMMON_SERVICE_LOGGING_H
#define AGENTOS_COMMON_SERVICE_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "logging.h"
#include "atomic_logging.h"
#include <stdbool.h>

/* ==================== 服务层配置 ==================== */

/**
 * @brief 服务层配置结构体
 * 
 * 配置服务层的各项高级功能。
 */
typedef struct {
    /** @brief 是否启用日志轮转功能 */
    bool enable_rotation;
    
    /** @brief 是否启用日志过滤功能 */
    bool enable_filtering;
    
    /** @brief 是否启用日志传输功能 */
    bool enable_transport;
    
    /** @brief 是否启用监控统计功能 */
    bool enable_monitoring;
    
    /** @brief 是否启用管理接口 */
    bool enable_management;
    
    /** @brief 服务层工作线程数量 */
    int worker_threads;
    
    /** @brief 最大并发输出器数量 */
    int max_outputters;
    
    /** @brief 最大并发过滤器数量 */
    int max_filters;
    
    /** @brief 配置热重载检查间隔（秒） */
    int config_reload_interval;
} service_logging_config_t;

/* ==================== 日志轮转配置 ==================== */

/**
 * @brief 日志轮转策略
 * 
 * 定义何时触发日志轮转。
 */
typedef enum {
    /** @brief 基于文件大小轮转，超过指定大小时触发 */
    ROTATION_STRATEGY_SIZE,
    
    /** @brief 基于时间轮转，每天/每周/每月触发 */
    ROTATION_STRATEGY_TIME,
    
    /** @brief 基于大小和时间组合轮转，任一条件满足即触发 */
    ROTATION_STRATEGY_BOTH
} rotation_strategy_t;

/**
 * @brief 时间轮转间隔
 * 
 * 定义时间轮转的间隔单位。
 */
typedef enum {
    /** @brief 每小时轮转 */
    ROTATION_INTERVAL_HOURLY,
    
    /** @brief 每天轮转 */
    ROTATION_INTERVAL_DAILY,
    
    /** @brief 每周轮转 */
    ROTATION_INTERVAL_WEEKLY,
    
    /** @brief 每月轮转 */
    ROTATION_INTERVAL_MONTHLY
} rotation_interval_t;

/**
 * @brief 日志轮转配置
 * 
 * 配置日志轮转的行为。
 */
typedef struct {
    /** @brief 轮转策略 */
    rotation_strategy_t strategy;
    
    /** @brief 最大文件大小（字节），仅当策略包含SIZE时有效 */
    size_t max_file_size;
    
    /** @brief 时间轮转间隔，仅当策略包含TIME时有效 */
    rotation_interval_t time_interval;
    
    /** @brief 最大保留文件数，超过此数量的旧文件将被删除 */
    int max_backup_files;
    
    /** @brief 是否压缩轮转后的文件 */
    bool compress_backups;
    
    /** @brief 压缩算法（如果启用压缩） */
    const char* compression_algorithm;
    
    /** @brief 轮转时的时间格式，用于生成文件名 */
    const char* time_format;
    
    /** @brief 轮转文件命名模式，支持占位符 */
    const char* filename_pattern;
} log_rotation_config_t;

/* ==================== 日志过滤配置 ==================== */

/**
 * @brief 过滤条件类型
 * 
 * 定义过滤条件的匹配类型。
 */
typedef enum {
    /** @brief 精确匹配 */
    FILTER_MATCH_EXACT,
    
    /** @brief 前缀匹配 */
    FILTER_MATCH_PREFIX,
    
    /** @brief 后缀匹配 */
    FILTER_MATCH_SUFFIX,
    
    /** @brief 包含匹配 */
    FILTER_MATCH_CONTAINS,
    
    /** @brief 正则表达式匹配 */
    FILTER_MATCH_REGEX,
    
    /** @brief 通配符匹配 */
    FILTER_MATCH_WILDCARD
} filter_match_type_t;

/**
 * @brief 过滤条件
 * 
 * 定义单个过滤条件。
 */
typedef struct {
    /** @brief 匹配类型 */
    filter_match_type_t match_type;
    
    /** @brief 匹配目标字段 */
    const char* field;
    
    /** @brief 匹配模式 */
    const char* pattern;
    
    /** @brief 是否取反（不匹配时通过） */
    bool negate;
    
    /** @brief 是否区分大小写 */
    bool case_sensitive;
} filter_condition_t;

/**
 * @brief 过滤规则
 * 
 * 定义完整的过滤规则，包含多个条件和逻辑关系。
 */
typedef struct {
    /** @brief 规则名称 */
    const char* name;
    
    /** @brief 条件数组 */
    filter_condition_t* conditions;
    
    /** @brief 条件数量 */
    size_t condition_count;
    
    /** @brief 逻辑关系：true表示AND，false表示OR */
    bool logical_and;
    
    /** @brief 匹配时的动作：true表示保留，false表示丢弃 */
    bool action_keep;
} filter_rule_t;

/**
 * @brief 过滤配置
 * 
 * 配置日志过滤的行为。
 */
typedef struct {
    /** @brief 规则数组 */
    filter_rule_t* rules;
    
    /** @brief 规则数量 */
    size_t rule_count;
    
    /** @brief 默认动作：true表示保留，false表示丢弃 */
    bool default_action;
    
    /** @brief 是否启用过滤缓存 */
    bool enable_cache;
    
    /** @brief 过滤缓存大小（条目数） */
    size_t cache_size;
} log_filter_config_t;

/* ==================== 日志传输配置 ==================== */

/**
 * @brief 传输协议
 * 
 * 定义日志传输的网络协议。
 */
typedef enum {
    /** @brief TCP协议，可靠连接 */
    TRANSPORT_PROTOCOL_TCP,
    
    /** @brief UDP协议，无连接 */
    TRANSPORT_PROTOCOL_UDP,
    
    /** @brief TLS协议，加密传输 */
    TRANSPORT_PROTOCOL_TLS,
    
    /** @brief WebSocket协议 */
    TRANSPORT_PROTOCOL_WS,
    
    /** @brief Syslog协议 */
    TRANSPORT_PROTOCOL_SYSLOG
} transport_protocol_t;

/**
 * @brief 传输配置
 * 
 * 配置日志传输的行为。
 */
typedef struct {
    /** @brief 协议类型 */
    transport_protocol_t protocol;
    
    /** @brief 目标主机地址 */
    const char* host;
    
    /** @brief 目标端口 */
    uint16_t port;
    
    /** @brief 连接超时时间（毫秒） */
    int connect_timeout;
    
    /** @brief 发送超时时间（毫秒） */
    int send_timeout;
    
    /** @brief 最大重试次数 */
    int max_retries;
    
    /** @brief 重试间隔（毫秒） */
    int retry_interval;
    
    /** @brief 批量发送大小（记录数） */
    size_t batch_size;
    
    /** @brief 批量发送超时时间（毫秒） */
    int batch_timeout;
    
    /** @brief 缓冲区大小（字节） */
    size_t buffer_size;
    
    /** @brief TLS配置（仅当协议为TLS时有效） */
    struct {
        const char* ca_cert_path;
        const char* client_cert_path;
        const char* client_key_path;
        bool verify_peer;
    } tls_config;
} log_transport_config_t;

/* ==================== 监控统计配置 ==================== */

/**
 * @brief 监控配置
 * 
 * 配置日志系统监控的行为。
 */
typedef struct {
    /** @brief 是否启用吞吐量统计 */
    bool enable_throughput;
    
    /** @brief 是否启用延迟监控 */
    bool enable_latency;
    
    /** @brief 是否启用错误率监控 */
    bool enable_error_rate;
    
    /** @brief 统计采样间隔（秒） */
    int sampling_interval;
    
    /** @brief 历史数据保留时间（分钟） */
    int history_retention;
    
    /** @brief 告警阈值配置 */
    struct {
        /** @brief 最大延迟告警阈值（毫秒） */
        uint32_t max_latency_ms;
        
        /** @brief 最小吞吐量告警阈值（记录/秒） */
        uint32_t min_throughput_rps;
        
        /** @brief 最大错误率告警阈值（百分比） */
        float max_error_rate_percent;
    } alert_thresholds;
    
    /** @brief 监控数据导出配置 */
    struct {
        /** @brief 是否导出到文件 */
        bool export_to_file;
        
        /** @brief 是否导出到网络 */
        bool export_to_network;
        
        /** @brief 导出格式（JSON、Prometheus等） */
        const char* export_format;
        
        /** @brief 导出间隔（秒） */
        int export_interval;
    } export_config;
} log_monitoring_config_t;

/* ==================== 服务层API函数 ==================== */

/**
 * @brief 初始化服务层
 * 
 * 初始化服务层内部组件，启动工作线程。
 * 必须在调用任何服务层函数之前调用。
 * 
 * @param manager 服务层配置，为NULL时使用默认配置
 * @return 0 成功，负值表示错误
 */
int service_logging_init(const service_logging_config_t* manager);

/**
 * @brief 配置日志轮转
 * 
 * 配置日志文件的轮转行为。
 * 
 * @param manager 轮转配置
 * @return 0 成功，负值表示错误
 */
int service_logging_configure_rotation(const log_rotation_config_t* manager);

/**
 * @brief 配置日志过滤
 * 
 * 配置日志记录的过滤规则。
 * 
 * @param manager 过滤配置
 * @return 0 成功，负值表示错误
 */
int service_logging_configure_filtering(const log_filter_config_t* manager);

/**
 * @brief 配置日志传输
 * 
 * 配置日志的远程传输。
 * 
 * @param manager 传输配置
 * @return 0 成功，负值表示错误
 */
int service_logging_configure_transport(const log_transport_config_t* manager);

/**
 * @brief 配置监控统计
 * 
 * 配置日志系统的监控和统计。
 * 
 * @param manager 监控配置
 * @return 0 成功，负值表示错误
 */
int service_logging_configure_monitoring(const log_monitoring_config_t* manager);

/**
 * @brief 启动日志轮转
 * 
 * 手动触发日志轮转，立即轮转当前日志文件。
 * 
 * @param reason 轮转原因（用于日志记录）
 * @return 0 成功，负值表示错误
 */
int service_logging_rotate_now(const char* reason);

/**
 * @brief 添加过滤规则
 * 
 * 动态添加过滤规则，支持运行时更新。
 * 
 * @param rule 过滤规则
 * @return 规则ID，负值表示错误
 */
int service_logging_add_filter_rule(const filter_rule_t* rule);

/**
 * @brief 移除过滤规则
 * 
 * 动态移除过滤规则。
 * 
 * @param rule_id 规则ID
 * @return 0 成功，负值表示错误
 */
int service_logging_remove_filter_rule(int rule_id);

/**
 * @brief 更新过滤规则
 * 
 * 动态更新过滤规则。
 * 
 * @param rule_id 规则ID
 * @param rule 新的过滤规则
 * @return 0 成功，负值表示错误
 */
int service_logging_update_filter_rule(int rule_id, const filter_rule_t* rule);

/**
 * @brief 测试过滤规则
 * 
 * 测试过滤规则是否匹配指定的日志记录。
 * 
 * @param rule_id 规则ID
 * @param record 日志记录
 * @return true 匹配，false 不匹配
 */
bool service_logging_test_filter_rule(int rule_id, const log_record_t* record);

/**
 * @brief 发送日志到远程
 * 
 * 立即发送缓冲的日志到远程服务器。
 * 
 * @return 成功发送的记录数，负值表示错误
 */
int service_logging_flush_transport(void);

/**
 * @brief 获取监控统计
 * 
 * 获取日志系统的当前监控统计信息。
 * 
 * @param out_stats 输出参数，接收统计信息
 * @return 0 成功，负值表示错误
 */
int service_logging_get_monitoring_stats(struct log_monitoring_stats* out_stats);

/**
 * @brief 重置监控统计
 * 
 * 重置所有监控统计计数器。
 * 
 * @return 0 成功，负值表示错误
 */
int service_logging_reset_monitoring_stats(void);

/**
 * @brief 查询日志
 * 
 * 查询符合条件的日志记录（需要启用日志存储功能）。
 * 
 * @param query 查询条件
 * @param out_records 输出数组，接收查询结果
 * @param max_records 最大返回记录数
 * @param timeout_ms 查询超时时间（毫秒）
 * @return 实际返回的记录数，负值表示错误
 */
int service_logging_query(const struct log_query* query, log_record_t* out_records, 
                         size_t max_records, int timeout_ms);

/**
 * @brief 重新加载服务层配置
 * 
 * 从配置文件重新加载服务层配置。
 * 
 * @param config_path 配置文件路径
 * @return 0 成功，负值表示错误
 */
int service_logging_reload_config(const char* config_path);

/**
 * @brief 清理服务层
 * 
 * 清理服务层资源，停止工作线程。
 * 必须在程序退出前调用。
 */
void service_logging_cleanup(void);

/* ==================== 监控统计结构体 ==================== */

/**
 * @brief 日志监控统计信息
 * 
 * 日志系统的运行时监控统计信息。
 */
typedef struct log_monitoring_stats {
    /** @brief 吞吐量统计 */
    struct {
        /** @brief 当前吞吐量（记录/秒） */
        uint32_t current_rps;
        
        /** @brief 平均吞吐量（记录/秒） */
        uint32_t avg_rps;
        
        /** @brief 最大吞吐量（记录/秒） */
        uint32_t max_rps;
        
        /** @brief 总处理记录数 */
        uint64_t total_records;
        
        /** @brief 处理速率（字节/秒） */
        uint64_t bytes_per_second;
    } throughput;
    
    /** @brief 延迟统计 */
    struct {
        /** @brief 当前延迟（毫秒） */
        uint32_t current_latency_ms;
        
        /** @brief 平均延迟（毫秒） */
        uint32_t avg_latency_ms;
        
        /** @brief 最大延迟（毫秒） */
        uint32_t max_latency_ms;
        
        /** @brief 延迟分布直方图 */
        uint32_t latency_histogram[10];  /* 0-10ms, 10-20ms, ..., 90-100ms, 100+ms */
        
        /** @brief P50延迟（毫秒） */
        uint32_t p50_latency_ms;
        
        /** @brief P90延迟（毫秒） */
        uint32_t p90_latency_ms;
        
        /** @brief P99延迟（毫秒） */
        uint32_t p99_latency_ms;
    } latency;
    
    /** @brief 错误率统计 */
    struct {
        /** @brief 当前错误率（百分比） */
        float current_error_rate;
        
        /** @brief 平均错误率（百分比） */
        float avg_error_rate;
        
        /** @brief 最大错误率（百分比） */
        float max_error_rate;
        
        /** @brief 总错误数 */
        uint64_t total_errors;
        
        /** @brief 错误类型分布 */
        struct {
            uint64_t io_errors;
            uint64_t network_errors;
            uint64_t format_errors;
            uint64_t other_errors;
        } error_types;
    } error_rate;
    
    /** @brief 队列统计 */
    struct {
        /** @brief 当前队列大小（记录数） */
        size_t current_queue_size;
        
        /** @brief 最大队列大小（记录数） */
        size_t max_queue_size;
        
        /** @brief 队列使用率（百分比） */
        float queue_usage;
        
        /** @brief 队列丢弃记录数 */
        uint64_t dropped_records;
    } queue;
    
    /** @brief 资源使用统计 */
    struct {
        /** @brief 内存使用量（字节） */
        size_t memory_usage;
        
        /** @brief CPU使用率（百分比） */
        float cpu_usage;
        
        /** @brief 线程数量 */
        int thread_count;
        
        /** @brief 文件描述符数量 */
        int fd_count;
    } resource;
} log_monitoring_stats_t;

/* ==================== 查询结构体 ==================== */

/**
 * @brief 日志查询条件
 * 
 * 定义日志查询的条件。
 */
typedef struct log_query {
    /** @brief 开始时间戳（Unix时间，毫秒） */
    uint64_t start_time;
    
    /** @brief 结束时间戳（Unix时间，毫秒） */
    uint64_t end_time;
    
    /** @brief 最小日志级别 */
    log_level_t min_level;
    
    /** @brief 最大日志级别 */
    log_level_t max_level;
    
    /** @brief 模块名称模式（支持通配符） */
    const char* module_pattern;
    
    /** @brief 追踪ID */
    const char* trace_id;
    
    /** @brief 消息内容关键词 */
    const char* keyword;
    
    /** @brief 排序方式：true=升序，false=降序 */
    bool ascending;
    
    /** @brief 最大返回记录数 */
    size_t limit;
    
    /** @brief 跳过记录数（分页） */
    size_t offset;
} log_query_t;

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMMON_SERVICE_LOGGING_H */