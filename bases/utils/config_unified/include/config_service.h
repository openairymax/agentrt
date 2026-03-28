/**
 * @file config_service.h
 * @brief 统一配置模块 - 服务层接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 服务层提供配置模块的高级功能：
 * 1. 配置验证和Schema定义
 * 2. 热更新和变化通知
 * 3. 配置加密和安全存储
 * 4. 配置版本管理和回滚
 * 5. 配置模板和变量展开
 */

#ifndef AGENTOS_CONFIG_SERVICE_H
#define AGENTOS_CONFIG_SERVICE_H

#include "core_config.h"
#include "config_source.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置验证相关 ==================== */

/**
 * @brief 配置验证器回调函数
 * @param key 配置键
 * @param value 配置值
 * @param user_data 用户数据
 * @return 验证结果（true:有效，false:无效）
 */
typedef bool (*config_validator_cb_t)(const char* key, const config_value_t* value, void* user_data);

/**
 * @brief 配置验证器
 */
typedef struct config_validator config_validator_t;

/**
 * @brief 验证器类型
 */
typedef enum {
    VALIDATOR_TYPE_RANGE = 0,      // 范围验证
    VALIDATOR_TYPE_REGEX = 1,      // 正则表达式验证
    VALIDATOR_TYPE_ENUM = 2,       // 枚举值验证
    VALIDATOR_TYPE_CUSTOM = 3      // 自定义验证
} validator_type_t;

/**
 * @brief 验证器选项
 */
typedef struct {
    validator_type_t type;         // 验证器类型
    const char* pattern;           // 模式（正则表达式或范围）
    const char** enum_values;      // 枚举值数组
    size_t enum_count;            // 枚举值数量
    config_validator_cb_t custom_cb; // 自定义验证回调
    void* user_data;              // 用户数据
} validator_options_t;

/* ==================== 配置Schema ==================== */

/**
 * @brief 配置Schema项
 */
typedef struct {
    const char* key;               // 配置键
    config_value_type_t type;      // 配置类型
    bool required;                 // 是否必需
    const char* description;       // 描述
    const char* default_value;     // 默认值（字符串形式）
    config_validator_t* validator; // 验证器
} config_schema_item_t;

/**
 * @brief 配置Schema
 */
typedef struct config_schema config_schema_t;

/* ==================== 配置热更新 ==================== */

/**
 * @brief 配置变化回调函数
 * @param ctx 配置上下文
 * @param key 变化的配置键（NULL表示全部变化）
 * @param old_value 旧值（可能为NULL）
 * @param new_value 新值
 * @param user_data 用户数据
 */
typedef void (*config_change_cb_t)(config_context_t* ctx, const char* key,
                                   const config_value_t* old_value,
                                   const config_value_t* new_value,
                                   void* user_data);

/**
 * @brief 热更新管理器
 */
typedef struct config_hot_reload_manager config_hot_reload_manager_t;

/* ==================== 配置加密 ==================== */

/**
 * @brief 加密算法类型
 */
typedef enum {
    ENCRYPTION_NONE = 0,           // 不加密
    ENCRYPTION_AES_256_GCM = 1,    // AES-256-GCM
    ENCRYPTION_CHACHA20_POLY1305 = 2 // ChaCha20-Poly1305
} encryption_algorithm_t;

/**
 * @brief 加密配置
 */
typedef struct {
    encryption_algorithm_t algorithm; // 加密算法
    const char* key;                // 加密密钥
    size_t key_len;                // 密钥长度
    const char* iv;                // 初始化向量
    size_t iv_len;                 // 初始化向量长度
} encryption_config_t;

/* ==================== 配置版本管理 ==================== */

/**
 * @brief 配置版本信息
 */
typedef struct {
    uint32_t version;             // 版本号
    uint64_t timestamp;           // 时间戳
    const char* author;           // 作者
    const char* description;      // 描述
    size_t change_count;         // 变化数量
} config_version_info_t;

/**
 * @brief 版本管理器
 */
typedef struct config_version_manager config_version_manager_t;

/* ==================== 配置服务API ==================== */

/* ==================== 配置验证API ==================== */

/**
 * @brief 创建配置验证器
 * @param options 验证器选项
 * @return 验证器对象，失败返回NULL
 */
config_validator_t* config_validator_create(const validator_options_t* options);

