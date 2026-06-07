/**
 * @file builtin_storage.c
 * @brief AgentOS 内置存储实现（文件系统 + JSON 索引）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 提供基于文件系统的 L1 原始存储和 JSON 索引。
 * 无 FAISS / SQLite 依赖，纯 C 实现。
 */

#include "error.h"
#include "memory_compat.h"
#include "memory_provider.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define MKDIR(p) _mkdir(p)

static unsigned short secure_random_short(void)
{
    unsigned int val = 0;
    if (rand_s(&val) == 0) {
        unsigned short result = (unsigned short)(val & 0xFFFF);
        return result ? result : 1;
    }
    unsigned short fallback = (unsigned short)(time(NULL) ^ ((size_t)&val >> 4));
    return fallback ? fallback : 1;
}
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p, 0755)

static unsigned short secure_random_short(void)
{
    int fd = open("/dev/urandom", O_RDONLY);
    unsigned short val = 0;
    if (fd >= 0) {
        ssize_t n = read(fd, &val, sizeof(val));
        close(fd);
        if (n == sizeof(val))
            return val;
    }
    unsigned short fallback = (unsigned short)(time(NULL) ^ ((size_t)&val >> 4));
    return fallback ? fallback : 1;
}
#endif

#define AGENTOS_STORAGE_VERSION "1.0.0"
#define AGENTOS_MAX_PATH_LEN 512
#define AGENTOS_MAX_RECORDS 1000000
#define AGENTOS_INDEX_FILE "index.json"

typedef struct storage_record {
    char record_id[64];
    char file_path[AGENTOS_MAX_PATH_LEN];
    size_t data_len;
    time_t created_at;
    time_t updated_at;
    char metadata_json[1024];
} storage_record_t;

typedef struct builtin_storage {
    char base_path[AGENTOS_MAX_PATH_LEN];
    storage_record_t *records;
    size_t record_count;
    size_t record_capacity;
    void *lock;
} builtin_storage_t;

static void ensure_dir(const char *path)
{
    char tmp[AGENTOS_MAX_PATH_LEN] = {0};
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            MKDIR(tmp);
            *p = '/';
        }
    }
    MKDIR(tmp);
}

static void generate_record_id(char *out, size_t out_size)
{
    snprintf(out, out_size, "rec-%lld-%04x", (long long)time(NULL),
             (unsigned)secure_random_short());
}

builtin_storage_t *builtin_storage_create(const char *base_path)
{
    builtin_storage_t *st = (builtin_storage_t *)AGENTOS_CALLOC(1, sizeof(builtin_storage_t));
    if (!st)
        return NULL;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(st->base_path, sizeof(st->base_path), "%s",
             base_path ? base_path : "./data/agentos/memory");
    ensure_dir(st->base_path);

    char data_dir[AGENTOS_MAX_PATH_LEN];
    snprintf(data_dir, sizeof(data_dir), "%s/data", st->base_path);
#pragma GCC diagnostic pop
    ensure_dir(data_dir);

    st->record_capacity = 1024;
    st->records = (storage_record_t *)AGENTOS_CALLOC(st->record_capacity, sizeof(storage_record_t));
    if (!st->records) {
        AGENTOS_FREE(st);
        return NULL;
    }

    st->lock = NULL;
    return st;
}

void builtin_storage_destroy(builtin_storage_t *st)
{
    if (!st)
        return;
    if (st->records)
        AGENTOS_FREE(st->records);
    AGENTOS_FREE(st);
}

