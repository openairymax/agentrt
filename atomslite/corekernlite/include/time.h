/**
 * @file time.h
 * @brief 时间管理接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_LITE_TIME_H
#define AGENTOS_LITE_TIME_H

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取当前时间（纳秒）
 * @return 纳秒时间戳
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_ns(void);

/**
 * @brief 获取当前时间（微秒）
 * @return 微秒时间戳
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_us(void);

/**
 * @brief 获取当前时间（毫秒）
 * @return 毫秒时间戳
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_ms(void);

/**
 * @brief 获取Unix时间戳（秒）
 * @return Unix时间戳
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_get_unix(void);

/**
 * @brief 计算时间差（纳秒）
 * @param start 开始时间（纳秒）
 * @param end 结束时间（纳秒）
 * @return 时间差（纳秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_ns(uint64_t start, uint64_t end);

/**
 * @brief 计算时间差（微秒）
 * @param start 开始时间（微秒）
 * @param end 结束时间（微秒）
 * @return 时间差（微秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_us(uint64_t start, uint64_t end);

/**
 * @brief 计算时间差（毫秒）
 * @param start 开始时间（毫秒）
 * @param end 结束时间（毫秒）
 * @return 时间差（毫秒）
 */
AGENTOS_LITE_API uint64_t agentos_lite_time_diff_ms(uint64_t start, uint64_t end);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_LITE_TIME_H */