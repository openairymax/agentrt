/**
 * @file core_config.h
 * @brief 统一配置模块 - 核心层接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 统一配置模块核心层，提供统一的配置数据模型和基础接口。
 * 设计原则：
 * 1. 统一的配置数据模型，支持多种数据类型
 * 2. 类型安全的配置访问接口
 * 3. 内存所有权明确，避免内存泄漏
 * 4. 线程安全的基础操作
 */

#ifndef AGENTOS_CORE_CONFIG_H
#define AGENTOS_CORE_CONFIG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 配置数据类型 ==================== */

typedef enum {
    CONFIG_TYPE_NULL = 0,     // 空类型
    CONFIG_TYPE_BOOL = 1,     // 布尔类型
    CONFIG_TYPE_INT = 2,      // 32位整数
    CONFIG_TYPE_INT64 = 3,    // 64位整数
    CONFIG_TYPE_DOUBLE = 4,   // 双精度浮点数
    CONFIG_TYPE_STRING = 5,   // 字符串
    CONFIG_TYPE_ARRAY = 6,    // 数组
    CONFIG_TYPE_OBJECT = 7,   // 对象（映射表）
    CONFIG_TYPE_BINARY = 8    // 二进制数据
} config_value_type_t;

/* ==================== 配置值结构 ==================== */

typedef struct config_value config_value_t;

/* ==================== 错误码定义 ==================== */

typedef enum {
    CONFIG_SUCCESS = 0,           // 成功
    CONFIG_ERROR_INVALID_ARG = 1, // 参数无效
    CONFIG_ERROR_NOT_FOUND = 2,   // 配置项不存在
    CONFIG_ERROR_TYPE_MISMATCH = 3, // 类型不匹配
    CONFIG_ERROR_OUT_OF_MEMORY = 4, // 内存不足
    CONFIG_ERROR_IO = 5,          // I/O错误
    CONFIG_ERROR_PARSE = 6,       // 解析错误
    CONFIG_ERROR_VALIDATION = 7,  // 验证失败
    CONFIG_ERROR_LOCKED = 8,      // 配置被锁定
    CONFIG_ERROR_UNSUPPORTED = 9  // 不支持的操作
} config_error_t;

/* ==================== 配置上下文 ==================== */

typedef struct config_context config_context_t;

/* ==================== 核心API：配置值操作 ==================== */

/**
 * @brief 创建空配置值
 * @return 配置值对象，失败返回NULL
 */
config_value_t* config_value_create_null(void);

/**
 * @brief 创建布尔配置值
 * @param value 布尔值
 * @return 配置值对象，失败返回NULL
 */
config_value_t* config_value_create_bool(bool value);

/**
 * @brief 创建整数配置值
 * @param value 整数值
 * @return 配置值对象，失败返回NULL
 */
config_value_t* config_value_create_int(int32_t value);

/**
 * @brief 创建64位整数配置值
 * @param value 64位整数值
 * @return 配置值对象，失败返回NULL
 */
config_value_t* config_value_create_int64(int64_t value);

/**
 * @brief 创建浮点数配置值
 * @param value 浮点数值
 * @return 配置值对象，失败返回NULL
 */
config_value_t* config_value_create_double(double value);

/**
 * @brief 创建字符串配置值
 * @param value 字符串值（会被复制）
 * @return 配置值对象，失败返回NULL
 */
config_value_t* config_value_create_string(const char* value);

/**
 * @brief 创建数组配置值
 * @param capacity 初始容量
 * @return 配置值对象，失败返回NULL
 */
config_value_t* config_value_create_array(size_t capacity);

/**
 * @brief 创建对象配置值
 * @param capacity 初始容量
 * @return 配置值对象，失败返回NULL
 */
config_value_t* config_value_create_object(size_t capacity);

/**
 * @brief 复制配置值
 * @param value 源配置值
 * @return 新的配置值副本，失败返回NULL
 */
config_value_t* config_value_clone(const config_value_t* value);

/**
 * @brief 销毁配置值
 * @param value 配置值对象
 */
void config_value_destroy(config_value_t* value);

/**
 * @brief 获取配置值类型
 * @param value 配置值
 * @return 配置值类型
 */