/**
 * @brief 销毁配置验证器
 * @param validator 验证器
 */
void config_validator_destroy(config_validator_t* validator);

/**
 * @brief 验证配置值
 * @param validator 验证器
 * @param key 配置键
 * @param value 配置值
 * @return 验证结果
 */
bool config_validator_validate(config_validator_t* validator, const char* key, const config_value_t* value);

/**
 * @brief 创建范围验证器
 * @param min 最小值（字符串形式）
 * @param max 最大值（字符串形式）
 * @return 验证器对象，失败返回NULL
 */
config_validator_t* config_validator_create_range(const char* min, const char* max);

/**
 * @brief 创建正则表达式验证器
 * @param pattern 正则表达式
 * @return 验证器对象，失败返回NULL
 */
config_validator_t* config_validator_create_regex(const char* pattern);

/**
 * @brief 创建枚举值验证器
 * @param values 枚举值数组
 * @param count 枚举值数量
 * @return 验证器对象，失败返回NULL
 */
config_validator_t* config_validator_create_enum(const char** values, size_t count);

/* ==================== 配置Schema API ==================== */

/**
 * @brief 创建配置Schema
 * @param name Schema名称
 * @return Schema对象，失败返回NULL
 */
config_schema_t* config_schema_create(const char* name);

/**
 * @brief 销毁配置Schema
 * @param schema Schema对象
 */
void config_schema_destroy(config_schema_t* schema);

/**
 * @brief 添加Schema项
 * @param schema Schema对象
 * @param item Schema项
 * @return 错误码
 */
config_error_t config_schema_add_item(config_schema_t* schema, const config_schema_item_t* item);

/**
 * @brief 验证配置上下文是否符合Schema
 * @param schema Schema对象
 * @param ctx 配置上下文
 * @param strict 是否严格模式（是否检查多余配置项）
 * @return 验证结果（true:有效，false:无效）
 */
bool config_schema_validate(config_schema_t* schema, const config_context_t* ctx, bool strict);

/**
 * @brief 获取Schema验证错误信息
 * @param schema Schema对象
 * @param index 错误索引
 * @return 错误信息，无错误返回NULL
 */
const char* config_schema_get_error(config_schema_t* schema, int index);

/**
 * @brief 应用Schema默认值到配置上下文
 * @param schema Schema对象
 * @param ctx 配置上下文
 * @return 错误码
 */
config_error_t config_schema_apply_defaults(config_schema_t* schema, config_context_t* ctx);

/* ==================== 配置热更新API ==================== */

/**
 * @brief 创建热更新管理器
 * @param ctx 配置上下文
 * @param source_manager 配置源管理器
 * @return 热更新管理器，失败返回NULL
 */
config_hot_reload_manager_t* config_hot_reload_manager_create(config_context_t* ctx,
                                                               config_source_manager_t* source_manager);

/**
 * @brief 销毁热更新管理器
 * @param manager 热更新管理器
 */
void config_hot_reload_manager_destroy(config_hot_reload_manager_t* manager);

/**
 * @brief 注册配置变化回调
 * @param manager 热更新管理器
 * @param key 配置键（NULL表示监听所有变化）
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
config_error_t config_hot_reload_register_callback(config_hot_reload_manager_t* manager,
                                                    const char* key,
                                                    config_change_cb_t callback,
                                                    void* user_data);

/**
 * @brief 开始监控配置变化
 * @param manager 热更新管理器
 * @param check_interval_ms 检查间隔（毫秒）
 * @return 错误码
 */
config_error_t config_hot_reload_start(config_hot_reload_manager_t* manager, uint32_t check_interval_ms);

/**
 * @brief 停止监控配置变化
 * @param manager 热更新管理器
 * @return 错误码
 */
config_error_t config_hot_reload_stop(config_hot_reload_manager_t* manager);

/**
 * @brief 手动触发配置重载
 * @param manager 热更新管理器
 * @return 错误码
 */
config_error_t config_hot_reload_trigger(config_hot_reload_manager_t* manager);

/* ==================== 配置加密API ==================== */

/**
 * @brief 加密配置值
 * @param value 配置值
 * @param manager 加密配置
 * @return 加密后的配置值，失败返回NULL
 */
config_value_t* config_encrypt_value(const config_value_t* value, const encryption_config_t* manager);

