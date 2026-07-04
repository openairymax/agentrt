/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_error_code_consistency.c - P0.25.6 (ACC-STD06) 错误码一致性测试
 *
 * 验证全项目错误码数值无冲突：
 *   1. cupolas_ERR_* enum 与 AGENTOS_ERR_* commons 宏核心数值一致（语义对齐）
 *   2. cupolas_ERROR_* 别名宏正确映射到 cupolas_ERR_*
 *   3. AGENTOS_ERR_CUPOLAS_* 段（-712~-723）与 AGENTOS_ERR_SEC_* 段（-700~-711）不冲突
 *   4. 各模块错误码段区间互不重叠（通用/系统/内核/服务/LLM/执行/记忆/安全/Cupolas/协调）
 *   5. cupolas_error_string / 转换函数基本可用性
 *
 * 已知差异（非 bug，0.1.1 内保留 cupolas enum 数值不变以避免破坏 ABI）：
 *   - cupolas_ERR_TRY_AGAIN=-15 与 AGENTOS_ERR_WOULD_BLOCK=-18 数值不同
 *     （cupolas_ERROR_WOULD_BLOCK 别名映射到 cupolas_ERR_TRY_AGAIN=-15）
 *     留待 1.0.1+ 统一为 commons 权威值。
 *
 * @author SPHARX Ltd. - Airymax Team
 * @date 2026-07-05
 * @version 1.0  P0.25.6 初始版本
 */

/* 测试目标头文件：cupolas 内部 enum + commons 权威宏 */
#include "../src/security/cupolas_error.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) \
    do { \
        printf("[FAIL] %s: %s\n", name, msg); \
        g_failed++; \
        return; \
    } while (0)

static int g_passed = 0;
static int g_failed = 0;

#define RUN_TEST(func) \
    do { \
        func(); \
        g_passed++; \
    } while (0)

/* ============================================================================
 * 测试 1: cupolas_ERR_* enum 与 AGENTOS_ERR_* commons 宏核心数值一致
 *
 * 设计意图：cupolas_error.h 已 #include "error.h"，cupolas enum 核心数值应与
 * commons 权威宏一致。若数值偏离，则 cupolas 公共 API 返回的错误码会被
 * 调用方误判（如 cupolas_ERR_OUT_OF_MEMORY=-4 必须等于 AGENTOS_ERR_OUT_OF_MEMORY=-4）。
 *
 * 注意：cupolas_ERR_TRY_AGAIN=-15 与 AGENTOS_ERR_WOULD_BLOCK=-18 数值不同
 * （已知差异，留待 1.0.1+ 统一），本测试不检查此项。
 * ============================================================================ */

static void test_cupolas_enum_matches_commons(void)
{
    /* 成功码 */
    if (cupolas_ERR_OK != AGENTOS_OK)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_OK != AGENTOS_OK");

    /* 通用错误码（-1 到 -14）— cupolas enum 与 commons 宏数值对齐 */
    if ((int)cupolas_ERR_UNKNOWN != (int)AGENTOS_ERR_UNKNOWN)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_UNKNOWN mismatch");
    if ((int)cupolas_ERR_INVALID_PARAM != (int)AGENTOS_ERR_INVALID_PARAM)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_INVALID_PARAM mismatch");
    if ((int)cupolas_ERR_NULL_POINTER != (int)AGENTOS_ERR_NULL_POINTER)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_NULL_POINTER mismatch");
    if ((int)cupolas_ERR_OUT_OF_MEMORY != (int)AGENTOS_ERR_OUT_OF_MEMORY)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_OUT_OF_MEMORY mismatch");
    if ((int)cupolas_ERR_BUFFER_TOO_SMALL != (int)AGENTOS_ERR_BUFFER_TOO_SMALL)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_BUFFER_TOO_SMALL mismatch");
    if ((int)cupolas_ERR_NOT_FOUND != (int)AGENTOS_ERR_NOT_FOUND)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_NOT_FOUND mismatch");
    if ((int)cupolas_ERR_ALREADY_EXISTS != (int)AGENTOS_ERR_ALREADY_EXISTS)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_ALREADY_EXISTS mismatch");
    if ((int)cupolas_ERR_TIMEOUT != (int)AGENTOS_ERR_TIMEOUT)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_TIMEOUT mismatch");
    if ((int)cupolas_ERR_NOT_SUPPORTED != (int)AGENTOS_ERR_NOT_SUPPORTED)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_NOT_SUPPORTED mismatch");
    if ((int)cupolas_ERR_PERMISSION_DENIED != (int)AGENTOS_ERR_PERMISSION_DENIED)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_PERMISSION_DENIED mismatch");
    if ((int)cupolas_ERR_IO != (int)AGENTOS_ERR_IO)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_IO mismatch");
    if ((int)cupolas_ERR_STATE_ERROR != (int)AGENTOS_ERR_STATE_ERROR)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_STATE_ERROR mismatch");
    if ((int)cupolas_ERR_OVERFLOW != (int)AGENTOS_ERR_OVERFLOW)
        TEST_FAIL("cupolas_enum_matches_commons", "cupolas_ERR_OVERFLOW mismatch");

    TEST_PASS("cupolas_enum_matches_commons");
}

