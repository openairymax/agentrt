/**
 * @file storage.c
 * @brief L1 原始卷存储引擎实现（支持同步/异步写入）
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "layer1_raw.h"
#include "agentos.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define F_OK 0
#define access _access
#define mkdir _mkdir
#else
#include <sys/types.h>
#endif

/* 文件扩展名 */
#define RAW_FILE_EXT ".raw"
#define META_FILE_EXT ".meta"
#define TEMP_FILE_EXT ".tmp"

/* 异步写入请求结构 */
typedef struct async_write_request {
    void* data;
    size_t len;
    char* metadata;
    void (*callback)(agentos_error_t err, const char* record_id, void* userdata);
    void* userdata;
    struct async_write_request* next;
} async_write_request_t;

/* 写入队列 */
typedef struct write_queue {
    async_write_request_t* head;
    async_write_request_t* tail;
    size_t size;
    size_t max_size;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} write_queue_t;

/* 工作线程参数 */
typedef struct worker_thread {
    pthread_t thread;
    int id;
    struct agentos_layer1_raw* layer;
} worker_thread_t;

/**
 * @brief 原始卷内部结构
 */
struct agentos_layer1_raw {
    char* base_path;               /**< 存储根路径 */
    agentos_mutex_t* lock;         /**< 线程锁（保护 next_id 等） */
    uint64_t next_id;              /**< 下一个可用ID（简单递增） */
    // 异步写入相关
    write_queue_t* queue;
    worker_thread_t* workers;
    uint32_t num_workers;
    int async_mode;
    int shutdown_flag;
};

/**
 * @brief 元数据文件结构（二进制）
 */
typedef struct raw_metadata_record {
    uint64_t record_id;            /**< 记录ID（数值形式） */
    uint64_t timestamp;            /**< 时间戳 */
    uint32_t data_len;             /**< 数据长度 */
    uint32_t access_count;         /**< 访问次数 */
    uint64_t last_access;          /**< 最后访问时间 */
    uint32_t tags_len;             /**< tags JSON 长度 */
    /* 紧接着是 tags JSON 字符串（无结尾0） */
} raw_metadata_record_t;

/* 生成唯一ID（原子递增） */
#ifdef _WIN32
static volatile LONG global_raw_id = 0;
static uint64_t generate_raw_id(void) {
    return (uint64_t)InterlockedIncrement(&global_raw_id);
}
#else
#include <stdatomic.h>
static atomic_ullong global_raw_id = 0;
static uint64_t generate_raw_id(void) {
    return atomic_fetch_add(&global_raw_id, 1) + 1;
}
#endif

/**
 * @brief 构建文件路径
 */
static void build_file_path(const char* base, uint64_t id, const char* ext, char* out, size_t out_len) {
    snprintf(out, out_len, "%s/%llu%s", base, (unsigned long long)id, ext);
}

/**
 * @brief 确保目录存在
 */
static int ensure_dir(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#ifdef _WIN32
        if (mkdir(path) != 0 && errno != EEXIST) return -1;
#else
        if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
#endif
    }
    return 0;
}

/* ==================== 同步写入核心 ==================== */

