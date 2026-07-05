/**
 * @file buffer.c
 * @brief IPC 消息缓冲?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc.h"
#include "mem.h"

#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"

#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


struct agentrt_ipc_buffer {
    uint8_t *data;
    size_t capacity;
    size_t used;
};

agentrt_ipc_buffer_t *agentrt_ipc_buffer_create(size_t capacity)
{
    agentrt_ipc_buffer_t *buf =
        (agentrt_ipc_buffer_t *)AGENTRT_CALLOC(1, sizeof(agentrt_ipc_buffer_t));
    if (!buf) return NULL;

    buf->data = (uint8_t *)agentrt_mem_alloc(capacity);
    if (!buf->data) {
        AGENTRT_FREE(buf);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    buf->capacity = capacity;
    buf->used = 0;
    return buf;
}

void agentrt_ipc_buffer_destroy(agentrt_ipc_buffer_t *buf)
{
    if (!buf)
        return;
    if (buf->data)
        agentrt_mem_free(buf->data);
    AGENTRT_FREE(buf);
}

agentrt_error_t agentrt_ipc_buffer_write(agentrt_ipc_buffer_t *buf, const void *data, size_t size)
{

    if (!buf || !data)
        ATM_RET_ERR(AGENTRT_EINVAL);
    if (buf->used + size > buf->capacity)
        ATM_RET_ERR(AGENTRT_ENOMEM);

    __builtin_memcpy(buf->data + buf->used, data, size);
    buf->used += size;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_ipc_buffer_read(agentrt_ipc_buffer_t *buf, void *out_data, size_t size,
                                        size_t *out_read)
{

    if (!buf || !out_data)
        ATM_RET_ERR(AGENTRT_EINVAL);
    size_t to_read = (size < buf->used) ? size : buf->used;
    __builtin_memcpy(out_data, buf->data, to_read);
    if (out_read)
        *out_read = to_read;
    return AGENTRT_SUCCESS;
}

size_t agentrt_ipc_buffer_available(agentrt_ipc_buffer_t *buf)
{
    return buf ? (buf->capacity - buf->used) : 0;
}

void agentrt_ipc_buffer_reset(agentrt_ipc_buffer_t *buf)
{
    if (buf)
        buf->used = 0;
}
