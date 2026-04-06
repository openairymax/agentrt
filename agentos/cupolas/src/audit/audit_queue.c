/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * audit_queue.c - Audit Log Queue Implementation: Thread-safe Producer-Consumer Queue
 */

/**
 * @file audit_queue.c
 * @brief Audit Log Queue Implementation - Thread-safe Producer-Consumer Queue
 * @author Spharx
 * @date 2024
 */

#include "audit_queue.h"
#include "../../utils/cupolas_utils.h"
#include <stdlib.h>
#include <string.h>

audit_entry_t* audit_entry_create(audit_event_type_t type,
                                   const char* agent_id,
                                   const char* action,
                                   const char* resource,
                                   const char* detail,
                                   int result) {
    audit_entry_t* entry = (audit_entry_t*)cupolas_mem_alloc(sizeof(audit_entry_t));
    if (!entry) return NULL;
    
    memset(entry, 0, sizeof(audit_entry_t));
    
    entry->timestamp_ms = cupolas_time_ms();
    entry->type = type;
    entry->result = result;
    
    if (agent_id) {
        entry->agent_id = cupolas_strdup(agent_id);
        if (!entry->agent_id) goto error;
    }
    if (action) {
        entry->action = cupolas_strdup(action);
        if (!entry->action) goto error;
    }
    if (resource) {
        entry->resource = cupolas_strdup(resource);
        if (!entry->resource) goto error;
    }
    if (detail) {
        entry->detail = cupolas_strdup(detail);
        if (!entry->detail) goto error;
    }
    
    return entry;
    
error:
    audit_entry_destroy(entry);
    return NULL;
}

void audit_entry_destroy(audit_entry_t* entry) {
    if (!entry) return;
    
    cupolas_mem_free(entry->agent_id);
    cupolas_mem_free(entry->action);
    cupolas_mem_free(entry->resource);
    cupolas_mem_free(entry->detail);
    cupolas_mem_free(entry);
}

audit_queue_t* audit_queue_create(size_t max_size) {
    audit_queue_t* queue = (audit_queue_t*)cupolas_mem_alloc(sizeof(audit_queue_t));
    if (!queue) return NULL;
    
    memset(queue, 0, sizeof(audit_queue_t));
    queue->max_size = max_size;
    
    if (cupolas_mutex_init(&queue->lock) != cupolas_OK) {
        cupolas_mem_free(queue);
        return NULL;
    }
    
    if (cupolas_cond_init(&queue->not_empty) != cupolas_OK) {
        cupolas_mutex_destroy(&queue->lock);
        cupolas_mem_free(queue);
        return NULL;
    }
    
    if (cupolas_cond_init(&queue->not_full) != cupolas_OK) {
        cupolas_cond_destroy(&queue->not_empty);
        cupolas_mutex_destroy(&queue->lock);
        cupolas_mem_free(queue);
        return NULL;
    }
    
    return queue;
}

void audit_queue_destroy(audit_queue_t* queue) {
    if (!queue) return;
    
    cupolas_mutex_lock(&queue->lock);
    queue->shutdown = true;
    cupolas_cond_broadcast(&queue->not_empty);
    cupolas_cond_broadcast(&queue->not_full);
    
    audit_entry_t* entry = queue->head;
    while (entry) {
        audit_entry_t* next = entry->next;
        audit_entry_destroy(entry);
        entry = next;
    }
    
    cupolas_mutex_unlock(&queue->lock);
    
    cupolas_cond_destroy(&queue->not_full);
    cupolas_cond_destroy(&queue->not_empty);
    cupolas_mutex_destroy(&queue->lock);
    cupolas_mem_free(queue);
}

int audit_queue_push(audit_queue_t* queue, audit_entry_t* entry) {
    if (!queue || !entry) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&queue->lock);
    
    while (queue->max_size > 0 && queue->size >= queue->max_size && !queue->shutdown) {
        cupolas_cond_wait(&queue->not_full, &queue->lock);
    }
    
    if (queue->shutdown) {
        cupolas_mutex_unlock(&queue->lock);
        return cupolas_ERROR_UNKNOWN;
    }
    
    entry->next = NULL;
    if (queue->tail) {
        queue->tail->next = entry;
    } else {
        queue->head = entry;
    }
    queue->tail = entry;
    queue->size++;
    
    cupolas_atomic_add64(&queue->total_pushed, 1);
    
    cupolas_cond_signal(&queue->not_empty);
    cupolas_mutex_unlock(&queue->lock);
    
    return cupolas_OK;
}

int audit_queue_try_push(audit_queue_t* queue, audit_entry_t* entry) {
    if (!queue || !entry) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&queue->lock);
    
    if (queue->shutdown) {
        cupolas_mutex_unlock(&queue->lock);
        return cupolas_ERROR_UNKNOWN;
    }
    
    if (queue->max_size > 0 && queue->size >= queue->max_size) {
        cupolas_mutex_unlock(&queue->lock);
        return cupolas_ERROR_WOULD_BLOCK;
    }
    
    entry->next = NULL;
    if (queue->tail) {
        queue->tail->next = entry;
    } else {
        queue->head = entry;
    }
    queue->tail = entry;
    queue->size++;
    
    cupolas_atomic_add64(&queue->total_pushed, 1);
    
    cupolas_cond_signal(&queue->not_empty);
    cupolas_mutex_unlock(&queue->lock);
    
    return cupolas_OK;
}