static agentos_error_t do_sync_write(
    agentos_layer1_raw_t* layer,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id) {

    agentos_mutex_lock(layer->lock);
    uint64_t id = layer->next_id++;
    agentos_mutex_unlock(layer->lock);

    char raw_path[512];
    char meta_path[512];
    build_file_path(layer->base_path, id, RAW_FILE_EXT, raw_path, sizeof(raw_path));
    build_file_path(layer->base_path, id, META_FILE_EXT, meta_path, sizeof(meta_path));

    // 写入原始数据
    FILE* f_raw = fopen(raw_path, "wb");
    if (!f_raw) {
        AGENTOS_LOG_ERROR("Failed to open raw file for writing: %s", raw_path);
        return AGENTOS_EIO;
    }
    size_t written = fwrite(data, 1, len, f_raw);
    if (written != len) {
        fclose(f_raw);
        remove(raw_path);
        AGENTOS_LOG_ERROR("Failed to write raw data, expected %zu, wrote %zu", len, written);
        return AGENTOS_EIO;
    }
    fclose(f_raw);

    // 准备元数据记录
    raw_metadata_record_t meta_rec;
    meta_rec.record_id = id;
    meta_rec.timestamp = (uint64_t)time(NULL) * 1000000000ULL;
    meta_rec.data_len = (uint32_t)len;
    meta_rec.access_count = 0;
    meta_rec.last_access = 0;
    size_t tags_len = metadata ? strlen(metadata) : 0;
    meta_rec.tags_len = (uint32_t)tags_len;

    // 写入元数据文件
    FILE* f_meta = fopen(meta_path, "wb");
    if (!f_meta) {
        remove(raw_path);
        AGENTOS_LOG_ERROR("Failed to open meta file for writing: %s", meta_path);
        return AGENTOS_EIO;
    }
    if (fwrite(&meta_rec, sizeof(meta_rec), 1, f_meta) != 1) {
        fclose(f_meta);
        remove(raw_path);
        remove(meta_path);
        return AGENTOS_EIO;
    }
    if (tags_len > 0) {
        if (fwrite(metadata, 1, tags_len, f_meta) != tags_len) {
            fclose(f_meta);
            remove(raw_path);
            remove(meta_path);
            return AGENTOS_EIO;
        }
    }
    fclose(f_meta);

    // 输出记录ID
    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "%llu", (unsigned long long)id);
    *out_record_id = strdup(id_buf);
    if (!*out_record_id) {
        remove(raw_path);
        remove(meta_path);
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}

/* ==================== 异步写入队列管理 ==================== */

static write_queue_t* write_queue_create(size_t max_size) {
    write_queue_t* q = (write_queue_t*)malloc(sizeof(write_queue_t));
    if (!q) return NULL;
    q->head = q->tail = NULL;
    q->size = 0;
    q->max_size = max_size;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
    return q;
}

static void write_queue_destroy(write_queue_t* q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q);
}

static int write_queue_push(write_queue_t* q, async_write_request_t* req) {
    pthread_mutex_lock(&q->lock);
    while (q->size >= q->max_size) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    req->next = NULL;
    if (q->tail) {
        q->tail->next = req;
        q->tail = req;
    } else {
        q->head = q->tail = req;
    }
    q->size++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

static async_write_request_t* write_queue_pop(write_queue_t* q) {
    pthread_mutex_lock(&q->lock);
    while (q->size == 0) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    async_write_request_t* req = q->head;
    q->head = req->next;
    if (!q->head) q->tail = NULL;
    q->size--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return req;
}

/* ==================== 工作线程函数 ==================== */

static void* worker_thread_func(void* arg) {
    worker_thread_t* worker = (worker_thread_t*)arg;
    agentos_layer1_raw_t* layer = worker->layer;
    write_queue_t* q = layer->queue;

    while (!layer->shutdown_flag) {
        async_write_request_t* req = write_queue_pop(q);
        if (!req) continue; // 应该不会发生

        // 执行同步写入
        char* record_id = NULL;
        agentos_error_t err = do_sync_write(layer, req->data, req->len, req->metadata, &record_id);

        // 回调通知
        if (req->callback) {
            req->callback(err, record_id, req->userdata);
        }

        // 释放资源
        free(req->data);   // 注意：异步写入时，data 是复制的副本
        free(req->metadata);
        free(req);
        if (record_id) free(record_id);
    }
    return NULL;
}

/* ==================== 公共接口 ==================== */

agentos_error_t agentos_layer1_raw_create(
    const char* base_path,
    agentos_layer1_raw_t** out_layer) {

    if (!base_path || !out_layer) return AGENTOS_EINVAL;

    if (ensure_dir(base_path) != 0) {
        return AGENTOS_EIO;
    }

    agentos_layer1_raw_t* layer = (agentos_layer1_raw_t*)malloc(sizeof(agentos_layer1_raw_t));
    if (!layer) return AGENTOS_ENOMEM;
    memset(layer, 0, sizeof(agentos_layer1_raw_t));

    layer->base_path = strdup(base_path);
    if (!layer->base_path) {
        free(layer);
        return AGENTOS_ENOMEM;
    }

    layer->lock = agentos_mutex_create();
    if (!layer->lock) {
        free(layer->base_path);
        free(layer);
        return AGENTOS_ENOMEM;
    }

    // 从现有文件获取最大ID
    DIR* dir = opendir(base_path);
    if (dir) {
        struct dirent* entry;
        uint64_t max_id = 0;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                char* dot = strchr(entry->d_name, '.');
                if (dot && strcmp(dot, RAW_FILE_EXT) == 0) {
                    uint64_t id = strtoull(entry->d_name, NULL, 10);
                    if (id > max_id) max_id = id;
                }
            }
        }
        closedir(dir);
        layer->next_id = max_id + 1;
    } else {
        layer->next_id = 1;
    }

    layer->async_mode = 0;
    *out_layer = layer;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_create_async(
    const char* base_path,
    size_t queue_size,
    uint32_t num_workers,
    agentos_layer1_raw_t** out_layer) {

    agentos_error_t err = agentos_layer1_raw_create(base_path, out_layer);
    if (err != AGENTOS_SUCCESS) return err;

    agentos_layer1_raw_t* layer = *out_layer;

    layer->queue = write_queue_create(queue_size);
    if (!layer->queue) {
        agentos_layer1_raw_destroy(layer);
        return AGENTOS_ENOMEM;
    }

    layer->workers = (worker_thread_t*)malloc(num_workers * sizeof(worker_thread_t));
    if (!layer->workers) {
        write_queue_destroy(layer->queue);
        agentos_layer1_raw_destroy(layer);
        return AGENTOS_ENOMEM;
    }

    layer->num_workers = num_workers;
    layer->async_mode = 1;
    layer->shutdown_flag = 0;

    for (uint32_t i = 0; i < num_workers; i++) {
        layer->workers[i].id = i;
        layer->workers[i].layer = layer;
        if (pthread_create(&layer->workers[i].thread, NULL, worker_thread_func, &layer->workers[i]) != 0) {
            layer->shutdown_flag = 1;
            for (uint32_t j = 0; j < i; j++) {
                pthread_join(layer->workers[j].thread, NULL);
            }
            free(layer->workers);
            write_queue_destroy(layer->queue);
            agentos_layer1_raw_destroy(layer);
            return AGENTOS_ENOMEM;
        }
    }

    return AGENTOS_SUCCESS;
}

