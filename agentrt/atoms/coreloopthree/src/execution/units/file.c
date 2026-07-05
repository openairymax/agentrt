/**
 * @file file.c
 * @brief 文件操作执行单元（读/写/删除/列表）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt.h"
#include "execution.h"
#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "error.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define PATH_SEPARATOR "\\"
#else
#include "agentrt_dirent.h"

#include <limits.h>
#include <unistd.h>
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)

#define PATH_SEPARATOR "/"
#endif

typedef struct file_unit_data {
    char *root_dir;
    char *metadata_json;
} file_unit_data_t;

static int is_path_traversal_attempt(const char *path)
{
    if (!path)
        return 1;
    if (path[0] == '/' || path[0] == '\\')
        return 1;
    if (strstr(path, "..") != NULL)
        return 1;
    if (strstr(path, "//") != NULL)
        return 1;
    if (strstr(path, "\\\\") != NULL)
        return 1;
    return 0;
}

static agentrt_error_t file_build_path(file_unit_data_t *data, const char *path, char *out_full,
                                       size_t max_len)
{
    if (!data || !path || !out_full)
        ATM_RET_ERR(AGENTRT_EINVAL);

    if (is_path_traversal_attempt(path)) {
        ATM_RET_ERR(AGENTRT_EPERM);
    }

    if (!data->root_dir) {
        ATM_RET_ERR(AGENTRT_EPERM);
    }

    snprintf(out_full, max_len, "%s/%s", data->root_dir, path);

#ifndef _WIN32
    {
        char resolved_root[PATH_MAX];
        char resolved_full[PATH_MAX];
        if (realpath(data->root_dir, resolved_root)) {
            if (realpath(out_full, resolved_full)) {
                size_t root_len = strlen(resolved_root);
                if (strncmp(resolved_full, resolved_root, root_len) != 0) {
                    ATM_RET_ERR(AGENTRT_EPERM);
                }
            } else {
                char resolved_parent[PATH_MAX];
                char parent_path[512];
                snprintf(parent_path, sizeof(parent_path), "%s/%s", data->root_dir, ".");
                if (realpath(parent_path, resolved_parent)) {
                    size_t parent_len = strlen(resolved_parent);
                    char full_copy[512];
                    snprintf(full_copy, sizeof(full_copy), "%s", out_full);
                    char *last_sep = strrchr(full_copy, '/');
                    if (last_sep) {
                        *last_sep = '\0';
                        char resolved_dir[PATH_MAX];
                        if (realpath(full_copy, resolved_dir)) {
                            if (strncmp(resolved_dir, resolved_parent, parent_len) != 0) {
                                ATM_RET_ERR(AGENTRT_EPERM);
                            }
                        }
                    }
                }
            }
        }
    }
#endif

    return AGENTRT_SUCCESS;
}

static agentrt_error_t file_do_read(const char *full_path, void **out_output)
{
    FILE *f = fopen(full_path, "rb");
    if (!f)
        ATM_RET_ERR(AGENTRT_ENOENT);

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        ATM_RET_ERR(AGENTRT_EIO);
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        ATM_RET_ERR(AGENTRT_EIO);
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        ATM_RET_ERR(AGENTRT_EIO);
    }

    char *content = (char *)AGENTRT_MALLOC((size_t)size + 1);
    if (!content) {
        fclose(f);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    size_t bytes_read = fread(content, 1, (size_t)size, f);
    if (bytes_read != (size_t)size) {
        AGENTRT_FREE(content);
        fclose(f);
        ATM_RET_ERR(AGENTRT_EIO);
    }

    content[size] = '\0';
    fclose(f);
    *out_output = content;
    return AGENTRT_SUCCESS;
}

static agentrt_error_t file_do_delete(const char *full_path, void **out_output)
{
    if (remove(full_path) == 0) {
        *out_output = AGENTRT_STRDUP("deleted");
        return AGENTRT_SUCCESS;
    }
    ATM_RET_ERR(AGENTRT_ENOENT);
}

#ifdef _WIN32
static agentrt_error_t file_do_list_win(const char *full_path, void **out_output)
{
    WIN32_FIND_DATAA find_data;
    char search_path[PATH_MAX];
    snprintf(search_path, sizeof(search_path), "%s" PATH_SEPARATOR "*", full_path);

    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE)
        ATM_RET_ERR(AGENTRT_ENOENT);

    size_t cap = 1024;
    char *listing = (char *)AGENTRT_MALLOC(cap);
    if (!listing) {
        FindClose(hFind);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    size_t pos = 0;
    listing[0] = '\0';

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
            continue;
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            size_t len = strlen(find_data.cFileName);
            if (pos + len + 2 > cap) {
                cap *= 2;
                char *new_list = (char *)AGENTRT_REALLOC(listing, cap);
                if (!new_list) {
                    AGENTRT_FREE(listing);
                    FindClose(hFind);
                    ATM_RET_ERR(AGENTRT_ENOMEM);
                }
                listing = new_list;
            }
            if (pos > 0)
                listing[pos++] = '\n';
            snprintf(listing + pos, cap - pos, "%s", find_data.cFileName);
            pos += len;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    listing[pos] = '\0';
    *out_output = listing;
    return AGENTRT_SUCCESS;
}
#else
static agentrt_error_t file_do_list_posix(const char *full_path, void **out_output)
{
    DIR *dir = opendir(full_path);
    if (!dir)
        ATM_RET_ERR(AGENTRT_ENOENT);

    struct dirent *entry;
    size_t cap = 1024;
    char *listing = (char *)AGENTRT_MALLOC(cap);
    if (!listing) {
        closedir(dir);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    size_t pos = 0;
    listing[0] = '\0';

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        size_t len = strlen(entry->d_name);
        if (pos + len + 2 > cap) {
            cap *= 2;
            char *new_list = (char *)AGENTRT_REALLOC(listing, cap);
            if (!new_list) {
                AGENTRT_FREE(listing);
                closedir(dir);
                ATM_RET_ERR(AGENTRT_ENOMEM);
            }
            listing = new_list;
        }
        if (pos > 0)
            listing[pos++] = '\n';
        snprintf(listing + pos, cap - pos, "%s", entry->d_name);
        pos += len;
    }

    listing[pos] = '\0';
    closedir(dir);
    *out_output = listing;
    return AGENTRT_SUCCESS;
}
#endif

static agentrt_error_t file_execute(agentrt_execution_unit_t *unit, const void *input,
                                    void **out_output)
{
    file_unit_data_t *data = (file_unit_data_t *)unit->execution_unit_data;
    if (!data || !input)
        ATM_RET_ERR(AGENTRT_EINVAL);

    const char *cmd = (const char *)input;
    char op[32] = {0};
    char path[256] = {0};

    const char *op_start = cmd + 3;
    const char *op_end = strchr(op_start, '&');
    if (!op_end) { ATM_RET_ERR(AGENTRT_EINVAL); }
    size_t op_len = (size_t)(op_end - op_start);
    if (op_len >= sizeof(op)) op_len = sizeof(op) - 1;
    __builtin_memcpy(op, op_start, op_len);
    op[op_len] = '\0';
    const char *path_start = op_end + 6;
    size_t path_avail = strlen(path_start);
    if (path_avail >= sizeof(path)) path_avail = sizeof(path) - 1;
    __builtin_memcpy(path, path_start, path_avail);
    path[path_avail] = '\0';
    if (op[0] == '\0' || path[0] == '\0') {
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    char full_path[512];
    agentrt_error_t err = file_build_path(data, path, full_path, sizeof(full_path));
    if (err != AGENTRT_SUCCESS)
        return err;

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
    ATM_RET_ERR(AGENTRT_EPROTONOSUPPORT);
}

static void file_destroy(agentrt_execution_unit_t *unit)
{
    if (!unit)
        return;
    file_unit_data_t *data = (file_unit_data_t *)unit->execution_unit_data;
    if (data) {
        if (data->root_dir)
            AGENTRT_FREE(data->root_dir);
        if (data->metadata_json)
            AGENTRT_FREE(data->metadata_json);
        AGENTRT_FREE(data);
    }
    AGENTRT_FREE(unit);
}

agentrt_execution_unit_t *agentrt_file_unit_create(const char *root_dir)
{
    agentrt_execution_unit_t *unit =
        (agentrt_execution_unit_t *)AGENTRT_MALLOC(sizeof(agentrt_execution_unit_t));
    if (!unit) return NULL;
    __builtin_memset(unit, 0, sizeof(*unit));

    file_unit_data_t *data = (file_unit_data_t *)AGENTRT_MALLOC(sizeof(file_unit_data_t));
    if (!data) {
        AGENTRT_FREE(unit);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    data->root_dir = root_dir ? AGENTRT_STRDUP(root_dir) : NULL;
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"type\":\"file\",\"root_dir\":\"%s\"}",
             root_dir ? root_dir : "");
    data->metadata_json = AGENTRT_STRDUP(meta);

    if (!data->metadata_json || (root_dir && !data->root_dir)) {
        if (data->root_dir)
            AGENTRT_FREE(data->root_dir);
        if (data->metadata_json)
            AGENTRT_FREE(data->metadata_json);
        AGENTRT_FREE(data);
        AGENTRT_FREE(unit);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    unit->execution_unit_data = data;
    unit->execution_unit_execute = file_execute;
    unit->execution_unit_destroy = file_destroy;

    return unit;
}
