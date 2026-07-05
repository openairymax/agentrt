/**
 * @file agentrt_time.h
 * @brief AgentRT 时间服务接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 提供系统时间服务，包括：
 * - 单调时钟（不受系统时间调整影响）
 * - 实时时钟（受系统时间调整影响）
 * - 定时器管理
 * - 事件同步原语
 *
 * 注意：此文件已从 time.h 重命名为 agentrt_time.h，
 * 以避免与系统 <time.h> 头文件冲突。
 * 当 CMake 包含路径中包含此目录时，
 * #include <time.h> 会错误地匹配到此文件而非系统 time.h。
 */

#ifndef AGENTRT_TIME_H
#define AGENTRT_TIME_H

#include "error.h"
#include "export.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentrt_timer agentrt_timer_t;
typedef struct agentrt_event agentrt_event_t;
typedef void (*agentrt_timer_callback_t)(void *userdata);

AGENTRT_API uint64_t agentrt_time_monotonic_ns(void);
AGENTRT_API uint64_t agentrt_time_monotonic_ms(void);
AGENTRT_API uint64_t agentrt_time_current_ns(void);
AGENTRT_API uint64_t agentrt_time_current_ms(void);
AGENTRT_API uint64_t agentrt_time_realtime_ns(void);
AGENTRT_API agentrt_error_t agentrt_time_nanosleep(uint64_t ns);
AGENTRT_API agentrt_error_t agentrt_time_sleep_ms(uint32_t ms);
AGENTRT_API agentrt_timer_t *agentrt_timer_create(agentrt_timer_callback_t callback,
                                                  void *userdata);
AGENTRT_API agentrt_error_t agentrt_timer_start(agentrt_timer_t *timer, uint32_t interval_ms,
                                                int one_shot);
AGENTRT_API agentrt_error_t agentrt_timer_stop(agentrt_timer_t *timer);
AGENTRT_API void agentrt_timer_destroy(agentrt_timer_t *timer);
AGENTRT_API agentrt_error_t agentrt_time_eventloop_init(void);
AGENTRT_API void agentrt_time_eventloop_run(void);
AGENTRT_API void agentrt_time_eventloop_stop(void);
AGENTRT_API void agentrt_time_timer_process(void);
AGENTRT_API agentrt_event_t *agentrt_event_create(void);
AGENTRT_API agentrt_error_t agentrt_event_wait(agentrt_event_t *event, uint32_t timeout_ms);
AGENTRT_API agentrt_error_t agentrt_event_signal(agentrt_event_t *event);
AGENTRT_API agentrt_error_t agentrt_event_reset(agentrt_event_t *event);
AGENTRT_API void agentrt_event_destroy(agentrt_event_t *event);
AGENTRT_API void agentrt_time_eventloop_cleanup(void);
AGENTRT_API void agentrt_time_timer_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_TIME_H */
