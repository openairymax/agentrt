/**
 * @file lodges_ipc.c
 * @brief AgentOS 数据分区 IPC 数据存储实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "lodges_ipc.h"
#include "private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

typedef struct ipc_channel_node {
    lodges_ipc_channel_t channel;
    struct ipc_channel_node* next;
} ipc_channel_node_t;

typedef struct ipc_buffer_node {
    lodges_ipc_buffer_t buffer;
    struct ipc_buffer_node* next;
} ipc_buffer_node_t;

typedef struct {
    ipc_channel_node_t* channels;
    ipc_buffer_node_t* buffers;
    size_t channel_count;
    size_t buffer_count;
    pthread_mutex_t lock;
    bool initialized;
} ipc_data_t;

static ipc_data_t g_ipc_data = {0};

static bool ensure_directory(const char* path) {
    if (!path) return false;

    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    size_t len = strlen(path_copy);
    for (size_t i = 0; i < len; i++) {
        if (path_copy[i] == '\\' || path_copy[i] == '/') {
            if (i > 0 && path_copy[i - 1] != ':') {
                path_copy[i] = '\0';
                mkdir(path_copy, 0755);
                path_copy[i] = '/';
            }
        }
    }

    mkdir(path_copy, 0755);
    return true;
}

lodges_error_t lodges_ipc_init(void) {
    if (g_ipc_data.initialized) {
        return lodges_ERR_ALREADY_INITIALIZED;
    }

    memset(&g_ipc_data, 0, sizeof(g_ipc_data));
    pthread_mutex_init(&g_ipc_data.lock, NULL);
    g_ipc_data.initialized = true;

    ensure_directory("lodges/kernel/ipc/channels");
    ensure_directory("lodges/kernel/ipc/buffers");

    return lodges_SUCCESS;
}

void lodges_ipc_shutdown(void) {
    if (!g_ipc_data.initialized) {
        return;
    }

    pthread_mutex_lock(&g_ipc_data.lock);

    ipc_channel_node_t* ch = g_ipc_data.channels;
    while (ch) {
        ipc_channel_node_t* next = ch->next;
        free(ch);
        ch = next;
    }

    ipc_buffer_node_t* buf = g_ipc_data.buffers;
    while (buf) {
        ipc_buffer_node_t* next = buf->next;
        free(buf);
        buf = next;
    }

    g_ipc_data.channels = NULL;
    g_ipc_data.buffers = NULL;
    g_ipc_data.channel_count = 0;
    g_ipc_data.buffer_count = 0;

    pthread_mutex_unlock(&g_ipc_data.lock);
    pthread_mutex_destroy(&g_ipc_data.lock);

    g_ipc_data.initialized = false;
}

lodges_error_t lodges_ipc_record_channel(const lodges_ipc_channel_t* channel) {
    if (!channel || !channel->channel_id[0]) {
        return lodges_ERR_INVALID_PARAM;
    }

    if (!g_ipc_data.initialized) {
        return lodges_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_ipc_data.lock);

    ipc_channel_node_t* node = (ipc_channel_node_t*)malloc(sizeof(ipc_channel_node_t));
    if (!node) {
        pthread_mutex_unlock(&g_ipc_data.lock);
        return lodges_ERR_OUT_OF_MEMORY;
    }

    memcpy(&node->channel, channel, sizeof(lodges_ipc_channel_t));
    node->next = g_ipc_data.channels;
    g_ipc_data.channels = node;
    g_ipc_data.channel_count++;

    pthread_mutex_unlock(&g_ipc_data.lock);

    return lodges_SUCCESS;
}

lodges_error_t lodges_ipc_get_channel(const char* channel_id, lodges_ipc_channel_t* channel) {
    if (!channel_id || !channel) {
        return lodges_ERR_INVALID_PARAM;
    }

    if (!g_ipc_data.initialized) {
        return lodges_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_ipc_data.lock);

    ipc_channel_node_t* node = g_ipc_data.channels;
    while (node) {
        if (strcmp(node->channel.channel_id, channel_id) == 0) {
            memcpy(channel, &node->channel, sizeof(lodges_ipc_channel_t));
            pthread_mutex_unlock(&g_ipc_data.lock);
            return lodges_SUCCESS;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&g_ipc_data.lock);

    return lodges_ERR_NOT_FOUND;
}

lodges_error_t lodges_ipc_update_channel_activity(const char* channel_id) {
    if (!channel_id) {
        return lodges_ERR_INVALID_PARAM;
    }

    if (!g_ipc_data.initialized) {
        return lodges_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_ipc_data.lock);

    ipc_channel_node_t* node = g_ipc_data.channels;
    while (node) {
        if (strcmp(node->channel.channel_id, channel_id) == 0) {
            node->channel.last_activity_at = (uint64_t)time(NULL);
            pthread_mutex_unlock(&g_ipc_data.lock);
            return lodges_SUCCESS;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&g_ipc_data.lock);

    return lodges_ERR_NOT_FOUND;
}

lodges_error_t lodges_ipc_record_buffer(const lodges_ipc_buffer_t* buffer) {
    if (!buffer || !buffer->buffer_id[0]) {
        return lodges_ERR_INVALID_PARAM;
    }

    if (!g_ipc_data.initialized) {
        return lodges_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_ipc_data.lock);

    ipc_buffer_node_t* node = (ipc_buffer_node_t*)malloc(sizeof(ipc_buffer_node_t));
    if (!node) {
        pthread_mutex_unlock(&g_ipc_data.lock);
        return lodges_ERR_OUT_OF_MEMORY;
    }

    memcpy(&node->buffer, buffer, sizeof(lodges_ipc_buffer_t));
    node->next = g_ipc_data.buffers;
    g_ipc_data.buffers = node;
    g_ipc_data.buffer_count++;

    pthread_mutex_unlock(&g_ipc_data.lock);

    return lodges_SUCCESS;
}

lodges_error_t lodges_ipc_get_buffer(const char* buffer_id, lodges_ipc_buffer_t* buffer) {
    if (!buffer_id || !buffer) {
        return lodges_ERR_INVALID_PARAM;
    }

    if (!g_ipc_data.initialized) {
        return lodges_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_ipc_data.lock);

    ipc_buffer_node_t* node = g_ipc_data.buffers;
    while (node) {
        if (strcmp(node->buffer.buffer_id, buffer_id) == 0) {
            memcpy(buffer, &node->buffer, sizeof(lodges_ipc_buffer_t));
            pthread_mutex_unlock(&g_ipc_data.lock);
            return lodges_SUCCESS;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&g_ipc_data.lock);

    return lodges_ERR_NOT_FOUND;
}

lodges_error_t lodges_ipc_get_stats(uint32_t* channel_count, uint32_t* buffer_count, uint64_t* total_size) {
    if (!g_ipc_data.initialized) {
        return lodges_ERR_NOT_INITIALIZED;
    }

    pthread_mutex_lock(&g_ipc_data.lock);

    if (channel_count) {
        *channel_count = (uint32_t)g_ipc_data.channel_count;
    }

    if (buffer_count) {
        *buffer_count = (uint32_t)g_ipc_data.buffer_count;
    }

    if (total_size) {
        *total_size = 0;
        ipc_buffer_node_t* node = g_ipc_data.buffers;
        while (node) {
            *total_size += node->buffer.size;
            node = node->next;
        }
    }

    pthread_mutex_unlock(&g_ipc_data.lock);

    return lodges_SUCCESS;
}

