/**
 * @file config_source.h
 * @brief 统一配置模块 - 源适配层接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 源适配层提供不同配置源的统一适配接口。
 * 支持多种配置源：文件、环境变量、命令行参数、内存、网络等。
 */

#ifndef AGENTOS_CONFIG_SOURCE_H
#define AGENTOS_CONFIG_SOURCE_H

#include "core_config.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置源类型 ==================== */

typedef enum {
    CONFIG_SOURCE_FILE = 0,        // 文件配置源
    CONFIG_SOURCE_ENV = 1,         // 环境变量配置源
    CONFIG_SOURCE_ARGS = 2,        // 命令行参数配置源
    CONFIG_SOURCE_MEMORY = 3,      // 内存配置源
    CONFIG_SOURCE_NETWORK = 4,     // 网络配置源
    CONFIG_SOURCE_DATABASE = 5,    // 数据库配置源
    CONFIG_SOURCE_DEFAULT = 6      // 默认值配置源
} config_source_type_t;

/* ==================== 配置源结构 ==================== */

typedef struct config_source config_source_t;

/* ==================== 配置源属性 ==================== */

typedef struct {
    config_source_type_t type;     // 源类型
    const char* name;              // 源名称
    int priority;                  // 优先级（0-100，越高优先级越高）
    bool read_only;                // 是否只读
    bool watchable;                // 是否可监控变化
    uint64_t timestamp;           // 最后更新时间戳
    uint32_t version;             // 版本号
} config_source_attr_t;

/* ==================== 文件配置源选项 ==================== */

typedef struct {
    const char* file_path;         // 文件路径
    const char* format;            // 文件格式（"json", "yaml", "toml", "ini"）
    const char* encoding;          // 文件编码（"utf-8", "gbk"等）
    bool auto_reload;              // 是否自动重载
    uint32_t reload_interval_ms;   // 重载间隔（毫秒）
} config_file_source_options_t;

/* ==================== 环境变量配置源选项 ==================== */

typedef struct {
    const char* prefix;            // 环境变量前缀（如"AGENTOS_"）
    bool case_sensitive;           // 是否区分大小写
    const char* separator;         // 键分隔符（默认为"_"）
    bool expand_vars;              // 是否展开变量引用（如${VAR}）
} config_env_source_options_t;

/* ==================== 命令行配置源选项 ==================== */

typedef struct {
    int argc;                      // 参数数量
    char** argv;                   // 参数数组
    const char* prefix;            // 参数前缀（如"--manager-"）
    const char* assign_char;       // 赋值字符（默认为"="）
    bool allow_positional;         // 是否允许位置参数
} config_args_source_options_t;

/* ==================== 内存配置源选项 ==================== */

typedef struct {
    const char* data;              // 配置数据
    size_t data_len;               // 数据长度
    const char* format;            // 数据格式
} config_memory_source_options_t;

/* ==================== 源适配器接口 ==================== */

/**
 * @brief 配置源适配器接口定义
 */
typedef struct {
    /** 加载配置 */
    config_error_t (*load)(config_source_t* source, config_context_t* ctx);
    
    /** 保存配置 */
    config_error_t (*save)(config_source_t* source, const config_context_t* ctx);
    
    /** 检查配置是否改变 */
    bool (*has_changed)(config_source_t* source);
    
    /** 获取源属性 */
    const config_source_attr_t* (*get_attributes)(config_source_t* source);
    
    /** 销毁源适配器 */
    void (*destroy)(config_source_t* source);
} config_source_adapter_t;

/* ==================== 文件配置源API ==================== */

/**
 * @brief 创建文件配置源
 * @param options 文件配置源选项
 * @return 配置源对象，失败返回NULL
 */
config_source_t* config_source_create_file(const config_file_source_options_t* options);

/**
 * @brief 创建环境变量配置源
 * @param options 环境变量配置源选项
 * @return 配置源对象，失败返回NULL
 */
config_source_t* config_source_create_env(const config_env_source_options_t* options);

/**
 * @brief 创建命令行配置源
 * @param options 命令行配置源选项
 * @return 配置源对象，失败返回NULL
 */
