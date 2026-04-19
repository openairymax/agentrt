/**
 * @file config_source.c
 * @brief 统一配置模块 - 源适配层实�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一配置模块的源适配层功能，提供�? * 1. 多种配置源的统一适配接口
 * 2. 文件、环境变量、命令行参数、内存等配置源实�? * 3. 配置源管理器和监控功�? * 4. 统一的错误处理和资源管理
 *
 * 注意：这是一个基础实现，实际使用时应根据需要扩展特定配置源�? */

#include "config_source.h"
#include "core_config.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

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

/** 文件配置源私有数�?*/
typedef struct {
    char* file_path;                 // 文件路径
    char* format;                    // 文件格式
    char* encoding;                  // 文件编码
    bool auto_reload;                // 是否自动重载
    uint32_t reload_interval_ms;     // 重载间隔
    uint64_t last_modified;          // 最后修改时�?    FILE* file_handle;               // 文件句柄（如果需要保持打开�?} file_source_priv_t;

/** 环境变量配置源私有数�?*/
typedef struct {
    char* prefix;                    // 环境变量前缀
    bool case_sensitive;             // 是否区分大小�?    char* separator;                 // 键分隔符
    bool expand_vars;                // 是否展开变量引用
    char** env_keys;                 // 环境变量键数�?    size_t env_count;                // 环境变量数量
} env_source_priv_t;

/** 命令行配置源私有数据 */
typedef struct {
    int argc;                        // 参数数量
    char** argv;                     // 参数数组（不拥有所有权�?    char* prefix;                    // 参数前缀
    char* assign_char;               // 赋值字�?    bool allow_positional;           // 是否允许位置参数
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
    char** keys;                     // 键数�?    char** values;                   // 值数�?    size_t count;                    // 键值对数量
} defaults_source_priv_t;

/** 配置源管理器结构�?*/
struct config_source_manager {
    /** 配置源数�?*/
    config_source_t** sources;
    
    /** 配置源数�?*/
    size_t count;
    
    /** 配置源容�?*/
    size_t capacity;
    
    /** 变化回调函数 */
    void (*change_callback)(config_source_t* source, void* user_data);
    
    /** 回调用户数据 */
    void* callback_user_data;
    
    /** 是否正在监控 */
    bool watching;
    
    /** 互斥锁保护管理器 */
    void* mutex; // 简化实现，实际应使用pthread_mutex_t
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
    
    // 初始化属�?    source->adapter = adapter;
    source->attributes.type = type;
    source->attributes.name = name ? AGENTOS_STRDUP(name) : NULL;
    source->attributes.priority = 50; // 默认优先�?    source->attributes.read_only = false;
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
 * @brief 解析JSON配置（简化实现）
 * 
 * 简化JSON解析，实际实现应使用完整JSON解析库�? * 
 * @param data JSON数据
 * @param data_len 数据长度
 * @param ctx 配置上下�? * @return 错误�? */
static config_error_t parse_json_simple(const char* data, size_t data_len, config_context_t* ctx) {
    if (!data || data_len == 0 || !ctx) return CONFIG_ERROR_INVALID_ARG;
    const char* p = data;
    const char* end = data + data_len;
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    if (p >= end) return CONFIG_SUCCESS;
    if (*p == '{') {
        p++;
        while (p < end) {
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')) p++;
            if (p >= end || *p == '}') break;
            if (*p != '"') { p++; continue; }
            p++;
            const char* key_start = p;
            while (p < end && *p != '"') { if (*p == '\\') p++; p++; }
            size_t key_len = (size_t)(p - key_start);
            char key[256];
            if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
            memcpy(key, key_start, key_len);
            key[key_len] = '\0';
            p++;
            while (p < end && *p != ':') p++;
            p++;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            if (p >= end) break;
            if (*p == '"') {
                p++;
                const char* val_start = p;
                while (p < end && *p != '"') { if (*p == '\\') p++; p++; }
                size_t val_len = (size_t)(p - val_start);
                char val[1024];
                if (val_len >= sizeof(val)) val_len = sizeof(val) - 1;
                memcpy(val, val_start, val_len);
                val[val_len] = '\0';
                config_value_t* cv = config_value_create_string(val);
                if (cv) config_context_set(ctx, key, cv);
                p++;
            } else if (*p == '-' || (*p >= '0' && *p <= '9')) {
                const char* num_start = p;
                while (p < end && *p != ',' && *p != '}' && *p != ' ' && *p != '\n') p++;
                char num_buf[64];
                size_t nlen = (size_t)(p - num_start);
                if (nlen >= sizeof(num_buf)) nlen = sizeof(num_buf) - 1;
                memcpy(num_buf, num_start, nlen);
                num_buf[nlen] = '\0';
                if (strchr(num_buf, '.') || strchr(num_buf, 'e') || strchr(num_buf, 'E')) {
                    config_value_t* cv = config_value_create_double(atof(num_buf));
                    if (cv) config_context_set(ctx, key, cv);
                } else {
                    config_value_t* cv = config_value_create_int(atoi(num_buf));
                    if (cv) config_context_set(ctx, key, cv);
                }
            } else if (strncmp(p, "true", 4) == 0) {
                config_value_t* cv = config_value_create_bool(true);
                if (cv) config_context_set(ctx, key, cv);
                p += 4;
            } else if (strncmp(p, "false", 5) == 0) {
                config_value_t* cv = config_value_create_bool(false);
                if (cv) config_context_set(ctx, key, cv);
                p += 5;
            }
        }
    }
    return CONFIG_SUCCESS;
}

/**
 * @brief 解析INI配置（简化实现）
 * 
 * 简化INI解析，支持基本的键值对�? * 
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

/**
 * @brief 检查文件是否修�? * 
 * 通过文件修改时间检查文件是否修改�? * 
 * @param file_path 文件路径
 * @param last_modified 上次修改时间
 * @return 是否已修�? */
static bool check_file_modified(const char* file_path, uint64_t last_modified) {
    if (!file_path) return false;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (!GetFileAttributesExA(file_path, GetFileExInfoStandard, &attr)) return false;
    uint64_t mod_time = ((uint64_t)attr.ftLastWriteTime.dwHighDateTime << 32) | attr.ftLastWriteTime.dwLowDateTime;
    mod_time /= 10000000;
    return mod_time > last_modified;
#else
    struct stat st;
    if (stat(file_path, &st) != 0) return false;
    uint64_t mod_time = (uint64_t)st.st_mtime;
    return mod_time > last_modified;
#endif
}

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
    
    // 根据格式解析配置
    config_error_t error = CONFIG_SUCCESS;
    if (priv->format && strcmp(priv->format, "json") == 0) {
        error = parse_json_simple(buffer, file_size, ctx);
    } else if (priv->format && strcmp(priv->format, "ini") == 0) {
        error = parse_ini_simple(buffer, file_size, ctx);
    } else {
        // 默认尝试JSON格式
        error = parse_json_simple(buffer, file_size, ctx);
    }
    
    AGENTOS_FREE(buffer);
    return error;
}

/**
 * @brief 文件配置源保存函�? * 
 * 保存配置到文件�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t file_source_save(config_source_t* source, const config_context_t* ctx) {
    // 简化实现：文件配置源暂不支持保�?    (void)source;
    (void)ctx;
    return CONFIG_ERROR_UNSUPPORTED;
}

/**
 * @brief 文件配置源检查变化函�? * 
 * 检查文件是否已修改�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool file_source_has_changed(config_source_t* source) {
    if (!source) return false;
    
    file_source_priv_t* priv = (file_source_priv_t*)source->priv_data;
    if (!priv || !priv->file_path) return false;
    
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

/* ==================== 环境变量配置源适配�?==================== */

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
        config_value_t* cv = config_value_create_string(val);
        if (cv) config_context_set(ctx, key, cv);
    }
    return CONFIG_SUCCESS;
}