agentos_error_t builtin_storage_write(builtin_storage_t *st, const void *data, size_t len,
                                      const char *metadata_json, char **out_record_id)
{

    if (!st || !data || len == 0 || !out_record_id)
        return AGENTOS_EINVAL;

    if (st->record_count >= st->record_capacity) {
        size_t new_cap = st->record_capacity * 2;
        if (new_cap > AGENTOS_MAX_RECORDS)
            return AGENTOS_ENOMEM;
        if (new_cap > SIZE_MAX / sizeof(storage_record_t))
            return AGENTOS_ENOMEM;
        storage_record_t *tmp =
            (storage_record_t *)AGENTOS_REALLOC(st->records, new_cap * sizeof(storage_record_t));
        if (!tmp)
            return AGENTOS_ENOMEM;
        st->records = tmp;
        st->record_capacity = new_cap;
    }

    storage_record_t *rec = &st->records[st->record_count];
    generate_record_id(rec->record_id, sizeof(rec->record_id));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(rec->file_path, sizeof(rec->file_path), "%s/data/%s.bin", st->base_path,
             rec->record_id);
#pragma GCC diagnostic pop

    FILE *fp = fopen(rec->file_path, "wb");
    if (!fp)
        return AGENTOS_EIO;

    size_t written = fwrite(data, 1, len, fp);
    fclose(fp);

    if (written != len) {
        remove(rec->file_path);
        return AGENTOS_EIO;
    }

    rec->data_len = len;
    time(&rec->created_at);
    rec->updated_at = rec->created_at;

    if (metadata_json) {
        snprintf(rec->metadata_json, sizeof(rec->metadata_json), "%s", metadata_json);
    } else {
        rec->metadata_json[0] = '\0';
    }

    *out_record_id = AGENTOS_STRDUP(rec->record_id);
    if (!*out_record_id)
        return AGENTOS_ENOMEM;

    st->record_count++;
    return AGENTOS_SUCCESS;
}

agentos_error_t builtin_storage_get(builtin_storage_t *st, const char *record_id, void **out_data,
                                    size_t *out_len)
{

    if (!st || !record_id || !out_data || !out_len)
        return AGENTOS_EINVAL;

    for (size_t i = 0; i < st->record_count; i++) {
        if (strcmp(st->records[i].record_id, record_id) == 0) {
            FILE *fp = fopen(st->records[i].file_path, "rb");
            if (!fp)
                return AGENTOS_EIO;

            size_t fsize = st->records[i].data_len;
            if (fsize == 0) {
                *out_data = NULL;
                *out_len = 0;
                return AGENTOS_SUCCESS;
            }
            void *buf = AGENTOS_MALLOC(fsize);
            if (!buf) {
                fclose(fp);
                return AGENTOS_ENOMEM;
            }

            size_t nread = fread(buf, 1, fsize, fp);
            fclose(fp);

            if (nread != fsize) {
                AGENTOS_FREE(buf);
                return AGENTOS_EIO;
            }

            *out_data = buf;
            *out_len = fsize;
            return AGENTOS_SUCCESS;
        }
    }

    return AGENTOS_ENOENT;
}

agentos_error_t builtin_storage_delete(builtin_storage_t *st, const char *record_id)
{

    if (!st || !record_id)
        return AGENTOS_EINVAL;

    for (size_t i = 0; i < st->record_count; i++) {
        if (strcmp(st->records[i].record_id, record_id) == 0) {
            remove(st->records[i].file_path);

            if (i < st->record_count - 1) {
                __builtin_memmove(&st->records[i], &st->records[i + 1],
                        (st->record_count - i - 1) * sizeof(storage_record_t));
            }
            st->record_count--;
            return AGENTOS_SUCCESS;
        }
    }

    return AGENTOS_ENOENT;
}

size_t builtin_storage_count(const builtin_storage_t *st)
{
    return st ? st->record_count : 0;
}

const storage_record_t *builtin_storage_get_record(const builtin_storage_t *st, size_t index)
{
    if (!st || index >= st->record_count)
        return NULL;
    return &st->records[index];
}

const char *builtin_storage_get_metadata(const builtin_storage_t *st, const char *record_id)
{
    if (!st || !record_id)
        return NULL;
    for (size_t i = 0; i < st->record_count; i++) {
        if (strcmp(st->records[i].record_id, record_id) == 0) {
            return st->records[i].metadata_json;
        }
    }
    return NULL;
}

const char *builtin_storage_get_record_id(const builtin_storage_t *st, size_t index)
{
    if (!st || index >= st->record_count)
        return NULL;
    return st->records[index].record_id;
}

time_t builtin_storage_get_updated_at(const builtin_storage_t *st, size_t index)
{
    if (!st || index >= st->record_count)
        return 0;
    return st->records[index].updated_at;
}

agentos_error_t builtin_storage_touch(builtin_storage_t *st, const char *record_id)
{
    if (!st || !record_id)
        return AGENTOS_EINVAL;
    for (size_t i = 0; i < st->record_count; i++) {
        if (strcmp(st->records[i].record_id, record_id) == 0) {
            st->records[i].updated_at = time(NULL);
            return AGENTOS_SUCCESS;
        }
    }
    return AGENTOS_ENOENT;
}
