#include "async_storage_queue.h"
#include "agentos.h"
#include "platform.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

struct async_queue {
    write_request_t* head;
    write_request_t* tail;
    size_t count;
    size_t capacity;
    uint64_t total_enqueued;
    uint64_t total_dequeued;
    agentos_mutex_t lock;
    agentos_cond_t not_empty;
    agentos_cond_t not_full;
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

    if (agentos_mutex_init(&queue->lock) != 0) {
        AGENTOS_LOG_ERROR("Failed to init mutex");
        AGENTOS_FREE(queue);
        return NULL;
    }

    if (agentos_cond_init(&queue->not_empty) != 0) {
        AGENTOS_LOG_ERROR("Failed to init cond not_empty");
        agentos_mutex_destroy(&queue->lock);
        AGENTOS_FREE(queue);
        return NULL;
    }

    if (agentos_cond_init(&queue->not_full) != 0) {
        AGENTOS_LOG_ERROR("Failed to init cond not_full");
        agentos_cond_destroy(&queue->not_empty);
        agentos_mutex_destroy(&queue->lock);
        AGENTOS_FREE(queue);
        return NULL;
    }

    return queue;
}

void async_queue_destroy(async_queue_t* queue) {
    if (!queue) return;

    queue->shutdown = 1;

    agentos_cond_broadcast(&queue->not_empty);
    agentos_cond_broadcast(&queue->not_full);

    agentos_mutex_lock(&queue->lock);

    write_request_t* request = queue->head;
    while (request) {
        write_request_t* next = request->next;
        if (request->id) AGENTOS_FREE(request->id);
        if (request->data) AGENTOS_FREE(request->data);
        AGENTOS_FREE(request);
        request = next;
    }

    agentos_mutex_unlock(&queue->lock);

    agentos_mutex_destroy(&queue->lock);
    agentos_cond_destroy(&queue->not_empty);
    agentos_cond_destroy(&queue->not_full);

    AGENTOS_FREE(queue);
}

agentos_error_t async_queue_enqueue(async_queue_t* queue, write_request_t* request,
                                     uint32_t timeout_ms) {
    if (!queue || !request) return AGENTOS_EINVAL;

    agentos_mutex_lock(&queue->lock);

    while (queue->count >= queue->capacity && !queue->shutdown) {
        if (timeout_ms == 0) {
            agentos_mutex_unlock(&queue->lock);
            return AGENTOS_EBUSY;
        }

        int wait_ret = agentos_cond_timedwait(&queue->not_full, &queue->lock, timeout_ms);

        if (wait_ret != 0 && !queue->shutdown) {
            agentos_mutex_unlock(&queue->lock);
            return AGENTOS_ETIMEDOUT;
        }

        if (queue->shutdown) {
            agentos_mutex_unlock(&queue->lock);
            return AGENTOS_ESHUTDOWN;
        }
    }

    if (queue->shutdown) {
        agentos_mutex_unlock(&queue->lock);
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
        agentos_cond_signal(&queue->not_empty);
    }

    agentos_mutex_unlock(&queue->lock);

    return AGENTOS_SUCCESS;
}

write_request_t* async_queue_dequeue(async_queue_t* queue, uint32_t timeout_ms) {
    if (!queue) return NULL;

    agentos_mutex_lock(&queue->lock);

    while (queue->count == 0 && !queue->shutdown) {
        if (timeout_ms == 0) {
            agentos_mutex_unlock(&queue->lock);
            return NULL;
        }

        int wait_ret = agentos_cond_timedwait(&queue->not_empty, &queue->lock, timeout_ms);

        if (wait_ret != 0 && !queue->shutdown) {
            agentos_mutex_unlock(&queue->lock);
            return NULL;
        }

        if (queue->shutdown) {
            agentos_mutex_unlock(&queue->lock);
            return NULL;
        }
    }

    if (queue->shutdown || queue->count == 0) {
        agentos_mutex_unlock(&queue->lock);
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
            agentos_cond_signal(&queue->not_full);
        }
    }

    agentos_mutex_unlock(&queue->lock);

    return request;
}

size_t async_queue_get_count(async_queue_t* queue) {
    if (!queue) return 0;

    agentos_mutex_lock(&queue->lock);
    size_t count = queue->count;
    agentos_mutex_unlock(&queue->lock);

    return count;
}
