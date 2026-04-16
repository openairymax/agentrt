/**
 * @file input_validator.c
 * @brief 输入验证工具库实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "input_validator.h"
#include "../observability/include/logger.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#include <netdb.h>
#endif

/* ==================== 内部常量 ==================== */

/** @brief 安全标识符字符集 */
static const char* SAFE_IDENTIFIER_CHARS __attribute__((unused)) = 
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

/** @brief 危险Shell命令 */
static const char* DANGEROUS_COMMANDS[] = {
    "rm -rf", "dd", "mkfs", "fdisk", "format",
    "del /", "erase", "shutdown", "reboot",
    "chmod 777", "chown", "> /dev/", "mkfs",
    NULL
};

/** @brief SQL危险关键�?*/
static const char* SQL_DANGEROUS_KEYWORDS[] = {
    "DROP", "TRUNCATE", "ALTER", "DELETE FROM",
    "--", "/*", "*/", "; --", "UNION SELECT",
    "OR 1=1", "OR '1'='1", "EXEC(", "EXECUTE(",
    NULL
};

/** @brief 危险URL协议 */
static const char* DANGEROUS_URL_SCHEMES[] = {
    "javascript", "data", "vbscript", "file",
    "about", "blob", "filesystem",
    NULL
};

/** @brief 内网IP范围 */
static const char* PRIVATE_IP_PREFIXES[] = {
    "10.", "172.16.", "172.17.", "172.18.", "172.19.",
    "172.20.", "172.21.", "172.22.", "172.23.", "172.24.",
    "172.25.", "172.26.", "172.27.", "172.28.", "172.29.",
    "172.30.", "172.31.", "192.168.", "127.", "0.",
    "169.254.", "::1", "fc", "fd", "fe", "ff",
    NULL
};

/* ==================== 辅助函数 ==================== */

/**
 * @brief 跨平台不区分大小写字符串比较（前n个字符）
 */
#ifdef _WIN32
#define strncasecmp_ci _strnicmp
#else
#define strncasecmp_ci strncasecmp
#endif

/**
 * @brief 检查字符串是否以指定前缀开头（不区分大小写�?
 */
static int starts_with_case(const char* str, const char* prefix) {
    if (!str || !prefix) return 0;
    size_t prefix_len = strlen(prefix);
    if (strlen(str) < prefix_len) return 0;
    return strncasecmp_ci(str, prefix, prefix_len) == 0;
}

/**
 * @brief 检查字符串是否包含指定子串（不区分大小写）
 */
static int contains_case(const char* str, const char* substr) {
    if (!str || !substr) return 0;
    
    size_t str_len = strlen(str);
    size_t substr_len = strlen(substr);
    
    if (substr_len > str_len) return 0;
    
    for (size_t i = 0; i <= str_len - substr_len; i++) {
        if (strncasecmp_ci(str + i, substr, substr_len) == 0) {
            return 1;
        }
    }
    return 0;
}

/* ==================== 字符串验证实�?==================== */

/**
 * @brief 验证字符串长�?
 */
