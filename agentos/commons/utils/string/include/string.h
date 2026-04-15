/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file string.h
 * @brief 统一字符串处理模块 - 核心层API
 * 
 * 提供安全、高效、统一的字符串处理接口，避免缓冲区溢出等常见安全问题。
 * 本模块旨在消除项目中分散的字符串处理代码，提供一致的字符串操作策略。
 * 
 * @author Spharx AgentOS Team
 * @date 2026-03-30
 * @version 2.0
 * 
 * @note 线程安全：所有公共接口均为线程安全
 * @see ARCHITECTURAL_PRINCIPLES.md E-1 安全内生原则
 */

#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wpedantic"
    #include_next <string.h>
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #include <string.h>
#else
    #include <string.h>
#endif

#ifndef AGENTOS_STRING_H
#define AGENTOS_STRING_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>

#ifdef _WIN32
    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;
#elif !defined(__ssize_t_defined) && !defined(_SSIZE_T_DEFINED)
    /* POSIX系统: ssize_t 通常定义在 unistd.h 中 */
    #include <sys/types.h>
    #if defined(__APPLE__)
        #include <machine/types.h>
        typedef __darwin_ssize_t ssize_t;
    #elif defined(_POSIX_VERSION)
        /* 大多数POSIX系统已经通过 sys/types.h 定义了 ssize_t */
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup string_api 字符串处理API
 * @{
 */

/**
 * @brief 字符串编码类型
 */
typedef enum {
    STRING_ENCODING_ASCII,               /**< ASCII编码 */
    STRING_ENCODING_UTF8,                /**< UTF-8编码 */
    STRING_ENCODING_UTF16_LE,            /**< UTF-16小端序 */
    STRING_ENCODING_UTF16_BE,            /**< UTF-16大端序 */
    STRING_ENCODING_UTF32_LE,            /**< UTF-32小端序 */
    STRING_ENCODING_UTF32_BE,            /**< UTF-32大端序 */
    STRING_ENCODING_LATIN1,              /**< Latin-1 (ISO-8859-1) */
    STRING_ENCODING_WINDOWS_1252         /**< Windows-1252 */
} string_encoding_t;

/**
 * @brief 字符串比较选项
 */
typedef enum {
    STRING_COMPARE_CASE_SENSITIVE = 0,   /**< 区分大小写 */
    STRING_COMPARE_CASE_INSENSITIVE = 1, /**< 不区分大小写 */
    STRING_COMPARE_NATURAL = 2,          /**< 自然排序（如"file10" > "file2"） */
    STRING_COMPARE_LOCALE_AWARE = 4      /**< 区域感知比较 */
} string_compare_option_t;

/**
 * @brief 字符串分割选项
 */
typedef enum {
    STRING_SPLIT_KEEP_EMPTY = 1,         /**< 保留空子串 */
    STRING_SPLIT_TRIM_WHITESPACE = 2,    /**< 修剪空白字符 */
    STRING_SPLIT_LIMIT_COUNT = 4         /**< 限制分割次数 */
} string_split_option_t;

/**
 * @brief 字符串缓冲区结构
 */
typedef struct {
    char* data;                          /**< 缓冲区数据 */
    size_t capacity;                     /**< 缓冲区容量（包括空字符） */
    size_t length;                       /**< 当前字符串长度（不包括空字符） */
    string_encoding_t encoding;          /**< 字符串编码 */
    bool gateway;                        /**< 是否为动态分配 */
} string_buffer_t;

/**
 * @brief 字符串视图结构（不拥有数据）
 */
typedef struct {
    const char* data;                    /**< 字符串数据 */
    size_t length;                       /**< 字符串长度 */
    string_encoding_t encoding;          /**< 字符串编码 */
} string_view_t;

/**
 * @brief 字符串列表结构
 */
typedef struct {
    string_view_t* items;                /**< 字符串项数组 */
    size_t count;                        /**< 项数 */
    size_t capacity;                     /**< 数组容量 */
} string_list_t;

/**
 * @brief 字符串格式化选项
 */
typedef struct {
    size_t initial_buffer_size;          /**< 初始缓冲区大小 */
    size_t max_buffer_size;              /**< 最大缓冲区大小（0表示无限制） */
    bool locale_aware;                   /**< 是否区域感知格式化 */
    const char* null_string;             /**< NULL指针的替代字符串 */
    const char* error_string;            /**< 错误时的替代字符串 */
} string_format_options_t;

/**
 * @brief 安全复制字符串到缓冲区
 * 
 * @param[out] dest 目标缓冲区
 * @param[in] src 源字符串
 * @param[in] dest_size 目标缓冲区大小（字节）
 * @return 成功返回复制的字符数（不包括空字符），失败返回-1
 * 
 * @note 保证目标缓冲区以空字符结尾
 * @note 如果目标缓冲区大小不足，会复制尽可能多的字符并返回-1
 */
int string_copy(char* dest, const char* src, size_t dest_size);

/**
 * @brief 安全复制指定长度的字符串到缓冲区
 * 
 * @param[out] dest 目标缓冲区
 * @param[in] src 源字符串
 * @param[in] count 要复制的最大字符数
 * @param[in] dest_size 目标缓冲区大小（字节）
 * @return 成功返回复制的字符数（不包括空字符），失败返回-1
 */
int string_copy_n(char* dest, const char* src, size_t count, size_t dest_size);

/**
 * @brief 安全连接字符串到缓冲区
 * 
 * @param[inout] dest 目标缓冲区（必须为空字符结尾）
 * @param[in] src 源字符串
 * @param[in] dest_size 目标缓冲区总大小（字节）
 * @return 成功返回连接后的总长度，失败返回-1
 */
int string_concat(char* dest, const char* src, size_t dest_size);

/**
 * @brief 安全连接指定长度的字符串到缓冲区
 * 
 * @param[inout] dest 目标缓冲区（必须为空字符结尾）
 * @param[in] src 源字符串
 * @param[in] count 要连接的最大字符数
 * @param[in] dest_size 目标缓冲区总大小（字节）
 * @return 成功返回连接后的总长度，失败返回-1
 */
int string_concat_n(char* dest, const char* src, size_t count, size_t dest_size);

/**
 * @brief 比较两个字符串
 * 
 * @param[in] str1 第一个字符串
 * @param[in] str2 第二个字符串
 * @param[in] options 比较选项（STRING_COMPARE_* 的位掩码）
 * @return 相等返回0，str1 < str2返回负数，str1 > str2返回正数
 */
int string_compare(const char* str1, const char* str2, int options);

/**
 * @brief 比较两个字符串（指定长度）
 * 
 * @param[in] str1 第一个字符串
 * @param[in] str2 第二个字符串
 * @param[in] len 要比较的最大字符数
 * @param[in] options 比较选项（STRING_COMPARE_* 的位掩码）
 * @return 相等返回0，str1 < str2返回负数，str1 > str2返回正数
 */
int string_compare_n(const char* str1, const char* str2, size_t len, int options);

/**
 * @brief 计算字符串长度（安全版本）
 * 
 * @param[in] str 字符串
 * @param[in] max_len 最大检查长度（防止无界字符串）
 * @return 字符串长度（不包括空字符），如果超过max_len返回max_len
 */
size_t string_length(const char* str, size_t max_len);

/**
 * @brief 查找子字符串
 * 
 * @param[in] haystack 要搜索的字符串
 * @param[in] needle 要查找的子字符串
 * @param[in] options 比较选项（STRING_COMPARE_* 的位掩码）
 * @return 找到返回子字符串起始位置，未找到返回NULL
 */
const char* string_find(const char* haystack, const char* needle, int options);

/**
 * @brief 从末尾开始查找子字符串
 * 
 * @param[in] haystack 要搜索的字符串
 * @param[in] needle 要查找的子字符串
 * @param[in] options 比较选项（STRING_COMPARE_* 的位掩码）
 * @return 找到返回子字符串起始位置，未找到返回NULL
 */
const char* string_find_last(const char* haystack, const char* needle, int options);

/**
 * @brief 查找字符第一次出现的位置
 * 
 * @param[in] str 字符串
 * @param[in] ch 要查找的字符
 * @return 找到返回字符位置，未找到返回NULL
 */
const char* string_find_char(const char* str, char ch);

/**
 * @brief 查找字符最后一次出现的位置
 * 
 * @param[in] str 字符串
 * @param[in] ch 要查找的字符
 * @return 找到返回字符位置，未找到返回NULL
 */
const char* string_find_char_last(const char* str, char ch);

/**
 * @brief 修剪字符串开头和结尾的空白字符
 * 
 * @param[inout] str 要修剪的字符串（会被修改）
 * @return 修剪后的字符串（指向原字符串的某个位置）
 */
char* string_trim(char* str);

/**
 * @brief 修剪字符串开头的空白字符
 * 
 * @param[inout] str 要修剪的字符串（会被修改）
 * @return 修剪后的字符串（指向原字符串的某个位置）
 */
char* string_trim_start(char* str);

/**
 * @brief 修剪字符串结尾的空白字符
 * 
 * @param[inout] str 要修剪的字符串（会被修改）
 * @return 修剪后的字符串（指向原字符串的某个位置）
 */
char* string_trim_end(char* str);

/**
 * @brief 转换字符串为小写
 * 
 * @param[inout] str 要转换的字符串（会被修改）
 * @return 转换后的字符串
 */
char* string_to_lower(char* str);

/**
 * @brief 转换字符串为大写
 * 
 * @param[inout] str 要转换的字符串（会被修改）
 * @return 转换后的字符串
 */
char* string_to_upper(char* str);

/**
 * @brief 替换字符串中的子字符串
 * 
 * @param[in] str 原始字符串
 * @param[in] old_substr 要替换的子字符串
 * @param[in] new_substr 新的子字符串
 * @param[out] result 结果缓冲区
 * @param[in] result_size 结果缓冲区大小
 * @return 成功返回结果字符串长度，失败返回-1
 * 
 * @note 如果结果缓冲区大小不足，会复制尽可能多的字符并返回-1
 */
int string_replace(const char* str, const char* old_substr, const char* new_substr,
                   char* result, size_t result_size);

/**
 * @brief 分割字符串
 * 
 * @param[in] str 要分割的字符串
 * @param[in] delimiter 分隔符
 * @param[in] options 分割选项（STRING_SPLIT_* 的位掩码）
 * @param[in] limit 最大分割次数（如果设置了STRING_SPLIT_LIMIT_COUNT）
 * @return 字符串列表，使用后必须调用string_list_free释放
 */
string_list_t string_split(const char* str, const char* delimiter, int options, size_t limit);

/**
 * @brief 连接字符串列表
 * 
 * @param[in] list 字符串列表
 * @param[in] delimiter 分隔符
 * @param[out] result 结果缓冲区
 * @param[in] result_size 结果缓冲区大小
 * @return 成功返回结果字符串长度，失败返回-1
 */
int string_join(const string_list_t* list, const char* delimiter,
                char* result, size_t result_size);

/**
 * @brief 检查字符串是否以指定前缀开头
 * 
 * @param[in] str 字符串
 * @param[in] prefix 前缀
 * @param[in] options 比较选项（STRING_COMPARE_* 的位掩码）
 * @return 以指定前缀开头返回true，否则返回false
 */
bool string_starts_with(const char* str, const char* prefix, int options);

/**
 * @brief 检查字符串是否以指定后缀结尾
 * 
 * @param[in] str 字符串
 * @param[in] suffix 后缀
 * @param[in] options 比较选项（STRING_COMPARE_* 的位掩码）
 * @return 以指定后缀结尾返回true，否则返回false
 */
bool string_ends_with(const char* str, const char* suffix, int options);

/**
 * @brief 检查字符串是否只包含空白字符
 * 
 * @param[in] str 字符串
 * @return 只包含空白字符返回true，否则返回false
 */
bool string_is_blank(const char* str);

/**
 * @brief 检查字符串是否只包含数字字符
 * 
 * @param[in] str 字符串
 * @return 只包含数字字符返回true，否则返回false
 */
bool string_is_digit(const char* str);

/**
 * @brief 检查字符串是否只包含字母字符
 * 
 * @param[in] str 字符串
 * @return 只包含字母字符返回true，否则返回false
 */
bool string_is_alpha(const char* str);

/**
 * @brief 检查字符串是否只包含字母数字字符
 * 
 * @param[in] str 字符串
 * @return 只包含字母数字字符返回true，否则返回false
 */
bool string_is_alnum(const char* str);

/**
 * @brief 格式化字符串（安全版本）
 * 
 * @param[out] buffer 输出缓冲区
 * @param[in] buffer_size 缓冲区大小
 * @param[in] format 格式字符串
 * @param[in] ... 格式化参数
 * @return 成功返回写入的字符数（不包括空字符），失败返回-1
 */
int string_format(char* buffer, size_t buffer_size, const char* format, ...);

/**
 * @brief 格式化字符串（va_list版本）
 * 
 * @param[out] buffer 输出缓冲区
 * @param[in] buffer_size 缓冲区大小
 * @param[in] format 格式字符串
 * @param[in] args 格式化参数
 * @return 成功返回写入的字符数（不包括空字符），失败返回-1
 */
int string_format_v(char* buffer, size_t buffer_size, const char* format, va_list args);

/**
 * @brief 分配并格式化字符串
 * 
 * @param[in] format 格式字符串
 * @param[in] ... 格式化参数
 * @return 成功返回分配的字符串，失败返回NULL
 * 
 * @note 返回的字符串必须使用free释放
 */
char* string_alloc_format(const char* format, ...);

/**
 * @brief 分配并格式化字符串（va_list版本）
 * 
 * @param[in] format 格式字符串
 * @param[in] args 格式化参数
 * @return 成功返回分配的字符串，失败返回NULL
 */
char* string_alloc_format_v(const char* format, va_list args);

/**
 * @brief 复制字符串（分配新内存）
 * 
 * @param[in] str 源字符串
 * @return 成功返回复制的字符串，失败返回NULL
 * 
 * @note 返回的字符串必须使用free释放
 */
char* string_alloc_copy(const char* str);

/**
 * @brief 复制指定长度的字符串（分配新内存）
 * 
 * @param[in] str 源字符串
 * @param[in] len 要复制的最大长度
 * @return 成功返回复制的字符串，失败返回NULL
 */
char* string_alloc_copy_n(const char* str, size_t len);

/**
 * @brief 连接字符串（分配新内存）
 * 
 * @param[in] str1 第一个字符串
 * @param[in] str2 第二个字符串
 * @return 成功返回连接的字符串，失败返回NULL
 */
char* string_alloc_concat(const char* str1, const char* str2);

/**
 * @brief 创建字符串缓冲区
 * 
 * @param[in] initial_capacity 初始容量（不包括空字符）
 * @param[in] encoding 字符串编码
 * @return 成功返回字符串缓冲区，失败返回NULL
 */
string_buffer_t* string_buffer_create(size_t initial_capacity, string_encoding_t encoding);

/**
 * @brief 销毁字符串缓冲区
 * 
 * @param[in] buffer 字符串缓冲区
 */
void string_buffer_destroy(string_buffer_t* buffer);

/**
 * @brief 向字符串缓冲区追加字符串
 * 
 * @param[in] buffer 字符串缓冲区
 * @param[in] str 要追加的字符串
 * @return 成功返回true，失败返回false
 */
bool string_buffer_append(string_buffer_t* buffer, const char* str);

/**
 * @brief 向字符串缓冲区追加指定长度的字符串
 * 
 * @param[in] buffer 字符串缓冲区
 * @param[in] str 要追加的字符串
 * @param[in] len 要追加的长度
 * @return 成功返回true，失败返回false
 */
bool string_buffer_append_n(string_buffer_t* buffer, const char* str, size_t len);

/**
 * @brief 向字符串缓冲区追加格式化字符串
 * 
 * @param[in] buffer 字符串缓冲区
 * @param[in] format 格式字符串
 * @param[in] ... 格式化参数
 * @return 成功返回true，失败返回false
 */
bool string_buffer_append_format(string_buffer_t* buffer, const char* format, ...);

/**
 * @brief 向字符串缓冲区追加字符
 * 
 * @param[in] buffer 字符串缓冲区
 * @param[in] ch 要追加的字符
 * @return 成功返回true，失败返回false
 */
bool string_buffer_append_char(string_buffer_t* buffer, char ch);

/**
 * @brief 清空字符串缓冲区
 * 
 * @param[in] buffer 字符串缓冲区
 */
void string_buffer_clear(string_buffer_t* buffer);

/**
 * @brief 获取字符串缓冲区的C字符串
 * 
 * @param[in] buffer 字符串缓冲区
 * @return C字符串（只读，生命周期与缓冲区相同）
 */
const char* string_buffer_cstr(const string_buffer_t* buffer);

/**
 * @brief 获取字符串缓冲区的长度
 * 
 * @param[in] buffer 字符串缓冲区
 * @return 字符串长度
 */
size_t string_buffer_length(const string_buffer_t* buffer);

/**
 * @brief 创建字符串视图
 * 
 * @param[in] str C字符串
 * @param[in] encoding 字符串编码
 * @return 字符串视图
 */
string_view_t string_view_create(const char* str, string_encoding_t encoding);

/**
 * @brief 从指定长度创建字符串视图
 * 
 * @param[in] str C字符串
 * @param[in] len 字符串长度
 * @param[in] encoding 字符串编码
 * @return 字符串视图
 */
string_view_t string_view_create_n(const char* str, size_t len, string_encoding_t encoding);

/**
 * @brief 比较两个字符串视图
 * 
 * @param[in] view1 第一个字符串视图
 * @param[in] view2 第二个字符串视图
 * @param[in] options 比较选项
 * @return 相等返回0，view1 < view2返回负数，view1 > view2返回正数
 */
int string_view_compare(const string_view_t* view1, const string_view_t* view2, int options);

/**
 * @brief 查找子字符串视图
 * 
 * @param[in] haystack 要搜索的字符串视图
 * @param[in] needle 要查找的子字符串视图
 * @param[in] options 比较选项
 * @return 找到返回子字符串起始位置在haystack中的索引，未找到返回-1
 */
ssize_t string_view_find(const string_view_t* haystack, const string_view_t* needle, int options);

/**
 * @brief 字符串视图转换为C字符串（分配新内存）
 * 
 * @param[in] view 字符串视图
 * @return C字符串（必须使用free释放）
 */
char* string_view_to_cstr(const string_view_t* view);

/**
 * @brief 创建字符串列表
 * 
 * @param[in] initial_capacity 初始容量
 * @return 字符串列表
 */
string_list_t string_list_create(size_t initial_capacity);

/**
 * @brief 销毁字符串列表
 * 
 * @param[in] list 字符串列表
 */
void string_list_destroy(string_list_t* list);

/**
 * @brief 向字符串列表添加字符串视图
 * 
 * @param[inout] list 字符串列表
 * @param[in] item 要添加的字符串视图
 * @return 成功返回true，失败返回false
 */
bool string_list_add(string_list_t* list, const string_view_t* item);

/**
 * @brief 向字符串列表添加C字符串
 * 
 * @param[inout] list 字符串列表
 * @param[in] str 要添加的C字符串
 * @return 成功返回true，失败返回false
 */
bool string_list_add_cstr(string_list_t* list, const char* str);

/**
 * @brief 清空字符串列表
 * 
 * @param[inout] list 字符串列表
 */
void string_list_clear(string_list_t* list);

/**
 * @brief 获取字符串列表的大小
 * 
 * @param[in] list 字符串列表
 * @return 列表大小
 */
size_t string_list_size(const string_list_t* list);

/**
 * @brief 获取字符串列表的项
 * 
 * @param[in] list 字符串列表
 * @param[in] index 索引
 * @return 字符串视图，索引无效返回空视图
 */
string_view_t string_list_get(const string_list_t* list, size_t index);

/**
 * @brief 编码转换
 * 
 * @param[in] src 源字符串
 * @param[in] src_encoding 源编码
 * @param[out] dest 目标缓冲区
 * @param[in] dest_size 目标缓冲区大小
 * @param[in] dest_encoding 目标编码
 * @return 成功返回转换后的字节数，失败返回-1
 */
int string_convert_encoding(const char* src, string_encoding_t src_encoding,
                           char* dest, size_t dest_size, string_encoding_t dest_encoding);

/**
 * @brief 计算UTF-8字符串的字符数（码点）
 * 
 * @param[in] str UTF-8字符串
 * @param[in] max_len 最大检查长度
 * @return 字符数（码点数）
 */
size_t string_utf8_char_count(const char* str, size_t max_len);

/**
 * @brief 获取UTF-8字符串的下一个字符
 * 
 * @param[in] str UTF-8字符串
 * @param[out] ch 输出字符（Unicode码点）
 * @return 成功返回跳过的字节数，失败返回0
 */
size_t string_utf8_next_char(const char* str, uint32_t* ch);

/**
 * @brief 检查字符串是否为有效的UTF-8
 * 
 * @param[in] str 字符串
 * @param[in] len 字符串长度
 * @return 有效的UTF-8返回true，否则返回false
 */
bool string_utf8_validate(const char* str, size_t len);

/** @} */ // end of string_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_STRING_H */