/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file config_unified.h
 * @brief 统一配置模块 - 主头文件
 * 
 * 统一配置模块主头文件，包含所有子模块的头文件。
 * 提供统一的分层配置管理：
 * 1. 核心层：统一的配置数据模型和基础接口
 * 2. 源适配层：多种配置源的统一适配
 * 3. 服务层：高级配置功能（验证、热更新、加密等）
 * 4. 兼容层：向后兼容现有配置API
 * 
 * @author Spharx AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-2 可观测性原则
 */
 *
 * 使用示例：
 * 
 * // 基础使用
 * config_context_t* ctx = config_context_create("myapp");
 * config_value_t* val = config_value_create_string("localhost");
 * config_context_set(ctx, "database.host", val);
 * 
 * // 使用源适配器
 * config_file_source_options_t file_opts = {
 *     .file_path = "manager.yaml",
 *     .format = "yaml"
 * };
 * config_source_t* source = config_source_create_file(&file_opts);
 * config_source_load(source, ctx);
 * 
 * // 使用服务层功能
 * config_hot_reload_manager_t* hot_reload = config_hot_reload_manager_create(ctx, NULL);
 * config_hot_reload_start(hot_reload, 5000);
 */

#ifndef AGENTOS_CONFIG_UNIFIED_H
#define AGENTOS_CONFIG_UNIFIED_H

/* ==================== 核心层 ==================== */
#include "core_config.h"

/* ==================== 源适配层 ==================== */
#include "config_source.h"

/* ==================== 服务层 ==================== */
#include "config_service.h"

/* ==================== 兼容层 ==================== */
#include "config_compat.h"

/* ==================== 简化宏定义 ==================== */

/**
 * @brief 创建字符串配置值的简化宏
 * @param value 字符串值
 * @return 配置值对象
 */
#define CONFIG_STRING(value) config_value_create_string(value)

/**
 * @brief 创建整数配置值的简化宏
 * @param value 整数值
 * @return 配置值对象
 */
#define CONFIG_INT(value) config_value_create_int(value)

/**
 * @brief 创建布尔配置值的简化宏
 * @param value 布尔值
 * @return 配置值对象
 */
#define CONFIG_BOOL(value) config_value_create_bool(value)

/**
 * @brief 创建浮点数配置值的简化宏
 * @param value 浮点数值
 * @return 配置值对象
 */
#define CONFIG_DOUBLE(value) config_value_create_double(value)

/**
 * @brief 安全设置配置值（自动销毁旧值）
 * @param ctx 配置上下文
 * @param key 配置键
 * @param value 配置值（所有权转移）
 */
#define CONFIG_SET_SAFE(ctx, key, value) do { \
    const config_value_t* old = config_context_get(ctx, key); \
    if (old) { \
        config_value_t* old_mutable = (config_value_t*)old; \
        config_context_delete(ctx, key); \
        config_value_destroy(old_mutable); \
    } \
    config_context_set(ctx, key, value); \
} while(0)

/**
 * @brief 安全获取字符串配置值
 * @param ctx 配置上下文
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
#define CONFIG_GET_STRING_SAFE(ctx, key, default_value) ({ \
    const config_value_t* val = config_context_get(ctx, key); \
    val ? config_value_get_string(val, default_value) : default_value; \
})

/**
 * @brief 安全获取整数配置值
 * @param ctx 配置上下文
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
#define CONFIG_GET_INT_SAFE(ctx, key, default_value) ({ \
    const config_value_t* val = config_context_get(ctx, key); \
    val ? config_value_get_int(val, default_value) : default_value; \
})

/**
 * @brief 安全获取布尔配置值
 * @param ctx 配置上下文
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
#define CONFIG_GET_BOOL_SAFE(ctx, key, default_value) ({ \
    const config_value_t* val = config_context_get(ctx, key); \
    val ? config_value_get_bool(val, default_value) : default_value; \
})

/**
 * @brief 安全获取浮点数配置值
 * @param ctx 配置上下文
 * @param key 配置键
 * @param default_value 默认值
 * @return 配置值
 */
#define CONFIG_GET_DOUBLE_SAFE(ctx, key, default_value) ({ \
    const config_value_t* val = config_context_get(ctx, key); \
    val ? config_value_get_double(val, default_value) : default_value; \
})

/* ==================== 配置路径辅助宏 ==================== */

/**
 * @brief 构建配置路径
 * @param base 基础路径
 * @param key 子键
 * @return 完整路径字符串
 */
#define CONFIG_PATH(base, key) (base "." key)

/**
 * @brief 构建数据库配置路径
 * @param key 子键
 * @return 完整路径字符串
 */
#define CONFIG_DB_PATH(key) CONFIG_PATH("database", key)

/**
 * @brief 构建日志配置路径
 * @param key 子键
 * @return 完整路径字符串
 */
#define CONFIG_LOG_PATH(key) CONFIG_PATH("logging", key)

/**
 * @brief 构建网络配置路径
 * @param key 子键
 * @return 完整路径字符串
 */
