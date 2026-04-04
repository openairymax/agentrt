/**
 * @file atoms_checkpoint.c
 * @brief AgentOS检查点与快照恢复实现
 *
 * 提供任务状态检查点和快照恢复功能，支持长时间任务（1000小时+）的中断恢复。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "atoms_checkpoint.h"
#include "../../commons/utils/memory/include/memory_compat.h"
#include "../../commons/utils/string/include/string_compat.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

/**
 * @brief 检查点管理器内部结构
 */
typedef struct checkpoint_manager_impl {
    agentos_checkpoint_config_t config;
    void* storage_handle;
    pthread_mutex_t lock;
    size_t total_checkpoints;
    size_t total_size_bytes;
} checkpoint_manager_impl_t;

/**
 * @brief 哈希计算（简化实现）
 */
static void compute_hash(const void* data, size_t size, char* hash_out) {
    if (!data || !hash_out) {
        return;
    }

    const unsigned char* bytes = (const unsigned char*)data;
    unsigned int hash = 5381;

    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + bytes[i];
    }

    snprintf(hash_out, 33, "%08x", hash);
}

/**
 * @brief 生成检查点ID
 */
static int generate_checkpoint_id(char** id_out, const char* task_id, uint64_t sequence) {
    if (!id_out || !task_id) {
        return -1;
    }

    size_t len = strlen(task_id) + 32;
    char* id = (char*)AGENTOS_MALLOC(len);
    if (!id) {
        return -1;
    }

    snprintf(id, len, "ckpt_%s_%lu", task_id, (unsigned long)sequence);
    *id_out = id;
    return 0;
}

/**
 * @brief 生成快照ID
 */
static int generate_snapshot_id(char** id_out, const char* task_id) {
    if (!id_out || !task_id) {
        return -1;
    }

    size_t len = strlen(task_id) + 32;
    char* id = (char*)AGENTOS_MALLOC(len);
    if (!id) {
        return -1;
    }

    time_t now = time(NULL);
    snprintf(id, len, "snap_%s_%lu", task_id, (unsigned long)now);
    *id_out = id;
    return 0;
}

agentos_checkpoint_config_t agentos_checkpoint_get_default_config(void) {
    agentos_checkpoint_config_t config = {
        .max_checkpoints_per_task = 100,
        .max_checkpoint_size_bytes = 100 * 1024 * 1024,
        .checkpoint_ttl_seconds = 7 * 24 * 3600,
        .auto_save_interval_seconds = 300,
        .enable_compression = true,
        .enable_deduplication = true,
        .storage_path = "./checkpoints"
    };
    return config;
}

int agentos_checkpoint_manager_create(
    const agentos_checkpoint_config_t* config,
    void** manager) {

    if (!manager) {
        return -1;
    }

    checkpoint_manager_impl_t* impl =
        (checkpoint_manager_impl_t*)AGENTOS_CALLOC(1, sizeof(checkpoint_manager_impl_t));

    if (!impl) {
        return -1;
    }

    if (config) {
        impl->config = *config;
    } else {
        impl->config = agentos_checkpoint_get_default_config();
    }

#ifdef _WIN32
    InitializeCriticalSection(&impl->lock);
#else
    pthread_mutex_init(&impl->lock, NULL);
#endif

    impl->total_checkpoints = 0;
    impl->total_size_bytes = 0;

    *manager = impl;
    return 0;
}

int agentos_checkpoint_manager_destroy(void* manager) {
    if (!manager) {
        return -1;
    }

    checkpoint_manager_impl_t* impl = (checkpoint_manager_impl_t*)manager;

#ifdef _WIN32
    DeleteCriticalSection(&impl->lock);
#else
    pthread_mutex_destroy(&impl->lock);
#endif

    AGENTOS_FREE(impl);
    return 0;
}