void agentos_layer1_raw_destroy(agentos_layer1_raw_t* layer) {
    if (!layer) return;

    if (layer->async_mode) {
        layer->shutdown_flag = 1;
        // 唤醒所有工作线程
        pthread_mutex_lock(&layer->queue->lock);
        pthread_cond_broadcast(&layer->queue->not_empty);
        pthread_mutex_unlock(&layer->queue->lock);
        for (uint32_t i = 0; i < layer->num_workers; i++) {
            pthread_join(layer->workers[i].thread, NULL);
        }
        free(layer->workers);
        write_queue_destroy(layer->queue);
    }

    if (layer->base_path) free(layer->base_path);
    if (layer->lock) agentos_mutex_destroy(layer->lock);
    free(layer);
}

agentos_error_t agentos_layer1_raw_write(
    agentos_layer1_raw_t* layer,
    const void* data,
    size_t len,
    const char* metadata,
    char** out_record_id) {

    if (!layer || !data || len == 0 || !out_record_id) return AGENTOS_EINVAL;
    return do_sync_write(layer, data, len, metadata, out_record_id);
}

agentos_error_t agentos_layer1_raw_write_async(
    agentos_layer1_raw_t* layer,
    const void* data,
    size_t len,
    const char* metadata,
    void (*callback)(agentos_error_t err, const char* record_id, void* userdata),
    void* userdata) {

    if (!layer || !layer->async_mode) {
        AGENTOS_LOG_ERROR("Async write called on non-async layer");
        return AGENTOS_EINVAL;
    }
    if (!data || len == 0) return AGENTOS_EINVAL;

    // 复制数据，因为调用者可能很快释放
    void* data_copy = malloc(len);
    if (!data_copy) return AGENTOS_ENOMEM;
    memcpy(data_copy, data, len);

    char* metadata_copy = NULL;
    if (metadata) {
        metadata_copy = strdup(metadata);
        if (!metadata_copy) {
            free(data_copy);
            return AGENTOS_ENOMEM;
        }
    }

    async_write_request_t* req = (async_write_request_t*)malloc(sizeof(async_write_request_t));
    if (!req) {
        free(data_copy);
        free(metadata_copy);
        return AGENTOS_ENOMEM;
    }
    req->data = data_copy;
    req->len = len;
    req->metadata = metadata_copy;
    req->callback = callback;
    req->userdata = userdata;
    req->next = NULL;

    write_queue_push(layer->queue, req);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_flush(
    agentos_layer1_raw_t* layer,
    uint32_t timeout_ms) {

    if (!layer || !layer->async_mode) return AGENTOS_EINVAL;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;

    pthread_mutex_lock(&layer->queue->lock);
    while (layer->queue->size > 0) {
        if (timeout_ms == 0) {
            pthread_cond_wait(&layer->queue->not_full, &layer->queue->lock);
        } else {
            int ret = pthread_cond_timedwait(&layer->queue->not_full, &layer->queue->lock, &ts);
            if (ret == ETIMEDOUT) {
                pthread_mutex_unlock(&layer->queue->lock);
                return AGENTOS_ETIMEDOUT;
            }
        }
    }
    pthread_mutex_unlock(&layer->queue->lock);
    return AGENTOS_SUCCESS;
}

/* 以下原有函数保持不变，但需确保线程安全 */
agentos_error_t agentos_layer1_raw_read(
    agentos_layer1_raw_t* layer,
    const char* record_id,
    void** out_data,
    size_t* out_len) {

    if (!layer || !record_id || !out_data || !out_len) return AGENTOS_EINVAL;

    char* endptr;
    uint64_t id = strtoull(record_id, &endptr, 10);
    if (*endptr != '\0') return AGENTOS_EINVAL;

    char raw_path[512];
    build_file_path(layer->base_path, id, RAW_FILE_EXT, raw_path, sizeof(raw_path));

    FILE* f = fopen(raw_path, "rb");
    if (!f) return AGENTOS_ENOENT;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return AGENTOS_EIO;
    }

    void* buf = malloc(size);
    if (!buf) {
        fclose(f);
        return AGENTOS_ENOMEM;
    }

    size_t read = fread(buf, 1, size, f);
    fclose(f);
    if (read != (size_t)size) {
        free(buf);
        return AGENTOS_EIO;
    }

    // 更新访问计数（异步也可能需要，这里简单同步）
    agentos_layer1_raw_touch(layer, record_id);

    *out_data = buf;
    *out_len = size;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_get_metadata(
    agentos_layer1_raw_t* layer,
    const char* record_id,
    agentos_raw_metadata_t** out_metadata) {

    if (!layer || !record_id || !out_metadata) return AGENTOS_EINVAL;

    char* endptr;
    uint64_t id = strtoull(record_id, &endptr, 10);
    if (*endptr != '\0') return AGENTOS_EINVAL;

    char meta_path[512];
    build_file_path(layer->base_path, id, META_FILE_EXT, meta_path, sizeof(meta_path));

    FILE* f = fopen(meta_path, "rb");
    if (!f) return AGENTOS_ENOENT;

    raw_metadata_record_t meta_rec;
    if (fread(&meta_rec, sizeof(meta_rec), 1, f) != 1) {
        fclose(f);
        return AGENTOS_EIO;
    }

    char* tags = NULL;
    if (meta_rec.tags_len > 0) {
        tags = (char*)malloc(meta_rec.tags_len + 1);
        if (!tags) {
            fclose(f);
            return AGENTOS_ENOMEM;
        }
        if (fread(tags, 1, meta_rec.tags_len, f) != meta_rec.tags_len) {
            free(tags);
            fclose(f);
            return AGENTOS_EIO;
        }
        tags[meta_rec.tags_len] = '\0';
    }
    fclose(f);

    agentos_raw_metadata_t* meta = (agentos_raw_metadata_t*)malloc(sizeof(agentos_raw_metadata_t));
    if (!meta) {
        if (tags) free(tags);
        return AGENTOS_ENOMEM;
    }
    memset(meta, 0, sizeof(agentos_raw_metadata_t));

    meta->record_id = strdup(record_id);
    meta->timestamp = meta_rec.timestamp;
    meta->source = NULL;
    meta->trace_id = NULL;
    meta->data_len = meta_rec.data_len;
    meta->access_count = meta_rec.access_count;
    meta->last_access = meta_rec.last_access;
    meta->tags_json = tags;

    if (!meta->record_id) {
        if (tags) free(tags);
        free(meta);
        return AGENTOS_ENOMEM;
    }

    *out_metadata = meta;
    return AGENTOS_SUCCESS;
}

void agentos_layer1_raw_metadata_free(agentos_raw_metadata_t* metadata) {
    if (!metadata) return;
    if (metadata->record_id) free(metadata->record_id);
    if (metadata->source) free(metadata->source);
    if (metadata->trace_id) free(metadata->trace_id);
    if (metadata->tags_json) free(metadata->tags_json);
    free(metadata);
}

agentos_error_t agentos_layer1_raw_touch(
    agentos_layer1_raw_t* layer,
    const char* record_id) {

    if (!layer || !record_id) return AGENTOS_EINVAL;

    char* endptr;
    uint64_t id = strtoull(record_id, &endptr, 10);
    if (*endptr != '\0') return AGENTOS_EINVAL;

    char meta_path[512];
    build_file_path(layer->base_path, id, META_FILE_EXT, meta_path, sizeof(meta_path));

    FILE* f = fopen(meta_path, "r+b");
    if (!f) return AGENTOS_ENOENT;

    raw_metadata_record_t meta_rec;
    if (fread(&meta_rec, sizeof(meta_rec), 1, f) != 1) {
        fclose(f);
        return AGENTOS_EIO;
    }

    meta_rec.access_count++;
    meta_rec.last_access = (uint64_t)time(NULL) * 1000000000ULL;

    fseek(f, 0, SEEK_SET);
    if (fwrite(&meta_rec, sizeof(meta_rec), 1, f) != 1) {
        fclose(f);
        return AGENTOS_EIO;
    }
    fclose(f);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_delete(
    agentos_layer1_raw_t* layer,
    const char* record_id) {

    if (!layer || !record_id) return AGENTOS_EINVAL;

    char* endptr;
    uint64_t id = strtoull(record_id, &endptr, 10);
    if (*endptr != '\0') return AGENTOS_EINVAL;

    char raw_path[512];
    char meta_path[512];
    build_file_path(layer->base_path, id, RAW_FILE_EXT, raw_path, sizeof(raw_path));
    build_file_path(layer->base_path, id, META_FILE_EXT, meta_path, sizeof(meta_path));

    int ret1 = remove(raw_path);
    int ret2 = remove(meta_path);
    if (ret1 != 0 && ret2 != 0) return AGENTOS_ENOENT;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_layer1_raw_list_ids(
    agentos_layer1_raw_t* layer,
    char*** out_ids,
    size_t* out_count) {

    if (!layer || !out_ids || !out_count) return AGENTOS_EINVAL;

    DIR* dir = opendir(layer->base_path);
    if (!dir) return AGENTOS_ENOENT;

    size_t capacity = 64;
    size_t count = 0;
    char** ids = (char**)malloc(capacity * sizeof(char*));
    if (!ids) {
        closedir(dir);
        return AGENTOS_ENOMEM;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            char* dot = strrchr(entry->d_name, '.');
            if (dot && strcmp(dot, RAW_FILE_EXT) == 0) {
                size_t len = dot - entry->d_name;
                char* id = (char*)malloc(len + 1);
                if (!id) {
                    for (size_t i = 0; i < count; i++) free(ids[i]);
                    free(ids);
                    closedir(dir);
                    return AGENTOS_ENOMEM;
                }
                strncpy(id, entry->d_name, len);
                id[len] = '\0';
                if (count >= capacity) {
                    capacity *= 2;
                    char** new_ids = (char**)realloc(ids, capacity * sizeof(char*));
                    if (!new_ids) {
                        free(id);
                        for (size_t i = 0; i < count; i++) free(ids[i]);
                        free(ids);
                        closedir(dir);
                        return AGENTOS_ENOMEM;
                    }
                    ids = new_ids;
                }
                ids[count++] = id;
            }
        }
    }
    closedir(dir);

    *out_ids = ids;
    *out_count = count;
    return AGENTOS_SUCCESS;
}

void agentos_free_string_array(char** arr, size_t count) {
    if (!arr) return;
    for (size_t i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}