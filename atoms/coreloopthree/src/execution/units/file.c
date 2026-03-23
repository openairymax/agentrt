/**
 * @file file.c
 * @brief 文件操作执行单元（读/写/删除/列表）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

typedef struct file_unit_data {
    char* root_dir;          // 允许操作的根目录
    char* metadata_json;
} file_unit_data_t;

static agentos_error_t file_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    file_unit_data_t* data = (file_unit_data_t*)unit->data;
    if (!data || !input) return AGENTOS_EINVAL;

    // 假设 input 是 JSON 字符串，包含操作和路径
    // 为了简化，我们用简单的格式 "op=read&path=/file.txt"
    const char* cmd = (const char*)input;
    char op[32] = {0};
    char path[256] = {0};
    if (sscanf(cmd, "op=%31[^&]&path=%255s", op, path) != 2) {
        return AGENTOS_EINVAL;
    }

    // 安全性检查：确保路径在 root_dir 内
    char full_path[512];
    if (data->root_dir) {
        snprintf(full_path, sizeof(full_path), "%s/%s", data->root_dir, path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", path);
    }
    // 可添加规范化检查防止路径遍历

    if (strcmp(op, "read") == 0) {
        FILE* f = fopen(full_path, "rb");
        if (!f) return AGENTOS_ENOENT;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* content = (char*)malloc(size + 1);
        if (!content) {
            fclose(f);
            return AGENTOS_ENOMEM;
        }
        fread(content, 1, size, f);
        content[size] = '\0';
        fclose(f);
        *out_output = content;
        return AGENTOS_SUCCESS;
    } else if (strcmp(op, "write") == 0) {
        // 需要数据，这里简化：假设内容也包含在 input 中，但我们的格式不支持
        return AGENTOS_ENOTSUP;
    } else if (strcmp(op, "delete") == 0) {
        if (remove(full_path) == 0) {
            *out_output = strdup("deleted");
            return AGENTOS_SUCCESS;
        } else {
            return AGENTOS_ENOENT;
        }
    } else if (strcmp(op, "list") == 0) {
#ifdef _WIN32
        WIN32_FIND_DATAA find_data;
        char search_path[PATH_MAX];
        snprintf(search_path, sizeof(search_path), "%s\\*", full_path);
        HANDLE hFind = FindFirstFileA(search_path, &find_data);
        if (hFind == INVALID_HANDLE_VALUE) return AGENTOS_ENOENT;
        size_t cap = 1024;
        char* listing = (char*)malloc(cap);
        if (!listing) {
            FindClose(hFind);
            return AGENTOS_ENOMEM;
        }
        size_t pos = 0;
        listing[0] = '\0';
        do {
            if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                size_t len = strlen(find_data.cFileName);
                if (pos + len + 2 > cap) {
                    cap *= 2;
                    char* new_list = (char*)realloc(listing, cap);
                    if (!new_list) { free(listing); FindClose(hFind); return AGENTOS_ENOMEM; }
                    listing = new_list;
                }
                if (pos > 0) listing[pos++] = '\n';
                strcpy(listing + pos, find_data.cFileName);
                pos += len;
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
        listing[pos] = '\0';
        *out_output = listing;
        return AGENTOS_SUCCESS;
#else
        DIR* dir = opendir(full_path);
        if (!dir) return AGENTOS_ENOENT;
        struct dirent* entry;
        size_t cap = 1024;
        char* listing = (char*)malloc(cap);
        if (!listing) {
            closedir(dir);
            return AGENTOS_ENOMEM;
        }
        size_t pos = 0;
        listing[0] = '\0';
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            size_t len = strlen(entry->d_name);
            if (pos + len + 2 > cap) {
                cap *= 2;
                char* new_list = (char*)realloc(listing, cap);
                if (!new_list) {
                    free(listing);
                    closedir(dir);
                    return AGENTOS_ENOMEM;
                }
                listing = new_list;
            }
            if (pos > 0) {
                listing[pos++] = '\n';
            }
            strcpy(listing + pos, entry->d_name);
            pos += len;
        }
        listing[pos] = '\0';
        closedir(dir);
        *out_output = listing;
        return AGENTOS_SUCCESS;
#endif
    } else {
        return AGENTOS_ENOTSUP;
    }
}

static void file_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    file_unit_data_t* data = (file_unit_data_t*)unit->data;
    if (data) {
        if (data->root_dir) free(data->root_dir);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
    }
    free(unit);
}

static const char* file_get_metadata(agentos_execution_unit_t* unit) {
    file_unit_data_t* data = (file_unit_data_t*)unit->data;
    return data ? data->metadata_json : NULL;
}

agentos_execution_unit_t* agentos_file_unit_create(const char* root_dir) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)malloc(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;

    file_unit_data_t* data = (file_unit_data_t*)malloc(sizeof(file_unit_data_t));
    if (!data) {
        free(unit);
        return NULL;
    }

    data->root_dir = root_dir ? strdup(root_dir) : NULL;
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"file\",\"root_dir\":\"%s\"}", root_dir ? root_dir : "");
    data->metadata_json = strdup(meta);

    if (!data->metadata_json || (root_dir && !data->root_dir)) {
        if (data->root_dir) free(data->root_dir);
        if (data->metadata_json) free(data->metadata_json);
        free(data);
        free(unit);
        return NULL;
    }

    unit->data = data;
    unit->execute = file_execute;
    unit->destroy = file_destroy;
    unit->get_metadata = file_get_metadata;

    return unit;
}