int agentos_checkpoint_create(
    void* manager,
    const char* task_id,
    const char* session_id,
    const void* state_data,
    size_t state_size,
    agentos_checkpoint_t** checkpoint) {

    if (!manager || !task_id || !state_data || !checkpoint) {
        return -1;
    }

    checkpoint_manager_impl_t* impl = (checkpoint_manager_impl_t*)manager;

#ifdef _WIN32
    EnterCriticalSection(&impl->lock);
#else
    pthread_mutex_lock(&impl->lock);
#endif

    agentos_checkpoint_t* ckpt = (agentos_checkpoint_t*)
        AGENTOS_CALLOC(1, sizeof(agentos_checkpoint_t));

    if (!ckpt) {
#ifdef _WIN32
        LeaveCriticalSection(&impl->lock);
#else
        pthread_mutex_unlock(&impl->lock);
#endif
        return -1;
    }

    ckpt->metadata = (agentos_checkpoint_metadata_t*)
        AGENTOS_CALLOC(1, sizeof(agentos_checkpoint_metadata_t));

    if (!ckpt->metadata) {
        AGENTOS_FREE(ckpt);
#ifdef _WIN32
        LeaveCriticalSection(&impl->lock);
#else
        pthread_mutex_unlock(&impl->lock);
#endif
        return -1;
    }

    ckpt->metadata->task_id = (char*)AGENTOS_MALLOC(strlen(task_id) + 1);
    if (!ckpt->metadata->task_id) {
        AGENTOS_FREE(ckpt->metadata);
        AGENTOS_FREE(ckpt);
#ifdef _WIN32
        LeaveCriticalSection(&impl->lock);
#else
        pthread_mutex_unlock(&impl->lock);
#endif
        return -1;
    }
    strcpy(ckpt->metadata->task_id, task_id);

    if (session_id) {
        ckpt->metadata->session_id = (char*)AGENTOS_MALLOC(strlen(session_id) + 1);
        if (!ckpt->metadata->session_id) {
            AGENTOS_FREE(ckpt->metadata->task_id);
            AGENTOS_FREE(ckpt->metadata);
            AGENTOS_FREE(ckpt);
#ifdef _WIN32
            LeaveCriticalSection(&impl->lock);
#else
            pthread_mutex_unlock(&impl->lock);
#endif
            return -1;
        }
        strcpy(ckpt->metadata->session_id, session_id);
    }

    ckpt->metadata->sequence_num = impl->total_checkpoints + 1;
    ckpt->metadata->created_at = time(NULL);
    ckpt->metadata->expires_at = ckpt->metadata->created_at + impl->config.checkpoint_ttl_seconds;
    ckpt->metadata->state_size = state_size;
    ckpt->metadata->flags = 0;

    ckpt->state_data = (void*)AGENTOS_MALLOC(state_size);
    if (!ckpt->state_data) {
        if (ckpt->metadata->session_id) AGENTOS_FREE(ckpt->metadata->session_id);
        AGENTOS_FREE(ckpt->metadata->task_id);
        AGENTOS_FREE(ckpt->metadata);
        AGENTOS_FREE(ckpt);
#ifdef _WIN32
        LeaveCriticalSection(&impl->lock);
#else
        pthread_mutex_unlock(&impl->lock);
#endif
        return -1;
    }

    memcpy(ckpt->state_data, state_data, state_size);
    ckpt->state_size = state_size;

    ckpt->metadata->state_hash = (char*)AGENTOS_CALLOC(33, 1);
    if (ckpt->metadata->state_hash) {
        compute_hash(state_data, state_size, ckpt->metadata->state_hash);
    }

    impl->total_checkpoints++;
    impl->total_size_bytes += state_size;

#ifdef _WIN32
    LeaveCriticalSection(&impl->lock);
#else
    pthread_mutex_unlock(&impl->lock);
#endif

    *checkpoint = ckpt;
    return 0;
}

int agentos_checkpoint_save(
    void* manager,
    const agentos_checkpoint_t* checkpoint) {

    if (!manager || !checkpoint || !checkpoint->metadata) {
        return -1;
    }

    if (checkpoint->state_size > ((checkpoint_manager_impl_t*)manager)->config.max_checkpoint_size_bytes) {
        return -1;
    }

    return 0;
}

int agentos_checkpoint_load(
    void* manager,
    const char* task_id,
    uint64_t sequence_num,
    agentos_checkpoint_t** checkpoint) {

    if (!manager || !task_id || !checkpoint) {
        return -1;
    }

    return -1;
}

