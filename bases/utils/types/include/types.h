/**
 * @file types.h
 * @brief 通用类型定义
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块提供通用类型定义：
 * - 跨平台基本类型别名
 * - 结果状态码定义
 * - 通用回调函数类型
 */

#ifndef AGENTOS_UTILS_TYPES_H
#define AGENTOS_UTILS_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 操作结果状态码
 */
typedef enum {
    AGENTOS_SUCCESS = 0,              /**< 成功 */
    AGENTOS_ERROR_GENERAL = -1,        /**< 通用错误 */
    AGENTOS_ERROR_INVALID_PARAM = -2, /**< 参数无效 */
    AGENTOS_ERROR_NO_MEMORY = -3,     /**< 内存不足 */
    AGENTOS_ERROR_TIMEOUT = -4,        /**< 操作超时 */
    AGENTOS_ERROR_NOT_FOUND = -5,     /**< 资源未找到 */
    AGENTOS_ERROR_PERMISSION = -6,     /**< 权限不足 */
    AGENTOS_ERROR_BUSY = -7,          /**< 资源忙 */
    AGENTOS_ERROR_CANCELLED = -8,     /**< 操作已取消 */
    AGENTOS_ERROR_UNSUPPORTED = -9,   /**< 不支持 */
    AGENTOS_ERROR_LIMIT = -10         /**< 超出限制 */
} agentos_result_t;

/**
 * @brief 结果回调函数类型
 * @param result 操作结果
 * @param user_data 用户数据
 */
typedef void (*agentos_result_callback_t)(agentos_result_t result, void* user_data);

/**
 * @brief 销毁回调函数类型
 * @param data 要销毁的数据
 */
typedef void (*agentos_destructor_t)(void* data);

/**
 * @brief 哈希函数类型
 * @param key 键
 * @param key_size 键大小
 * @return 哈希值
 */
typedef uint64_t (*agentos_hash_func_t)(const void* key, size_t key_size);

/**
 * @brief 比较函数类型
 * @param a 第一个元素
 * @param b 第二个元素
 * @return 比较结果：<0 a<b, =0 a==b, >0 a>b
 */
typedef int (*agentos_compare_func_t)(const void* a, const void* b);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_UTILS_TYPES_H */