/* ============================================================================
 * 测试 2: cupolas_ERROR_* 别名宏正确映射到 cupolas_ERR_*
 *
 * 设计意图：cupolas_error.h L280-315 提供 cupolas_ERROR_* 向后兼容别名，
 * 这些别名必须正确映射到 cupolas_ERR_* enum 值。若映射错误，旧代码使用
 * 别名时会得到意外错误码。
 * ============================================================================ */

static void test_cupolas_alias_macros(void)
{
    if ((int)cupolas_OK != (int)cupolas_ERR_OK)
        TEST_FAIL("cupolas_alias_macros", "cupolas_OK mapping error");
    if ((int)cupolas_ERROR_UNKNOWN != (int)cupolas_ERR_UNKNOWN)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_UNKNOWN mapping error");
    if ((int)cupolas_ERROR_INVALID_ARG != (int)cupolas_ERR_INVALID_PARAM)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_INVALID_ARG mapping error");
    if ((int)cupolas_ERROR_NO_MEMORY != (int)cupolas_ERR_OUT_OF_MEMORY)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_NO_MEMORY mapping error");
    if ((int)cupolas_ERROR_NOT_FOUND != (int)cupolas_ERR_NOT_FOUND)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_NOT_FOUND mapping error");
    if ((int)cupolas_ERROR_PERMISSION != (int)cupolas_ERR_PERMISSION_DENIED)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_PERMISSION mapping error");
    if ((int)cupolas_ERROR_BUSY != (int)cupolas_ERR_STATE_ERROR)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_BUSY mapping error");
    if ((int)cupolas_ERROR_TIMEOUT != (int)cupolas_ERR_TIMEOUT)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_TIMEOUT mapping error");
    if ((int)cupolas_ERROR_WOULD_BLOCK != (int)cupolas_ERR_TRY_AGAIN)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_WOULD_BLOCK mapping error");
    if ((int)cupolas_ERROR_OVERFLOW != (int)cupolas_ERR_OVERFLOW)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_OVERFLOW mapping error");
    if ((int)cupolas_ERROR_NOT_SUPPORTED != (int)cupolas_ERR_NOT_SUPPORTED)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_NOT_SUPPORTED mapping error");
    if ((int)cupolas_ERROR_IO != (int)cupolas_ERR_IO)
        TEST_FAIL("cupolas_alias_macros", "cupolas_ERROR_IO mapping error");

    TEST_PASS("cupolas_alias_macros");
}

