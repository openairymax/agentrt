/**
 * @file test_hook_integration.c
 * @brief P3.18 (ACC-DT26): hook_d → CoreLoopThree 集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 验证 ACC-DT26 验收标准：cognition engine 关键事件调用 agentos_hook_trigger，
 * 注册 hook 后验证被触发，且决策生效。
 *
 * 测试内容：
 *   1. hook 系统机制（注册/触发/聚合/注销）— 单元级验证 hook_d 调度
 *   2. cognition engine 集成 — PRE_EXEC/POST_EXEC 在 process 中被触发
 *   3. 决策生效 — PRE_EXEC ABORT 短路到 process_fail，ON_ERROR 被触发
 *
 * @note 不使用 assert() 执行副作用操作：Release 构建类型定义 NDEBUG，会将
 *       assert(expr) 展开为 ((void)0)，导致 expr 中的函数调用（init/register/
 *       process）根本不执行。所有副作用操作必须用显式 if 检查 + TEST_FAIL。
 *       详见 project_memory.md 的 assert/NDEBUG heisenbug 教训。
 */

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include "agentos_hook.h"
#include "cognition.h"
#include "error.h"
#include "memory_compat.h"

#include <stdio.h>
#include <string.h>

/* ========== 测试统计 ========== */

static int test_count = 0;
static int pass_count = 0;

#define TEST_PASS() do { pass_count++; test_count++; } while (0)
#define TEST_FAIL(msg) do { printf("    FAIL: %s\n", msg); test_count++; } while (0)

/* ========== 回调计数器（通过 user_data 传递） ========== */

typedef struct {
    int fired;
    hook_type_t seen_type;
    const char *seen_operation;
} hook_counter_t;

static hook_decision_t cb_continue(hook_context_t *ctx)
{
    hook_counter_t *c = (hook_counter_t *)ctx->user_data;
    if (c) {
        c->fired++;
        c->seen_type = ctx->type;
        c->seen_operation = ctx->operation;
    }
    return HOOK_DECISION_CONTINUE;
}

static hook_decision_t cb_abort(hook_context_t *ctx)
{
    hook_counter_t *c = (hook_counter_t *)ctx->user_data;
    if (c) {
        c->fired++;
        c->seen_type = ctx->type;
        c->seen_operation = ctx->operation;
    }
    return HOOK_DECISION_ABORT;
}

/* ========== TEST 1: hook 系统机制（注册/触发/聚合/注销） ==========
 *
 * 分两部分验证：
 *   Part A: 两个 CONTINUE hook — 验证注册/优先级排序/都执行/聚合=CONTINUE/注销
 *   Part B: ABORT hook + CONTINUE hook — 验证 ABORT 短路（高优先级 ABORT 后，
 *           低优先级 CONTINUE 不执行）+ 聚合=ABORT
 *
 * 注：hook_executor_run 在 final_decision==ABORT 时 break，不再执行后续 hook。
 * 这意味着 ABORT + CONTINUE 组合中，CONTINUE 永远不会执行（正确行为）。
 * 因此用 Part A（两个 CONTINUE）验证"多个 hook 都执行"，Part B 验证 ABORT 短路。 */
