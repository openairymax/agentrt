/**
 * @file layer1_raw_io.c
 * @brief L1 原始卷文件 I/O 操作实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "layer1_raw_io.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#endif

static void perform_exponential_backoff(uint8_t attempt) {
    uint32_t delay_ms = INITIAL_RETRY_DELAY_MS << attempt;
    if (delay_ms > MAX_RETRY_DELAY_MS) {
        delay_ms = MAX_RETRY_DELAY_MS;
    }

#ifdef _WIN32
    Sleep(delay_ms);
#else
    struct timespec ts;
    ts.tv_sec = delay_ms / 1000;
    ts.tv_nsec = (delay_ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

static void delete_file_safely(const char* file_path) {
    if (!file_path) return;

#ifdef _WIN32
    DeleteFileA(file_path);
#else
    unlink(file_path);
#endif
}

agentos_error_t layer1_raw_ensure_directory_exists(const char* path) {
    if (!path) return AGENTOS_EINVAL;

#ifdef _WIN32
    char buffer[MAX_FILE_PATH_LENGTH];
    char* p = buffer;

    strncpy(buffer, path, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    while (*p != '\0') {
        while (*p != '\0' && *p != '\\' && *p != '/') p++;

        char save = *p;
        *p = '\0';

        if (buffer[0] != '\0' &&
            !(strlen(buffer) == 2 && buffer[1] == ':')) {
            if (_access(buffer, 0) != 0) {
                if (_mkdir(buffer) != 0 && errno != EEXIST) {
                    return AGENTOS_EIO;
                }
            }
        }

        if (save != '\0') {
            *p = save;
            p++;
        }
    }
#else
    struct stat st;
    if (stat(path, &st) != 0) {
        if (mkdir(path, 0755) != 0 && errno != EEXIST) {
            return AGENTOS_EIO;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        return AGENTOS_ENOTDIR;
    }
#endif

    return AGENTOS_SUCCESS;
}

agentos_error_t layer1_raw_build_file_path(const char* storage_path,
                                            const char* id,
                                            char* buffer,
                                            size_t buffer_size) {
    if (!storage_path || !id || !buffer) {
        return AGENTOS_EINVAL;
    }

    if (buffer_size == 0) {
        return AGENTOS_EINVAL;
    }

    for (const char* p = id; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '*' || *p == '?' ||
            *p == '"' || *p == '<' || *p == '>' || *p == '|') {
            AGENTOS_LOG_WARN("Invalid character in record ID: %s", id);
            return AGENTOS_EINVAL;
        }
    }

    int written = snprintf(buffer, buffer_size, "%s%s%s.dat",
                          storage_path, PATH_SEPARATOR, id);

    if (written < 0 || (size_t)written >= buffer_size) {
        return AGENTOS_ENAMETOOLONG;
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t layer1_raw_safe_write_file(const char* file_path,
                                            const void* data,
                                            size_t data_len,
                                            uint8_t retry_count) {
    if (!file_path || !data || data_len == 0) {
        return AGENTOS_EINVAL;
    }

    agentos_error_t result = AGENTOS_EIO;
    uint8_t actual_retries = retry_count > 0 ? retry_count : MAX_WRITE_RETRIES;

    for (uint8_t attempt = 0; attempt <= actual_retries; attempt++) {
        FILE* fp = NULL;

#ifdef _WIN32
        fp = fopen(file_path, "wb");
#else
        int fd = open(file_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd >= 0) {
            fp = fdopen(fd, "wb");
        }
#endif

        if (!fp) {
            if (attempt < actual_retries) {
                perform_exponential_backoff(attempt);
                continue;
            }
            AGENTOS_LOG_ERROR("Failed to open file for writing: %s", file_path);
            return AGENTOS_EIO;
        }

        size_t total_written = 0;
        const uint8_t* data_ptr = (const uint8_t*)data;
        int write_failed = 0;

        while (total_written < data_len) {
            size_t to_write = data_len - total_written;
            if (to_write > FILE_WRITE_BUFFER_SIZE) {
                to_write = FILE_WRITE_BUFFER_SIZE;
            }

            size_t written = fwrite(data_ptr + total_written, 1, to_write, fp);
            if (written != to_write) {
                fclose(fp);
                write_failed = 1;
                break;
            }

            total_written += written;
        }

        if (write_failed) {
            if (attempt < actual_retries) {
                delete_file_safely(file_path);
                perform_exponential_backoff(attempt);
                continue;
            }
            AGENTOS_LOG_ERROR("Failed to write to file: %s", file_path);
            return AGENTOS_EIO;
        }

        if (fflush(fp) != 0) {
            fclose(fp);
            if (attempt < actual_retries) {
                delete_file_safely(file_path);
                perform_exponential_backoff(attempt);
                continue;
            }
            AGENTOS_LOG_ERROR("Failed to flush file: %s", file_path);
            return AGENTOS_EIO;
        }

        fclose(fp);

#ifdef _WIN32
        WIN32_FILE_ATTRIBUTE_DATA file_info;
        if (GetFileAttributesExA(file_path, GetFileExInfoStandard, &file_info)) {
            LARGE_INTEGER size;
            size.HighPart = file_info.nFileSizeHigh;
            size.LowPart = file_info.nFileSizeLow;
            if (size.QuadPart == (LONGLONG)data_len) {
                result = AGENTOS_SUCCESS;
                break;
            }
        }
#else
        struct stat st;
        if (stat(file_path, &st) == 0 && st.st_size == (off_t)data_len) {
            result = AGENTOS_SUCCESS;
            break;
        }
#endif

        if (attempt < actual_retries) {
            delete_file_safely(file_path);
            perform_exponential_backoff(attempt);
        } else {
            result = AGENTOS_EIO;
        }
    }

    return result;
}

agentos_error_t layer1_raw_safe_read_file(const char* file_path,
                                          void** out_data,
                                          size_t* out_len) {
    if (!file_path || !out_data || !out_len) {
        return AGENTOS_EINVAL;
    }

    FILE* fp = NULL;

#ifdef _WIN32
    fp = fopen(file_path, "rb");
#else
    int fd = open(file_path, O_RDONLY);
    if (fd >= 0) {
        fp = fdopen(fd, "rb");
    }
#endif

    if (!fp) {
        return AGENTOS_ENOENT;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return AGENTOS_EIO;
    }

    long file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return AGENTOS_EIO;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return AGENTOS_EIO;
    }

    if (file_size == 0) {
        fclose(fp);
        *out_data = NULL;
        *out_len = 0;
        return AGENTOS_SUCCESS;
    }

    void* data = AGENTOS_MALLOC((size_t)file_size);
    if (!data) {
        fclose(fp);
        return AGENTOS_ENOMEM;
    }

    size_t total_read = 0;
    while (total_read < (size_t)file_size) {
        size_t to_read = (size_t)file_size - total_read;
        if (to_read > FILE_WRITE_BUFFER_SIZE) {
            to_read = FILE_WRITE_BUFFER_SIZE;
        }

        size_t read = fread((uint8_t*)data + total_read, 1, to_read, fp);
        if (read != to_read) {
            AGENTOS_FREE(data);
            fclose(fp);
            return AGENTOS_EIO;
        }

        total_read += read;
    }

    fclose(fp);

    *out_data = data;
    *out_len = (size_t)file_size;

    return AGENTOS_SUCCESS;
}
