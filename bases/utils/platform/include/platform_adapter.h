/**
 * @file platform_adapter.h
 * @brief 平台适配器 - 消除平台相关代码重复
 * 
 * 提供统一的跨平台抽象层，包括：
 * - 进程管理（跨平台执行命令）
 * - 文件系统操作
 * - 网络操作
 * - 系统时间
 * - 环境变量
 * - 路径处理
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_PLATFORM_ADAPTER_H
#define AGENTOS_PLATFORM_ADAPTER_H

#include <stddef.h>
#include <stdbool.h>

/* Unified base library compatibility layer */
#include "../../memory/include/memory_compat.h"
#include "../../string/include/string_compat.h"
#include <string.h>

/**
 * @brief 平台类型
 */
typedef enum {
    PLATFORM_UNKNOWN,
    PLATFORM_WINDOWS,
    PLATFORM_LINUX,
    PLATFORM_MACOS,
    PLATFORM_UNIX
} platform_type_t;

/**
 * @brief 命令执行结果
 */
typedef struct platform_exec_result {
    int exit_code;         /**< 退出码 */
    char* output;          /**< 命令输出 */
    size_t output_length;  /**< 输出长度 */
    bool success;          /**< 是否成功 */
} platform_exec_result_t;

/**
 * @brief 文件信息
 */
typedef struct platform_file_info {
    const char* path;      /**< 文件路径 */
    size_t size;           /**< 文件大小 */
    time_t mtime;          /**< 修改时间 */
    bool is_directory;     /**< 是否为目录 */
    bool exists;           /**< 是否存在 */
} platform_file_info_t;

/**
 * @brief 获取当前平台类型
 * @return 平台类型
 */
platform_type_t platform_get_type(void);

/**
 * @brief 获取平台名称
 * @return 平台名称字符串
 */
const char* platform_get_name(void);

/**
 * @brief 执行系统命令
 * @param command 命令字符串
 * @param timeout_ms 超时时间（毫秒，0表示无超时）
 * @return 执行结果
 */
platform_exec_result_t platform_exec(const char* command, unsigned int timeout_ms);

/**
 * @brief 释放执行结果
 * @param result 执行结果
 */
void platform_free_exec_result(platform_exec_result_t* result);

/**
 * @brief 获取文件信息
 * @param path 文件路径
 * @return 文件信息
 */
platform_file_info_t platform_get_file_info(const char* path);

/**
 * @brief 创建目录
 * @param path 目录路径
 * @return true表示成功，false表示失败
 */
bool platform_mkdir(const char* path);

/**
 * @brief 创建目录（递归）
 * @param path 目录路径
 * @return true表示成功，false表示失败
 */
bool platform_mkdir_recursive(const char* path);

/**
 * @brief 删除文件
 * @param path 文件路径
 * @return true表示成功，false表示失败
 */
bool platform_unlink(const char* path);

/**
 * @brief 删除目录
 * @param path 目录路径
 * @return true表示成功，false表示失败
 */
bool platform_rmdir(const char* path);

/**
 * @brief 复制文件
 * @param src 源文件路径
 * @param dest 目标文件路径
 * @return true表示成功，false表示失败
 */
bool platform_copy_file(const char* src, const char* dest);

/**
 * @brief 移动文件
 * @param src 源文件路径
 * @param dest 目标文件路径
 * @return true表示成功，false表示失败
 */
bool platform_move_file(const char* src, const char* dest);

/**
 * @brief 获取环境变量
 * @param name 环境变量名称
 * @param default_value 默认值
 * @return 环境变量值（需要调用AGENTOS_FREE释放）
 */
char* platform_get_env(const char* name, const char* default_value);

/**
 * @brief 设置环境变量
 * @param name 环境变量名称
 * @param value 环境变量值
 * @return true表示成功，false表示失败
 */
bool platform_set_env(const char* name, const char* value);

/**
 * @brief 获取当前工作目录
 * @return 当前工作目录（需要调用AGENTOS_FREE释放）
 */
char* platform_get_cwd(void);

/**
 * @brief 改变当前工作目录
 * @param path 目标路径
 * @return true表示成功，false表示失败
 */
bool platform_chdir(const char* path);

/**
 * @brief 获取临时目录
 * @return 临时目录路径（需要调用AGENTOS_FREE释放）
 */
char* platform_get_temp_dir(void);

/**
 * @brief 生成临时文件路径
 * @param prefix 前缀
 * @return 临时文件路径（需要调用AGENTOS_FREE释放）
 */
char* platform_get_temp_file(const char* prefix);

/**
 * @brief 路径连接
 * @param path1 路径1
 * @param path2 路径2
 * @return 连接后的路径（需要调用AGENTOS_FREE释放）
 */
char* platform_path_join(const char* path1, const char* path2);

/**
 * @brief 路径规范化
 * @param path 路径
 * @return 规范化后的路径（需要调用AGENTOS_FREE释放）
 */
char* platform_path_normalize(const char* path);

/**
 * @brief 获取路径中的文件名部分
 * @param path 路径
 * @return 文件名（需要调用AGENTOS_FREE释放）
 */
char* platform_path_basename(const char* path);

/**
 * @brief 获取路径中的目录部分
 * @param path 路径
 * @return 目录路径（需要调用AGENTOS_FREE释放）
 */
char* platform_path_dirname(const char* path);

/**
 * @brief 检查路径是否存在
 * @param path 路径
 * @return true表示存在，false表示不存在
 */
bool platform_path_exists(const char* path);

/**
 * @brief 检查路径是否为目录
 * @param path 路径
 * @return true表示是目录，false表示不是
 */
bool platform_path_is_directory(const char* path);

/**
 * @brief 检查路径是否为文件
 * @param path 路径
 * @return true表示是文件，false表示不是
 */
bool platform_path_is_file(const char* path);

/**
 * @brief 获取系统时间戳（毫秒）
 * @return 时间戳
 */
uint64_t platform_get_timestamp_ms(void);

/**
 * @brief 获取系统时间戳（微秒）
 * @return 时间戳
 */
uint64_t platform_get_timestamp_us(void);

/**
 * @brief 休眠指定毫秒数
 * @param ms 毫秒数
 */
void platform_sleep_ms(unsigned int ms);

/**
 * @brief 初始化平台适配器
 * @return true表示成功，false表示失败
 */
bool platform_adapter_init(void);

/**
 * @brief 清理平台适配器
 */
void platform_adapter_cleanup(void);

#endif /* AGENTOS_PLATFORM_ADAPTER_H */