config_value_type_t config_value_get_type(const config_value_t* value);

/**
 * @brief 获取布尔值
 * @param value 配置值
 * @param default_value 默认值（当类型不匹配或为空时返回）
 * @return 布尔值
 */
bool config_value_get_bool(const config_value_t* value, bool default_value);

/**
 * @brief 获取整数值
 * @param value 配置值
 * @param default_value 默认值（当类型不匹配或为空时返回）
 * @return 整数值
 */
int32_t config_value_get_int(const config_value_t* value, int32_t default_value);

/**
 * @brief 获取64位整数值
 * @param value 配置值
 * @param default_value 默认值（当类型不匹配或为空时返回）
 * @return 64位整数值
 */
int64_t config_value_get_int64(const config_value_t* value, int64_t default_value);

/**
 * @brief 获取浮点数值
 * @param value 配置值
 * @param default_value 默认值（当类型不匹配或为空时返回）
 * @return 浮点数值
 */
double config_value_get_double(const config_value_t* value, double default_value);

/**
 * @brief 获取字符串值
 * @param value 配置值
 * @param default_value 默认值（当类型不匹配或为空时返回）
 * @return 字符串指针（内部所有，勿释放）
 */
const char* config_value_get_string(const config_value_t* value, const char* default_value);

/* ==================== 核心API：配置上下文操作 ==================== */

/**
 * @brief 创建配置上下文
 * @param name 上下文名称（用于调试和日志）
 * @return 配置上下文，失败返回NULL
 */
config_context_t* config_context_create(const char* name);

/**
 * @brief 销毁配置上下文
 * @param ctx 配置上下文
 */
void config_context_destroy(config_context_t* ctx);

/**
 * @brief 设置配置值
 * @param ctx 配置上下文
 * @param key 配置键（点分格式，如"database.host"）
 * @param value 配置值（所有权转移给上下文）
 * @return 错误码
 */
config_error_t config_context_set(config_context_t* ctx, const char* key, config_value_t* value);

/**
 * @brief 获取配置值
 * @param ctx 配置上下文
 * @param key 配置键
 * @return 配置值（内部所有，勿释放或修改），不存在返回NULL
 */
const config_value_t* config_context_get(const config_context_t* ctx, const char* key);

/**
 * @brief 删除配置项
 * @param ctx 配置上下文
 * @param key 配置键
 * @return 错误码
 */
config_error_t config_context_delete(config_context_t* ctx, const char* key);

/**
 * @brief 检查配置项是否存在
 * @param ctx 配置上下文
 * @param key 配置键
 * @return 是否存在
 */
bool config_context_has(const config_context_t* ctx, const char* key);

/**
 * @brief 清除所有配置项
 * @param ctx 配置上下文
 */
void config_context_clear(config_context_t* ctx);

/**
 * @brief 获取配置项数量
 * @param ctx 配置上下文
 * @return 配置项数量
 */
size_t config_context_count(const config_context_t* ctx);

/**
 * @brief 锁定配置上下文（防止修改）
 * @param ctx 配置上下文
 * @return 错误码
 */
config_error_t config_context_lock(config_context_t* ctx);

/**
 * @brief 解锁配置上下文
 * @param ctx 配置上下文
 * @return 错误码
 */
config_error_t config_context_unlock(config_context_t* ctx);

config_context_t* config_context_clone(const config_context_t* ctx);

config_error_t config_context_copy(config_context_t* dst, const config_context_t* src);

const char* config_context_get_key_at(const config_context_t* ctx, size_t index);

const config_value_t* config_context_get_value_at(const config_context_t* ctx, size_t index);

/* ==================== 工具函数 ==================== */

/**
 * @brief 获取错误码描述
 * @param error 错误码
 * @return 错误描述字符串
 */
const char* config_error_to_string(config_error_t error);

/**
 * @brief 获取配置值类型描述
 * @param type 配置值类型
 * @return 类型描述字符串
 */
const char* config_type_to_string(config_value_type_t type);

/**
 * @brief 打印配置值（用于调试）
 * @param value 配置值
 * @param indent 缩进级别
 */
void config_value_print(const config_value_t* value, int indent);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORE_CONFIG_H */
