/**
 * @file audit_queue.c
 * @brief 审计日志队列实现 - 线程安全的生产者-消费者队列
 * @author Spharx
 * @date 2024
 */

#include "audit_queue.h"
#include <stdlib.h>
#include <string.h>

audit_entry_t* audit_entry_create(audit_event_type_t type,
                                   const char* agent_id,
                                   const char* action,
                                   const char* resource,
                                   const char* detail,
                                   int result) {
    audit_entry_t* entry = (audit_entry_t*)domes_mem_alloc(sizeof(audit_entry_t));
    if (!entry) return NULL;
    
    memset(entry, 0, sizeof(audit_entry_t));
    
    entry->timestamp_ms = domes_time_ms();
    entry->type = type;
    entry->result = result;
    
    if (agent_id) {
        entry->agent_id = domes_strdup(agent_id);
        if (!entry->agent_id) goto error;
    }
    if (action) {
        entry->action = domes_strdup(action);
        if (!entry->action) goto error;
    }
    if (resource) {
        entry->resource = domes_strdup(resource);
        if (!entry->resource) goto error;
    }
    if (detail) {
        entry->detail = domes_strdup(detail);
        if (!entry->detail) goto error;
    }
    
    return entry;
    
error:
    audit_entry_destroy(entry);
    return NULL;
}

void audit_entry_destroy(audit_entry_t* entry) {
    if (!entry) return;
    
    domes_mem_free(entry->agent_id);
    domes_mem_free(entry->action);
    domes_mem_free(entry->resource);
    domes_mem_free(entry->detail);
    domes_mem_free(entry);
}

audit_queue_t* audit_queue_create(size_t max_size) {
    audit_queue_t* queue = (audit_queue_t*)domes_mem_alloc(sizeof(audit_queue_t));
    if (!queue) return NULL;
    
    memset(queue, 0, sizeof(audit_queue_t));
    queue->max_size = max_size;
    
    if (domes_mutex_init(&queue->lock) != DOMES_OK) {
        domes_mem_free(queue);
        return NULL;
    }
    
    if (domes_cond_init(&queue->not_empty) != DOMES_OK) {
        domes_mutex_destroy(&queue->lock);
        domes_mem_free(queue);
        return NULL;
    }
    
    if (domes_cond_init(&queue->not_full) != DOMES_OK) {
        domes_cond_destroy(&queue->not_empty);
        domes_mutex_destroy(&queue->lock);
        domes_mem_free(queue);
        return NULL;
    }
    
    return queue;
}

void audit_queue_destroy(audit_queue_t* queue) {
    if (!queue) return;
    
    domes_mutex_lock(&queue->lock);
    queue->shutdown = true;
    domes_cond_broadcast(&queue->not_empty);
    domes_cond_broadcast(&queue->not_full);
    
    audit_entry_t* entry = queue->head;
    while (entry) {
        audit_entry_t* next = entry->next;
        audit_entry_destroy(entry);
        entry = next;
    }
    
    domes_mutex_unlock(&queue->lock);
    
    domes_cond_destroy(&queue->not_full);
    domes_cond_destroy(&queue->not_empty);
    domes_mutex_destroy(&queue->lock);
    domes_mem_free(queue);
}

int audit_queue_push(audit_queue_t* queue, audit_entry_t* entry) {
    if (!queue || !entry) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&queue->lock);
    
    while (queue->max_size > 0 && queue->size >= queue->max_size && !queue->shutdown) {
        domes_cond_wait(&queue->not_full, &queue->lock);
    }
    
    if (queue->shutdown) {
        domes_mutex_unlock(&queue->lock);
        return DOMES_ERROR_UNKNOWN;
    }
    
    entry->next = NULL;
    if (queue->tail) {
        queue->tail->next = entry;
    } else {
        queue->head = entry;
    }
    queue->tail = entry;
    queue->size++;
    
    domes_atomic_add64(&queue->total_pushed, 1);
    
    domes_cond_signal(&queue->not_empty);
    domes_mutex_unlock(&queue->lock);
    
    return DOMES_OK;
}

int audit_queue_try_push(audit_queue_t* queue, audit_entry_t* entry) {
    if (!queue || !entry) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&queue->lock);
    
    if (queue->shutdown) {
        domes_mutex_unlock(&queue->lock);
        return DOMES_ERROR_UNKNOWN;
    }
    
    if (queue->max_size > 0 && queue->size >= queue->max_size) {
        domes_mutex_unlock(&queue->lock);
        return DOMES_ERROR_WOULD_BLOCK;
    }
    
    entry->next = NULL;
    if (queue->tail) {
        queue->tail->next = entry;
    } else {
        queue->head = entry;
    }
    queue->tail = entry;
    queue->size++;
    
    domes_atomic_add64(&queue->total_pushed, 1);
    
    domes_cond_signal(&queue->not_empty);
    domes_mutex_unlock(&queue->lock);
    
    return DOMES_OK;
}