/**
 * @brief 环境变量配置源保存函�? * 
 * 保存配置到环境变量�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t env_source_save(config_source_t* source, const config_context_t* ctx) {
    // 简化实现：环境变量配置源暂不支持保�?    (void)source;
    (void)ctx;
    return CONFIG_ERROR_UNSUPPORTED;
}

/**
 * @brief 环境变量配置源检查变化函�? * 
 * 环境变量通常不会变化�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool env_source_has_changed(config_source_t* source) {
    // 环境变量变化检测复杂，简化实现返回false
    (void)source;
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
    // 命令行配置源不支持保�?    (void)source;
    (void)ctx;
    return CONFIG_ERROR_UNSUPPORTED;
}

/**
 * @brief 命令行配置源检查变化函�? * 
 * 命令行参数不会变化�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool args_source_has_changed(config_source_t* source) {
    // 命令行参数不会变�?    (void)source;
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
        error = parse_json_simple(priv->data, priv->data_len, ctx);
    } else if (priv->format && strcmp(priv->format, "ini") == 0) {
        error = parse_ini_simple(priv->data, priv->data_len, ctx);
    } else {
        // 默认尝试JSON格式
        error = parse_json_simple(priv->data, priv->data_len, ctx);
    }
    
    return error;
}

/**
 * @brief 内存配置源保存函�? * 
 * 内存配置源不支持保存�? * 
 * @param source 配置�? * @param ctx 配置上下�? * @return 错误�? */
static config_error_t memory_source_save(config_source_t* source, const config_context_t* ctx) {
    // 内存配置源不支持保存
    (void)source;
    (void)ctx;
    return CONFIG_ERROR_UNSUPPORTED;
}

