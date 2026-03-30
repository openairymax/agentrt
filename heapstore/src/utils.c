/**
 * @file utils.c
 * @brief AgentOS heapstore 公共工具函数实现
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

bool heapstore_ensure_directory(const char* path) {
    if (!path || strlen(path) == 0) {
        return false;
    }

    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    size_t len = strlen(path_copy);
    for (size_t i = 0; i < len; i++) {
        if (path_copy[i] == '\\' || path_copy[i] == '/') {
            if (i > 0 && path_copy[i - 1] != ':') {
                path_copy[i] = '\0';
                if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
                    return false;
                }
                path_copy[i] = '/';
            }
        }
    }

    if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
        return false;
    }

    return true;
}
