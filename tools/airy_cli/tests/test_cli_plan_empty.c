// SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0

/**
 * @file test_cli_plan_empty.c
 * @brief C-2 空计划独立用例：零节点计划报「无步骤」而非「内存不足」。
 *
 * cli_dag_submit_remote 在装配 DAG 前即可判定空计划，故不依赖网关。
 */

#include "cli_internal.h"
#include "error_codes.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_null_plan(void)
{
    printf("  test_null_plan...\n");

    char *dag_id = NULL;

    assert(cli_dag_submit_remote(NULL, "t", NULL, &dag_id) == AIRY_ERR_INVALID_PARAM);
    assert(dag_id == NULL);
    printf("    PASSED\n");
}

static void test_zero_nodes(void)
{
    printf("  test_zero_nodes...\n");

    airy_task_plan_t plan;
    char *dag_id = NULL;

    memset(&plan, 0, sizeof(plan));
    airy_err_t rc = cli_dag_submit_remote(&plan, "t", NULL, &dag_id);

    printf("    rc=%d (PLAN_EMPTY=%d, OUT_OF_MEMORY=%d)\n", (int)rc, (int)AIRY_ERR_PLAN_EMPTY,
           (int)AIRY_ERR_OUT_OF_MEMORY);
    assert(rc == AIRY_ERR_PLAN_EMPTY);
    assert(rc != AIRY_ERR_OUT_OF_MEMORY);
    assert(dag_id == NULL);
    printf("    PASSED\n");
}

/* 节点计数非 0 但节点数组缺失：同样是空计划，不得误报内存不足。 */
static void test_missing_nodes_array(void)
{
    printf("  test_missing_nodes_array...\n");

    airy_task_plan_t plan;
    char *dag_id = NULL;

    memset(&plan, 0, sizeof(plan));
    plan.task_plan_node_count = 2;
    plan.task_plan_nodes = NULL;
    airy_err_t rc = cli_dag_submit_remote(&plan, "t", NULL, &dag_id);

    assert(rc == AIRY_ERR_PLAN_EMPTY);
    assert(dag_id == NULL);
    printf("    PASSED\n");
}

int main(void)
{
    printf("=========================================\n");
    printf("  CLI Empty Plan Tests (C-2)\n");
    printf("=========================================\n");

    test_null_plan();
    test_zero_nodes();
    test_missing_nodes_array();

    printf("\nAll CLI empty plan tests PASSED\n");
    return 0;
}
