/**
 * @file manager.h
 * @brief 配置管理器接口
 * 
 * 支持从配置文件、环境变量加载配置，支持运行时更新。
 * 使用JSON格式配置文件，支持热重载。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef DYNAMIC_CONFIG_H
#define DYNAMIC_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include "agentos.h"

/**
 * @brief 日志级别定义
 */
#define LOG_LEVEL_DEBUG 4
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 1

/**
 * @brief 配置管理器不透明句柄
 */
typedef struct config_manager config_manager_t;

/**
 * @brief 服务器配置
 */
typedef struct {
    char* host;                      /**< 监听地址 */
    uint16_t http_port;              /**< HTTP端口 */
    uint16_t ws_port;                /**< WebSocket端口 */
    uint16_t metrics_port;           /**< 指标端口 */
    char* metrics_path;              /**< 指标路径 */
    size_t max_request_size;        /**< 最大请求大小 */
    size_t max_connections;          /**< 最大连接数 */
    uint32_t request_timeout;        /**< 请求超时（秒） */
    uint32_t session_timeout;       /**< 会话超时（秒） */
    uint32_t health_check_interval;  /**< 健康检查间隔（秒） */
} server_config_t;

/**
 * @brief 日志配置
 */
typedef struct {
    char* log_file;                 /**< 日志文件路径 */
    int log_level;                  /**< 日志级别 */
    int log_rotation;               /**< 日志轮转大小（字节） */
    int log_backup_count;           /**< 日志备份数量 */
} log_config_t;

/**
 * @brief 安全配置
 */
typedef struct {
    char* jwt_secret;               /**< JWT密钥 */
    uint32_t jwt_expire;            /**< JWT过期时间（秒） */
    uint32_t max_login_attempts;    /**< 最大登录尝试次数 */
    uint32_t lockout_duration;      /**< 锁定持续时间（秒） */
    char* allowed_origins;          /**< 允许的源（CORS） */
    int enable_auth;                /**< 是否启用认证 */
} security_config_t;

/**
 * @brief 性能配置
 */
typedef struct {
    size_t thread_pool_size;        /**< 线程池大小 */
    size_t connection_pool_size;    /**< 连接池大小 */
    uint32_t request_queue_size;    /**< 请求队列大小 */
    uint32_t response_buffer_size;  /**< 响应缓冲区大小 */
    int enable_compression;         /**< 是否启用压缩 */
} performance_config_t;

/**
 * @brief 完整配置结构
 */
typedef struct {
    server_config_t server;         /**< 服务器配置 */
    log_config_t log;               /**< 日志配置 */
    security_config_t security;     /**< 安全配置 */
    performance_config_t performance; /**< 性能配置 */
} gateway_config_t;

/**
 * @brief 创建配置管理器
 * 
 * @param config_path 配置文件路径（可为NULL使用默认）
 * @return 句柄，失败返回NULL
 * 
 * @ownership 调用者需通过 config_manager_destroy() 释放
 */
config_manager_t* config_manager_create(const char* config_path);

/**
 * @brief 销毁配置管理器
 * @param mgr 管理器句柄
 */
void config_manager_destroy(config_manager_t* mgr);

/**
 * @brief 重新加载配置
 * @param mgr 管理器句柄
 * @param config_path 新的配置文件路径（可为NULL使用当前路径）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_reload(config_manager_t* mgr, const char* config_path);

/**
 * @brief 获取完整配置
 * @param mgr 管理器句柄
 * @param out_config 输出配置结构（需调用者释放内部字符串）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_get_config(config_manager_t* mgr, gateway_config_t* out_config);

/**
 * @brief 获取服务器配置
 * @param mgr 管理器句柄
 * @param out_config 输出服务器配置（需调用者释放内部字符串）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_get_server_config(config_manager_t* mgr, server_config_t* out_config);

/**
 * @brief 获取日志配置
 * @param mgr 管理器句柄
 * @param out_config 输出日志配置（需调用者释放内部字符串）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_get_log_config(config_manager_t* mgr, log_config_t* out_config);

/**
 * @brief 获取安全配置
 * @param mgr 管理器句柄
 * @param out_config 输出安全配置（需调用者释放内部字符串）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_get_security_config(config_manager_t* mgr, security_config_t* out_config);

/**
 * @brief 获取性能配置
 * @param mgr 管理器句柄
 * @param out_config 输出性能配置
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_get_performance_config(config_manager_t* mgr, performance_config_t* out_config);

/**
 * @brief 设置配置值（运行时更新）
 * @param mgr 管理器句柄
 * @param key 配置键（如 "server.http_port"）
 * @param value 配置值
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_set_value(config_manager_t* mgr, const char* key, const char* value);

/**
 * @brief 监听配置变化
 * @param mgr 管理器句柄
 * @param callback 变化回调函数
 * @param user_data 用户数据
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_watch_changes(config_manager_t* mgr,
                                            void (*callback)(const char* key, const char* value, void* user_data),
                                            void* user_data);

/**
 * @brief 导出当前配置为JSON字符串
 * @param mgr 管理器句柄
 * @param out_json 输出JSON字符串（需调用者free）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_export_json(config_manager_t* mgr, char** out_json);

/**
 * @brief 验证配置有效性
 * @param mgr 管理器句柄
 * @param out_error 错误信息（可为NULL）
 * @return AGENTOS_SUCCESS 配置有效
 * @return AGENTOS_EINVAL 配置无效
 */
agentos_error_t config_manager_validate(config_manager_t* mgr, char** out_error);

/**
 * @brief 运行时更新配置值
 * @param mgr 管理器句柄
 * @param key 配置键（如"server.host"）
 * @param value JSON格式的值
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 */
agentos_error_t config_manager_update_value(config_manager_t* mgr, const char* key, const char* value);

/**
 * @brief 设置配置变更监听器
 * @param mgr 管理器句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t config_manager_set_watcher(config_manager_t* mgr,
                                          void (*callback)(const char* key, const char* value, void* user_data),
                                          void* user_data);

#endif /* DYNAMIC_CONFIG_H */