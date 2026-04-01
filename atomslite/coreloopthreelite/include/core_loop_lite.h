/**
 * @file core_loop_lite.h
 * @brief AgentOS Lite CoreLoopThree - 内部数据结构定义
 * @version 1.0.0
 * @date 2026-03-26
 * 
 * 轻量化核心循环的内部数据结构定义，用于模块内部实现。
 * 外部用户不应直接使用这些结构。
 */

#ifndef AGENTOS_CORE_LOOP_LITE_H
#define AGENTOS_CORE_LOOP_LITE_H

#include "agentos_coreloopthreelite.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 内部类型重定义 ==================== */

/* 为了在core_loop_lite.c和内部头文件之间保持一致性，重新定义内部类型 */
typedef struct agentos_clt_engine_handle_s clt_engine_t;
typedef struct agentos_clt_task_handle_s clt_task_t;
typedef struct clt_worker_thread_s clt_worker_thread_t;
typedef struct clt_task_queue_s clt_task_queue_t;

/* ==================== 内部函数声明 ==================== */

/* 认知层接口 */
char* clt_cognition_process(const char* task_data, size_t task_data_len);

/* 执行层接口 */
char* clt_execution_execute(const char* cognition_result, size_t result_len);

/* 记忆层接口 */
bool clt_memory_init(void);
void clt_memory_cleanup(void);
bool clt_memory_save_result(size_t task_id, const char* result, size_t result_len);

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 设置最后错误信息（内部使用）
 * @param format 错误信息格式字符串
 * @param ... 可变参数
 */
void clt_set_last_error(const char* format, ...);

/**
 * @brief 获取当前时间戳（毫秒）
 * @return 当前时间戳（毫秒）
 */
uint64_t clt_get_current_time_ms(void);

/**
 * @brief 初始化任务队列
 * @param queue 任务队列指针
 * @param capacity 队列容量
 * @return 成功返回true，失败返回false
 */
bool clt_init_task_queue(clt_task_queue_t* queue, size_t capacity);

/**
 * @brief 销毁任务队列
 * @param queue 任务队列指针
 */
void clt_destroy_task_queue(clt_task_queue_t* queue);

/**
 * @brief 任务队列是否为空
 * @param queue 任务队列指针
 * @return 队列为空返回true，否则返回false
 */
bool clt_is_task_queue_empty(const clt_task_queue_t* queue);

/**
 * @brief 任务队列是否已满
 * @param queue 任务队列指针
 * @return 队列已满返回true，否则返回false
 */
bool clt_is_task_queue_full(const clt_task_queue_t* queue);

/**
 * @brief 向任务队列添加任务
 * @param queue 任务队列指针
 * @param task 任务指针
 * @return 成功返回true，失败返回false
 */
bool clt_enqueue_task(clt_task_queue_t* queue, clt_task_t* task);

/**
 * @brief 从任务队列取出任务
 * @param queue 任务队列指针
 * @return 任务指针，队列为空返回NULL
 */
clt_task_t* clt_dequeue_task(clt_task_queue_t* queue);

/**
 * @brief 工作线程函数
 * @param arg 线程参数（worker_thread_t指针）
 * @return 线程退出码
 */
#ifdef _WIN32
DWORD WINAPI clt_worker_thread_func(LPVOID arg);
#else
void* clt_worker_thread_func(void* arg);
#endif

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CORE_LOOP_LITE_H */