/**
 * @file test_ipc_backpressure.c
 * @brief P1.24.4: IPC Bus 背压控制单元测试
 *
 * 测试场景：
 *   1. 创建/销毁背压控制器
 *   2. 默认配置验证
 *   3. 自定义配置验证
 *   4. 三级背压策略：NORMAL → SLOW → DROP → REJECT
 *   5. 背压恢复：REJECT → DROP → SLOW → NORMAL
 *   6. should_send 在各级别下的行为
 *   7. should_accept_connection 在各级别下的行为
 *   8. 统计信息正确性
 *   9. 模拟队列满 → 生产者降速 → 消费者消费 → 恢复
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc_backpressure.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* ==================== 测试框架 ==================== */

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_BEGIN(name) \
    do { printf("  %s...\n", name); } while (0)

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            printf("    FAIL: %s (line %d)\n", msg, __LINE__); \
            g_tests_failed++; \
            return; \
        } \
    } while (0)

#define TEST_END() \
    do { printf("    PASSED\n"); g_tests_passed++; } while (0)

/* ==================== 测试用例 ==================== */

/**
 * 测试 1: 创建和销毁背压控制器
 */
static void test_bp_create_destroy(void)
{
    TEST_BEGIN("test_bp_create_destroy");

    /* 默认配置创建 */
    ipc_bp_controller_t *ctrl = ipc_bp_create(NULL);
    TEST_ASSERT(ctrl != NULL, "ipc_bp_create(NULL) should succeed");

    /* 销毁 */
    ipc_bp_destroy(ctrl);

    /* NULL 安全销毁 */
    ipc_bp_destroy(NULL);

    TEST_END();
}

/**
 * 测试 2: 默认配置验证
 */
