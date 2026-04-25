/**
 * @file utils.c
 * @brief AgentOS heapstore 公共工具函数实现
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * "From data intelligence emerges."
 */

#include "utils.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#endif

bool heapstore_ensure_directory(const char* path) {
    if (!path || !path[0]) {
        return false;
    }

#ifdef _WIN32
    if (_access(path, 0) == 0) {
        return true;
    }

    char tmp[1024];
    char* p = NULL;
    size_t len;

    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    len = strlen(tmp);

    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            if (_mkdir(tmp) != 0 && errno != EEXIST) {
            }
            *p = '/';
        }
    }

    if (_mkdir(tmp) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
#else
    struct stat st = {0};

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    char tmp[1024];
    char* p = NULL;
    size_t len;

    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    len = strlen(tmp);

    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
#endif
}

bool heapstore_calculate_directory_size(const char* path, uint64_t* out_size, uint32_t* out_count) {
    if (!path || !path[0] || !out_size || !out_count) {
        return false;
    }

    *out_size = 0;
    *out_count = 0;

#ifdef _WIN32
    char search_path[MAX_PATH];
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;

    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }

    do {
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* 递归计算子目录 */
            char sub_path[MAX_PATH];
            snprintf(sub_path, sizeof(sub_path), "%s\\%s", path, find_data.cFileName);

            uint64_t sub_size = 0;
            uint32_t sub_count = 0;
            if (heapstore_calculate_directory_size(sub_path, &sub_size, &sub_count)) {
                *out_size += sub_size;
                *out_count += sub_count;
            }
        } else {
            LARGE_INTEGER file_size;
            file_size.HighPart = find_data.nFileSizeHigh;
            file_size.LowPart = find_data.nFileSizeLow;
            *out_size += (uint64_t)file_size.QuadPart;
            (*out_count)++;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
#else
    DIR* dir = opendir(path);
    if (!dir) {
        return false;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            /* 递归计算子目录 */
            uint64_t sub_size = 0;
            uint32_t sub_count = 0;
            if (heapstore_calculate_directory_size(full_path, &sub_size, &sub_count)) {
                *out_size += sub_size;
                *out_count += sub_count;
            }
        } else {
            *out_size += (uint64_t)st.st_size;
            (*out_count)++;
        }
    }

    closedir(dir);
#endif

    return true;
}

int heapstore_sanitize_path_component(char* output, const char* input, size_t size) {
    if (!output || !input || size == 0) {
        return -1;
    }

    size_t input_len = strlen(input);
    if (input_len == 0 || input_len >= size) {
        return -1;
    }

    if (strstr(input, "..") != NULL) {
        return -1;
    }

    if (strchr(input, '/') != NULL) {
        return -1;
    }

    if (strchr(input, '\\') != NULL) {
        return -1;
    }

    for (size_t i = 0; i < input_len; i++) {
        if (input[i] == '\0') {
            return -1;
        }
    }

    for (size_t i = 0; i < input_len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (isalnum(c) || c == '_' || c == '-' || c == '.') {
            output[i] = input[i];
        } else {
            output[i] = '_';
        }
    }
    output[input_len] = '\0';

    return 0;
}

bool heapstore_is_safe_identifier(const char* input) {
    if (!input || !input[0]) {
        return false;
    }

    if (strstr(input, "..") != NULL) {
        return false;
    }

    if (strchr(input, '/') != NULL) {
        return false;
    }

    if (strchr(input, '\\') != NULL) {
        return false;
    }

    size_t len = strlen(input);
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\0') {
            return false;
        }
        unsigned char c = (unsigned char)input[i];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) {
            return false;
        }
    }

    return true;
}
