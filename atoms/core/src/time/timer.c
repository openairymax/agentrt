/**
 * @file timer.c
 * @brief 定时器管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "time.h"
#include "task.h"
#include "mem.h"
#include <stddef.h>

typedef struct agentos_timer {
    uint64_t id;
    uint32_t interval_ms;
    agentos_timer_callback_t callback;
    void* userdata;
    uint64_t next_expire;
    int running;
    int one_shot;
    struct agentos_timer* next;
} agentos_timer_t;

typedef struct agentos_event {
    int signaled;
    agentos_mutex_t* lock;
    agentos_cond_t* cond;
} agentos_event_t;

static agentos_timer_t* timer_list = NULL;
static agentos_mutex_t* timer_lock = NULL;
static uint64_t next_timer_id = 1;

static agentos_error_t timer_init(void) {
    if (!timer_lock) {
        timer_lock = agentos_mutex_create();
        if (!timer_lock) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

agentos_timer_t* agentos_timer_create(agentos_timer_callback_t callback, void* userdata) {
    if (!callback) return NULL;
    
    // 初始化定时器系统
    if (timer_init() != AGENTOS_SUCCESS) {
        return NULL;
    }
    
    agentos_timer_t* timer = (agentos_timer_t*)agentos_mem_alloc(sizeof(agentos_timer_t));
    if (!timer) return NULL;
    
    timer->id = next_timer_id++;
    timer->callback = callback;
    timer->userdata = userdata;
    timer->running = 0;
    timer->one_shot = 0;
    
    return timer;
}

agentos_error_t agentos_timer_start(agentos_timer_t* timer, uint32_t interval_ms, int one_shot) {
    if (!timer) return AGENTOS_EINVAL;
    if (interval_ms == 0) return AGENTOS_EINVAL; // 间隔不能为 0
    
    agentos_mutex_lock(timer_lock);
    
    // 如果定时器已经在运行，先停止
    if (timer->running) {
        // 从定时器列表中移除
        agentos_timer_t** p = &timer_list;
        while (*p) {
            if (*p == timer) {
                *p = timer->next;
                break;
            }
            p = &(*p)->next;
        }
    }
    
    // 设置定时器参数
    timer->interval_ms = interval_ms;
    timer->one_shot = one_shot;
    timer->next_expire = agentos_time_monotonic_ns() / 1000000 + interval_ms;
    timer->running = 1;
    
    // 添加到定时器列表
    timer->next = timer_list;
    timer_list = timer;
    
    agentos_mutex_unlock(timer_lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_timer_stop(agentos_timer_t* timer) {
    if (!timer) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(timer_lock);
    
    // 从定时器列表中移除
    agentos_timer_t** p = &timer_list;
    while (*p) {
        if (*p == timer) {
            *p = timer->next;
            break;
        }
        p = &(*p)->next;
    }
    
    timer->running = 0;
    
    agentos_mutex_unlock(timer_lock);
    return AGENTOS_SUCCESS;
}

void agentos_timer_destroy(agentos_timer_t* timer) {
    if (!timer) return;
    
    // 停止定时器
    agentos_timer_stop(timer);
    
    // 释放内存
    agentos_mem_free(timer);
}

void agentos_timer_process(void) {
    uint64_t now = agentos_time_monotonic_ns() / 1000000;
    agentos_mutex_lock(timer_lock);
    agentos_timer_t* node = timer_list;
    agentos_timer_t* prev = NULL;
    while (node) {
        if (node->running && node->next_expire <= now) {
            // 触发回调
            node->callback(node->userdata);
            if (!node->one_shot) {
                // 周期性定时器，重置
                node->next_expire = now + node->interval_ms;
                prev = node;
                node = node->next;
            } else {
                // 一次性定时器，停止
                node->running = 0;
                agentos_timer_t* to_delete = node;
                if (prev) {
                    prev->next = node->next;
                } else {
                    timer_list = node->next;
                }
                node = node->next;
                agentos_mem_free(to_delete);
            }
        } else {
            prev = node;
            node = node->next;
        }
    }
    agentos_mutex_unlock(timer_lock);
}

agentos_event_t* agentos_event_create(void) {
    agentos_event_t* event = (agentos_event_t*)agentos_mem_alloc(sizeof(agentos_event_t));
    if (!event) return NULL;
    
    event->signaled = 0;
    event->lock = agentos_mutex_create();
    if (!event->lock) {
        agentos_mem_free(event);
        return NULL;
    }
    
    event->cond = agentos_cond_create();
    if (!event->cond) {
        agentos_mutex_destroy(event->lock);
        agentos_mem_free(event);
        return NULL;
    }
    
    return event;
}

agentos_error_t agentos_event_wait(agentos_event_t* event, uint32_t timeout_ms) {
    if (!event) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(event->lock);
    
    if (!event->signaled) {
        agentos_error_t err = agentos_cond_wait(event->cond, event->lock, timeout_ms);
        if (err != AGENTOS_SUCCESS) {
            agentos_mutex_unlock(event->lock);
            return err;
        }
    }
    
    event->signaled = 0;
    agentos_mutex_unlock(event->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_event_signal(agentos_event_t* event) {
    if (!event) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(event->lock);
    event->signaled = 1;
    agentos_cond_signal(event->cond);
    agentos_mutex_unlock(event->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_event_reset(agentos_event_t* event) {
    if (!event) return AGENTOS_EINVAL;
    
    agentos_mutex_lock(event->lock);
    event->signaled = 0;
    agentos_mutex_unlock(event->lock);
    return AGENTOS_SUCCESS;
}

void agentos_event_destroy(agentos_event_t* event) {
    if (!event) return;
    
    if (event->lock) {
        agentos_mutex_destroy(event->lock);
    }
    if (event->cond) {
        agentos_cond_destroy(event->cond);
    }
    agentos_mem_free(event);
}