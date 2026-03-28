/**
 * @file string.c
 * @brief 统一字符串处理模�?- 核心层实�? * 
 * 实现安全、高效、统一的字符串处理功能，提供完整的字符串操作API�? * 包括字符串复制、连接、比较、查找、分割、格式化等常用功能�? * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "string.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../bases/utils/memory/include/memory_compat.h"
#include "../bases/utils/string/include/string_compat.h"
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include <wchar.h>
#include <wctype.h>

#ifdef _WIN32
#include <windows.h>
#include <locale.h>
#else
#include <locale.h>
#include <strings.h>
#endif

/**
 * @brief 空白字符定义
 */
static const char* WHITESPACE_CHARS = " \t\n\r\v\f";

/**
 * @brief 默认字符串格式化选项
 */
static const string_format_options_t DEFAULT_FORMAT_OPTIONS = {
    .initial_buffer_size = 256,
    .max_buffer_size = 0,
    .locale_aware = false,
    .null_string = "(null)",
    .error_string = "(error)"
};

/**
 * @brief 内部错误代码
 */
typedef enum {
    STRING_ERROR_NONE = 0,
    STRING_ERROR_INVALID_ARGUMENT,
    STRING_ERROR_BUFFER_TOO_SMALL,
    STRING_ERROR_MEMORY_ALLOCATION,
    STRING_ERROR_ENCODING_CONVERSION,
    STRING_ERROR_FORMAT,
    STRING_ERROR_OVERFLOW
} string_error_t;

/**
 * @brief 内部上下文结�? */
typedef struct {
    string_error_t last_error;
    char error_message[256];
    bool initialized;
} string_context_t;

/**
 * @brief 全局上下文实�? */
static string_context_t g_context = {
    .last_error = STRING_ERROR_NONE,
    .error_message = {0},
    .initialized = true
};

/**
 * @brief 设置内部错误
 * 
 * @param[in] error 错误代码
 * @param[in] message 错误信息
 */
static void string_set_error(string_error_t error, const char* message) {
    g_context.last_error = error;
    if (message != NULL) {
        strncpy(g_context.error_message, message, sizeof(g_context.error_message) - 1);
        g_context.error_message[sizeof(g_context.error_message) - 1] = '\0';
    }
}

/**
 * @brief 清除内部错误
 */
static void string_clear_error(void) {
    g_context.last_error = STRING_ERROR_NONE;
    g_context.error_message[0] = '\0';
}

/**
 * @brief 安全计算字符串长�? * 
 * @param[in] str 字符�? * @param[in] max_len 最大检查长�? * @return 字符串长�? */
static size_t string_safe_strlen(const char* str, size_t max_len) {
    if (str == NULL) {
        return 0;
    }
    
    size_t len = 0;
    while (len < max_len && str[len] != '\0') {
        len++;
    }
    
    return len;
}

/**
 * @brief 检查是否为空白字符
 * 
 * @param[in] ch 字符
 * @return 是空白字符返回true，否则返回false
 */
static bool string_is_whitespace_char(char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' || 
            ch == '\r' || ch == '\v' || ch == '\f');
}

