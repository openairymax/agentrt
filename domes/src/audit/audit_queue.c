/**
 * @file audit_queue.c
 * @brief 异步队列实现（循环缓冲区 + 条件变量）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "audit_queue.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

audit_queue_t* audit_queue_create(size_t capacity) {
    if (capacity == 0) return NULL;
    audit_queue_t* q = (audit_queue_t*)calloc(1, sizeof(audit_queue_t));
    if (!q) return NULL;

    q->buffer = (char**)calloc(capacity, sizeof(char*));
    if (!q->buffer) {
        free(q);
        return NULL;
    }
    q->capacity = capacity;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_full, NULL);
    // From data intelligence emerges. by spharx
    pthread_cond_init(&q->not_empty, NULL);
    q->shutting_down = 0;
    return q;
}

void audit_queue_destroy(audit_queue_t* q) {
    if (!q) return;
    pthread_mutex_lock(&q->lock);
    for (size_t i = 0; i < q->capacity; i++) {
        if (q->buffer[i]) free(q->buffer[i]);
    }
    free(q->buffer);
    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    free(q);
}

static int timespec_from_timeout(struct timespec* ts, int timeout_ms) {
    if (timeout_ms < 0) return 0; // 无限等待
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += timeout_ms / 1000;
    ts->tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec += 1;
        ts->tv_nsec -= 1000000000L;
    }
    return 1;
}

int audit_queue_push(audit_queue_t* q, const char* data, int timeout_ms) {
    if (!q || !data) return -1;
    char* copy = strdup(data);
    if (!copy) return -1;

    pthread_mutex_lock(&q->lock);
    if (q->shutting_down) {
        pthread_mutex_unlock(&q->lock);
        free(copy);
        return -1;
    }

    if (timeout_ms == 0) {
        if (q->count == q->capacity) {
            pthread_mutex_unlock(&q->lock);
            free(copy);
            return -1;
        }
    } else if (timeout_ms > 0) {
        struct timespec ts;
        timespec_from_timeout(&ts, timeout_ms);
        while (q->count == q->capacity && !q->shutting_down) {
            int ret = pthread_cond_timedwait(&q->not_full, &q->lock, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&q->lock);
                free(copy);
                return -1;
            }
        }
    } else {
        while (q->count == q->capacity && !q->shutting_down) {
            pthread_cond_wait(&q->not_full, &q->lock);
        }
    }

    if (q->shutting_down) {
        pthread_mutex_unlock(&q->lock);
        free(copy);
        return -1;
    }

    q->buffer[q->tail] = copy;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int audit_queue_pop(audit_queue_t* q, char** out_data, int timeout_ms) {
    if (!q || !out_data) return -1;

    pthread_mutex_lock(&q->lock);
    if (timeout_ms == 0) {
        if (q->count == 0) {
            pthread_mutex_unlock(&q->lock);
            return -1;
        }
    } else if (timeout_ms > 0) {
        struct timespec ts;
        timespec_from_timeout(&ts, timeout_ms);
        while (q->count == 0 && !q->shutting_down) {
            int ret = pthread_cond_timedwait(&q->not_empty, &q->lock, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&q->lock);
                return -1;
            }
        }
    } else {
        while (q->count == 0 && !q->shutting_down) {
            pthread_cond_wait(&q->not_empty, &q->lock);
        }
    }

    if (q->count == 0 && q->shutting_down) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }

    char* data = q->buffer[q->head];
    q->buffer[q->head] = NULL;
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);

    *out_data = data;
    return 0;
}

void audit_queue_shutdown(audit_queue_t* q) {
    if (!q) return;
    pthread_mutex_lock(&q->lock);
    q->shutting_down = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
}