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
#include <agentos/memory.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define PATH_SEPARATOR "\\"
#else
#include <unistd.h>
#include <dirent.h>
#define PATH_SEPARATOR "/"
#endif

typedef struct file_unit_data {
    char* root_dir;
    char* metadata_json;
} file_unit_data_t;

static agentos_error_t file_build_path(
    file_unit_data_t* data,
    const char* path,
    char* out_full,
    size_t max_len) {
    if (!data || !path || !out_full) return AGENTOS_EINVAL;
    if (data->root_dir) {
        snprintf(out_full, max_len, "%s/%s", data->root_dir, path);
    } else {
        snprintf(out_full, max_len, "%s", path);
    }
    return AGENTOS_SUCCESS;
}

static agentos_error_t file_do_read(
    const char* full_path,
    void** out_output) {
    FILE* f = fopen(full_path, "rb");
    if (!f) return AGENTOS_ENOENT;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return AGENTOS_EIO; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return AGENTOS_EIO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return AGENTOS_EIO; }

    char* content = (char*)AGENTOS_MALLOC((size_t)size + 1);
    if (!content) { fclose(f); return AGENTOS_ENOMEM; }

    size_t bytes_read = fread(content, 1, (size_t)size, f);
    if (bytes_read != (size_t)size) { AGENTOS_FREE(content); fclose(f); return AGENTOS_EIO; }

    content[size] = '\0';
    fclose(f);
    *out_output = content;
    return AGENTOS_SUCCESS;
}

static agentos_error_t file_do_delete(
    const char* full_path,
    void** out_output) {
    if (remove(full_path) == 0) {
        *out_output = AGENTOS_STRDUP("deleted");
        return AGENTOS_SUCCESS;
    }
    return AGENTOS_ENOENT;
}

#ifdef _WIN32
static agentos_error_t file_do_list_win(
    const char* full_path,
    void** out_output) {
    WIN32_FIND_DATAA find_data;
    char search_path[PATH_MAX];
    snprintf(search_path, sizeof(search_path), "%s" PATH_SEPARATOR "*", full_path);

    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return AGENTOS_ENOENT;

    size_t cap = 1024;
    char* listing = (char*)AGENTOS_MALLOC(cap);
    if (!listing) { FindClose(hFind); return AGENTOS_ENOMEM; }

    size_t pos = 0;
    listing[0] = '\0';

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) continue;
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            size_t len = strlen(find_data.cFileName);
            if (pos + len + 2 > cap) {
                cap *= 2;
                char* new_list = (char*)AGENTOS_REALLOC(listing, cap);
                if (!new_list) { AGENTOS_FREE(listing); FindClose(hFind); return AGENTOS_ENOMEM; }
                listing = new_list;
            }
            if (pos > 0) listing[pos++] = '\n';
            snprintf(listing + pos, cap - pos, "%s", find_data.cFileName);
            pos += len;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    listing[pos] = '\0';
    *out_output = listing;
    return AGENTOS_SUCCESS;
}
#else
static agentos_error_t file_do_list_posix(
    const char* full_path,
    void** out_output) {
    DIR* dir = opendir(full_path);
    if (!dir) return AGENTOS_ENOENT;

    struct dirent* entry;
    size_t cap = 1024;
    char* listing = (char*)AGENTOS_MALLOC(cap);
    if (!listing) { closedir(dir); return AGENTOS_ENOMEM; }

    size_t pos = 0;
    listing[0] = '\0';

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        size_t len = strlen(entry->d_name);
        if (pos + len + 2 > cap) {
            cap *= 2;
            char* new_list = (char*)AGENTOS_REALLOC(listing, cap);
            if (!new_list) { AGENTOS_FREE(listing); closedir(dir); return AGENTOS_ENOMEM; }
            listing = new_list;
        }
        if (pos > 0) listing[pos++] = '\n';
        snprintf(listing + pos, cap - pos, "%s", entry->d_name);
        pos += len;
    }

    listing[pos] = '\0';
    closedir(dir);
    *out_output = listing;
    return AGENTOS_SUCCESS;
}
#endif

static agentos_error_t file_execute(agentos_execution_unit_t* unit, const void* input, void** out_output) {
    file_unit_data_t* data = (file_unit_data_t*)unit->execution_unit_data;
    if (!data || !input) return AGENTOS_EINVAL;

    const char* cmd = (const char*)input;
    char op[32] = {0};
    char path[256] = {0};

    if (sscanf(cmd, "op=%31[^&]&path=%255s", op, path) != 2) {
        return AGENTOS_EINVAL;
    }

    char full_path[512];
    agentos_error_t err = file_build_path(data, path, full_path, sizeof(full_path));
    if (err != AGENTOS_SUCCESS) return err;

    if (strcmp(op, "read") == 0) {
        return file_do_read(full_path, out_output);
    } else if (strcmp(op, "delete") == 0) {
        return file_do_delete(full_path, out_output);
    } else if (strcmp(op, "list") == 0) {
#ifdef _WIN32
        return file_do_list_win(full_path, out_output);
#else
        return file_do_list_posix(full_path, out_output);
#endif
    }
    return AGENTOS_ENOTSUP;
}

static void file_destroy(agentos_execution_unit_t* unit) {
    if (!unit) return;
    file_unit_data_t* data = (file_unit_data_t*)unit->execution_unit_data;
    if (data) {
        if (data->root_dir) AGENTOS_FREE(data->root_dir);
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
    }
    AGENTOS_FREE(unit);
}

agentos_execution_unit_t* agentos_file_unit_create(const char* root_dir) {
    agentos_execution_unit_t* unit = (agentos_execution_unit_t*)AGENTOS_MALLOC(sizeof(agentos_execution_unit_t));
    if (!unit) return NULL;
    memset(unit, 0, sizeof(*unit));

    file_unit_data_t* data = (file_unit_data_t*)AGENTOS_MALLOC(sizeof(file_unit_data_t));
    if (!data) {
        AGENTOS_FREE(unit);
        return NULL;
    }

    data->root_dir = root_dir ? AGENTOS_STRDUP(root_dir) : NULL;
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"file\",\"root_dir\":\"%s\"}", root_dir ? root_dir : "");
    data->metadata_json = AGENTOS_STRDUP(meta);

    if (!data->metadata_json || (root_dir && !data->root_dir)) {
        if (data->root_dir) AGENTOS_FREE(data->root_dir);
        if (data->metadata_json) AGENTOS_FREE(data->metadata_json);
        AGENTOS_FREE(data);
        AGENTOS_FREE(unit);
        return NULL;
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = file_execute;
    unit->execution_unit_destroy = file_destroy;

    return unit;
}
