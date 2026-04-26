/**
 * @file log_sanitizer.c
 * @brief 日志脱敏过滤器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "log_sanitizer.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>

/* 默认敏感字段模式 */
static const sensitive_field_t default_patterns[] = {
    {"api_key", "***"},
    {"apikey", "***"},
    {"api-key", "***"},
    {"password", "***"},
    {"passwd", "***"},
    {"secret", "***"},
    {"token", "***"},
    {"access_token", "***"},
    {"refresh_token", "***"},
    {"auth_token", "***"},
    {"bearer", "***"},
    {"credential", "***"},
    {"private_key", "***"},
    {"authorization", "***"},
    {"x-api-key", "***"},
};

/* 全局状态 */
static sensitive_field_t* g_patterns = NULL;
static size_t g_pattern_count = 0;
static size_t g_pattern_capacity = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_initialized = 0;

/* 默认替换字符串 */
#define DEFAULT_REPLACEMENT "***"
#define MAX_LINE_LENGTH 4096

/**
 * @brief 线程安全的初始化
 */
static void ensure_initialized(void) {
    if (g_initialized) return;
    
    pthread_mutex_lock(&g_mutex);
    if (g_initialized) {
        pthread_mutex_unlock(&g_mutex);
        return;
    }
    
    g_pattern_capacity = 32;
    g_patterns = calloc(g_pattern_capacity, sizeof(sensitive_field_t));
    if (!g_patterns) {
        pthread_mutex_unlock(&g_mutex);
        return;
    }
    
    /* 添加默认模式 */
    for (size_t i = 0; i < sizeof(default_patterns) / sizeof(default_patterns[0]); i++) {
        g_patterns[g_pattern_count++] = default_patterns[i];
    }
    
    g_initialized = 1;
    pthread_mutex_unlock(&g_mutex);
}

/**
 * @brief 简单的大小写不敏感字符串比较
 */
static const char* log_strcasestr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    
    size_t haystack_len = strlen(haystack);
    size_t needle_len = strlen(needle);
    
    if (needle_len > haystack_len) return NULL;
    
    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        int match = 1;
        for (size_t j = 0; j < needle_len; j++) {
            if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return &haystack[i];
    }
    return NULL;
}

/**
 * @brief 查找匹配模式的起始位置
 */
static const char* find_pattern(const char* message, const sensitive_field_t* pattern) {
    const char* pos = message;
    while ((pos = log_strcasestr(pos, pattern->pattern)) != NULL) {
        /* 检查是否是独立的字段（前面是空白或引号，后面是 = 或 : 或空白） */
        if (pos > message) {
            char prev = *(pos - 1);
            if (!isspace(prev) && prev != '"' && prev != '\'' && prev != ',') {
                pos++;
                continue;
            }
        }
        
        /* 检查后面是否是分隔符 */
        const char* after = pos + strlen(pattern->pattern);
        if (*after && *after != '=' && *after != ':' && *after != ' ' && *after != '"' && *after != '\'' && *after != '\n' && *after != '&') {
            pos++;
            continue;
        }
        
        return pos;
    }
    return NULL;
}

/**
 * @brief 查找字段值的结束位置
 */
static const char* find_value_end(const char* value_start) {
    if (!value_start || !*value_start) return NULL;
    
    char quote = 0;
    if (*value_start == '"' || *value_start == '\'') {
        quote = *value_start;
        value_start++;
    }
    
    if (quote) {
        /* 引号包裹的值 */
        const char* end = strchr(value_start, quote);
        return end ? end + 1 : NULL;
    } else {
        /* 普通值 */
        const char* end = value_start;
        while (*end && !isspace(*end) && *end != ',' && *end != '&' && *end != '"') {
            end++;
        }
        return end;
    }
}

/**
 * @brief 脱敏核心逻辑
 */
