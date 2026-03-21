/**
 * @file time.h
 * @brief 时间服务接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_TIME_H
#define AGENTOS_TIME_H

#include <stdint.h>
#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 定时器句柄 */
typedef struct agentos_timer agentos_timer_t;

/** 事件句柄 */
typedef struct agentos_event agentos_event_t;

/** 定时器回调函数 */
typedef void (*agentos_timer_callback_t)(void* userdata);

// From data intelligence emerges. by spharx
/**
 * @brief 获取系统启动以来的单调时间（纳秒）
 * @return 单调时间
 */
uint64_t agentos_time_monotonic_ns(void);

/**
 * @brief 获取系统启动以来的单调时间（毫秒）
 * @return 单调时间
 */
uint64_t agentos_time_monotonic_ms(void);

/**
 * @brief 获取当前系统时间戳（纳秒）
 * @return 系统时间戳
 */
uint64_t agentos_time_current_ns(void);

/**
 * @brief 获取当前系统时间戳（毫秒）
 * @return 系统时间戳
 */
uint64_t agentos_time_current_ms(void);

/**
 * @brief 纳秒级睡眠
 * @param ns 睡眠时长（纳秒）
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_time_nanosleep(uint64_t ns);

/**
 * @brief 创建定时器
 * @param callback 回调函数
 * @param userdata 用户数据
 * @return 定时器句柄，失败返回 NULL
 */
agentos_timer_t* agentos_timer_create(agentos_timer_callback_t callback, void* userdata);

/**
 * @brief 启动定时器
 * @param timer 定时器句柄
 * @param interval_ms 定时间隔（毫秒）
 * @param one_shot 是否只执行一次
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_timer_start(agentos_timer_t* timer, uint32_t interval_ms, int one_shot);

/**
 * @brief 停止定时器
 * @param timer 定时器句柄
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_timer_stop(agentos_timer_t* timer);

/**
 * @brief 销毁定时器
 * @param timer 定时器句柄
 */
void agentos_timer_destroy(agentos_timer_t* timer);

/**
 * @brief 创建事件
 * @return 事件句柄，失败返回 NULL
 */
agentos_event_t* agentos_event_create(void);

/**
 * @brief 等待事件
 * @param event 事件句柄
 * @param timeout_ms 超时时间（毫秒，0表示无限等待）
 * @return AGENTOS_SUCCESS 或 AGENTOS_ETIMEDOUT
 */
agentos_error_t agentos_event_wait(agentos_event_t* event, uint32_t timeout_ms);

/**
 * @brief 触发事件
 * @param event 事件句柄
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_event_signal(agentos_event_t* event);

/**
 * @brief 重置事件
 * @param event 事件句柄
 * @return AGENTOS_SUCCESS 或错误码
 */
agentos_error_t agentos_event_reset(agentos_event_t* event);

/**
 * @brief 销毁事件
 * @param event 事件句柄
 */
void agentos_event_destroy(agentos_event_t* event);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TIME_H */