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
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define mkdir(path, mode) _mkdir(path)
#define F_OK 0
#define access _access
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
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

static heapstore_error_t persist_channel_to_file(const heapstore_ipc_channel_t* channel) {
    if (!channel || !s_ipc_path[0]) return heapstore_ERR_INVALID_PARAM;
    char path[heapstore_IPC_MAX_PATH + 256];
    snprintf(path, sizeof(path), "%s/channels/%s.json", s_ipc_path, channel->channel_id);
    FILE* fp = fopen(path, "w");
    if (!fp) return heapstore_ERR_FILE_OPEN_FAILED;
    fprintf(fp, "{\"channel_id\":\"%s\",\"name\":\"%s\",\"type\":\"%s\","
                "\"status\":\"%s\",\"created_at\":%llu,"
                "\"last_activity_at\":%llu,\"buffer_size\":%zu,"
                "\"current_usage\":%zu}\n",
            channel->channel_id, channel->name, channel->type,
            channel->status, (unsigned long long)channel->created_at,
            (unsigned long long)channel->last_activity_at,
            channel->buffer_size, channel->current_usage);
    fclose(fp);
    return heapstore_SUCCESS;
}

static heapstore_error_t persist_buffer_to_file(const heapstore_ipc_buffer_t* buffer) {
    if (!buffer || !s_ipc_path[0]) return heapstore_ERR_INVALID_PARAM;
    char path[heapstore_IPC_MAX_PATH + 256];
    snprintf(path, sizeof(path), "%s/buffers/%s.json", s_ipc_path, buffer->buffer_id);
    FILE* fp = fopen(path, "w");
    if (!fp) return heapstore_ERR_FILE_OPEN_FAILED;
    fprintf(fp, "{\"buffer_id\":\"%s\",\"channel_id\":\"%s\","
                "\"size\":%zu,\"used\":%zu,"
                "\"created_at\":%llu,\"status\":\"%s\"}\n",
            buffer->buffer_id, buffer->channel_id,
            buffer->size, buffer->used,
            (unsigned long long)buffer->created_at, buffer->status);
    fclose(fp);
    return heapstore_SUCCESS;
}

static heapstore_error_t load_channel_from_file(const char* channel_id, heapstore_ipc_channel_t* channel) {
    if (!channel_id || !channel || !s_ipc_path[0]) return heapstore_ERR_INVALID_PARAM;
    char path[heapstore_IPC_MAX_PATH + 256];
    snprintf(path, sizeof(path), "%s/channels/%s.json", s_ipc_path, channel_id);
    FILE* fp = fopen(path, "r");
    if (!fp) return heapstore_ERR_NOT_FOUND;
    memset(channel, 0, sizeof(*channel));
    char buf[2048];
    if (fgets(buf, sizeof(buf), fp)) {
        char* v;
        if ((v = strstr(buf, "\"channel_id\":\""))) {
            v += 14; char* e = strchr(v, '"');
            if (e) { size_t l = (size_t)(e-v); if (l>=sizeof(channel->channel_id)) l=sizeof(channel->channel_id)-1; memcpy(channel->channel_id, v, l); }
        }
        if ((v = strstr(buf, "\"name\":\""))) {
            v += 8; char* e = strchr(v, '"');
            if (e) { size_t l = (size_t)(e-v); if (l>=sizeof(channel->name)) l=sizeof(channel->name)-1; memcpy(channel->name, v, l); }
        }
        if ((v = strstr(buf, "\"type\":\""))) {
            v += 8; char* e = strchr(v, '"');
            if (e) { size_t l = (size_t)(e-v); if (l>=sizeof(channel->type)) l=sizeof(channel->type)-1; memcpy(channel->type, v, l); }
        }
        if ((v = strstr(buf, "\"status\":\""))) {
            v += 10; char* e = strchr(v, '"');
            if (e) { size_t l = (size_t)(e-v); if (l>=sizeof(channel->status)) l=sizeof(channel->status)-1; memcpy(channel->status, v, l); }
        }
        if ((v = strstr(buf, "\"buffer_size\":"))) { channel->buffer_size = (size_t)atoll(v+14); }
        if ((v = strstr(buf, "\"current_usage\":"))) { channel->current_usage = (size_t)atoll(v+16); }
        if ((v = strstr(buf, "\"created_at\":"))) { channel->created_at = (uint64_t)strtoull(v+13, NULL, 10); }
        if ((v = strstr(buf, "\"last_activity_at\":"))) { channel->last_activity_at = (uint64_t)strtoull(v+20, NULL, 10); }
    }
    fclose(fp);
    return heapstore_SUCCESS;
}