config_source_t* config_source_create_args(const config_args_source_options_t* options);

/**
 * @brief 创建内存配置源
 * @param options 内存配置源选项
 * @return 配置源对象，失败返回NULL
 */
config_source_t* config_source_create_memory(const config_memory_source_options_t* options);

/**
 * @brief 创建默认值配置源
 * @param default_values 默认值映射表（键值对数组）
 * @param count 键值对数量
 * @return 配置源对象，失败返回NULL
 */
config_source_t* config_source_create_defaults(const char* const* default_values, size_t count);

/* ==================== 通用配置源API ==================== */

/**
 * @brief 销毁配置源
 * @param source 配置源对象
 */
void config_source_destroy(config_source_t* source);

/**
 * @brief 从配置源加载配置到上下文
 * @param source 配置源
 * @param ctx 配置上下文
 * @return 错误码
 */
config_error_t config_source_load(config_source_t* source, config_context_t* ctx);

/**
 * @brief 保存配置上下文到配置源
 * @param source 配置源
 * @param ctx 配置上下文
 * @return 错误码
 */
config_error_t config_source_save(config_source_t* source, const config_context_t* ctx);

/**
 * @brief 检查配置源是否已改变
 * @param source 配置源
 * @return 是否已改变
 */
bool config_source_has_changed(config_source_t* source);

/**
 * @brief 获取配置源属性
 * @param source 配置源
 * @return 配置源属性
 */
const config_source_attr_t* config_source_get_attributes(config_source_t* source);

/**
 * @brief 获取配置源类型
 * @param source 配置源
 * @return 配置源类型
 */
config_source_type_t config_source_get_type(config_source_t* source);

/* ==================== 配置源管理器 ==================== */

typedef struct config_source_manager config_source_manager_t;

/**
 * @brief 创建配置源管理器
 * @return 配置源管理器，失败返回NULL
 */
config_source_manager_t* config_source_manager_create(void);

/**
 * @brief 销毁配置源管理器
 * @param manager 配置源管理器
 */
void config_source_manager_destroy(config_source_manager_t* manager);

/**
 * @brief 添加配置源到管理器
 * @param manager 配置源管理器
 * @param source 配置源
 * @return 错误码
 */
config_error_t config_source_manager_add(config_source_manager_t* manager, config_source_t* source);

/**
 * @brief 从管理器移除配置源
 * @param manager 配置源管理器
 * @param source 配置源
 * @return 错误码
 */
config_error_t config_source_manager_remove(config_source_manager_t* manager, config_source_t* source);

/**
 * @brief 按名称查找配置源
 * @param manager 配置源管理器
 * @param name 配置源名称
 * @return 配置源，未找到返回NULL
 */
config_source_t* config_source_manager_find(config_source_manager_t* manager, const char* name);

/**
 * @brief 从所有配置源加载配置
 * @param manager 配置源管理器
 * @param ctx 配置上下文
 * @param merge_strategy 合并策略（0:覆盖，1:合并，2:智能合并）
 * @return 错误码
 */
config_error_t config_source_manager_load_all(config_source_manager_t* manager, 
                                              config_context_t* ctx, 
                                              int merge_strategy);

/**
 * @brief 监控配置源变化
 * @param manager 配置源管理器
 * @param callback 变化回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
config_error_t config_source_manager_watch(config_source_manager_t* manager,
                                           void (*callback)(config_source_t* source, void* user_data),
                                           void* user_data);

/* ==================== 工具函数 ==================== */

/**
 * @brief 获取配置源类型描述
 * @param type 配置源类型
 * @return 类型描述字符串
 */
const char* config_source_type_to_string(config_source_type_t type);

/**
 * @brief 解析配置文件格式
 * @param file_path 文件路径
 * @return 文件格式字符串，未知返回"unknown"
 */
const char* config_parse_file_format(const char* file_path);

/**
 * @brief 创建配置源名称
 * @param type 配置源类型
 * @param identifier 标识符（如文件路径、环境变量前缀等）
 * @return 配置源名称字符串（需调用者释放）
 */
char* config_source_create_name(config_source_type_t type, const char* identifier);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CONFIG_SOURCE_H */
