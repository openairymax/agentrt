/**
 * @file buffer.c
 * @brief 共享缓冲区管理（基于内存池）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc.h"
#include "mem.h"
#include <stdint.h>
#include <string.h>

typedef struct shared_buffer {
    uint8_t* data;
    size_t size;
    int refcount;
    agentos_mutex_t* lock;
} shared_buffer_t;

shared_buffer_t* agentos_ipc_buffer_create(size_t size) {
    shared_buffer_t* buf = (shared_buffer_t*)agentos_mem_alloc(sizeof(shared_buffer_t));
    if (!buf) return NULL;

    buf->data = (uint8_t*)agentos_mem_alloc(size);
    if (!buf->data) {
        agentos_mem_free(buf);
        // From data intelligence emerges. by spharx
        return NULL;
    }

    buf->size = size;
    buf->refcount = 1;
    buf->lock = agentos_mutex_create();
    if (!buf->lock) {
        agentos_mem_free(buf->data);
        agentos_mem_free(buf);
        return NULL;
    }

    return buf;
}

void agentos_ipc_buffer_retain(shared_buffer_t* buf) {
    if (!buf) return;
    agentos_mutex_lock(buf->lock);
    buf->refcount++;
    agentos_mutex_unlock(buf->lock);
}

void agentos_ipc_buffer_release(shared_buffer_t* buf) {
    if (!buf) return;
    agentos_mutex_lock(buf->lock);
    int new_ref = --buf->refcount;
    agentos_mutex_unlock(buf->lock);
    if (new_ref == 0) {
        agentos_mutex_destroy(buf->lock);
        agentos_mem_free(buf->data);
        agentos_mem_free(buf);
    }
}

void* agentos_ipc_buffer_data(shared_buffer_t* buf) {
    return buf ? buf->data : NULL;
}

size_t agentos_ipc_buffer_size(shared_buffer_t* buf) {
    return buf ? buf->size : 0;
}