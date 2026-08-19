// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_smoke.c
 * @brief 构建环境冒烟测试（Smoke Test）
 *
 * 轻量级自包含测试，用于快速验证 AgentRT 合规构建环境是否就绪：
 *
 * 1. AIRY_COMPLIANCE_STRICT 毒化模式已生效（编译期验证）
 * 2. 安全宏 AIRY_MEMCPY / AIRY_MEMSET / AIRY_STRNCPY_TERM 可用且行为正确
 *    （基本拷贝、null 终止保证、size=0 安全 no-op）
 * 3. 核心 Error code 语义正确（AIRY_OK == 0，错误码为负）
 *
 * 自包含说明：
 * - 不链接 airy_common 库（避免 ASan 符号依赖），仅使用 __builtin_* 安全宏
 * - STRICT 模式下 printf/fprintf 被毒化，使用 fputs + vsnprintf 输出
 * - 与 test_banned_functions.c 不同，本测试只做最小可用性验证，便于快速定位
 *   构建环境 / 头文件 / 宏定义层面的基础问题
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
#ifndef AIRY_ERR_INVALID_PARAM
#error "AIRY_ERR_INVALID_PARAM must be defined"
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

static TEST_FUNC void test_env_strict_mode(void)
{
    TEST("smoke: AIRY_COMPLIANCE_STRICT is active");
#ifdef AIRY_COMPLIANCE_STRICT
    PASS();
#else
    FAIL("AIRY_COMPLIANCE_STRICT not defined — poison inactive");
#endif
}

static TEST_FUNC void test_memcpy_basic(void)
{
    TEST("smoke: AIRY_MEMCPY basic copy");
    const char src[] = "smoke";
    char dst[16] = {0};
    AIRY_MEMCPY(dst, src, sizeof(src));
    if (strcmp(dst, src) == 0)
        PASS();
    else
        FAIL("copy mismatch");
}

static TEST_FUNC void test_memset_basic(void)
{
    TEST("smoke: AIRY_MEMSET fills buffer");
    char buf[8];
    AIRY_MEMSET(buf, 0, sizeof(buf));
    int ok = 1;
    for (int i = 0; i < 8; i++)
        if (buf[i] != 0)
            ok = 0;
    if (ok)
        PASS();
    else
        FAIL("buffer not cleared");
}

static TEST_FUNC void test_strncpy_term_terminates(void)
{
    TEST("smoke: AIRY_STRNCPY_TERM guarantees null termination");
    char dst[4];
    AIRY_STRNCPY_TERM(dst, "way too long for this buffer", sizeof(dst));
    if (dst[3] == '\0' && strlen(dst) == 3)
        PASS();
    else
        FAIL("no null terminator — buffer overflow risk");
}

static TEST_FUNC void test_zero_size_noop(void)
{
    TEST("smoke: size=0 is a safe no-op");
    char buf[8] = "keepme";
    AIRY_MEMSET(buf, 0, 0);
    if (strcmp(buf, "keepme") == 0)
        PASS();
    else
        FAIL("size=0 modified the buffer");
}

static TEST_FUNC void test_error_code_semantics(void)
{
    TEST("smoke: error codes (0 success, negative error)");
    if (AIRY_OK == 0 && AIRY_ERR_INVALID_PARAM < 0 && AIRY_ERR_NOT_FOUND < 0)
        PASS();
    else
        FAIL("error code semantics violated");
}

int main(void)
{
    fputs("\n================================================\n", stdout);
    fputs("  AgentRT Build Smoke Test\n", stdout);
    fputs("================================================\n\n", stdout);

    test_env_strict_mode();
    test_memcpy_basic();
    test_memset_basic();
    test_strncpy_term_terminates();
    test_zero_size_noop();
    test_error_code_semantics();

    fputs("\n================================================\n", stdout);
    test_printf("  Results: %d/%d passed, %d failed\n", g_passed, g_total, g_failed);
    fputs("================================================\n\n", stdout);

    return (g_failed > 0) ? 1 : 0;
}