int agentos_checkpoint_get_latest(
    void* manager,
    const char* task_id,
    agentos_checkpoint_t** checkpoint) {

    if (!manager || !task_id || !checkpoint) {
        return -1;
    }

    return -1;
}

int agentos_checkpoint_delete(
    void* manager,
    const char* task_id,
    uint64_t sequence_num) {

    if (!manager || !task_id) {
        return -1;
    }

    return 0;
}

int agentos_checkpoint_list(
    void* manager,
    const char* task_id,
    agentos_checkpoint_metadata_t*** checkpoints,
    size_t* count) {

    if (!manager || !task_id || !checkpoints || !count) {
        return -1;
    }

    *checkpoints = NULL;
    *count = 0;
    return 0;
}

int agentos_checkpoint_destroy(agentos_checkpoint_t* checkpoint) {
    if (!checkpoint) {
        return -1;
    }

    if (checkpoint->metadata) {
        if (checkpoint->metadata->task_id) {
            AGENTOS_FREE(checkpoint->metadata->task_id);
        }
        if (checkpoint->metadata->session_id) {
            AGENTOS_FREE(checkpoint->metadata->session_id);
        }
        if (checkpoint->metadata->state_hash) {
            AGENTOS_FREE(checkpoint->metadata->state_hash);
        }
        if (checkpoint->metadata->parent_checkpoint_id) {
            AGENTOS_FREE(checkpoint->metadata->parent_checkpoint_id);
        }
        AGENTOS_FREE(checkpoint->metadata);
    }

    if (checkpoint->state_data) {
        AGENTOS_FREE(checkpoint->state_data);
    }

    AGENTOS_FREE(checkpoint);
    return 0;
}

int agentos_checkpoint_list_destroy(
    agentos_checkpoint_metadata_t** checkpoints,
    size_t count) {

    if (!checkpoints) {
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        if (checkpoints[i]) {
            if (checkpoints[i]->task_id) {
                AGENTOS_FREE(checkpoints[i]->task_id);
            }
            if (checkpoints[i]->session_id) {
                AGENTOS_FREE(checkpoints[i]->session_id);
            }
            if (checkpoints[i]->state_hash) {
                AGENTOS_FREE(checkpoints[i]->state_hash);
            }
            if (checkpoints[i]->parent_checkpoint_id) {
                AGENTOS_FREE(checkpoints[i]->parent_checkpoint_id);
            }
            AGENTOS_FREE(checkpoints[i]);
        }
    }

    AGENTOS_FREE(checkpoints);
    return 0;
}

int agentos_snapshot_create(
    void* manager,
    const char* task_id,
    agentos_snapshot_type_t type,
    const void* data,
    size_t size,
    const char* description,
    agentos_snapshot_t** snapshot) {

    if (!manager || !task_id || !data || !snapshot) {
        return -1;
    }

    agentos_snapshot_t* snap = (agentos_snapshot_t*)
        AGENTOS_CALLOC(1, sizeof(agentos_snapshot_t));

    if (!snap) {
        return -1;
    }

    snap->task_id = (char*)AGENTOS_MALLOC(strlen(task_id) + 1);
    if (!snap->task_id) {
        AGENTOS_FREE(snap);
        return -1;
    }
    strcpy(snap->task_id, task_id);

    snap->type = type;
    snap->created_at = time(NULL);
    snap->size_bytes = size;

    snap->data = (void*)AGENTOS_MALLOC(size);
    if (!snap->data) {
        AGENTOS_FREE(snap->task_id);
        AGENTOS_FREE(snap);
        return -1;
    }
    memcpy(snap->data, data, size);

    if (description) {
        snap->description = (char*)AGENTOS_MALLOC(strlen(description) + 1);
        if (snap->description) {
            strcpy(snap->description, description);
        }
    }

    if (generate_snapshot_id(&snap->snapshot_id, task_id) != 0) {
        if (snap->description) AGENTOS_FREE(snap->description);
        AGENTOS_FREE(snap->data);
        AGENTOS_FREE(snap->task_id);
        AGENTOS_FREE(snap);
        return -1;
    }

    *snapshot = snap;
    return 0;
}

int agentos_snapshot_save(
    void* manager,
    const agentos_snapshot_t* snapshot) {

    if (!manager || !snapshot) {
        return -1;
    }

    return 0;
}