/* ============================================================================
 * 测试 3: AGENTOS_ERR_CUPOLAS_* 段与 AGENTOS_ERR_SEC_* 段不冲突
 *
 * 设计意图：P0.25.4 新增 AGENTOS_ERR_CUPOLAS_* 段（-712~-723），必须与
 * 已有的 AGENTOS_ERR_SEC_* 段（-700~-711）不重叠。若冲突，调用方无法
 * 区分错误来源是通用安全违规还是 Cupolas 专属决策。
 *
 * 数值关系（负数）：-723 < -712 < -711 < -700
 *   SEC 段：[-711, -700]（lower=-711, upper=-700）
 *   CUPOLAS 段：[-723, -712]（lower=-723, upper=-712）
 *   不重叠条件：CUPOLAS.upper (-712) < SEC.lower (-711)，即 -712 < -711 为 true
 * ============================================================================ */

static void test_cupolas_segment_no_conflict(void)
{
    /* SEC_* 段：upper=-700（最接近 0），lower=-711（最远离 0） */
    int sec_upper = AGENTOS_ERR_SEC_BASE;          /* -700 */
    int sec_lower = AGENTOS_ERR_ESANITIZE;          /* -711 */

    /* CUPOLAS_* 段：upper=-712（最接近 0），lower=-723（最远离 0） */
    int cupolas_upper = AGENTOS_ERR_CUPOLAS_BASE;   /* -712 */
    int cupolas_lower = AGENTOS_ERR_CUPOLAS_NETWORK; /* -723 */

    /* 数值校验 */
    if (sec_upper != -700)
        TEST_FAIL("cupolas_segment_no_conflict", "SEC_BASE should be -700");
    if (sec_lower != -711)
        TEST_FAIL("cupolas_segment_no_conflict", "SEC_ESANITIZE should be -711");
    if (cupolas_upper != -712)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_BASE should be -712");
    if (cupolas_lower != -723)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_NETWORK should be -723");

    /* 段不重叠：CUPOLAS 段上界（-712）< SEC 段下界（-711）
     * 即 -712 < -711 为 true，表示 CUPOLAS 段完全在 SEC 段下方（数值更小） */
    if (!(cupolas_upper < sec_lower))
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS segment overlaps with SEC segment");

    /* CUPOLAS 段内部连续（数值递减：-712, -713, -714, ...） */
    if (AGENTOS_ERR_CUPOLAS_DENIED != -713)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_DENIED should be -713");
    if (AGENTOS_ERR_CUPOLAS_QUARANTINE != -714)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_QUARANTINE should be -714");
    if (AGENTOS_ERR_CUPOLAS_POLICY != -715)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_POLICY should be -715");
    if (AGENTOS_ERR_CUPOLAS_SANDBOX != -716)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_SANDBOX should be -716");
    if (AGENTOS_ERR_CUPOLAS_AUDIT != -717)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_AUDIT should be -717");
    if (AGENTOS_ERR_CUPOLAS_TAMPERED != -718)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_TAMPERED should be -718");
    if (AGENTOS_ERR_CUPOLAS_SIGNATURE != -719)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_SIGNATURE should be -719");
    if (AGENTOS_ERR_CUPOLAS_VAULT != -720)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_VAULT should be -720");
    if (AGENTOS_ERR_CUPOLAS_ENTITLEMENT != -721)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_ENTITLEMENT should be -721");
    if (AGENTOS_ERR_CUPOLAS_RUNTIME != -722)
        TEST_FAIL("cupolas_segment_no_conflict", "CUPOLAS_RUNTIME should be -722");

    TEST_PASS("cupolas_segment_no_conflict");
}

/* ============================================================================
 * 测试 4: 各模块错误码段区间互不重叠
 *
 * 设计意图：commons error.h 定义了 10 个错误码段（通用/系统/内核/服务/LLM/
 * 执行/记忆/安全-SEC/Cupolas/协调），各段区间必须互不重叠。若重叠，调用方
 * 无法通过错误码数值判断错误来源模块。
 *
 * 数值关系（负数）：每段的 upper（最接近 0）> lower（最远离 0）
 * 不重叠条件：前一段的 lower > 后一段的 upper（即前一段完全在后一段上方）
 * ============================================================================ */

