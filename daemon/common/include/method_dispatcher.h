/**
 * @file method_dispatcher.h
 * @brief JSON-RPC 方法分发器框架
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * @version 1.0.0
 * @date 2026-04-04
 *
 * 设计说明：
 * - 提供统一的方法处理框架，降低main.c复杂度
 * - 使用函数指针表替代冗长的if-else/switch-case
 * - 遵循ARCHITECTURAL_PRINCIPLES.md E-3资源确定性原则
 */

#ifndef AGENTOS_METHOD_DISPATCHER_H
#define AGENTOS_METHOD_DISPATCHER_H

#include <cjson/cJSON.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 前向声明 ==================== */

typedef struct method_dispatcher method_dispatcher_t;
typedef struct method_handler method_handler_t;
typedef void (*method_fn)(cJSON* params, int id, void* user_data);

/* ==================== 方法处理器 ==================== */

/**
 * @brief 创建方法分发器
 * @param max_methods 最大方法数
 * @return 分发器实例，失败返回NULL
 */
method_dispatcher_t* method_dispatcher_create(size_t max_methods);

/**
 * @brief 销毁方法分发器
 * @param disp 分发器实例
 */
void method_dispatcher_destroy(method_dispatcher_t* disp);

/**
 * @brief 注册方法处理器
 * @param disp 分发器实例
 * @param method 方法名
 * @param handler 处理函数
 * @param user_data 用户数据
 * @return 0成功，非0失败
 */
int method_dispatcher_register(method_dispatcher_t* disp,
                               const char* method,
                               method_fn handler,
                               void* user_data);

/**
 * @brief 注销方法处理器
 * @param disp 分发器实例
 * @param method 方法名
 * @return 0成功，非0失败
 */
int method_dispatcher_unregister(method_dispatcher_t* disp, const char* method);

/**
 * @brief 分发请求
 * @param disp 分发器实例
 * @param request JSON-RPC请求对象
 * @param error_response_fn 错误响应回调
 * @param user_data 用户数据
 * @return 0成功处理，1通知消息（无需响应），负数表示错误
 */
int method_dispatcher_dispatch(method_dispatcher_t* disp,
                               cJSON* request,
                               char* (*error_response_fn)(int code, const char* msg, int id),
                               void* user_data);

/**
 * @brief 获取已注册方法数量
 * @param disp 分发器实例
 * @return 方法数量
 */
size_t method_dispatcher_count(const method_dispatcher_t* disp);

/**
 * @brief 列出所有已注册的方法
 * @param disp 分发器实例
 * @param out_methods [out] 方法名数组（调用者负责释放）
 * @param max_methods 最大方法数
 * @return 实际返回的方法数
 */
size_t method_dispatcher_list(const method_dispatcher_t* disp,
                             char*** out_methods,
                             size_t max_methods);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_METHOD_DISPATCHER_H */
