/**
 * @file checkpoint.c
 * @brief AgentOS 任务检查点实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 *
 * @note 实现任务检查点机制，支持长时间任务的状态保存和恢复。
 *       符合 ARCHITECTURAL_PRINCIPLES.md 中的 S-1 反馈闭环原则。
 */

#include "../include/checkpoint.h"
#include "agentos.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#endif

#define CHECKPOINT_DIRECTORY "checkpoints"
#define CHECKPOINT_FILE_PREFIX "checkpoint_"
#define CHECKPOINT_FILE_EXTENSION ".json"
#define MAX_CHECKPOINT_PATH 1024
#define CHECKPOINT_VERSION 1

static char g_checkpoint_storage_path[MAX_CHECKPOINT_PATH] = {0};
static int g_checkpoint_initialized = 0;

#ifndef _WIN32
static pthread_mutex_t g_checkpoint_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static agentos_checkpoint_stats_t g_checkpoint_stats = {0};

static uint32_t calculate_checksum(const char* data, size_t len) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum = (checksum << 1) ^ (uint32_t)data[i];
    }
    return checksum;
}

static const char* state_to_string(agentos_checkpoint_state_t state) {
    switch (state) {
        case CHECKPOINT_STATE_PENDING: return "pending";
        case CHECKPOINT_STATE_COMPLETED: return "completed";
        case CHECKPOINT_STATE_FAILED: return "failed";
        case CHECKPOINT_STATE_INVALID: return "invalid";
        default: return "unknown";
    }
}

static agentos_checkpoint_state_t string_to_state(const char* state_str) {
    if (strcmp(state_str, "pending") == 0) return CHECKPOINT_STATE_PENDING;
    if (strcmp(state_str, "completed") == 0) return CHECKPOINT_STATE_COMPLETED;
    if (strcmp(state_str, "failed") == 0) return CHECKPOINT_STATE_FAILED;
    if (strcmp(state_str, "invalid") == 0) return CHECKPOINT_STATE_INVALID;
    return CHECKPOINT_STATE_INVALID;
}

agentos_error_t agentos_checkpoint_init(const char* storage_path) {
    if (g_checkpoint_initialized) {
        return AGENTOS_SUCCESS;
    }

    if (storage_path == NULL) {
        snprintf(g_checkpoint_storage_path, sizeof(g_checkpoint_storage_path),
                 "./" CHECKPOINT_DIRECTORY);
    } else {
        strncpy(g_checkpoint_storage_path, storage_path, sizeof(g_checkpoint_storage_path) - 1);
        g_checkpoint_storage_path[sizeof(g_checkpoint_storage_path) - 1] = '\0';
    }

#ifndef _WIN32
    pthread_mutex_init(&g_checkpoint_mutex, NULL);
#endif

    memset(&g_checkpoint_stats, 0, sizeof(g_checkpoint_stats));
    g_checkpoint_initialized = 1;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_shutdown(void) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_SUCCESS;
    }

#ifndef _WIN32
    pthread_mutex_destroy(&g_checkpoint_mutex);
