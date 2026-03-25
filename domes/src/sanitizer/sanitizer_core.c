/**
 * @file sanitizer_core.c
 * @brief 输入净化器核心实现
 * @author Spharx
 * @date 2024
 */

#include "sanitizer.h"
#include "sanitizer_rules.h"
#include "sanitizer_cache.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DEFAULT_MAX_LENGTH 65536

struct sanitizer {
    sanitizer_rules_t* rules;
    sanitizer_cache_t* cache;
    domes_rwlock_t lock;
    domes_atomic64_t total_sanitized;
    domes_atomic64_t total_rejected;
};

void sanitizer_default_context(sanitize_context_t* ctx) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(sanitize_context_t));
    ctx->level = SANITIZE_LEVEL_NORMAL;
    ctx->max_length = DEFAULT_MAX_LENGTH;
    ctx->allow_html = false;
    ctx->allow_sql = false;
    ctx->allow_shell = false;
    ctx->allow_path = false;
}

sanitizer_t* sanitizer_create(const char* rules_path) {
    sanitizer_t* san = (sanitizer_t*)domes_mem_alloc(sizeof(sanitizer_t));
    if (!san) return NULL;
    
    memset(san, 0, sizeof(sanitizer_t));
    
    if (domes_rwlock_init(&san->lock) != DOMES_OK) {
        domes_mem_free(san);
        return NULL;
    }
    
    san->rules = sanitizer_rules_create(rules_path);
    if (!san->rules) {
        domes_rwlock_destroy(&san->lock);
        domes_mem_free(san);
        return NULL;
    }
    
    san->cache = sanitizer_cache_create(1024);
    if (!san->cache) {
        sanitizer_rules_destroy(san->rules);
        domes_rwlock_destroy(&san->lock);
        domes_mem_free(san);
        return NULL;
    }
    
    return san;
}

void sanitizer_destroy(sanitizer_t* sanitizer) {
    if (!sanitizer) return;
    
    domes_rwlock_wrlock(&sanitizer->lock);
    
    if (sanitizer->rules) {
        sanitizer_rules_destroy(sanitizer->rules);
    }
    if (sanitizer->cache) {
        sanitizer_cache_destroy(sanitizer->cache);
    }
    
    domes_rwlock_unlock(&sanitizer->lock);
    domes_rwlock_destroy(&sanitizer->lock);
    domes_mem_free(sanitizer);
}

static bool contains_dangerous_chars(const char* input, const sanitize_context_t* ctx) {
    if (!input) return false;
    
    const char* p = input;
    while (*p) {
        if (!ctx->allow_html) {
            if (*p == '<' || *p == '>') return true;
        }
        if (!ctx->allow_sql) {
            if (*p == '\'' || *p == '"' || *p == ';') {
                if (ctx->level == SANITIZE_LEVEL_STRICT) return true;
            }
        }
        if (!ctx->allow_shell) {
            if (*p == '|' || *p == '&' || *p == '$' || *p == '`' ||
                *p == '(' || *p == ')' || *p == '{' || *p == '}') {
                if (ctx->level != SANITIZE_LEVEL_RELAXED) return true;
            }
        }
        if (!ctx->allow_path) {
            if (*p == '\\' || (p != input && *p == '.' && *(p-1) == '.')) {
                if (ctx->level == SANITIZE_LEVEL_STRICT) return true;
            }
        }
        if ((unsigned char)*p < 0x20 && *p != '\t' && *p != '\n' && *p != '\r') {
            return true;
        }
        p++;
    }
    
    return false;
}

static int apply_escape_rules(const char* input, char* output, size_t output_size, 
                               const sanitize_context_t* ctx) {
    if (!input || !output || output_size == 0) return DOMES_ERROR_INVALID_ARG;
    
    size_t in_len = strlen(input);
    size_t out_pos = 0;
    
    for (size_t i = 0; i < in_len && out_pos < output_size - 1; i++) {
        char c = input[i];
        
        if (!ctx->allow_html) {
            if (c == '<') {
                if (out_pos + 4 >= output_size) break;
                memcpy(output + out_pos, "&lt;", 4);
                out_pos += 4;
                continue;
            }
            if (c == '>') {
                if (out_pos + 4 >= output_size) break;
                memcpy(output + out_pos, "&gt;", 4);
                out_pos += 4;
                continue;
            }
            if (c == '&') {
                if (out_pos + 5 >= output_size) break;
                memcpy(output + out_pos, "&amp;", 5);
                out_pos += 5;
                continue;
            }
        }
        
        if (!ctx->allow_sql) {
            if (c == '\'') {
                if (out_pos + 2 >= output_size) break;
                output[out_pos++] = '\'';
                output[out_pos++] = '\'';
                continue;
            }
        }
        
        if (!ctx->allow_shell) {
            if (c == '\\' || c == '\'' || c == '\"' || c == '`' ||
                c == '$' || c == '|' || c == '&' || c == ';' ||
                c == '(' || c == ')' || c == '{' || c == '}') {
                if (out_pos + 2 >= output_size) break;
                output[out_pos++] = '\\';
                output[out_pos++] = c;
                continue;
            }
        }
        
        output[out_pos++] = c;
    }
    
    output[out_pos] = '\0';
    return DOMES_OK;
}

