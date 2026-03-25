/**
 * @file ipc.h
 * @brief 内核 IPC 接口定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_IPC_H
#define AGENTOS_IPC_H

#include <stdint.h>
#include <stddef.h>
#include "error.h"
#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct agentos_ipc_channel agentos_ipc_channel_t;
typedef struct agentos_ipc_buffer agentos_ipc_buffer_t;

typedef struct {
    uint32_t    code;
    const void* data;
    size_t      size;
    int32_t     fd;
    uint64_t    msg_id;
} agentos_ipc_message_t;

typedef agentos_error_t (*agentos_ipc_callback_t)(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg,
    void* userdata);

AGENTOS_API agentos_error_t agentos_ipc_init(void);

AGENTOS_API void agentos_ipc_cleanup(void);

AGENTOS_API agentos_error_t agentos_ipc_create_channel(
    const char* name,
    agentos_ipc_callback_t callback,
    void* userdata,
    agentos_ipc_channel_t** out_channel);

AGENTOS_API agentos_error_t agentos_ipc_connect(
    const char* name,
    agentos_ipc_channel_t** out_channel);

AGENTOS_API agentos_error_t agentos_ipc_call(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg,
    void* response,
    size_t* response_size,
    uint32_t timeout_ms);

AGENTOS_API agentos_error_t agentos_ipc_send(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg);

AGENTOS_API agentos_error_t agentos_ipc_reply(
    agentos_ipc_channel_t* channel,
    const agentos_ipc_message_t* msg);

AGENTOS_API void agentos_ipc_close(agentos_ipc_channel_t* channel);

AGENTOS_API agentos_ipc_buffer_t* agentos_ipc_buffer_create(size_t capacity);

AGENTOS_API void agentos_ipc_buffer_destroy(agentos_ipc_buffer_t* buf);

AGENTOS_API agentos_error_t agentos_ipc_buffer_write(
    agentos_ipc_buffer_t* buf,
    const void* data,
    size_t size);

AGENTOS_API agentos_error_t agentos_ipc_buffer_read(
    agentos_ipc_buffer_t* buf,
    void* out_data,
    size_t size,
    size_t* out_read);

AGENTOS_API size_t agentos_ipc_buffer_available(agentos_ipc_buffer_t* buf);

AGENTOS_API void agentos_ipc_buffer_reset(agentos_ipc_buffer_t* buf);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_IPC_H */
