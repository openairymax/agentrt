/**
 * @file platform_adapter.c
 * @brief 平台适配器 - 实现
 *
 * 实现跨平台抽象层，消除平台相关代码重复。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

/* _POSIX_C_SOURCE: defined via CMakeLists.txt target_compile_definitions (BAN-182) */
/* _XOPEN_SOURCE: defined via CMakeLists.txt target_compile_definitions (BAN-182) */
/* _GNU_SOURCE: defined via CMakeLists.txt target_compile_definitions (BAN-182) */

/* 1. POSIX标准头文件（必须最先包含） */
#include <time.h>
#include <unistd.h>

/* 2. C标准库头文件 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#define PLATFORM_SLASH '\\'
#else
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#define PLATFORM_SLASH '/'
#endif

#include "platform_adapter.h"

/* 确保系统头文件声明在项目头文件之后仍然可用 */
#include "platform.h"

#include <string.h>
#include "memory_compat.h"
#include "error.h"

/**
 * @brief 获取当前平台类型
 */
platform_type_t platform_get_type(void)
{
#if defined(_WIN32)
    return PLATFORM_WINDOWS;
#elif defined(__linux__)
    return PLATFORM_LINUX;
#elif defined(__APPLE__)
    return PLATFORM_MACOS;
#elif defined(__unix__)
    return PLATFORM_UNIX;
#else
    return PLATFORM_UNKNOWN;
#endif
}

/**
 * @brief 获取平台名称
 */
const char *platform_get_name(void)
{
    switch (platform_get_type()) {
    case PLATFORM_WINDOWS:
        return "Windows";
    case PLATFORM_LINUX:
        return "Linux";
    case PLATFORM_MACOS:
        return "macOS";
    case PLATFORM_UNIX:
        return "Unix";
    default:
        return "Unknown";
    }
}

/* platform_exec() / platform_free_exec_result() 已移除（BAN-211/235 安全合规）。
 * 这两个函数通过 /bin/sh -c 执行命令字符串，存在命令注入风险，且全仓库零调用者。
 * 统一使用 agentos_process_run_capture()（fork+execvp，不经 shell）作为规范子进程 API。
 * 见 platform.h 的 agentos_process_run_capture 声明。 */

/**
 * @brief 获取文件信息
 */
platform_file_info_t platform_get_file_info(const char *path)
{
    platform_file_info_t info = {
        .path = path, .size = 0, .mtime = 0, .is_directory = false, .exists = false};

    if (!path) {
        return info;
    }

#if defined(_WIN32)
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(path, &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        info.exists = true;
        info.is_directory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!info.is_directory) {
            info.size = ((uint64_t)findData.nFileSizeHigh << 32) | findData.nFileSizeLow;
            info.mtime = (time_t)findData.ftLastWriteTime.dwLowDateTime;
        }
        FindClose(hFind);
    }
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        info.exists = true;
        info.is_directory = S_ISDIR(st.st_mode);
        if (!info.is_directory) {
            info.size = st.st_size;
            info.mtime = st.st_mtime;
        }
    }
#endif

    return info;
}

/**
 * @brief 创建目录
 */
bool platform_mkdir(const char *path)
{
    if (!path) {
        return false;
    }

#if defined(_WIN32)
    return (_mkdir(path) == 0);
#else
    return (mkdir(path, 0755) == 0);
#endif
}

/**
 * @brief 创建目录（递归）
 */
bool platform_mkdir_recursive(const char *path)
{
    if (!path) {
        return false;
    }

    char *copy = (char *)AGENTOS_MALLOC(strlen(path) + 1);
    if (!copy) {
        return false;
    }

    __builtin_memcpy(copy, path, strlen(path) + 1);
    char *p = copy;

    while (*p) {
        if (*p == PLATFORM_SLASH) {
            *p = '\0';
            if (*copy && !platform_path_exists(copy)) {
                if (!platform_mkdir(copy)) {
                    AGENTOS_FREE(copy);
                    return false;
                }
            }
            *p = PLATFORM_SLASH;
        }
        p++;
    }

    if (*copy && !platform_path_exists(copy)) {
        if (!platform_mkdir(copy)) {
            AGENTOS_FREE(copy);
            return false;
        }
    }

    AGENTOS_FREE(copy);
    return true;
}