sanitize_result_t sanitizer_sanitize(sanitizer_t* sanitizer,
                                      const char* input,
                                      char* output,
                                      size_t output_size,
                                      const sanitize_context_t* ctx) {
    if (!sanitizer || !input || !output || output_size == 0) {
        return SANITIZE_ERROR;
    }
    
    sanitize_context_t default_ctx;
    if (!ctx) {
        sanitizer_default_context(&default_ctx);
        ctx = &default_ctx;
    }
    
    size_t input_len = strlen(input);
    if (ctx->max_length > 0 && input_len > ctx->max_length) {
        domes_atomic_add64(&sanitizer->total_rejected, 1);
        return SANITIZE_REJECTED;
    }
    
    domes_rwlock_rdlock(&sanitizer->lock);
    
    bool cached = false;
    sanitize_result_t cached_result = SANITIZE_OK;
    char* cached_output = sanitizer_cache_get(sanitizer->cache, input, ctx->level);
    if (cached_output) {
        strncpy(output, cached_output, output_size - 1);
        output[output_size - 1] = '\0';
        domes_mem_free(cached_output);
        cached = true;
    }
    domes_rwlock_unlock(&sanitizer->lock);
    
    if (cached) {
        domes_atomic_add64(&sanitizer->total_sanitized, 1);
        return cached_result;
    }
    
    if (contains_dangerous_chars(input, ctx)) {
        if (ctx->level == SANITIZE_LEVEL_STRICT) {
            domes_atomic_add64(&sanitizer->total_rejected, 1);
            return SANITIZE_REJECTED;
        }
        
        if (apply_escape_rules(input, output, output_size, ctx) != DOMES_OK) {
            domes_atomic_add64(&sanitizer->total_rejected, 1);
            return SANITIZE_ERROR;
        }
        
        domes_rwlock_wrlock(&sanitizer->lock);
        sanitizer_cache_put(sanitizer->cache, input, output, ctx->level);
        domes_rwlock_unlock(&sanitizer->lock);
        
        domes_atomic_add64(&sanitizer->total_sanitized, 1);
        return SANITIZE_MODIFIED;
    }
    
    strncpy(output, input, output_size - 1);
    output[output_size - 1] = '\0';
    
    domes_rwlock_wrlock(&sanitizer->lock);
    sanitizer_cache_put(sanitizer->cache, input, output, ctx->level);
    domes_rwlock_unlock(&sanitizer->lock);
    
    domes_atomic_add64(&sanitizer->total_sanitized, 1);
    return SANITIZE_OK;
}

bool sanitizer_is_safe(sanitizer_t* sanitizer,
                       const char* input,
                       const sanitize_context_t* ctx) {
    if (!sanitizer || !input) return false;
    
    sanitize_context_t default_ctx;
    if (!ctx) {
        sanitizer_default_context(&default_ctx);
        ctx = &default_ctx;
    }
    
    size_t input_len = strlen(input);
    if (ctx->max_length > 0 && input_len > ctx->max_length) {
        return false;
    }
    
    return !contains_dangerous_chars(input, ctx);
}