static void test_error_segments_disjoint(void)
{
    /* 各段：upper（最接近 0 的值，即段基址）与 lower（最远离 0 的值） */
    struct {
        const char *name;
        int upper;     /* 段基址（最接近 0 的值，数值较大） */
        int lower;     /* 段下界（最远离 0 的值，数值较小） */
    } segments[] = {
        { "GENERIC",  -1,   -31 },   /* 通用基础错误 -1 到 -31 */
        { "SYS",      -100, -113 },  /* 系统与平台错误 -100 到 -113 */
        { "KERN",     -200, -208 },  /* 内核层错误 -200 到 -208 */
        { "SVC",      -300, -307 },  /* 服务层错误 -300 到 -307 */
        { "LLM",      -400, -410 },  /* LLM/AI服务错误 -400 到 -410 */
        { "EXEC",     -500, -508 },  /* 执行/工具错误 -500 到 -508 */
        { "MEM",      -600, -607 },  /* 记忆/存储错误 -600 到 -607 */
        { "SEC",      -700, -711 },  /* 安全/沙箱错误 -700 到 -711 */
        { "CUPOLAS",  -712, -723 },  /* Cupolas 专属错误 -712 到 -723 */
        { "COORD",    -800, -806 },  /* 协调/规划错误 -800 到 -806 */
    };

    const size_t n = sizeof(segments) / sizeof(segments[0]);

    /* 验证各段基址（upper）正确 */
    if (segments[0].upper != AGENTOS_ERR_UNKNOWN)
        TEST_FAIL("error_segments_disjoint", "GENERIC upper mismatch");
    if (segments[1].upper != AGENTOS_ERR_SYS_BASE)
        TEST_FAIL("error_segments_disjoint", "SYS upper mismatch");
    if (segments[2].upper != AGENTOS_ERR_KERN_BASE)
        TEST_FAIL("error_segments_disjoint", "KERN upper mismatch");
    if (segments[3].upper != AGENTOS_ERR_SVC_BASE)
        TEST_FAIL("error_segments_disjoint", "SVC upper mismatch");
    if (segments[4].upper != AGENTOS_ERR_LLM_BASE)
        TEST_FAIL("error_segments_disjoint", "LLM upper mismatch");
    if (segments[5].upper != AGENTOS_ERR_EXEC_BASE)
        TEST_FAIL("error_segments_disjoint", "EXEC upper mismatch");
    if (segments[6].upper != AGENTOS_ERR_MEM_BASE)
        TEST_FAIL("error_segments_disjoint", "MEM upper mismatch");
    if (segments[7].upper != AGENTOS_ERR_SEC_BASE)
        TEST_FAIL("error_segments_disjoint", "SEC upper mismatch");
    if (segments[8].upper != AGENTOS_ERR_CUPOLAS_BASE)
        TEST_FAIL("error_segments_disjoint", "CUPOLAS upper mismatch");
    if (segments[9].upper != AGENTOS_ERR_COORD_BASE)
        TEST_FAIL("error_segments_disjoint", "COORD upper mismatch");

    /* 验证相邻段不重叠：前一段的 lower > 后一段的 upper
     * 即前一段的最小值（最远离 0）仍大于后一段的最大值（最接近 0）
     * 例如：GENERIC.lower=-31 > SYS.upper=-100（-31 > -100 为 true → 不重叠） */
    for (size_t i = 0; i + 1 < n; i++) {
        if (!(segments[i].lower > segments[i + 1].upper)) {
            char msg[128];
            snprintf(msg, sizeof(msg), "segment %s [%d,%d] overlaps with %s [%d,%d]",
                     segments[i].name, segments[i].upper, segments[i].lower,
                     segments[i + 1].name, segments[i + 1].upper, segments[i + 1].lower);
            TEST_FAIL("error_segments_disjoint", msg);
        }
    }

    TEST_PASS("error_segments_disjoint");
}