#define CONFIG_NETWORK_PATH(key) CONFIG_PATH("network", key)

/**
 * @brief 构建安全配置路径
 * @param key 子键
 * @return 完整路径字符串
 */
#define CONFIG_SECURITY_PATH(key) CONFIG_PATH("security", key)

/* ==================== 配置验证辅助宏 ==================== */

/**
 * @brief 验证配置值是否为有效字符串
 * @param value 配置值
 * @return 是否有效
 */
#define CONFIG_VALID_STRING(value) (value && config_value_get_type(value) == CONFIG_TYPE_STRING)

/**
 * @brief 验证配置值是否为有效整数
 * @param value 配置值
 * @return 是否有效
 */
#define CONFIG_VALID_INT(value) (value && config_value_get_type(value) == CONFIG_TYPE_INT)

/**
 * @brief 验证配置值是否为有效布尔值
 * @param value 配置值
 * @return 是否有效
 */
#define CONFIG_VALID_BOOL(value) (value && config_value_get_type(value) == CONFIG_TYPE_BOOL)

/**
 * @brief 验证配置值是否为有效浮点数
 * @param value 配置值
 * @return 是否有效
 */
#define CONFIG_VALID_DOUBLE(value) (value && config_value_get_type(value) == CONFIG_TYPE_DOUBLE)

/* ==================== 配置错误处理宏 ==================== */

/**
 * @brief 检查配置操作是否成功
 * @param error 错误码
 * @return 是否成功
 */
#define CONFIG_SUCCESS(error) ((error) == CONFIG_SUCCESS)

/**
 * @brief 检查配置操作是否失败
 * @param error 错误码
 * @return 是否失败
 */
#define CONFIG_FAILED(error) ((error) != CONFIG_SUCCESS)

/**
 * @brief 如果配置操作失败则返回
 * @param error 错误码
 */
#define CONFIG_RETURN_IF_FAILED(error) do { \
    if (CONFIG_FAILED(error)) { \
        return error; \
    } \
} while(0)

/**
 * @brief 如果配置操作失败则转到标签
 * @param error 错误码
 * @param label 跳转标签
 */
#define CONFIG_GOTO_IF_FAILED(error, label) do { \
    if (CONFIG_FAILED(error)) { \
        goto label; \
    } \
} while(0)

/* ==================== 配置初始化宏 ==================== */

/**
 * @brief 初始化配置上下文并设置默认值
 * @param ctx_name 上下文名称
 * @param defaults 默认值数组
 * @param count 默认值数量
 * @return 配置上下文
 */
#define CONFIG_INIT_WITH_DEFAULTS(ctx_name, defaults, count) ({ \
    config_context_t* ctx = config_context_create(ctx_name); \
    if (ctx) { \
        for (size_t i = 0; i < (count); i += 2) { \
            config_value_t* val = config_value_create_string((defaults)[i + 1]); \
            if (val) { \
                config_context_set(ctx, (defaults)[i], val); \
            } \
        } \
    } \
    ctx; \
})

/**
 * @brief 自动初始化配置兼容层
 */
#define CONFIG_COMPAT_AUTO_INIT() do { \
    static int _config_compat_initialized = 0; \
    if (!_config_compat_initialized) { \
        config_compat_config_t manager = { \
            .mode = COMPAT_MODE_MIXED, \
            .auto_init = true, \
            .lazy_load = true \
        }; \
        config_compat_init(&manager); \
        _config_compat_initialized = 1; \
    } \
} while(0)

/* ==================== 配置类型转换宏 ==================== */

/**
 * @brief 安全转换配置值为字符串
 * @param value 配置值
 * @param default_value 默认值
 * @return 字符串值
 */
#define CONFIG_AS_STRING(value, default_value) \
    (config_value_get_type(value) == CONFIG_TYPE_STRING ? \
     config_value_get_string(value, default_value) : default_value)

/**
 * @brief 安全转换配置值为整数
 * @param value 配置值
 * @param default_value 默认值
 * @return 整数值
 */
#define CONFIG_AS_INT(value, default_value) \
    (config_value_get_type(value) == CONFIG_TYPE_INT ? \
     config_value_get_int(value, default_value) : default_value)

/**
 * @brief 安全转换配置值为布尔值
 * @param value 配置值
 * @param default_value 默认值
 * @return 布尔值
 */
#define CONFIG_AS_BOOL(value, default_value) \
    (config_value_get_type(value) == CONFIG_TYPE_BOOL ? \
     config_value_get_bool(value, default_value) : default_value)

/**
 * @brief 安全转换配置值为浮点数
 * @param value 配置值
 * @param default_value 默认值
 * @return 浮点数值
 */
#define CONFIG_AS_DOUBLE(value, default_value) \
    (config_value_get_type(value) == CONFIG_TYPE_DOUBLE ? \
     config_value_get_double(value, default_value) : default_value)

#endif /* AGENTOS_CONFIG_UNIFIED_H */
