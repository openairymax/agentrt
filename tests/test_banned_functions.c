// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_banned_functions.c
  * @brief BAN-030/BAN-155/BAN-154/BAN-073 编译期与运行期合规验证测试
 *
 * 此测试在 AIRY_COMPLIANCE_STRICT 模式下编译（不定义 AIRY_COMPLIANCE_IMPL），
  * 验证以下合规要求：
 *
 * 1. BAN-030 (strcpy): 被毒化，必须使用 AIRY_STRNCPY_TERM 替代
 * 2. BAN-155 (strncpy): 被毒化，必须使用 AIRY_STRNCPY_TERM 替代
 * 3. BAN-154 (memcpy/memset): 被毒化，必须使用 AIRY_MEMCPY/AIRY_MEMSET 替代
  * 4. BAN-073 (return -1): 生产代码禁止裸 return -1，必须使用 AIRY_ERR_* Error code
 *
  * 编译期验证：
 * - 此文件在 STRICT 模式下成功编译即证明安全宏不依赖被毒化的函数
  * - #error 指令验证关键宏与Error code已定义
 *
  * 运行期验证：
 * - 安全宏功能正确性（边界、零大小、null 终止、二进制数据等）
  * - Error code值正确性
 *
 * 输出说明：
 * - STRICT 模式下 printf/fprintf 被毒化（BAN-151 区域），此测试使用
  *   fputs + vsnprintf 输出（两者均未毒化），验证合规测试自身也遵守禁令。
 *
 * 自包含说明：
 * - 此测试不链接 airy_common 库（避免 ASan 符号依赖）
 * - 仅使用基于 __builtin_* 的安全宏，无需外部链接
 *
  * Task #39: 补充 BAN-073/BAN-154 编译期验证测试
 */

#include "airy_memory.h" /* AIRY_MEMSET/MEMCPY/STRNCPY_TERM/MALLOC/FREE */
#include "error.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* noinline: prevents LTO+ASan inlining from merging frames and falsely reporting stack overflow.
  * With -O3 -flto, the compiler may inline all small functions into main()'s frame,
  * making ASan report spurious cross-function stack-buffer-overflow.
  * __attribute__((noinline)) keeps each test function in its own frame. */
#define TEST_FUNC __attribute__((noinline))

#ifndef AIRY_MEMSET
#error "BAN-154: AIRY_MEMSET must be defined (memset is poisoned under STRICT)"
#endif
#ifndef AIRY_MEMCPY
#error "BAN-154: AIRY_MEMCPY must be defined (memcpy is poisoned under STRICT)"
#endif

#ifndef AIRY_STRNCPY_TERM
#error "BAN-155: AIRY_STRNCPY_TERM must be defined (strncpy is poisoned under STRICT)"
#endif

#ifndef AIRY_MALLOC
#error "BAN-073: AIRY_MALLOC must be defined (malloc is poisoned under STRICT)"
#endif
#ifndef AIRY_FREE
#error "BAN-073: AIRY_FREE must be defined (free is poisoned under STRICT)"
#endif

#ifndef AIRY_OK
#error "BAN-073: AIRY_OK (0) must be defined"
#endif
#ifndef AIRY_ERR_NOT_FOUND
#error "BAN-073: AIRY_ERR_NOT_FOUND must be defined"
#endif
#ifndef AIRY_ERR_INVALID_PARAM
#error "BAN-073: AIRY_ERR_INVALID_PARAM must be defined"
#endif

/* In STRICT mode printf/fprintf are poisoned; use fputs + vsnprintf instead.
  * Neither vsnprintf nor fputs is on the poison list. */

static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_total = 0;

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
        g_tests_total++;                       \
        test_printf("  [TEST] %s ... ", name); \
    } while (0)

#define PASS()                   \
    do {                         \
        g_tests_passed++;        \
        fputs("PASS\n", stdout); \
    } while (0)

#define FAIL(reason)                       \
    do {                                   \
        g_tests_failed++;                  \
        test_printf("FAIL: %s\n", reason); \
    } while (0)

static TEST_FUNC void test_memset_basic_fill(void)
{
    TEST("BAN-154: AIRY_MEMSET fills buffer with specified value");
    unsigned char buf[64];
    AIRY_MEMSET(buf, 0xAB, sizeof(buf));
    int ok = 1;
    for (int i = 0; i < 64; i++) {
        if (buf[i] != 0xAB) {
            ok = 0;
            break;
        }
    }
    if (ok)
        PASS();
    else
        FAIL("buffer not fully filled with 0xAB");
}

