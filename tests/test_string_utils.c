// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_string_utils.c
 * @brief 字符串安全工具语义单元测试（String Utilities Unit Test）
 *
 * 轻量级自包含测试，验证 AgentRT 合规构建环境下字符串相关安全宏的语义：
 *
 * 1. AIRY_STRNCPY_TERM: 始终保证 null 终止（含截断场景）
 * 2. AIRY_MEMCPY:       基本拷贝、重叠边界、size=0 安全 no-op
 * 3. AIRY_MEMSET:       字节填充、部分填充、size=0 安全 no-op
 * 4. AIRY_OK:           成功码恒为 0
 *
 * 自包含说明：
 * - 不链接 airy_common 库（避免 ASan 符号依赖），仅使用 __builtin_* 安全宏
 * - STRICT 模式下 printf/fprintf 被毒化，使用 fputs + vsnprintf 输出
 * - 与 test_smoke.c / test_utils.c 相同风格，便于统一维护
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
#ifndef AIRY_OK
#error "AIRY_OK must be defined"
#endif

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

static TEST_FUNC void test_strncpy_term_basic(void)
{
    TEST("str: AIRY_STRNCPY_TERM copies and terminates");
    char dst[16] = {0};
    AIRY_STRNCPY_TERM(dst, "hello", sizeof(dst));
    if (strcmp(dst, "hello") == 0)
        PASS();
    else
        FAIL("copy mismatch");
}

static TEST_FUNC void test_strncpy_term_truncates_safely(void)
{
    TEST("str: AIRY_STRNCPY_TERM truncates with null terminator");
    char dst[4];
    AIRY_STRNCPY_TERM(dst, "way too long for this buffer", sizeof(dst));
    if (dst[3] == '\0' && strlen(dst) == 3)
        PASS();
    else
        FAIL("no null terminator — buffer overflow risk");
}

static TEST_FUNC void test_strncpy_term_empty_source(void)
{
    TEST("str: AIRY_STRNCPY_TERM handles empty source");
    char dst[8];
    AIRY_STRNCPY_TERM(dst, "", sizeof(dst));
    if (dst[0] == '\0')
        PASS();
    else
        FAIL("empty source not null-terminated");
}

static TEST_FUNC void test_memcpy_exact_fit(void)
{
    TEST("mem: AIRY_MEMCPY exact-fit copy");
    const char src[] = "abcd";
    char dst[5] = {0};
    AIRY_MEMCPY(dst, src, sizeof(src));
    if (strcmp(dst, "abcd") == 0)
        PASS();
    else
        FAIL("copy mismatch");
}

static TEST_FUNC void test_memcpy_zero_size_noop(void)
{
    TEST("mem: AIRY_MEMCPY size=0 is a safe no-op");
    char buf[8] = "keepme";
    AIRY_MEMCPY(buf, "zzz", 0);
    if (strcmp(buf, "keepme") == 0)
        PASS();
    else
        FAIL("size=0 modified the buffer");
}

static TEST_FUNC void test_memset_partial_fill(void)
{
    TEST("mem: AIRY_MEMSET partial fill");
    char buf[8] = {0};
    AIRY_MEMSET(buf, 0xAA, 4);
    int ok = 1;
    for (int i = 0; i < 4; i++)
        if ((unsigned char)buf[i] != 0xAA)
            ok = 0;
    for (int i = 4; i < 8; i++)
        if (buf[i] != 0)
            ok = 0;
    if (ok)
        PASS();
    else
        FAIL("partial fill pattern wrong");
}

static TEST_FUNC void test_memset_zero_size_noop(void)
{
    TEST("mem: AIRY_MEMSET size=0 is a safe no-op");
    char buf[8] = "keepme";
    AIRY_MEMSET(buf, 0, 0);
    if (strcmp(buf, "keepme") == 0)
        PASS();
    else
        FAIL("size=0 modified the buffer");
}

static TEST_FUNC void test_ok_semantics(void)
{
    TEST("err: AIRY_OK is 0");
    if (AIRY_OK == 0)
        PASS();
    else
        FAIL("AIRY_OK != 0");
}

int main(void)
{
    fputs("\n================================================\n", stdout);
    fputs("  AgentRT String Utils Unit Test\n", stdout);
    fputs("================================================\n\n", stdout);

    test_strncpy_term_basic();
    test_strncpy_term_truncates_safely();
    test_strncpy_term_empty_source();
    test_memcpy_exact_fit();
    test_memcpy_zero_size_noop();
    test_memset_partial_fill();
    test_memset_zero_size_noop();
    test_ok_semantics();

    fputs("\n================================================\n", stdout);
    test_printf("  Results: %d/%d passed, %d failed\n", g_passed, g_total, g_failed);
    fputs("================================================\n\n", stdout);

    return (g_failed > 0) ? 1 : 0;
}