int audit_queue_pop(audit_queue_t* queue, audit_entry_t** entry) {
    if (!queue || !entry) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&queue->lock);
    
    while (queue->size == 0 && !queue->shutdown) {
        cupolas_cond_wait(&queue->not_empty, &queue->lock);
    }
    
    if (queue->size == 0) {
        cupolas_mutex_unlock(&queue->lock);
        return cupolas_ERROR_UNKNOWN;
    }
    
    *entry = queue->head;
    queue->head = (*entry)->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    
    cupolas_atomic_add64(&queue->total_popped, 1);
    
    cupolas_cond_signal(&queue->not_full);
    cupolas_mutex_unlock(&queue->lock);
    
    return cupolas_OK;
}

int audit_queue_timed_pop(audit_queue_t* queue, audit_entry_t** entry, uint32_t timeout_ms) {
    if (!queue || !entry) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&queue->lock);
    
    while (queue->size == 0 && !queue->shutdown) {
        int ret = cupolas_cond_timedwait(&queue->not_empty, &queue->lock, timeout_ms);
        if (ret == cupolas_ERROR_TIMEOUT) {
            cupolas_mutex_unlock(&queue->lock);
            return cupolas_ERROR_TIMEOUT;
        }
    }
    
    if (queue->size == 0) {
        cupolas_mutex_unlock(&queue->lock);
        return cupolas_ERROR_UNKNOWN;
    }
    
    *entry = queue->head;
    queue->head = (*entry)->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    
    cupolas_atomic_add64(&queue->total_popped, 1);
    
    cupolas_cond_signal(&queue->not_full);
    cupolas_mutex_unlock(&queue->lock);
    
    return cupolas_OK;
}

int audit_queue_try_pop(audit_queue_t* queue, audit_entry_t** entry) {
    if (!queue || !entry) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&queue->lock);
    
    if (queue->size == 0) {
        cupolas_mutex_unlock(&queue->lock);
        return cupolas_ERROR_WOULD_BLOCK;
    }
    
    *entry = queue->head;
    queue->head = (*entry)->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    
    cupolas_atomic_add64(&queue->total_popped, 1);
    
    cupolas_cond_signal(&queue->not_full);
    cupolas_mutex_unlock(&queue->lock);
    
    return cupolas_OK;
}

int audit_queue_pop_batch(audit_queue_t* queue, audit_entry_t** entries, 
                           size_t max_count, size_t* actual_count) {
    if (!queue || !entries || !actual_count) return cupolas_ERROR_INVALID_ARG;
    
    cupolas_mutex_lock(&queue->lock);
    
    while (queue->size == 0 && !queue->shutdown) {
        cupolas_cond_wait(&queue->not_empty, &queue->lock);
    }
    
    if (queue->size == 0) {
        *actual_count = 0;
        cupolas_mutex_unlock(&queue->lock);
        return cupolas_ERROR_UNKNOWN;
    }
    
    size_t count = 0;
    while (count < max_count && queue->head) {
        entries[count] = queue->head;
        queue->head = entries[count]->next;
        count++;
        queue->size--;
        cupolas_atomic_add64(&queue->total_popped, 1);
    }
    
    if (!queue->head) {
        queue->tail = NULL;
    }
    
    *actual_count = count;
    
    cupolas_cond_broadcast(&queue->not_full);
    cupolas_mutex_unlock(&queue->lock);
    
    return cupolas_OK;
}

void audit_queue_shutdown(audit_queue_t* queue, bool wait_empty) {
    if (!queue) return;
    
    cupolas_mutex_lock(&queue->lock);
    
    if (wait_empty) {
        while (queue->size > 0) {
            cupolas_cond_broadcast(&queue->not_empty);
            cupolas_mutex_unlock(&queue->lock);
            cupolas_sleep_ms(10);
            cupolas_mutex_lock(&queue->lock);
        }
    }
    
    queue->shutdown = true;
    cupolas_cond_broadcast(&queue->not_empty);
    cupolas_cond_broadcast(&queue->not_full);
    cupolas_mutex_unlock(&queue->lock);
}

size_t audit_queue_size(audit_queue_t* queue) {
    if (!queue) return 0;
    
    cupolas_mutex_lock(&queue->lock);
    size_t size = queue->size;
    cupolas_mutex_unlock(&queue->lock);
    
    return size;
}

void audit_queue_stats(audit_queue_t* queue, uint64_t* total_pushed, uint64_t* total_popped) {
    if (!queue) {
        if (total_pushed) *total_pushed = 0;
        if (total_popped) *total_popped = 0;
        return;
    }
    
    if (total_pushed) *total_pushed = cupolas_atomic_load64(&queue->total_pushed);
    if (total_popped) *total_popped = cupolas_atomic_load64(&queue->total_popped);
}
