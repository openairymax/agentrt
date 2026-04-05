/**
 * @file async_storage_utils.c
 * @brief L1 原始卷异步存储工具函数实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "async_storage_utils.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

agentos_error_t async_storage_ensure_directory_exists(const char* path) {
    if (!path) return AGENTOS_EINVAL;

#ifdef _WIN32
    if (_mkdir(path) != 0 && errno != EEXIST) {
        AGENTOS_LOG_ERROR("Failed to create directory %s: %d", path, errno);
        return AGENTOS_EFAIL;
    }
#else
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        AGENTOS_LOG_ERROR("Failed to create directory %s: %d", path, errno);
        return AGENTOS_EFAIL;
    }
#endif

    return AGENTOS_SUCCESS;
}

agentos_error_t async_storage_build_file_path(const char* storage_path, const char* id,
                                              char* file_path, size_t max_len) {
    if (!storage_path || !id || !file_path) return AGENTOS_EINVAL;

    int written = snprintf(file_path, max_len, "%s/%s.raw", storage_path, id);
    if (written < 0 || (size_t)written >= max_len) {
        AGENTOS_LOG_ERROR("File path too long: %s/%s.raw", storage_path, id);
        return AGENTOS_EINVAL;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t async_storage_safe_write_file(const char* file_path, const void* data,
                                              size_t data_len) {
    if (!file_path || !data) return AGENTOS_EINVAL;

    char temp_path[MAX_FILE_PATH];
    int written = snprintf(temp_path, sizeof(temp_path), "%s.tmp", file_path);
    if (written < 0 || (size_t)written >= sizeof(temp_path)) {
        return AGENTOS_EINVAL;
    }

    FILE* f = fopen(temp_path, "wb");
    if (!f) {
        AGENTOS_LOG_ERROR("Failed to open temp file %s: %d", temp_path, errno);
        return AGENTOS_EFAIL;
    }

    size_t written_bytes = fwrite(data, 1, data_len, f);
    fclose(f);

    if (written_bytes != data_len) {
        AGENTOS_LOG_ERROR("Failed to write temp file %s: wrote %zu of %zu bytes",
                         temp_path, written_bytes, data_len);
        remove(temp_path);
        return AGENTOS_EFAIL;
    }

#ifdef _WIN32
    if (MoveFileExA(temp_path, file_path, MOVEFILE_REPLACE_EXISTING) == 0) {
        AGENTOS_LOG_ERROR("Failed to rename temp file %s to %s: %lu",
                         temp_path, file_path, GetLastError());
        remove(temp_path);
        return AGENTOS_EFAIL;
    }
#else
    if (rename(temp_path, file_path) != 0) {
        AGENTOS_LOG_ERROR("Failed to rename temp file %s to %s: %d",
                         temp_path, file_path, errno);
        remove(temp_path);
        return AGENTOS_EFAIL;
    }
#endif

    return AGENTOS_SUCCESS;
}

agentos_error_t async_storage_safe_read_file(const char* file_path, void** out_data,
                                             size_t* out_len) {
    if (!file_path || !out_data || !out_len) return AGENTOS_EINVAL;

    FILE* f = fopen(file_path, "rb");
    if (!f) {
        if (errno == ENOENT) {
            return AGENTOS_ENOTFOUND;
        }
        AGENTOS_LOG_ERROR("Failed to open file %s: %d", file_path, errno);
        return AGENTOS_EFAIL;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(f);
        AGENTOS_LOG_ERROR("Failed to get file size %s: %d", file_path, errno);
        return AGENTOS_EFAIL;
    }

    void* data = AGENTOS_MALLOC((size_t)file_size);
    if (!data) {
        fclose(f);
        AGENTOS_LOG_ERROR("Failed to allocate memory for file %s: size=%ld",
                         file_path, file_size);
        return AGENTOS_ENOMEM;
    }

    size_t read_bytes = fread(data, 1, (size_t)file_size, f);
    fclose(f);

    if (read_bytes != (size_t)file_size) {
        AGENTOS_FREE(data);
        AGENTOS_LOG_ERROR("Failed to read file %s: read %zu of %ld bytes",
                         file_path, read_bytes, file_size);
        return AGENTOS_EFAIL;
    }

    *out_data = data;
    *out_len = (size_t)file_size;

    return AGENTOS_SUCCESS;
}

agentos_error_t async_storage_safe_delete_file(const char* file_path) {
    if (!file_path) return AGENTOS_EINVAL;

    if (remove(file_path) != 0) {
        if (errno == ENOENT) {
            return AGENTOS_ENOTFOUND;
        }
        AGENTOS_LOG_ERROR("Failed to delete file %s: %d", file_path, errno);
        return AGENTOS_EFAIL;
    }

    return AGENTOS_SUCCESS;
}
