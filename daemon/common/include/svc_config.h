/**
 * @file svc_config.h
 * @brief AgentOS 配置服务接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 设计原则：
 * 1. 支持多种配置源：文件、环境变量、命令行
 * 2. 配置优先级：命令行 > 环境变量 > 配置文件 > 默认值
 * 3. 统一配置格式：YAML/JSON
 * 4. 热重载支持
 */

#ifndef AGENTOS_SVC_CONFIG_H
#define AGENTOS_SVC_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置类型 ==================== */

typedef enum {
    CONFIG_TYPE_STRING = 0,
    CONFIG_TYPE_INT    = 1,
    CONFIG_TYPE_INT64  = 2,
    CONFIG_TYPE_FLOAT  = 3,
    CONFIG_TYPE_BOOL   = 4,
    CONFIG_TYPE_ARRAY  = 5,
    CONFIG_TYPE_OBJECT = 6
} config_type_t;

/* ==================== 配置项 ==================== */

typedef struct config_item config_item_t;

/**
 * @brief 配置迭代器
 */
typedef struct config_iterator {
    const char* key;
    config_item_t* value;
    int index;
} config_iterator_t;

/* ==================== 配置上下文 ==================== */

typedef struct config_context config_context_t;

/* ==================== 初始化与清理 ==================== */

/**
 * @brief 创建配置上下文
 * @param service_name 服务名称（用于配置文件命名）
 * @return 配置上下文，失败返回 NULL
 */
config_context_t* svc_config_create(const char* service_name);

/**
 * @brief 销毁配置上下文
 * @param ctx 配置上下文
 */
void svc_config_destroy(config_context_t* ctx);

/**
 * @brief 从文件加载配置
 * @param ctx 配置上下文
 * @param file_path 配置文件路径
 * @return 0 成功，非0 失败
 */
int svc_config_load_file(config_context_t* ctx, const char* file_path);

/**
 * @brief 从字符串加载配置
 * @param ctx 配置上下文
 * @param yaml_content YAML 内容
 * @return 0 成功，非0 失败
 */
int svc_config_load_string(config_context_t* ctx, const char* yaml_content);

/**
 * @brief 从环境变量加载配置
 * @param ctx 配置上下文
 * @param prefix 环境变量前缀（如 "AGENTOS_"）
 * @return 加载的配置项数量
 */
int svc_config_load_env(config_context_t* ctx, const char* prefix);

/**
 * @brief 热重载配置
 * @param ctx 配置上下文
 * @return 0 成功，非0 失败
 */
int svc_config_reload(config_context_t* ctx);

/**
 * @brief 保存配置到文件
 * @param ctx 配置上下文
 * @param file_path 输出文件路径
 * @return 0 成功，非0 失败
 */
int svc_config_save(config_context_t* ctx, const char* file_path);

/* ==================== 获取配置值 ==================== */

/**
 * @brief 获取字符串配置
 * @param ctx 配置上下文
 * @param key 配置键（如 "database.host"）
 * @param default_value 默认值
 * @return 配置值（内部指针，勿释放）
 */
const char* svc_config_get_string(config_context_t* ctx,
                                  const char* key,
                                  const char* default_value);

/**
 * @brief 获取整数配置
 * @param ctx 配置上下文
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
int svc_config_get_int(config_context_t* ctx,
                       const char* key,
                       int default_value);

/**
 * @brief 获取 64 位整数配置
 * @param ctx 配置上下文
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
int64_t svc_config_get_int64(config_context_t* ctx,
                              const char* key,
                              int64_t default_value);

/**
 * @brief 获取浮点数配置
 * @param ctx 配置上下文
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
double svc_config_get_float(config_context_t* ctx,
                            const char* key,
                            double default_value);

/**
 * @brief 获取布尔配置
 * @param ctx 配置上下文
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
bool svc_config_get_bool(config_context_t* ctx,
                         const char* key,
                         bool default_value);

/* ==================== 设置配置值 ==================== */

/**
 * @brief 设置字符串配置
 * @param ctx 配置上下文
 * @param key 配置键
 * @param value 配置值
 * @return 0 成功，非0 失败
 */