/**
 * @brief 删除文件
 */
bool platform_unlink(const char *path)
{
    if (!path) {
        return false;
    }

#if defined(_WIN32)
    return (DeleteFileA(path) != 0);
#else
    return (unlink(path) == 0);
#endif
}

/**
 * @brief 删除目录
 */
bool platform_rmdir(const char *path)
{
    if (!path) {
        return false;
    }

#if defined(_WIN32)
    return (RemoveDirectoryA(path) != 0);
#else
    return (rmdir(path) == 0);
#endif
}

/**
 * @brief 复制文件
 */
bool platform_copy_file(const char *src, const char *dest)
{
    if (!src || !dest) {
        return false;
    }

#if defined(_WIN32)
    return (CopyFileA(src, dest, FALSE) != 0);
#else
    FILE *srcFile = fopen(src, "rb");
    if (!srcFile) {
        return false;
    }

    FILE *destFile = fopen(dest, "wb");
    if (!destFile) {
        fclose(srcFile);
        return false;
    }

    char buffer[4096];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), srcFile)) > 0) {
        if (fwrite(buffer, 1, bytesRead, destFile) != bytesRead) {
            fclose(srcFile);
            fclose(destFile);
            return false;
        }
    }

    fclose(srcFile);
    fclose(destFile);
    return true;
#endif
}

/**
 * @brief 移动文件
 */
bool platform_move_file(const char *src, const char *dest)
{
    if (!src || !dest) {
        return false;
    }

#if defined(_WIN32)
    return (MoveFileA(src, dest) != 0);
#else
    return (rename(src, dest) == 0);
#endif
}

/**
 * @brief 获取环境变量
 */
char *platform_get_env(const char *name, const char *default_value)
{
    if (!name) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

#if defined(_WIN32)
    char buffer[4096];
    DWORD size = GetEnvironmentVariableA(name, buffer, sizeof(buffer));
    if (size == 0) {
        if (default_value) {
            return AGENTOS_STRDUP(default_value);
        }
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "operation failed");
        return NULL;
    }

    char *value = (char *)AGENTOS_MALLOC(size + 1);
    if (value) {
        GetEnvironmentVariableA(name, value, size + 1);
    }
    return value;
#else
    const char *value = getenv(name);
    if (value) {
        return AGENTOS_STRDUP(value);
    }
    if (default_value) {
        return AGENTOS_STRDUP(default_value);
    }
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "operation failed");
    return NULL;
#endif
}

/**
 * @brief 设置环境变量
 */
bool platform_set_env(const char *name, const char *value)
{
    if (!name) {
        return false;
    }

#if defined(_WIN32)
    return (SetEnvironmentVariableA(name, value) != 0);
#else
    return (setenv(name, value, 1) == 0);
#endif
}

/**
 * @brief 获取当前工作目录
 */
char *platform_get_cwd(void)
{
#if defined(_WIN32)
    char buffer[4096];
    if (_getcwd(buffer, sizeof(buffer)) != NULL) {
        return AGENTOS_STRDUP(buffer);
    }
#else
    char *buffer = getcwd(NULL, 0);
    if (buffer) {
        char *copy = AGENTOS_STRDUP(buffer);
        AGENTOS_FREE(buffer);
        return copy;
    }
#endif
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "operation failed");
    return NULL;
}

/**
 * @brief 改变当前工作目录
 */
bool platform_chdir(const char *path)
{
    if (!path) {
        return false;
    }

#if defined(_WIN32)
    return (_chdir(path) == 0);
#else
    return (chdir(path) == 0);
#endif
}

/**
 * @brief 获取临时目录
 */
char *platform_get_temp_dir(void)
{
#if defined(_WIN32)
    char buffer[4096];
    if (GetTempPathA(sizeof(buffer), buffer) > 0) {
        return AGENTOS_STRDUP(buffer);
    }
#else
    const char *temp = getenv("TMPDIR");
    if (temp) {
        return AGENTOS_STRDUP(temp);
    }
    return AGENTOS_STRDUP("/tmp");
#endif
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "operation failed");
    return NULL;
}

