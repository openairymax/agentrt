/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file gateway_rpc_handler.h
 * @brief 统一的RPC请求处理模块
 *
 * 提供HTTP/WS/Stdio三种网关共享的RPC处理逻辑，
 * 消除代码重复，确保DRY原则。
 *
 * 设计原则：
 *   K-1 内核极简：只做协议转换，零业务逻辑
 *   K-2 接口契约化：所有函数有完整Doxygen注释
 *   E-8 可测试性：独立可单元测试
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef GATEWAY_RPC_HANDLER_H
#define GATEWAY_RPC_HANDLER_H

#include <cJSON.h>
#include <stddef.h>

/**
 * @brief RPC处理结果结构
 */
typedef struct {
    char* response_json;           /**< JSON-RPC响应字符串 */
    int error_code;                /**< 错误码 (0=成功) */
    const char* error_message;     /**< 错误消息 */
} rpc_result_t;

/**
 * @brief 处理JSON-RPC请求的统一接口
 *
 * 此函数封装了完整的JSON-RPC请求处理流程：
 * 1. 验证请求格式
 * 2. 提取method和params
 * 3. 调用自定义handler或默认syscall路由
 * 4. 生成响应
 *
 * @param[in] request JSON-RPC请求对象（不释放）
 * @param[in] handler 自定义处理回调（可为NULL）
 * @param[in] handler_data 回调用户数据
 * @return RPC处理结果，需调用者使用rpc_result_free()释放
 *
 * @ownership 返回值由调用者负责释放
 * @threadsafe 安全（如果handler是线程安全的）
 * @since 1.0.1
 *
 * @code
 * cJSON* request = cJSON_Parse(json_string);
 * if (!request) { return NULL; }
 *
 * rpc_result_t result = gateway_rpc_handle_request(request, my_handler, my_data);
 * 
 * if (result.error_code != 0) {
 *     // 错误处理
 * }
 * 
 * printf("Response: %s\n", result.response_json);
 * rpc_result_free(&result);
 * @endcode
 */
rpc_result_t gateway_rpc_handle_request(const cJSON* request,
                                       int (*handler)(const char*, char**, void*),
                                       void* handler_data);

/**
 * @brief 创建RPC错误结果
 * @param code 错误码
 * @param message 错误消息
 * @return RPC处理结果
 */
rpc_result_t gateway_rpc_create_error(int code, const char* message);

/**
 * @brief 释放RPC处理结果
 * @param result RPC处理结果指针
 */
void gateway_rpc_free(rpc_result_t* result);

#endif /* GATEWAY_RPC_HANDLER_H */
