/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file auth.h
 * @brief API认证模块接口
 *
 * 提供API密钥认证功能，支持Bearer Token格式。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef gateway_AUTH_H
#define gateway_AUTH_H

#include <stdbool.h>
#include "agentos.h"

/**
 * @brief 认证上下文不透明句柄
 */
typedef struct auth_context auth_context_t;

/**
 * @brief 认证结果枚举
 */
typedef enum {
    AUTH_RESULT_ALLOWED = 0,    /**< 认证通过 */
    AUTH_RESULT_DENIED,         /**< 认证拒绝 */
    AUTH_RESULT_DISABLED        /**< 认证已禁用 */
} auth_result_t;

/**
 * @brief 认证类型枚举
 */
typedef enum {
    AUTH_TYPE_API_KEY = 0,  /**< API密钥认证 */
    AUTH_TYPE_JWT           /**< JWT认证 */
} auth_type_t;

/**
 * @brief 创建认证上下文
 *
 * @param api_key API密钥，若为NULL则禁用API密钥认证
 * @param jwt_secret JWT密钥，若为NULL则禁用JWT认证
 * @param jwt_expire JWT过期时间（秒），默认3600
 * @return 认证上下文，失败返回NULL
 *
 * @ownership 调用者需通过 auth_context_destroy() 释放
 */
auth_context_t* auth_context_create(const char* api_key, const char* jwt_secret, uint32_t jwt_expire);

/**
 * @brief 销毁认证上下文
 * @param ctx 认证上下文
 */
void auth_context_destroy(auth_context_t* ctx);

/**
 * @brief 验证请求
 *
 * 支持 Bearer Token 格式：
 * - Authorization: Bearer <api_key>
 *
 * @param ctx 认证上下文
 * @param auth_header Authorization头值
 * @return 认证结果
 */
auth_result_t auth_validate(auth_context_t* ctx, const char* auth_header);

/**
 * @brief 检查认证是否启用
 * @param ctx 认证上下文
 * @return true 启用，false 禁用
 */
bool auth_is_enabled(auth_context_t* ctx);

/**
 * @brief 设置API密钥
 *
 * @param ctx 认证上下文
 * @param api_key 新的API密钥
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t auth_set_api_key(auth_context_t* ctx, const char* api_key);

/**
 * @brief 生成认证错误响应（JSON格式）
 *
 * @param ctx 认证上下文
 * @param out_json 输出JSON字符串（需调用者free）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t auth_generate_error_response(auth_context_t* ctx, char** out_json);

/**
 * @brief 生成JWT令牌
 *
 * @param ctx 认证上下文
 * @param subject 主题
 * @param issuer 签发者
 * @param payload 载荷JSON
 * @param out_token 输出JWT令牌（需调用者free）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t auth_generate_jwt_token(
    auth_context_t* ctx,
    const char* subject,
    const char* issuer,
    const char* payload,
    char** out_token);

/**
 * @brief 获取认证统计信息
 *
 * @param ctx 认证上下文
 * @param out_json 输出JSON字符串（需调用者free）
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t auth_get_stats(auth_context_t* ctx, char** out_json);

/**
 * @brief 清理令牌缓存
 * @param ctx 认证上下文
 */
void auth_clear_token_cache(auth_context_t* ctx);

/**
 * @brief 设置认证类型
 *
 * @param ctx 认证上下文
 * @param type 认证类型
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t auth_set_type(auth_context_t* ctx, auth_type_t type);

/**
 * @brief 获取当前认证类型
 * @param ctx 认证上下文
 * @return 认证类型
 */
auth_type_t auth_get_type(auth_context_t* ctx);

#endif /* gateway_AUTH_H */
