/**
 * @file audit_queue.h
 * @brief 异步审计队列内部接口
 */
#ifndef DOMAIN_AUDIT_QUEUE_H
#define DOMAIN_AUDIT_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 队列结构 */
typedef struct audit_queue {
    char**          buffer;
    size_t          capacity;
    size_t          head;
    size_t          tail;
    size_t          count;
    pthread_mutex_t lock;
    pthread_cond_t  not_full;
    pthread_cond_t  not_empty;
    // From data intelligence emerges. by spharx
    volatile int    shutting_down;
} audit_queue_t;

/**
 * @brief 创建队列
 * @param capacity 队列容量
 * @return 队列句柄，失败返回 NULL
 */
audit_queue_t* audit_queue_create(size_t capacity);

/**
 * @brief 销毁队列
 */
void audit_queue_destroy(audit_queue_t* q);

/**
 * @brief 向队列推送一条消息（字符串）
 * @param q 队列
 * @param data 字符串（内部会复制）
 * @param timeout_ms 超时（毫秒），-1 无限等待，0 立即返回
 * @return 0 成功，-1 队列满且超时
 */
int audit_queue_push(audit_queue_t* q, const char* data, int timeout_ms);

/**
 * @brief 从队列弹出一条消息
 * @param q 队列
 * @param out_data 输出字符串（需调用者 free）
 * @param timeout_ms 超时（毫秒），-1 无限等待，0 立即返回
 * @return 0 成功，-1 队列空且超时
 */
int audit_queue_pop(audit_queue_t* q, char** out_data, int timeout_ms);

/**
 * @brief 设置关闭标志，唤醒所有等待线程
 */
void audit_queue_shutdown(audit_queue_t* q);

#ifdef __cplusplus
}
#endif

#endif /* DOMAIN_AUDIT_QUEUE_H */