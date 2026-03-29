/**
 * @file audit_queue.h
 * @brief 审计日志队列内部接口 - 线程安全的生产�?消费者队�?
 * @author Spharx
 * @date 2024
 * 
 * 设计原则�?
 * - 线程安全：互斥锁 + 条件变量
 * - 高吞吐：批量写入支持
 * - 优雅关闭：支持等待队列清�?
 */

#ifndef CUPOLAS_AUDIT_QUEUE_H
#define CUPOLAS_AUDIT_QUEUE_H

#include "../platform/platform.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 审计条目类型 */
typedef enum audit_event_type {
    AUDIT_EVENT_PERMISSION = 1,
    AUDIT_EVENT_SANITIZER,
    AUDIT_EVENT_WORKBENCH,
    AUDIT_EVENT_SYSTEM,
    AUDIT_EVENT_CUSTOM
} audit_event_type_t;

/* 审计条目 */
typedef struct audit_entry {
    uint64_t timestamp_ms;
    audit_event_type_t type;
    char* agent_id;
    char* action;
    char* resource;
    char* detail;
    int result;
    struct audit_entry* next;
} audit_entry_t;

/* 审计队列 */
typedef struct audit_queue {
    audit_entry_t* head;
    audit_entry_t* tail;
    size_t size;
    size_t max_size;
    cupolas_mutex_t lock;
    cupolas_cond_t not_empty;
    cupolas_cond_t not_full;
    bool shutdown;
    cupolas_atomic64_t total_pushed;
    cupolas_atomic64_t total_popped;
} audit_queue_t;

/**
 * @brief 创建审计队列
 * @param max_size 最大条目数�? 表示无限�?
 * @return 队列句柄，失败返�?NULL
 */
audit_queue_t* audit_queue_create(size_t max_size);

/**
 * @brief 销毁审计队�?
 * @param queue 队列句柄
 */
void audit_queue_destroy(audit_queue_t* queue);

/**
 * @brief 推入审计条目（阻塞）
 * @param queue 队列句柄
 * @param entry 审计条目（所有权转移�?
 * @return 0 成功，其他失�?
 */
int audit_queue_push(audit_queue_t* queue, audit_entry_t* entry);

/**
 * @brief 推入审计条目（非阻塞�?
 * @param queue 队列句柄
 * @param entry 审计条目（所有权转移�?
 * @return 0 成功，CUPOLAS_ERROR_WOULD_BLOCK 队列满，其他失败
 */
int audit_queue_try_push(audit_queue_t* queue, audit_entry_t* entry);

/**
 * @brief 弹出审计条目（阻塞）
 * @param queue 队列句柄
 * @param entry 输出条目指针
 * @return 0 成功，其他失�?
 */
int audit_queue_pop(audit_queue_t* queue, audit_entry_t** entry);

/**
 * @brief 弹出审计条目（带超时�?
 * @param queue 队列句柄
 * @param entry 输出条目指针
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，CUPOLAS_ERROR_TIMEOUT 超时，其他失败
 */
int audit_queue_timed_pop(audit_queue_t* queue, audit_entry_t** entry, uint32_t timeout_ms);

/**
 * @brief 弹出审计条目（非阻塞�?
 * @param queue 队列句柄
 * @param entry 输出条目指针
 * @return 0 成功，DOMES_ERROR_WOULD_BLOCK 队列空，其他失败
 */
int audit_queue_try_pop(audit_queue_t* queue, audit_entry_t** entry);

/**
 * @brief 批量弹出审计条目
 * @param queue 队列句柄
 * @param entries 输出条目数组
 * @param max_count 最大条目数
 * @param actual_count 实际条目�?
 * @return 0 成功，其他失�?
 */
int audit_queue_pop_batch(audit_queue_t* queue, audit_entry_t** entries, 
                           size_t max_count, size_t* actual_count);

/**
 * @brief 关闭队列
 * @param queue 队列句柄
 * @param wait_empty 是否等待队列清空
 */
void audit_queue_shutdown(audit_queue_t* queue, bool wait_empty);

/**
 * @brief 获取队列大小
 * @param queue 队列句柄
 * @return 队列大小
 */
size_t audit_queue_size(audit_queue_t* queue);

/**
 * @brief 创建审计条目
 * @param type 事件类型
 * @param agent_id Agent ID
 * @param action 操作
 * @param resource 资源
 * @param detail 详情
 * @param result 结果
 * @return 审计条目，失败返�?NULL
 */
audit_entry_t* audit_entry_create(audit_event_type_t type,
                                   const char* agent_id,
                                   const char* action,
                                   const char* resource,
                                   const char* detail,
                                   int result);

/**
 * @brief 销毁审计条�?
 * @param entry 审计条目
 */
void audit_entry_destroy(audit_entry_t* entry);

/**
 * @brief 获取队列统计信息
 * @param queue 队列句柄
 * @param total_pushed 总推入数
 * @param total_popped 总弹出数
 */
void audit_queue_stats(audit_queue_t* queue, uint64_t* total_pushed, uint64_t* total_popped);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_AUDIT_QUEUE_H */
