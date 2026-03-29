/**
 * @file platform_adapter.c
 * @brief 平台适配器 - 实现
 * 
 * 实现跨平台抽象层，消除平台相关代码重复。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "include/platform_adapter.h"
#include <stdio.h>
#include <time.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <io.h>
    #include <direct.h>
    #include <process.h>
    #include <sys/stat.h>
    #define PLATFORM_SLASH '\\'
#else
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <limits.h>
    #define PLATFORM_SLASH '/'
#endif

/**
 * @brief 获取当前平台类型
 */
platform_type_t platform_get_type(void) {
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
const char* platform_get_name(void) {
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

/**
 * @brief 执行系统命令
 */
platform_exec_result_t platform_exec(const char* command, unsigned int timeout_ms) {
    platform_exec_result_t result = {
        .exit_code = -1,
        .output = NULL,
        .output_length = 0,
        .success = false
    };
    
    if (!command) {
        return result;
    }
    
#if defined(_WIN32)
    // Windows implementation
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return result;
    }
    
    STARTUPINFOA si = { 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    
    PROCESS_INFORMATION pi = { 0 };
    char cmdBuf[4096];
    snprintf(cmdBuf, sizeof(cmdBuf), "cmd.exe /c %s", command);
    
    if (!CreateProcessA(NULL, cmdBuf, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return result;
    }
    
    CloseHandle(hWritePipe);
    
    // Read output
    char buffer[4096];
    DWORD bytesRead;
    size_t totalRead = 0;
    
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        char* newOutput = (char*)AGENTOS_REALLOC(result.output, totalRead + bytesRead + 1);
        if (!newOutput) {
            break;
        }
        result.output = newOutput;
        memcpy(result.output + totalRead, buffer, bytesRead);
        totalRead += bytesRead;
    }
    
    if (result.output) {
        result.output[totalRead] = '\0';
        result.output_length = totalRead;
    }
    
    // Wait for process to exit
    if (timeout_ms > 0) {
        if (WaitForSingleObject(pi.hProcess, timeout_ms) == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, 0);
        }
    } else {
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    
    GetExitCodeProcess(pi.hProcess, (DWORD*)&result.exit_code);
    result.success = (result.exit_code == 0);
    
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    // POSIX implementation
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return result;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return result;
    }
    
    if (pid == 0) {
        // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        
        execl("/bin/sh", "sh", "-c", command, NULL);
        exit(1);
    }
    
    // Parent process
    close(pipefd[1]);
    
    char buffer[4096];
    ssize_t bytesRead;
    size_t totalRead = 0;
    
    while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        char* newOutput = (char*)AGENTOS_REALLOC(result.output, totalRead + bytesRead + 1);
        if (!newOutput) {
            break;
        }
        result.output = newOutput;
        memcpy(result.output + totalRead, buffer, bytesRead);
        totalRead += bytesRead;
    }
    
    if (result.output) {
        result.output[totalRead] = '\0';
        result.output_length = totalRead;
    }
    
    close(pipefd[0]);
    
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        result.exit_code = WEXITSTATUS(status);
        result.success = (result.exit_code == 0);
    }
#endif
    
    return result;
}

/**
 * @brief 释放执行结果
 */
void platform_free_exec_result(platform_exec_result_t* result) {
    if (result && result->output) {
        AGENTOS_FREE(result->output);
        result->output = NULL;
        result->output_length = 0;
    }
}

/**
 * @brief 获取文件信息
 */