int sanitizer_escape_html(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) return DOMES_ERROR_INVALID_ARG;
    
    size_t in_len = strlen(input);
    size_t out_pos = 0;
    
    for (size_t i = 0; i < in_len && out_pos < output_size - 1; i++) {
        char c = input[i];
        switch (c) {
            case '<':
                if (out_pos + 4 >= output_size) goto overflow;
                memcpy(output + out_pos, "&lt;", 4);
                out_pos += 4;
                break;
            case '>':
                if (out_pos + 4 >= output_size) goto overflow;
                memcpy(output + out_pos, "&gt;", 4);
                out_pos += 4;
                break;
            case '&':
                if (out_pos + 5 >= output_size) goto overflow;
                memcpy(output + out_pos, "&amp;", 5);
                out_pos += 5;
                break;
            case '"':
                if (out_pos + 6 >= output_size) goto overflow;
                memcpy(output + out_pos, "&quot;", 6);
                out_pos += 6;
                break;
            case '\'':
                if (out_pos + 6 >= output_size) goto overflow;
                memcpy(output + out_pos, "&#39;", 5);
                out_pos += 5;
                break;
            default:
                output[out_pos++] = c;
                break;
        }
    }
    
    output[out_pos] = '\0';
    return DOMES_OK;
    
overflow:
    output[out_pos] = '\0';
    return DOMES_ERROR_OVERFLOW;
}

int sanitizer_escape_sql(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) return DOMES_ERROR_INVALID_ARG;
    
    size_t in_len = strlen(input);
    size_t out_pos = 0;
    
    for (size_t i = 0; i < in_len && out_pos < output_size - 1; i++) {
        char c = input[i];
        switch (c) {
            case '\'':
                if (out_pos + 2 >= output_size) goto overflow;
                output[out_pos++] = '\'';
                output[out_pos++] = '\'';
                break;
            case '\\':
                if (out_pos + 2 >= output_size) goto overflow;
                output[out_pos++] = '\\';
                output[out_pos++] = '\\';
                break;
            case '\0':
                if (out_pos + 2 >= output_size) goto overflow;
                output[out_pos++] = '\\';
                output[out_pos++] = '0';
                break;
            case '\n':
                if (out_pos + 2 >= output_size) goto overflow;
                output[out_pos++] = '\\';
                output[out_pos++] = 'n';
                break;
            case '\r':
                if (out_pos + 2 >= output_size) goto overflow;
                output[out_pos++] = '\\';
                output[out_pos++] = 'r';
                break;
            default:
                output[out_pos++] = c;
                break;
        }
    }
    
    output[out_pos] = '\0';
    return DOMES_OK;
    
overflow:
    output[out_pos] = '\0';
    return DOMES_ERROR_OVERFLOW;
}

int sanitizer_escape_shell(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) return DOMES_ERROR_INVALID_ARG;
    
    size_t in_len = strlen(input);
    size_t out_pos = 0;
    
    for (size_t i = 0; i < in_len && out_pos < output_size - 1; i++) {
        char c = input[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '/') {
            output[out_pos++] = c;
        } else {
            if (out_pos + 4 >= output_size) goto overflow;
            out_pos += sprintf(output + out_pos, "\\x%02x", (unsigned char)c);
        }
    }
    
    output[out_pos] = '\0';
    return DOMES_OK;
    
overflow:
    output[out_pos] = '\0';
    return DOMES_ERROR_OVERFLOW;
}

int sanitizer_escape_path(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) return DOMES_ERROR_INVALID_ARG;
    
    size_t in_len = strlen(input);
    size_t out_pos = 0;
    
    for (size_t i = 0; i < in_len && out_pos < output_size - 1; i++) {
        char c = input[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || 
            c == '/' || c == '\\' || c == ':') {
            output[out_pos++] = c;
        } else {
            if (out_pos + 4 >= output_size) goto overflow;
            out_pos += sprintf(output + out_pos, "%%%02X", (unsigned char)c);
        }
    }
    
    output[out_pos] = '\0';
    return DOMES_OK;
    
overflow:
    output[out_pos] = '\0';
    return DOMES_ERROR_OVERFLOW;
}

int sanitizer_add_rule(sanitizer_t* sanitizer,
                       const char* pattern,
                       const char* replacement) {
    if (!sanitizer || !pattern) return DOMES_ERROR_INVALID_ARG;
    
    domes_rwlock_wrlock(&sanitizer->lock);
    int ret = sanitizer_rules_add(sanitizer->rules, pattern, replacement);
    sanitizer_cache_clear(sanitizer->cache);
    domes_rwlock_unlock(&sanitizer->lock);
    
    return ret;
}

void sanitizer_clear_rules(sanitizer_t* sanitizer) {
    if (!sanitizer) return;
    
    domes_rwlock_wrlock(&sanitizer->lock);
    sanitizer_rules_clear(sanitizer->rules);
    sanitizer_cache_clear(sanitizer->cache);
    domes_rwlock_unlock(&sanitizer->lock);
}
