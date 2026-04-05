/**
 * @file test_scheduler.c
 * @brief 调度服务单元测试
 * @details 测试调度服务的各个功能模块
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler_service.h"

/**
 * @brief 测试创建和销毁调度服务
 * @return 0 表示成功，非 0 表示失败
 */
int test_create_destroy() {
    printf("=== Testing create and destroy ===\n");
    
    sched_config_t manager = {
        .strategy = SCHED_STRATEGY_ROUND_ROBIN,
        .health_check_interval_ms = 5000,
        .stats_report_interval_ms = 10000,
        .enable_ml_strategy = false,
        .ml_model_path = NULL,
        .max_agents = 10
    };

    sched_service_t* service = NULL;
    int ret = sched_service_create(&manager, &service);
    if (ret != 0) {
        printf("Failed to create scheduler service\n");
        return ret;
    }

    ret = sched_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy scheduler service\n");
        return ret;
    }

    printf("Create and destroy test passed\n\n");
    return 0;
}

/**
 * @brief 测试注册和注销 Agent
 * @return 0 表示成功，非 0 表示失败
 */
int test_register_unregister_agent() {
    printf("=== Testing register and unregister agent ===\n");
    
    sched_config_t manager = {
        .strategy = SCHED_STRATEGY_ROUND_ROBIN,
        .health_check_interval_ms = 5000,
        .stats_report_interval_ms = 10000,
        .enable_ml_strategy = false,
        .ml_model_path = NULL,
        .max_agents = 10
    };

    sched_service_t* service = NULL;
    int ret = sched_service_create(&manager, &service);
    if (ret != 0) {
        printf("Failed to create scheduler service\n");
        return ret;
    }

    // 注册 Agent
    agent_info_t agent1 = {
        .agent_id = "agent1",
        .agent_name = "Agent 1",
        .load_factor = 0.3,
        .success_rate = 0.95,
        .avg_response_time_ms = 100,
        .is_available = true,
        .weight = 1.0
    };

    ret = sched_service_register_agent(service, &agent1);
    if (ret != 0) {
        printf("Failed to register agent\n");
        sched_service_destroy(service);
        return ret;
    }

    // 注销 Agent
    ret = sched_service_unregister_agent(service, "agent1");
    if (ret != 0) {
        printf("Failed to unregister agent\n");
        sched_service_destroy(service);
        return ret;
    }

    ret = sched_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy scheduler service\n");
        return ret;
    }

    printf("Register and unregister agent test passed\n\n");
    return 0;
}

/**
 * @brief 主测试函数
 */
int main() {
    int ret = 0;

    ret |= test_create_destroy();
    ret |= test_register_unregister_agent();

    if (ret == 0) {
        printf("All tests passed!\n");
    } else {
        printf("Some tests failed!\n");
    }

    return ret;
}