/**
 * @brief 解密配置值
 * @param encrypted_value 加密的配置值
 * @param manager 加密配置
 * @return 解密后的配置值，失败返回NULL
 */
config_value_t* config_decrypt_value(const config_value_t* encrypted_value, const encryption_config_t* manager);

/**
 * @brief 创建加密配置源包装器
 * @param source 原始配置源
 * @param manager 加密配置
 * @return 加密配置源，失败返回NULL
 */
config_source_t* config_source_create_encrypted(config_source_t* source, const encryption_config_t* manager);

/* ==================== 配置版本管理API ==================== */

/**
 * @brief 创建配置版本管理器
 * @param ctx 配置上下文
 * @param max_versions 最大保留版本数
 * @return 版本管理器，失败返回NULL
 */
config_version_manager_t* config_version_manager_create(config_context_t* ctx, size_t max_versions);

/**
 * @brief 销毁配置版本管理器
 * @param manager 版本管理器
 */
void config_version_manager_destroy(config_version_manager_t* manager);

/**
 * @brief 创建配置快照（新版本）
 * @param manager 版本管理器
 * @param author 作者
 * @param description 描述
 * @return 版本号，失败返回0
 */
uint32_t config_version_create_snapshot(config_version_manager_t* manager,
                                         const char* author,
                                         const char* description);

/**
 * @brief 回滚到指定版本
 * @param manager 版本管理器
 * @param version 版本号
 * @return 错误码
 */
config_error_t config_version_rollback(config_version_manager_t* manager, uint32_t version);

/**
 * @brief 获取版本列表
 * @param manager 版本管理器
 * @param versions 版本信息数组（输出）
 * @param max_count 最大数量
 * @return 实际返回的版本数量
 */
size_t config_version_get_list(config_version_manager_t* manager,
                               config_version_info_t* versions,
                               size_t max_count);

/**
 * @brief 获取版本差异
 * @param manager 版本管理器
 * @param version1 版本1
 * @param version2 版本2
 * @param diff 差异输出缓冲区
 * @param diff_size 缓冲区大小
 * @return 差异大小
 */
size_t config_version_get_diff(config_version_manager_t* manager,
                               uint32_t version1,
                               uint32_t version2,
                               char* diff,
                               size_t diff_size);

/* ==================== 配置模板API ==================== */

/**
 * @brief 展开配置模板变量
 * @param ctx 配置上下文
 * @param template_str 模板字符串
 * @param result 结果输出缓冲区
 * @param result_size 缓冲区大小
 * @return 错误码
 */
config_error_t config_expand_template(config_context_t* ctx,
                                      const char* template_str,
                                      char* result,
                                      size_t result_size);

/**
 * @brief 应用配置模板到上下文
 * @param ctx 配置上下文
 * @param template_ctx 模板配置上下文
 * @return 错误码
 */
config_error_t config_apply_template(config_context_t* ctx, config_context_t* template_ctx);

/* ==================== 高级配置服务API ==================== */

/**
 * @brief 创建完整的配置服务
 * @param service_name 服务名称
 * @param schema 配置Schema（可为NULL）
 * @param enable_hot_reload 是否启用热更新
 * @param enable_encryption 是否启用加密
 * @return 配置服务上下文，失败返回NULL
 */
config_context_t* config_service_create(const char* service_name,
                                        config_schema_t* schema,
                                        bool enable_hot_reload,
                                        bool enable_encryption);

/**
 * @brief 加载配置服务
 * @param ctx 配置服务上下文
 * @param sources 配置源数组
 * @param source_count 配置源数量
 * @return 错误码
 */
config_error_t config_service_load(config_context_t* ctx,
                                   config_source_t** sources,
                                   size_t source_count);

/**
 * @brief 保存配置服务
 * @param ctx 配置服务上下文
 * @param primary_source 主配置源
 * @return 错误码
 */
config_error_t config_service_save(config_context_t* ctx, config_source_t* primary_source);

/**
 * @brief 获取配置服务状态
 * @param ctx 配置服务上下文
 * @param status_json 状态JSON输出缓冲区
 * @param status_size 缓冲区大小
 * @return 错误码
 */
config_error_t config_service_get_status(config_context_t* ctx,
                                         char* status_json,
                                         size_t status_size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CONFIG_SERVICE_H */