int string_copy(char* dest, const char* src, size_t dest_size) {
    string_clear_error();
    
    if (dest == NULL || src == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    size_t src_len = string_safe_strlen(src, dest_size - 1);
    
    if (src_len >= dest_size) {
        // 缓冲区不足，复制尽可能多的字�?        memcpy(dest, src, dest_size - 1);
        dest[dest_size - 1] = '\0';
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "缓冲区大小不�?);
        return -1;
    }
    
    memcpy(dest, src, src_len);
    dest[src_len] = '\0';
    
    return (int)src_len;
}

int string_copy_n(char* dest, const char* src, size_t count, size_t dest_size) {
    string_clear_error();
    
    if (dest == NULL || src == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    size_t src_len = string_safe_strlen(src, count);
    size_t copy_len = (src_len < count) ? src_len : count;
    
    if (copy_len >= dest_size) {
        // 缓冲区不足，复制尽可能多的字�?        size_t actual_copy = (dest_size > 0) ? dest_size - 1 : 0;
        memcpy(dest, src, actual_copy);
        if (dest_size > 0) {
            dest[actual_copy] = '\0';
        }
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "缓冲区大小不�?);
        return -1;
    }
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    
    return (int)copy_len;
}

int string_concat(char* dest, const char* src, size_t dest_size) {
    string_clear_error();
    
    if (dest == NULL || src == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    size_t dest_len = string_safe_strlen(dest, dest_size);
    size_t src_len = string_safe_strlen(src, dest_size - dest_len);
    
    if (dest_len + src_len >= dest_size) {
        // 缓冲区不足，连接尽可能多的字�?        size_t available = dest_size - dest_len - 1;
        if (available > 0) {
            memcpy(dest + dest_len, src, available);
            dest[dest_len + available] = '\0';
        }
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "缓冲区大小不�?);
        return -1;
    }
    
    memcpy(dest + dest_len, src, src_len);
    dest[dest_len + src_len] = '\0';
    
    return (int)(dest_len + src_len);
}

int string_concat_n(char* dest, const char* src, size_t count, size_t dest_size) {
    string_clear_error();
    
    if (dest == NULL || src == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    size_t dest_len = string_safe_strlen(dest, dest_size);
    size_t src_len = string_safe_strlen(src, count);
    size_t copy_len = (src_len < count) ? src_len : count;
    
    if (dest_len + copy_len >= dest_size) {
        // 缓冲区不足，连接尽可能多的字�?        size_t available = dest_size - dest_len - 1;
        if (available > 0) {
            memcpy(dest + dest_len, src, available);
            dest[dest_len + available] = '\0';
        }
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "缓冲区大小不�?);
        return -1;
    }
    
    memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';
    
    return (int)(dest_len + copy_len);
}

int string_compare(const char* str1, const char* str2, int options) {
    if (str1 == str2) {
        return 0;
    }
    
    if (str1 == NULL) {
        return -1;
    }
    
    if (str2 == NULL) {
        return 1;
    }
    
    if (options & STRING_COMPARE_CASE_INSENSITIVE) {
        // 不区分大小写比较
#ifdef _WIN32
        return _stricmp(str1, str2);
#else
        return strcasecmp(str1, str2);
#endif
    } else {
        // 区分大小写比�?        return strcmp(str1, str2);
    }
    
    // 注意：自然排序和区域感知比较需要更复杂的实�?    // 这里简化处理，只实现基本功�?}

int string_compare_n(const char* str1, const char* str2, size_t len, int options) {
    if (str1 == str2 || len == 0) {
        return 0;
    }
    
    if (str1 == NULL) {
        return -1;
    }
    
    if (str2 == NULL) {
        return 1;
    }
    
    if (options & STRING_COMPARE_CASE_INSENSITIVE) {
        // 不区分大小写比较
#ifdef _WIN32
        return _strnicmp(str1, str2, len);
#else
        return strncasecmp(str1, str2, len);
#endif
    } else {
        // 区分大小写比�?        return strncmp(str1, str2, len);
    }
}

size_t string_length(const char* str, size_t max_len) {
    if (str == NULL) {
        return 0;
    }
    
    return string_safe_strlen(str, max_len);
}

const char* string_find(const char* haystack, const char* needle, int options) {
    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        return NULL;
    }
    
    if (options & STRING_COMPARE_CASE_INSENSITIVE) {
        // 不区分大小写查找
        const char* h = haystack;
        size_t needle_len = strlen(needle);
        
        while (*h != '\0') {
            if (string_compare_n(h, needle, needle_len, options) == 0) {
                return h;
            }
            h++;
        }
        
        return NULL;
    } else {
        // 区分大小写查�?        return strstr(haystack, needle);
    }
}

const char* string_find_last(const char* haystack, const char* needle, int options) {
    if (haystack == NULL || needle == NULL || needle[0] == '\0') {
        return NULL;
    }
    
    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);
    
    if (needle_len > haystack_len) {
        return NULL;
    }
    
    const char* last_found = NULL;
    const char* current = haystack;
    
    while (*current != '\0') {
        if (string_compare_n(current, needle, needle_len, options) == 0) {
            last_found = current;
        }
        current++;
    }
    
    return last_found;
}

const char* string_find_char(const char* str, char ch) {
    if (str == NULL) {
        return NULL;
    }
    
    return strchr(str, ch);
}

const char* string_find_char_last(const char* str, char ch) {
    if (str == NULL) {
        return NULL;
    }
    
    return strrchr(str, ch);
}

char* string_trim(char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    // 先修剪结�?    char* end = str + strlen(str) - 1;
    while (end >= str && string_is_whitespace_char(*end)) {
        *end = '\0';
        end--;
    }
    
    // 再修剪开�?    char* start = str;
    while (*start != '\0' && string_is_whitespace_char(*start)) {
        start++;
    }
    
    // 移动字符串到开�?    if (start != str) {
        size_t len = strlen(start) + 1;
        memmove(str, start, len);
    }
    
    return str;
}

char* string_trim_start(char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    char* start = str;
    while (*start != '\0' && string_is_whitespace_char(*start)) {
        start++;
    }
    
    // 移动字符串到开�?    if (start != str) {
        size_t len = strlen(start) + 1;
        memmove(str, start, len);
    }
    
    return str;
}

char* string_trim_end(char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    char* end = str + strlen(str) - 1;
    while (end >= str && string_is_whitespace_char(*end)) {
        *end = '\0';
        end--;
    }
    
    return str;
}

char* string_to_lower(char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    char* p = str;
    while (*p != '\0') {
        *p = (char)tolower((unsigned char)*p);
        p++;
    }
    
    return str;
}

char* string_to_upper(char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    char* p = str;
    while (*p != '\0') {
        *p = (char)toupper((unsigned char)*p);
        p++;
    }
    
    return str;
}

int string_replace(const char* str, const char* old_substr, const char* new_substr,
                   char* result, size_t result_size) {
    if (str == NULL || old_substr == NULL || new_substr == NULL || 
        result == NULL || result_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    size_t str_len = strlen(str);
    size_t old_len = strlen(old_substr);
    size_t new_len = strlen(new_substr);
    
    // 计算结果长度
    size_t result_len = 0;
    const char* current = str;
    const char* next;
    
    while ((next = strstr(current, old_substr)) != NULL) {
        result_len += (next - current);
        result_len += new_len;
        current = next + old_len;
    }
    
    result_len += strlen(current);
    
    if (result_len >= result_size) {
        // 缓冲区不�?        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "缓冲区大小不�?);
        return -1;
    }
    
    // 执行替换
    char* dest = result;
    current = str;
    
    while ((next = strstr(current, old_substr)) != NULL) {
        // 复制旧子字符串之前的部分
        size_t copy_len = next - current;
        memcpy(dest, current, copy_len);
        dest += copy_len;
        
        // 复制新子字符�?        memcpy(dest, new_substr, new_len);
        dest += new_len;
        
        current = next + old_len;
    }
    
    // 复制剩余部分
    size_t remaining_len = strlen(current);
    memcpy(dest, current, remaining_len);
    dest += remaining_len;
    *dest = '\0';
    
    return (int)result_len;
}

string_list_t string_split(const char* str, const char* delimiter, int options, size_t limit) {
    string_list_t list = string_list_create(8);
    
    if (str == NULL || delimiter == NULL || delimiter[0] == '\0') {
        return list;
    }
    
    size_t delimiter_len = strlen(delimiter);
    const char* start = str;
    const char* end;
    size_t count = 0;
    
    while (*start != '\0' && (limit == 0 || count < limit - 1)) {
        end = strstr(start, delimiter);
        
        if (end == NULL) {
            // 最后一部分
            end = str + strlen(str);
        }
        
        size_t token_len = end - start;
        
        // 应用修剪选项
        const char* token_start = start;
        const char* token_end = end;
        
        if (options & STRING_SPLIT_TRIM_WHITESPACE) {
            // 修剪开头空�?            while (token_start < token_end && string_is_whitespace_char(*token_start)) {
                token_start++;
            }
            
            // 修剪结尾空白
            while (token_end > token_start && string_is_whitespace_char(*(token_end - 1))) {
                token_end--;
            }
        }
        
        size_t trimmed_len = token_end - token_start;
        
        // 检查是否保留空子串
        if (trimmed_len > 0 || (options & STRING_SPLIT_KEEP_EMPTY)) {
            string_view_t view = string_view_create_n(token_start, trimmed_len, STRING_ENCODING_UTF8);
            string_list_add(&list, &view);
            count++;
        }
        
        if (end == NULL || *end == '\0') {
            break;
        }
        
        start = end + delimiter_len;
    }
    
    // 处理最后一部分（如果limit未限制）
    if (*start != '\0' && (limit == 0 || count < limit)) {
        size_t token_len = strlen(start);
        const char* token_start = start;
        const char* token_end = start + token_len;
        
        if (options & STRING_SPLIT_TRIM_WHITESPACE) {
            // 修剪开头空�?            while (token_start < token_end && string_is_whitespace_char(*token_start)) {
                token_start++;
            }
            
            // 修剪结尾空白
            while (token_end > token_start && string_is_whitespace_char(*(token_end - 1))) {
                token_end--;
            }
        }
        
        size_t trimmed_len = token_end - token_start;
        
        if (trimmed_len > 0 || (options & STRING_SPLIT_KEEP_EMPTY)) {
            string_view_t view = string_view_create_n(token_start, trimmed_len, STRING_ENCODING_UTF8);
            string_list_add(&list, &view);
        }
    }
    
    return list;
}

