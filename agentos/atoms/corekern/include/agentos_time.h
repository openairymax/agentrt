/**
 * @file agentos_time.h
 * @brief AgentOS 时间服务接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 提供系统时间服务，包括：
 * - 单调时钟（不受系统时间调整影响）
 * - 实时时钟（受系统时间调整影响）
 * - 定时器管理
 * - 事件同步原语
 *
 * 注意：此文件已从 time.h 重命名为 agentos_time.h，
 * 以避免与系统 <time.h> 头文件冲突。
 * 当 CMake 包含路径中包含此目录时，
 * #include <time.h> 会错误地匹配到此文件而非系统 time.h。
 */

#ifndef AGENTOS_TIME_H
#define AGENTOS_TIME_H

#include <stdint.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_timer agentos_timer_t;
typedef struct agentos_event agentos_event_t;
typedef void (*agentos_timer_callback_t)(void* userdata);

AGENTOS_API uint64_t agentos_time_monotonic_ns(void);
AGENTOS_API uint64_t agentos_time_monotonic_ms(void);
AGENTOS_API uint64_t agentos_time_current_ns(void);
AGENTOS_API uint64_t agentos_time_current_ms(void);
AGENTOS_API uint64_t agentos_time_realtime_ns(void);
AGENTOS_API agentos_error_t agentos_time_nanosleep(uint64_t ns);
AGENTOS_API agentos_error_t agentos_time_sleep_ms(uint32_t ms);
AGENTOS_API agentos_timer_t* agentos_timer_create(agentos_timer_callback_t callback, void* userdata);
AGENTOS_API agentos_error_t agentos_timer_start(agentos_timer_t* timer, uint32_t interval_ms, int one_shot);
AGENTOS_API agentos_error_t agentos_timer_stop(agentos_timer_t* timer);
AGENTOS_API void agentos_timer_destroy(agentos_timer_t* timer);
AGENTOS_API agentos_error_t agentos_time_eventloop_init(void);
AGENTOS_API void agentos_time_eventloop_run(void);
AGENTOS_API void agentos_time_eventloop_stop(void);
AGENTOS_API void agentos_time_timer_process(void);
AGENTOS_API agentos_event_t* agentos_event_create(void);
AGENTOS_API agentos_error_t agentos_event_wait(agentos_event_t* event, uint32_t timeout_ms);
AGENTOS_API agentos_error_t agentos_event_signal(agentos_event_t* event);
AGENTOS_API agentos_error_t agentos_event_reset(agentos_event_t* event);
AGENTOS_API void agentos_event_destroy(agentos_event_t* event);
AGENTOS_API void agentos_time_eventloop_cleanup(void);
AGENTOS_API void agentos_time_timer_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TIME_H */