#endif

    g_checkpoint_initialized = 0;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_create(
    const char* task_id,
    const char* session_id,
    uint64_t sequence_num,
    const char* state_json,
    char** completed_nodes,
    size_t completed_count,
    char** pending_nodes,
    size_t pending_count,
    agentos_task_checkpoint_t** out_checkpoint) {

    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id || !state_json || !out_checkpoint) {
        return AGENTOS_EINVAL;
    }

    agentos_task_checkpoint_t* checkpoint = (agentos_task_checkpoint_t*)
        malloc(sizeof(agentos_task_checkpoint_t));
    if (!checkpoint) {
        return AGENTOS_ENOMEM;
    }

    memset(checkpoint, 0, sizeof(agentos_task_checkpoint_t));

    strncpy(checkpoint->task_id, task_id, sizeof(checkpoint->task_id) - 1);
    if (session_id) {
        strncpy(checkpoint->session_id, session_id, sizeof(checkpoint->session_id) - 1);
    }
    checkpoint->sequence_num = sequence_num;
    checkpoint->timestamp = (uint64_t)time(NULL) * 1000000000ULL;

    checkpoint->state_json = strdup(state_json);
    if (!checkpoint->state_json) {
        free(checkpoint);
        return AGENTOS_ENOMEM;
    }
    checkpoint->state_size = strlen(state_json);

    checkpoint->completed_count = completed_count;
    if (completed_nodes && completed_count > 0) {
        checkpoint->completed_nodes = (char**)malloc(sizeof(char*) * completed_count);
        if (!checkpoint->completed_nodes) {
            free(checkpoint->state_json);
            free(checkpoint);
            return AGENTOS_ENOMEM;
        }
        for (size_t i = 0; i < completed_count; i++) {
            checkpoint->completed_nodes[i] = strdup(completed_nodes[i]);
            if (!checkpoint->completed_nodes[i]) {
                for (size_t j = 0; j < i; j++) {
                    free(checkpoint->completed_nodes[j]);
                }
                free(checkpoint->completed_nodes);
                free(checkpoint->state_json);
                free(checkpoint);
                return AGENTOS_ENOMEM;
            }
        }
    }

    checkpoint->pending_count = pending_count;
    if (pending_nodes && pending_count > 0) {
        checkpoint->pending_nodes = (char**)malloc(sizeof(char*) * pending_count);
        if (!checkpoint->pending_nodes) {
            if (checkpoint->completed_nodes) {
                for (size_t i = 0; i < completed_count; i++) {
                    free(checkpoint->completed_nodes[i]);
                }
                free(checkpoint->completed_nodes);
            }
            free(checkpoint->state_json);
            free(checkpoint);
            return AGENTOS_ENOMEM;
        }
        for (size_t i = 0; i < pending_count; i++) {
            checkpoint->pending_nodes[i] = strdup(pending_nodes[i]);
            if (!checkpoint->pending_nodes[i]) {
                for (size_t j = 0; j < i; j++) {
                    free(checkpoint->pending_nodes[j]);
                }
                free(checkpoint->pending_nodes);
                if (checkpoint->completed_nodes) {
                    for (size_t j = 0; j < completed_count; j++) {
                        free(checkpoint->completed_nodes[j]);
                    }
                    free(checkpoint->completed_nodes);
                }
                free(checkpoint->state_json);
                free(checkpoint);
                return AGENTOS_ENOMEM;
            }
        }
    }

    checkpoint->state = CHECKPOINT_STATE_PENDING;
    checkpoint->checksum = calculate_checksum(state_json, strlen(state_json));

    *out_checkpoint = checkpoint;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_save(const agentos_task_checkpoint_t* checkpoint) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!checkpoint) {
        return AGENTOS_EINVAL;
    }

    char filepath[MAX_CHECKPOINT_PATH];
    snprintf(filepath, sizeof(filepath), "%s/%s%s%s",
             g_checkpoint_storage_path,
             CHECKPOINT_FILE_PREFIX,
             checkpoint->task_id,
             CHECKPOINT_FILE_EXTENSION);

    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        g_checkpoint_stats.failed_checkpoints++;
        return AGENTOS_EIO;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": %d,\n", CHECKPOINT_VERSION);
    fprintf(fp, "  \"task_id\": \"%s\",\n", checkpoint->task_id);
    fprintf(fp, "  \"session_id\": \"%s\",\n", checkpoint->session_id);
    fprintf(fp, "  \"sequence_num\": %lu,\n", (unsigned long)checkpoint->sequence_num);
    fprintf(fp, "  \"timestamp\": %lu,\n", (unsigned long)checkpoint->timestamp);
    fprintf(fp, "  \"state\": \"%s\",\n", state_to_string(checkpoint->state));
    fprintf(fp, "  \"checksum\": %u,\n", checkpoint->checksum);
    fprintf(fp, "  \"state_size\": %zu,\n", checkpoint->state_size);
    fprintf(fp, "  \"state_json\": %s,\n", checkpoint->state_json);
    fprintf(fp, "  \"completed_count\": %zu,\n", checkpoint->completed_count);
    fprintf(fp, "  \"pending_count\": %zu,\n", checkpoint->pending_count);
    fprintf(fp, "  \"metadata\": \"%s\"\n", checkpoint->metadata);
    fprintf(fp, "}\n");

    fclose(fp);

    checkpoint->state = CHECKPOINT_STATE_COMPLETED;
    g_checkpoint_stats.successful_checkpoints++;
    g_checkpoint_stats.total_checkpoints++;
    g_checkpoint_stats.last_checkpoint_time = checkpoint->timestamp;
    g_checkpoint_stats.avg_checkpoint_size =
        (g_checkpoint_stats.avg_checkpoint_size * (g_checkpoint_stats.total_checkpoints - 1) +
         checkpoint->state_size) / g_checkpoint_stats.total_checkpoints;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_restore(
    const char* task_id,
    uint64_t sequence_num,
    agentos_task_checkpoint_t** out_checkpoint) {

    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id || !out_checkpoint) {
        return AGENTOS_EINVAL;
    }

    char filepath[MAX_CHECKPOINT_PATH];
    snprintf(filepath, sizeof(filepath), "%s/%s%s%s",
             g_checkpoint_storage_path,
             CHECKPOINT_FILE_PREFIX,
             task_id,
             CHECKPOINT_FILE_EXTENSION);

    FILE* fp = fopen(filepath, "r");
    if (!fp) {
        return AGENTOS_ENOENT;
    }

    agentos_task_checkpoint_t* checkpoint = (agentos_task_checkpoint_t*)
        calloc(1, sizeof(agentos_task_checkpoint_t));
    if (!checkpoint) {
        fclose(fp);
        return AGENTOS_ENOMEM;
    }

    char line[2048];
    char state_str[64] = {0};

    while (fgets(line, sizeof(line), fp)) {
        char key[128], value[1024];
        if (sscanf(line, "  \"%[^\"]\": \"%[^\"]\"", key, value) == 2) {
            if (strcmp(key, "task_id") == 0) {
                strncpy(checkpoint->task_id, value, sizeof(checkpoint->task_id) - 1);
            } else if (strcmp(key, "session_id") == 0) {
                strncpy(checkpoint->session_id, value, sizeof(checkpoint->session_id) - 1);
            } else if (strcmp(key, "state") == 0) {
                strncpy(state_str, value, sizeof(state_str) - 1);
            }
        } else if (sscanf(line, "  \"%[^\"]\": %lu", key, (unsigned long*)&checkpoint->sequence_num) == 2) {
            if (strcmp(key, "sequence_num") != 0 && strcmp(key, "timestamp") != 0 &&
                strcmp(key, "checksum") != 0 && strcmp(key, "state_size") != 0 &&
                strcmp(key, "completed_count") != 0 && strcmp(key, "pending_count") != 0) {
            }
        } else if (sscanf(line, "  \"state_json\": \"%[^\"]\"", value) == 1) {
            checkpoint->state_json = strdup(value);
            if (!checkpoint->state_json) {
                fclose(fp);
                free(checkpoint);
                return AGENTOS_ENOMEM;
            }
            checkpoint->state_size = strlen(value);
        }
    }

    fclose(fp);

    checkpoint->state = string_to_state(state_str);
    g_checkpoint_stats.total_restore_ops++;

    *out_checkpoint = checkpoint;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_delete(const char* task_id, uint64_t sequence_num) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id) {
        return AGENTOS_EINVAL;
    }

    char filepath[MAX_CHECKPOINT_PATH];
    snprintf(filepath, sizeof(filepath), "%s/%s%s%s",
             g_checkpoint_storage_path,
             CHECKPOINT_FILE_PREFIX,
             task_id,
             CHECKPOINT_FILE_EXTENSION);

    if (unlink(filepath) == 0) {
        return AGENTOS_SUCCESS;
    }

    return AGENTOS_ENOENT;
}

