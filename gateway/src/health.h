/**
 * @file health.h
 * @brief 健康检查器接口
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef gateway_HEALTH_H
#define gateway_HEALTH_H

#include <stdint.h>
#include "agentos.h"

/**
 * @brief 健康检查器不透明句柄
 */
typedef struct health_checker health_checker_t;

/**
 * @brief 健康状态枚�? */
typedef enum {
    HEALTH_STATUS_HEALTHY = 0,    /**< 健康 */
    HEALTH_STATUS_DEGRADED,       /**< 降级 */
    HEALTH_STATUS_UNHEALTHY       /**< 不健�?*/
} health_status_t;

/**
 * @brief 创建健康检查器
 * 
 * @param check_interval_sec 检查间隔（秒）
 * @return 句柄，失败返�?NULL
 * 
 * @ownership 调用者需通过 health_checker_destroy() 释放
 */
health_checker_t* health_checker_create(uint32_t check_interval_sec);

/**
 * @brief 销毁健康检查器
 * @param checker 检查器句柄
 */
void health_checker_destroy(health_checker_t* checker);

/**
 * @brief 获取当前健康状�? * @param checker 检查器
 * @return 健康状�? */
health_status_t health_checker_get_status(health_checker_t* checker);

/**
 * @brief 获取健康报告（JSON 格式�? * 
 * @param checker 检查器
 * @param out_json 输出 JSON 字符串（需调用�?free�? * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t health_checker_get_report(
    health_checker_t* checker,
    char** out_json);

/**
 * @brief 注册健康检查回�? * 
 * @param checker 检查器
 * @param name 检查项名称
 * @param check_fn 检查函数（返回 0 表示健康�? * @param user_data 用户数据
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t health_checker_register(
    health_checker_t* checker,
    const char* name,
    int (*check_fn)(void* user_data),
    void* user_data);

/**
 * @brief 取消注册健康检查回�? * 
 * @param checker 检查器
 * @param name 检查项名称
 * @return AGENTOS_SUCCESS 成功
 */
agentos_error_t health_checker_unregister(
    health_checker_t* checker,
    const char* name);

#endif /* gateway_HEALTH_H */