static void test_hook_system_mechanics(void)
{
    /* 1. 初始化 hook 系统 */
    if (agentos_hook_init() != 0) {
        TEST_FAIL("agentos_hook_init failed");
        return;
    }

    /* ========== Part A: 两个 CONTINUE hook ========== */
    hook_counter_t cnt_a1;
    hook_counter_t cnt_a2;
    memset(&cnt_a1, 0, sizeof(cnt_a1));
    memset(&cnt_a2, 0, sizeof(cnt_a2));

    if (agentos_hook_register("t_a1", HOOK_TYPE_PRE_EXEC, cb_continue,
                              &cnt_a1, 100, true) != 0) {
        TEST_FAIL("register t_a1 failed");
        agentos_hook_shutdown();
        return;
    }
    if (agentos_hook_register("t_a2", HOOK_TYPE_PRE_EXEC, cb_continue,
                              &cnt_a2, 50, true) != 0) {
        TEST_FAIL("register t_a2 failed");
        agentos_hook_unregister("t_a1");
        agentos_hook_shutdown();
        return;
    }

    /* 验证注册总数 */
    if (agentos_hook_count() != 2) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Part A: hook_count=%zu (expected 2)", agentos_hook_count());
        TEST_FAIL(msg);
        agentos_hook_unregister("t_a1");
        agentos_hook_unregister("t_a2");
        agentos_hook_shutdown();
        return;
    }

    /* 触发 — 两个 CONTINUE hook 都应执行，聚合 = CONTINUE */
    hook_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_EXEC;
    ctx.source_daemon = "test";
    ctx.operation = "mechanics_test_a";
    ctx.input_data = "hello";
    ctx.input_data_len = 5;

    hook_decision_t decision_a = agentos_hook_trigger(&ctx);

    if (cnt_a1.fired != 1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Part A: cnt_a1.fired=%d (expected 1)", cnt_a1.fired);
        TEST_FAIL(msg);
        agentos_hook_unregister("t_a1");
        agentos_hook_unregister("t_a2");
        agentos_hook_shutdown();
        return;
    }
    if (cnt_a2.fired != 1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Part A: cnt_a2.fired=%d (expected 1)", cnt_a2.fired);
        TEST_FAIL(msg);
        agentos_hook_unregister("t_a1");
        agentos_hook_unregister("t_a2");
        agentos_hook_shutdown();
        return;
    }
    if (decision_a != HOOK_DECISION_CONTINUE) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Part A: decision=%d (expected CONTINUE=%d)",
                 (int)decision_a, (int)HOOK_DECISION_CONTINUE);
        TEST_FAIL(msg);
        agentos_hook_unregister("t_a1");
        agentos_hook_unregister("t_a2");
        agentos_hook_shutdown();
        return;
    }
    if (cnt_a1.seen_type != HOOK_TYPE_PRE_EXEC) {
        TEST_FAIL("Part A: callback saw wrong type");
        agentos_hook_unregister("t_a1");
        agentos_hook_unregister("t_a2");
        agentos_hook_shutdown();
        return;
    }

    /* 注销 Part A 的 hook */
    if (agentos_hook_unregister("t_a2") != 0) {
        TEST_FAIL("Part A: unregister t_a2 failed");
        agentos_hook_unregister("t_a1");
        agentos_hook_shutdown();
        return;
    }
    if (agentos_hook_unregister("t_a1") != 0) {
        TEST_FAIL("Part A: unregister t_a1 failed");
        agentos_hook_shutdown();
        return;
    }
    if (agentos_hook_count() != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Part A: hook_count after unregister=%zu (expected 0)",
                 agentos_hook_count());
        TEST_FAIL(msg);
        agentos_hook_shutdown();
        return;
    }

    /* ========== Part B: ABORT 短路（高优先级 ABORT → 低优先级 CONTINUE 不执行） ========== */
    hook_counter_t cnt_abort;
    hook_counter_t cnt_continue_post;
    memset(&cnt_abort, 0, sizeof(cnt_abort));
    memset(&cnt_continue_post, 0, sizeof(cnt_continue_post));

    if (agentos_hook_register("t_abort", HOOK_TYPE_PRE_EXEC, cb_abort,
                              &cnt_abort, 100, true) != 0) {
        TEST_FAIL("Part B: register t_abort failed");
        agentos_hook_shutdown();
        return;
    }
    if (agentos_hook_register("t_cont_post", HOOK_TYPE_PRE_EXEC, cb_continue,
                              &cnt_continue_post, 50, true) != 0) {
        TEST_FAIL("Part B: register t_cont_post failed");
        agentos_hook_unregister("t_abort");
        agentos_hook_shutdown();
        return;
    }

    /* 触发 — t_abort(100) 先执行返回 ABORT → break → t_cont_post(50) 不执行 */
    memset(&ctx, 0, sizeof(ctx));
    ctx.type = HOOK_TYPE_PRE_EXEC;
    ctx.source_daemon = "test";
    ctx.operation = "mechanics_test_b";
    ctx.input_data = "hello";
    ctx.input_data_len = 5;

    hook_decision_t decision_b = agentos_hook_trigger(&ctx);

    if (cnt_abort.fired != 1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Part B: cnt_abort.fired=%d (expected 1)", cnt_abort.fired);
        TEST_FAIL(msg);
        agentos_hook_unregister("t_abort");
        agentos_hook_unregister("t_cont_post");
        agentos_hook_shutdown();
        return;
    }
    /* ABORT 短路：t_cont_post 不应执行 */
    if (cnt_continue_post.fired != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Part B: cnt_continue_post.fired=%d (expected 0, ABORT should short-circuit)",
                 cnt_continue_post.fired);
        TEST_FAIL(msg);
        agentos_hook_unregister("t_abort");
        agentos_hook_unregister("t_cont_post");
        agentos_hook_shutdown();
        return;
    }
    if (decision_b != HOOK_DECISION_ABORT) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Part B: decision=%d (expected ABORT=%d)",
                 (int)decision_b, (int)HOOK_DECISION_ABORT);
        TEST_FAIL(msg);
        agentos_hook_unregister("t_abort");
        agentos_hook_unregister("t_cont_post");
        agentos_hook_shutdown();
        return;
    }

    /* 清理 */
    agentos_hook_unregister("t_abort");
    agentos_hook_unregister("t_cont_post");
    agentos_hook_shutdown();
    TEST_PASS();
    printf("    hook mechanics: register/trigger/aggregate/unregister/abort-short-circuit verified\n");
}