platform_file_info_t platform_get_file_info(const char* path) {
    platform_file_info_t info = {
        .path = path,
        .size = 0,
        .mtime = 0,
        .is_directory = false,
        .exists = false
    };
    
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
bool platform_mkdir(const char* path) {
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
bool platform_mkdir_recursive(const char* path) {
    if (!path) {
        return false;
    }
    
    char* copy = (char*)AGENTOS_MALLOC(strlen(path) + 1);
    if (!copy) {
        return false;
    }
    
    strcpy(copy, path);
    char* p = copy;
    
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
bool platform_unlink(const char* path) {
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
bool platform_rmdir(const char* path) {
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
bool platform_copy_file(const char* src, const char* dest) {
    if (!src || !dest) {
        return false;
    }
    
#if defined(_WIN32)
    return (CopyFileA(src, dest, FALSE) != 0);
#else
    FILE* srcFile = fopen(src, "rb");
    if (!srcFile) {
        return false;
    }
    
    FILE* destFile = fopen(dest, "wb");
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
bool platform_move_file(const char* src, const char* dest) {
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
char* platform_get_env(const char* name, const char* default_value) {
    if (!name) {
        return NULL;
    }
    
#if defined(_WIN32)
    char buffer[4096];
    DWORD size = GetEnvironmentVariableA(name, buffer, sizeof(buffer));
    if (size == 0) {
        if (default_value) {
            return AGENTOS_STRDUP(default_value);
        }
        return NULL;
    }
    
    char* value = (char*)AGENTOS_MALLOC(size + 1);
    if (value) {
        GetEnvironmentVariableA(name, value, size + 1);
    }
    return value;
#else
    const char* value = getenv(name);
    if (value) {
        return AGENTOS_STRDUP(value);
    }
    if (default_value) {
        return AGENTOS_STRDUP(default_value);
    }
    return NULL;
#endif
}

/**
 * @brief 设置环境变量
 */
bool platform_set_env(const char* name, const char* value) {
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
char* platform_get_cwd(void) {
#if defined(_WIN32)
    char buffer[4096];
    if (_getcwd(buffer, sizeof(buffer)) != NULL) {
        return AGENTOS_STRDUP(buffer);
    }
#else
    char* buffer = getcwd(NULL, 0);
    if (buffer) {
        char* copy = AGENTOS_STRDUP(buffer);
        free(buffer);
        return copy;
    }
#endif
    return NULL;
}

/**
 * @brief 改变当前工作目录
 */
bool platform_chdir(const char* path) {
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
char* platform_get_temp_dir(void) {
#if defined(_WIN32)
    char buffer[4096];
    if (GetTempPathA(sizeof(buffer), buffer) > 0) {
        return AGENTOS_STRDUP(buffer);
    }
#else
    const char* temp = getenv("TMPDIR");
    if (temp) {
        return AGENTOS_STRDUP(temp);
    }
    return AGENTOS_STRDUP("/tmp");
#endif
    return NULL;
}

/**
 * @brief 生成临时文件路径
 */
char* platform_get_temp_file(const char* prefix) {
    char* temp_dir = platform_get_temp_dir();
    if (!temp_dir) {
        return NULL;
    }
    
    char* path = NULL;
    const char* base = prefix ? prefix : "agentos";
    
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
char* platform_path_join(const char* path1, const char* path2) {
    if (!path1 || !path2) {
        return NULL;
    }
    
    size_t len1 = strlen(path1);
    size_t len2 = strlen(path2);
    size_t total_len = len1 + len2 + 2; /* 1 for separator, 1 for null terminator */
    
    char* result = (char*)AGENTOS_MALLOC(total_len);
    if (!result) {
        return NULL;
    }
    
    strcpy(result, path1);
    
    if (len1 > 0 && path1[len1 - 1] != PLATFORM_SLASH) {
        strcat(result, (char[]){PLATFORM_SLASH, '\0'});
    }
    
    strcat(result, path2);
    return result;
}

/**
 * @brief 路径规范化
 */
char* platform_path_normalize(const char* path) {
    if (!path) {
        return NULL;
    }
    
    // Simple implementation - real implementation would handle .. and .
    return AGENTOS_STRDUP(path);
}

/**
 * @brief 获取路径中的文件名部分
 */
char* platform_path_basename(const char* path) {
    if (!path) {
        return NULL;
    }
    
    const char* last_slash = strrchr(path, PLATFORM_SLASH);
    if (last_slash) {
        return AGENTOS_STRDUP(last_slash + 1);
    }
    return AGENTOS_STRDUP(path);
}

/**
 * @brief 获取路径中的目录部分
 */
char* platform_path_dirname(const char* path) {
    if (!path) {
        return NULL;
    }
    
    const char* last_slash = strrchr(path, PLATFORM_SLASH);
    if (!last_slash) {
        return AGENTOS_STRDUP(".");
    }
    
    size_t len = last_slash - path;
    char* result = (char*)AGENTOS_MALLOC(len + 1);
    if (result) {
        strncpy(result, path, len);
        result[len] = '\0';
    }
    return result;
}

/**
 * @brief 检查路径是否存在
 */
bool platform_path_exists(const char* path) {
    if (!path) {
        return false;
    }
    
    platform_file_info_t info = platform_get_file_info(path);
    return info.exists;
}

/**
 * @brief 检查路径是否为目录
 */
bool platform_path_is_directory(const char* path) {
    if (!path) {
        return false;
    }
    
    platform_file_info_t info = platform_get_file_info(path);
    return info.exists && info.is_directory;
}

/**
 * @brief 检查路径是否为文件
 */
bool platform_path_is_file(const char* path) {
    if (!path) {
        return false;
    }
    
    platform_file_info_t info = platform_get_file_info(path);
    return info.exists && !info.is_directory;
}

/**
 * @brief 获取系统时间戳（毫秒）
 */
uint64_t platform_get_timestamp_ms(void) {
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10000; /* Convert 100-nanosecond intervals to milliseconds */
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/**
 * @brief 获取系统时间戳（微秒）
 */
uint64_t platform_get_timestamp_us(void) {
#if defined(_WIN32)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t timestamp = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return timestamp / 10; /* Convert 100-nanosecond intervals to microseconds */
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#endif
}

/**
 * @brief 休眠指定毫秒数
 */
void platform_sleep_ms(unsigned int ms) {
#if defined(_WIN32)
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

/**
 * @brief 初始化平台适配器
 */
bool platform_adapter_init(void) {
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
void platform_adapter_cleanup(void) {
#if defined(_WIN32)
    WSACleanup();
#endif
}