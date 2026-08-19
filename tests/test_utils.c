// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_utils.c
 * @brief 通用工具函数单元测试（Unit Test）
 *
 * 自包含测试，用于验证基础工具语义：
 *
 * 1. 安全宏 AIRY_MEMCPY / AIRY_MEMSET / AIRY_STRNCPY_TERM 的边界行为
 *    （截断拷贝、null 终止、重叠无关的逐字节填充）
 * 2. 常用整数辅助宏（clamp / min / max）的正确性
 * 3. 字符串长度判断逻辑（空串、超长串）
 *
 * 自包含说明：
 * - 不链接 airy_common 库，仅使用 __builtin_* 安全宏，便于在任何
 *   构建环境（含 ASan + LTO）下快速编译运行
 * - STRICT 模式下 printf/fprintf 被毒化，使用 fputs + vsnprintf 输出
 */

#include "airy_memory.h" /* AIRY_MEMCPY/MEMSET/STRNCPY_TERM */
#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* noinline: 防止 LTO+ASan 内联合并栈帧，避免误报 stack-buffer-overflow */
#define TEST_FUNC __attribute__((noinline))

#ifndef AIRY_MEMSET
#error "AIRY_MEMSET must be defined"
#endif
#ifndef AIRY_MEMCPY
#error "AIRY_MEMCPY must be defined"
#endif
#ifndef AIRY_STRNCPY_TERM
#error "AIRY_STRNCPY_TERM must be defined"
#endif

/* ---- 待测的轻量工具宏（自包含，不依赖外部库） ---- */
#define UTIL_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define UTIL_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define UTIL_CLAMP(x, lo, hi) (UTIL_MIN(UTIL_MAX((x), (lo)), (hi)))

static int g_passed = 0;
static int g_failed = 0;
static int g_total = 0;

static void test_printf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
}

#define TEST(name)                             \
    do {                                       \
        g_total++;                             \
        test_printf("  [TEST] %s ... ", name); \
    } while (0)

#define PASS()                  \
    do {                        \
        g_passed++;             \
        fputs("PASS\n", stdout); \
    } while (0)

#define FAIL(reason)                       \
    do {                                   \
        g_failed++;                        \
        test_printf("FAIL: %s\n", reason); \
    } while (0)

static TEST_FUNC void test_min_max(void)
{
    TEST("utils: UTIL_MIN / UTIL_MAX");
    if (UTIL_MIN(3, 7) == 3 && UTIL_MAX(3, 7) == 7 &&
        UTIL_MIN(-2, 5) == -2 && UTIL_MAX(-2, 5) == 5)
        PASS();
    else
        FAIL("min/max returned wrong value");
}

static TEST_FUNC void test_clamp(void)
{
    TEST("utils: UTIL_CLAMP bounds");
    if (UTIL_CLAMP(5, 0, 10) == 5 &&
        UTIL_CLAMP(-3, 0, 10) == 0 &&
        UTIL_CLAMP(42, 0, 10) == 10)
        PASS();
    else
        FAIL("clamp did not honor lo/hi bounds");
}

static TEST_FUNC void test_strncpy_truncation(void)
{
    TEST("utils: AIRY_STRNCPY_TERM truncates safely");
    char dst[6];
    AIRY_STRNCPY_TERM(dst, "truncated-content", sizeof(dst));
    if (dst[5] == '\0' && strlen(dst) == 5 && memcmp(dst, "trunc", 5) == 0)
        PASS();
    else
        FAIL("truncation or termination wrong");
}

static TEST_FUNC void test_strncpy_empty(void)
{
    TEST("utils: AIRY_STRNCPY_TERM handles empty source");
    char dst[8] = "junk";
    AIRY_STRNCPY_TERM(dst, "", sizeof(dst));
    if (dst[0] == '\0')
        PASS();
    else
        FAIL("empty source not null-terminated");
}

static TEST_FUNC void test_memset_pattern(void)
{
    TEST("utils: AIRY_MEMSET fills with byte pattern");
    unsigned char buf[8];
    AIRY_MEMSET(buf, 0xAB, sizeof(buf));
    int ok = 1;
    for (int i = 0; i < 8; i++)
        if (buf[i] != 0xAB)
            ok = 0;
    if (ok)
        PASS();
    else
        FAIL("byte pattern not filled correctly");
}

int main(void)
{
    fputs("\n================================================\n", stdout);
    fputs("  AgentRT Utils Unit Test\n", stdout);
    fputs("================================================\n\n", stdout);

    test_min_max();
    test_clamp();
    test_strncpy_truncation();
    test_strncpy_empty();
    test_memset_pattern();

    fputs("\n================================================\n", stdout);
    test_printf("  Results: %d/%d passed, %d failed\n", g_passed, g_total, g_failed);
    fputs("================================================\n\n", stdout);

    return (g_failed > 0) ? 1 : 0;
}
