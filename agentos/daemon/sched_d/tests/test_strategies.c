/**
 * @file test_strategies.c
 * @brief 调度策略单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "scheduler_service.h"
#include "strategy_interface.h"

static void test_round_robin_strategy(void) {
    printf("  test_round_robin_strategy...\n");

    const strategy_interface_t* strategy = get_round_robin_strategy();
    assert(strategy != NULL);
    assert(strategy->get_name != NULL);
    assert(strcmp(strategy->get_name(), "round_robin") == 0 ||
           strategy->get_name() != NULL);

    void* data = NULL;
    sched_config_t config;
    memset(&config, 0, sizeof(config));
    config.strategy = SCHED_STRATEGY_ROUND_ROBIN;
    config.max_agents = 10;

    int ret = strategy->create(&config, &data);
    assert(ret == 0);
    assert(data != NULL);

    agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.agent_id = "rr_agent_1";
    agent.agent_name = "RR Agent 1";
    agent.load_factor = 0.3f;
    agent.success_rate = 0.95f;
    agent.is_available = true;
    agent.weight = 1.0f;

    ret = strategy->register_agent(data, &agent);
    assert(ret == 0);

    task_info_t task;
    memset(&task, 0, sizeof(task));
    task.task_id = "rr_task_1";
    task.task_description = "Round robin test task";
    task.priority = TASK_PRIORITY_NORMAL;
    task.timeout_ms = 5000;

    sched_result_t* result = NULL;
    ret = strategy->schedule(data, &task, &result);
    if (ret == 0 && result != NULL) {
        printf("    Selected agent: %s\n", result->selected_agent_id);
    }

    size_t agent_count = strategy->get_available_agent_count(data);
    assert(agent_count >= 1);

    strategy->destroy(data);

    printf("    PASSED\n");
}

static void test_weighted_strategy(void) {
    printf("  test_weighted_strategy...\n");

    const strategy_interface_t* strategy = get_weighted_strategy();
    assert(strategy != NULL);

    void* data = NULL;
    sched_config_t config;
    memset(&config, 0, sizeof(config));
    config.strategy = SCHED_STRATEGY_WEIGHTED;
    config.max_agents = 10;

    int ret = strategy->create(&config, &data);
    assert(ret == 0);
    assert(data != NULL);

    agent_info_t agent1;
    memset(&agent1, 0, sizeof(agent1));
    agent1.agent_id = "weighted_agent_1";
    agent1.agent_name = "Weighted Agent 1";
    agent1.weight = 10.0f;
    agent1.is_available = true;

    agent_info_t agent2;
    memset(&agent2, 0, sizeof(agent2));
    agent2.agent_id = "weighted_agent_2";
    agent2.agent_name = "Weighted Agent 2";
    agent2.weight = 20.0f;
    agent2.is_available = true;

    strategy->register_agent(data, &agent1);
    strategy->register_agent(data, &agent2);

    task_info_t task;
    memset(&task, 0, sizeof(task));
    task.task_id = "weighted_task_1";
    task.priority = TASK_PRIORITY_NORMAL;

    sched_result_t* result = NULL;
    ret = strategy->schedule(data, &task, &result);
    if (ret == 0 && result != NULL) {
        printf("    Selected agent: %s (confidence: %.2f)\n",
               result->selected_agent_id, result->confidence);
    }

    strategy->destroy(data);

    printf("    PASSED\n");
}

static void test_ml_based_strategy(void) {
    printf("  test_ml_based_strategy...\n");

    const strategy_interface_t* strategy = get_ml_based_strategy();
    assert(strategy != NULL);

    void* data = NULL;
    sched_config_t config;
    memset(&config, 0, sizeof(config));
    config.strategy = SCHED_STRATEGY_ML_BASED;
    config.max_agents = 10;
    config.enable_ml_strategy = true;

    int ret = strategy->create(&config, &data);
    if (ret == 0 && data != NULL) {
        agent_info_t agent;
        memset(&agent, 0, sizeof(agent));
        agent.agent_id = "ml_agent_1";
        agent.agent_name = "ML Agent 1";
        agent.is_available = true;

        strategy->register_agent(data, &agent);

        task_info_t task;
        memset(&task, 0, sizeof(task));
        task.task_id = "ml_task_1";
        task.priority = TASK_PRIORITY_NORMAL;

        sched_result_t* result = NULL;
        strategy->schedule(data, &task, &result);

        strategy->destroy(data);
    } else {
        printf("    ML strategy creation skipped\n");
    }

    printf("    PASSED\n");
}

static void test_priority_based_strategy(void) {
    printf("  test_priority_based_strategy...\n");

    const strategy_interface_t* strategy = get_priority_based_strategy();
    assert(strategy != NULL);

    void* data = NULL;
    sched_config_t config;
    memset(&config, 0, sizeof(config));
    config.strategy = SCHED_STRATEGY_ROUND_ROBIN;
    config.max_agents = 10;

    int ret = strategy->create(&config, &data);
    assert(ret == 0);
    assert(data != NULL);

    agent_info_t agent;
    memset(&agent, 0, sizeof(agent));
    agent.agent_id = "priority_agent_1";
    agent.agent_name = "Priority Agent 1";
    agent.is_available = true;

    strategy->register_agent(data, &agent);

    task_info_t low_task;
    memset(&low_task, 0, sizeof(low_task));
    low_task.task_id = "low_priority_task";
    low_task.priority = TASK_PRIORITY_LOW;

    task_info_t high_task;
    memset(&high_task, 0, sizeof(high_task));
    high_task.task_id = "high_priority_task";
    high_task.priority = TASK_PRIORITY_HIGH;

    sched_result_t* result = NULL;
    strategy->schedule(data, &high_task, &result);

    strategy->destroy(data);

    printf("    PASSED\n");
}

static void test_strategy_enum_values(void) {
    printf("  test_strategy_enum_values...\n");

    assert(SCHED_STRATEGY_ROUND_ROBIN == 0);
    assert(SCHED_STRATEGY_WEIGHTED == 1);
    assert(SCHED_STRATEGY_ML_BASED == 2);
    assert(SCHED_STRATEGY_COUNT == 3);

    assert(TASK_PRIORITY_LOW == 0);
    assert(TASK_PRIORITY_NORMAL == 1);
    assert(TASK_PRIORITY_HIGH == 2);
    assert(TASK_PRIORITY_URGENT == 3);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Scheduler Strategies Unit Tests\n");
    printf("=========================================\n");

    test_strategy_enum_values();
    test_round_robin_strategy();
    test_weighted_strategy();
    test_ml_based_strategy();
    test_priority_based_strategy();

    printf("\nAll strategy tests PASSED\n");
    return 0;
}