static heapstore_error_t load_buffer_from_file(const char* buffer_id, heapstore_ipc_buffer_t* buffer) {
    if (!buffer_id || !buffer || !s_ipc_path[0]) return heapstore_ERR_INVALID_PARAM;
    char path[heapstore_IPC_MAX_PATH + 256];
    snprintf(path, sizeof(path), "%s/buffers/%s.json", s_ipc_path, buffer_id);
    FILE* fp = fopen(path, "r");
    if (!fp) return heapstore_ERR_NOT_FOUND;
    memset(buffer, 0, sizeof(*buffer));
    char buf[2048];
    if (fgets(buf, sizeof(buf), fp)) {
        char* v;
        if ((v = strstr(buf, "\"buffer_id\":\""))) {
            v += 13; char* e = strchr(v, '"');
            if (e) { size_t l = (size_t)(e-v); if (l>=sizeof(buffer->buffer_id)) l=sizeof(buffer->buffer_id)-1; memcpy(buffer->buffer_id, v, l); }
        }
        if ((v = strstr(buf, "\"channel_id\":\""))) {
            v += 14; char* e = strchr(v, '"');
            if (e) { size_t l = (size_t)(e-v); if (l>=sizeof(buffer->channel_id)) l=sizeof(buffer->channel_id)-1; memcpy(buffer->channel_id, v, l); }
        }
        if ((v = strstr(buf, "\"size\":"))) { buffer->size = (size_t)atoll(v+7); }
        if ((v = strstr(buf, "\"used\":"))) { buffer->used = (size_t)atoll(v+7); }
        if ((v = strstr(buf, "\"status\":\""))) {
            v += 10; char* e = strchr(v, '"');
            if (e) { size_t l = (size_t)(e-v); if (l>=sizeof(buffer->status)) l=sizeof(buffer->status)-1; memcpy(buffer->status, v, l); }
        }
        if ((v = strstr(buf, "\"created_at\":"))) { buffer->created_at = (uint64_t)strtoull(v+13, NULL, 10); }
    }
    fclose(fp);
    return heapstore_SUCCESS;
}

#ifndef _WIN32
typedef struct {
    char shm_name[256];
    int shm_fd;
    void* mapped;
    size_t mapped_size;
} ipc_shm_region_t;

#define IPC_SHM_MAX_REGIONS 32
static ipc_shm_region_t s_shm_regions[IPC_SHM_MAX_REGIONS];
static size_t s_shm_region_count = 0;

static ipc_shm_region_t* find_or_create_shm(const char* name, size_t size) {
    for (size_t i = 0; i < s_shm_region_count; i++) {
        if (strcmp(s_shm_regions[i].shm_name, name) == 0) {
            return &s_shm_regions[i];
        }
    }
    if (s_shm_region_count >= IPC_SHM_MAX_REGIONS) return NULL;

    ipc_shm_region_t* r = &s_shm_regions[s_shm_region_count];
    snprintf(r->shm_name, sizeof(r->shm_name), "/agentos_ipc_%s", name);

    r->shm_fd = shm_open(r->shm_name, O_CREAT | O_RDWR, 0666);
    if (r->shm_fd < 0) return NULL;

    if (ftruncate(r->shm_fd, (off_t)size) != 0) {
        close(r->shm_fd);
        shm_unlink(r->shm_name);
        return NULL;
    }

    r->mapped = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, r->shm_fd, 0);
    if (r->mapped == MAP_FAILED) {
        close(r->shm_fd);
        shm_unlink(r->shm_name);
        return NULL;
    }

    r->mapped_size = size;
    s_shm_region_count++;
    return r;
}
#endif

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

#ifndef _WIN32
    for (size_t i = 0; i < s_shm_region_count; i++) {
        if (s_shm_regions[i].mapped && s_shm_regions[i].mapped != MAP_FAILED) {
            munmap(s_shm_regions[i].mapped, s_shm_regions[i].mapped_size);
        }
        if (s_shm_regions[i].shm_fd >= 0) {
            close(s_shm_regions[i].shm_fd);
        }
        if (s_shm_regions[i].shm_name[0]) {
            shm_unlink(s_shm_regions[i].shm_name);
        }
    }
    s_shm_region_count = 0;
#endif

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

    persist_channel_to_file(channel);

#ifndef _WIN32
    if (strcmp(channel->type, "shared_memory") == 0 && channel->buffer_size > 0) {
        find_or_create_shm(channel->channel_id, channel->buffer_size);
    }
#endif

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

    heapstore_error_t file_err = load_channel_from_file(channel_id, channel);
    if (file_err == heapstore_SUCCESS) {
        pthread_mutex_lock(&s_ipc_lock);
        if (s_channel_count < heapstore_IPC_MAX_CHANNELS) {
            memcpy(&s_channels[s_channel_count], channel, sizeof(heapstore_ipc_channel_t));
            s_channel_count++;
        }
        pthread_mutex_unlock(&s_ipc_lock);
        return heapstore_SUCCESS;
    }

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

    persist_buffer_to_file(buffer);

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

    heapstore_error_t file_err = load_buffer_from_file(buffer_id, buffer);
    if (file_err == heapstore_SUCCESS) {
        pthread_mutex_lock(&s_ipc_lock);
        if (s_buffer_count < heapstore_IPC_MAX_BUFFERS) {
            memcpy(&s_buffers[s_buffer_count], buffer, sizeof(heapstore_ipc_buffer_t));
            s_buffer_count++;
        }
        pthread_mutex_unlock(&s_ipc_lock);
        return heapstore_SUCCESS;
    }

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
