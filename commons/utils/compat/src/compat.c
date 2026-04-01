// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file compat.c
 * @brief 跨平台兼容性实现
 */

#include "compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AGENTOS_PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <intrin.h>
#else
    #include <signal.h>
    #include <unistd.h>
#endif

/* ==================== 安全字符串函数实现 ==================== */

int agentos_strlcpy(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    
    return (int)(src_len >= dest_size);
}

int agentos_strlcat(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return -1;
    }
    
    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size) {
        return -1;
    }
    
    size_t src_len = strlen(src);
    size_t remaining = dest_size - dest_len - 1;
    size_t copy_len = (src_len < remaining) ? src_len : remaining;
    
    memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';
    
    return (int)(src_len > remaining);
}

char* agentos_strncpy_safe(char* dest, const char* src, size_t dest_size) {
    if (!dest || !src || dest_size == 0) {
        return dest;
    }
    
    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;
    
    memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';
    
    return dest;
}

/* ==================== 安全内存函数实现 ==================== */

int agentos_memset_s(void* dest, int c, size_t dest_size, size_t count) {
    if (!dest) {
        return -1;
    }
    
    if (count > dest_size) {
        return -1;
    }
    
    memset(dest, c, count);
    return 0;
}

int agentos_memcpy_s(void* dest, size_t dest_size, const void* src, size_t count) {
    if (!dest || !src) {
        return -1;
    }
    
    if (count > dest_size) {
        return -1;
    }
    
    if ((char*)dest < (const char*)src + count && (const char*)src < (char*)dest + count) {
        /* 重叠区域，使用 memmove */
        memmove(dest, src, count);
    } else {
        memcpy(dest, src, count);
    }
    
    return 0;
}

int agentos_memmove_s(void* dest, size_t dest_size, const void* src, size_t count) {
    if (!dest || !src) {
        return -1;
    }
    
    if (count > dest_size) {
        return -1;
    }
    
    memmove(dest, src, count);
    return 0;
}

/* ==================== 断言函数实现 ==================== */

void agentos_assert_fail(const char* cond, const char* file, int line, const char* func) {
    fprintf(stderr, "Assertion failed: %s\n", cond);
    fprintf(stderr, "  at %s:%d in %s()\n", file, line, func);
    
#ifdef AGENTOS_PLATFORM_WINDOWS
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#else
    raise(SIGABRT);
#endif
    
    abort();
}

void agentos_assert_fail_msg(const char* cond, const char* file, int line, const char* func, const char* msg) {
    fprintf(stderr, "Assertion failed: %s\n", cond);
    fprintf(stderr, "  Message: %s\n", msg);
    fprintf(stderr, "  at %s:%d in %s()\n", file, line, func);
    
#ifdef AGENTOS_PLATFORM_WINDOWS
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#else
    raise(SIGABRT);
#endif
    
    abort();
}

/* ==================== 调试函数实现 ==================== */

void agentos_debug_break(void) {
#ifdef AGENTOS_PLATFORM_WINDOWS
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#else
    #ifdef SIGTRAP
        raise(SIGTRAP);
    #else
        raise(SIGABRT);
    #endif
#endif
}

/* ==================== 版本信息实现 ==================== */

static const char* g_version_string = AGENTOS_VERSION_STRING;

const char* agentos_version_string(void) {
    return g_version_string;
}

const char* agentos_build_info(void) {
    static char build_info[256] = {0};
    
    if (build_info[0] == '\0') {
        snprintf(build_info, sizeof(build_info),
            "AgentOS v%s | Compiler: %s | Platform: %s | Build: %s %s",
            AGENTOS_VERSION_STRING,
            AGENTOS_COMPILER_NAME,
            AGENTOS_PLATFORM_NAME,
            __DATE__,
            __TIME__
        );
    }
    
    return build_info;
}
