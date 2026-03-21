/**
 * @file sync.c
 * @brief 同步原语实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "task.h"
#include "mem.h"
#include <stddef.h>

typedef struct agentos_mutex {
    volatile int locked;
    agentos_task_id_t owner;
    int recursion_count;
} agentos_mutex_t;

typedef struct agentos_cond {
    // 等待队列（简化，仅占位）
    void* waiters;
} agentos_cond_t;

typedef struct agentos_sem {
    volatile int value;
    // 等待队列（略）
} agentos_sem_t;

agentos_mutex_t* agentos_mutex_create(void) {
    agentos_mutex_t* m = (agentos_mutex_t*)agentos_mem_alloc(sizeof(agentos_mutex_t));
    if (!m) return NULL;
    m->locked = 0;
    m->owner = 0;
    m->recursion_count = 0;
    return m;
}

void agentos_mutex_destroy(agentos_mutex_t* mutex) {
    agentos_mem_free(mutex);
}

void agentos_mutex_lock(agentos_mutex_t* mutex) {
    agentos_task_id_t self = agentos_task_self();
    if (mutex->owner == self) {
        // 递归锁
        mutex->recursion_count++;
        return;
    }
    // 自旋等待（应改为阻塞）
    while (__sync_lock_test_and_set(&mutex->locked, 1)) {
        agentos_task_yield();
    }
    mutex->owner = self;
    mutex->recursion_count = 1;
}

int agentos_mutex_trylock(agentos_mutex_t* mutex) {
    agentos_task_id_t self = agentos_task_self();
    if (mutex->owner == self) {
        mutex->recursion_count++;
        return 0;
    }
    if (__sync_lock_test_and_set(&mutex->locked, 1) == 0) {
        mutex->owner = self;
        mutex->recursion_count = 1;
        return 0;
    }
    return -1;
}

void agentos_mutex_unlock(agentos_mutex_t* mutex) {
    agentos_task_id_t self = agentos_task_self();
    if (mutex->owner != self) return; // 错误
    if (--mutex->recursion_count > 0) return;
    mutex->owner = 0;
    __sync_lock_release(&mutex->locked);
}

agentos_cond_t* agentos_cond_create(void) {
    agentos_cond_t* c = (agentos_cond_t*)agentos_mem_alloc(sizeof(agentos_cond_t));
    if (!c) return NULL;
    c->waiters = NULL;
    return c;
}

void agentos_cond_destroy(agentos_cond_t* cond) {
    agentos_mem_free(cond);
}

agentos_error_t agentos_cond_wait(
    agentos_cond_t* cond,
    agentos_mutex_t* mutex,
    uint32_t timeout_ms) {
    if (!cond || !mutex) return AGENTOS_EINVAL;
    // 解锁互斥锁，等待条件，再重新加锁
    agentos_mutex_unlock(mutex);
    // 简化：直接睡眠
    agentos_task_sleep(timeout_ms ? timeout_ms : 10);
    agentos_mutex_lock(mutex);
    return AGENTOS_SUCCESS;
}

void agentos_cond_signal(agentos_cond_t* cond) {
    // 唤醒一个等待者（暂未实现）
}

void agentos_cond_broadcast(agentos_cond_t* cond) {
    // 唤醒所有
}

agentos_sem_t* agentos_sem_create(uint32_t initial) {
    agentos_sem_t* s = (agentos_sem_t*)agentos_mem_alloc(sizeof(agentos_sem_t));
    if (!s) return NULL;
    s->value = initial;
    return s;
}

void agentos_sem_destroy(agentos_sem_t* sem) {
    agentos_mem_free(sem);
}

agentos_error_t agentos_sem_wait(agentos_sem_t* sem, uint32_t timeout_ms) {
    while (__sync_sub_and_fetch(&sem->value, 1) < 0) {
        __sync_add_and_fetch(&sem->value, 1);
        if (timeout_ms == 0) {
            return AGENTOS_ETIMEDOUT;
        }
        agentos_task_sleep(1);
    }
    return AGENTOS_SUCCESS;
}

void agentos_sem_post(agentos_sem_t* sem) {
    __sync_add_and_fetch(&sem->value, 1);
}