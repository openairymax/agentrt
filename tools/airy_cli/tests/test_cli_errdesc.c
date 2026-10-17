// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_errdesc.c
 * @brief L2 呈现层错误文案分诊回归测试（0.1.19 §3.7.2 N1）。
 *
 * 断言 cli_err_desc 对未列明错误按 class 分流：transport 只用于"服务不可
 * 达"，policy/config/internal 各有独立文案，禁止跨 class 合并话术。网关
 * 原文缓冲为空时方可命中分诊路径，故先经 cli_gw_err_take 清零。
 */

#include "cli_gw.h"
#include "cli_render.h"
#include "error_codes.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_known(void)
{
    printf("  test_known...\n");
    assert(strstr(cli_err_desc(AIRY_ERR_TIMEOUT), "超时") != NULL);
    assert(strstr(cli_err_desc(AIRY_ERR_PERMISSION_DENIED), "权限") != NULL);
    printf("    PASSED\n");
}

static void test_transport(void)
{
    const char *d;

    printf("  test_transport...\n");
    d = cli_err_desc(AIRY_ERR_SYS_SOCKET);
    assert(d && d[0] != '\0');
    assert(strstr(d, "不可达") != NULL);
    printf("    PASSED\n");
}

static void test_class_split(void)
{
    const char *p;
    const char *c;
    const char *i;

    printf("  test_class_split...\n");
    p = cli_err_desc(AIRY_ERR_EXEC_SANDBOX);      /* POLICY */
    c = cli_err_desc(AIRY_ERR_SVC_CONFIG);        /* CONFIG */
    i = cli_err_desc(AIRY_ERR_NULL_POINTER);      /* INTERNAL */
    assert(p && strstr(p, "策略") != NULL);
    assert(c && strstr(c, "配置") != NULL);
    assert(i && strstr(i, "内部") != NULL);
    /* 三类文案必须互不相同，且都不回落到 transport 话术 */
    assert(strcmp(p, c) != 0 && strcmp(c, i) != 0 && strcmp(p, i) != 0);
    assert(strstr(p, "不可达") == NULL && strstr(c, "不可达") == NULL &&
           strstr(i, "不可达") == NULL);
    printf("    PASSED\n");
}

static void test_gw_priority(void)
{
    const char *d;

    printf("  test_gw_priority...\n");
    (void)cli_gw_err_take();
    cli_gw_err_set("gateway said no");
    d = cli_err_desc(AIRY_ERR_SYS_SOCKET);
    assert(strcmp(d, "gateway said no") == 0);
    /* 一次性消费：再次取文案须回到 class 分诊 */
    d = cli_err_desc(AIRY_ERR_SYS_SOCKET);
    assert(strstr(d, "不可达") != NULL);
    printf("    PASSED\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  cli_err_desc Unit Tests\n");
    printf("=========================================\n");

    test_known();
    test_transport();
    test_class_split();
    test_gw_priority();

    printf("\nAll cli_err_desc tests PASSED\n");
    return 0;
}
