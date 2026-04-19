/**
 * @file file_utils.c
 * @brief 文件操作实现（跨平台�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "io.h"
#include "../memory/include/agentos_memory.h"
#include <agentos/utils/memory/memory_compat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#else
#include <dirent.h>
#include <unistd.h>
#endif

/**
 * @brief 读取文件内容
 */
char* agentos_io_read_file(const char* path, size_t* out_len) {
    if (!path) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    char* buf = (char*)memory_alloc(size + 1, "file_read_buffer");
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read = fread(buf, 1, size, f);
    fclose(f);
    if (read != (size_t)size) {
        memory_free(buf);
        return NULL;
    }
    buf[size] = '\0';
    if (out_len) *out_len = size;
    return buf;
}

/**
 * @brief 写入文件内容
 */
int agentos_io_write_file(const char* path, const void* data, size_t len) {
    if (!path || !data) return -1;
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    if (len == (size_t)-1) len = strlen((const char*)data);
    size_t written = fwrite(data, 1, len, f);
    fclose(f);
    return (written == len) ? 0 : -1;
}

/**
 * @brief 确保目录存在
 */
int agentos_io_ensure_dir(const char* path) {
    if (!path) return -1;
    struct stat st = {0};
    if (stat(path, &st) == -1) {
#ifdef _WIN32
        if (_mkdir(path) != 0) return -1;
#else
        if (mkdir(path, 0755) != 0) return -1;
#endif
    }
    return 0;
}

/**
 * @brief 列出目录中的文件
 */
int agentos_io_list_files(const char* path, char*** out_files, size_t* out_count) {
    if (!path || !out_files || !out_count) return -1;

#ifdef _WIN32
    char search_path[MAX_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    WIN32_FIND_DATAA find_data;
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) return -1;

    size_t capacity = 64;
    size_t count = 0;
    char** files = (char**)AGENTOS_MALLOC(capacity * sizeof(char*));
    if (!files) {
        FindClose(hFind);
        return -1;
    }

    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (count >= capacity) {
                capacity *= 2;
                char** new_files = (char**)AGENTOS_REALLOC(files, capacity * sizeof(char*));
                if (!new_files) {
                    for (size_t i = 0; i < count; i++) AGENTOS_FREE(files[i]);
                    AGENTOS_FREE(files);
                    FindClose(hFind);
                    return -1;
                }
                files = new_files;
            }
            files[count] = AGENTOS_STRDUP(find_data.cFileName);
            if (!files[count]) {
                for (size_t i = 0; i < count; i++) AGENTOS_FREE(files[i]);
                AGENTOS_FREE(files);
                FindClose(hFind);
                return -1;
            }
            count++;
        }
    } while (FindNextFileA(hFind, &find_data));

    FindClose(hFind);
    *out_files = files;
    *out_count = count;
    return 0;
#else
    DIR* dir = opendir(path);
    if (!dir) return -1;

    size_t capacity = 64;
    size_t count = 0;
    char** files = (char**)AGENTOS_MALLOC(capacity * sizeof(char*));
    if (!files) {
        closedir(dir);
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            if (count >= capacity) {
                capacity *= 2;
                char** new_files = (char**)AGENTOS_REALLOC(files, capacity * sizeof(char*));
                if (!new_files) {
                    for (size_t i = 0; i < count; i++) AGENTOS_FREE(files[i]);
                    AGENTOS_FREE(files);
                    closedir(dir);
                    return -1;
                }
                files = new_files;
            }
            files[count] = AGENTOS_STRDUP(entry->d_name);
            if (!files[count]) {
                for (size_t i = 0; i < count; i++) AGENTOS_FREE(files[i]);
                AGENTOS_FREE(files);
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
#endif
}

/**
 * @brief 释放文件列表
 */
void agentos_io_free_list(char** files, size_t count) {
    if (!files) return;
    for (size_t i = 0; i < count; i++) AGENTOS_FREE(files[i]);
    AGENTOS_FREE(files);
}

/**
 * @brief 递归创建目录（跨平台）
 * @param path 目录路径
 * @param mode 目录权限（Unix风格，Windows忽略）
 * @return 0成功，-1失败
 */
int agentos_io_mkdir_p(const char* path, int mode) {
    if (!path) return -1;
    
    // 检查目录是否已存在
    struct stat st = {0};
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    
    // 复制路径以便修改
    char path_copy[1024];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';
    
    size_t len = strlen(path_copy);
    
    // 处理根目录和驱动器前缀
    #ifdef _WIN32
    // Windows: 跳过驱动器号（如 C:）
    size_t start_pos = 0;
    if (len >= 2 && path_copy[1] == ':') {
        start_pos = 2;
    }
    
    // 将正斜杠转换为反斜杠
    for (size_t i = start_pos; i < len; i++) {
        if (path_copy[i] == '/') {
            path_copy[i] = '\\';
        }
    }
    #else
    size_t start_pos = 0;
    // Unix: 跳过根目录
    if (path_copy[0] == '/') {
        start_pos = 1;
    }
    #endif
    
    // 逐级创建目录
    for (size_t i = start_pos; i < len; i++) {
        #ifdef _WIN32
        if (path_copy[i] == '\\') {
        #else
        if (path_copy[i] == '/') {
        #endif
            char save_char = path_copy[i];
            path_copy[i] = '\0';
            
            // 创建当前级目录（如果不存在）
            if (stat(path_copy, &st) == -1) {
                #ifdef _WIN32
                if (_mkdir(path_copy) != 0) {
                    return -1;
                }
                #else
                if (mkdir(path_copy, mode) != 0) {
                    return -1;
                }
                #endif
            } else if (!S_ISDIR(st.st_mode)) {
                // 存在但不是目录
                return -1;
            }
            
            path_copy[i] = save_char;
        }
    }
    
    // 创建最后一级目录
    #ifdef _WIN32
    return _mkdir(path_copy) == 0 ? 0 : -1;
    #else
    return mkdir(path_copy, mode) == 0 ? 0 : -1;
    #endif
}