/* ============================================================================
 * 测试 5: cupolas_error_string / 转换函数基本可用性
 *
 * 设计意图：验证 cupolas 模块的错误码转换函数（cupolas_error_string 等）
 * 对核心错误码返回非空字符串，确保 P0.25 错误码统一未破坏现有 API。
 *
 * 注意：cupolas_ERROR_IS_FATAL(e) 定义为 (e) < cupolas_ERR_OUT_OF_MEMORY
 * （即 e < -4），所以 cupolas_ERR_OUT_OF_MEMORY=-4 本身不是 fatal
 * （可恢复 OOM），只有更严重的错误（如 -5 BUFFER_TOO_SMALL）才是 fatal。
 * ============================================================================ */

static void test_cupolas_error_string_api(void)
{
    const char *str;

    str = cupolas_error_string(cupolas_ERR_OK);
    if (str == NULL)
        TEST_FAIL("cupolas_error_string_api", "string for OK is NULL");

    str = cupolas_error_string(cupolas_ERR_OUT_OF_MEMORY);
    if (str == NULL)
        TEST_FAIL("cupolas_error_string_api", "string for OUT_OF_MEMORY is NULL");

    str = cupolas_error_string(cupolas_ERR_PERMISSION_DENIED);
    if (str == NULL)
        TEST_FAIL("cupolas_error_string_api", "string for PERMISSION_DENIED is NULL");

    /* 转换函数：sig → unified */
    cupolas_error_t err = cupolas_error_from_sig(cupolas_SIG_ERR_UNTRUSTED);
    if (err != cupolas_ERR_PERMISSION_DENIED)
        TEST_FAIL("cupolas_error_string_api", "from_sig(UNTRUSTED) should be PERMISSION_DENIED");

    /* 转换函数：ent → unified */
    err = cupolas_error_from_ent(cupolas_ENT_ERR_DENIED);
    if (err != cupolas_ERR_PERMISSION_DENIED)
        TEST_FAIL("cupolas_error_string_api", "from_ent(DENIED) should be PERMISSION_DENIED");

    /* 工具宏 */
    if (!cupolas_ERROR_IS_SUCCESS(cupolas_ERR_OK))
        TEST_FAIL("cupolas_error_string_api", "IS_SUCCESS(OK) should be true");
    if (cupolas_ERROR_IS_SUCCESS(cupolas_ERR_OUT_OF_MEMORY))
        TEST_FAIL("cupolas_error_string_api", "IS_SUCCESS(OOM) should be false");
    /* IS_FATAL(e) = (e) < cupolas_ERR_OUT_OF_MEMORY，即 e < -4
     * cupolas_ERR_OUT_OF_MEMORY=-4 不是 fatal（可恢复 OOM）
     * cupolas_ERR_BUFFER_TOO_SMALL=-5 是 fatal（严重错误） */
    if (cupolas_ERROR_IS_FATAL(cupolas_ERR_OUT_OF_MEMORY))
        TEST_FAIL("cupolas_error_string_api", "IS_FATAL(OOM) should be false (recoverable)");
    if (!cupolas_ERROR_IS_FATAL(cupolas_ERR_BUFFER_TOO_SMALL))
        TEST_FAIL("cupolas_error_string_api", "IS_FATAL(BUFFER_TOO_SMALL) should be true");
    if (!cupolas_ERROR_IS_PARAM(cupolas_ERR_INVALID_PARAM))
        TEST_FAIL("cupolas_error_string_api", "IS_PARAM(INVALID_PARAM) should be true");

    TEST_PASS("cupolas_error_string_api");
}

/* ============================================================================
 * 主入口
 * ============================================================================ */

int main(void)
{
    printf("=== P0.25.6 Error Code Consistency Tests ===\n");

    RUN_TEST(test_cupolas_enum_matches_commons);
    RUN_TEST(test_cupolas_alias_macros);
    RUN_TEST(test_cupolas_segment_no_conflict);
    RUN_TEST(test_error_segments_disjoint);
    RUN_TEST(test_cupolas_error_string_api);

    printf("\n=== Summary: %d passed, %d failed ===\n", g_passed, g_failed);

    if (g_failed > 0) {
        printf("[FAIL] %d test(s) failed\n", g_failed);
        return 1;
    }
    printf("[PASS] All error code consistency tests passed\n");
    return 0;
}
