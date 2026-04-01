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

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
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