static void test_bp_default_config(void)
{
    TEST_BEGIN("test_bp_default_config");

    ipc_bp_controller_t *ctrl = ipc_bp_create(NULL);
    TEST_ASSERT(ctrl != NULL, "create failed");

    ipc_bp_stats_t stats;
    ipc_bp_get_stats(ctrl, &stats);

    /* 默认状态应为 NORMAL */
    TEST_ASSERT(stats.current_level == IPC_BP_NORMAL, "default level should be NORMAL");

    /* 默认队列容量应为 10000 */
    TEST_ASSERT(stats.queue_capacity == 10000, "default capacity should be 10000");

    /* 初始队列深度为 0 */
    TEST_ASSERT(stats.queue_depth == 0, "initial depth should be 0");

    /* 初始计数器为 0 */
    TEST_ASSERT(stats.total_sent == 0, "initial total_sent should be 0");
    TEST_ASSERT(stats.total_dropped == 0, "initial total_dropped should be 0");
    TEST_ASSERT(stats.total_rejected == 0, "initial total_rejected should be 0");

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/**
 * 测试 3: 自定义配置验证
 */
static void test_bp_custom_config(void)
{
    TEST_BEGIN("test_bp_custom_config");

    ipc_bp_config_t config = {
        .queue_capacity = 1000,
        .slow_threshold_pct = 70,
        .drop_threshold_pct = 85,
        .reject_threshold_pct = 95,
        .recover_threshold_pct = 50,
        .sample_interval_ms = 1000,
    };

    ipc_bp_controller_t *ctrl = ipc_bp_create(&config);
    TEST_ASSERT(ctrl != NULL, "create with custom config failed");

    ipc_bp_stats_t stats;
    ipc_bp_get_stats(ctrl, &stats);

    TEST_ASSERT(stats.queue_capacity == 1000, "custom capacity should be 1000");
    TEST_ASSERT(stats.current_level == IPC_BP_NORMAL, "should start at NORMAL");

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/**
 * 测试 4: 三级背压策略 — NORMAL → SLOW → DROP → REJECT
 */
static void test_bp_escalation(void)
{
    TEST_BEGIN("test_bp_escalation");

    ipc_bp_config_t config = {
        .queue_capacity = 1000,
        .slow_threshold_pct = 80,
        .drop_threshold_pct = 90,
        .reject_threshold_pct = 95,
        .recover_threshold_pct = 60,
        .sample_interval_ms = 0,  /* 禁用采样间隔，立即更新 */
    };

    ipc_bp_controller_t *ctrl = ipc_bp_create(&config);
    TEST_ASSERT(ctrl != NULL, "create failed");

    /* 50% → NORMAL */
    ipc_bp_level_t level = ipc_bp_update(ctrl, 500);
    TEST_ASSERT(level == IPC_BP_NORMAL, "50% should be NORMAL");

    /* 80% → SLOW */
    level = ipc_bp_update(ctrl, 800);
    TEST_ASSERT(level == IPC_BP_SLOW, "80% should be SLOW");

    /* 90% → DROP */
    level = ipc_bp_update(ctrl, 900);
    TEST_ASSERT(level == IPC_BP_DROP, "90% should be DROP");

    /* 95% → REJECT */
    level = ipc_bp_update(ctrl, 950);
    TEST_ASSERT(level == IPC_BP_REJECT, "95% should be REJECT");

    /* 100% → REJECT */
    level = ipc_bp_update(ctrl, 1000);
    TEST_ASSERT(level == IPC_BP_REJECT, "100% should be REJECT");

    /* 验证 slow_down_events 计数 */
    ipc_bp_stats_t stats;
    ipc_bp_get_stats(ctrl, &stats);
    TEST_ASSERT(stats.slow_down_events >= 3, "should have 3+ slow_down events");

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/**
 * 测试 5: 背压恢复 — REJECT → DROP → SLOW → NORMAL
 */
static void test_bp_recovery(void)
{
    TEST_BEGIN("test_bp_recovery");

    ipc_bp_config_t config = {
        .queue_capacity = 1000,
        .slow_threshold_pct = 80,
        .drop_threshold_pct = 90,
        .reject_threshold_pct = 95,
        .recover_threshold_pct = 60,
        .sample_interval_ms = 0,
    };

    ipc_bp_controller_t *ctrl = ipc_bp_create(&config);
    TEST_ASSERT(ctrl != NULL, "create failed");

    /* 先升到 REJECT */
    ipc_bp_update(ctrl, 960);
    TEST_ASSERT(ipc_bp_get_level(ctrl) == IPC_BP_REJECT, "should be at REJECT");

    /* 降到 80% — 不应立即恢复到 NORMAL（需低于 recover 阈值 60%） */
    ipc_bp_update(ctrl, 800);
    TEST_ASSERT(ipc_bp_get_level(ctrl) <= IPC_BP_SLOW, "80% should not fully recover");

    /* 降到 55% — 应恢复到 NORMAL */
    ipc_bp_level_t level = ipc_bp_update(ctrl, 550);
    TEST_ASSERT(level == IPC_BP_NORMAL, "55% should recover to NORMAL");

    /* 验证 recover_events 计数 */
    ipc_bp_stats_t stats;
    ipc_bp_get_stats(ctrl, &stats);
    TEST_ASSERT(stats.recover_events >= 1, "should have 1+ recover events");

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/**
 * 测试 6: should_send 在各级别下的行为
 */
static void test_bp_should_send(void)
{
    TEST_BEGIN("test_bp_should_send");

    ipc_bp_config_t config = {
        .queue_capacity = 1000,
        .slow_threshold_pct = 80,
        .drop_threshold_pct = 90,
        .reject_threshold_pct = 95,
        .recover_threshold_pct = 60,
        .sample_interval_ms = 0,
    };

    ipc_bp_controller_t *ctrl = ipc_bp_create(&config);
    TEST_ASSERT(ctrl != NULL, "create failed");

    /* NORMAL: 所有消息都允许发送 */
    ipc_bp_update(ctrl, 100);
    TEST_ASSERT(ipc_bp_should_send(ctrl, false) == true, "NORMAL: critical msg should send");
    TEST_ASSERT(ipc_bp_should_send(ctrl, true) == true, "NORMAL: droppable msg should send");

    /* SLOW: 所有消息仍允许发送（但建议降速） */
    ipc_bp_update(ctrl, 850);
    TEST_ASSERT(ipc_bp_should_send(ctrl, false) == true, "SLOW: critical msg should send");
    TEST_ASSERT(ipc_bp_should_send(ctrl, true) == true, "SLOW: droppable msg should send");

    /* DROP: 可丢弃消息被丢弃，关键消息允许 */
    ipc_bp_update(ctrl, 920);
    TEST_ASSERT(ipc_bp_should_send(ctrl, false) == true, "DROP: critical msg should send");
    TEST_ASSERT(ipc_bp_should_send(ctrl, true) == false, "DROP: droppable msg should be dropped");

    /* REJECT: 可丢弃消息被丢弃，关键消息仍允许 */
    ipc_bp_update(ctrl, 980);
    TEST_ASSERT(ipc_bp_should_send(ctrl, false) == true, "REJECT: critical msg should still send");
    TEST_ASSERT(ipc_bp_should_send(ctrl, true) == false, "REJECT: droppable msg should be dropped");

    /* 验证丢弃计数 */
    ipc_bp_stats_t stats;
    ipc_bp_get_stats(ctrl, &stats);
    TEST_ASSERT(stats.total_dropped >= 2, "should have 2+ dropped messages");

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/**
 * 测试 7: should_accept_connection 在各级别下的行为
 */
static void test_bp_should_accept_connection(void)
{
    TEST_BEGIN("test_bp_should_accept_connection");

    ipc_bp_config_t config = {
        .queue_capacity = 1000,
        .slow_threshold_pct = 80,
        .drop_threshold_pct = 90,
        .reject_threshold_pct = 95,
        .recover_threshold_pct = 60,
        .sample_interval_ms = 0,
    };

    ipc_bp_controller_t *ctrl = ipc_bp_create(&config);
    TEST_ASSERT(ctrl != NULL, "create failed");

    /* NORMAL: 接受连接 */
    ipc_bp_update(ctrl, 100);
    TEST_ASSERT(ipc_bp_should_accept_connection(ctrl) == true, "NORMAL: should accept");

    /* SLOW: 接受连接 */
    ipc_bp_update(ctrl, 850);
    TEST_ASSERT(ipc_bp_should_accept_connection(ctrl) == true, "SLOW: should accept");

    /* DROP: 接受连接 */
    ipc_bp_update(ctrl, 920);
    TEST_ASSERT(ipc_bp_should_accept_connection(ctrl) == true, "DROP: should accept");

    /* REJECT: 拒绝连接 */
    ipc_bp_update(ctrl, 980);
    TEST_ASSERT(ipc_bp_should_accept_connection(ctrl) == false, "REJECT: should reject");

    /* 验证拒绝计数 */
    ipc_bp_stats_t stats;
    ipc_bp_get_stats(ctrl, &stats);
    TEST_ASSERT(stats.total_rejected >= 1, "should have 1+ rejected connections");

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/**
 * 测试 8: 统计信息正确性
 */
static void test_bp_stats_accuracy(void)
{
    TEST_BEGIN("test_bp_stats_accuracy");

    ipc_bp_config_t config = {
        .queue_capacity = 100,
        .slow_threshold_pct = 80,
        .drop_threshold_pct = 90,
        .reject_threshold_pct = 95,
        .recover_threshold_pct = 60,
        .sample_interval_ms = 0,
    };

    ipc_bp_controller_t *ctrl = ipc_bp_create(&config);
    TEST_ASSERT(ctrl != NULL, "create failed");

    /* 发送 5 条关键消息 */
    ipc_bp_update(ctrl, 10);  /* NORMAL */
    for (int i = 0; i < 5; i++) {
        ipc_bp_should_send(ctrl, false);
    }

    /* 发送 3 条可丢弃消息（NORMAL 下全部通过） */
    for (int i = 0; i < 3; i++) {
        ipc_bp_should_send(ctrl, true);
    }

    ipc_bp_stats_t stats;
    ipc_bp_get_stats(ctrl, &stats);

    TEST_ASSERT(stats.total_sent == 8, "total_sent should be 8");
    TEST_ASSERT(stats.total_dropped == 0, "total_dropped should be 0");

    /* 升级到 DROP，发送 5 条可丢弃消息（应全部被丢弃） */
    ipc_bp_update(ctrl, 92);  /* DROP */
    for (int i = 0; i < 5; i++) {
        ipc_bp_should_send(ctrl, true);
    }

    ipc_bp_get_stats(ctrl, &stats);
    TEST_ASSERT(stats.total_dropped == 5, "total_dropped should be 5");
    TEST_ASSERT(stats.total_sent == 8, "total_sent should still be 8");

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/**
 * 测试 9: 模拟队列满 → 生产者降速 → 消费者消费 → 恢复
 *
 * 这是 P1.24.4 验收标准的完整模拟：
 * "OOM 场景下 IPC 不死锁、不丢关键消息"
 */
static void test_bp_full_cycle_simulation(void)
{
    TEST_BEGIN("test_bp_full_cycle_simulation");

    ipc_bp_config_t config = {
        .queue_capacity = 100,
        .slow_threshold_pct = 80,
        .drop_threshold_pct = 90,
        .reject_threshold_pct = 95,
        .recover_threshold_pct = 60,
        .sample_interval_ms = 0,
    };

    ipc_bp_controller_t *ctrl = ipc_bp_create(&config);
    TEST_ASSERT(ctrl != NULL, "create failed");

    /* 阶段 1: 队列逐渐填满 */
    size_t depth = 0;
    for (depth = 0; depth <= 100; depth += 10) {
        ipc_bp_update(ctrl, depth);
    }

    /* 此时应在 REJECT 级别 */
    TEST_ASSERT(ipc_bp_get_level(ctrl) == IPC_BP_REJECT, "should be at REJECT when full");

    /* 关键消息仍能发送 */
    bool critical_sent = ipc_bp_should_send(ctrl, false);
    TEST_ASSERT(critical_sent == true, "critical message must not be dropped at REJECT");

    /* 可丢弃消息被丢弃 */
    bool droppable_sent = ipc_bp_should_send(ctrl, true);
    TEST_ASSERT(droppable_sent == false, "droppable message should be dropped at REJECT");

    /* 新连接被拒绝 */
    bool accept_conn = ipc_bp_should_accept_connection(ctrl);
    TEST_ASSERT(accept_conn == false, "new connection should be rejected at REJECT");

    /* 阶段 2: 消费者开始消费，队列深度下降 */
    for (depth = 100; depth >= 50; depth -= 10) {
        ipc_bp_update(ctrl, depth);
    }

    /* 降到 50% 应恢复到 NORMAL */
    TEST_ASSERT(ipc_bp_get_level(ctrl) == IPC_BP_NORMAL, "should recover to NORMAL at 50%");

    /* 恢复后所有消息都能发送 */
    TEST_ASSERT(ipc_bp_should_send(ctrl, false) == true, "critical should send after recovery");
    TEST_ASSERT(ipc_bp_should_send(ctrl, true) == true, "droppable should send after recovery");
    TEST_ASSERT(ipc_bp_should_accept_connection(ctrl) == true, "connections accepted after recovery");

    /* 阶段 3: 验证统计 */
    ipc_bp_stats_t stats;
    ipc_bp_get_stats(ctrl, &stats);
    TEST_ASSERT(stats.slow_down_events > 0, "should have slow_down events");
    TEST_ASSERT(stats.recover_events > 0, "should have recover events");
    TEST_ASSERT(stats.total_dropped > 0, "should have dropped messages");
    TEST_ASSERT(stats.total_rejected > 0, "should have rejected connections");

    printf("    Stats: sent=%zu, dropped=%zu, rejected=%zu, slow_down=%zu, recover=%zu\n",
           (size_t)stats.total_sent, (size_t)stats.total_dropped,
           (size_t)stats.total_rejected, (size_t)stats.slow_down_events,
           (size_t)stats.recover_events);

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/**
 * 测试 10: NULL 安全性
 */
static void test_bp_null_safety(void)
{
    TEST_BEGIN("test_bp_null_safety");

    /* NULL 控制器的所有操作都应安全返回默认值 */
    TEST_ASSERT(ipc_bp_update(NULL, 100) == IPC_BP_NORMAL, "update(NULL) should return NORMAL");
    TEST_ASSERT(ipc_bp_should_send(NULL, false) == true, "should_send(NULL) should return true");
    TEST_ASSERT(ipc_bp_should_send(NULL, true) == true, "should_send(NULL, droppable) should return true");
    TEST_ASSERT(ipc_bp_should_accept_connection(NULL) == true, "should_accept(NULL) should return true");
    TEST_ASSERT(ipc_bp_get_level(NULL) == IPC_BP_NORMAL, "get_level(NULL) should return NORMAL");

    /* get_stats with NULL should not crash */
    ipc_bp_stats_t stats;
    ipc_bp_get_stats(NULL, &stats);
    /* stats 内容未定义，但不应该崩溃 */

    ipc_bp_get_stats(NULL, NULL);

    TEST_END();
}

/**
 * 测试 11: 边界值测试
 */
static void test_bp_boundary_values(void)
{
    TEST_BEGIN("test_bp_boundary_values");

    ipc_bp_config_t config = {
        .queue_capacity = 1000,
        .slow_threshold_pct = 80,
        .drop_threshold_pct = 90,
        .reject_threshold_pct = 95,
        .recover_threshold_pct = 60,
        .sample_interval_ms = 0,
    };

    ipc_bp_controller_t *ctrl = ipc_bp_create(&config);
    TEST_ASSERT(ctrl != NULL, "create failed");

    /* depth = 0 → NORMAL */
    TEST_ASSERT(ipc_bp_update(ctrl, 0) == IPC_BP_NORMAL, "depth=0 should be NORMAL");

    /* depth = 799 → NORMAL (just below 80%) */
    TEST_ASSERT(ipc_bp_update(ctrl, 799) == IPC_BP_NORMAL, "depth=799 should be NORMAL");

    /* depth = 800 → SLOW (exactly 80%) */
    TEST_ASSERT(ipc_bp_update(ctrl, 800) == IPC_BP_SLOW, "depth=800 should be SLOW");

    /* depth = 899 → SLOW (just below 90%) */
    TEST_ASSERT(ipc_bp_update(ctrl, 899) == IPC_BP_SLOW, "depth=899 should be SLOW");

    /* depth = 900 → DROP (exactly 90%) */
    TEST_ASSERT(ipc_bp_update(ctrl, 900) == IPC_BP_DROP, "depth=900 should be DROP");

    /* depth = 949 → DROP (just below 95%) */
    TEST_ASSERT(ipc_bp_update(ctrl, 949) == IPC_BP_DROP, "depth=949 should be DROP");

    /* depth = 950 → REJECT (exactly 95%) */
    TEST_ASSERT(ipc_bp_update(ctrl, 950) == IPC_BP_REJECT, "depth=950 should be REJECT");

    /* depth = 1000 → REJECT (100%) */
    TEST_ASSERT(ipc_bp_update(ctrl, 1000) == IPC_BP_REJECT, "depth=1000 should be REJECT");

    ipc_bp_destroy(ctrl);
    TEST_END();
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("====================================\n");
    printf("  P1.24.4: IPC Backpressure Tests\n");
    printf("====================================\n\n");

    test_bp_create_destroy();
    test_bp_default_config();
    test_bp_custom_config();
    test_bp_escalation();
    test_bp_recovery();
    test_bp_should_send();
    test_bp_should_accept_connection();
    test_bp_stats_accuracy();
    test_bp_full_cycle_simulation();
    test_bp_null_safety();
    test_bp_boundary_values();

    printf("\n====================================\n");
    printf("  Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("====================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}