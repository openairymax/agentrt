/**
 * @file buffer.c
 * @brief IPC 消息缓冲�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc.h"
#include "mem.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

struct agentos_ipc_buffer {
    uint8_t* data;
    size_t capacity;
    size_t used;
};

agentos_ipc_buffer_t* agentos_ipc_buffer_create(size_t capacity) {
    agentos_ipc_buffer_t* buf = (agentos_ipc_buffer_t*)AGENTOS_CALLOC(1, sizeof(agentos_ipc_buffer_t));
    if (!buf) return NULL;

    buf->data = (uint8_t*)agentos_mem_alloc(capacity);
    if (!buf->data) {
        AGENTOS_FREE(buf);
        return NULL;
    }
    buf->capacity = capacity;
    buf->used = 0;
    return buf;
}

void agentos_ipc_buffer_destroy(agentos_ipc_buffer_t* buf) {
    if (!buf) return;
    if (buf->data) agentos_mem_free(buf->data);
    AGENTOS_FREE(buf);
}

agentos_error_t agentos_ipc_buffer_write(
    agentos_ipc_buffer_t* buf,
    const void* data,
    size_t size) {

    if (!buf || !data) return AGENTOS_EINVAL;
    if (buf->used + size > buf->capacity) return AGENTOS_ENOMEM;

    memcpy(buf->data + buf->used, data, size);
    buf->used += size;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_ipc_buffer_read(
    agentos_ipc_buffer_t* buf,
    void* out_data,
    size_t size,
    size_t* out_read) {

    if (!buf || !out_data) return AGENTOS_EINVAL;
    size_t to_read = (size < buf->used) ? size : buf->used;
    memcpy(out_data, buf->data, to_read);
    if (out_read) *out_read = to_read;
    return AGENTOS_SUCCESS;
}

size_t agentos_ipc_buffer_available(agentos_ipc_buffer_t* buf) {
    return buf ? (buf->capacity - buf->used) : 0;
}

void agentos_ipc_buffer_reset(agentos_ipc_buffer_t* buf) {
    if (buf) buf->used = 0;
}
