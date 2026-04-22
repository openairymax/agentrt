/**
 * @file layer1_raw_queue.c
 * @brief L1 原始卷异步队列管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer1_raw_queue.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

struct async_queue {
    write_request_t* head;
    write_request_t* tail;
    size_t count;
    size_t capacity;
    uint64_t total_enqueued;
    uint64_t total_dequeued;

#ifdef _WIN32
    CRITICAL_SECTION lock;
    HANDLE not_empty;
    HANDLE not_full;
#else
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
#endif

    int shutdown;
};

async_queue_t* async_queue_create(size_t capacity) {
    async_queue_t* queue = (async_queue_t*)AGENTOS_CALLOC(1, sizeof(async_queue_t));
    if (!queue) {
        AGENTOS_LOG_ERROR("Failed to allocate async queue");
        return NULL;
    }

    queue->capacity = capacity > 0 ? capacity : DEFAULT_ASYNC_QUEUE_SIZE;
    queue->count = 0;
    queue->total_enqueued = 0;
    queue->total_dequeued = 0;
    queue->shutdown = 0;

#ifdef _WIN32
    InitializeCriticalSection(&queue->lock);
    queue->not_empty = CreateEvent(NULL, FALSE, FALSE, NULL);
    queue->not_full = CreateEvent(NULL, FALSE, TRUE, NULL);

    if (!queue->not_empty || !queue->not_full) {
        if (queue->not_empty) CloseHandle(queue->not_empty);
        if (queue->not_full) CloseHandle(queue->not_full);
        DeleteCriticalSection(&queue->lock);
        AGENTOS_FREE(queue);
        return NULL;
    }
#else
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
#endif

    return queue;
}

void async_queue_destroy(async_queue_t* queue) {
    if (!queue) return;

    queue->shutdown = 1;

#ifdef _WIN32
    SetEvent(queue->not_empty);
    SetEvent(queue->not_full);

    EnterCriticalSection(&queue->lock);

    write_request_t* request = queue->head;
    while (request) {
        write_request_t* next = request->next;
        if (request->id) AGENTOS_FREE(request->id);
        if (request->data) AGENTOS_FREE(request->data);
        AGENTOS_FREE(request);
        request = next;
    }

    LeaveCriticalSection(&queue->lock);

    DeleteCriticalSection(&queue->lock);
    CloseHandle(queue->not_empty);
    CloseHandle(queue->not_full);
#else
    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);

    pthread_mutex_lock(&queue->lock);

    write_request_t* request = queue->head;
    while (request) {
        write_request_t* next = request->next;
        if (request->id) AGENTOS_FREE(request->id);
        if (request->data) AGENTOS_FREE(request->data);
        AGENTOS_FREE(request);
        request = next;
    }

    pthread_mutex_unlock(&queue->lock);

    pthread_mutex_destroy(&queue->lock);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
#endif

    AGENTOS_FREE(queue);
}

agentos_error_t async_queue_enqueue(async_queue_t* queue, write_request_t* request,
                                    uint32_t timeout_ms) {
    if (!queue || !request) return AGENTOS_EINVAL;

#ifdef _WIN32
    DWORD timeout = timeout_ms == 0 ? INFINITE : (DWORD)timeout_ms;

    if (WaitForSingleObject(queue->not_full, timeout) != WAIT_OBJECT_0) {
        return AGENTOS_ETIMEOUT;
    }

    EnterCriticalSection(&queue->lock);

    if (queue->count >= queue->capacity) {
        LeaveCriticalSection(&queue->lock);
        return AGENTOS_EBUSY;
    }

    request->next = NULL;
    if (queue->tail) {
        queue->tail->next = request;
        queue->tail = request;
    } else {
        queue->head = queue->tail = request;
    }

    queue->count++;
    queue->total_enqueued++;

    if (queue->count == 1) {
        SetEvent(queue->not_empty);
    }

    if (queue->count >= queue->capacity) {
        ResetEvent(queue->not_full);
    }

    LeaveCriticalSection(&queue->lock);
#else
    struct timespec ts;
    if (timeout_ms > 0) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
    }

    pthread_mutex_lock(&queue->lock);

    while (queue->count >= queue->capacity && !queue->shutdown) {
        if (timeout_ms == 0) {
            pthread_mutex_unlock(&queue->lock);
            return AGENTOS_EBUSY;
        }

        int wait_ret;
        if (timeout_ms > 0) {
            wait_ret = pthread_cond_timedwait(&queue->not_full, &queue->lock, &ts);
        } else {
            wait_ret = pthread_cond_wait(&queue->not_full, &queue->lock);
        }

        if (wait_ret != 0 && !queue->shutdown) {
            if (wait_ret == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->lock);
                return AGENTOS_ETIMEOUT;
            }
            continue;
        }

        if (queue->shutdown) {
            pthread_mutex_unlock(&queue->lock);
            return AGENTOS_ESHUTDOWN;
        }
    }

    if (queue->shutdown) {
        pthread_mutex_unlock(&queue->lock);
        return AGENTOS_ESHUTDOWN;
    }

    request->next = NULL;
    if (queue->tail) {
        queue->tail->next = request;
        queue->tail = request;
    } else {
        queue->head = queue->tail = request;
    }

    queue->count++;
    queue->total_enqueued++;

    if (queue->count == 1) {
        pthread_cond_signal(&queue->not_empty);
    }

    pthread_mutex_unlock(&queue->lock);
#endif

    return AGENTOS_SUCCESS;
}

write_request_t* async_queue_dequeue(async_queue_t* queue, uint32_t timeout_ms) {
    if (!queue) return NULL;

#ifdef _WIN32
    DWORD timeout = timeout_ms == 0 ? INFINITE : (DWORD)timeout_ms;

    if (WaitForSingleObject(queue->not_empty, timeout) != WAIT_OBJECT_0) {
        return NULL;
    }

    EnterCriticalSection(&queue->lock);

    if (queue->count == 0 || queue->shutdown) {
        LeaveCriticalSection(&queue->lock);
        return NULL;
    }

    write_request_t* request = queue->head;
    if (request) {
        queue->head = request->next;
        if (!queue->head) {
            queue->tail = NULL;
        }

        queue->count--;
        queue->total_dequeued++;

        if (queue->count == queue->capacity - 1) {
            SetEvent(queue->not_full);
        }

        if (queue->count == 0) {
            ResetEvent(queue->not_empty);
        }
    }

    LeaveCriticalSection(&queue->lock);
#else
    struct timespec ts;
    if (timeout_ms > 0) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
    }

    pthread_mutex_lock(&queue->lock);

    while (queue->count == 0 && !queue->shutdown) {
        if (timeout_ms == 0) {
            pthread_mutex_unlock(&queue->lock);
            return NULL;
        }

        int wait_ret;
        if (timeout_ms > 0) {
            wait_ret = pthread_cond_timedwait(&queue->not_empty, &queue->lock, &ts);
        } else {
            wait_ret = pthread_cond_wait(&queue->not_empty, &queue->lock);
        }

        if (wait_ret != 0 && !queue->shutdown) {
            if (wait_ret == ETIMEDOUT) {
                pthread_mutex_unlock(&queue->lock);
                return NULL;
            }
            continue;
        }

        if (queue->shutdown) {
            pthread_mutex_unlock(&queue->lock);
            return NULL;
        }
    }

    if (queue->shutdown || queue->count == 0) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    write_request_t* request = queue->head;
    if (request) {
        queue->head = request->next;
        if (!queue->head) {
            queue->tail = NULL;
        }

        queue->count--;
        queue->total_dequeued++;

        if (queue->count == queue->capacity - 1) {
            pthread_cond_signal(&queue->not_full);
        }
    }

    pthread_mutex_unlock(&queue->lock);
#endif

    return request;
}

size_t async_queue_get_count(async_queue_t* queue) {
    if (!queue) return 0;

    size_t count;
#ifdef _WIN32
    EnterCriticalSection(&queue->lock);
    count = queue->count;
    LeaveCriticalSection(&queue->lock);
#else
    pthread_mutex_lock(&queue->lock);
    count = queue->count;
    pthread_mutex_unlock(&queue->lock);
#endif

    return count;
}

int async_queue_is_shutting_down(async_queue_t* queue) {
    if (!queue) return 1;
    return queue->shutdown;
}