int string_join(const string_list_t* list, const char* delimiter,
                char* result, size_t result_size) {
    if (list == NULL || result == NULL || result_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    size_t delimiter_len = (delimiter != NULL) ? strlen(delimiter) : 0;
    
    // 计算总长�?    size_t total_len = 0;
    for (size_t i = 0; i < list->count; i++) {
        total_len += list->items[i].length;
        if (i < list->count - 1 && delimiter_len > 0) {
            total_len += delimiter_len;
        }
    }
    
    if (total_len >= result_size) {
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "缓冲区大小不�?);
        return -1;
    }
    
    // 执行连接
    char* dest = result;
    for (size_t i = 0; i < list->count; i++) {
        const string_view_t* item = &list->items[i];
        
        // 复制�?        memcpy(dest, item->data, item->length);
        dest += item->length;
        
        // 复制分隔符（除了最后一项）
        if (i < list->count - 1 && delimiter_len > 0) {
            memcpy(dest, delimiter, delimiter_len);
            dest += delimiter_len;
        }
    }
    
    *dest = '\0';
    
    return (int)total_len;
}

bool string_starts_with(const char* str, const char* prefix, int options) {
    if (str == NULL || prefix == NULL) {
        return false;
    }
    
    size_t prefix_len = strlen(prefix);
    return string_compare_n(str, prefix, prefix_len, options) == 0;
}

bool string_ends_with(const char* str, const char* suffix, int options) {
    if (str == NULL || suffix == NULL) {
        return false;
    }
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) {
        return false;
    }
    
    const char* str_suffix = str + (str_len - suffix_len);
    return string_compare(str_suffix, suffix, options) == 0;
}

