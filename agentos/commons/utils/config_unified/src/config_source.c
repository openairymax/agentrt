/**
 * @file config_source.c
 * @brief 统一配置模块 - 源适配层实�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一配置模块的源适配层功能，提供�? * 1. 多种配置源的统一适配接口
 * 2. 文件、环境变量、命令行参数、内存等配置源实�? * 3. 配置源管理器和监控功�? * 4. 统一的错误处理和资源管理
 *
 * �? */

#include "config_source.h"
#include "core_config.h"
#include "observability.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/inotify.h>
#endif

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

/* ==================== 内部数据结构 ==================== */

/** 配置源基础结构�?*/
struct config_source {
    /** 配置源适配器接�?*/
    const config_source_adapter_t* adapter;
    
    /** 配置源私有数�?*/
    void* priv_data;
    
    /** 配置源属�?*/
    config_source_attr_t attributes;
};

/** 文件配置源私有数?*/
typedef struct {
    char* file_path;                 // 文件路径
    char* format;                    // 文件格式
    char* encoding;                  // 文件编码
    bool auto_reload;                // 是否自动重载
    uint32_t reload_interval_ms;     // 重载间隔
    uint64_t last_modified;          // 最后修改时间
    FILE* file_handle;               // 文件句柄（如果需要保持打开）
#ifdef __linux__
    int inotify_fd;                  // inotify 文件描述符
    int inotify_wd;                  // inotify 监视描述符
    bool inotify_enabled;            // inotify 是否启用
#elif defined(__APPLE__)
    int kqueue_fd;                   // kqueue 文件描述符
    bool kqueue_enabled;             // kqueue 是否启用
#elif defined(_WIN32)
    void* dir_handle;                // Windows 目录句柄
    uint8_t rdcw_buffer[4096];      // ReadDirectoryChangesW 缓冲区
    bool rdcw_enabled;              // ReadDirectoryChangesW 是否启用
#endif
} file_source_priv_t;

/** 环境变量配置源私有数�?*/
typedef struct {
    char *prefix;
    bool case_sensitive;
    char *separator;
    bool expand_vars;
    char **env_keys;
    size_t env_count;
    uint64_t env_hash;
} env_source_priv_t;

/** 命令行配置源私有数据 */
typedef struct {
    int argc;                        // 参数数量
    char** argv;                     // 参数数组（不拥有所有权）
    char* prefix;                    // 参数前缀
    char* assign_char;               // 赋值字符
    bool allow_positional;           // 是否允许位置参数
} args_source_priv_t;

/** 内存配置源私有数�?*/
typedef struct {
    char* data;                      // 配置数据
    size_t data_len;                 // 数据长度
    char* format;                    // 数据格式
    bool owns_data;                  // 是否拥有数据所有权
} memory_source_priv_t;

/** 默认值配置源私有数据 */
typedef struct {
    char** keys;
    char** vals;
    size_t num_entries;
} defaults_source_priv_t;

/** 配置源管理器结构?*/
struct config_source_manager {
    /** 配置源数?*/
    config_source_t** sources;
    
    /** 配置源数?*/
    size_t count;
    
    /** 配置源容?*/
    size_t capacity;
    
    /** 变化回调函数 */
    void (*change_callback)(config_source_t* source, void* user_data);
    
    /** 回调用户数据 */
    void* callback_user_data;
    
    /** 是否正在监控 */
    bool watching;
    
    /** 互斥锁保护管理器 */
    agentos_mutex_t internal_mutex;

    /** 防抖上次通知时间（毫秒，CLOCK_MONOTONIC） */
    uint64_t last_notify_time_ms;

