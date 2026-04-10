/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file check.h
 * @brief 通用检查宏 - 减少重复的参数验证和错误处理代码
 *
 * 提供一组通用的检查宏，用于参数验证、错误处理和资源清理。
 * 旨在消除项目中分散的检查代码，提供一致的验证模式。
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-07
 * @version 1.0
 *
 * @note 线程安全：所有宏均为线程安全（无副作用）
 * @see ARCHITECTURAL_PRINCIPLES.md E-1 安全内生原则
 */

#ifndef AGENTOS_CHECK_H
#define AGENTOS_CHECK_H

#include <stdbool.h>

#include "../error/include/error.h"

/**
 * @defgroup check_macros 检查宏
 * @{
 */

/**
 * @brief 检查指针是否为NULL，如果是则返回错误码
 * @param ptr 要检查的指针
 * @param err_code 错误码（如AGENTOS_EINVAL）
 * @return 如果ptr为NULL，返回err_code
 *
 * @code
 * CHECK_NULL_RET(input, AGENTOS_EINVAL);
 * @endcode
 */
#define CHECK_NULL_RET(ptr, err_code) \
    do { \
        if ((ptr) == NULL) { \
            return (err_code); \
        } \
    } while(0)

/**
 * @brief 检查指针是否为NULL，如果是则返回默认错误AGENTOS_EINVAL
 * @param ptr 要检查的指针
 * @return 如果ptr为NULL，返回AGENTOS_EINVAL
 */
#define CHECK_NULL(ptr) CHECK_NULL_RET(ptr, AGENTOS_EINVAL)

/**
 * @brief 检查表达式是否为真，如果为假则返回错误码
 * @param expr 要检查的表达式
 * @param err_code 错误码
 * @return 如果expr为假，返回err_code
 *
 * @code
 * CHECK_COND_RET(size > 0, AGENTOS_EINVAL);
 * @endcode
 */
#define CHECK_COND_RET(expr, err_code) \
    do { \
        if (!(expr)) { \
            return (err_code); \
        } \
    } while(0)

/**
 * @brief 检查表达式是否为真，如果为假则返回默认错误AGENTOS_EINVAL
 * @param expr 要检查的表达式
 * @return 如果expr为假，返回AGENTOS_EINVAL
 */
#define CHECK_COND(expr) CHECK_COND_RET(expr, AGENTOS_EINVAL)

/**
 * @brief 检查函数调用结果，如果失败则返回错误码
 * @param func_call 函数调用表达式（返回agentos_error_t）
 * @param err_var 存储错误结果的变量名
 * @return 如果func_call失败，返回错误码
 *
 * @code
 * CHECK_ERR_RET(agentos_init(), err);
 * @endcode
 */
#define CHECK_ERR_RET(func_call, err_var) \
    do { \
        agentos_error_t err_var = (func_call); \
        if (err_var != AGENTOS_SUCCESS) { \
            return err_var; \
        } \
    } while(0)

/**
 * @brief 检查函数调用结果，如果失败则跳转到清理标签
 * @param func_call 函数调用表达式（返回agentos_error_t）
 * @param err_var 存储错误结果的变量名
 *
 * @code
 * CHECK_ERR_GOTO(agentos_alloc(&ptr), err, cleanup);
 * @endcode
 */
#define CHECK_ERR_GOTO(func_call, err_var, label) \
    do { \
        agentos_error_t err_var = (func_call); \
        if (err_var != AGENTOS_SUCCESS) { \
            goto label; \
        } \
    } while(0)

/**
 * @brief 检查指针是否为NULL，如果是则跳转到清理标签
 * @param ptr 要检查的指针
 * @param label 跳转标签
 *
 * @code
 * CHECK_NULL_GOTO(buffer, cleanup);
 * @endcode
 */
#define CHECK_NULL_GOTO(ptr, label) \
    do { \
        if ((ptr) == NULL) { \
            goto label; \
        } \
    } while(0)

/**
 * @brief 安全释放指针并将其置为NULL
 * @param ptr 要释放的指针
 *
 * @note 使用AGENTOS_FREE进行释放
 * @code
 * SAFE_FREE(buffer);
 * @endcode
 */
#define SAFE_FREE(ptr) \
    do { \
        if ((ptr) != NULL) { \
            AGENTOS_FREE(ptr); \
            (ptr) = NULL; \
        } \
    } while(0)

/**
 * @brief 分配内存并检查结果，失败则跳转到清理标签
 * @param ptr_var 指针变量名
 * @param size 分配大小
 * @param label 跳转标签
 *
 * @code
 * ALLOC_CHECK(buffer, sizeof(buffer_t), cleanup);
 * @endcode
 */
#define ALLOC_CHECK(ptr_var, size, label) \
    do { \
        (ptr_var) = AGENTOS_MALLOC(size); \
        CHECK_NULL_GOTO(ptr_var, label); \
    } while(0)