static TEST_FUNC void test_memset_zero_fill(void)
{
    TEST("BAN-154: AIRY_MEMSET zero-fills buffer");
    char buf[32];
    for (int i = 0; i < 32; i++)
        buf[i] = 'X';
    AIRY_MEMSET(buf, 0, sizeof(buf));
    int ok = 1;
    for (int i = 0; i < 32; i++) {
        if (buf[i] != 0) {
            ok = 0;
            break;
        }
    }
    if (ok)
        PASS();
    else
        FAIL("buffer not zero-filled");
}

static TEST_FUNC void test_memset_zero_size(void)
{
    TEST("BAN-154: AIRY_MEMSET with size=0 is safe no-op");
    char buf[8] = "ABCDEFG";
    AIRY_MEMSET(buf, 0, 0);
    if (strcmp(buf, "ABCDEFG") == 0)
        PASS();
    else
        FAIL("size=0 should not modify buffer");
}

static TEST_FUNC void test_memset_partial_fill(void)
{
    TEST("BAN-154: AIRY_MEMSET partial fill respects size boundary");
    char buf[16];
    AIRY_MEMSET(buf, 0, sizeof(buf)); /* clear all */
    AIRY_MEMSET(buf, 'Y', 8); /* only first 8 bytes */
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        if (buf[i] != 'Y') {
            ok = 0;
            break;
        }
    }
    for (int i = 8; i < 16; i++) {
        if (buf[i] != 0) {
            ok = 0;
            break;
        }
    }
    if (ok)
        PASS();
    else
        FAIL("partial fill boundary incorrect");
}

static TEST_FUNC void test_memcpy_basic_copy(void)
{
    TEST("BAN-154: AIRY_MEMCPY copies string data correctly");
    const char src[] = "Hello, AgentRT!";
    char dst[32] = {0};
    AIRY_MEMCPY(dst, src, sizeof(src));
    if (strcmp(dst, src) == 0)
        PASS();
    else
        FAIL("string copy mismatch");
}

static TEST_FUNC void test_memcpy_zero_size(void)
{
    TEST("BAN-154: AIRY_MEMCPY with size=0 is safe no-op");
    char dst[16] = "original"; /* 16 bytes: "original" (8 chars + null) fits safely */
    const char src[] = "XXXXXXX";
    AIRY_MEMCPY(dst, src, 0);
    if (strcmp(dst, "original") == 0)
        PASS();
    else
        FAIL("size=0 should not modify destination");
}

static TEST_FUNC void test_memcpy_binary_data(void)
{
    TEST("BAN-154: AIRY_MEMCPY handles binary data with embedded nulls");
    const unsigned char src[8] = {0x00, 0x01, 0x02, 0x00, 0x04, 0x05, 0x06, 0x07};
    unsigned char dst[8] = {0};
    AIRY_MEMCPY(dst, src, 8);
    int ok = 1;
    for (int i = 0; i < 8; i++) {
        if (dst[i] != src[i]) {
            ok = 0;
            break;
        }
    }
    if (ok)
        PASS();
    else
        FAIL("binary data copy mismatch");
}

static TEST_FUNC void test_strncpy_term_short_src(void)
{
    TEST("BAN-155: AIRY_STRNCPY_TERM with short src copies correctly");
    char dst[32];
    AIRY_STRNCPY_TERM(dst, "Hi", sizeof(dst));
    if (strcmp(dst, "Hi") == 0 && dst[2] == '\0')
        PASS();
    else
        FAIL("short src copy failed");
}

static TEST_FUNC void test_strncpy_term_long_src_null_termination(void)
{
    TEST("BAN-155: AIRY_STRNCPY_TERM guarantees null termination (long src)");
    char dst[8];
    AIRY_STRNCPY_TERM(dst, "This is a very long string exceeding buffer", sizeof(dst));
    /* Critical: dst must be null-terminated at dst[size-1] */
    if (dst[7] == '\0' && strlen(dst) == 7)
        PASS();
    else
        FAIL("null termination NOT guaranteed — BUFFER OVERFLOW RISK");
}

