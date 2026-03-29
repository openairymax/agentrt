/**
 * @file manager.h
 * @brief 简单配置管理接�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供基础的键值对配置管理功能�?
 * 完整实现请参�?daemon/commons/svc_config.h
 */

#ifndef AGENTOS_UTILS_CONFIG_H
#define AGENTOS_UTILS_CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置结构 ==================== */

typedef struct agentos_config agentos_config_t;

/* ==================== 配置接口 ==================== */

/**
 * @brief 创建配置对象
 * @return 配置对象，失败返�?NULL
 */
agentos_config_t* agentos_config_create(void);

/**
 * @brief 销毁配置对�?
 * @param manager 配置对象
 */
void agentos_config_destroy(agentos_config_t* manager);

/**
 * @brief 从字符串解析配置（格�? key=value�?
 * @param manager 配置对象
 * @param text 配置文本
 * @return 0 成功，非0 失败
 */
int agentos_config_parse(agentos_config_t* manager, const char* text);

/**
 * @brief 从文件加载配�?
 * @param manager 配置对象
 * @param path 文件路径
 * @return 0 成功，非0 失败
 */
int agentos_config_load_file(agentos_config_t* manager, const char* path);

/**
 * @brief 保存配置到文�?
 * @param manager 配置对象
 * @param path 文件路径
 * @return 0 成功，非0 失败
 */
int agentos_config_save_file(agentos_config_t* manager, const char* path);

/**
 * @brief 获取字符串�?
 * @param manager 配置对象
 * @param key �?
 * @param default_value 默认�?
 * @return 值字符串，找不到返回默认�?
 */
const char* agentos_config_get_string(agentos_config_t* manager, const char* key, const char* default_value);

/**
 * @brief 获取整数�?
 * @param manager 配置对象
 * @param key �?
 * @param default_value 默认�?
 * @return �?
 */
int agentos_config_get_int(agentos_config_t* manager, const char* key, int default_value);

/**
 * @brief 获取浮点数�?
 * @param manager 配置对象
 * @param key �?
 * @param default_value 默认�?
 * @return �?
 */
double agentos_config_get_double(agentos_config_t* manager, const char* key, double default_value);

/**
 * @brief 获取布尔�?
 * @param manager 配置对象
 * @param key �?
 * @param default_value 默认�?
 * @return �?
 */
int agentos_config_get_bool(agentos_config_t* manager, const char* key, int default_value);

/**
 * @brief 设置字符串�?
 * @param manager 配置对象
 * @param key �?
 * @param value �?
 * @return 0 成功，非0 失败
 */
int agentos_config_set_string(agentos_config_t* manager, const char* key, const char* value);

/**
 * @brief 设置整数�?
 * @param manager 配置对象
 * @param key �?
 * @param value �?
 * @return 0 成功，非0 失败
 */
int agentos_config_set_int(agentos_config_t* manager, const char* key, int value);

/**
 * @brief 设置浮点数�?
 * @param manager 配置对象
 * @param key �?
 * @param value �?
 * @return 0 成功，非0 失败
 */
int agentos_config_set_double(agentos_config_t* manager, const char* key, double value);

/**
 * @brief 设置布尔�?
 * @param manager 配置对象
 * @param key �?
 * @param value �?
 * @return 0 成功，非0 失败
 */
int agentos_config_set_bool(agentos_config_t* manager, const char* key, int value);

/**
 * @brief 删除配置�?
 * @param manager 配置对象
 * @param key �?
 * @return 0 成功，非0 失败
 */
int agentos_config_remove(agentos_config_t* manager, const char* key);

/**
 * @brief 检查配置项是否存在
 * @param manager 配置对象
 * @param key �?
 * @return 1 存在�? 不存�?
 */
int agentos_config_has(agentos_config_t* manager, const char* key);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_CONFIG_H */
