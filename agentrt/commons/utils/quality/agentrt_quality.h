/**
 * @file agentrt_quality.h
 * @brief AgentRT 代码质量保证框架
 *
 * 提供标准化的代码质量保证工具，包括：
 * - 输入验证宏（NULL检查、范围检查、类型检查）
 * - 错误处理宏（安全返回、错误传播）
 * - 资源管理宏（RAII模式、自动清理）
 * - 边界检查宏（数组越界、整数溢出）
 *
 * 遵循E-1安全内生、E-3资源确定性、E-6错误可追溯原则。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_QUALITY_H
#define AGENTRT_QUALITY_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "error.h"

/* 前向声明安全内存函数（避免裸 malloc/calloc/free 触发 BAN 合规违规） */
void *agentrt_malloc(size_t size);
void *agentrt_calloc(size_t num, size_t size);
void agentrt_free(const void *ptr);

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup quality_assurance
 * @{
 */

/* ==================== 输入验证宏 ==================== */

/**
 * @brief 检查指针是否为NULL，如果为NULL则返回错误码
 */
#ifndef AGENTRT_CHECK_NULL
#define AGENTRT_CHECK_NULL(ptr, error_code) \
    do {                                    \
        if ((ptr) == NULL) {                \
            return (error_code);            \
        }                                   \
    } while (0)
#endif

/**
 * @brief 检查指针是否为NULL，如果为NULL则跳转到错误标签
 */
#define AGENTRT_CHECK_NULL_GOTO(ptr, label, error_code) \
    do {                                                \
        if ((ptr) == NULL) {                            \
            err = (error_code);                         \
            goto label;                                 \
        }                                               \
    } while (0)

/**
 * @brief 检查条件是否成立，如果不成立则返回错误码
 */
#define AGENTRT_CHECK_CONDITION(cond, error_code) \
    do {                                          \
        if (!(cond)) {                            \
            return (error_code);                  \
        }                                         \
    } while (0)

/**
 * @brief 检查条件是否成立，如果不成立则跳转到错误标签
 */
#define AGENTRT_CHECK_CONDITION_GOTO(cond, label, error_code) \
    do {                                                      \
        if (!(cond)) {                                        \
            err = (error_code);                               \
            goto label;                                       \
        }                                                     \
    } while (0)

/**
 * @brief 检查值是否在范围内 [min, max]
 */
#define AGENTRT_CHECK_RANGE(value, min_val, max_val, error_code) \
    do {                                                         \
        if ((value) < (min_val) || (value) > (max_val)) {        \
            return (error_code);                                 \
        }                                                        \
    } while (0)

/**
 * @brief 检查值是否大于等于最小值
 */
#define AGENTRT_CHECK_MIN(value, min_val, error_code) \
    do {                                              \
        if ((value) < (min_val)) {                    \
            return (error_code);                      \
        }                                             \
    } while (0)

/**
 * @brief 检查值是否小于等于最大值
 */
#define AGENTRT_CHECK_MAX(value, max_val, error_code) \
    do {                                              \
        if ((value) > (max_val)) {                    \
            return (error_code);                      \
        }                                             \
    } while (0)

/**
 * @brief 检查字符串长度是否在允许范围内
 */
#define AGENTRT_CHECK_STR_LEN(str, max_len, error_code) \
    do {                                                \
        if (!(str) || strlen((str)) > (max_len)) {      \
            return (error_code);                        \
        }                                               \
    } while (0)

/**
 * @brief 检查数组索引是否有效
 */
#define AGENTRT_CHECK_ARRAY_INDEX(index, array_size, error_code) \
    do {                                                         \
        if ((index) >= (array_size)) {                           \
            return (error_code);                                 \
        }                                                        \
    } while (0)

/**
 * @brief 检查字符串是否为空或NULL
 */
#define AGENTRT_CHECK_EMPTY(str, error_code)     \
    do {                                         \
        if ((str) == NULL || (str)[0] == '\0') { \
            return (error_code);                 \
        }                                        \
    } while (0)

/**
 * @brief 检查数组索引是否越界（兼容宏）
 */
#define AGENTRT_CHECK_BOUNDS(idx, size, error_code) \
    AGENTRT_CHECK_ARRAY_INDEX((idx), (size), (error_code))

/* ==================== 错误处理宏 ==================== */

/**
 * @brief 安全执行操作，失败时跳转到清理标签
 */
#define AGENTRT_SAFE_EXEC(expr, cleanup_label, error_var) \
    do {                                                  \
        int _ret = (expr);                                \
        if (_ret != 0) {                                  \
            (error_var) = _ret;                           \
            goto cleanup_label;                           \
        }                                                 \
    } while (0)

/**
 * @brief 安全分配内存，失败时跳转到清理标签
 */
#define AGENTRT_SAFE_ALLOC(var, size, cleanup_label, error_var) \
    do {                                                        \
        (var) = agentrt_malloc((size));                          \
        if (!(var)) {                                           \
            (error_var) = -1;                                   \
            goto cleanup_label;                                 \
        }                                                       \
    } while (0)

/**
 * @brief 安全分配内存并清零，失败时跳转到清理标签
 */
#define AGENTRT_SAFE_CALLOC(var, size, cleanup_label, error_var) \
    do {                                                         \
        (var) = agentrt_calloc(1, (size));                      \
        if (!(var)) {                                            \
            (error_var) = -1;                                    \
            goto cleanup_label;                                  \
        }                                                        \
    } while (0)

/**
 * @brief 记录错误并返回
 */
#define AGENTRT_LOG_ERROR_AND_RETURN(error_code, fmt, ...) \
    do {                                                   \
        /* 日志记录 */                                 \
        return (error_code);                               \
    } while (0)

/* ==================== 资源管理宏 ==================== */

/**
 * @brief 定义RAII风格的资源守卫作用域开始
 */
#define AGENTRT_RESOURCE_GUARD_SCOPE_BEGIN() {

/**
 * @brief 定义RAII风格的资源守卫作用域结束
 */
#define AGENTRT_RESOURCE_GUARD_SCOPE_END() }

/**
 * @brief 自动释放资源的宏（用于局部变量）
 */
#ifndef AGENTRT_AUTO_FREE
#define AGENTRT_AUTO_FREE(ptr) \
    __attribute__((cleanup(agentrt_auto_free))) char **_auto_##ptr = &(char *)(ptr)
#endif

/**
 * @brief 自动关闭文件描述符的宏
 */
#define AGENTRT_AUTO_CLOSE(fd) __attribute__((cleanup(agentrt_auto_close))) int *_auto_##fd = &(fd)

/**
 * @brief 安全释放内存并置为NULL
 */
#define AGENTRT_SAFE_FREE(ptr) \
    do {                       \
        if ((ptr) != NULL) {   \
            agentrt_free((ptr)); \
            (ptr) = NULL;      \
        }                      \
    } while (0)

/**
 * @brief 安全释放内存：先清零再释放（SEC-15 合规）
 *
 * 用于释放包含敏感数据的内存（API Key、Token、密码等），
 * 防止数据残留在堆上被泄露。
 *
 * @note 仅对通过 agentrt_mem_alloc/malloc 分配的内存有效
 * @note 释放后指针置 NULL，防止 use-after-free
 *
 * BAN-247: 敏感数据释放前必须清零
 */
#ifndef AGENTRT_SECURE_FREE
#define AGENTRT_SECURE_FREE(ptr, size)                    \
    do {                                                  \
        if ((ptr) != NULL) {                              \
            if ((size) > 0) {                             \
                agentrt_explicit_bzero((ptr), (size));    \
            }                                             \
            free((ptr));                                  \
            (ptr) = NULL;                                 \
        }                                                 \
    } while (0)
#endif

/**
 * @brief 安全释放内存（自动计算大小版本，用于已知类型的指针）
 *
 * 用法: AGENTRT_SECURE_FREE_T(my_struct_ptr, my_struct_t)
 */
#define AGENTRT_SECURE_FREE_T(ptr, type) \
    AGENTRT_SECURE_FREE((ptr), sizeof(type))

/**
 * @brief 显式内存清零（防止编译器优化掉 memset）
 *
 * 使用 volatile 函数指针确保编译器不会将清零操作优化掉。
 * 这对于安全敏感数据的擦除至关重要。
 */
static inline void agentrt_explicit_bzero(void *s, size_t n) {
    if (s == NULL || n == 0) return;
    volatile unsigned char *p = (volatile unsigned char *)s;
    while (n--) {
        *p++ = 0;
    }
}

/* ==================== 数值验证函数 ==================== */

/**
 * @brief 验证数值是否非负
 */
static inline bool agentrt_validate_non_negative(int value)
{
    return value >= 0;
}

/**
 * @brief 验证数值是否为正数
 */
static inline bool agentrt_validate_positive(int value)
{
    return value > 0;
}

/**
 * @brief 验证数值是否为有效百分比 [0, 100]
 */
static inline bool agentrt_validate_percentage(float value)
{
    return value >= 0.0f && value <= 100.0f;
}

/**
 * @brief 验证数值是否为有效概率 [0, 1]
 */
static inline bool agentrt_validate_probability(float value)
{
    return value >= 0.0f && value <= 1.0f;
}

/**
 * @brief 验证优先级是否在有效范围内
 */
static inline bool agentrt_validate_priority(int priority, int min_val, int max_val)
{
    return priority >= min_val && priority <= max_val;
}

/* ==================== 边界检查函数 ==================== */

/**
 * @brief 安全的整数加法（检测溢出）
 * @param[in] a 操作数a
 * @param[in] b 操作数b
 * @param[out] result 结果
 * @return 0成功，-1溢出
 */
static inline int safe_add_int(int a, int b, int *result)
{
    if (!result)
        return AGENTRT_EINVAL;

    if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b)) {
        return AGENTRT_EINVAL;
    }

    *result = a + b;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的整数乘法（检测溢出）
 * @param[in] a 操作数a
 * @param[in] b 操作数b
 * @param[out] result 结果
 * @return 0成功，-1溢出
 */