static int sanitize_core(const char* message, char* buffer, size_t buffer_size) {
    if (!message || !buffer || buffer_size == 0) return -1;
    
    ensure_initialized();
    
    char* out = buffer;
    char* out_end = buffer + buffer_size - 1;
    const char* pos = message;
    
    while (*pos && out < out_end) {
        int matched = 0;
        
        /* 检查每个敏感模式 */
        for (size_t i = 0; i < g_pattern_count && !matched; i++) {
            const char* match = find_pattern(pos, &g_patterns[i]);
            if (match && match == pos) {
                /* 找到匹配，复制字段名 */
                const char* after_pattern = pos + strlen(g_patterns[i].pattern);
                const char* value_start = after_pattern;
                
                /* 跳过空白和分隔符 */
                while (*value_start && (isspace(*value_start) || *value_start == '=' || *value_start == ':')) {
                    value_start++;
                }
                
                /* 找到值结束位置 */
                const char* value_end = find_value_end(value_start);
                if (!value_end) value_end = value_start + strlen(value_start);
                
                /* 计算可写的空间 */
                size_t field_name_len = after_pattern - pos;
                size_t repl_len = strlen(g_patterns[i].replacement);
                
                size_t needed = field_name_len + 1 + repl_len + 1;
                if ((size_t)(out_end - out) < needed) {
                    *out = '\0';
                    return -1;
                }
                
                /* 写入字段名 */
                memcpy(out, pos, field_name_len);
                out += field_name_len;
                
                /* 写入分隔符 */
                *out++ = '=';
                
                /* 写入脱敏值 */
                memcpy(out, g_patterns[i].replacement, repl_len);
                out += repl_len;
                
                *out = '\0';
                
                /* 更新位置 */
                pos = value_end;
                matched = 1;
            }
        }
        
        if (!matched) {
            *out++ = *pos++;
        }
    }
    
    *out = '\0';
    return (int)(out - buffer);
}

void log_sanitizer_init(size_t max_fields) {
    pthread_mutex_lock(&g_mutex);
    
    if (g_patterns) {
        free(g_patterns);
    }
    
    g_pattern_capacity = max_fields > 0 ? max_fields : 32;
    g_patterns = calloc(g_pattern_capacity, sizeof(sensitive_field_t));
    g_pattern_count = 0;
    
    /* 添加默认模式 */
    for (size_t i = 0; i < sizeof(default_patterns) / sizeof(default_patterns[0]); i++) {
        if (g_pattern_count < g_pattern_capacity) {
            g_patterns[g_pattern_count++] = default_patterns[i];
        }
    }
    
    g_initialized = 1;
    pthread_mutex_unlock(&g_mutex);
}

void log_sanitizer_destroy(void) {
    pthread_mutex_lock(&g_mutex);
    
    if (g_patterns) {
        free(g_patterns);
        g_patterns = NULL;
    }
    g_pattern_count = 0;
    g_pattern_capacity = 0;
    g_initialized = 0;
    
    pthread_mutex_unlock(&g_mutex);
}

bool log_sanitizer_add_pattern(const char* pattern, const char* replacement) {
    if (!pattern) return false;
    
    pthread_mutex_lock(&g_mutex);
    ensure_initialized();
    
    if (g_pattern_count >= g_pattern_capacity) {
        pthread_mutex_unlock(&g_mutex);
        return false;
    }
    
    g_patterns[g_pattern_count].pattern = pattern;
    g_patterns[g_pattern_count].replacement = replacement ? replacement : DEFAULT_REPLACEMENT;
    g_pattern_count++;
    
    pthread_mutex_unlock(&g_mutex);
    return true;
}

int log_sanitize(const char* message, char* buffer, size_t buffer_size) {
    return sanitize_core(message, buffer, buffer_size);
}

char* log_sanitize_dup(const char* message) {
    if (!message) return NULL;
    
    /* 预分配缓冲区 */
    size_t alloc_size = strlen(message) + 1;
    if (alloc_size < MAX_LINE_LENGTH) alloc_size = MAX_LINE_LENGTH;
    
    char* buffer = malloc(alloc_size);
    if (!buffer) return NULL;
    
    int result = sanitize_core(message, buffer, alloc_size);
    if (result < 0) {
        free(buffer);
        return NULL;
    }
    
    return buffer;
}

bool log_contains_sensitive(const char* message) {
    if (!message) return false;
    
    ensure_initialized();
    
    for (size_t i = 0; i < g_pattern_count; i++) {
        if (find_pattern(message, &g_patterns[i])) {
            return true;
        }
    }
    return false;
}

const sensitive_field_t* log_get_default_patterns(size_t* count) {
    *count = sizeof(default_patterns) / sizeof(default_patterns[0]);
    return default_patterns;
}