void agentos_validate_string_length(
    const char* str,
    size_t min_len,
    size_t max_len,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (!str) {
        result->error_message = "String is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    size_t len = strlen(str);
    
    if (len < min_len) {
        result->error_message = "String too short";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    if (len > max_len) {
        result->error_message = "String too long";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    result->is_valid = 1;
}

/**
 * @brief 验证字符串是否只包含安全字符
 */
void agentos_validate_string_charset(
    const char* str,
    const char* allowed_chars,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (!str) {
        result->error_message = "String is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    if (!allowed_chars) {
        result->error_message = "Allowed chars is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "allowed_chars";
        return;
    }
    
    for (const char* p = str; *p; p++) {
        if (!strchr(allowed_chars, *p)) {
            result->error_message = "String contains disallowed character";
            result->error_code = AGENTOS_ESANITIZE;
            result->error_field = "str";
            return;
        }
    }
    
    result->is_valid = 1;
}

/**
 * @brief 验证标识�?
 */
void agentos_validate_identifier(
    const char* str,
    size_t max_len,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (!str) {
        result->error_message = "Identifier is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    size_t len = strlen(str);
    
    if (len == 0) {
        result->error_message = "Identifier is empty";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    if (len > max_len) {
        result->error_message = "Identifier too long";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    if (!isalpha(str[0]) && str[0] != '_') {
        result->error_message = "Identifier must start with letter or underscore";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    for (size_t i = 1; i < len; i++) {
        if (!isalnum(str[i]) && str[i] != '_') {
            result->error_message = "Identifier contains invalid character";
            result->error_code = AGENTOS_ESANITIZE;
            result->error_field = "str";
            return;
        }
    }
    
    result->is_valid = 1;
}

/**
 * @brief 验证JSON字符�?
 */
void agentos_validate_json_string(
    const char* str,
    size_t max_len,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (!str) {
        result->error_message = "JSON string is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    size_t len = strlen(str);
    
    if (len > max_len) {
        result->error_message = "JSON string too long";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "str";
        return;
    }
    
    int brace_count = 0;
    int bracket_count = 0;
    int in_string = 0;
    int escape_next = 0;
    
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        
        if (escape_next) {
            escape_next = 0;
            continue;
        }
        
        if (c == '\\' && in_string) {
            escape_next = 1;
            continue;
        }
        
        if (c == '"') {
            in_string = !in_string;
            continue;
        }
        
        if (!in_string) {
            if (c == '{') brace_count++;
            else if (c == '}') brace_count--;
            else if (c == '[') bracket_count++;
            else if (c == ']') bracket_count--;
            
            if (brace_count < 0 || bracket_count < 0) {
                result->error_message = "JSON has unbalanced brackets";
                result->error_code = AGENTOS_ESANITIZE;
                result->error_field = "str";
                return;
            }
        }
    }
    
    if (in_string) {
        result->error_message = "JSON has unclosed string";
        result->error_code = AGENTOS_ESANITIZE;
        result->error_field = "str";
        return;
    }
    
    if (brace_count != 0 || bracket_count != 0) {
        result->error_message = "JSON has unbalanced brackets";
        result->error_code = AGENTOS_ESANITIZE;
        result->error_field = "str";
        return;
    }
    
    result->is_valid = 1;
}

/* ==================== 路径验证实现 ==================== */

/**
 * @brief 验证文件路径安全�?
 */
void agentos_validate_file_path(
    const char* path,
    const char* allowed_root,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (!path) {
        result->error_message = "Path is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "path";
        return;
    }
    
    size_t len = strlen(path);
    
    if (len == 0) {
        result->error_message = "Path is empty";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "path";
        return;
    }
    
    if (len > 4096) {
        result->error_message = "Path too long";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "path";
        return;
    }
    
    if (strchr(path, '\0') != path + len) {
        result->error_message = "Path contains null byte";
        result->error_code = AGENTOS_ESECURITY;
        result->error_field = "path";
        return;
    }
    
    if (strstr(path, "..") || strstr(path, "../") || strstr(path, "..\\")) {
        result->error_message = "Path contains directory traversal";
        result->error_code = AGENTOS_ESECURITY;
        result->error_field = "path";
        return;
    }
    
    if (allowed_root && !starts_with_case(path, allowed_root)) {
        result->error_message = "Path outside allowed root";
        result->error_code = AGENTOS_ESECURITY;
        result->error_field = "path";
        return;
    }
    
    result->is_valid = 1;
}

/**
 * @brief 规范化路�?
 */
agentos_error_t agentos_normalize_path(
    const char* path,
    char** out_normalized,
    size_t* out_len) {
    
    if (!path || !out_normalized) return AGENTOS_EINVAL;
    
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetFullPathNameA(path, MAX_PATH, buffer, NULL);
    if (len == 0 || len >= MAX_PATH) {
        return AGENTOS_EINVAL;
    }
    
    *out_normalized = AGENTOS_STRDUP(buffer);
    if (!*out_normalized) return AGENTOS_ENOMEM;
    
    if (out_len) *out_len = strlen(*out_normalized);
    return AGENTOS_SUCCESS;
#else
    char* resolved = realpath(path, NULL);
    if (!resolved) return AGENTOS_EINVAL;
    
    *out_normalized = resolved;
    if (out_len) *out_len = strlen(resolved);
    return AGENTOS_SUCCESS;
#endif
}

/* ==================== 命令验证实现 ==================== */

/**
 * @brief 验证Shell命令安全�?
 */
void agentos_validate_shell_command(
    const char* cmd,
    const char** allowed_commands,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (!cmd) {
        result->error_message = "Command is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "cmd";
        return;
    }
    
    if (strchr(cmd, ';') || strchr(cmd, '|') || strchr(cmd, '&') ||
        strchr(cmd, '$') || strchr(cmd, '`') || strchr(cmd, '\n')) {
        result->error_message = "Command contains dangerous characters";
        result->error_code = AGENTOS_ESECURITY;
        result->error_field = "cmd";
        return;
    }
    
    for (int i = 0; DANGEROUS_COMMANDS[i]; i++) {
        if (contains_case(cmd, DANGEROUS_COMMANDS[i])) {
            result->error_message = "Command contains dangerous operation";
            result->error_code = AGENTOS_ESECURITY;
            result->error_field = "cmd";
            return;
        }
    }
    
    if (allowed_commands) {
        int found = 0;
        for (int i = 0; allowed_commands[i]; i++) {
            if (starts_with_case(cmd, allowed_commands[i])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            result->error_message = "Command not in allowed list";
            result->error_code = AGENTOS_ESECURITY;
            result->error_field = "cmd";
            return;
        }
    }
    
    result->is_valid = 1;
}

/**
 * @brief 净化Shell参数
 */
agentos_error_t agentos_sanitize_shell_param(
    const char* param,
    char** out_sanitized) {
    
    if (!param || !out_sanitized) return AGENTOS_EINVAL;
    
    size_t len = strlen(param);
    size_t buf_size = len * 2 + 3;
    
    char* sanitized = (char*)AGENTOS_MALLOC(buf_size);
    if (!sanitized) return AGENTOS_ENOMEM;
    
    size_t j = 0;
    sanitized[j++] = '\'';
    
    for (size_t i = 0; i < len; i++) {
        char c = param[i];
        if (c == '\'') {
            sanitized[j++] = '\'';
            sanitized[j++] = '\\';
            sanitized[j++] = '\'';
            sanitized[j++] = '\'';
        } else if (isprint(c) && c != '`' && c != '$') {
            sanitized[j++] = c;
        }
    }
    
    sanitized[j++] = '\'';
    sanitized[j] = '\0';
    
    *out_sanitized = sanitized;
    return AGENTOS_SUCCESS;
}

/* ==================== SQL验证实现 ==================== */

/**
 * @brief 验证SQL查询安全�?
 */
void agentos_validate_sql_query(
    const char* sql,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (!sql) {
        result->error_message = "SQL is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "sql";
        return;
    }
    
    for (int i = 0; SQL_DANGEROUS_KEYWORDS[i]; i++) {
        if (contains_case(sql, SQL_DANGEROUS_KEYWORDS[i])) {
            result->error_message = "SQL contains dangerous keyword";
            result->error_code = AGENTOS_ESECURITY;
            result->error_field = "sql";
            return;
        }
    }
    
    int quote_count = 0;
    for (const char* p = sql; *p; p++) {
        if (*p == '\'') quote_count++;
    }
    
    if (quote_count % 2 != 0) {
        result->error_message = "SQL has unbalanced quotes";
        result->error_code = AGENTOS_ESANITIZE;
        result->error_field = "sql";
        return;
    }
    
    result->is_valid = 1;
}

/**
 * @brief 净化SQL标识�?
 */
agentos_error_t agentos_sanitize_sql_identifier(
    const char* identifier,
    char** out_sanitized) {
    
    if (!identifier || !out_sanitized) return AGENTOS_EINVAL;
    
    agentos_validation_result_t result;
    agentos_validate_identifier(identifier, 128, &result);
    
    if (!result.is_valid) {
        return AGENTOS_ESANITIZE;
    }
    
    size_t len = strlen(identifier);
    char* sanitized = (char*)AGENTOS_MALLOC(len + 3);
    if (!sanitized) return AGENTOS_ENOMEM;
    
    sanitized[0] = '"';
    memcpy(sanitized + 1, identifier, len);
    sanitized[len + 1] = '"';
    sanitized[len + 2] = '\0';
    
    *out_sanitized = sanitized;
    return AGENTOS_SUCCESS;
}

/* ==================== URL验证实现 ==================== */

/**
 * @brief 验证URL安全�?
 */
void agentos_validate_url(
    const char* url,
    const char** allowed_schemes,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (!url) {
        result->error_message = "URL is NULL";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "url";
        return;
    }
    
    for (int i = 0; DANGEROUS_URL_SCHEMES[i]; i++) {
        if (starts_with_case(url, DANGEROUS_URL_SCHEMES[i])) {
            result->error_message = "URL uses dangerous scheme";
            result->error_code = AGENTOS_ESECURITY;
            result->error_field = "url";
            return;
        }
    }
    
    if (allowed_schemes) {
        int found = 0;
        for (int i = 0; allowed_schemes[i]; i++) {
            if (starts_with_case(url, allowed_schemes[i])) {
                found = 1;
                break;
            }
        }
        if (!found) {
            result->error_message = "URL scheme not allowed";
            result->error_code = AGENTOS_ESECURITY;
            result->error_field = "url";
            return;
        }
    }
    
    for (int i = 0; PRIVATE_IP_PREFIXES[i]; i++) {
        if (contains_case(url, PRIVATE_IP_PREFIXES[i])) {
            result->error_message = "URL points to private IP";
            result->error_code = AGENTOS_ESECURITY;
            result->error_field = "url";
            return;
        }
    }
    
    if (contains_case(url, "localhost") || contains_case(url, "127.0.0.1")) {
        result->error_message = "URL points to localhost";
        result->error_code = AGENTOS_ESECURITY;
        result->error_field = "url";
        return;
    }
    
    result->is_valid = 1;
}

/**
 * @brief 解析URL组件
 */
agentos_error_t agentos_parse_url(
    const char* url,
    char** out_scheme,
    char** out_host,
    uint16_t* out_port,
    char** out_path) {
    
    if (!url) return AGENTOS_EINVAL;
    
    const char* scheme_end = strstr(url, "://");
    if (!scheme_end) return AGENTOS_EINVAL;
    
    size_t scheme_len = scheme_end - url;
    if (out_scheme) {
        *out_scheme = (char*)AGENTOS_MALLOC(scheme_len + 1);
        if (!*out_scheme) return AGENTOS_ENOMEM;
        memcpy(*out_scheme, url, scheme_len);
        (*out_scheme)[scheme_len] = '\0';
    }
    
    const char* host_start = scheme_end + 3;
    const char* port_start = strchr(host_start, ':');
    const char* path_start = strchr(host_start, '/');
    
    if (!path_start) path_start = url + strlen(url);
    
    size_t host_len;
    uint16_t port = 0;
    
    if (port_start && port_start < path_start) {
        host_len = port_start - host_start;
        port = (uint16_t)atoi(port_start + 1);
    } else {
        host_len = path_start - host_start;
    }
    
    if (out_host) {
        *out_host = (char*)AGENTOS_MALLOC(host_len + 1);
        if (!*out_host) {
            if (out_scheme && *out_scheme) AGENTOS_FREE(*out_scheme);
            return AGENTOS_ENOMEM;
        }
        memcpy(*out_host, host_start, host_len);
        (*out_host)[host_len] = '\0';
    }
    
    if (out_port) *out_port = port;
    
    if (out_path) {
        size_t path_len = strlen(path_start);
        *out_path = (char*)AGENTOS_MALLOC(path_len + 1);
        if (!*out_path) {
            if (out_scheme && *out_scheme) AGENTOS_FREE(*out_scheme);
            if (out_host && *out_host) AGENTOS_FREE(*out_host);
            return AGENTOS_ENOMEM;
        }
        memcpy(*out_path, path_start, path_len);
        (*out_path)[path_len] = '\0';
    }
    
    return AGENTOS_SUCCESS;
}

/* ==================== 数值验证实�?==================== */

/**
 * @brief 验证整数范围
 */
void agentos_validate_int_range(
    int64_t value,
    int64_t min_val,
    int64_t max_val,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (value < min_val) {
        result->error_message = "Value below minimum";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "value";
        return;
    }
    
    if (value > max_val) {
        result->error_message = "Value above maximum";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "value";
        return;
    }
    
    result->is_valid = 1;
}

/**
 * @brief 验证浮点数范�?
 */
void agentos_validate_float_range(
    double value,
    double min_val,
    double max_val,
    agentos_validation_result_t* result) {
    
    if (!result) return;
    
    result->is_valid = 0;
    result->error_message = NULL;
    result->error_code = AGENTOS_SUCCESS;
    result->error_field = NULL;
    
    if (value < min_val) {
        result->error_message = "Value below minimum";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "value";
        return;
    }
    
    if (value > max_val) {
        result->error_message = "Value above maximum";
        result->error_code = AGENTOS_EINVAL;
        result->error_field = "value";
        return;
    }
    
    result->is_valid = 1;
}

/* ==================== 缓冲区验证实�?==================== */

/**
 * @brief 安全内存复制
 */
agentos_error_t agentos_safe_memcpy(
    void* dest,
    size_t dest_size,
    const void* src,
    size_t src_size) {
    
    if (!dest || !src) return AGENTOS_EINVAL;
    if (src_size > dest_size) return AGENTOS_EOVERFLOW;
    
    memcpy(dest, src, src_size);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 安全字符串复�?
 */
agentos_error_t agentos_safe_strcpy(
    char* dest,
    size_t dest_size,
    const char* src) {
    
    if (!dest || !src) return AGENTOS_EINVAL;
    if (dest_size == 0) return AGENTOS_EOVERFLOW;
    
    size_t src_len = strlen(src);
    if (src_len >= dest_size) return AGENTOS_EOVERFLOW;
    
    memcpy(dest, src, src_len + 1);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 安全字符串拼�?
 */
agentos_error_t agentos_safe_strcat(
    char* dest,
    size_t dest_size,
    const char* src) {
    
    if (!dest || !src) return AGENTOS_EINVAL;
    if (dest_size == 0) return AGENTOS_EOVERFLOW;
    
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);
    
    if (dest_len + src_len >= dest_size) return AGENTOS_EOVERFLOW;
    
    memcpy(dest + dest_len, src, src_len + 1);
    return AGENTOS_SUCCESS;
}
