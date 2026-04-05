/**
 * @file time.h
 * @brief 时间服务接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 提供系统时间服务，包括：
 * - 单调时钟（不受系统时间调整影响）
 * - 实时时钟（受系统时间调整影响）
 * - 定时器管理
 * - 事件同步原语
 *
 * 遵循 POSIX 时间服务规范，支持跨平台使用。
 */

#ifndef AGENTOS_TIME_H
#define AGENTOS_TIME_H

#include <stdint.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 定时器句柄（不透明指针）
 * @ownership 由 agentos_timer_create() 创建，agentos_timer_destroy() 销毁
 */
typedef struct agentos_timer agentos_timer_t;

/**
 * @brief 事件句柄（不透明指针）
 * @ownership 由 agentos_event_create() 创建，agentos_event_destroy() 销毁
 */
typedef struct agentos_event agentos_event_t;

/**
 * @brief 定时器回调函数类型
 * @param userdata 用户数据指针
 *
 * @ownership userdata 的所有权由调用者管理
 * @threadsafe 回调在定时器线程中执行，需保证线程安全
 */
typedef void (*agentos_timer_callback_t)(void* userdata);

/**
 * @brief 获取单调时间（纳秒）
 * @return 自系统启动以来的纳秒数
 *
 * @details
 * 单调时钟不受系统时间调整（如 NTP 同步）影响，
 * 适用于计算时间间隔和超时。
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API uint64_t agentos_time_monotonic_ns(void);

/**
 * @brief 获取单调时间（毫秒）
 * @return 自系统启动以来的毫秒数
 *
 * @details
 * 单调时钟不受系统时间调整影响，精度为毫秒。
 *
 * @threadsafe 是
 * @reentrant 是
 * @see agentos_time_monotonic_ns()
 */
AGENTOS_API uint64_t agentos_time_monotonic_ms(void);

/**
 * @brief 获取当前实时时间（纳秒）
 * @return 自 Unix 纪元（1970-01-01 00:00:00 UTC）以来的纳秒数
 *
 * @details
 * 实时时钟受系统时间调整影响，适用于时间戳记录。
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API uint64_t agentos_time_current_ns(void);

/**
 * @brief 获取当前实时时间（毫秒）
 * @return 自 Unix 纪元以来的毫秒数
 *
 * @threadsafe 是
 * @reentrant 是
 * @see agentos_time_current_ns()
 */
AGENTOS_API uint64_t agentos_time_current_ms(void);

/**
 * @brief 高精度休眠
 * @param ns 休眠纳秒数
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @details
 * 提供纳秒级精度的休眠功能，实际精度取决于操作系统。
 *
 * @threadsafe 是
 * @reentrant 是
 */
AGENTOS_API agentos_error_t agentos_time_nanosleep(uint64_t ns);

/**
 * @brief 创建定时器
 * @param callback 定时器回调函数（必填）
 * @param userdata 用户数据指针（可选）
 * @return 定时器句柄，失败返回 NULL
 *
 * @ownership 返回的定时器由调用者负责通过 agentos_timer_destroy() 销毁
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_timer_start(), agentos_timer_stop(), agentos_timer_destroy()
 */
AGENTOS_API agentos_timer_t* agentos_timer_create(agentos_timer_callback_t callback, void* userdata);

/**
 * @brief 启动定时器
 * @param timer 定时器句柄（非 NULL）
 * @param interval_ms 定时间隔（毫秒）
 * @param one_shot 是否为单次触发（1=单次，0=周期）
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @ownership timer 必须由 agentos_timer_create() 创建
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_timer_stop()
 */
AGENTOS_API agentos_error_t agentos_timer_start(agentos_timer_t* timer, uint32_t interval_ms, int one_shot);

/**
 * @brief 停止定时器
 * @param timer 定时器句柄（非 NULL）
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_timer_start()
 */
AGENTOS_API agentos_error_t agentos_timer_stop(agentos_timer_t* timer);

/**
 * @brief 销毁定时器
 * @param timer 定时器句柄（可为 NULL）
 *
 * @ownership 释放 timer 及其内部资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_timer_create()
 */
AGENTOS_API void agentos_timer_destroy(agentos_timer_t* timer);

/**
 * @brief 初始化事件循环
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @details
 * 初始化全局事件循环，用于管理定时器和事件。
 * 必须在调用其他事件循环函数前调用。
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_time_eventloop_run(), agentos_time_eventloop_cleanup()
 */
AGENTOS_API agentos_error_t agentos_time_eventloop_init(void);

/**
 * @brief 运行事件循环
 *
 * @details
 * 启动事件循环，处理定时器和事件。
 * 此函数会阻塞直到调用 agentos_time_eventloop_stop()。
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_time_eventloop_stop()
 */
AGENTOS_API void agentos_time_eventloop_run(void);

/**
 * @brief 停止事件循环
 *
 * @details
 * 请求停止事件循环，agentos_time_eventloop_run() 将返回。
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_time_eventloop_run()
 */
AGENTOS_API void agentos_time_eventloop_stop(void);

/**
 * @brief 处理定时器事件
 *
 * @details
 * 在事件循环中处理到期的定时器。
 *
 * @threadsafe 否
 * @reentrant 否
 */
AGENTOS_API void agentos_time_timer_process(void);

/**
 * @brief 创建事件对象
 * @return 事件句柄，失败返回 NULL
 *
 * @details
 * 事件是一种同步原语，可用于线程间信号传递。
 *
 * @ownership 返回的事件由调用者负责通过 agentos_event_destroy() 销毁
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_event_wait(), agentos_event_signal(), agentos_event_reset()
 */
AGENTOS_API agentos_event_t* agentos_event_create(void);

/**
 * @brief 等待事件
 * @param event 事件句柄（非 NULL）
 * @param timeout_ms 超时时间（毫秒），0 表示无限等待
 * @return AGENTOS_SUCCESS 成功，AGENTOS_ETIMEOUT 超时，其他为错误码
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_event_signal(), agentos_event_reset()
 */
AGENTOS_API agentos_error_t agentos_event_wait(agentos_event_t* event, uint32_t timeout_ms);

/**
 * @brief 触发事件
 * @param event 事件句柄（非 NULL）
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_event_wait(), agentos_event_reset()
 */
AGENTOS_API agentos_error_t agentos_event_signal(agentos_event_t* event);

/**
 * @brief 重置事件
 * @param event 事件句柄（非 NULL）
 * @return AGENTOS_SUCCESS 成功，其他为错误码
 *
 * @details
 * 将事件状态重置为未触发状态。
 *
 * @threadsafe 是
 * @reentrant 否
 *
 * @see agentos_event_signal()
 */
AGENTOS_API agentos_error_t agentos_event_reset(agentos_event_t* event);

/**
 * @brief 销毁事件对象
 * @param event 事件句柄（可为 NULL）
 *
 * @ownership 释放 event 及其内部资源
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_event_create()
 */
AGENTOS_API void agentos_event_destroy(agentos_event_t* event);

/**
 * @brief 清理事件循环资源
 *
 * @details
 * 释放事件循环占用的所有资源，应在程序退出前调用。
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @see agentos_time_eventloop_init()
 */
AGENTOS_API void agentos_time_eventloop_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TIME_H */