agentos_error_t agentos_checkpoint_list(
    const char* task_id,
    agentos_task_checkpoint_t*** out_checkpoints,
    size_t* out_count) {

    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id || !out_checkpoints || !out_count) {
        return AGENTOS_EINVAL;
    }

    *out_checkpoints = NULL;
    *out_count = 0;

    agentos_task_checkpoint_t* checkpoint = NULL;
    agentos_error_t err = agentos_checkpoint_restore(task_id, 0, &checkpoint);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    *out_checkpoints = (agentos_task_checkpoint_t**)malloc(sizeof(agentos_task_checkpoint_t*));
    if (!*out_checkpoints) {
        agentos_checkpoint_destroy(checkpoint);
        return AGENTOS_ENOMEM;
    }

    (*out_checkpoints)[0] = checkpoint;
    *out_count = 1;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_get_stats(agentos_checkpoint_stats_t* out_stats) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!out_stats) {
        return AGENTOS_EINVAL;
    }

    memcpy(out_stats, &g_checkpoint_stats, sizeof(agentos_checkpoint_stats_t));
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_verify(
    const agentos_task_checkpoint_t* checkpoint,
    bool* is_valid) {

    if (!checkpoint || !is_valid) {
        return AGENTOS_EINVAL;
    }

    if (checkpoint->state == CHECKPOINT_STATE_INVALID) {
        *is_valid = false;
        return AGENTOS_SUCCESS;
    }

    uint32_t calculated_checksum = calculate_checksum(
        checkpoint->state_json,
        checkpoint->state_size
    );

    *is_valid = (calculated_checksum == checkpoint->checksum);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_destroy(agentos_task_checkpoint_t* checkpoint) {
    if (!checkpoint) {
        return AGENTOS_SUCCESS;
    }

    if (checkpoint->state_json) {
        free(checkpoint->state_json);
    }

    if (checkpoint->completed_nodes) {
        for (size_t i = 0; i < checkpoint->completed_count; i++) {
            if (checkpoint->completed_nodes[i]) {
                free(checkpoint->completed_nodes[i]);
            }
        }
        free(checkpoint->completed_nodes);
    }

    if (checkpoint->pending_nodes) {
        for (size_t i = 0; i < checkpoint->pending_count; i++) {
            if (checkpoint->pending_nodes[i]) {
                free(checkpoint->pending_nodes[i]);
            }
        }
        free(checkpoint->pending_nodes);
    }

    free(checkpoint);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_checkpoint_cleanup(uint64_t max_age_seconds, size_t max_count) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    uint64_t now = (uint64_t)time(NULL) * 1000000000ULL;
    uint64_t cutoff_time = now - (max_age_seconds * 1000000000ULL);

    if (g_checkpoint_stats.total_checkpoints > max_count) {
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_snapshot_create(const char* task_id, const char* snapshot_path) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!task_id || !snapshot_path) {
        return AGENTOS_EINVAL;
    }

    agentos_task_checkpoint_t* checkpoint = NULL;
    agentos_error_t err = agentos_checkpoint_restore(task_id, 0, &checkpoint);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    FILE* fp = fopen(snapshot_path, "wb");
    if (!fp) {
        agentos_checkpoint_destroy(checkpoint);
        return AGENTOS_EIO;
    }

    fprintf(fp, "SNAPSHOT_V1\n");
    fprintf(fp, "TaskID: %s\n", checkpoint->task_id);
    fprintf(fp, "SessionID: %s\n", checkpoint->session_id);
    fprintf(fp, "SequenceNum: %lu\n", (unsigned long)checkpoint->sequence_num);
    fprintf(fp, "Timestamp: %lu\n", (unsigned long)checkpoint->timestamp);
    fprintf(fp, "StateSize: %zu\n", checkpoint->state_size);
    fprintf(fp, "---DATA---\n");
    fwrite(checkpoint->state_json, 1, checkpoint->state_size, fp);
    fprintf(fp, "\n---END---\n");

    fclose(fp);
    agentos_checkpoint_destroy(checkpoint);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_snapshot_restore(const char* snapshot_path, char** task_id) {
    if (!g_checkpoint_initialized) {
        return AGENTOS_ENOTINIT;
    }

    if (!snapshot_path || !task_id) {
        return AGENTOS_EINVAL;
    }

    FILE* fp = fopen(snapshot_path, "rb");
    if (!fp) {
        return AGENTOS_ENOENT;
    }

    char header[64];
    if (!fgets(header, sizeof(header), fp) || strncmp(header, "SNAPSHOT_V1", 11) != 0) {
        fclose(fp);
        return AGENTOS_EIO;
    }

    char line[256];
    char* state_data = NULL;
    size_t state_size = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "TaskID: ", 8) == 0) {
            *task_id = strdup(line + 8);
            (*task_id)[strlen(*task_id) - 1] = '\0';
        } else if (strncmp(line, "---DATA---", 10) == 0) {
            break;
        }
    }

    fclose(fp);

    if (!*task_id) {
        return AGENTOS_EIO;
    }

    return AGENTOS_SUCCESS;
}
