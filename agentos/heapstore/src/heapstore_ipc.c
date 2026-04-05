/**
 * @file heapstore_ipc.c
 * @brief AgentOS 数据分区 IPC 数据存储实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "heapstore_ipc.h"
#include "private.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

#define heapstore_IPC_MAX_CHANNELS 256
#define heapstore_IPC_MAX_BUFFERS 1024
#define heapstore_IPC_MAX_PATH 512

static bool s_initialized = false;
static pthread_mutex_t s_ipc_lock = PTHREAD_MUTEX_INITIALIZER;
static heapstore_ipc_channel_t s_channels[heapstore_IPC_MAX_CHANNELS];
static size_t s_channel_count = 0;
static heapstore_ipc_buffer_t s_buffers[heapstore_IPC_MAX_BUFFERS];
static size_t s_buffer_count = 0;
static char s_ipc_path[heapstore_IPC_MAX_PATH] = {0};

heapstore_error_t heapstore_ipc_init(void) {
    if (s_initialized) {
        return heapstore_SUCCESS;
    }

    const char* base_path = "agentos/heapstore/kernel/ipc";
    strncpy(s_ipc_path, base_path, sizeof(s_ipc_path) - 1);
    s_ipc_path[sizeof(s_ipc_path) - 1] = '\0';

    heapstore_ensure_directory(s_ipc_path);

    char channels_path[heapstore_IPC_MAX_PATH];
    snprintf(channels_path, sizeof(channels_path), "%s/channels", s_ipc_path);
    heapstore_ensure_directory(channels_path);

    char buffers_path[heapstore_IPC_MAX_PATH];
    snprintf(buffers_path, sizeof(buffers_path), "%s/buffers", s_ipc_path);
    heapstore_ensure_directory(buffers_path);

    memset(s_channels, 0, sizeof(s_channels));
    memset(s_buffers, 0, sizeof(s_buffers));
    s_channel_count = 0;
    s_buffer_count = 0;

    s_initialized = true;

    return heapstore_SUCCESS;
}

void heapstore_ipc_shutdown(void) {
    if (!s_initialized) {
        return;
    }

    pthread_mutex_lock(&s_ipc_lock);

    memset(s_channels, 0, sizeof(s_channels));
    memset(s_buffers, 0, sizeof(s_buffers));
    s_channel_count = 0;
    s_buffer_count = 0;

    s_initialized = false;
    pthread_mutex_unlock(&s_ipc_lock);
}

heapstore_error_t heapstore_ipc_record_channel(const heapstore_ipc_channel_t* channel) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!channel) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_ipc_lock);

    if (s_channel_count >= heapstore_IPC_MAX_CHANNELS) {
        pthread_mutex_unlock(&s_ipc_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < s_channel_count; i++) {
        if (strcmp(s_channels[i].channel_id, channel->channel_id) == 0) {
            memcpy(&s_channels[i], channel, sizeof(heapstore_ipc_channel_t));
            pthread_mutex_unlock(&s_ipc_lock);
            return heapstore_SUCCESS;
        }
    }

    memcpy(&s_channels[s_channel_count], channel, sizeof(heapstore_ipc_channel_t));
    s_channel_count++;

    pthread_mutex_unlock(&s_ipc_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_ipc_get_channel(const char* channel_id, heapstore_ipc_channel_t* channel) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!channel_id || !channel) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_ipc_lock);

    for (size_t i = 0; i < s_channel_count; i++) {
        if (strcmp(s_channels[i].channel_id, channel_id) == 0) {
            memcpy(channel, &s_channels[i], sizeof(heapstore_ipc_channel_t));
            pthread_mutex_unlock(&s_ipc_lock);
            return heapstore_SUCCESS;
        }
    }

    pthread_mutex_unlock(&s_ipc_lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_ipc_update_channel_activity(const char* channel_id) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!channel_id) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_ipc_lock);

    for (size_t i = 0; i < s_channel_count; i++) {
        if (strcmp(s_channels[i].channel_id, channel_id) == 0) {
            s_channels[i].last_activity_at = (uint64_t)time(NULL);
            pthread_mutex_unlock(&s_ipc_lock);
            return heapstore_SUCCESS;
        }
    }

    pthread_mutex_unlock(&s_ipc_lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_ipc_record_buffer(const heapstore_ipc_buffer_t* buffer) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!buffer) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_ipc_lock);

    if (s_buffer_count >= heapstore_IPC_MAX_BUFFERS) {
        pthread_mutex_unlock(&s_ipc_lock);
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < s_buffer_count; i++) {
        if (strcmp(s_buffers[i].buffer_id, buffer->buffer_id) == 0) {
            memcpy(&s_buffers[i], buffer, sizeof(heapstore_ipc_buffer_t));
            pthread_mutex_unlock(&s_ipc_lock);
            return heapstore_SUCCESS;
        }
    }

    memcpy(&s_buffers[s_buffer_count], buffer, sizeof(heapstore_ipc_buffer_t));
    s_buffer_count++;

    pthread_mutex_unlock(&s_ipc_lock);

    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_ipc_get_buffer(const char* buffer_id, heapstore_ipc_buffer_t* buffer) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    if (!buffer_id || !buffer) {
        return heapstore_ERR_INVALID_PARAM;
    }

    pthread_mutex_lock(&s_ipc_lock);

    for (size_t i = 0; i < s_buffer_count; i++) {
        if (strcmp(s_buffers[i].buffer_id, buffer_id) == 0) {
            memcpy(buffer, &s_buffers[i], sizeof(heapstore_ipc_buffer_t));
            pthread_mutex_unlock(&s_ipc_lock);
            return heapstore_SUCCESS;
        }
    }

    pthread_mutex_unlock(&s_ipc_lock);
    return heapstore_ERR_NOT_FOUND;
}

heapstore_error_t heapstore_ipc_get_stats(uint32_t* channel_count, uint32_t* buffer_count, uint64_t* total_size) {
    if (!s_initialized) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&s_ipc_lock);

    if (channel_count) {
        *channel_count = (uint32_t)s_channel_count;
    }
    if (buffer_count) {
        *buffer_count = (uint32_t)s_buffer_count;
    }
    if (total_size) {
        uint64_t size = 0;
        for (size_t i = 0; i < s_channel_count; i++) {
            size += s_channels[i].buffer_size;
        }
        for (size_t i = 0; i < s_buffer_count; i++) {
            size += s_buffers[i].size;
        }
        *total_size = size;
    }

    pthread_mutex_unlock(&s_ipc_lock);

    return heapstore_SUCCESS;
}

bool heapstore_ipc_is_healthy(void) {
    return s_initialized;
}
