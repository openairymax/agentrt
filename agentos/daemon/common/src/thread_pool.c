#include "thread_pool.h"

#include "error.h"
#include "memory_compat.h"

#include <platform.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct task_node {
    thread_task_fn_t fn;
    void *arg;
    struct task_node *next;
} task_node_t;

struct thread_pool_s {
    thread_pool_config_t config;
    agentos_thread_t *threads;
    uint32_t thread_count;
    task_node_t *queue_head;
    task_node_t *queue_tail;
    uint32_t queue_count;
    uint32_t active_count;
    agentos_mutex_t lock;
    agentos_cond_t notify;
    bool running;
    bool shutdown;
};

static void *worker_thread_func(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (true) {
        agentos_mutex_lock(&pool->lock);

        while (pool->queue_count == 0 && !pool->shutdown) {
            agentos_cond_wait(&pool->notify, &pool->lock);
        }

        if (pool->shutdown && pool->queue_count == 0) {
            agentos_mutex_unlock(&pool->lock);
            break;
        }

        task_node_t *task = pool->queue_head;
        if (task) {
            pool->queue_head = task->next;
            if (!pool->queue_head)
                pool->queue_tail = NULL;
            pool->queue_count--;
            pool->active_count++;
        }

        agentos_mutex_unlock(&pool->lock);

        if (task) {
            task->fn(task->arg);
            AGENTOS_FREE(task);

            agentos_mutex_lock(&pool->lock);
            pool->active_count--;
            agentos_mutex_unlock(&pool->lock);
        }
    }

    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OVERFLOW, "limit exceeded");
    return NULL;
}

thread_pool_t *thread_pool_create(const thread_pool_config_t *config)
{
    thread_pool_t *pool = (thread_pool_t *)AGENTOS_CALLOC(1, sizeof(thread_pool_t));
    if (!pool) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    if (config) {
        pool->config = *config;
    } else {
        thread_pool_config_t defaults;
        defaults.min_threads = 2;
        defaults.max_threads = 8;
        defaults.queue_size = 256;
        defaults.idle_timeout_ms = 30000;
        pool->config = defaults;
    }

    pool->threads =
        (agentos_thread_t *)AGENTOS_CALLOC(pool->config.max_threads, sizeof(agentos_thread_t));
    if (!pool->threads) {
        AGENTOS_FREE(pool);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    agentos_mutex_init(&pool->lock);
    agentos_cond_init(&pool->notify);

    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->queue_count = 0;
    pool->active_count = 0;
    pool->running = true;
    pool->shutdown = false;

    uint32_t num_threads = pool->config.min_threads;
    if (num_threads < 1)
        num_threads = 1;
    if (num_threads > pool->config.max_threads)
        num_threads = pool->config.max_threads;

    for (uint32_t i = 0; i < num_threads; i++) {
        int rc = agentos_thread_create(&pool->threads[i], worker_thread_func, pool);
        if (rc == 0) {
            pool->thread_count++;
        }
    }

    if (pool->thread_count == 0) {
        agentos_mutex_destroy(&pool->lock);
        agentos_cond_destroy(&pool->notify);
        AGENTOS_FREE(pool->threads);
        AGENTOS_FREE(pool);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OVERFLOW, "limit exceeded");
        return NULL;
    }

    return pool;
}

void thread_pool_destroy(thread_pool_t *pool)
{
    if (!pool)
        return;

    agentos_mutex_lock(&pool->lock);
    pool->shutdown = true;
    agentos_cond_broadcast(&pool->notify);
    agentos_mutex_unlock(&pool->lock);

    for (uint32_t i = 0; i < pool->thread_count; i++) {
        agentos_thread_join(pool->threads[i], NULL);
    }

    task_node_t *node = pool->queue_head;
    while (node) {
        task_node_t *next = node->next;
        AGENTOS_FREE(node);
        node = next;
    }

    agentos_mutex_destroy(&pool->lock);
    agentos_cond_destroy(&pool->notify);
    AGENTOS_FREE(pool->threads);
    AGENTOS_FREE(pool);
}

int thread_pool_submit(thread_pool_t *pool, thread_task_fn_t task, void *arg)
{
    if (!pool || !task)
        return AGENTOS_ERR_INVALID_PARAM;
    if (!pool->running)
        return AGENTOS_ERR_UNKNOWN;

    task_node_t *node = (task_node_t *)AGENTOS_CALLOC(1, sizeof(task_node_t));
    if (!node)
        return AGENTOS_ERR_OUT_OF_MEMORY;

    node->fn = task;
    node->arg = arg;
    node->next = NULL;

    agentos_mutex_lock(&pool->lock);

    if (pool->shutdown) {
        agentos_mutex_unlock(&pool->lock);
        AGENTOS_FREE(node);
        return AGENTOS_ERR_UNKNOWN;
    }

    if (pool->queue_count >= pool->config.queue_size) {
        agentos_mutex_unlock(&pool->lock);
        AGENTOS_FREE(node);
        return AGENTOS_ERR_OVERFLOW;
    }

    if (pool->queue_tail) {
        pool->queue_tail->next = node;
    } else {
        pool->queue_head = node;
    }
    pool->queue_tail = node;
    pool->queue_count++;

    agentos_cond_signal(&pool->notify);
    agentos_mutex_unlock(&pool->lock);

    return 0;
}

uint32_t thread_pool_active_count(thread_pool_t *pool)
{
    if (!pool)
        return 0;
    agentos_mutex_lock(&pool->lock);
    uint32_t count = pool->active_count;
    agentos_mutex_unlock(&pool->lock);
    return count;
}

uint32_t thread_pool_pending_count(thread_pool_t *pool)
{
    if (!pool)
        return 0;
    agentos_mutex_lock(&pool->lock);
    uint32_t count = pool->queue_count;
    agentos_mutex_unlock(&pool->lock);
    return count;
}

bool thread_pool_is_running(thread_pool_t *pool)
{
    if (!pool)
        return false;
    return pool->running;
}