/**
 * @brief 生成临时文件路径
 */
char *platform_get_temp_file(const char *prefix)
{
    char *temp_dir = platform_get_temp_dir();
    if (!temp_dir) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    char *path = NULL;
    const char *base = prefix ? prefix : "agentos";

#if defined(_WIN32)
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), "%s\\%s_XXXXXX", temp_dir, base);
    if (GetTempFileNameA(temp_dir, base, 0, buffer) != 0) {
        path = AGENTOS_STRDUP(buffer);
    }
#else
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), "%s/%s_XXXXXX", temp_dir, base);
    int fd = mkstemp(buffer);
    if (fd != -1) {
        close(fd);
        path = AGENTOS_STRDUP(buffer);
    }
#endif

    AGENTOS_FREE(temp_dir);
    return path;
}

/**
 * @brief 路径连接
 */
char *platform_path_join(const char *path1, const char *path2)
{
    if (!path1 || !path2) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    bool needs_slash = (len1 > 0 && path1[len1 - 1] != PLATFORM_SLASH);
    size_t total_len = len1 + len2 + (needs_slash ? 1 : 0) + 1;

    char *result = (char *)AGENTOS_MALLOC(total_len);
    if (!result) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    if (needs_slash) {
        snprintf(result, total_len, "%s%c%s", path1, PLATFORM_SLASH, path2);
    } else {
        snprintf(result, total_len, "%s%s", path1, path2);
    }
    return result;
}

/**
 * @brief 路径规范化
 */
char *platform_path_normalize(const char *path)
{
    if (!path) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    // Simple implementation - real implementation would handle .. and .
    return AGENTOS_STRDUP(path);
}

/**
 * @brief 获取路径中的文件名部分
 */
char *platform_path_basename(const char *path)
{
    if (!path) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    const char *last_slash = strrchr(path, PLATFORM_SLASH);
    if (last_slash) {
        return AGENTOS_STRDUP(last_slash + 1);
    }
    return AGENTOS_STRDUP(path);
}

/**
 * @brief 获取路径中的目录部分
 */
char *platform_path_dirname(const char *path)
{
    if (!path) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    const char *last_slash = strrchr(path, PLATFORM_SLASH);
    if (!last_slash) {
        return AGENTOS_STRDUP(".");
    }

    size_t len = last_slash - path;
    char *result = (char *)AGENTOS_MALLOC(len + 1);
    if (result) {
        AGENTOS_STRNCPY_TERM(result, path, len);
        result[len] = '\0';
    }
    return result;
}

/**
 * @brief 检查路径是否存在
 */
bool platform_path_exists(const char *path)
{
    if (!path) {
        return false;
    }

    platform_file_info_t info = platform_get_file_info(path);
    return info.exists;
}

/**
 * @brief 检查路径是否为目录
 */
bool platform_path_is_directory(const char *path)
{
    if (!path) {
        return false;
    }

    platform_file_info_t info = platform_get_file_info(path);
    return info.exists && info.is_directory;
}

/**
 * @brief 检查路径是否为文件
 */
bool platform_path_is_file(const char *path)
{
    if (!path) {
        return false;
    }

    platform_file_info_t info = platform_get_file_info(path);
    return info.exists && !info.is_directory;
}

/**
 * @brief 获取系统时间戳（毫秒）
 */
uint64_t platform_get_timestamp_ms(void)
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10000; /* Convert 100-nanosecond intervals to milliseconds */
#else
    return agentos_time_ms();
#endif
}

/**
 * @brief 获取系统时间戳（微秒）
 */
uint64_t platform_get_timestamp_us(void)
{
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10; /* Convert 100-nanosecond intervals to microseconds */
#else
    return agentos_time_ns() / 1000;
#endif
}

/**
 * @brief 休眠指定毫秒数
 */
void platform_sleep_ms(unsigned int ms)
{
#if defined(_WIN32)
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/**
 * @brief 初始化平台适配器
 */
bool platform_adapter_init(void)
{
#if defined(_WIN32)
    WSADATA wsaData;
    return (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#else
    return true;
#endif
}

/**
 * @brief 清理平台适配器
 */
void platform_adapter_cleanup(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}