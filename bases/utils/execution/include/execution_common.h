/**
 * @file execution_common.h
 * @brief 执行单元通用功能定义
 * 
 * 提供执行单元共享的功能，包括命令执行、结果处理等
 * 减少执行单元之间的代码重复
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef EXECUTION_COMMON_H
#define EXECUTION_COMMON_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 执行结果结构
 */
typedef struct {
    int status;               /**< 执行状态码 */
    char* output;             /**< 执行输出 */
    size_t output_size;       /**< 输出大小 */
    char* error;              /**< 错误信息 */
    size_t error_size;        /**< 错误信息大小 */
    uint64_t execution_time;  /**< 执行时间（毫秒） */
} execution_result_t;

/**
 * @brief 执行配置结构
 */
typedef struct {
    bool capture_output;      /**< 是否捕获输出 */
    bool capture_error;       /**< 是否捕获错误 */
    bool timeout_enabled;     /**< 是否启用超时 */
    uint32_t timeout_ms;      /**< 超时时间（毫秒） */
    bool shell_enabled;       /**< 是否在shell中执行 */
} execution_config_t;

/**
 * @brief 初始化执行结果
 * @param result 执行结果指针
 * @return 0 成功，非0 失败
 */
int execution_result_init(execution_result_t* result);

/**
 * @brief 清理执行结果
 * @param result 执行结果指针
 */
void execution_result_cleanup(execution_result_t* result);

/**
 * @brief 设置执行结果
 * @param result 执行结果指针
 * @param status 状态码
 * @param output 输出内容
 * @param output_size 输出大小
 * @param error 错误信息
 * @param error_size 错误大小
 * @param execution_time 执行时间
 */
void execution_set_result(execution_result_t* result, int status, 
                         const char* output, size_t output_size, 
                         const char* error, size_t error_size, 
                         uint64_t execution_time);

/**
 * @brief 执行命令
 * @param command 命令字符串
 * @param manager 执行配置
 * @param result 执行结果
 * @return 0 成功，非0 失败
 */
int execution_execute_command(const char* command, 
                             const execution_config_t* manager, 
                             execution_result_t* result);

/**
 * @brief 验证命令安全性
 * @param command 命令字符串
 * @return true 安全，false 不安全
 */
bool execution_validate_command(const char* command);

/**
 * @brief 格式化执行结果为JSON
 * @param result 执行结果指针
 * @return JSON字符串，需要手动释放
 */
char* execution_format_result_json(const execution_result_t* result);

/**
 * @brief 初始化默认执行配置
 * @param manager 执行配置指针
 */
void execution_config_init(execution_config_t* manager);

#ifdef __cplusplus
}
#endif

#endif // EXECUTION_COMMON_H