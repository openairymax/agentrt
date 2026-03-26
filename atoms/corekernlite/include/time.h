/**
 * @file time.h
 * @brief AgentOS Lite 时间管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供轻量级的时间管理功能：
 * - 高精度时钟
 * - 时间戳获取
 * - 延时函数
 */

#ifndef AGENTOS_LITE_TIME_H
#define AGENTOS_LITE_TIME_H

#include <stdint.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化时间管理子系统
 * @return 成功返回AGENTOS_LITE_SUCCESS，失败返回错误码
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_time_init(void);

/**
 * @brief 清理时间管理子系统
 */
AGENTOS_LITE_API void agentos_lite_time_cleanup(void);

/**
 * @brief 获取高精度时间戳（纳秒）
 * @return 时间戳（纳秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_ns(void);

/**
 * @brief 获取高精度时间戳（微秒）
 * @return 时间戳（微秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_us(void);

/**
 * @brief 获取高精度时间戳（毫秒）
 * @return 时间戳（毫秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_ms(void);

/**
 * @brief 获取Unix时间戳（秒）
 * @return Unix时间戳（秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_unix(void);

/**
 * @brief 计算时间差（纳秒）
 * @param start 开始时间戳
 * @param end 结束时间戳
 * @return 时间差（纳秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_ns(uint64_t start, uint64_t end);

/**
 * @brief 计算时间差（微秒）
 * @param start 开始时间戳
 * @param end 结束时间戳
 * @return 时间差（微秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_us(uint64_t start, uint64_t end);

/**
 * @brief 计算时间差（毫秒）
 * @param start 开始时间戳
 * @param end 结束时间戳
 * @return 时间差（毫秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_ms(uint64_t start, uint64_t end);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_TIME_H */