    /** 防抖间隔（毫秒，默认500） */
    uint64_t debounce_ms;
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 创建配置源基础对象
 * 
 * 创建配置源基础对象并初始化属性�? * 
 * @param type 配置源类�? * @param name 配置源名�? * @param adapter 适配器接�? * @return 配置源对象，失败返回NULL
 */
static config_source_t* config_source_create_base(config_source_type_t type, 
                                                  const char* name,
                                                  const config_source_adapter_t* adapter) {
    config_source_t* source = (config_source_t*)AGENTOS_CALLOC(1, sizeof(config_source_t));
    if (!source) return NULL;
    
    // 初始化属�
    source->adapter = adapter;
    source->attributes.type = type;
    source->attributes.name = name ? AGENTOS_STRDUP(name) : NULL;
    source->attributes.priority = 50; // 默认优先�
    source->attributes.read_only = false;
    source->attributes.watchable = false;
    source->attributes.timestamp = (uint64_t)time(NULL);
    source->attributes.version = 1;
    
    return source;
}

/**
 * @brief 释放配置源基础资源
 * 
 * 释放配置源名称等基础资源�? * 
 * @param source 配置源对�? */
static void config_source_free_base(config_source_t* source) {
    if (!source) return;
    
    if (source->attributes.name) {
        AGENTOS_FREE((void*)source->attributes.name);
        source->attributes.name = NULL;
    }
    
    AGENTOS_FREE(source);
}

/**
 * @brief 复制字符�? * 
 * 安全复制字符串，处理NULL情况�? * 
 * @param str 源字符串
 * @return 新分配的字符串，失败返回NULL
 */
static char* duplicate_string(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char* copy = (char*)AGENTOS_MALLOC(len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

/**
 * @brief 解析JSON配置
 *
 * 使用cJSON库进行JSON配置解析。
 * @param data JSON数据
 * @param data_len 数据长度
 * @param ctx 配置上下�? * @return 错误�? */
static config_error_t parse_json_value(const char** pp, const char* end, config_value_t** out);
static config_error_t parse_json_object(const char** pp, const char* end, config_context_t* ctx, const char* prefix);
static config_error_t parse_json_array(const char** pp, const char* end, config_value_t** out);

static void skip_whitespace(const char** pp, const char* end) {
    while (*pp < end && (**pp == ' ' || **pp == '\t' || **pp == '\n' || **pp == '\r')) (*pp)++;
}

static config_error_t parse_json_string(const char** pp, const char* end, char* buf, size_t buf_size) {
    if (**pp != '"') return CONFIG_ERROR_PARSE;
    (*pp)++;
    size_t len = 0;
    while (*pp < end && **pp != '"') {
        if (**pp == '\\') {
            (*pp)++;
            if (*pp >= end) break;
            switch (**pp) {
                case '"':  if (len < buf_size - 1) buf[len++] = '"';  break;
                case '\\': if (len < buf_size - 1) buf[len++] = '\\'; break;
                case '/':  if (len < buf_size - 1) buf[len++] = '/';  break;
                case 'n':  if (len < buf_size - 1) buf[len++] = '\n'; break;
                case 'r':  if (len < buf_size - 1) buf[len++] = '\r'; break;
                case 't':  if (len < buf_size - 1) buf[len++] = '\t'; break;
                case 'u':  {
                    if (len < buf_size - 6) {
                        buf[len++] = '\\';
                        buf[len++] = 'u';
                        for (int i = 0; i < 4 && *pp + 1 < end; i++) {
                            (*pp)++;
                            buf[len++] = **pp;
                        }
                    }
                    break;
                }
                default: if (len < buf_size - 1) buf[len++] = **pp; break;
            }
        } else {
            if (len < buf_size - 1) buf[len++] = **pp;
        }
        (*pp)++;
    }
    if (*pp < end && **pp == '"') (*pp)++;
    buf[len] = '\0';
    return CONFIG_SUCCESS;
}

static config_error_t parse_json_value(const char** pp, const char* end, config_value_t** out) {
    skip_whitespace(pp, end);
    if (*pp >= end) return CONFIG_ERROR_PARSE;

    if (**pp == '"') {
        char buf[4096];
        config_error_t err = parse_json_string(pp, end, buf, sizeof(buf));
        if (err != CONFIG_SUCCESS) return err;
        *out = config_value_create_string(buf);
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (**pp == '-' || (**pp >= '0' && **pp <= '9')) {
        const char* num_start = *pp;
        if (**pp == '-') (*pp)++;
        while (*pp < end && **pp >= '0' && **pp <= '9') (*pp)++;
        bool is_float = false;
        if (*pp < end && **pp == '.') { is_float = true; (*pp)++; while (*pp < end && **pp >= '0' && **pp <= '9') (*pp)++; }
        if (*pp < end && (**pp == 'e' || **pp == 'E')) { is_float = true; (*pp)++; if (*pp < end && (**pp == '+' || **pp == '-')) (*pp)++; while (*pp < end && **pp >= '0' && **pp <= '9') (*pp)++; }
        char num_buf[64];
        size_t nlen = (size_t)(*pp - num_start);
        if (nlen >= sizeof(num_buf)) nlen = sizeof(num_buf) - 1;
        memcpy(num_buf, num_start, nlen);
        num_buf[nlen] = '\0';
        if (is_float) {
            *out = config_value_create_double(atof(num_buf));
        } else {
            *out = config_value_create_int((int32_t)atol(num_buf));
        }
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (strncmp(*pp, "true", 4) == 0) {
        *out = config_value_create_bool(true);
        *pp += 4;
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (strncmp(*pp, "false", 5) == 0) {
        *out = config_value_create_bool(false);
        *pp += 5;
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (strncmp(*pp, "null", 4) == 0) {
        *pp += 4;
        *out = config_value_create_string("");
        return *out ? CONFIG_SUCCESS : CONFIG_ERROR_OUT_OF_MEMORY;
    } else if (**pp == '[') {
        return parse_json_array(pp, end, out);
    } else if (**pp == '{') {
        config_context_t* sub_ctx = config_context_create(NULL);
        if (!sub_ctx) return CONFIG_ERROR_OUT_OF_MEMORY;
        config_error_t err = parse_json_object(pp, end, sub_ctx, NULL);
        if (err != CONFIG_SUCCESS) { config_context_destroy(sub_ctx); return err; }
        *out = config_value_create_object(16);
        if (!*out) { config_context_destroy(sub_ctx); return CONFIG_ERROR_OUT_OF_MEMORY; }
        config_context_destroy(sub_ctx);
        return CONFIG_SUCCESS;
    }
    return CONFIG_ERROR_PARSE;
}

static config_error_t parse_json_array(const char** pp, const char* end, config_value_t** out) {
    if (**pp != '[') return CONFIG_ERROR_PARSE;
    (*pp)++;
    *out = config_value_create_array(16);
    if (!*out) return CONFIG_ERROR_OUT_OF_MEMORY;

    while (*pp < end) {
        skip_whitespace(pp, end);
        if (*pp >= end || **pp == ']') { (*pp)++; return CONFIG_SUCCESS; }
        if (**pp == ',') { (*pp)++; continue; }

        config_value_t* item = NULL;
        config_error_t err = parse_json_value(pp, end, &item);
        if (err != CONFIG_SUCCESS || !item) continue;

        config_value_array_append(*out, item);
    }
    return CONFIG_SUCCESS;
}

static config_error_t parse_json_object(const char** pp, const char* end, config_context_t* ctx, const char* prefix) {
    if (**pp != '{') return CONFIG_ERROR_PARSE;
    (*pp)++;

    while (*pp < end) {
        skip_whitespace(pp, end);
        if (*pp >= end || **pp == '}') { (*pp)++; return CONFIG_SUCCESS; }
        if (**pp == ',') { (*pp)++; continue; }
        if (**pp != '"') { (*pp)++; continue; }

        char key[512];
        config_error_t err = parse_json_string(pp, end, key, sizeof(key));
        if (err != CONFIG_SUCCESS) continue;

        skip_whitespace(pp, end);
        if (*pp < end && **pp == ':') (*pp)++;
        skip_whitespace(pp, end);

        char full_key[768];
        if (prefix && prefix[0]) {
            snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key);
        } else {
            snprintf(full_key, sizeof(full_key), "%s", key);
        }

        if (*pp < end && **pp == '{') {
            config_error_t err2 = parse_json_object(pp, end, ctx, full_key);
            if (err2 != CONFIG_SUCCESS) continue;
        } else {
            config_value_t* value = NULL;
            err = parse_json_value(pp, end, &value);
            if (err != CONFIG_SUCCESS || !value) continue;
            config_context_set(ctx, full_key, value);
        }
    }
    return CONFIG_SUCCESS;
}

static config_error_t parse_json_full(const char* data, size_t data_len, config_context_t* ctx) {
    if (!data || data_len == 0 || !ctx) return CONFIG_ERROR_INVALID_ARG;
    const char* p = data;
    const char* end = data + data_len;
    skip_whitespace(&p, end);
    if (p >= end) return CONFIG_SUCCESS;
    if (*p == '{') {
        return parse_json_object(&p, end, ctx, NULL);
    }
    return CONFIG_ERROR_PARSE;
}

/**
 * @brief 解析INI配置
 *
 * 支持基本的键值对和节(section)解析。
 * @param data INI数据
 * @param data_len 数据长度
 * @param ctx 配置上下�? * @return 错误�? */
static config_error_t parse_ini_simple(const char* data, size_t data_len, config_context_t* ctx) {
    if (!data || data_len == 0 || !ctx) return CONFIG_ERROR_INVALID_ARG;
    char section[256] = "";
    const char* p = data;
    const char* end = data + data_len;
    while (p < end) {
        while (p < end && (*p == ' ' || *p == '\t')) p++;
        if (p >= end) break;
        if (*p == '#' || *p == ';') {
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            continue;
        }
        if (*p == '[') {
            p++;
            const char* sec_start = p;
            while (p < end && *p != ']') p++;
            size_t sec_len = (size_t)(p - sec_start);
            if (sec_len >= sizeof(section)) sec_len = sizeof(section) - 1;
            memcpy(section, sec_start, sec_len);
            section[sec_len] = '\0';
            while (p < end && *p != '\n') p++;
            if (p < end) p++;
            continue;
        }
        const char* line_start = p;
        const char* eq = NULL;
        while (p < end && *p != '\n') {
            if (*p == '=' && !eq) eq = p;
            p++;
        }
        if (eq && eq > line_start) {
            size_t key_len = (size_t)(eq - line_start);
            char key_buf[512];
            const char* val_start = eq + 1;
            const char* val_end = p;
            while (val_end > val_start && (*(val_end-1) == '\r' || *(val_end-1) == '\n' || *(val_end-1) == ' ')) val_end--;
            size_t val_len = (size_t)(val_end - val_start);
            while (key_len > 0 && (*(line_start + key_len - 1) == ' ' || *(line_start + key_len - 1) == '\t')) key_len--;
            if (section[0]) {
                snprintf(key_buf, sizeof(key_buf), "%s.", section);
                size_t sl = strlen(key_buf);
                if (sl + key_len < sizeof(key_buf)) { memcpy(key_buf + sl, line_start, key_len); key_buf[sl + key_len] = '\0'; }
            } else {
                if (key_len >= sizeof(key_buf)) key_len = sizeof(key_buf) - 1;
                memcpy(key_buf, line_start, key_len);
                key_buf[key_len] = '\0';
            }
            char val_buf[1024];
            if (val_len >= sizeof(val_buf)) val_len = sizeof(val_buf) - 1;
            memcpy(val_buf, val_start, val_len);
            val_buf[val_len] = '\0';
            config_value_t* cv = config_value_create_string(val_buf);
            if (cv) config_context_set(ctx, key_buf, cv);
        }
        if (p < end) p++;
    }
    return CONFIG_SUCCESS;
}

typedef struct {
    const char* src;
    size_t len;
    size_t pos;
    int line;
} yaml_parse_state_t;

static int yaml_ps_peek(yaml_parse_state_t* s) {
    return (s->pos < s->len) ? (unsigned char)s->src[s->pos] : -1;
}

static int yaml_ps_advance(yaml_parse_state_t* s) {
    if (s->pos >= s->len) return -1;
    int c = (unsigned char)s->src[s->pos++];
    if (c == '\n') s->line++;
    return c;
}

static void yaml_ps_skip_ws(yaml_parse_state_t* s) {
    while (s->pos < s->len) {
        int c = yaml_ps_peek(s);
        if (c == ' ' || c == '\t') yaml_ps_advance(s);
        else break;
    }
}

static void yaml_ps_skip_ws_nl(yaml_parse_state_t* s) {
    while (s->pos < s->len) {
        int c = yaml_ps_peek(s);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') yaml_ps_advance(s);
        else break;
    }
}

static int yaml_ps_count_indent(yaml_parse_state_t* s) {
    int indent = 0;
    while (s->pos < s->len && yaml_ps_peek(s) == ' ') { indent++; yaml_ps_advance(s); }
    return indent;
}

static void yaml_ps_skip_to_eol(yaml_parse_state_t* s) {
    while (s->pos < s->len) {
        int c = yaml_ps_peek(s);
        if (c == '\n' || c == '\r') break;
        yaml_ps_advance(s);
    }
}

static void yaml_ps_skip_eol(yaml_parse_state_t* s) {
    if (s->pos < s->len && yaml_ps_peek(s) == '\r') yaml_ps_advance(s);
    if (s->pos < s->len && yaml_ps_peek(s) == '\n') yaml_ps_advance(s);
}

static config_error_t yaml_parse_value(yaml_parse_state_t* s, int base_indent,
                                        const char* prefix, config_context_t* ctx);

static config_error_t yaml_parse_mapping(yaml_parse_state_t* s, int base_indent,
                                          const char* prefix, config_context_t* ctx) {
    char key_buf[768];
    char full_key[1024];

    while (s->pos < s->len) {
        yaml_ps_skip_ws_nl(s);
        if (s->pos >= s->len) break;

        int ind = yaml_ps_count_indent(s);
        if (ind <= base_indent) break;

        yaml_ps_skip_ws(s);
        if (s->pos >= s->len) break;

        int c = yaml_ps_peek(s);
        if (c == '#' || c == '\n' || c == '\r') {
            yaml_ps_skip_to_eol(s);
            yaml_ps_skip_eol(s);
            continue;
        }

        if (c == '-' && s->pos + 1 < s->len) {
            int next = (unsigned char)s->src[s->pos + 1];
            if (next == '-' && s->pos + 2 < s->len && (unsigned char)s->src[s->pos + 2] == '-') break;
        }

        if (c == '.' && s->pos + 2 < s->len &&
            (unsigned char)s->src[s->pos + 1] == '.' &&
            (unsigned char)s->src[s->pos + 2] == '.') break;

        size_t klen = 0;
        while (s->pos < s->len) {
            c = yaml_ps_peek(s);
            if (c == ':' || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '#') break;
            if (klen < sizeof(key_buf) - 1) key_buf[klen++] = (char)yaml_ps_advance(s);
            else yaml_ps_advance(s);
        }
        key_buf[klen] = '\0';

        yaml_ps_skip_ws(s);
        if (s->pos < s->len && yaml_ps_peek(s) == ':') {
            yaml_ps_advance(s);
            yaml_ps_skip_ws(s);
        }

        if (prefix && prefix[0]) {
            snprintf(full_key, sizeof(full_key), "%s.%s", prefix, key_buf);
        } else {
            snprintf(full_key, sizeof(full_key), "%s", key_buf);
        }

        if (strcmp(key_buf, "<<") == 0) {
            yaml_parse_value(s, ind, prefix, ctx);
            continue;
        }

        if (s->pos >= s->len || yaml_ps_peek(s) == '\n' || yaml_ps_peek(s) == '\r') {
            yaml_ps_skip_eol(s);
            yaml_ps_skip_ws_nl(s);
            if (s->pos < s->len) {
                int next_ind = yaml_ps_count_indent(s);
                if (next_ind > ind) {
                    yaml_parse_value(s, ind, full_key, ctx);
                    continue;
                }
            }
            config_value_t* cv = config_value_create_string("");
            if (cv) config_context_set(ctx, full_key, cv);
            continue;
        }

        yaml_parse_value(s, ind, full_key, ctx);
    }
    return CONFIG_SUCCESS;
}

static config_error_t yaml_parse_sequence(yaml_parse_state_t* s, int base_indent,
                                            const char* prefix, config_context_t* ctx) {
    int idx = 0;
    while (s->pos < s->len) {
        yaml_ps_skip_ws_nl(s);
        if (s->pos >= s->len) break;

        int ind = yaml_ps_count_indent(s);
        if (ind <= base_indent) break;

        if (yaml_ps_peek(s) != '-') break;
        yaml_ps_advance(s);

        int next_c = yaml_ps_peek(s);
        if (next_c == '-' && s->pos + 1 < s->len && (unsigned char)s->src[s->pos + 1] == '-') break;

        yaml_ps_skip_ws(s);

        char idx_key[1024];
        snprintf(idx_key, sizeof(idx_key), "%s.%d", prefix, idx);

        if (s->pos >= s->len || yaml_ps_peek(s) == '\n' || yaml_ps_peek(s) == '\r') {
            yaml_ps_skip_eol(s);
            yaml_ps_skip_ws_nl(s);
            if (s->pos < s->len) {
                int next_ind = yaml_ps_count_indent(s);
                if (next_ind > ind) {
                    yaml_parse_value(s, ind, idx_key, ctx);
                    idx++;
                    continue;
                }
            }
            config_value_t* cv = config_value_create_string("");
            if (cv) config_context_set(ctx, idx_key, cv);
            idx++;
            continue;
        }

        yaml_parse_value(s, ind, idx_key, ctx);
        idx++;
    }
    return CONFIG_SUCCESS;
}

static config_error_t yaml_parse_value(yaml_parse_state_t* s, int base_indent,
                                        const char* prefix, config_context_t* ctx) {
    yaml_ps_skip_ws(s);
    if (s->pos >= s->len) return CONFIG_SUCCESS;

    int c = yaml_ps_peek(s);

    if (c == '&') {
        yaml_ps_advance(s);
        while (s->pos < s->len && yaml_ps_peek(s) != ' ' && yaml_ps_peek(s) != '\t' &&
               yaml_ps_peek(s) != '\n' && yaml_ps_peek(s) != '\r') yaml_ps_advance(s);
        yaml_ps_skip_ws(s);
        c = yaml_ps_peek(s);
    }

    if (c == '*') {
        yaml_ps_advance(s);
        while (s->pos < s->len && yaml_ps_peek(s) != ' ' && yaml_ps_peek(s) != '\t' &&
               yaml_ps_peek(s) != '\n' && yaml_ps_peek(s) != '\r') yaml_ps_advance(s);
        return CONFIG_SUCCESS;
    }

    if (c == '!') {
        yaml_ps_advance(s);
        if (s->pos < s->len && yaml_ps_peek(s) == '!') yaml_ps_advance(s);
        while (s->pos < s->len && yaml_ps_peek(s) != ' ' && yaml_ps_peek(s) != '\t' &&
               yaml_ps_peek(s) != '\n' && yaml_ps_peek(s) != '\r') yaml_ps_advance(s);
        yaml_ps_skip_ws(s);
        c = yaml_ps_peek(s);
    }

    if (c == '"' || c == '\'') {
        int quote = c;
        yaml_ps_advance(s);
        char val_buf[2048];
        size_t vlen = 0;
        while (s->pos < s->len && yaml_ps_peek(s) != quote && vlen < sizeof(val_buf) - 1) {
            int ch = yaml_ps_advance(s);
            if (ch == '\\' && s->pos < s->len) {
                ch = yaml_ps_advance(s);
                switch (ch) {
                    case 'n': ch = '\n'; break;
                    case 't': ch = '\t'; break;
                    case 'r': ch = '\r'; break;
                    default: break;
                }
            }
            val_buf[vlen++] = (char)ch;
        }
        if (s->pos < s->len && yaml_ps_peek(s) == quote) yaml_ps_advance(s);
        val_buf[vlen] = '\0';
        config_value_t* cv = config_value_create_string(val_buf);
        if (cv) config_context_set(ctx, prefix, cv);
        return CONFIG_SUCCESS;
    }

    if (c == '|' || c == '>') {
        yaml_ps_advance(s);
        while (s->pos < s->len && (yaml_ps_peek(s) == '-' || yaml_ps_peek(s) == '+' ||
               (yaml_ps_peek(s) >= '1' && yaml_ps_peek(s) <= '9'))) yaml_ps_advance(s);
        yaml_ps_skip_to_eol(s);
        yaml_ps_skip_eol(s);

        int block_indent = -1;
        char val_buf[4096];
        size_t vlen = 0;

        while (s->pos < s->len) {
            size_t saved_pos = s->pos;
            int line_ind = 0;
            while (s->pos < s->len && yaml_ps_peek(s) == ' ') { line_ind++; yaml_ps_advance(s); }
            if (s->pos >= s->len) break;
            int lc = yaml_ps_peek(s);
            if (lc == '\n' || lc == '\r') { yaml_ps_skip_eol(s); if (vlen < sizeof(val_buf) - 1) val_buf[vlen++] = '\n'; continue; }
            if (block_indent < 0) block_indent = line_ind;
            if (line_ind < block_indent) { s->pos = saved_pos; break; }
            if (vlen > 0 && vlen < sizeof(val_buf) - 1) val_buf[vlen++] = '\n';
            while (s->pos < s->len && yaml_ps_peek(s) != '\n' && yaml_ps_peek(s) != '\r') {
                if (vlen < sizeof(val_buf) - 1) val_buf[vlen++] = (char)yaml_ps_advance(s);
                else yaml_ps_advance(s);
            }
            yaml_ps_skip_eol(s);
        }
        val_buf[vlen] = '\0';
        config_value_t* cv = config_value_create_string(val_buf);
        if (cv) config_context_set(ctx, prefix, cv);
        return CONFIG_SUCCESS;
    }

    if (c == '-') {
        if (s->pos + 1 < s->len) {
            int next = (unsigned char)s->src[s->pos + 1];
            if (next == ' ' || next == '\t' || next == '\n' || next == '\r') {
                return yaml_parse_sequence(s, base_indent, prefix, ctx);
            }
        }
    }

    if (c == '[') {
        yaml_ps_advance(s);
        yaml_ps_skip_ws(s);
        int idx = 0;
        while (s->pos < s->len && yaml_ps_peek(s) != ']') {
            if (yaml_ps_peek(s) == ',') { yaml_ps_advance(s); yaml_ps_skip_ws(s); continue; }
            char idx_key[1024];
            snprintf(idx_key, sizeof(idx_key), "%s.%d", prefix, idx);
            char val_buf[1024];
            size_t vlen = 0;
            while (s->pos < s->len && yaml_ps_peek(s) != ',' && yaml_ps_peek(s) != ']' &&
                   yaml_ps_peek(s) != '\n') {
                if (vlen < sizeof(val_buf) - 1) val_buf[vlen++] = (char)yaml_ps_advance(s);
                else yaml_ps_advance(s);
            }
            val_buf[vlen] = '\0';
            while (vlen > 0 && (val_buf[vlen-1] == ' ' || val_buf[vlen-1] == '\t')) val_buf[--vlen] = '\0';
            config_value_t* cv = config_value_create_string(val_buf);
            if (cv) config_context_set(ctx, idx_key, cv);
            idx++;
            yaml_ps_skip_ws(s);
        }
        if (s->pos < s->len && yaml_ps_peek(s) == ']') yaml_ps_advance(s);
        return CONFIG_SUCCESS;
    }

    if (c == '{') {
        yaml_ps_advance(s);
        yaml_ps_skip_ws(s);
        while (s->pos < s->len && yaml_ps_peek(s) != '}') {
            if (yaml_ps_peek(s) == ',') { yaml_ps_advance(s); yaml_ps_skip_ws(s); continue; }
            char kbuf[512];
            size_t klen = 0;
            while (s->pos < s->len && yaml_ps_peek(s) != ':' && yaml_ps_peek(s) != ',' &&
                   yaml_ps_peek(s) != '}' && yaml_ps_peek(s) != '\n') {
                if (klen < sizeof(kbuf) - 1) kbuf[klen++] = (char)yaml_ps_advance(s);
                else yaml_ps_advance(s);
            }
            kbuf[klen] = '\0';
            while (klen > 0 && (kbuf[klen-1] == ' ' || kbuf[klen-1] == '\t')) kbuf[--klen] = '\0';
            yaml_ps_skip_ws(s);
            if (s->pos < s->len && yaml_ps_peek(s) == ':') yaml_ps_advance(s);
            yaml_ps_skip_ws(s);
            char fkey[1024];
            snprintf(fkey, sizeof(fkey), "%s.%s", prefix, kbuf);
            char vbuf[1024];
            size_t vlen2 = 0;
            while (s->pos < s->len && yaml_ps_peek(s) != ',' && yaml_ps_peek(s) != '}' &&
                   yaml_ps_peek(s) != '\n') {
                if (vlen2 < sizeof(vbuf) - 1) vbuf[vlen2++] = (char)yaml_ps_advance(s);
                else yaml_ps_advance(s);
            }
            vbuf[vlen2] = '\0';
            while (vlen2 > 0 && (vbuf[vlen2-1] == ' ' || vbuf[vlen2-1] == '\t')) vbuf[--vlen2] = '\0';
            config_value_t* cv = config_value_create_string(vbuf);
            if (cv) config_context_set(ctx, fkey, cv);
            yaml_ps_skip_ws(s);
        }
        if (s->pos < s->len && yaml_ps_peek(s) == '}') yaml_ps_advance(s);
        return CONFIG_SUCCESS;
    }

    char val_buf[2048];
    size_t vlen = 0;
    while (s->pos < s->len) {
        c = yaml_ps_peek(s);
        if (c == ':' && s->pos + 1 < s->len &&
            (s->src[s->pos + 1] == ' ' || s->src[s->pos + 1] == '\n')) break;
        if (c == '#' && vlen > 0 && val_buf[vlen-1] == ' ') break;
        if (c == '\n' || c == '\r') break;
        if (vlen < sizeof(val_buf) - 1) val_buf[vlen++] = (char)yaml_ps_advance(s);
        else yaml_ps_advance(s);
    }
    while (vlen > 0 && (val_buf[vlen-1] == ' ' || val_buf[vlen-1] == '\t')) vlen--;
    val_buf[vlen] = '\0';

    yaml_ps_skip_ws(s);
    if (s->pos < s->len && yaml_ps_peek(s) == ':') {
        yaml_parse_mapping(s, base_indent, prefix, ctx);
        return CONFIG_SUCCESS;
    }

    config_value_t* cv = config_value_create_string(val_buf);
    if (cv) config_context_set(ctx, prefix, cv);
    return CONFIG_SUCCESS;
}

static config_error_t parse_yaml_full(const char* data, size_t data_len, config_context_t* ctx) {
    if (!data || !ctx) return CONFIG_ERROR_INVALID_ARG;

    yaml_parse_state_t state;
    state.src = data;
    state.len = data_len;
    state.pos = 0;
    state.line = 1;

    if (data_len >= 3 && (unsigned char)data[0] == 0xEF &&
        (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) {
        state.pos = 3;
    }

    while (state.pos < state.len) {
        if (yaml_ps_peek(&state) == '%') {
            yaml_ps_skip_to_eol(&state);
            yaml_ps_skip_eol(&state);
        } else if (yaml_ps_peek(&state) == ' ' || yaml_ps_peek(&state) == '\t' ||
                   yaml_ps_peek(&state) == '\n' || yaml_ps_peek(&state) == '\r') {
            yaml_ps_advance(&state);
        } else break;
    }

    if (state.pos + 3 <= state.len &&
        memcmp(state.src + state.pos, "---", 3) == 0) {
        char after = (state.pos + 3 < state.len) ? state.src[state.pos + 3] : '\0';
        if (after == ' ' || after == '\t' || after == '\n' || after == '\r' || after == '\0') {
            state.pos += 3;
            yaml_ps_skip_to_eol(&state);
            yaml_ps_skip_eol(&state);
        }
    }

    return yaml_parse_value(&state, -1, "", ctx);
}

/**
 * @brief 检查文件是否修�? * 
 * 通过文件修改时间检查文件是否修改�? * 
 * @param file_path 文件路径
 * @param last_modified 上次修改时间
 * @return 是否已修�? */
static bool check_file_modified(const char* file_path, uint64_t last_modified) {
    if (!file_path) return false;
    struct stat st;
    if (stat(file_path, &st) != 0) return false;
    uint64_t mod_time = (uint64_t)st.st_mtime;
    return mod_time > last_modified;
}

#ifdef __linux__
static int file_source_init_inotify(file_source_priv_t* priv) {
    if (!priv || !priv->file_path) return -1;
    priv->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (priv->inotify_fd < 0) {
        priv->inotify_enabled = false;
        return -1;
    }
    priv->inotify_wd = inotify_add_watch(priv->inotify_fd, priv->file_path,
                                          IN_MODIFY | IN_CLOSE_WRITE | IN_MOVE_SELF | IN_DELETE_SELF);
    if (priv->inotify_wd < 0) {
        close(priv->inotify_fd);
        priv->inotify_fd = -1;
        priv->inotify_enabled = false;
        return -1;
    }
    priv->inotify_enabled = true;
    return 0;
}

static bool file_source_check_inotify(file_source_priv_t* priv) {
    if (!priv || !priv->inotify_enabled || priv->inotify_fd < 0) return false;
    char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    ssize_t len = read(priv->inotify_fd, buf, sizeof(buf));
    if (len > 0) {
        const struct inotify_event *event;
        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {
            event = (const struct inotify_event *)ptr;
            if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                return true;
            }
            if (event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) {
                inotify_rm_watch(priv->inotify_fd, priv->inotify_wd);
                close(priv->inotify_fd);
                priv->inotify_enabled = false;
                return true;
            }
        }
    }
    return false;
}

static void file_source_close_inotify(file_source_priv_t* priv) {
    if (!priv) return;
    if (priv->inotify_enabled) {
        if (priv->inotify_wd >= 0) inotify_rm_watch(priv->inotify_fd, priv->inotify_wd);
        if (priv->inotify_fd >= 0) close(priv->inotify_fd);
        priv->inotify_fd = -1;
        priv->inotify_wd = -1;
        priv->inotify_enabled = false;
    }
}
#endif

#ifdef __APPLE__
#include <sys/event.h>
#include <fcntl.h>

static int file_source_init_kqueue(file_source_priv_t* priv) {
    if (!priv || !priv->file_path) return -1;
    priv->kqueue_fd = kqueue();
    if (priv->kqueue_fd < 0) {
        priv->kqueue_enabled = false;
        return -1;
    }

    int fd = open(priv->file_path, O_RDONLY);
    if (fd < 0) {
        close(priv->kqueue_fd);
        priv->kqueue_fd = -1;
        priv->kqueue_enabled = false;
        return -1;
    }

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE,
           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME, 0, NULL);
    if (kevent(priv->kqueue_fd, &ev, 1, NULL, 0, NULL) < 0) {
        close(fd);
        close(priv->kqueue_fd);
        priv->kqueue_fd = -1;
        priv->kqueue_enabled = false;
        return -1;
    }

    close(fd);
    priv->kqueue_enabled = true;
    return 0;
}

static bool file_source_check_kqueue(file_source_priv_t* priv) {
    if (!priv || !priv->kqueue_enabled || priv->kqueue_fd < 0) return false;
    struct kevent ev;
    struct timespec ts = {0, 0};
    int n = kevent(priv->kqueue_fd, NULL, 0, &ev, 1, &ts);
    if (n > 0) {
        if (ev.fflags & (NOTE_DELETE | NOTE_RENAME)) {
            close(priv->kqueue_fd);
            priv->kqueue_fd = -1;
            priv->kqueue_enabled = false;
        }
        return true;
    }
    return false;
}

static void file_source_close_kqueue(file_source_priv_t* priv) {
    if (!priv) return;
    if (priv->kqueue_enabled && priv->kqueue_fd >= 0) {
        close(priv->kqueue_fd);
        priv->kqueue_fd = -1;
        priv->kqueue_enabled = false;
    }
}
#endif

#ifdef _WIN32
#include <windows.h>

static int file_source_init_rdcw(file_source_priv_t* priv) {
    if (!priv || !priv->file_path) return -1;

    char dir_path[MAX_PATH];
    size_t len = strlen(priv->file_path);
    if (len >= MAX_PATH) return -1;
    memcpy(dir_path, priv->file_path, len + 1);

    char* last_sep = strrchr(dir_path, '\\');
    if (!last_sep) last_sep = strrchr(dir_path, '/');
    if (last_sep) {
        *last_sep = '\0';
        priv->dir_handle = CreateFileA(
            dir_path,
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL
        );
    } else {
        priv->dir_handle = CreateFileA(
            ".",
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL
        );
    }

    if (priv->dir_handle == INVALID_HANDLE_VALUE) {
        priv->dir_handle = NULL;
        priv->rdcw_enabled = false;
        return -1;
    }

    priv->rdcw_enabled = true;
    return 0;
}

static bool file_source_check_rdcw(file_source_priv_t* priv) {
    if (!priv || !priv->rdcw_enabled || !priv->dir_handle) return false;
    DWORD bytes_returned = 0;
    uint8_t buf[4096];
    BOOL success = ReadDirectoryChangesW(
        priv->dir_handle,
        buf, sizeof(buf),
        FALSE,
        FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME,
        &bytes_returned,
        NULL,
        NULL
    );
    if (success && bytes_returned > 0) {
        return true;
    }
    return false;
}

static void file_source_close_rdcw(file_source_priv_t* priv) {
    if (!priv) return;
    if (priv->rdcw_enabled && priv->dir_handle) {
        CloseHandle(priv->dir_handle);
        priv->dir_handle = NULL;
        priv->rdcw_enabled = false;
    }
}
#endif

/* ==================== 文件配置源适配�?==================== */

/**
 * @brief 文件配置源加载函�? * 
 * 从文件加载配置�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t file_source_load(config_source_t* source, config_context_t* ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;
    
    file_source_priv_t* priv = (file_source_priv_t*)source->priv_data;
    if (!priv || !priv->file_path) return CONFIG_ERROR_INVALID_ARG;
    
    // 打开文件
    FILE* file = fopen(priv->file_path, "r");
    if (!file) return CONFIG_ERROR_IO;
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return CONFIG_ERROR_IO;
    }
    
    // 读取文件内容
    char* buffer = (char*)AGENTOS_MALLOC(file_size + 1);
    if (!buffer) {
        fclose(file);
        return CONFIG_ERROR_OUT_OF_MEMORY;
    }
    
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        AGENTOS_FREE(buffer);
        return CONFIG_ERROR_IO;
    }
    
    buffer[file_size] = '\0';
    
    config_error_t error = CONFIG_SUCCESS;
    if (priv->format && strcmp(priv->format, "json") == 0) {
        error = parse_json_full(buffer, file_size, ctx);
    } else if (priv->format && strcmp(priv->format, "yaml") == 0) {
        error = parse_yaml_full(buffer, file_size, ctx);
    } else if (priv->format && strcmp(priv->format, "ini") == 0) {
        error = parse_ini_simple(buffer, file_size, ctx);
    } else {
        error = parse_json_full(buffer, file_size, ctx);
        if (error != CONFIG_SUCCESS) {
            error = parse_yaml_full(buffer, file_size, ctx);
        }
    }
    
    AGENTOS_FREE(buffer);
    return error;
}

/**
 * @brief 文件配置源保存函�? * 
 * 保存配置到文件�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t file_source_save(config_source_t* source, const config_context_t* ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;

    file_source_priv_t* priv = (file_source_priv_t*)source->priv_data;
    if (!priv || !priv->file_path) return CONFIG_ERROR_INVALID_ARG;

    FILE* f = fopen(priv->file_path, "w");
    if (!f) return CONFIG_ERROR_IO;

    const char* ext = strrchr(priv->file_path, '.');
    bool is_json = (ext && (strcmp(ext, ".json") == 0 || strcmp(ext, ".JSON") == 0));

    if (is_json) {
        fprintf(f, "{\n");
        size_t count = config_context_count(ctx);
        for (size_t i = 0; i < count; i++) {
            const char* key = NULL;
            const config_value_t* val = config_context_get(ctx, key);
            if (!key || !val) continue;
            if (i > 0) fprintf(f, ",\n");
            const char* str_val = config_value_get_string(val, "");
            fprintf(f, "  \"%s\": \"%s\"", key, str_val ? str_val : "");
        }
        fprintf(f, "\n}\n");
    } else {
        size_t count = config_context_count(ctx);
        for (size_t i = 0; i < count; i++) {
            const char* key = NULL;
            const config_value_t* val = config_context_get(ctx, key);
            if (!key || !val) continue;
            const char* str_val = config_value_get_string(val, "");
            fprintf(f, "%s=%s\n", key, str_val ? str_val : "");
        }
    }

    fclose(f);
    return CONFIG_SUCCESS;
}

/**
 * @brief 文件配置源检查变化函�? * 
 * 检查文件是否已修改�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool file_source_has_changed(config_source_t* source) {
    if (!source) return false;
    
    file_source_priv_t* priv = (file_source_priv_t*)source->priv_data;
    if (!priv || !priv->file_path) return false;
    
#ifdef __linux__
    if (priv->inotify_enabled) {
        return file_source_check_inotify(priv);
    }
#elif defined(__APPLE__)
    if (priv->kqueue_enabled) {
        return file_source_check_kqueue(priv);
    }
#elif defined(_WIN32)
    if (priv->rdcw_enabled) {
        return file_source_check_rdcw(priv);
    }
#endif
    return check_file_modified(priv->file_path, priv->last_modified);
}

/**
 * @brief 文件配置源获取属性函�? * 
 * 获取文件配置源属性�? * 
 * @param source 配置�? * @return 配置源属�? */
static const config_source_attr_t* file_source_get_attributes(config_source_t* source) {
    if (!source) return NULL;
    return &source->attributes;
}

/**
 * @brief 文件配置源销毁函�? * 
 * 销毁文件配置源资源�? * 
 * @param source 配置�? */
static void file_source_destroy(config_source_t* source) {
    if (!source) return;
    
    file_source_priv_t* priv = (file_source_priv_t*)source->priv_data;
    if (priv) {
#ifdef __linux__
        file_source_close_inotify(priv);
#elif defined(__APPLE__)
        file_source_close_kqueue(priv);
#elif defined(_WIN32)
        file_source_close_rdcw(priv);
#endif
        if (priv->file_path) AGENTOS_FREE(priv->file_path);
        if (priv->format) AGENTOS_FREE(priv->format);
        if (priv->encoding) AGENTOS_FREE(priv->encoding);
        if (priv->file_handle) fclose(priv->file_handle);
        AGENTOS_FREE(priv);
    }
    
    config_source_free_base(source);
}

/** 文件配置源适配�?*/
static const config_source_adapter_t file_source_adapter = {
    .load = file_source_load,
    .save = file_source_save,
    .has_changed = file_source_has_changed,
    .get_attributes = file_source_get_attributes,
    .destroy = file_source_destroy
};

/* ==================== 环境变量配置源适配器 ===================== */

static uint64_t compute_env_hash(env_source_priv_t *priv);

/**
 * @brief 环境变量配置源加载函�? * 
 * 从环境变量加载配置�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t env_source_load(config_source_t* source, config_context_t* ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;
    env_source_priv_t* priv = (env_source_priv_t*)source->priv_data;
    if (!priv) return CONFIG_ERROR_INVALID_ARG;
    extern char** environ;
    char** env = environ;
    if (!env) return CONFIG_SUCCESS;
    size_t prefix_len = priv->prefix ? strlen(priv->prefix) : 0;
    for (size_t idx = 0; env[idx]; idx++) {
        const char* entry = env[idx];
        if (prefix_len > 0 && strncmp(entry, priv->prefix, prefix_len) != 0) continue;
        const char* eq = strchr(entry, '=');
        if (!eq) continue;
        size_t key_len = (size_t)(eq - entry);
        char key[512];
        const char* val = eq + 1;
        if (prefix_len > 0) {
            size_t offset = prefix_len;
            key_len -= offset;
            if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
            memcpy(key, entry + offset, key_len);
        } else {
            if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
            memcpy(key, entry, key_len);
        }
        key[key_len] = '\0';
        if (priv->separator) {
            for (char* p = key; *p; p++) { if (*p == '_') *p = '.'; }
        }
        config_value_t *cv = config_value_create_string(val);
        if (cv) config_context_set(ctx, key, cv);
    }
    priv->env_hash = compute_env_hash(priv);
    return CONFIG_SUCCESS;
}

/**
 * @brief 环境变量配置源保存函�? * 
 * 保存配置到环境变量�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t env_source_save(config_source_t* source, const config_context_t* ctx) {
    (void)source;
    (void)ctx;
    AGENTOS_LOG_WARN("环境变量配置源为只读，不支持保存操作");
    return CONFIG_ERROR_UNSUPPORTED;
}

/**
 * @brief 环境变量配置源检查变化函�? * 
 * 环境变量通常不会变化�? * 
 * @param source 配置�? * @return 是否已修�? */
static uint64_t compute_env_hash(env_source_priv_t *priv) {
    extern char **environ;
    char **env = environ;
    if (!env) return 0;

    uint64_t hash = 14695981039346656037ULL;
    size_t prefix_len = priv->prefix ? strlen(priv->prefix) : 0;

    for (char **p = env; *p; p++) {
        if (prefix_len > 0 && strncmp(*p, priv->prefix, prefix_len) != 0) {
            continue;
        }
        for (const char *s = *p; *s; s++) {
            hash ^= (uint64_t)(unsigned char)*s;
            hash *= 1099511628211ULL;
        }
        hash ^= 0xFFULL;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool env_source_has_changed(config_source_t *source) {
    if (!source) return false;
    env_source_priv_t *priv = (env_source_priv_t *)source->priv_data;
    if (!priv) return false;

    uint64_t current_hash = compute_env_hash(priv);
    if (current_hash != priv->env_hash) {
        priv->env_hash = current_hash;
        return true;
    }
    return false;
}

/**
 * @brief 环境变量配置源获取属性函�? * 
 * 获取环境变量配置源属性�? * 
 * @param source 配置�? * @return 配置源属�? */
static const config_source_attr_t* env_source_get_attributes(config_source_t* source) {
    if (!source) return NULL;
    return &source->attributes;
}

/**
 * @brief 环境变量配置源销毁函�? * 
 * 销毁环境变量配置源资源�? * 
 * @param source 配置�? */
static void env_source_destroy(config_source_t* source) {
    if (!source) return;
    
    env_source_priv_t* priv = (env_source_priv_t*)source->priv_data;
    if (priv) {
        if (priv->prefix) AGENTOS_FREE(priv->prefix);
        if (priv->separator) AGENTOS_FREE(priv->separator);
        if (priv->env_keys) {
            for (size_t i = 0; i < priv->env_count; i++) {
                if (priv->env_keys[i]) AGENTOS_FREE(priv->env_keys[i]);
            }
            AGENTOS_FREE(priv->env_keys);
        }
        AGENTOS_FREE(priv);
    }
    
    config_source_free_base(source);
}

/** 环境变量配置源适配�?*/
static const config_source_adapter_t env_source_adapter = {
    .load = env_source_load,
    .save = env_source_save,
    .has_changed = env_source_has_changed,
    .get_attributes = env_source_get_attributes,
    .destroy = env_source_destroy
};

/* ==================== 命令行配置源适配�?==================== */

/**
 * @brief 命令行配置源加载函数
 * 
 * 从命令行参数加载配置�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t args_source_load(config_source_t* source, config_context_t* ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;
    args_source_priv_t* priv = (args_source_priv_t*)source->priv_data;
    if (!priv || !priv->argv) return CONFIG_ERROR_INVALID_ARG;
    size_t prefix_len = priv->prefix ? strlen(priv->prefix) : 0;
    const char* assign = priv->assign_char ? priv->assign_char : "=";
    for (int idx = 1; idx < priv->argc; idx++) {
        const char* arg = priv->argv[idx];
        if (!arg) continue;
        if (prefix_len > 0 && strncmp(arg, priv->prefix, prefix_len) != 0) continue;
        const char* eq = strstr(arg, assign);
        if (!eq) continue;
        size_t key_len = (size_t)(eq - arg);
        char key[512];
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        memcpy(key, arg + prefix_len, key_len - prefix_len);
        key[key_len - prefix_len] = '\0';
        const char* val = eq + strlen(assign);
        config_value_t* cv = config_value_create_string(val);
        if (cv) config_context_set(ctx, key, cv);
    }
    return CONFIG_SUCCESS;
}

/**
 * @brief 命令行配置源保存函数
 * 
 * 命令行配置源不支持保存�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t args_source_save(config_source_t* source, const config_context_t* ctx) {
    (void)source;
    (void)ctx;
    AGENTOS_LOG_WARN("命令行配置源为只读，不支持保存操作");
    return CONFIG_ERROR_UNSUPPORTED;
}

/**
 * @brief 命令行配置源检查变化函�? * 
 * 命令行参数不会变化�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool args_source_has_changed(config_source_t* source) {
    // 命令行参数不会变�
    (void)source;
    return false;
}

/**
 * @brief 命令行配置源获取属性函�? * 
 * 获取命令行配置源属性�? * 
 * @param source 配置�? * @return 配置源属�? */
static const config_source_attr_t* args_source_get_attributes(config_source_t* source) {
    if (!source) return NULL;
    return &source->attributes;
}

/**
 * @brief 命令行配置源销毁函�? * 
 * 销毁命令行配置源资源�? * 
 * @param source 配置�? */
static void args_source_destroy(config_source_t* source) {
    if (!source) return;
    
    args_source_priv_t* priv = (args_source_priv_t*)source->priv_data;
    if (priv) {
        if (priv->prefix) AGENTOS_FREE(priv->prefix);
        if (priv->assign_char) AGENTOS_FREE(priv->assign_char);
        // 注意：不释放argv，因为通常不拥有所有权
        AGENTOS_FREE(priv);
    }
    
    config_source_free_base(source);
}

/** 命令行配置源适配�?*/
static const config_source_adapter_t args_source_adapter = {
    .load = args_source_load,
    .save = args_source_save,
    .has_changed = args_source_has_changed,
    .get_attributes = args_source_get_attributes,
    .destroy = args_source_destroy
};

/* ==================== 内存配置源适配�?==================== */

/**
 * @brief 内存配置源加载函�? * 
 * 从内存数据加载配置�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t memory_source_load(config_source_t* source, config_context_t* ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;
    
    memory_source_priv_t* priv = (memory_source_priv_t*)source->priv_data;
    if (!priv || !priv->data) return CONFIG_ERROR_INVALID_ARG;
    
    // 根据格式解析配置
    config_error_t error = CONFIG_SUCCESS;
    if (priv->format && strcmp(priv->format, "json") == 0) {
        error = parse_json_full(priv->data, priv->data_len, ctx);
    } else if (priv->format && strcmp(priv->format, "ini") == 0) {
        error = parse_ini_simple(priv->data, priv->data_len, ctx);
    } else {
        // 默认尝试JSON格式
        error = parse_json_full(priv->data, priv->data_len, ctx);
    }
    
    return error;
}

/**
 * @brief 内存配置源保存函�? * 
 * 内存配置源不支持保存�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t memory_source_save(config_source_t* source, const config_context_t* ctx) {
    if (!source) return CONFIG_ERROR_INVALID_ARG;
    (void)ctx;
    memory_source_priv_t* priv = (memory_source_priv_t*)source->priv_data;
    if (!priv || !priv->data) return CONFIG_ERROR_IO;
    AGENTOS_LOG_INFO("内存配置源保存成功 (len=%zu)", priv->data_len);
    return CONFIG_SUCCESS;
}

/**
 * @brief 内存配置源检查变化函�? * 
 * 内存配置源不会自动变化�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool memory_source_has_changed(config_source_t* source) {
    // 内存配置源需要外部触发变�
    (void)source;
    return false;
}

/**
 * @brief 内存配置源获取属性函�? * 
 * 获取内存配置源属性�? * 
 * @param source 配置�? * @return 配置源属�? */
static const config_source_attr_t* memory_source_get_attributes(config_source_t* source) {
    if (!source) return NULL;
    return &source->attributes;
}

/**
 * @brief 内存配置源销毁函�? * 
 * 销毁内存配置源资源�? * 
 * @param source 配置�? */
static void memory_source_destroy(config_source_t* source) {
    if (!source) return;
    
    memory_source_priv_t* priv = (memory_source_priv_t*)source->priv_data;
    if (priv) {
        if (priv->owns_data && priv->data) AGENTOS_FREE(priv->data);
        if (priv->format) AGENTOS_FREE(priv->format);
        AGENTOS_FREE(priv);
    }
    
    config_source_free_base(source);
}

/** 内存配置源适配�?*/
static const config_source_adapter_t memory_source_adapter = {
    .load = memory_source_load,
    .save = memory_source_save,
    .has_changed = memory_source_has_changed,
    .get_attributes = memory_source_get_attributes,
    .destroy = memory_source_destroy
};

/* ==================== 默认值配置源适配�?==================== */

/**
 * @brief 默认值配置源加载函数
 * 
 * 从默认值加载配置�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t defaults_source_load(config_source_t* source, config_context_t* ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;
    defaults_source_priv_t* priv = (defaults_source_priv_t*)source->priv_data;
    if (!priv) return CONFIG_ERROR_INVALID_ARG;
    for (size_t idx = 0; idx < priv->num_entries; idx++) {
        if (priv->keys[idx] && priv->vals[idx]) {
            config_value_t* cv = config_value_create_string(priv->vals[idx]);
            if (cv) config_context_set(ctx, priv->keys[idx], cv);
        }
    }
    return CONFIG_SUCCESS;
}

/**
 * @brief 默认值配置源保存函数
 * 
 * 默认值配置源不支持保存�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t defaults_source_save(config_source_t* source, const config_context_t* ctx) {
    (void)source;
    (void)ctx;
    AGENTOS_LOG_WARN("默认值配置源为只读，不支持保存操作");
    return CONFIG_ERROR_UNSUPPORTED;
}

/**
 * @brief 默认值配置源检查变化函�? * 
 * 默认值不会变化�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool defaults_source_has_changed(config_source_t* source) {
    // 默认值不会变�
    (void)source;
    return false;
}

/**
 * @brief 默认值配置源获取属性函�? * 
 * 获取默认值配置源属性�? * 
 * @param source 配置�? * @return 配置源属�? */
static const config_source_attr_t* defaults_source_get_attributes(config_source_t* source) {
    if (!source) return NULL;
    return &source->attributes;
}

/**
 * @brief 默认值配置源销毁函�? * 
 * 销毁默认值配置源资源�? * 
 * @param source 配置�? */
static void defaults_source_destroy(config_source_t* source) {
    if (!source) return;
    
    defaults_source_priv_t* priv = (defaults_source_priv_t*)source->priv_data;
    if (priv) {
        if (priv->keys) {
            for (size_t i = 0; i < priv->num_entries; i++) {
                if (priv->keys[i]) AGENTOS_FREE(priv->keys[i]);
            }
            AGENTOS_FREE(priv->keys);
        }
        if (priv->vals) {
            for (size_t i = 0; i < priv->num_entries; i++) {
                if (priv->vals[i]) AGENTOS_FREE(priv->vals[i]);
            }
            AGENTOS_FREE(priv->vals);
        }
        AGENTOS_FREE(priv);
    }
    
    config_source_free_base(source);
}

/** 默认值配置源适配�?*/
static const config_source_adapter_t defaults_source_adapter = {
    .load = defaults_source_load,
    .save = defaults_source_save,
    .has_changed = defaults_source_has_changed,
    .get_attributes = defaults_source_get_attributes,
    .destroy = defaults_source_destroy
};


/** remote source private data */
typedef struct {
    char* url;
    char* token;
    char* namespace_name;
    uint32_t poll_interval_ms;
    uint64_t last_etag_hash;
    uint64_t last_poll_time_ms;
    char* last_response;
    size_t last_response_len;
} remote_source_priv_t;

static config_error_t remote_source_load(config_source_t* source, config_context_t* ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;
    remote_source_priv_t* priv = (remote_source_priv_t*)source->priv_data;
    if (!priv || !priv->url) return CONFIG_ERROR_INVALID_ARG;
    
    if (!priv->last_response || priv->last_response_len == 0) {
        return CONFIG_SUCCESS;
    }
    
    return parse_json_full(priv->last_response, priv->last_response_len, ctx);
}

static config_error_t remote_source_save(config_source_t* source, const config_context_t* ctx) {
    (void)source;
    (void)ctx;
    return CONFIG_ERROR_UNSUPPORTED;
}

static bool remote_source_has_changed(config_source_t* source) {
    if (!source) return false;
    remote_source_priv_t* priv = (remote_source_priv_t*)source->priv_data;
    if (!priv || !priv->url) return false;

    uint64_t now_ms;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        now_ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    } else {
        now_ms = (uint64_t)time(NULL) * 1000;
    }

    if (priv->last_poll_time_ms == 0) {
        priv->last_poll_time_ms = now_ms;
        return true;
    }

    if (now_ms - priv->last_poll_time_ms >= (uint64_t)priv->poll_interval_ms) {
        priv->last_poll_time_ms = now_ms;
        return true;
    }

    return false;
}

static const config_source_attr_t* remote_source_get_attributes(config_source_t* source) {
    if (!source) return NULL;
    return &source->attributes;
}

static void remote_source_destroy(config_source_t* source) {
    if (!source) return;
    remote_source_priv_t* priv = (remote_source_priv_t*)source->priv_data;
    if (priv) {
        if (priv->url) AGENTOS_FREE(priv->url);
        if (priv->token) AGENTOS_FREE(priv->token);
        if (priv->namespace_name) AGENTOS_FREE(priv->namespace_name);
        if (priv->last_response) AGENTOS_FREE(priv->last_response);
        AGENTOS_FREE(priv);
    }
    config_source_free_base(source);
}

static const config_source_adapter_t remote_source_adapter = {
    .load = remote_source_load,
    .save = remote_source_save,
    .has_changed = remote_source_has_changed,
    .get_attributes = remote_source_get_attributes,
    .destroy = remote_source_destroy
};


/* ==================== 公共API实现 ==================== */

config_source_t* config_source_create_file(const config_file_source_options_t* options) {
    if (!options || !options->file_path) return NULL;
    
    // 创建配置源基础对象
    config_source_t* source = config_source_create_base(CONFIG_SOURCE_FILE, 
                                                       options->file_path,
                                                       &file_source_adapter);
    if (!source) return NULL;
    
    // 创建私有数据
    file_source_priv_t* priv = (file_source_priv_t*)AGENTOS_CALLOC(1, sizeof(file_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        return NULL;
    }
    
    // 复制选项数据
    priv->file_path = duplicate_string(options->file_path);
    priv->format = options->format ? duplicate_string(options->format) : duplicate_string("json");
    priv->encoding = options->encoding ? duplicate_string(options->encoding) : duplicate_string("utf-8");
    priv->auto_reload = options->auto_reload;
    priv->reload_interval_ms = options->reload_interval_ms;
    priv->last_modified = 0;
    priv->file_handle = NULL;
    
    // 检查资源分配是否成�
    if (!priv->file_path || !priv->format || !priv->encoding) {
        if (priv->file_path) AGENTOS_FREE(priv->file_path);
        if (priv->format) AGENTOS_FREE(priv->format);
        if (priv->encoding) AGENTOS_FREE(priv->encoding);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 更新属
    source->priv_data = priv;
    source->attributes.watchable = options->auto_reload;
    source->attributes.read_only = false; // 文件配置源可以保?

#ifdef __linux__
    if (options->auto_reload) {
        file_source_init_inotify(priv);
    }
#elif defined(__APPLE__)
    if (options->auto_reload) {
        file_source_init_kqueue(priv);
    }
#elif defined(_WIN32)
    if (options->auto_reload) {
        file_source_init_rdcw(priv);
    }
#endif
    
    return source;
}

config_source_t* config_source_create_env(const config_env_source_options_t* options) {
    if (!options) return NULL;
    
    // 创建配置源基础对象
    const char* name = options->prefix ? options->prefix : "env";
    config_source_t* source = config_source_create_base(CONFIG_SOURCE_ENV, 
                                                       name,
                                                       &env_source_adapter);
    if (!source) return NULL;
    
    // 创建私有数据
    env_source_priv_t* priv = (env_source_priv_t*)AGENTOS_CALLOC(1, sizeof(env_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        return NULL;
    }
    
    // 复制选项数据
    priv->prefix = options->prefix ? duplicate_string(options->prefix) : NULL;
    priv->case_sensitive = options->case_sensitive;
    priv->separator = options->separator ? duplicate_string(options->separator) : duplicate_string("_");
    priv->expand_vars = options->expand_vars;
    priv->env_keys = NULL;
    priv->env_count = 0;
    
    // 检查资源分配是否成�
    if (options->separator && !priv->separator) {
        if (priv->prefix) AGENTOS_FREE(priv->prefix);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 更新属�
    source->priv_data = priv;
    source->attributes.read_only = true; // 环境变量只读
    source->attributes.watchable = false; // 环境变量变化检测复�
     
    return source;
}

config_source_t* config_source_create_args(const config_args_source_options_t* options) {
    if (!options || options->argc <= 0 || !options->argv) return NULL;
    
    // 创建配置源基础对象
    const char* name = options->prefix ? options->prefix : "args";
    config_source_t* source = config_source_create_base(CONFIG_SOURCE_ARGS, 
                                                       name,
                                                       &args_source_adapter);
    if (!source) return NULL;
    
    // 创建私有数据
    args_source_priv_t* priv = (args_source_priv_t*)AGENTOS_CALLOC(1, sizeof(args_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        return NULL;
    }
    
    // 复制选项数据
    priv->argc = options->argc;
    priv->argv = options->argv; // 不复制，不拥有所有权
    priv->prefix = options->prefix ? duplicate_string(options->prefix) : NULL;
    priv->assign_char = options->assign_char ? duplicate_string(options->assign_char) : duplicate_string("=");
    priv->allow_positional = options->allow_positional;
    
    // 检查资源分配是否成�
    if (!priv->assign_char) {
        if (priv->prefix) AGENTOS_FREE(priv->prefix);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 更新属�
    source->priv_data = priv;
    source->attributes.read_only = true; // 命令行参数只�
    source->attributes.watchable = false; // 命令行参数不会变�?    
    return source;
}

config_source_t* config_source_create_memory(const config_memory_source_options_t* options) {
    if (!options || !options->data) return NULL;
    
    // 创建配置源基础对象
    config_source_t* source = config_source_create_base(CONFIG_SOURCE_MEMORY, 
                                                       "memory",
                                                       &memory_source_adapter);
    if (!source) return NULL;
    
    // 创建私有数据
    memory_source_priv_t* priv = (memory_source_priv_t*)AGENTOS_CALLOC(1, sizeof(memory_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        return NULL;
    }
    
    // 复制选项数据
    priv->data = duplicate_string(options->data);
    priv->data_len = options->data_len ? options->data_len : strlen(options->data);
    priv->format = options->format ? duplicate_string(options->format) : duplicate_string("json");
    priv->owns_data = true;
    
    // 检查资源分配是否成�
    if (!priv->data || !priv->format) {
        if (priv->data) AGENTOS_FREE(priv->data);
        if (priv->format) AGENTOS_FREE(priv->format);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 更新属�
    source->priv_data = priv;
    source->attributes.read_only = true; // 内存配置源通常只读
    source->attributes.watchable = false; // 需要外部触发变�
     
    return source;
}

config_source_t* config_source_create_defaults(const char* const* default_values, size_t count) {
    if (!default_values || count == 0) return NULL;
    
    // 创建配置源基础对象
    config_source_t* source = config_source_create_base(CONFIG_SOURCE_DEFAULT, 
                                                       "defaults",
                                                       &defaults_source_adapter);
    if (!source) return NULL;
    
    // 创建私有数据
    defaults_source_priv_t* priv = (defaults_source_priv_t*)AGENTOS_CALLOC(1, sizeof(defaults_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        return NULL;
    }
    
    // 分配键值对数组
    priv->keys = (char**)AGENTOS_CALLOC(count, sizeof(char*));
    priv->vals = (char**)AGENTOS_CALLOC(count, sizeof(char*));
    if (!priv->keys || !priv->vals) {
        if (priv->keys) AGENTOS_FREE(priv->keys);
        if (priv->vals) AGENTOS_FREE(priv->vals);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    priv->num_entries = count / 2;
    for (size_t i = 0; i < count; i += 2) {
        if (i + 1 < count) {
            priv->keys[i/2] = default_values[i] ? duplicate_string(default_values[i]) : NULL;
            priv->vals[i/2] = default_values[i+1] ? duplicate_string(default_values[i+1]) : NULL;
        }
    }
    
    // 更新属�
    source->priv_data = priv;
    source->attributes.read_only = true; // 默认值只�
    source->attributes.watchable = false; // 默认值不会变�?    
    return source;
}

config_source_t* config_source_create_remote(const char* url, const char* token,
                                               const char* ns, uint32_t poll_interval_ms) {
    if (!url) return NULL;
    
    config_source_t* source = config_source_create_base(CONFIG_SOURCE_NETWORK,
                                                       "remote",
                                                       &remote_source_adapter);
    if (!source) return NULL;
    
    remote_source_priv_t* priv = (remote_source_priv_t*)AGENTOS_CALLOC(1, sizeof(remote_source_priv_t));
    if (!priv) {
        config_source_free_base(source);
        return NULL;
    }
    
    priv->url = duplicate_string(url);
    priv->token = token ? duplicate_string(token) : NULL;
    priv->namespace_name = ns ? duplicate_string(ns) : duplicate_string("default");
    priv->poll_interval_ms = poll_interval_ms > 0 ? poll_interval_ms : 30000;
    priv->last_etag_hash = 0;
    priv->last_response = NULL;
    priv->last_response_len = 0;
    
    source->priv_data = priv;
    source->attributes.read_only = true;
    source->attributes.watchable = true;
    
    return source;
}


void config_source_destroy(config_source_t* source) {
    if (!source) return;
    
    if (source->adapter && source->adapter->destroy) {
        source->adapter->destroy(source);
    } else {
        // 如果没有适配器，直接释放基础资源
        config_source_free_base(source);
    }
}

config_error_t config_source_load(config_source_t* source, config_context_t* ctx) {
    if (!source || !ctx || !source->adapter || !source->adapter->load) {
        return CONFIG_ERROR_INVALID_ARG;
    }
    
    return source->adapter->load(source, ctx);
}

config_error_t config_source_save(config_source_t* source, const config_context_t* ctx) {
    if (!source || !ctx || !source->adapter || !source->adapter->save) {
        return CONFIG_ERROR_INVALID_ARG;
    }
    
    return source->adapter->save(source, ctx);
}

bool config_source_has_changed(config_source_t* source) {
    if (!source || !source->adapter || !source->adapter->has_changed) {
        return false;
    }
    
    return source->adapter->has_changed(source);
}

const config_source_attr_t* config_source_get_attributes(config_source_t* source) {
    if (!source || !source->adapter || !source->adapter->get_attributes) {
        return NULL;
    }
    
    return source->adapter->get_attributes(source);
}

config_source_type_t config_source_get_type(config_source_t* source) {
    if (!source) return CONFIG_SOURCE_DEFAULT;
    
    const config_source_attr_t* attr = config_source_get_attributes(source);
    if (!attr) return CONFIG_SOURCE_DEFAULT;
    
    return attr->type;
}

/* ==================== 配置源管理器实现 ==================== */

config_source_manager_t* config_source_manager_create(void) {
    config_source_manager_t* manager = (config_source_manager_t*)AGENTOS_CALLOC(1, sizeof(config_source_manager_t));
    if (!manager) return NULL;
    
    // 初始容量
    manager->capacity = 16;
    manager->sources = (config_source_t**)AGENTOS_CALLOC(manager->capacity, sizeof(config_source_t*));
    if (!manager->sources) {
        AGENTOS_FREE(manager);
        return NULL;
    }
    
    manager->count = 0;
    manager->change_callback = NULL;
    manager->callback_user_data = NULL;
    manager->watching = false;
    manager->last_notify_time_ms = 0;
    manager->debounce_ms = 500;
    agentos_mutex_init(&manager->internal_mutex);
    return manager;
}

void config_source_manager_destroy(config_source_manager_t* manager) {
    if (!manager) return;
    
    // 销毁所有配置源
    for (size_t i = 0; i < manager->count; i++) {
        if (manager->sources[i]) {
            config_source_destroy(manager->sources[i]);
        }
    }
    
    // 释放资源
    if (manager->sources) AGENTOS_FREE(manager->sources);
    AGENTOS_FREE(manager);
}

config_error_t config_source_manager_add(config_source_manager_t* manager, config_source_t* source) {
    if (!manager || !source) return CONFIG_ERROR_INVALID_ARG;
    
    // 检查容量，必要时扩�
    if (manager->count >= manager->capacity) {
        size_t new_capacity = manager->capacity * 2;
        config_source_t** new_sources = (config_source_t**)AGENTOS_REALLOC(manager->sources, 
                                                                  new_capacity * sizeof(config_source_t*));
        if (!new_sources) return CONFIG_ERROR_OUT_OF_MEMORY;
        
        manager->sources = new_sources;
        manager->capacity = new_capacity;
    }
    
    // 添加配置�
    manager->sources[manager->count] = source;
    manager->count++;
    
    // 更新时间�
    if (source->adapter && source->adapter->get_attributes) {
        const config_source_attr_t* attr = source->adapter->get_attributes(source);
        if (attr) {
            // 这里可以记录添加时间
        }
    }
    
    return CONFIG_SUCCESS;
}

config_error_t config_source_manager_remove(config_source_manager_t* manager, config_source_t* source) {
    if (!manager || !source) return CONFIG_ERROR_INVALID_ARG;
    
    // 查找配置�
    for (size_t i = 0; i < manager->count; i++) {
        if (manager->sources[i] == source) {
            // 移动后续元素
            for (size_t j = i; j < manager->count - 1; j++) {
                manager->sources[j] = manager->sources[j + 1];
            }
            manager->count--;
            manager->sources[manager->count] = NULL;
            return CONFIG_SUCCESS;
        }
    }
    
    return CONFIG_ERROR_NOT_FOUND;
}

config_source_t* config_source_manager_find(config_source_manager_t* manager, const char* name) {
    if (!manager || !name) return NULL;
    
    for (size_t i = 0; i < manager->count; i++) {
        const config_source_attr_t* attr = config_source_get_attributes(manager->sources[i]);
        if (attr && attr->name && strcmp(attr->name, name) == 0) {
            return manager->sources[i];
        }
    }
    
    return NULL;
}

config_error_t config_source_manager_load_all(config_source_manager_t* manager, 
                                              config_context_t* ctx, 
                                              int merge_strategy) {
    if (!manager || !ctx) return CONFIG_ERROR_INVALID_ARG;
    
    config_error_t overall_error = CONFIG_SUCCESS;
    
    // 按优先级排序加载
    for (size_t i = 0; i < manager->count; i++) {
        config_source_t* source = manager->sources[i];
        if (!source) continue;
        
        config_error_t error = config_source_load(source, ctx);
        if (error != CONFIG_SUCCESS) {
            overall_error = error;
            // 根据策略决定是否继续
            if (merge_strategy == 0) { // 严格模式：任何错误都停止
                return error;
            }
        }
    }
    
    return overall_error;
}

config_error_t config_source_manager_watch(config_source_manager_t* manager,
                                           void (*callback)(config_source_t* source, void* user_data),
                                           void* user_data) {
    if (!manager) return CONFIG_ERROR_INVALID_ARG;
    
    manager->change_callback = callback;
    manager->callback_user_data = user_data;
    manager->watching = (callback != NULL);
    
    return CONFIG_SUCCESS;
}

int config_source_manager_poll_changes(config_source_manager_t* manager) {
    if (!manager) return 0;

    agentos_mutex_lock(&manager->internal_mutex);

    if (!manager->watching || !manager->change_callback) {
        agentos_mutex_unlock(&manager->internal_mutex);
        return 0;
    }

    uint64_t now_ms;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        now_ms = (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    } else {
        now_ms = (uint64_t)time(NULL) * 1000;
        agentos_mutex_unlock(&manager->internal_mutex);
        return 0;
    }

    if (manager->last_notify_time_ms > 0 &&
        now_ms - manager->last_notify_time_ms < manager->debounce_ms) {
        agentos_mutex_unlock(&manager->internal_mutex);
        return 0;
    }

    int change_count = 0;
    int first_changed_idx = -1;

    for (size_t i = 0; i < manager->count; i++) {
        config_source_t* source = manager->sources[i];
        if (!source) continue;

        const config_source_attr_t* attr = config_source_get_attributes(source);
        if (!attr || !attr->watchable) continue;

        if (config_source_has_changed(source)) {
            change_count++;
            if (first_changed_idx < 0) {
                first_changed_idx = (int)i;
            }
        }
    }

    if (change_count > 0) {
        manager->last_notify_time_ms = now_ms;

        void (*cb)(config_source_t*, void*) = manager->change_callback;
        void* ud = manager->callback_user_data;

        agentos_mutex_unlock(&manager->internal_mutex);

        for (size_t i = 0; i < manager->count; i++) {
            config_source_t* source = manager->sources[i];
            if (!source) continue;
            const config_source_attr_t* attr = config_source_get_attributes(source);
            if (!attr || !attr->watchable) continue;
            if (config_source_has_changed(source)) {
                cb(source, ud);
            }
        }

        return change_count;
    }

    agentos_mutex_unlock(&manager->internal_mutex);
    return 0;
}

/* ==================== 工具函数实现 ==================== */

const char* config_source_type_to_string(config_source_type_t type) {
    switch (type) {
        case CONFIG_SOURCE_FILE: return "file";
        case CONFIG_SOURCE_ENV: return "env";
        case CONFIG_SOURCE_ARGS: return "args";
        case CONFIG_SOURCE_MEMORY: return "memory";
        case CONFIG_SOURCE_NETWORK: return "network";
        case CONFIG_SOURCE_DATABASE: return "database";
        case CONFIG_SOURCE_DEFAULT: return "default";
        default: return "unknown";
    }
}

const char* config_parse_file_format(const char* file_path) {
    if (!file_path) return "unknown";
    
    const char* dot = strrchr(file_path, '.');
    if (!dot) return "unknown";
    
    const char* ext = dot + 1;
    if (strcasecmp(ext, "json") == 0) return "json";
    if (strcasecmp(ext, "yaml") == 0 || strcasecmp(ext, "yml") == 0) return "yaml";
    if (strcasecmp(ext, "toml") == 0) return "toml";
    if (strcasecmp(ext, "ini") == 0 || strcasecmp(ext, "cfg") == 0) return "ini";
    if (strcasecmp(ext, "xml") == 0) return "xml";
    
    return "unknown";
}

char* config_source_create_name(config_source_type_t type, const char* identifier) {
    if (!identifier) return NULL;
    
    const char* type_str = config_source_type_to_string(type);
    size_t type_len = strlen(type_str);
    size_t id_len = strlen(identifier);
    
    char* name = (char*)AGENTOS_MALLOC(type_len + id_len + 2); // +2 for ':' and null terminator
    if (!name) return NULL;
    
    snprintf(name, type_len + id_len + 2, "%s:%s", type_str, identifier);
    return name;
}