static TEST_FUNC void test_strncpy_term_exact_fit(void)
{
    TEST("BAN-155: AIRY_STRNCPY_TERM with exact-fit src (dst[size-1]=null)");
    char dst[6]; /* fits "Hello" (5 chars) + null */
    AIRY_STRNCPY_TERM(dst, "Hello", sizeof(dst));
    if (strcmp(dst, "Hello") == 0 && dst[5] == '\0')
        PASS();
    else
        FAIL("exact-fit copy failed");
}

static TEST_FUNC void test_strncpy_term_empty_src(void)
{
    TEST("BAN-155: AIRY_STRNCPY_TERM with empty src produces empty string");
    char dst[8] = "XXXXXXX";
    AIRY_STRNCPY_TERM(dst, "", sizeof(dst));
    if (dst[0] == '\0')
        PASS();
    else
        FAIL("empty src should produce empty string");
}

static TEST_FUNC void test_strncpy_term_single_byte_dst(void)
{
    TEST("BAN-155: AIRY_STRNCPY_TERM with 1-byte dst (null only)");
    char dst[1];
    AIRY_STRNCPY_TERM(dst, "overflow", sizeof(dst));
    if (dst[0] == '\0')
        PASS();
    else
        FAIL("1-byte dst should contain only null terminator");
}

static TEST_FUNC void test_error_codes_values(void)
{
    TEST("BAN-073: Key error codes have correct semantic values");
    /* BAN-073 core requirement: 0 on success, negative on error.
      * Production code must not return bare -1; use these semantic error codes. */
    if (AIRY_OK == 0 && AIRY_ERR_INVALID_PARAM < 0 && AIRY_ERR_NOT_FOUND < 0 &&
        AIRY_ERR_UNKNOWN < 0) {
        PASS();
    } else {
        FAIL("error code values do not match expected semantics");
    }
}

static TEST_FUNC void test_poison_active(void)
{
    TEST("BAN-030/155/154: Poison active under AIRY_COMPLIANCE_STRICT");
    /*
      * This function compiling proves:
      * 1. no poisoned functions are used (strcpy/strncpy/memcpy/memset/malloc/free, etc.)
      * 2. all memory ops go through the safe macros (AIRY_MEMSET/MEMCPY/STRNCPY_TERM)
      * 3. the poison mechanism is active in STRICT mode
     *
      * Using bare strcpy/strncpy/memcpy/memset in STRICT mode
      * fails compilation at #pragma GCC poison; this file cannot be built.
     */
#ifdef AIRY_COMPLIANCE_STRICT
    PASS();
#else
    FAIL("AIRY_COMPLIANCE_STRICT not defined — poison inactive (non-strict build)");
#endif
}

int main(void)
{
    fputs("\n", stdout);
    fputs("================================================\n", stdout);
    fputs("  Banned Functions Compliance Test Suite\n", stdout);
    fputs("  BAN-030 (strcpy) / BAN-155 (strncpy)\n", stdout);
    fputs("  BAN-154 (memcpy/memset) / BAN-073 (return -1)\n", stdout);
    fputs("================================================\n\n", stdout);

    fputs("--- BAN-154: AIRY_MEMSET ---\n", stdout);
    test_memset_basic_fill();
    test_memset_zero_fill();
    test_memset_zero_size();
    test_memset_partial_fill();

    fputs("\n--- BAN-154: AIRY_MEMCPY ---\n", stdout);
    test_memcpy_basic_copy();
    test_memcpy_zero_size();
    test_memcpy_binary_data();

    fputs("\n--- BAN-155: AIRY_STRNCPY_TERM ---\n", stdout);
    test_strncpy_term_short_src();
    test_strncpy_term_long_src_null_termination();
    test_strncpy_term_exact_fit();
    test_strncpy_term_empty_src();
    test_strncpy_term_single_byte_dst();

    fputs("\n--- BAN-073: Error Codes ---\n", stdout);
    test_error_codes_values();

    fputs("\n--- Compile-Time Poison Verification ---\n", stdout);
    test_poison_active();

    fputs("\n================================================\n", stdout);
    test_printf("  Results: %d/%d passed, %d failed\n", g_tests_passed, g_tests_total,
                g_tests_failed);
    fputs("================================================\n\n", stdout);

    return (g_tests_failed > 0) ? 1 : 0;
}