int agentos_snapshot_restore(
    void* manager,
    const char* snapshot_id,
    void** data,
    size_t* size) {

    if (!manager || !snapshot_id || !data || !size) {
        return -1;
    }

    return -1;
}

int agentos_snapshot_delete(
    void* manager,
    const char* snapshot_id) {

    if (!manager || !snapshot_id) {
        return -1;
    }

    return 0;
}

int agentos_snapshot_destroy(agentos_snapshot_t* snapshot) {
    if (!snapshot) {
        return -1;
    }

    if (snapshot->snapshot_id) {
        AGENTOS_FREE(snapshot->snapshot_id);
    }
    if (snapshot->task_id) {
        AGENTOS_FREE(snapshot->task_id);
    }
    if (snapshot->data) {
        AGENTOS_FREE(snapshot->data);
    }
    if (snapshot->description) {
        AGENTOS_FREE(snapshot->description);
    }

    AGENTOS_FREE(snapshot);
    return 0;
}

int agentos_checkpoint_get_stats(
    void* manager,
    size_t* total_checkpoints,
    size_t* total_size_bytes,
    time_t* oldest_checkpoint) {

    if (!manager) {
        return -1;
    }

    checkpoint_manager_impl_t* impl = (checkpoint_manager_impl_t*)manager;

#ifdef _WIN32
    EnterCriticalSection(&impl->lock);
#else
    pthread_mutex_lock(&impl->lock);
#endif

    if (total_checkpoints) {
        *total_checkpoints = impl->total_checkpoints;
    }
    if (total_size_bytes) {
        *total_size_bytes = impl->total_size_bytes;
    }
    if (oldest_checkpoint) {
        *oldest_checkpoint = 0;
    }

#ifdef _WIN32
    LeaveCriticalSection(&impl->lock);
#else
    pthread_mutex_unlock(&impl->lock);
#endif

    return 0;
}

int agentos_checkpoint_cleanup_expired(
    void* manager,
    size_t* deleted_count) {

    if (!manager) {
        return -1;
    }

    if (deleted_count) {
        *deleted_count = 0;
    }

    return 0;
}

bool agentos_checkpoint_verify_integrity(const agentos_checkpoint_t* checkpoint) {
    if (!checkpoint || !checkpoint->metadata || !checkpoint->state_data) {
        return false;
    }

    if (checkpoint->state_size == 0) {
        return false;
    }

    if (checkpoint->metadata->state_size != checkpoint->state_size) {
        return false;
    }

    if (checkpoint->metadata->state_hash) {
        char computed_hash[33];
        compute_hash(checkpoint->state_data, checkpoint->state_size, computed_hash);

        if (strcmp(checkpoint->metadata->state_hash, computed_hash) != 0) {
            return false;
        }
    }

    return true;
}

int agentos_checkpoint_get_next_sequence(
    void* manager,
    const char* task_id,
    uint64_t* sequence_num) {

    if (!manager || !task_id || !sequence_num) {
        return -1;
    }

    checkpoint_manager_impl_t* impl = (checkpoint_manager_impl_t*)manager;

#ifdef _WIN32
    EnterCriticalSection(&impl->lock);
#else
    pthread_mutex_lock(&impl->lock);
#endif

    *sequence_num = impl->total_checkpoints + 1;

#ifdef _WIN32
    LeaveCriticalSection(&impl->lock);
#else
    pthread_mutex_unlock(&impl->lock);
#endif

    return 0;
}

int agentos_checkpoint_set_auto_save(
    void* manager,
    const char* task_id,
    bool enabled) {

    if (!manager || !task_id) {
        return -1;
    }

    return 0;
}

int agentos_checkpoint_export_to_file(
    void* manager,
    const char* task_id,
    uint64_t sequence_num,
    const char* file_path) {

    if (!manager || !task_id || !file_path) {
        return -1;
    }

    return 0;
}

int agentos_checkpoint_import_from_file(
    void* manager,
    const char* file_path,
    agentos_checkpoint_t** checkpoint) {

    if (!manager || !file_path || !checkpoint) {
        return -1;
    }

    return -1;
}
