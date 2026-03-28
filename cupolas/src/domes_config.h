/**
 * @file domes_config.h
 * @brief 配置热重载 - 运行时配置更新
 * @author Spharx
 * @date 2024
 *
 * 设计原则：
 * - 热重载：无需重启即可更新配置
 * - 原子切换：新配置生效时旧配置安全释放
 * - 验证优先：配置变更前进行验证
 * - 回滚支持：验证失败时自动回滚到旧配置
 *
 * 支持的配置类型：
 * - 权限规则 (permission rules)
 * - 净化规则 (sanitizer rules)
 * - 资源限制 (resource limits)
 * - 日志级别 (log levels)
 * - 审计策略 (audit policy)
 */

#ifndef DOMES_CONFIG_H
#define DOMES_CONFIG_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 配置类型 */
typedef enum config_type {
    CONFIG_TYPE_PERMISSION_RULES = 0,
    CONFIG_TYPE_SANITIZER_RULES,
    CONFIG_TYPE_RESOURCE_LIMITS,
    CONFIG_TYPE_LOG_LEVEL,
    CONFIG_TYPE_AUDIT_POLICY,
    CONFIG_TYPE_ALL
} config_type_t;

/* 配置状态 */
typedef enum config_status {
    CONFIG_STATUS_OK = 0,
    CONFIG_STATUS_LOADING,
    CONFIG_STATUS_VALIDATING,
    CONFIG_STATUS_APPLIED,
    CONFIG_STATUS_ROLLBACK,
    CONFIG_STATUS_ERROR
} config_status_t;

/* 配置变更事件 */
typedef enum config_event {
    CONFIG_EVENT_LOADED = 0,
    CONFIG_EVENT_APPLIED,
    CONFIG_EVENT_ROLLBACK,
    CONFIG_EVENT_ERROR
} config_event_t;

/* 配置版本信息 */
typedef struct config_version {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    uint64_t timestamp_ns;
    const char* commit_hash;
} config_version_t;

/* 配置变更描述 */
typedef struct config_change {
    config_type_t type;
    config_status_t status;
    config_version_t old_version;
    config_version_t new_version;
    const char* file_path;
    const char* error_message;
} config_change_t;

/* 配置观察者回调 */
typedef void (*config_observer_t)(config_event_t event, const config_change_t* change, void* user_data);

/* 配置验证结果 */
typedef struct config_validation_result {
    bool valid;
    const char** errors;
    size_t error_count;
    const char** warnings;
    size_t warning_count;
} config_validation_result_t;

/* 配置句柄 */
typedef struct domes_config domes_config_t;

/**
 * @brief 创建配置管理器
 * @param config_dir 配置文件目录（NULL 使用默认目录）
 * @return 配置管理器句柄，失败返回 NULL
 */
domes_config_t* domes_config_create(const char* config_dir);

/**
 * @brief 销毁配置管理器
 * @param cfg 配置管理器句柄
 */
void domes_config_destroy(domes_config_t* cfg);

/**
 * @brief 加载配置文件
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @param file_path 文件路径（NULL 使用默认路径）
 * @return 0 成功，其他失败
 */
int domes_config_load(domes_config_t* cfg, config_type_t type, const char* file_path);

/**
 * @brief 重载配置文件
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @return 0 成功，其他失败
 */
int domes_config_reload(domes_config_t* cfg, config_type_t type);

/**
 * @brief 验证配置
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @param result 验证结果输出
 * @return 0 验证通过，其他失败
 */
int domes_config_validate(domes_config_t* cfg, config_type_t type,
                        config_validation_result_t* result);

/**
 * @brief 应用配置变更
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @return 0 成功，其他失败
 */
int domes_config_apply(domes_config_t* cfg, config_type_t type);

/**
 * @brief 回滚配置变更
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @return 0 成功，其他失败
 */
int domes_config_rollback(domes_config_t* cfg, config_type_t type);

/**
 * @brief 获取配置版本
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @param version 版本输出
 * @return 0 成功，其他失败
 */
int domes_config_get_version(domes_config_t* cfg, config_type_t type,
                            config_version_t* version);

/**
 * @brief 注册配置观察者
 * @param cfg 配置管理器句柄
 * @param type 关注的配置类型（CONFIG_TYPE_ALL 表示全部）
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 观察者 ID，失败返回 -1
 */
int domes_config_watch(domes_config_t* cfg, config_type_t type,
                      config_observer_t callback, void* user_data);

/**
 * @brief 取消注册配置观察者
 * @param cfg 配置管理器句柄
 * @param watcher_id 观察者 ID
 * @return 0 成功，其他失败
 */
int domes_config_unwatch(domes_config_t* cfg, int watcher_id);

/**
 * @brief 获取配置状态
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @return 配置状态
 */
config_status_t domes_config_get_status(domes_config_t* cfg, config_type_t type);

/**
 * @brief 设置自动重载
 * @param cfg 配置管理器句柄
 * @param type 配置类型（CONFIG_TYPE_ALL 表示全部）
 * @param interval_ms 重载间隔（毫秒），0 表示禁用
 * @return 0 成功，其他失败
 */
int domes_config_set_auto_reload(domes_config_t* cfg, config_type_t type,
                                uint32_t interval_ms);

/**
 * @brief 触发配置重载检查
 * @param cfg 配置管理器句柄
 * @param type 配置类型（CONFIG_TYPE_ALL 表示全部）
 * @return 发生变更的配置类型数量
 */
int domes_config_check_reload(domes_config_t* cfg, config_type_t type);

/**
 * @brief 获取最后错误信息
 * @param cfg 配置管理器句柄
 * @return 错误信息（NULL 表示无错误）
 */
const char* domes_config_get_last_error(domes_config_t* cfg);

/**
 * @brief 获取配置目录
 * @param cfg 配置管理器句柄
 * @return 配置目录路径
 */
const char* domes_config_get_config_dir(domes_config_t* cfg);

/**
 * @brief 导出当前配置为 JSON
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @return 实际写入的字节数
 */
size_t domes_config_export_json(domes_config_t* cfg, config_type_t type,
                               char* buffer, size_t size);

/**
 * @brief 导出当前配置为 YAML
 * @param cfg 配置管理器句柄
 * @param type 配置类型
 * @param buffer 输出缓冲区
 * @param size 缓冲区大小
 * @return 实际写入的字节数
 */
size_t domes_config_export_yaml(domes_config_t* cfg, config_type_t type,
                               char* buffer, size_t size);

/* ============================================================================
 * 便捷函数
 * ============================================================================ */

/**
 * @brief 重载所有配置
 * @param cfg 配置管理器句柄
 * @return 发生变更的配置类型数量
 */
int domes_config_reload_all(domes_config_t* cfg);

/**
 * @brief 验证所有配置
 * @param cfg 配置管理器句柄
 * @return true 所有配置有效，false 存在无效配置
 */
bool domes_config_validate_all(domes_config_t* cfg);

/**
 * @brief 获取配置状态字符串
 * @param status 配置状态
 * @return 状态字符串
 */
const char* domes_config_status_string(config_status_t status);

/**
 * @brief 获取配置类型字符串
 * @param type 配置类型
 * @return 类型字符串
 */
const char* domes_config_type_string(config_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* DOMES_CONFIG_H */