int svc_config_set_string(config_context_t* ctx,
                          const char* key,
                          const char* value);

/**
 * @brief 设置整数配置
 * @param ctx 配置上下文
 * @param key 配置键
 * @param value 配置值
 * @return 0 成功，非0 失败
 */
int svc_config_set_int(config_context_t* ctx,
                       const char* key,
                       int value);

/**
 * @brief 设置浮点数配置
 * @param ctx 配置上下文
 * @param key 配置键
 * @param value 配置值
 * @return 0 成功，非0 失败
 */
int svc_config_set_float(config_context_t* ctx,
                          const char* key,
                          double value);

/**
 * @brief 设置布尔配置
 * @param ctx 配置上下文
 * @param key 配置键
 * @param value 配置值
 * @return 0 成功，非0 失败
 */
int svc_config_set_bool(config_context_t* ctx,
                         const char* key,
                         bool value);

/* ==================== 配置检查 ==================== */

/**
 * @brief 检查配置键是否存在
 * @param ctx 配置上下文
 * @param key 配置键
 * @return 1 存在，0 不存在
 */
int svc_config_has(config_context_t* ctx, const char* key);

/**
 * @brief 获取配置类型
 * @param ctx 配置上下文
 * @param key 配置键
 * @return 配置类型，不存在返回 -1
 */
int svc_config_get_type(config_context_t* ctx, const char* key);

/**
 * @brief 删除配置项
 * @param ctx 配置上下文
 * @param key 配置键
 * @return 0 成功，非0 失败
 */
int svc_config_delete(config_context_t* ctx, const char* key);

/**
 * @brief 获取配置项数量
 * @param ctx 配置上下文
 * @return 配置项数量
 */
size_t svc_config_count(config_context_t* ctx);

/* ==================== 配置迭代 ==================== */

/**
 * @brief 创建配置迭代器
 * @param ctx 配置上下文
 * @return 迭代器（失败返回 NULL）
 */
config_iterator_t* svc_config_iter_create(config_context_t* ctx);

/**
 * @brief 获取迭代器下一个项
 * @param iter 迭代器
 * @return 1 成功，0 迭代结束
 */
int svc_config_iter_next(config_iterator_t* iter);

/**
 * @brief 销毁迭代器
 * @param iter 迭代器
 */
void svc_config_iter_destroy(config_iterator_t* iter);

/* ==================== 配置验证 ==================== */

/**
 * @brief 配置验证回调
 * @param key 配置键
 * @param value 配置值
 * @param user_data 用户数据
 * @return 1 有效，0 无效
 */
typedef int (*config_validator_t)(const char* key,
                                   const void* value,
                                   void* user_data);

/**
 * @brief 添加验证器
 * @param ctx 配置上下文
 * @param key 配置键（支持通配符如 "*.port"）
 * @param validator 验证回调
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
int svc_config_add_validator(config_context_t* ctx,
                             const char* key,
                             config_validator_t validator,
                             void* user_data);

/**
 * @brief 验证所有配置
 * @param ctx 配置上下文
 * @return 1 全部有效，0 有无效配置
 */
int svc_config_validate(config_context_t* ctx);

/* ==================== 宏辅助 ==================== */

/**
 * @brief 获取配置并检查是否改变
 * @param ctx 配置上下文
 * @param key 配置键
 * @param type 类型前缀
 * @param default_value 默认值
 * @param changed 输出参数，是否改变（可为 NULL）
 */
#define SVC_CONFIG_GET_CHANGED(ctx, key, type, default_value, changed) \
    svc_config_get_##type##_with_check(ctx, key, default_value, changed)

/* ==================== 默认配置路径 ==================== */

/**
 * @brief 获取默认配置文件路径
 * @param service_name 服务名称
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 0 成功，非0 失败
 */
int svc_config_get_default_path(const char* service_name,
                                char* buf,
                                size_t buf_size);

/**
 * @brief 搜索配置文件的搜索路径
 * @param service_name 服务名称
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 0 成功，非0 失败
 */
int svc_config_search_paths(const char* service_name,
                           char* buf,
                           size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_SVC_CONFIG_H */
