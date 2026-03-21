/**
 * @file file_utils.c
 * @brief 文件操作实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

char* agentos_io_read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read = fread(buf, 1, size, f);
    fclose(f);
    if (read != (size_t)size) {
        free(buf);
        return NULL;
    }
    buf[size] = '\0';
    if (out_len) *out_len = size;
    return buf;
}

int agentos_io_write_file(const char* path, const void* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    if (len == (size_t)-1) len = strlen((const char*)data);
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

int agentos_io_ensure_dir(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#ifdef _WIN32
        if (mkdir(path) != 0) return -1;
#else
        if (mkdir(path, 0755) != 0) return -1;
#endif
    }
    return 0;
}

int agentos_io_list_files(const char* path, char*** out_files, size_t* out_count) {
    DIR* dir = opendir(path);
    if (!dir) return -1;
    size_t capacity = 64;
    size_t count = 0;
    char** files = (char**)malloc(capacity * sizeof(char*));
    if (!files) {
        closedir(dir);
        return -1;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            if (count >= capacity) {
                capacity *= 2;
                char** new_files = (char**)realloc(files, capacity * sizeof(char*));
                if (!new_files) {
                    for (size_t i = 0; i < count; i++) free(files[i]);
                    free(files);
                    closedir(dir);
                    return -1;
                }
                files = new_files;
            }
            files[count] = strdup(entry->d_name);
            if (!files[count]) {
                for (size_t i = 0; i < count; i++) free(files[i]);
                free(files);
                closedir(dir);
                return -1;
            }
            count++;
        }
    }
    closedir(dir);
    *out_files = files;
    *out_count = count;
    return 0;
}

void agentos_io_free_list(char** files, size_t count) {
    for (size_t i = 0; i < count; i++) free(files[i]);
    free(files);
}