int audit_queue_pop(audit_queue_t* queue, audit_entry_t** entry) {
    if (!queue || !entry) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&queue->lock);
    
    while (queue->size == 0 && !queue->shutdown) {
        domes_cond_wait(&queue->not_empty, &queue->lock);
    }
    
    if (queue->size == 0) {
        domes_mutex_unlock(&queue->lock);
        return DOMES_ERROR_UNKNOWN;
    }
    
    *entry = queue->head;
    queue->head = (*entry)->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    
    domes_atomic_add64(&queue->total_popped, 1);
    
    domes_cond_signal(&queue->not_full);
    domes_mutex_unlock(&queue->lock);
    
    return DOMES_OK;
}

int audit_queue_timed_pop(audit_queue_t* queue, audit_entry_t** entry, uint32_t timeout_ms) {
    if (!queue || !entry) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&queue->lock);
    
    while (queue->size == 0 && !queue->shutdown) {
        int ret = domes_cond_timedwait(&queue->not_empty, &queue->lock, timeout_ms);
        if (ret == DOMES_ERROR_TIMEOUT) {
            domes_mutex_unlock(&queue->lock);
            return DOMES_ERROR_TIMEOUT;
        }
    }
    
    if (queue->size == 0) {
        domes_mutex_unlock(&queue->lock);
        return DOMES_ERROR_UNKNOWN;
    }
    
    *entry = queue->head;
    queue->head = (*entry)->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    
    domes_atomic_add64(&queue->total_popped, 1);
    
    domes_cond_signal(&queue->not_full);
    domes_mutex_unlock(&queue->lock);
    
    return DOMES_OK;
}

int audit_queue_try_pop(audit_queue_t* queue, audit_entry_t** entry) {
    if (!queue || !entry) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&queue->lock);
    
    if (queue->size == 0) {
        domes_mutex_unlock(&queue->lock);
        return DOMES_ERROR_WOULD_BLOCK;
    }
    
    *entry = queue->head;
    queue->head = (*entry)->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    
    domes_atomic_add64(&queue->total_popped, 1);
    
    domes_cond_signal(&queue->not_full);
    domes_mutex_unlock(&queue->lock);
    
    return DOMES_OK;
}

int audit_queue_pop_batch(audit_queue_t* queue, audit_entry_t** entries, 
                           size_t max_count, size_t* actual_count) {
    if (!queue || !entries || !actual_count) return DOMES_ERROR_INVALID_ARG;
    
    domes_mutex_lock(&queue->lock);
    
    while (queue->size == 0 && !queue->shutdown) {
        domes_cond_wait(&queue->not_empty, &queue->lock);
    }
    
    if (queue->size == 0) {
        *actual_count = 0;
        domes_mutex_unlock(&queue->lock);
        return DOMES_ERROR_UNKNOWN;
    }
    
    size_t count = 0;
    while (count < max_count && queue->head) {
        entries[count] = queue->head;
        queue->head = entries[count]->next;
        count++;
        queue->size--;
        domes_atomic_add64(&queue->total_popped, 1);
    }
    
    if (!queue->head) {
        queue->tail = NULL;
    }
    
    *actual_count = count;
    
    domes_cond_broadcast(&queue->not_full);
    domes_mutex_unlock(&queue->lock);
    
    return DOMES_OK;
}

void audit_queue_shutdown(audit_queue_t* queue, bool wait_empty) {
    if (!queue) return;
    
    domes_mutex_lock(&queue->lock);
    
    if (wait_empty) {
        while (queue->size > 0) {
            domes_cond_broadcast(&queue->not_empty);
            domes_mutex_unlock(&queue->lock);
            domes_sleep_ms(10);
            domes_mutex_lock(&queue->lock);
        }
    }
    
    queue->shutdown = true;
    domes_cond_broadcast(&queue->not_empty);
    domes_cond_broadcast(&queue->not_full);
    domes_mutex_unlock(&queue->lock);
}

size_t audit_queue_size(audit_queue_t* queue) {
    if (!queue) return 0;
    
    domes_mutex_lock(&queue->lock);
    size_t size = queue->size;
    domes_mutex_unlock(&queue->lock);
    
    return size;
}

void audit_queue_stats(audit_queue_t* queue, uint64_t* total_pushed, uint64_t* total_popped) {
    if (!queue) {
        if (total_pushed) *total_pushed = 0;
        if (total_popped) *total_popped = 0;
        return;
    }
    
    if (total_pushed) *total_pushed = domes_atomic_load64(&queue->total_pushed);
    if (total_popped) *total_popped = domes_atomic_load64(&queue->total_popped);
}
