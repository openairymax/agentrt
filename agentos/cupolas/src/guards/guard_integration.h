// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file guard_integration.h
 * @brief SafetyGuard与Cupolas组件集成接口
 * 
 * 提供SafetyGuard框架与Cupolas现有组件的集成API。
 */

#ifndef CUPOLAS_GUARD_INTEGRATION_H
#define CUPOLAS_GUARD_INTEGRATION_H

#include "guard_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 集成API
// ============================================================================

/**
 * @brief 初始化Cupolas守卫集成
 * @param config 守卫管理器配置
 * @return 错误码
 */
CUPOLAS_API int cupolas_guards_init(const guard_manager_config_t* config);

/**
 * @brief 清理Cupolas守卫集成
 */
CUPOLAS_API void cupolas_guards_cleanup(void);

/**
 * @brief 启用守卫
 */
CUPOLAS_API void cupolas_guards_enable(void);

/**
 * @brief 禁用守卫
 */
CUPOLAS_API void cupolas_guards_disable(void);

/**
 * @brief 检查守卫是否启用
 * @return 1启用，0禁用
 */
CUPOLAS_API int cupolas_guards_is_enabled(void);

/**
 * @brief 获取守卫管理器实例
 * @return 守卫管理器句柄
 */
CUPOLAS_API guard_manager_t* cupolas_guards_get_manager(void);

/**
 * @brief 注册守卫到Cupolas
 * @param guard 守卫实例
 * @return 错误码
 */
CUPOLAS_API int cupolas_guards_register_guard(guard_t* guard);

/**
 * @brief 执行安全检测（针对Cupolas操作）
 * @param operation 操作名称
 * @param resource 资源标识
 * @param agent_id 代理ID
 * @param input_data 输入数据
 * @param input_size 输入数据大小
 * @param results 结果数组（输出）
 * @param max_results 最大结果数
 * @param actual_results 实际结果数（输出）
 * @return 错误码
 */
CUPOLAS_API int cupolas_guards_check(
    const char* operation,
    const char* resource,
    const char* agent_id,
    const void* input_data,
    size_t input_size,
    guard_result_t* results,
    size_t max_results,
    size_t* actual_results);

/**
 * @brief 注册Cupolas钩子
 * 
 * 将守卫钩子注册到Cupolas各个组件。
 * 注意：此函数需要在Cupolas初始化后调用。
 * @return 错误码
 */
CUPOLAS_API int cupolas_guards_register_hooks(void);

/**
 * @brief 注销Cupolas钩子
 */
CUPOLAS_API void cupolas_guards_unregister_hooks(void);

#ifdef __cplusplus
}
#endif

#endif // CUPOLAS_GUARD_INTEGRATION_H