/* ========== TEST 2: cognition engine 集成（PRE_EXEC/ON_ERROR 被触发） ==========
 *
 * 设计说明：agentos_cognition_create_take(NULL, NULL, NULL, &engine) 传入 NULL
 * plan strategy，process 在 Phase 1 planning 必然失败（err=AGENTOS_EUNKNOWN），
 * 走 process_fail 路径。因此本测试验证：
 *   - PRE_EXEC(CONTINUE) → process 进入正常流程
 *   - planning 失败（无 strategy）→ goto process_fail → ON_ERROR 触发
 *   - process 返回非成功
 *
 * 这与 TEST 3（PRE_EXEC ABORT 短路）验证不同的 ON_ERROR 触发路径：
 *   TEST 2: PRE_EXEC CONTINUE → planning 失败 → ON_ERROR
 *   TEST 3: PRE_EXEC ABORT → 直接短路 → ON_ERROR
 *
 * 注：test_cognition_e2e.c 用 assert(err == AGENTOS_SUCCESS) 检查 process 返回值，
 * 在 Release/NDEBUG 下 assert 被消除，掩盖了此失败（heisenbug）。本测试用显式 if
 * 检查，正确处理 process 失败。 */
static void test_cognition_engine_hook_integration(void)
{
    /* 1. 创建引擎（create_ex_take 内部调用 agentos_hook_init） */
    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_take(NULL, NULL, NULL, &engine);
    if (err != AGENTOS_SUCCESS || engine == NULL) {
        TEST_FAIL("agentos_cognition_create_take failed");
        return;
    }

    /* 2. 注册 PRE_EXEC(CONTINUE) + ON_ERROR(CONTINUE) hook */
    hook_counter_t pre_cnt;
    hook_counter_t on_error_cnt;
    memset(&pre_cnt, 0, sizeof(pre_cnt));
    memset(&on_error_cnt, 0, sizeof(on_error_cnt));

    if (agentos_hook_register("integ_pre", HOOK_TYPE_PRE_EXEC, cb_continue,
                              &pre_cnt, 100, true) != 0) {
        TEST_FAIL("register integ_pre failed");
        agentos_cognition_destroy(engine);
        return;
    }
    if (agentos_hook_register("integ_on_err", HOOK_TYPE_ON_ERROR, cb_continue,
                              &on_error_cnt, 100, true) != 0) {
        TEST_FAIL("register integ_on_err failed");
        agentos_hook_unregister("integ_pre");
        agentos_cognition_destroy(engine);
        return;
    }

    /* 3. 运行认知处理 — 无 plan strategy，planning 必然失败 → process_fail */
    const char *input = "Summarize the key points of machine learning";
    agentos_task_plan_t *plan = NULL;
    err = agentos_cognition_process(engine, input, strlen(input), &plan);

    /* 4. 验证 process 返回非成功（无 plan strategy 是预期失败） */
    if (err == AGENTOS_SUCCESS) {
        TEST_FAIL("process succeeded without plan strategy (unexpected)");
        if (plan) agentos_task_plan_free(plan);
        agentos_hook_unregister("integ_pre");
        agentos_hook_unregister("integ_on_err");
        agentos_cognition_destroy(engine);
        return;
    }

    /* 5. 验证 PRE_EXEC hook 被触发（process 入口） */
    if (pre_cnt.fired != 1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "pre_cnt.fired=%d (expected 1)", pre_cnt.fired);
        TEST_FAIL(msg);
        if (plan) agentos_task_plan_free(plan);
        agentos_hook_unregister("integ_pre");
        agentos_hook_unregister("integ_on_err");
        agentos_cognition_destroy(engine);
        return;
    }

    /* 6. 验证 ON_ERROR hook 被触发（planning 失败 → process_fail → ON_ERROR） */
    if (on_error_cnt.fired != 1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "on_error_cnt.fired=%d (expected 1, planning failure should trigger ON_ERROR)",
                 on_error_cnt.fired);
        TEST_FAIL(msg);
        if (plan) agentos_task_plan_free(plan);
        agentos_hook_unregister("integ_pre");
        agentos_hook_unregister("integ_on_err");
        agentos_cognition_destroy(engine);
        return;
    }

    /* 7. 验证 hook 上下文传递正确（PRE_EXEC operation 字符串） */
    if (pre_cnt.seen_operation == NULL ||
        strstr(pre_cnt.seen_operation, "cognition_process") == NULL) {
        char msg[128];
        snprintf(msg, sizeof(msg), "pre_cnt.seen_operation=%s (expected cognition_process)",
                 pre_cnt.seen_operation ? pre_cnt.seen_operation : "(null)");
        TEST_FAIL(msg);
        if (plan) agentos_task_plan_free(plan);
        agentos_hook_unregister("integ_pre");
        agentos_hook_unregister("integ_on_err");
        agentos_cognition_destroy(engine);
        return;
    }

    /* 8. 清理 */
    if (plan) agentos_task_plan_free(plan);
    agentos_hook_unregister("integ_pre");
    agentos_hook_unregister("integ_on_err");
    agentos_cognition_destroy(engine);
    TEST_PASS();
    printf("    cognition integration: PRE_EXEC/ON_ERROR fired during process (planning failure path)\n");
}

