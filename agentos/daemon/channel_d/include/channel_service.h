/**
 * @file channel_service.h
 * @brief UnifiedChannel 统一通道服务 API
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * IMP-08: 统一通道服务，支持 SOCKET/SHM 通道类型
 */

#ifndef AGENTOS_CHANNEL_SERVICE_H
#define AGENTOS_CHANNEL_SERVICE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHANNEL_MAX_ID       128
#define CHANNEL_MAX_NAME     256
#define CHANNEL_MAX_TYPE     32
#define CHANNEL_MAX_STATUS   32
#define CHANNEL_MAX_ENDPOINT 512
#define CHANNEL_MAX_CHANNELS 256

typedef enum {
    CHANNEL_TYPE_SOCKET = 0,
    CHANNEL_TYPE_SHM = 1,
    CHANNEL_TYPE_PIPE = 2,
    CHANNEL_TYPE_UNKNOWN = 99
} channel_type_t;

typedef enum {
    CHANNEL_STATUS_CLOSED = 0,
    CHANNEL_STATUS_OPEN = 1,
    CHANNEL_STATUS_ERROR = 2,
    CHANNEL_STATUS_DRAINING = 3
} channel_status_t;

typedef struct {
    char channel_id[CHANNEL_MAX_ID];
    char name[CHANNEL_MAX_NAME];
    channel_type_t type;
    channel_status_t status;
    char endpoint[CHANNEL_MAX_ENDPOINT];
    uint64_t created_at;
    uint64_t last_activity;
    size_t buffer_size;
    size_t messages_sent;
    size_t messages_received;
} channel_info_t;

typedef struct channel_service channel_service_t;

typedef struct {
    uint32_t max_channels;
    uint32_t default_buffer_size;
    uint32_t socket_backlog;
    char socket_dir[512];
    char shm_prefix[64];
    uint32_t idle_timeout_ms;
} channel_config_t;

#define CHANNEL_CONFIG_DEFAULTS { \
    .max_channels = CHANNEL_MAX_CHANNELS, \
    .default_buffer_size = 65536, \
    .socket_backlog = 128, \
    .socket_dir = "/tmp/agentos/channels", \
    .shm_prefix = "/agentos_ch_", \
    .idle_timeout_ms = 30000 }

typedef void (*channel_message_cb_t)(
    const char* channel_id,
    const void* data,
    size_t data_len,
    void* user_data);

channel_service_t* channel_service_create(const channel_config_t* config);
void channel_service_destroy(channel_service_t* svc);

int channel_service_start(channel_service_t* svc);
int channel_service_stop(channel_service_t* svc);

int channel_service_open(channel_service_t* svc,
                          const char* channel_id,
                          const char* name,
                          channel_type_t type,
                          const char* endpoint);

int channel_service_close(channel_service_t* svc, const char* channel_id);

int channel_service_send(channel_service_t* svc,
                           const char* channel_id,
                           const void* data,
                           size_t data_len);

int channel_service_receive(channel_service_t* svc,
                              const char* channel_id,
                              void** out_data,
                              size_t* out_len);

int channel_service_list(channel_service_t* svc,
                           channel_info_t* out_list,
                           size_t list_capacity,
                           size_t* out_count);

int channel_service_get_info(channel_service_t* svc,
                               const char* channel_id,
                               channel_info_t* out_info);

int channel_service_set_callback(channel_service_t* svc,
                                   const char* channel_id,
                                   channel_message_cb_t callback,
                                   void* user_data);

bool channel_service_is_healthy(channel_service_t* svc);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CHANNEL_SERVICE_H */