/**
 * @brief 内存配置源检查变化函�? * 
 * 内存配置源不会自动变化�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool memory_source_has_changed(config_source_t* source) {
    // 内存配置源需要外部触发变�?    (void)source;
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
    for (size_t idx = 0; idx < priv->count; idx++) {
        if (priv->keys[idx] && priv->values[idx]) {
            config_value_t* cv = config_value_create_string(priv->values[idx]);
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
    // 默认值配置源不支持保�?    (void)source;
    (void)ctx;
    return CONFIG_ERROR_UNSUPPORTED;
}

/**
 * @brief 默认值配置源检查变化函�? * 
 * 默认值不会变化�? * 
 * @param source 配置�? * @return 是否已修�? */
static bool defaults_source_has_changed(config_source_t* source) {
    // 默认值不会变�?    (void)source;
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
            for (size_t i = 0; i < priv->count; i++) {
                if (priv->keys[i]) AGENTOS_FREE(priv->keys[i]);
            }
            AGENTOS_FREE(priv->keys);
        }
        if (priv->values) {
            for (size_t i = 0; i < priv->count; i++) {
                if (priv->values[i]) AGENTOS_FREE(priv->values[i]);
            }
            AGENTOS_FREE(priv->values);
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
    
    // 检查资源分配是否成�?    if (!priv->file_path || !priv->format || !priv->encoding) {
        if (priv->file_path) AGENTOS_FREE(priv->file_path);
        if (priv->format) AGENTOS_FREE(priv->format);
        if (priv->encoding) AGENTOS_FREE(priv->encoding);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 更新属�?    source->priv_data = priv;
    source->attributes.watchable = options->auto_reload;
    source->attributes.read_only = false; // 文件配置源可以保�?    
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
    
    // 检查资源分配是否成�?    if (options->separator && !priv->separator) {
        if (priv->prefix) AGENTOS_FREE(priv->prefix);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 更新属�?    source->priv_data = priv;
    source->attributes.read_only = true; // 环境变量只读
    source->attributes.watchable = false; // 环境变量变化检测复�?    
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
    
    // 检查资源分配是否成�?    if (!priv->assign_char) {
        if (priv->prefix) AGENTOS_FREE(priv->prefix);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 更新属�?    source->priv_data = priv;
    source->attributes.read_only = true; // 命令行参数只�?    source->attributes.watchable = false; // 命令行参数不会变�?    
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
    
    // 检查资源分配是否成�?    if (!priv->data || !priv->format) {
        if (priv->data) AGENTOS_FREE(priv->data);
        if (priv->format) AGENTOS_FREE(priv->format);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 更新属�?    source->priv_data = priv;
    source->attributes.read_only = true; // 内存配置源通常只读
    source->attributes.watchable = false; // 需要外部触发变�?    
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
    priv->values = (char**)AGENTOS_CALLOC(count, sizeof(char*));
    if (!priv->keys || !priv->values) {
        if (priv->keys) AGENTOS_FREE(priv->keys);
        if (priv->values) AGENTOS_FREE(priv->values);
        AGENTOS_FREE(priv);
        config_source_free_base(source);
        return NULL;
    }
    
    // 复制键值对（假设default_values是[key, value, key, value, ...]格式�?    priv->count = count / 2; // 每对键值算一个配置项
    for (size_t i = 0; i < count; i += 2) {
        if (i + 1 < count) {
            priv->keys[i/2] = default_values[i] ? duplicate_string(default_values[i]) : NULL;
            priv->values[i/2] = default_values[i+1] ? duplicate_string(default_values[i+1]) : NULL;
        }
    }
    
    // 更新属�?    source->priv_data = priv;
    source->attributes.read_only = true; // 默认值只�?    source->attributes.watchable = false; // 默认值不会变�?    
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
    manager->mutex = NULL; // 简化实�?    
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
    
    // 检查容量，必要时扩�?    if (manager->count >= manager->capacity) {
        size_t new_capacity = manager->capacity * 2;
        config_source_t** new_sources = (config_source_t**)AGENTOS_REALLOC(manager->sources, 
                                                                  new_capacity * sizeof(config_source_t*));
        if (!new_sources) return CONFIG_ERROR_OUT_OF_MEMORY;
        
        manager->sources = new_sources;
        manager->capacity = new_capacity;
    }
    
    // 添加配置�?    manager->sources[manager->count] = source;
    manager->count++;
    
    // 更新时间�?    if (source->adapter && source->adapter->get_attributes) {
        const config_source_attr_t* attr = source->adapter->get_attributes(source);
        if (attr) {
            // 这里可以记录添加时间
        }
    }
    
    return CONFIG_SUCCESS;
}

config_error_t config_source_manager_remove(config_source_manager_t* manager, config_source_t* source) {
    if (!manager || !source) return CONFIG_ERROR_INVALID_ARG;
    
    // 查找配置�?    for (size_t i = 0; i < manager->count; i++) {
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
    
    // 按优先级排序（简化实现：按添加顺序加载）
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