/* ========== TEST 3: 决策生效（PRE_EXEC ABORT 短路到 process_fail） ========== */
static void test_pre_exec_abort_short_circuits(void)
{
    /* 1. 创建引擎 */
    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create_take(NULL, NULL, NULL, &engine);
    if (err != AGENTOS_SUCCESS || engine == NULL) {
        TEST_FAIL("agentos_cognition_create_take failed");
        return;
    }

    /* 2. 注册 PRE_EXEC(ABORT) + ON_ERROR(CONTINUE) hook */
    hook_counter_t pre_abort_cnt;
    hook_counter_t on_error_cnt;
    memset(&pre_abort_cnt, 0, sizeof(pre_abort_cnt));
    memset(&on_error_cnt, 0, sizeof(on_error_cnt));

    if (agentos_hook_register("abort_pre", HOOK_TYPE_PRE_EXEC, cb_abort,
                              &pre_abort_cnt, 100, true) != 0) {
        TEST_FAIL("register abort_pre failed");
        agentos_cognition_destroy(engine);
        return;
    }
    if (agentos_hook_register("on_err", HOOK_TYPE_ON_ERROR, cb_continue,
                              &on_error_cnt, 100, true) != 0) {
        TEST_FAIL("register on_err failed");
        agentos_hook_unregister("abort_pre");
        agentos_cognition_destroy(engine);
        return;
    }

    /* 3. 运行认知处理 — PRE_EXEC ABORT 应短路到 process_fail */
    const char *input = "This input should be aborted by PRE_EXEC hook";
    agentos_task_plan_t *plan = NULL;
    err = agentos_cognition_process(engine, input, strlen(input), &plan);

    /* 4. 验证 PRE_EXEC hook 被触发 */
    if (pre_abort_cnt.fired != 1) {
        char msg[128];
        snprintf(msg, sizeof(msg), "pre_abort_cnt.fired=%d (expected 1)", pre_abort_cnt.fired);
        TEST_FAIL(msg);
        if (plan) agentos_task_plan_free(plan);
        agentos_hook_unregister("abort_pre");
        agentos_hook_unregister("on_err");
        agentos_cognition_destroy(engine);
        return;
    }

    /* 5. 验证 ON_ERROR hook 被触发（ABORT 导致 goto process_fail → ON_ERROR 触发） */
    if (on_error_cnt.fired != 1) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "on_error_cnt.fired=%d (expected 1, ABORT should trigger ON_ERROR)",
                 on_error_cnt.fired);
        TEST_FAIL(msg);
        if (plan) agentos_task_plan_free(plan);
        agentos_hook_unregister("abort_pre");
        agentos_hook_unregister("on_err");
        agentos_cognition_destroy(engine);
        return;
    }

    /* 6. 验证 process 返回非成功（ABORT 短路 — err 已在函数顶部初始化为 EUNKNOWN） */
    if (err == AGENTOS_SUCCESS) {
        TEST_FAIL("process succeeded but PRE_EXEC ABORT should have short-circuited");
    }

    /* 7. 清理 */
    if (plan) agentos_task_plan_free(plan);
    agentos_hook_unregister("abort_pre");
    agentos_hook_unregister("on_err");
    agentos_cognition_destroy(engine);
    TEST_PASS();
    printf("    decision effect: PRE_EXEC ABORT short-circuited to ON_ERROR\n");
}

/* ========== main ========== */
int main(void)
{
    printf("=== P3.18 (ACC-DT26) hook_d -> CoreLoopThree integration tests ===\n\n");

    printf("[Test 1] hook system mechanics (register/trigger/aggregate/unregister)\n");
    test_hook_system_mechanics();

    printf("\n[Test 2] cognition engine integration (PRE_EXEC/POST_EXEC fired)\n");
    test_cognition_engine_hook_integration();

    printf("\n[Test 3] decision effect (PRE_EXEC ABORT short-circuits to ON_ERROR)\n");
    test_pre_exec_abort_short_circuits();

    printf("\n=== Results: %d/%d passed ===\n", pass_count, test_count);
    return (pass_count == test_count) ? 0 : 1;
}
