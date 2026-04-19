/**
 * @file layer1_raw.c
 * @brief L1 原始卷实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/layer1_raw.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#endif

#define DEFAULT_QUEUE_SIZE 1024
#define DEFAULT_WORKERS 4

static int is_path_component_safe(const char* id) {
    if (!id || !*id) return 0;
    for (const char* p = id; *p; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' ||
            *p == '?' || *p == '"' || *p == '<' || *p == '>' || *p == '|') {
            return 0;
        }
    }
    if (strstr(id, "..") != NULL) return 0;
    return 1;
}

/**
 * @brief L1 原始卷内部结构
 */
typedef struct agentos_layer1_raw_inner {
    char storage_path[256];
    uint32_t queue_size;
    uint32_t async_workers;
    void* queue;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
    pthread_t* workers;
} agentos_layer1_raw_inner_t;

struct agentos_layer1_raw {
    agentos_layer1_raw_inner_t* inner;
};

/**
 * @brief 队列条目
 */
typedef struct queue_entry {
    char* id;
    void* data;
    size_t len;
    struct queue_entry* next;
} queue_entry_t;

/**
 * @brief 异步队列
 */
typedef struct async_queue {
    queue_entry_t* head;
    queue_entry_t* tail;
    size_t count;
    size_t max_size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int shutdown;
} async_queue_t;

__attribute__((unused))
static async_queue_t* queue_create(size_t max_size) {
    async_queue_t* q = (async_queue_t*)AGENTOS_CALLOC(1, sizeof(async_queue_t));
    if (!q) return NULL;
    q->max_size = max_size;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
    return q;
}

__attribute__((unused))
static void queue_destroy(async_queue_t* q) {
    if (!q) return;
    pthread_mutex_lock(&q->mutex);
    q->shutdown = 1;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->mutex);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
    AGENTOS_FREE(q);
}

__attribute__((unused))
static agentos_error_t queue_push(async_queue_t* q, const char* id, const void* data, size_t len) {
    if (!q || !id) return AGENTOS_EINVAL;
    pthread_mutex_lock(&q->mutex);
    if (q->count >= q->max_size) {
        pthread_mutex_unlock(&q->mutex);
        return AGENTOS_EBUSY;
    }
    queue_entry_t* entry = (queue_entry_t*)AGENTOS_MALLOC(sizeof(queue_entry_t));
    if (!entry) {
        pthread_mutex_unlock(&q->mutex);
        return AGENTOS_ENOMEM;
    }
    entry->id = AGENTOS_STRDUP(id);
    entry->data = AGENTOS_MALLOC(len);
    if (!entry->data) {
        AGENTOS_FREE(entry->id);
        AGENTOS_FREE(entry);
        pthread_mutex_unlock(&q->mutex);
        return AGENTOS_ENOMEM;
    }
    memcpy(entry->data, data, len);
    entry->len = len;
    entry->next = NULL;
    if (q->tail) {
        q->tail->next = entry;
        q->tail = entry;
    } else {
        q->head = q->tail = entry;
    }
    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_create_async(
    const char* path,
    uint32_t queue_size,
    uint32_t workers,
    agentos_layer1_raw_t** out) {
    if (!out) return AGENTOS_EINVAL;

    agentos_layer1_raw_t* l1 = (agentos_layer1_raw_t*)AGENTOS_CALLOC(1, sizeof(agentos_layer1_raw_t));
    if (!l1) return AGENTOS_ENOMEM;

    l1->inner = (agentos_layer1_raw_inner_t*)AGENTOS_CALLOC(1, sizeof(agentos_layer1_raw_inner_t));
    if (!l1->inner) {
        AGENTOS_FREE(l1);
        return AGENTOS_ENOMEM;
    }

    if (path) {
        snprintf(l1->inner->storage_path, sizeof(l1->inner->storage_path), "%s", path);
    }

    l1->inner->queue_size = queue_size > 0 ? queue_size : DEFAULT_QUEUE_SIZE;
    l1->inner->async_workers = workers > 0 ? workers : DEFAULT_WORKERS;
    l1->inner->shutdown = 0;

    pthread_mutex_init(&l1->inner->mutex, NULL);
    pthread_cond_init(&l1->inner->cond, NULL);

    *out = l1;
    return AGENTOS_SUCCESS;
}

void agentos_layer1_raw_destroy(agentos_layer1_raw_t* l1) {
    if (!l1) return;
    if (l1->inner) {
        l1->inner->shutdown = 1;
        pthread_cond_broadcast(&l1->inner->cond);
        pthread_mutex_destroy(&l1->inner->mutex);
        pthread_cond_destroy(&l1->inner->cond);
        AGENTOS_FREE(l1->inner);
    }
    AGENTOS_FREE(l1);
}

agentos_error_t agentos_layer1_raw_write(
    agentos_layer1_raw_t* l1,
    const char* id,
    const void* data,
    size_t len) {
    if (!l1 || !id || !data) return AGENTOS_EINVAL;
    if (!is_path_component_safe(id)) return AGENTOS_EINVAL;

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s.dat", l1->inner->storage_path, id);

    FILE* fp = fopen(filepath, "wb");
    if (!fp) return AGENTOS_EIO;

    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);

    return (written == len) ? AGENTOS_SUCCESS : AGENTOS_EIO;
}

agentos_error_t agentos_layer1_raw_read(
    agentos_layer1_raw_t* l1,
    const char* id,
    void** out_data,
    size_t* out_len) {
    if (!l1 || !id || !out_data) return AGENTOS_EINVAL;
    if (!is_path_component_safe(id)) return AGENTOS_EINVAL;

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s.dat", l1->inner->storage_path, id);

    FILE* fp = fopen(filepath, "rb");
    if (!fp) return AGENTOS_ENOENT;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (len <= 0) {
        fclose(fp);
        return len < 0 ? AGENTOS_EIO : AGENTOS_ENOENT;
    }

    void* data = AGENTOS_MALLOC((size_t)len);
    if (!data) {
        fclose(fp);
        return AGENTOS_ENOMEM;
    }

    size_t read_len = fread(data, 1, len, fp);
    fclose(fp);

    if (read_len != (size_t)len) {
        AGENTOS_FREE(data);
        return AGENTOS_EIO;
    }

    *out_data = data;
    if (out_len) *out_len = len;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_delete(
    agentos_layer1_raw_t* l1,
    const char* id) {
    if (!l1 || !id) return AGENTOS_EINVAL;
    if (!is_path_component_safe(id)) return AGENTOS_EINVAL;

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s.dat", l1->inner->storage_path, id);

    if (remove(filepath) != 0) {
        return AGENTOS_ENOENT;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_list_ids(
    agentos_layer1_raw_t* l1,
    char*** out_ids,
    size_t* out_count) {
    if (!l1 || !out_ids || !out_count) return AGENTOS_EINVAL;

    *out_ids = NULL;
    *out_count = 0;

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_flush(
    agentos_layer1_raw_t* l1,
    uint32_t timeout_ms) {
    if (!l1) return AGENTOS_EINVAL;
    (void)timeout_ms;
    return AGENTOS_SUCCESS;
}

void agentos_free_string_array(char** arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) {
        if (arr[i]) AGENTOS_FREE(arr[i]);
    }
    AGENTOS_FREE(arr);
}
