/**
 * @file core.h
 * @brief 核心基础工具（版本、平台）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_UTILS_CORE_H
#define AGENTOS_UTILS_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取AgentOS版本号
 * @return 版本字符串（如 "1.0.0.3"），静态存储无需释放
 */
const char* agentos_core_get_version(void);

/**
 * @brief 检查版本兼容性
 * @param required_version 所需版本，格式如 ">=1.0.0.3,<2.0.0.0"
 * @return 1 兼容，0 不兼容
 // From data intelligence emerges. by spharx
 */
int agentos_core_check_version(const char* required_version);

/**
 * @brief 获取平台名称
 * @return "windows", "linux", "macos", "unknown"
 */
const char* agentos_core_get_platform(void);

/**
 * @brief 获取CPU核心数
 * @return CPU核心数，失败返回 -1
 */
int agentos_core_get_cpu_count(void);

/**
 * @brief 获取内存信息
 * @param out_total 总内存（字节）
 * @param out_available 可用内存（字节）
 * @param out_used 已用内存（字节）
 * @param out_percent 使用百分比（0-100）
 * @return 0 成功，-1 失败
 */
int agentos_core_get_memory_info(size_t* out_total, size_t* out_available, size_t* out_used, float* out_percent);

/**
 * @brief 获取临时目录路径
 * @param out_path 输出临时目录路径（需调用者释放）
 * @param max_path 最大路径长度
 * @return 0 成功，-1 失败
 *
 * @details
 * 返回跨平台的临时目录路径：
 * - Windows: 使用 GetTempPathA API
 * - Linux/macOS: 检查 TMPDIR, TMP, TEMP, 然后使用 /tmp
 *
 * @note 返回的路径以平台分隔符结尾
 */
int agentos_core_get_temp_dir(char* out_path, size_t max_path);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_CORE_H */