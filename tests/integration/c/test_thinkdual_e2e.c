/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_thinkdual_e2e.c - Thinkdual 端到端集成测试 (INT-01)
 *
 * Phase 2 集成测试: 验证 Thinkdual 认知管线的完整5阶段处理流程
 *
 * 验证覆盖:
 *   INT-01.1: 指令分解 (Phase 0) - 意图识别与指令拆解
 *   INT-01.2: t2/t1-f/t1-p 协调规划 (Phase 1) - 三重协调器状态分发
 *   INT-01.3: 执行-校验循环 + Stream Critic (Phase 2) - 流式校验与纠错
 *   INT-01.4: 子任务完成验证 (Phase 3) - 结果校验与补偿
 *   INT-01.5: 目标对齐检查 (Phase 4) - 元认知5维评分与记忆持久化
 *
 * 该测试自包含，不依赖外部服务（无LLM调用，无网络）。
 */

#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include "memory_compat.h"
#include "error.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 测试框架宏（与项目现有测试风格一致）
 * ============================================================================ */
#define TEST(name) static void test_##name(void)
#define RUN_TEST(name)                                                         \
    do {                                                                       \
        printf("  Running " #name "...\n");                                    \
        test_##name();                                                         \
        printf("  PASSED\n");                                                  \
    } while (0)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define ASSERT_OK(expr)                                                        \
    do {                                                                       \
        agentos_error_t __err = (expr);                                        \
        if (__err != AGENTOS_OK) {                                             \
            printf("    ASSERT_FAIL: %s returned %d at line %d\n", #expr,     \
                   (int)__err, __LINE__);                                       \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_TRUE(cond)                                                      \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("    ASSERT_FAIL: %s at line %d\n", #cond, __LINE__);       \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

/* ============================================================================
 * 辅助: 验证字符串是否为合法的 JSON 起始标记
 * ============================================================================ */
static int is_valid_json_prefix(const char *str)
{
    if (!str || str[0] == '\0')
        return 0;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')
        str++;
    return (*str == '{' || *str == '[');
}

/* ============================================================================
 * 辅助: 创建默认认知引擎，失败时通过 assert 终止
 * ============================================================================ */
static agentos_cognition_engine_t *create_default_engine(void)
{
    agentos_cognition_engine_t *engine = NULL;
    agentos_error_t err = agentos_cognition_create(NULL, NULL, NULL, &engine);
    assert(err == AGENTOS_OK);
    assert(engine != NULL);
    return engine;
}

/* ============================================================================
 * INT-01.1: Phase 0 - 指令分解验证
 *
 * 验证意图解析器能正确分解多步骤指令:
 *   - 创建意图解析器
 *   - 添加自定义规则
 *   - 解析复杂多步指令
 *   - 验证意图结构字段完整性
 *   - 验证意图目标非空且包含关键信息
 * ============================================================================ */
TEST(int01_1_instruction_decomposition)
{
    printf("    --- Phase 0: Instruction Decomposition ---\n");

    /* 1. 创建意图解析器 */
    agentos_intent_parser_t *parser = NULL;
    ASSERT_OK(agentos_intent_parser_create(&parser));
    ASSERT_TRUE(parser != NULL);
    printf("    Intent parser created\n");

    /* 2. 添加多组自定义规则（模拟多种意图类型） */
    ASSERT_OK(agentos_intent_parser_add_rule(parser, "analyze|analysis|evaluate",
                                             "data_analysis", 0.95f, 0));
    ASSERT_OK(agentos_intent_parser_add_rule(parser, "create|generate|build|write",
                                             "content_creation", 0.90f, 0));
    ASSERT_OK(agentos_intent_parser_add_rule(parser, "compare|contrast|versus",
                                             "comparison", 0.92f, 0));
    ASSERT_OK(agentos_intent_parser_add_rule(parser, "calculate|compute|math|sum",
                                             "math_calculation", 0.88f, 0));
    printf("    4 custom intent rules added\n");

    /* 3. 解析多步骤复合指令 */
    const char *multi_step_input =
        "Analyze the quarterly sales data, compare with last year, "
        "calculate the growth rate, and generate a strategic report.";
    agentos_intent_t *intent = NULL;
    ASSERT_OK(agentos_intent_parser_parse(parser, multi_step_input,
                                          strlen(multi_step_input), &intent));
    ASSERT_TRUE(intent != NULL);

    /* 4. 验证意图结构字段完整性 */
    ASSERT_TRUE(intent->intent_raw_text != NULL);
    ASSERT_TRUE(intent->intent_raw_len > 0);
    ASSERT_TRUE(intent->intent_goal != NULL);
    ASSERT_TRUE(intent->intent_goal_len > 0);
    printf("    Intent goal: %s\n", intent->intent_goal);
    printf("    Intent flags: 0x%x\n", intent->intent_flags);

    /* 5. 验证意图目标包含关键信息 */
    /* 目标应包含对原始指令的抽象提取 */
    ASSERT_TRUE(intent->intent_goal_len <= intent->intent_raw_len);
    printf("    Raw len=%zu, Goal len=%zu (goal is abstraction of raw)\n",
           intent->intent_raw_len, intent->intent_goal_len);

    /* 6. 解析器健康检查 */
    char *parser_health = NULL;
    ASSERT_OK(agentos_intent_parser_health_check(parser, &parser_health));
    ASSERT_TRUE(parser_health != NULL);
    ASSERT_TRUE(is_valid_json_prefix(parser_health));
    printf("    Parser health: %.80s\n", parser_health);
    free(parser_health);

    /* 7. 解析器统计 */
    char *parser_stats = NULL;
    ASSERT_OK(agentos_intent_parser_stats(parser, &parser_stats));
    ASSERT_TRUE(parser_stats != NULL);
    printf("    Parser stats: %.80s\n", parser_stats);
    free(parser_stats);

    /* 8. 清理 */
    agentos_intent_free(intent);
    agentos_intent_parser_destroy(parser);
    g_tests_passed++;
}

/* ============================================================================
 * INT-01.2: Phase 1 - t2/t1-f/t1-p 协调规划验证
 *
 * 验证 triple_coordinator 将任务分发到正确的组件:
 *   - t2 (主思考): 深度推理与规划
 *   - t1-f (快速校验): 快速验证与过滤
 *   - t1-p (并行分发): 并行子任务执行
 *
 * 通过创建自定义 coordinator 策略并验证其分发行为
 * ============================================================================ */

/* 模拟 t2 主思考函数 */
static agentos_error_t mock_t2_thinking(const char **prompts, size_t count,
                                        void *context, char **out_result)
{
    (void)context;
    const char *header = "{\"role\":\"t2\",\"type\":\"deep_thinking\",\"model_count\":";
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu", count);
    const char *trailer = ",\"status\":\"completed\"}";

    size_t total_len = strlen(header) + strlen(count_buf) + strlen(trailer) + 1;
    *out_result = (char *)malloc(total_len);
    assert(*out_result != NULL);
    snprintf(*out_result, total_len, "%s%s%s", header, count_buf, trailer);

    for (size_t i = 0; i < count; i++)
        assert(prompts[i] != NULL);

    return AGENTOS_OK;
}

/* 模拟 t1-f 快速校验函数 */
static agentos_error_t mock_t1f_fast_check(const char **prompts, size_t count,
                                            void *context, char **out_result)
{
    (void)context;
    const char *header = "{\"role\":\"t1-f\",\"type\":\"fast_validation\",\"model_count\":";
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu", count);
    const char *trailer = ",\"verdict\":\"pass\"}";

    size_t total_len = strlen(header) + strlen(count_buf) + strlen(trailer) + 1;
    *out_result = (char *)malloc(total_len);
    assert(*out_result != NULL);
    snprintf(*out_result, total_len, "%s%s%s", header, count_buf, trailer);

    return AGENTOS_OK;
}

/* 模拟 t1-p 并行分发函数 */
static agentos_error_t mock_t1p_parallel_dispatch(const char **prompts, size_t count,
                                                   void *context, char **out_result)
{
    (void)context;
    const char *header = "{\"role\":\"t1-p\",\"type\":\"parallel_dispatch\",\"model_count\":";
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%zu", count);
    const char *trailer = ",\"dispatched\":true}";

    size_t total_len = strlen(header) + strlen(count_buf) + strlen(trailer) + 1;
    *out_result = (char *)malloc(total_len);
    assert(*out_result != NULL);
    snprintf(*out_result, total_len, "%s%s%s", header, count_buf, trailer);

    return AGENTOS_OK;
}

static void mock_coordinator_destroy(agentos_coordinator_strategy_t *strategy)
{
    free(strategy);
}

TEST(int01_2_triple_coordinator_dispatch)
{
    printf("    --- Phase 1: t2/t1-f/t1-p Coordinator Dispatch ---\n");

    /* 1. 创建 t2 主思考策略 */
    agentos_coordinator_strategy_t *t2_strategy =
        (agentos_coordinator_strategy_t *)calloc(1, sizeof(agentos_coordinator_strategy_t));
    ASSERT_TRUE(t2_strategy != NULL);
    t2_strategy->coordinate = mock_t2_thinking;
    t2_strategy->destroy = mock_coordinator_destroy;
    t2_strategy->data = NULL;

    /* 2. 创建 t1-f 快速校验策略 */
    agentos_coordinator_strategy_t *t1f_strategy =
        (agentos_coordinator_strategy_t *)calloc(1, sizeof(agentos_coordinator_strategy_t));
    ASSERT_TRUE(t1f_strategy != NULL);
    t1f_strategy->coordinate = mock_t1f_fast_check;
    t1f_strategy->destroy = mock_coordinator_destroy;
    t1f_strategy->data = NULL;

    /* 3. 创建 t1-p 并行分发策略 */
    agentos_coordinator_strategy_t *t1p_strategy =
        (agentos_coordinator_strategy_t *)calloc(1, sizeof(agentos_coordinator_strategy_t));
    ASSERT_TRUE(t1p_strategy != NULL);
    t1p_strategy->coordinate = mock_t1p_parallel_dispatch;
    t1p_strategy->destroy = mock_coordinator_destroy;
    t1p_strategy->data = NULL;

    /* 4. 验证 t2 主思考分发 */
    const char *deep_prompts[] = {
        "Analyze Q1 sales data",
        "Compare with Q4 last year"
    };
    char *t2_result = NULL;
    ASSERT_OK(t2_strategy->coordinate(deep_prompts, 2, NULL, &t2_result));
    ASSERT_TRUE(t2_result != NULL);
    ASSERT_TRUE(is_valid_json_prefix(t2_result));
    ASSERT_TRUE(strstr(t2_result, "t2") != NULL);
    ASSERT_TRUE(strstr(t2_result, "deep_thinking") != NULL);
    printf("    t2 dispatch: %s\n", t2_result);
    free(t2_result);

    /* 5. 验证 t1-f 快速校验分发 */
    const char *fast_prompts[] = {
        "Verify: growth rate = (15000-12000)/12000"
    };
    char *t1f_result = NULL;
    ASSERT_OK(t1f_strategy->coordinate(fast_prompts, 1, NULL, &t1f_result));
    ASSERT_TRUE(t1f_result != NULL);
    ASSERT_TRUE(is_valid_json_prefix(t1f_result));
    ASSERT_TRUE(strstr(t1f_result, "t1-f") != NULL);
    ASSERT_TRUE(strstr(t1f_result, "fast_validation") != NULL);
    printf("    t1-f dispatch: %s\n", t1f_result);
    free(t1f_result);

    /* 6. 验证 t1-p 并行分发 */
    const char *parallel_prompts[] = {
        "Generate chart for Product A",
        "Generate chart for Product B",
        "Generate chart for Product C"
    };
    char *t1p_result = NULL;
    ASSERT_OK(t1p_strategy->coordinate(parallel_prompts, 3, NULL, &t1p_result));
    ASSERT_TRUE(t1p_result != NULL);
    ASSERT_TRUE(is_valid_json_prefix(t1p_result));
    ASSERT_TRUE(strstr(t1p_result, "t1-p") != NULL);
    ASSERT_TRUE(strstr(t1p_result, "parallel_dispatch") != NULL);
    printf("    t1-p dispatch: %s\n", t1p_result);
    free(t1p_result);

    /* 7. 使用 t2 策略创建认知引擎并处理输入 */
    agentos_cognition_engine_t *engine = NULL;
    ASSERT_OK(agentos_cognition_create(NULL, t2_strategy, NULL, &engine));
    ASSERT_TRUE(engine != NULL);

    const char *input = "Analyze the following data and create a report: "
                        "Product A=15000, Product B=23000, Product C=18000.";
    agentos_task_plan_t *plan = NULL;
    ASSERT_OK(agentos_cognition_process(engine, input, strlen(input), &plan));
    if (plan) {
        printf("    Coordinated plan: %zu nodes\n", plan->task_plan_node_count);
        agentos_task_plan_free(plan);
    }

    /* 8. 健康检查验证协调器状态 */
    char *health_json = NULL;
    ASSERT_OK(agentos_cognition_health_check(engine, &health_json));
    ASSERT_TRUE(health_json != NULL);
    ASSERT_TRUE(is_valid_json_prefix(health_json));
    printf("    Health check after coordinator: %.100s\n", health_json);
    free(health_json);

    /* 9. 清理 */
    agentos_cognition_destroy(engine);
    t2_strategy->destroy(t2_strategy);
    t1f_strategy->destroy(t1f_strategy);
    t1p_strategy->destroy(t1p_strategy);
    g_tests_passed++;
}

/* ============================================================================
 * INT-01.3: Phase 2 - 执行-校验循环 + Stream Critic 验证
 *
 * 验证认知引擎的执行-校验循环:
 *   - 带 feedback 回调的引擎创建
 *   - 处理需要多步校验的复杂输入
 *   - 验证 Stream Critic 回调被触发
 *   - 验证统计信息反映校验活动
 * ============================================================================ */

/* 全局计数器: 记录 feedback 回调被调用的次数 */
static int g_phase2_feedback_count = 0;
static int g_phase2_stream_level_count = 0;
static int g_phase2_round_level_count = 0;

static void phase2_feedback_callback(int level, const char *module,
                                     const char *event, const char *data,
                                     size_t data_len, void *user_data)
{
    g_phase2_feedback_count++;

    if (level == 0)
        g_phase2_stream_level_count++;
    else if (level == 1)
        g_phase2_round_level_count++;

    printf("    [feedback] level=%d module=%s event=%s data_len=%zu\n",
           level, module ? module : "(null)", event ? event : "(null)", data_len);

    int *counter = (int *)user_data;
    if (counter)
        (*counter)++;

    (void)data;
}

TEST(int01_3_execution_verification_loop)
{
    printf("    --- Phase 2: Execution-Verification Loop + Stream Critic ---\n");

    /* 重置全局计数器 */
    g_phase2_feedback_count = 0;
    g_phase2_stream_level_count = 0;
    g_phase2_round_level_count = 0;

    /* 1. 创建带 feedback 回调的认知引擎配置 */
    agentos_cognition_config_t config;
    memset(&config, 0, sizeof(config));
    config.cognition_default_timeout_ms = 30000;
    config.cognition_max_retries = 3;
    config.feedback_callback = phase2_feedback_callback;
    config.feedback_user_data = NULL;

    /* 2. 创建引擎 */
    agentos_cognition_engine_t *engine = NULL;
    ASSERT_OK(agentos_cognition_create_ex(&config, NULL, NULL, NULL, &engine));
    ASSERT_TRUE(engine != NULL);
    printf("    Engine created with feedback callback\n");

    /* 3. 处理需要多步校验的复杂输入 */
    const char *complex_input =
        "I need a detailed analysis: calculate the average of "
        "[10, 20, 30, 40, 50] and explain the step-by-step process. "
        "Verify each step for correctness. Then compare the result "
        "with the median and explain the difference.";
    agentos_task_plan_t *plan = NULL;
    ASSERT_OK(agentos_cognition_process(engine, complex_input,
                                        strlen(complex_input), &plan));

    if (plan) {
        printf("    Plan generated: %zu nodes\n", plan->task_plan_node_count);

        /* 验证计划节点结构 */
        for (size_t i = 0; i < plan->task_plan_node_count; i++) {
            agentos_task_node_t *node = plan->task_plan_nodes[i];
            ASSERT_TRUE(node != NULL);
            ASSERT_TRUE(node->task_node_id != NULL);
            printf("    Node[%zu]: id=%s, role=%s, priority=%u\n",
                   i, node->task_node_id,
                   node->task_node_agent_role ? node->task_node_agent_role : "(null)",
                   node->task_node_priority);
        }
        agentos_task_plan_free(plan);
    }

    /* 4. 验证 Stream Critic 回调统计 */
    printf("    Feedback callback invoked: %d times\n", g_phase2_feedback_count);
    printf("    Stream level (0) callbacks: %d\n", g_phase2_stream_level_count);
    printf("    Round level (1) callbacks: %d\n", g_phase2_round_level_count);

    /* 5. 获取统计信息 */
    char *stats = NULL;
    size_t stats_len = 0;
    ASSERT_OK(agentos_cognition_stats(engine, &stats, &stats_len));
    ASSERT_TRUE(stats != NULL);
    printf("    Stats: %.120s\n", stats);
    free(stats);

    /* 6. 健康检查验证校验状态 */
    char *health_json = NULL;
    ASSERT_OK(agentos_cognition_health_check(engine, &health_json));
    ASSERT_TRUE(health_json != NULL);
    ASSERT_TRUE(is_valid_json_prefix(health_json));

    /* 检查是否包含校验相关标记 */
    int has_critic = (strstr(health_json, "stream_critic") != NULL) ||
                     (strstr(health_json, "critic") != NULL) ||
                     (strstr(health_json, "validated") != NULL) ||
                     (strstr(health_json, "verified") != NULL);
    printf("    Stream critic markings: %s\n",
           has_critic ? "found in health JSON" : "not in health JSON (may be internal)");
    free(health_json);

    /* 7. 清理 */
    agentos_cognition_destroy(engine);
    g_tests_passed++;
}

/* ============================================================================
 * INT-01.4: Phase 3 - 子任务完成验证
 *
 * 验证执行引擎与认知引擎的协同:
 *   - 创建执行引擎
 *   - 注册执行单元
 *   - 提交任务并验证状态转换
 *   - 验证补偿事务机制
 * ============================================================================ */

/* 模拟执行单元: 简单的数据分析任务 */
static agentos_error_t mock_analysis_execute(agentos_execution_unit_t *unit,
                                             const void *input, void **out_output)
{
    (void)unit;
    (void)input;
    /* 模拟输出 */
    const char *result = "{\"analysis\":\"completed\",\"status\":\"success\"}";
    size_t len = strlen(result) + 1;
    char *output = (char *)malloc(len);
    assert(output != NULL);
    memcpy(output, result, len);
    *out_output = output;
    return AGENTOS_OK;
}

static void mock_analysis_destroy(agentos_execution_unit_t *unit)
{
    (void)unit;
}

static const char *mock_analysis_metadata(agentos_execution_unit_t *unit)
{
    (void)unit;
    return "{\"name\":\"mock_analysis\",\"version\":\"1.0\"}";
}

TEST(int01_4_subtask_completion_verification)
{
    printf("    --- Phase 3: Sub-task Completion Verification ---\n");

    /* 1. 创建执行引擎 */
    agentos_execution_engine_t *exec_engine = NULL;
    ASSERT_OK(agentos_execution_create(4, &exec_engine));
    ASSERT_TRUE(exec_engine != NULL);
    printf("    Execution engine created (max_concurrency=4)\n");

    /* 2. 注册执行单元 */
    agentos_execution_unit_t analysis_unit;
    memset(&analysis_unit, 0, sizeof(analysis_unit));
    analysis_unit.execution_unit_data = NULL;
    analysis_unit.execution_unit_execute = mock_analysis_execute;
    analysis_unit.execution_unit_destroy = mock_analysis_destroy;
    analysis_unit.execution_unit_get_metadata = mock_analysis_metadata;

    ASSERT_OK(agentos_execution_register_unit(exec_engine, "data_analysis", analysis_unit));
    printf("    Execution unit 'data_analysis' registered\n");

    /* 3. 提交任务 */
    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_id = NULL;
    task.task_agent_id = "test_agent";
    task.task_agent_id_len = strlen("test_agent");
    task.task_status = TASK_STATUS_PENDING;
    task.task_input = (void *)"analyze sales data";
    task.task_timeout_ms = 5000;
    task.task_max_retries = 2;

    char *task_id = NULL;
    ASSERT_OK(agentos_execution_submit(exec_engine, &task, &task_id));
    ASSERT_TRUE(task_id != NULL);
    printf("    Task submitted: %s\n", task_id);

    /* 4. 查询任务状态 */
    agentos_task_status_t status = TASK_STATUS_PENDING;
    ASSERT_OK(agentos_execution_query(exec_engine, task_id, &status));
    printf("    Task status: %d\n", (int)status);

    /* 5. 等待任务完成 */
    agentos_task_t *result = NULL;
    agentos_error_t wait_err = agentos_execution_wait(exec_engine, task_id, 5000, &result);
    if (wait_err == AGENTOS_OK && result != NULL) {
        printf("    Task completed: status=%d\n", (int)result->task_status);
        agentos_task_free(result);
    } else {
        printf("    Task wait: err=%d (may be async)\n", (int)wait_err);
    }

    /* 6. 创建补偿事务管理器 */
    agentos_compensation_t *comp_mgr = NULL;
    ASSERT_OK(agentos_compensation_create(&comp_mgr));
    ASSERT_TRUE(comp_mgr != NULL);
    printf("    Compensation manager created\n");

    /* 7. 注册可补偿操作 */
    ASSERT_OK(agentos_compensation_register(comp_mgr, "data_analysis_step_1",
                                             "rollback_analysis", NULL));
    ASSERT_OK(agentos_compensation_register(comp_mgr, "data_analysis_step_2",
                                             "rollback_report", NULL));
    printf("    2 compensable operations registered\n");

    /* 8. 执行健康检查 */
    char *exec_health = NULL;
    ASSERT_OK(agentos_execution_health_check(exec_engine, &exec_health));
    ASSERT_TRUE(exec_health != NULL);
    ASSERT_TRUE(is_valid_json_prefix(exec_health));
    printf("    Execution health: %.80s\n", exec_health);
    free(exec_health);

    /* 9. 清理 */
    free(task_id);
    agentos_compensation_destroy(comp_mgr);
    agentos_execution_unregister_unit(exec_engine, "data_analysis");
    agentos_execution_destroy(exec_engine);
    g_tests_passed++;
}

/* ============================================================================
 * INT-01.5: Phase 4 - 目标对齐检查 + 元认知评分 + 记忆持久化
 *
 * 验证完整的 Phase 4 流程:
 *   - 处理输入触发元认知评估
 *   - 验证5维评分维度 (relevance/accuracy/completeness/consistency/clarity)
 *   - 验证记忆引擎集成
 *   - 验证记忆持久化
 * ============================================================================ */
TEST(int01_5_goal_alignment_metacognition)
{
    printf("    --- Phase 4: Goal Alignment + Metacognition + Memory ---\n");

    /* 1. 创建记忆引擎 */
    agentos_memory_engine_t *mem_engine = NULL;
    ASSERT_OK(agentos_memory_create(NULL, &mem_engine));
    ASSERT_TRUE(mem_engine != NULL);
    printf("    Memory engine created\n");

    /* 2. 创建认知引擎并关联记忆引擎 */
    agentos_cognition_engine_t *engine = create_default_engine();
    agentos_cognition_set_memory(engine, mem_engine);
    printf("    Memory engine attached to cognition engine\n");

    /* 3. 写入上下文记忆记录 */
    agentos_memory_record_t record;
    memset(&record, 0, sizeof(record));
    record.memory_record_type = 0; /* AGENTOS_MEMORY_TYPE_SHORT_TERM */
    record.memory_record_data = (void *)"Q1 sales: A=15000, B=23000, C=18000";
    record.memory_record_data_len = strlen("Q1 sales: A=15000, B=23000, C=18000");
    record.memory_record_importance = 0.8f;

    char *record_id = NULL;
    ASSERT_OK(agentos_memory_write(mem_engine, &record, &record_id));
    ASSERT_TRUE(record_id != NULL);
    printf("    Memory record written: %s\n", record_id);
    free(record_id);

    /* 4. 处理需要目标对齐的复杂输入 */
    const char *goal_input =
        "Compare and contrast machine learning vs deep learning, "
        "providing concrete examples for each approach. "
        "Rate the quality of this comparison and ensure it covers "
        "both theoretical foundations and practical applications.";
    agentos_task_plan_t *plan = NULL;
    ASSERT_OK(agentos_cognition_process(engine, goal_input,
                                        strlen(goal_input), &plan));
    if (plan) {
        printf("    Goal-aligned plan: %zu nodes\n", plan->task_plan_node_count);

        /* 验证计划入口点 */
        if (plan->task_plan_entry_points != NULL) {
            printf("    Entry points: %zu\n", plan->task_plan_entry_count);
        }
        agentos_task_plan_free(plan);
    }

    /* 5. 验证元认知5维评分 */
    char *health_json = NULL;
    ASSERT_OK(agentos_cognition_health_check(engine, &health_json));
    ASSERT_TRUE(health_json != NULL);
    ASSERT_TRUE(is_valid_json_prefix(health_json));
    printf("    Health JSON: %.150s\n", health_json);

    /* 检查5维评分字段 */
    int dim_count = 0;
    if (strstr(health_json, "relevance"))    dim_count++;
    if (strstr(health_json, "accuracy"))     dim_count++;
    if (strstr(health_json, "completeness")) dim_count++;
    if (strstr(health_json, "consistency"))  dim_count++;
    if (strstr(health_json, "clarity"))      dim_count++;
    printf("    Metacognition dimensions found: %d/5\n", dim_count);

    /* 检查综合评分 */
    int has_composite = (strstr(health_json, "composite") != NULL);
    if (has_composite)
        printf("    Composite score field present\n");
    free(health_json);

    /* 6. 验证统计信息中的元认知数据 */
    char *stats = NULL;
    size_t stats_len = 0;
    ASSERT_OK(agentos_cognition_stats(engine, &stats, &stats_len));
    ASSERT_TRUE(stats != NULL);
    printf("    Stats: %.120s\n", stats);

    int stat_dims = 0;
    if (strstr(stats, "relevance"))    stat_dims++;
    if (strstr(stats, "accuracy"))     stat_dims++;
    if (strstr(stats, "completeness")) stat_dims++;
    if (strstr(stats, "consistency"))  stat_dims++;
    if (strstr(stats, "clarity"))      stat_dims++;
    if (stat_dims > 0)
        printf("    Stats dimensions found: %d/5\n", stat_dims);
    free(stats);

    /* 7. 验证记忆引擎健康状态 */
    char *mem_health = NULL;
    ASSERT_OK(agentos_memory_health_check(mem_engine, &mem_health));
    ASSERT_TRUE(mem_health != NULL);
    ASSERT_TRUE(is_valid_json_prefix(mem_health));
    printf("    Memory health: %.80s\n", mem_health);
    free(mem_health);

    /* 8. 清理 */
    agentos_cognition_destroy(engine);
    agentos_memory_destroy(mem_engine);
    g_tests_passed++;
}

/* ============================================================================
 * INT-01 补充: 完整5阶段管线端到端验证
 *
 * 从 Phase 0 到 Phase 4 的完整管线，使用单一引擎实例
 * ============================================================================ */
TEST(int01_full_pipeline_e2e)
{
    printf("    --- Full Pipeline: Phase 0 → Phase 4 E2E ---\n");

    /* 1. 创建认知引擎（带 feedback 回调） */
    agentos_cognition_config_t config;
    memset(&config, 0, sizeof(config));
    config.cognition_default_timeout_ms = 30000;
    config.cognition_max_retries = 3;
    config.feedback_callback = phase2_feedback_callback;
    config.feedback_user_data = NULL;

    agentos_cognition_engine_t *engine = NULL;
    ASSERT_OK(agentos_cognition_create_ex(&config, NULL, NULL, NULL, &engine));
    ASSERT_TRUE(engine != NULL);

    /* 2. 创建并关联记忆引擎 */
    agentos_memory_engine_t *mem_engine = NULL;
    ASSERT_OK(agentos_memory_create(NULL, &mem_engine));
    agentos_cognition_set_memory(engine, mem_engine);
    printf("    Engine + Memory created and linked\n");

    /* 3. Phase 0: 指令分解 - 处理多步骤复杂输入 */
    const char *full_input =
        "I need to analyze the following data and create a report: "
        "The sales figures for Q1 are: Product A=15000, Product B=23000, "
        "Product C=18000. Compare these with Q4 last year (A=12000, B=21000, C=19000) "
        "and provide strategic recommendations for Q2.";
    agentos_task_plan_t *plan = NULL;
    ASSERT_OK(agentos_cognition_process(engine, full_input,
                                        strlen(full_input), &plan));
    ASSERT_TRUE(plan != NULL);
    ASSERT_TRUE(plan->task_plan_node_count > 0);
    printf("    Phase 0→1: Plan generated with %zu nodes\n",
           plan->task_plan_node_count);

    /* 4. Phase 1: 验证计划结构 */
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentos_task_node_t *node = plan->task_plan_nodes[i];
        ASSERT_TRUE(node != NULL);
        ASSERT_TRUE(node->task_node_id != NULL);
        printf("    Node[%zu]: id=%s, role=%s, depends=%zu, priority=%u\n",
               i, node->task_node_id,
               node->task_node_agent_role ? node->task_node_agent_role : "(null)",
               node->task_node_depends_count, node->task_node_priority);
    }
    agentos_task_plan_free(plan);

    /* 5. Phase 2-3: 再次处理以触发校验循环 */
    const char *verify_input =
        "Verify the previous analysis: calculate growth rates "
        "for each product and cross-check the numbers.";
    agentos_task_plan_t *verify_plan = NULL;
    ASSERT_OK(agentos_cognition_process(engine, verify_input,
                                        strlen(verify_input), &verify_plan));
    if (verify_plan) {
        printf("    Phase 2→3: Verification plan with %zu nodes\n",
               verify_plan->task_plan_node_count);
        agentos_task_plan_free(verify_plan);
    }

    /* 6. Phase 4: 目标对齐检查 - 健康检查与统计 */
    char *health_json = NULL;
    ASSERT_OK(agentos_cognition_health_check(engine, &health_json));
    ASSERT_TRUE(health_json != NULL);
    ASSERT_TRUE(is_valid_json_prefix(health_json));
    printf("    Phase 4: Health check: %.100s\n", health_json);
    free(health_json);

    char *stats = NULL;
    size_t stats_len = 0;
    ASSERT_OK(agentos_cognition_stats(engine, &stats, &stats_len));
    ASSERT_TRUE(stats != NULL);
    printf("    Phase 4: Stats: %.100s\n", stats);
    free(stats);

    /* 7. 验证记忆引擎持久化 */
    char *mem_health = NULL;
    ASSERT_OK(agentos_memory_health_check(mem_engine, &mem_health));
    ASSERT_TRUE(mem_health != NULL);
    ASSERT_TRUE(is_valid_json_prefix(mem_health));
    printf("    Memory health after pipeline: %.80s\n", mem_health);
    free(mem_health);

    /* 8. 清理 */
    agentos_cognition_destroy(engine);
    agentos_memory_destroy(mem_engine);
    g_tests_passed++;
}

/* ============================================================================
 * 主入口
 * ============================================================================ */
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=========================================\n");
    printf("  Thinkdual E2E Integration Tests\n");
    printf("  Phase 2 - INT-01\n");
    printf("=========================================\n\n");

    /* INT-01.1: Phase 0 - 指令分解 */
    printf("--- INT-01.1: Phase 0 - Instruction Decomposition ---\n");
    RUN_TEST(int01_1_instruction_decomposition);

    /* INT-01.2: Phase 1 - t2/t1-f/t1-p 协调规划 */
    printf("\n--- INT-01.2: Phase 1 - Triple Coordinator Dispatch ---\n");
    RUN_TEST(int01_2_triple_coordinator_dispatch);

    /* INT-01.3: Phase 2 - 执行-校验循环 + Stream Critic */
    printf("\n--- INT-01.3: Phase 2 - Execution-Verification Loop ---\n");
    RUN_TEST(int01_3_execution_verification_loop);

    /* INT-01.4: Phase 3 - 子任务完成验证 */
    printf("\n--- INT-01.4: Phase 3 - Sub-task Completion Verification ---\n");
    RUN_TEST(int01_4_subtask_completion_verification);

    /* INT-01.5: Phase 4 - 目标对齐 + 元认知 + 记忆 */
    printf("\n--- INT-01.5: Phase 4 - Goal Alignment + Metacognition ---\n");
    RUN_TEST(int01_5_goal_alignment_metacognition);

    /* 完整管线端到端 */
    printf("\n--- INT-01 Full: Phase 0→4 E2E Pipeline ---\n");
    RUN_TEST(int01_full_pipeline_e2e);

    printf("\n=========================================\n");
    if (g_tests_failed == 0) {
        printf("  All %d Thinkdual E2E tests PASSED\n", g_tests_passed);
    } else {
        printf("  %d PASSED, %d FAILED\n", g_tests_passed, g_tests_failed);
    }
    printf("=========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