/**
 * @brief 分配并清零内存，失败则跳转到清理标签
 * @param ptr_var 指针变量名
 * @param count 元素数量
 * @param size 元素大小
 * @param label 跳转标签
 *
 * @code
 * CALLOC_CHECK(array, 10, sizeof(int), cleanup);
 * @endcode
 */
#define CALLOC_CHECK(ptr_var, count, size, label) \
    do { \
        (ptr_var) = AGENTOS_CALLOC(count, size); \
        CHECK_NULL_GOTO(ptr_var, label); \
    } while(0)

/**
 * @brief 字符串复制检查，失败则跳转到清理标签
 * @param dest 目标指针变量
 * @param src 源字符串
 * @param label 跳转标签
 *
 * @code
 * STRDUP_CHECK(copy, original, cleanup);
 * @endcode
 */
#define STRDUP_CHECK(dest, src, label) \
    do { \
        (dest) = AGENTOS_STRDUP(src); \
        CHECK_NULL_GOTO(dest, label); \
    } while(0)

/**
 * @brief 范围检查，确保值在[min, max]范围内
 * @param value 要检查的值
 * @param min 最小值
 * @param max 最大值
 * @param err_code 错误码
 * @return 如果值超出范围，返回err_code
 */
#define CHECK_RANGE_RET(value, min, max, err_code) \
    do { \
        if ((value) < (min) || (value) > (max)) { \
            return (err_code); \
        } \
    } while(0)

/**
 * @brief 非零检查，确保值不为零
 * @param value 要检查的值
 * @param err_code 错误码
 * @return 如果值为零，返回err_code
 */
#define CHECK_NONZERO_RET(value, err_code) \
    do { \
        if ((value) == 0) { \
            return (err_code); \
        } \
    } while(0)

/**
 * @brief 检查字符串是否为空或NULL
 * @param str 要检查的字符串
 * @param err_code 错误码
 * @return 如果字符串为空或NULL，返回err_code
 */
#define CHECK_STRING_RET(str, err_code) \
    do { \
        if ((str) == NULL || (str)[0] == '\0') { \
            return (err_code); \
        } \
    } while(0)

/**
 * @brief 检查指针是否为NULL，如果是则设置错误码并跳转
 * @param ptr 要检查的指针
 * @param label 跳转标签
 * @param err_var 错误变量名（如ret）
 * @param err_code 错误码
 *
 * @code
 * CHECK_NULL_GOTO_ERR(buffer, cleanup, ret, AGENTOS_ENOMEM);
 * @endcode
 */
#define CHECK_NULL_GOTO_ERR(ptr, label, err_var, err_code) \
    do { \
        if ((ptr) == NULL) { \
            (err_var) = (err_code); \
            goto label; \
        } \
    } while(0)

/**
 * @brief 字符串复制检查，失败则设置错误码并跳转
 * @param dest 目标指针变量
 * @param src 源字符串
 * @param label 跳转标签
 * @param err_var 错误变量名
 * @param err_code 错误码
 *
 * @code
 * STRDUP_CHECK_ERR(copy, original, cleanup, ret, AGENTOS_ENOMEM);
 * @endcode
 */
#define STRDUP_CHECK_ERR(dest, src, label, err_var, err_code) \
    do { \
        (dest) = AGENTOS_STRDUP(src); \
        CHECK_NULL_GOTO_ERR(dest, label, err_var, err_code); \
    } while(0)

/**
 * @brief 分配内存检查，失败则设置错误码并跳转
 * @param ptr_var 指针变量名
 * @param size 分配大小
 * @param label 跳转标签
 * @param err_var 错误变量名
 * @param err_code 错误码
 *
 * @code
 * MALLOC_CHECK_ERR(buffer, sizeof(buffer_t), cleanup, ret, AGENTOS_ENOMEM);
 * @endcode
 */
#define MALLOC_CHECK_ERR(ptr_var, size, label, err_var, err_code) \
    do { \
        (ptr_var) = AGENTOS_MALLOC(size); \
        CHECK_NULL_GOTO_ERR(ptr_var, label, err_var, err_code); \
    } while(0)

/**
 * @brief 分配并清零内存检查，失败则设置错误码并跳转
 * @param ptr_var 指针变量名
 * @param count 元素数量
 * @param size 元素大小
 * @param label 跳转标签
 * @param err_var 错误变量名
 * @param err_code 错误码
 *
 * @code
 * CALLOC_CHECK_ERR(array, 10, sizeof(int), cleanup, ret, AGENTOS_ENOMEM);
 * @endcode
 */
#define CALLOC_CHECK_ERR(ptr_var, count, size, label, err_var, err_code) \
    do { \
        (ptr_var) = AGENTOS_CALLOC(count, size); \
        CHECK_NULL_GOTO_ERR(ptr_var, label, err_var, err_code); \
    } while(0)

/** @} */ // end of check_macros

#endif /* AGENTOS_CHECK_H */
