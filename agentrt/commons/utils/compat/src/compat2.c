// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file compat.c
 * @brief 跨平台兼容性实现
 */

#include "compat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.h"


typedef void (*assert_handler_fn_t)(const char *cond, const char *file, int line, const char *func,
                                    const char *msg);

static assert_handler_fn_t g_assert_handler = NULL;

void agentrt_set_assert_handler(void (*handler)(const char *, const char *, int, const char *,
                                                const char *))
{
    g_assert_handler = (assert_handler_fn_t)handler;
}

void (*agentrt_get_assert_handler(void))(const char *, const char *, int, const char *,
                                         const char *)
{
    return (void (*)(const char *, const char *, int, const char *, const char *))g_assert_handler;
}

#ifdef AGENTRT_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <intrin.h>
#include <windows.h>
#else
#include <signal.h>
#include <unistd.h>
#endif

int agentrt_strlcpy(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0) {
        return AGENTRT_EINVAL;
    }

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

    __builtin_memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return (int)(src_len >= dest_size);
}

int agentrt_strlcat(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0) {
        return AGENTRT_EINVAL;
    }

    size_t dest_len = strlen(dest);
    if (dest_len >= dest_size) {
        return AGENTRT_EINVAL;
    }

    size_t src_len = strlen(src);
    size_t remaining = dest_size - dest_len - 1;
    size_t copy_len = (src_len < remaining) ? src_len : remaining;

    __builtin_memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';

    return (int)(src_len > remaining);
}

char *agentrt_strncpy_safe(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0) {
        return dest;
    }

    size_t src_len = strlen(src);
    size_t copy_len = (src_len < dest_size - 1) ? src_len : dest_size - 1;

    __builtin_memcpy(dest, src, copy_len);
    dest[copy_len] = '\0';

    return dest;
}

int agentrt_memset_s(void *dest, int c, size_t dest_size, size_t count)
{
    if (!dest) {
        return AGENTRT_EINVAL;
    }

    if (count > dest_size) {
        return AGENTRT_EINVAL;
    }

    __builtin_memset(dest, (int)c, count);
    return 0;
}

int agentrt_memcpy_s(void *dest, size_t dest_size, const void *src, size_t count)
{
    if (!dest || !src) {
        return AGENTRT_EINVAL;
    }

    if (count > dest_size) {
        return AGENTRT_EINVAL;
    }

    if ((char *)dest < (const char *)src + count && (const char *)src < (char *)dest + count) {
        __builtin_memmove(dest, src, count);
    } else {
        __builtin_memcpy(dest, src, count);
    }

    return 0;
}

int agentrt_memmove_s(void *dest, size_t dest_size, const void *src, size_t count)
{
    if (!dest || !src) {
        return AGENTRT_EINVAL;
    }

    if (count > dest_size) {
        return AGENTRT_EINVAL;
    }

    __builtin_memmove(dest, src, count);
    return 0;
}

void agentrt_assert_fail(const char *cond, const char *file, int line, const char *func)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "Assertion failed: %s\n", cond);
    fputs(buf, stderr);
    snprintf(buf, sizeof(buf), "  at %s:%d in %s()\n", file, line, func);
    fputs(buf, stderr);

    if (g_assert_handler) {
        g_assert_handler(cond, file, line, func, NULL);
        return;
    }

#ifdef AGENTRT_PLATFORM_WINDOWS
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#else
    raise(SIGABRT);
#endif

    abort();
}

void agentrt_assert_fail_msg(const char *cond, const char *file, int line, const char *func,
                             const char *msg)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "Assertion failed: %s\n", cond);
    fputs(buf, stderr);
    snprintf(buf, sizeof(buf), "  Message: %s\n", msg);
    fputs(buf, stderr);
    snprintf(buf, sizeof(buf), "  at %s:%d in %s()\n", file, line, func);
    fputs(buf, stderr);

    if (g_assert_handler) {
        g_assert_handler(cond, file, line, func, msg);
        return;
    }

#ifdef AGENTRT_PLATFORM_WINDOWS
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#else
    raise(SIGABRT);
#endif

    abort();
}

void agentrt_debug_break(void)
{
#ifdef AGENTRT_PLATFORM_WINDOWS
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

static const char *g_version_string = "0.1.0";

const char *agentrt_version_string(void)
{
    return g_version_string;
}

const char *agentrt_build_info(void)
{
    static char build_info[256] = {0};

    if (build_info[0] == '\0') {
        snprintf(build_info, sizeof(build_info),
                 "AgentRT v%s | Compiler: %s | Platform: %s | Build: %s %s", "0.1.0", "gcc",
                 "linux", __DATE__, __TIME__);
    }

    return build_info;
}

#ifdef _WIN32
#include <windows.h>

int gethostname(char *name, size_t len)
{
    DWORD size = (DWORD)len;
    if (!GetComputerNameA(name, &size)) {
        return AGENTRT_EINVAL;
    }
    return 0;
}

enum {
    AGENTRT_SC_PAGESIZE = 1,
    AGENTRT_SC_NPROCESSORS_ONLN = 2,
    AGENTRT_SC_OPEN_MAX = 3,
    AGENTRT_SC_CLK_TCK = 4
};

long sysconf(int name)
{
    switch (name) {
    case AGENTRT_SC_PAGESIZE: {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return (long)si.dwPageSize;
    }
    case AGENTRT_SC_NPROCESSORS_ONLN: {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return (long)si.dwNumberOfProcessors;
    }
    case AGENTRT_SC_OPEN_MAX:
        return 512;
    case AGENTRT_SC_CLK_TCK:
        return 1000;
    default:
        return AGENTRT_EINVAL;
    }
}
#endif
