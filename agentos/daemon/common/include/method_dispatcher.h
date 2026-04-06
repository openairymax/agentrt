/**
 * @file method_dispatcher.h
 * @brief JSON-RPC 方法分发器框架（基于注册表模式的高效路由）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 设计目标：
 * 1. 消除 if-else 方法路由链，降低圈复杂度
 * 2. 统一所有 JSON-RPC 服务的请求处理模式
 * 3. 支持动态方法注册和运行时扩展
 * 4. 线程安全的分发机制
 *
 * 使用示例：
 * @code
 *   // 创建分发器
 *   method_dispatcher_t* disp = method_dispatcher_create(16);
 *
 *   // 注册方法处理器
 *   method_dispatcher_register(disp, "my_method", on_my_method, NULL);
 *
 *   // 分发请求（自动调用匹配的处理器）
 *   method_dispatcher_dispatch(disp, request, jsonrpc_build_error, &client_fd);
 *
 *   // 清理资源
 *   method_dispatcher_destroy(disp);
 * @endcode
 *
 * 性能特征：
 * - 注册：O(1) 哈希表插入
 * - 分发：O(1) 平均查找
 * - 内存：方法数量线性增长
 */

#ifndef AGENTOS_METHOD_DISPATCHER_H
#define AGENTOS_METHOD_DISPATCHER_H

#include <cjson/cJSON.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct method_dispatcher method_dispatcher_t;

/**
 * @brief 方法处理函数类型签名
 * @param params JSON-RPC 请求的 params 对象
 * @param id 请求 ID（用于构建响应）
 * @param user_data 用户上下文数据（注册时传入）
 * @note 所有方法处理器必须符合此签名
 */
typedef void (*method_fn)(cJSON* params, int id, void* user_data);

/**
 * @创建方法分发器实例
 * @param max_methods 最大支持的方法数量
 * @return 分发器指针，失败返回 NULL
 *
 * 内部实现：分配哈希表和数组存储，O(1) 空间复杂度
 */
method_dispatcher_t* method_dispatcher_create(size_t max_methods);

/**
 * @brief 销毁分发器并释放所有资源
 * @param disp 分发器实例（可为 NULL，安全无操作）
 *
 * 注意：不会销毁注册时传入的 user_data
 */
void method_dispatcher_destroy(method_dispatcher_t* disp);

/**
 * @brief 注册方法处理器
 * @param disp 分发器实例
 * @param method 方法名（如 "complete", "register"）
 * @param handler 处理函数指针
 * @param user_data 传递给处理器的用户数据（可为 NULL）
 * @return 0 成功，-1 失败（已存在或参数无效）
 *
 * 如果方法已存在，将覆盖旧处理器并返回警告日志
 */
int method_dispatcher_register(method_dispatcher_t* disp, const char* method, method_fn handler, void* user_data);

/**
 * @brief 分发 JSON-RPC 请求到对应的处理器
 * @param disp 分发器实例
 * @param request 完整的 JSON-RPC 请求对象
 * @param error_response_fn 错误响应构建函数（用于未找到方法时）
 * @param user_data 传递给处理器的用户数据（通常为 client_fd）
 * @return 0 成功分发，-1 方法未找到或错误
 *
 * 处理流程：
 * 1. 从 request 中提取 "method" 字段
 * 2. 在注册表中查找匹配的处理器
 * 3. 调用处理器，传入 params, id, user_data
 * 4. 如果方法未找到，调用 error_response_fn 构建错误响应
 */
int method_dispatcher_dispatch(method_dispatcher_t* disp, cJSON* request, char* (*error_response_fn)(int, const char*, int), void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_METHOD_DISPATCHER_H */