static inline int safe_mul_int(int a, int b, int *result)
{
    if (!result)
        return AGENTRT_EINVAL;

    if (a > 0) {
        if (b > 0 && a > INT_MAX / b)
            return AGENTRT_EINVAL;
        if (b < 0 && b < INT_MIN / a)
            return AGENTRT_EINVAL;
    } else if (a < 0) {
        if (b > 0 && a < INT_MIN / b)
            return AGENTRT_EINVAL;
        if (b < 0 && a > INT_MAX / b)
            return AGENTRT_EINVAL;
    }

    *result = a * b;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的size_t加法（检测溢出）
 * @param[in] a 操作数a
 * @param[in] b 操作数b
 * @param[out] result 结果
 * @return 0成功，-1溢出
 */
static inline int safe_add_size(size_t a, size_t b, size_t *result)
{
    if (!result)
        return AGENTRT_EINVAL;

    if (a > SIZE_MAX - b) {
        return AGENTRT_EINVAL;
    }

    *result = a + b;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的size_t乘法（检测溢出）
 * @param[in] a 操作数a
 * @param[in] b 操作数b
 * @param[out] result 结果
 * @return 0成功，-1溢出
 */
static inline int safe_mul_size(size_t a, size_t b, size_t *result)
{
    if (!result)
        return AGENTRT_EINVAL;

    if (b != 0 && a > SIZE_MAX / b) {
        return AGENTRT_EINVAL;
    }

    *result = a * b;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 检查数组访问是否安全
 * @param[in] index 索引
 * @param[in] size 数组大小
 * @return true如果安全，false否则
 */
static inline bool is_safe_array_access(size_t index, size_t size)
{
    return index < size;
}

/**
 * @brief 检查指针偏移是否安全
 * @param[in] ptr 基地址指针
 * @param[in] offset 偏移量
 * @param[in] size 缓冲区大小
 * @return true如果安全，false否则
 */
static inline bool is_safe_ptr_offset(const void *ptr, size_t offset, size_t size)
{
    if (!ptr || offset >= size) {
        return false;
    }
    return true;
}

/**
 * @brief 检查字符串拷贝是否安全
 * @param[in] src 源字符串
 * @param[in] dest 目标缓冲区
 * @param[in] dest_size 目标缓冲区大小
 * @return true如果安全，false否则
 */
static inline bool is_safe_str_copy(const char *src, char *dest, size_t dest_size)
{
    if (!src || !dest || dest_size == 0) {
        return false;
    }

    if (strlen(src) >= dest_size) {
        return false;
    }

    return true;
}

/* ==================== 内存安全辅助函数 ==================== */

/**
 * @brief 安全的内存复制（带边界检查）
 * @param[out] dest 目标缓冲区
 * @param[in] dest_size 目标缓冲区大小
 * @param[in] src 源数据
 * @param[in] src_size 源数据大小
 * @return 0成功，-1参数无效或缓冲区不足
 */
static inline int safe_memcpy(void *dest, size_t dest_size, const void *src, size_t src_size)
{
    if (!dest || !src)
        return AGENTRT_EINVAL;
    if (src_size > dest_size)
        return AGENTRT_EINVAL;

    AGENTRT_MEMCPY(dest, src, src_size);
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的内存设置（带边界检查）
 * @param[out] dest 目标缓冲区
 * @param[in] dest_size 目标缓冲区大小
 * @param[in] value 设置值
 * @param[in] count 字节数
 * @return 0成功，-1参数无效或超出范围
 */
static inline int safe_memset(void *dest, size_t dest_size, int value, size_t count)
{
    if (!dest)
        return AGENTRT_EINVAL;
    if (count > dest_size)
        return AGENTRT_EINVAL;

    AGENTRT_MEMSET(dest, value, count);
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的字符串复制（带长度限制）
 * @param[out] dest 目标缓冲区
 * @param[in] dest_size 目标缓冲区大小（包含终止符空间）
 * @param[in] src 源字符串
 * @return 0成功，-1参数无效或源字符串过长
 */
static inline int safe_strcpy(char *dest, size_t dest_size, const char *src)
{
    if (!dest || !src || dest_size == 0)
        return AGENTRT_EINVAL;

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overread"
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
    size_t src_len = strlen(src);
    if (src_len >= dest_size)
        return AGENTRT_EINVAL;

    AGENTRT_MEMCPY(dest, src, src_len + 1);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的字符串拼接（带长度限制）
 * @param[in,out] dest 目标缓冲区
 * @param[in] dest_size 目标缓冲区总大小
 * @param[in] src 源字符串
 * @return 0成功，-1参数无效或超出范围
 */
static inline int safe_strcat(char *dest, size_t dest_size, const char *src)
{
    if (!dest || !src || dest_size == 0)
        return AGENTRT_EINVAL;

    size_t current_len = strlen(dest);
    size_t src_len = strlen(src);

    if (current_len + src_len >= dest_size)
        return AGENTRT_EINVAL;

    AGENTRT_MEMCPY(dest + current_len, src, src_len + 1);
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的字符串长度获取（带NULL保护）
 * @param[in] str 字符串
 * @return 字符串长度，NULL返回0
 */
static inline size_t safe_strlen(const char *str)
{
    if (!str)
        return AGENTRT_SUCCESS;
    return strlen(str);
}

/**
 * @brief 安全的字符串比较（带NULL保护）
 * @param[in] str1 字符串1
 * @param[in] str2 字符串2
 * @return 比较结果，NULL视为空字符串
 */
static inline int safe_strcmp(const char *str1, const char *str2)
{
    if (!str1 && !str2)
        return 0;
    if (!str1)
        return -1;
    if (!str2)
        return 1;
    return strcmp(str1, str2);
}

/* ==================== 类型转换安全函数 ==================== */

/**
 * @brief 安全的int到size_t转换（检查负数）
 * @param[in] value 整数值
 * @param[out] result 转换结果
 * @return 0成功，-1负数溢出
 */
static inline int safe_int_to_size(int value, size_t *result)
{
    if (!result)
        return AGENTRT_EINVAL;
    if (value < 0)
        return AGENTRT_EINVAL;
    *result = (size_t)value;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的size_t到int转换（检查范围）
 * @param[in] value size_t值
 * @param[out] result 转换结果
 * @return 0成功，-1超出int范围
 */
static inline int safe_size_to_int(size_t value, int *result)
{
    if (!result)
        return AGENTRT_EINVAL;
    if (value > (size_t)INT_MAX)
        return AGENTRT_EINVAL;
    *result = (int)value;
    return AGENTRT_SUCCESS;
}

/**
 * @brief 安全的double转int转换（检查范围）
 * @param[in] value double值
 * @param[out] result 转换结果
 * @return 0成功，-1超出范围
 */
static inline int safe_double_to_int(double value, int *result)
{
    if (!result)
        return AGENTRT_EINVAL;
    if (value > (double)INT_MAX || value < (double)INT_MIN)
        return AGENTRT_EINVAL;
    *result = (int)value;
    return AGENTRT_SUCCESS;
}

/* ==================== 兼容别名 ==================== */

/**
 * @brief agentrt_safe_strcpy 兼容别名
 * @note 保留此别名以兼容atoms/tests/test_common_utils.c
 */
#define agentrt_safe_strcpy(dest, dest_size, src) safe_strcpy((dest), (dest_size), (src))

/**
 * @brief agentrt_safe_strcat 兼容别名
 * @note 保留此别名以兼容atoms/tests/test_common_utils.c
 */
#define agentrt_safe_strcat(dest, dest_size, src) safe_strcat((dest), (dest_size), (src))

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_QUALITY_H */