bool string_is_blank(const char* str) {
    if (str == NULL) {
        return true;
    }
    
    while (*str != '\0') {
        if (!string_is_whitespace_char(*str)) {
            return false;
        }
        str++;
    }
    
    return true;
}

bool string_is_digit(const char* str) {
    if (str == NULL || *str == '\0') {
        return false;
    }
    
    while (*str != '\0') {
        if (!isdigit((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    
    return true;
}

bool string_is_alpha(const char* str) {
    if (str == NULL || *str == '\0') {
        return false;
    }
    
    while (*str != '\0') {
        if (!isalpha((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    
    return true;
}

bool string_is_alnum(const char* str) {
    if (str == NULL || *str == '\0') {
        return false;
    }
    
    while (*str != '\0') {
        if (!isalnum((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    
    return true;
}

int string_format(char* buffer, size_t buffer_size, const char* format, ...) {
    if (buffer == NULL || format == NULL || buffer_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    va_list args;
    va_start(args, format);
    int result = string_format_v(buffer, buffer_size, format, args);
    va_end(args);
    
    return result;
}

int string_format_v(char* buffer, size_t buffer_size, const char* format, va_list args) {
    if (buffer == NULL || format == NULL || buffer_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    // 使用vsnprintf进行格式�?    va_list args_copy;
    va_copy(args_copy, args);
    
    int result = vsnprintf(buffer, buffer_size, format, args_copy);
    va_end(args_copy);
    
    if (result < 0) {
        string_set_error(STRING_ERROR_FORMAT, "格式化失�?);
        return -1;
    }
    
    if ((size_t)result >= buffer_size) {
        string_set_error(STRING_ERROR_BUFFER_TOO_SMALL, "缓冲区大小不�?);
        // 缓冲区被截断，但已正确以空字符结�?        return -1;
    }
    
    return result;
}

char* string_alloc_format(const char* format, ...) {
    if (format == NULL) {
        return NULL;
    }
    
    va_list args;
    va_start(args, format);
    char* result = string_alloc_format_v(format, args);
    va_end(args);
    
    return result;
}

char* string_alloc_format_v(const char* format, va_list args) {
    if (format == NULL) {
        return NULL;
    }
    
    // 第一次调用计算所需长度
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (needed < 0) {
        string_set_error(STRING_ERROR_FORMAT, "格式化失�?);
        return NULL;
    }
    
    // 分配内存
    char* buffer = (char*)AGENTOS_MALLOC((size_t)needed + 1);
    if (buffer == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        return NULL;
    }
    
    // 第二次调用实际格式化
    va_copy(args_copy, args);
    int result = vsnprintf(buffer, (size_t)needed + 1, format, args_copy);
    va_end(args_copy);
    
    if (result < 0) {
        AGENTOS_FREE(buffer);
        string_set_error(STRING_ERROR_FORMAT, "格式化失�?);
        return NULL;
    }
    
    return buffer;
}

char* string_alloc_copy(const char* str) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t len = strlen(str);
    char* copy = (char*)AGENTOS_MALLOC(len + 1);
    if (copy == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        return NULL;
    }
    
    memcpy(copy, str, len);
    copy[len] = '\0';
    
    return copy;
}

char* string_alloc_copy_n(const char* str, size_t len) {
    if (str == NULL) {
        return NULL;
    }
    
    size_t actual_len = string_safe_strlen(str, len);
    char* copy = (char*)AGENTOS_MALLOC(actual_len + 1);
    if (copy == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        return NULL;
    }
    
    memcpy(copy, str, actual_len);
    copy[actual_len] = '\0';
    
    return copy;
}

char* string_alloc_concat(const char* str1, const char* str2) {
    if (str1 == NULL && str2 == NULL) {
        return NULL;
    }
    
    size_t len1 = (str1 != NULL) ? strlen(str1) : 0;
    size_t len2 = (str2 != NULL) ? strlen(str2) : 0;
    
    char* result = (char*)AGENTOS_MALLOC(len1 + len2 + 1);
    if (result == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        return NULL;
    }
    
    if (str1 != NULL) {
        memcpy(result, str1, len1);
    }
    
    if (str2 != NULL) {
        memcpy(result + len1, str2, len2);
    }
    
    result[len1 + len2] = '\0';
    
    return result;
}

string_buffer_t* string_buffer_create(size_t initial_capacity, string_encoding_t encoding) {
    if (initial_capacity == 0) {
        initial_capacity = 16;
    }
    
    string_buffer_t* buffer = (string_buffer_t*)AGENTOS_MALLOC(sizeof(string_buffer_t));
    if (buffer == NULL) {
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        return NULL;
    }
    
    buffer->data = (char*)AGENTOS_MALLOC(initial_capacity + 1);
    if (buffer->data == NULL) {
        AGENTOS_FREE(buffer);
        string_set_error(STRING_ERROR_MEMORY_ALLOCATION, "内存分配失败");
        return NULL;
    }
    
    buffer->data[0] = '\0';
    buffer->capacity = initial_capacity + 1;
    buffer->length = 0;
    buffer->encoding = encoding;
    buffer->gateway = true;
    
    return buffer;
}

void string_buffer_destroy(string_buffer_t* buffer) {
    if (buffer == NULL) {
        return;
    }
    
    if (buffer->gateway && buffer->data != NULL) {
        AGENTOS_FREE(buffer->data);
    }
    
    AGENTOS_FREE(buffer);
}

bool string_buffer_append(string_buffer_t* buffer, const char* str) {
    if (buffer == NULL || str == NULL) {
        return false;
    }
    
    return string_buffer_append_n(buffer, str, strlen(str));
}

bool string_buffer_append_n(string_buffer_t* buffer, const char* str, size_t len) {
    if (buffer == NULL || str == NULL) {
        return false;
    }
    
    // 检查是否需要扩�?    size_t new_length = buffer->length + len;
    if (new_length >= buffer->capacity) {
        // 计算新容�?        size_t new_capacity = buffer->capacity * 2;
        while (new_capacity <= new_length) {
            new_capacity *= 2;
        }
        
        char* new_data = (char*)AGENTOS_REALLOC(buffer->data, new_capacity);
        if (new_data == NULL) {
            return false;
        }
        
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    
    // 追加字符�?    memcpy(buffer->data + buffer->length, str, len);
    buffer->length = new_length;
    buffer->data[buffer->length] = '\0';
    
    return true;
}

bool string_buffer_append_format(string_buffer_t* buffer, const char* format, ...) {
    if (buffer == NULL || format == NULL) {
        return false;
    }
    
    va_list args;
    va_start(args, format);
    
    // 计算所需长度
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (needed < 0) {
        va_end(args);
        return false;
    }
    
    // 检查是否需要扩�?    size_t new_length = buffer->length + (size_t)needed;
    if (new_length >= buffer->capacity) {
        size_t new_capacity = buffer->capacity;
        while (new_capacity <= new_length) {
            new_capacity *= 2;
        }
        
        char* new_data = (char*)AGENTOS_REALLOC(buffer->data, new_capacity);
        if (new_data == NULL) {
            va_end(args);
            return false;
        }
        
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    
    // 格式化到缓冲�?    int result = vsnprintf(buffer->data + buffer->length, 
                          buffer->capacity - buffer->length,
                          format, args);
    va_end(args);
    
    if (result < 0) {
        return false;
    }
    
    buffer->length += (size_t)result;
    return true;
}

bool string_buffer_append_char(string_buffer_t* buffer, char ch) {
    if (buffer == NULL) {
        return false;
    }
    
    // 检查是否需要扩�?    if (buffer->length + 1 >= buffer->capacity) {
        size_t new_capacity = buffer->capacity * 2;
        char* new_data = (char*)AGENTOS_REALLOC(buffer->data, new_capacity);
        if (new_data == NULL) {
            return false;
        }
        
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }
    
    // 追加字符
    buffer->data[buffer->length] = ch;
    buffer->length++;
    buffer->data[buffer->length] = '\0';
    
    return true;
}

void string_buffer_clear(string_buffer_t* buffer) {
    if (buffer == NULL) {
        return;
    }
    
    buffer->length = 0;
    if (buffer->data != NULL) {
        buffer->data[0] = '\0';
    }
}

const char* string_buffer_cstr(const string_buffer_t* buffer) {
    if (buffer == NULL) {
        return NULL;
    }
    
    return buffer->data;
}

size_t string_buffer_length(const string_buffer_t* buffer) {
    if (buffer == NULL) {
        return 0;
    }
    
    return buffer->length;
}

string_view_t string_view_create(const char* str, string_encoding_t encoding) {
    string_view_t view = {
        .data = str,
        .length = (str != NULL) ? strlen(str) : 0,
        .encoding = encoding
    };
    
    return view;
}

string_view_t string_view_create_n(const char* str, size_t len, string_encoding_t encoding) {
    string_view_t view = {
        .data = str,
        .length = len,
        .encoding = encoding
    };
    
    return view;
}

int string_view_compare(const string_view_t* view1, const string_view_t* view2, int options) {
    if (view1 == view2) {
        return 0;
    }
    
    if (view1 == NULL) {
        return -1;
    }
    
    if (view2 == NULL) {
        return 1;
    }
    
    // 使用较短的长�?    size_t min_len = (view1->length < view2->length) ? view1->length : view2->length;
    
    int result = 0;
    if (options & STRING_COMPARE_CASE_INSENSITIVE) {
        // 不区分大小写比较
        for (size_t i = 0; i < min_len; i++) {
            char ch1 = (char)tolower((unsigned char)view1->data[i]);
            char ch2 = (char)tolower((unsigned char)view2->data[i]);
            
            if (ch1 != ch2) {
                result = (ch1 < ch2) ? -1 : 1;
                break;
            }
        }
    } else {
        // 区分大小写比�?        result = memcmp(view1->data, view2->data, min_len);
    }
    
    // 如果前min_len个字符相同，比较长度
    if (result == 0 && view1->length != view2->length) {
        result = (view1->length < view2->length) ? -1 : 1;
    }
    
    return result;
}

ssize_t string_view_find(const string_view_t* haystack, const string_view_t* needle, int options) {
    if (haystack == NULL || needle == NULL || 
        needle->length == 0 || needle->length > haystack->length) {
        return -1;
    }
    
    for (size_t i = 0; i <= haystack->length - needle->length; i++) {
        string_view_t subview = {
            .data = haystack->data + i,
            .length = needle->length,
            .encoding = haystack->encoding
        };
        
        if (string_view_compare(&subview, needle, options) == 0) {
            return (ssize_t)i;
        }
    }
    
    return -1;
}

char* string_view_to_cstr(const string_view_t* view) {
    if (view == NULL) {
        return NULL;
    }
    
    char* str = (char*)AGENTOS_MALLOC(view->length + 1);
    if (str == NULL) {
        return NULL;
    }
    
    memcpy(str, view->data, view->length);
    str[view->length] = '\0';
    
    return str;
}

string_list_t string_list_create(size_t initial_capacity) {
    string_list_t list = {
        .items = NULL,
        .count = 0,
        .capacity = 0
    };
    
    if (initial_capacity > 0) {
        list.items = (string_view_t*)AGENTOS_MALLOC(initial_capacity * sizeof(string_view_t));
        if (list.items != NULL) {
            list.capacity = initial_capacity;
        }
    }
    
    return list;
}

void string_list_destroy(string_list_t* list) {
    if (list == NULL) {
        return;
    }
    
    if (list->items != NULL) {
        AGENTOS_FREE(list->items);
        list->items = NULL;
    }
    
    list->count = 0;
    list->capacity = 0;
}

bool string_list_add(string_list_t* list, const string_view_t* item) {
    if (list == NULL || item == NULL) {
        return false;
    }
    
    // 检查是否需要扩�?    if (list->count >= list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        string_view_t* new_items = (string_view_t*)AGENTOS_REALLOC(list->items, 
                                                          new_capacity * sizeof(string_view_t));
        if (new_items == NULL) {
            return false;
        }
        
        list->items = new_items;
        list->capacity = new_capacity;
    }
    
    // 添加项（浅拷贝）
    list->items[list->count] = *item;
    list->count++;
    
    return true;
}

bool string_list_add_cstr(string_list_t* list, const char* str) {
    if (list == NULL || str == NULL) {
        return false;
    }
    
    string_view_t view = string_view_create(str, STRING_ENCODING_UTF8);
    return string_list_add(list, &view);
}

void string_list_clear(string_list_t* list) {
    if (list == NULL) {
        return;
    }
    
    list->count = 0;
}

size_t string_list_size(const string_list_t* list) {
    if (list == NULL) {
        return 0;
    }
    
    return list->count;
}

string_view_t string_list_get(const string_list_t* list, size_t index) {
    static const string_view_t EMPTY_VIEW = {NULL, 0, STRING_ENCODING_ASCII};
    
    if (list == NULL || index >= list->count) {
        return EMPTY_VIEW;
    }
    
    return list->items[index];
}

int string_convert_encoding(const char* src, string_encoding_t src_encoding,
                           char* dest, size_t dest_size, string_encoding_t dest_encoding) {
    // 简化实现：仅支持ASCII和UTF-8之间的转�?    if (src == NULL || dest == NULL || dest_size == 0) {
        string_set_error(STRING_ERROR_INVALID_ARGUMENT, "无效参数");
        return -1;
    }
    
    if (src_encoding == dest_encoding) {
        // 编码相同，直接复�?        return string_copy(dest, src, dest_size);
    }
    
    // 这里实现简单的编码转换
    // 实际实现可能需要使用libiconv或其他编码库
    
    string_set_error(STRING_ERROR_ENCODING_CONVERSION, "编码转换未完全实�?);
    return -1;
}

size_t string_utf8_char_count(const char* str, size_t max_len) {
    if (str == NULL) {
        return 0;
    }
    
    size_t count = 0;
    size_t i = 0;
    
    while (i < max_len && str[i] != '\0') {
        unsigned char ch = (unsigned char)str[i];
        
        // 检查UTF-8字符起始字节
        if ((ch & 0xC0) != 0x80) {
            count++;
        }
        
        i++;
    }
    
    return count;
}

size_t string_utf8_next_char(const char* str, uint32_t* ch) {
    if (str == NULL || ch == NULL) {
        return 0;
    }
    
    unsigned char first = (unsigned char)str[0];
    
    if (first == 0) {
        return 0;
    }
    
    // 单字节字�?(0xxxxxxx)
    if (first < 0x80) {
        *ch = first;
        return 1;
    }
    
    // 双字节字�?(110xxxxx)
    if ((first & 0xE0) == 0xC0) {
        if (str[1] == 0) {
            return 0;
        }
        
        unsigned char second = (unsigned char)str[1];
        if ((second & 0xC0) != 0x80) {
            return 0;
        }
        
        *ch = ((first & 0x1F) << 6) | (second & 0x3F);
        return 2;
    }
    
    // 三字节字�?(1110xxxx)
    if ((first & 0xF0) == 0xE0) {
        if (str[1] == 0 || str[2] == 0) {
            return 0;
        }
        
        unsigned char second = (unsigned char)str[1];
        unsigned char third = (unsigned char)str[2];
        
        if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80) {
            return 0;
        }
        
        *ch = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
        return 3;
    }
    
    // 四字节字�?(11110xxx)
    if ((first & 0xF8) == 0xF0) {
        if (str[1] == 0 || str[2] == 0 || str[3] == 0) {
            return 0;
        }
        
        unsigned char second = (unsigned char)str[1];
        unsigned char third = (unsigned char)str[2];
        unsigned char fourth = (unsigned char)str[3];
        
        if ((second & 0xC0) != 0x80 || (third & 0xC0) != 0x80 || (fourth & 0xC0) != 0x80) {
            return 0;
        }
        
        *ch = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | 
              ((third & 0x3F) << 6) | (fourth & 0x3F);
        return 4;
    }
    
    // 无效的UTF-8起始字节
    return 0;
}

bool string_utf8_validate(const char* str, size_t len) {
    if (str == NULL) {
        return true; // 空指针视为有�?    }
    
    size_t i = 0;
    while (i < len && str[i] != '\0') {
        unsigned char first = (unsigned char)str[i];
        
        // 单字节字�?(0xxxxxxx)
        if (first < 0x80) {
            i++;
            continue;
        }
        
        // 多字节字�?        size_t char_len = 0;
        
        // 双字节字�?(110xxxxx)
        if ((first & 0xE0) == 0xC0) {
            char_len = 2;
        }
        // 三字节字�?(1110xxxx)
        else if ((first & 0xF0) == 0xE0) {
            char_len = 3;
        }
        // 四字节字�?(11110xxx)
        else if ((first & 0xF8) == 0xF0) {
            char_len = 4;
        }
        // 无效的UTF-8起始字节
        else {
            return false;
        }
        
        // 检查是否有足够的字�?        if (i + char_len > len) {
            return false;
        }
        
        // 检查后续字节是否为10xxxxxx
        for (size_t j = 1; j < char_len; j++) {
            unsigned char next = (unsigned char)str[i + j];
            if ((next & 0xC0) != 0x80) {
                return false;
            }
        }
        
        // 检查过长的编码（如0xC0 0x80编码U+0000，这是无效的�?        if (char_len == 2 && first == 0xC0 && (unsigned char)str[i + 1] == 0x80) {
            return false;
        }
        
        // 检查Unicode码点是否在有效范围内
        // 这里简化检查，实际可能需要更复杂的验�?        uint32_t code_point = 0;
        if (char_len == 2) {
            code_point = ((first & 0x1F) << 6) | (str[i + 1] & 0x3F);
        } else if (char_len == 3) {
            code_point = ((first & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
        } else if (char_len == 4) {
            code_point = ((first & 0x07) << 18) | ((str[i + 1] & 0x3F) << 12) | 
                         ((str[i + 2] & 0x3F) << 6) | (str[i + 3] & 0x3F);
        }
        
        // 检查码点是否在有效范围�?        if (code_point > 0x10FFFF) {
            return false;
        }
        
        // 检查是否为代理对（U+D800到U+DFFF�?        if (code_point >= 0xD800 && code_point <= 0xDFFF) {
            return false;
        }
        
        // 检查过大的码点�? 0x10FFFF�?        if (code_point > 0x10FFFF) {
            return false;
        }
        
        i += char_len;
    }
    
    return true;
}