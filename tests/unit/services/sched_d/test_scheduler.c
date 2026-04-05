/**
 * @file test_scheduler.c
 * @brief 调度服务单元测试
 * @details 测试调度服务的各个功能模�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../../agentos/daemon/sched_d/include/scheduler_service.h"

/**
 * @brief 测试创建和销毁调度服�?
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
        // From data intelligence emerges. by spharx
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
 * @brief 测试更新 Agent 状�?
 * @return 0 表示成功，非 0 表示失败
 */
int test_update_agent_status() {
    printf("=== Testing update agent status ===\n");

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

    // 更新 Agent 状�?
    agent1.load_factor = 0.5;
    agent1.success_rate = 0.98;
    ret = sched_service_update_agent_status(service, &agent1);
    if (ret != 0) {
        printf("Failed to update agent status\n");
        sched_service_destroy(service);
        return ret;
    }

    ret = sched_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy scheduler service\n");
        return ret;
    }

    printf("Update agent status test passed\n\n");
    return 0;
}

/**
 * @brief 测试调度任务
 * @return 0 表示成功，非 0 表示失败
 */
int test_schedule_task() {
    printf("=== Testing schedule task ===\n");

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

    // 创建任务
    task_info_t task1 = {
        .task_id = "task1",
        .task_description = "Task 1",
        .priority = TASK_PRIORITY_NORMAL,
        .timeout_ms = 5000,
        .task_data = NULL,
        .task_data_size = 0
    };

    // 调度任务
    sched_result_t* result = NULL;
    ret = sched_service_schedule_task(service, &task1, &result);
    if (ret != 0) {
        printf("Failed to schedule task\n");
        sched_service_destroy(service);
        return ret;
    }

    if (result) {
        printf("Task scheduled to agent: %s\n", result->selected_agent_id);
        free(result->selected_agent_id);
        free(result);
    }

    ret = sched_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy scheduler service\n");
        return ret;
    }

    printf("Schedule task test passed\n\n");
    return 0;
}

/**
 * @brief 测试健康检�?
 * @return 0 表示成功，非 0 表示失败
 */
int test_health_check() {
    printf("=== Testing health check ===\n");

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

    // 健康检�?
    bool health_status = false;
    ret = sched_service_health_check(service, &health_status);
    if (ret != 0) {
        printf("Failed to perform health check\n");
        sched_service_destroy(service);
        return ret;
    }

    printf("Health check status: %s\n", health_status ? "Healthy" : "Unhealthy");

    ret = sched_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy scheduler service\n");
        return ret;
    }

    printf("Health check test passed\n\n");
    return 0;
}

/**
 * @brief 测试获取统计信息
 * @return 0 表示成功，非 0 表示失败
 */
int test_get_stats() {
    printf("=== Testing get stats ===\n");

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

    // 创建任务
    task_info_t task1 = {
        .task_id = "task1",
        .task_description = "Task 1",
        .priority = TASK_PRIORITY_NORMAL,
        .timeout_ms = 5000,
        .task_data = NULL,
        .task_data_size = 0
    };

    // 调度任务
    sched_result_t* result = NULL;
    ret = sched_service_schedule_task(service, &task1, &result);
    if (ret != 0) {
        printf("Failed to schedule task\n");
        sched_service_destroy(service);
        return ret;
    }

    if (result) {
        free(result->selected_agent_id);
        free(result);
    }

    // 获取统计信息
    void* stats = NULL;
    ret = sched_service_get_stats(service, &stats);
    if (ret != 0) {
        printf("Failed to get stats\n");
        sched_service_destroy(service);
        return ret;
    }

    if (stats) {
        printf("Stats:\n%s\n", (char*)stats);
        free(stats);
    }

    ret = sched_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy scheduler service\n");
        return ret;
    }

    printf("Get stats test passed\n\n");
    return 0;
}

/**
 * @brief 测试重载配置
 * @return 0 表示成功，非 0 表示失败
 */
int test_reload_config() {
    printf("=== Testing reload manager ===\n");

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

    // 重载配置
    sched_config_t new_config = {
        .strategy = SCHED_STRATEGY_WEIGHTED,
        .health_check_interval_ms = 10000,
        .stats_report_interval_ms = 20000,
        .enable_ml_strategy = false,
        .ml_model_path = NULL,
        .max_agents = 20
    };

    ret = sched_service_reload_config(service, &new_config);
    if (ret != 0) {
        printf("Failed to reload manager\n");
        sched_service_destroy(service);
        return ret;
    }

    ret = sched_service_destroy(service);
    if (ret != 0) {
        printf("Failed to destroy scheduler service\n");
        return ret;
    }

    printf("Reload manager test passed\n\n");
    return 0;
}

/**
 * @brief 主测试函�?
 * @return 0 表示所有测试通过，非 0 表示有测试失�?
 */
int main() {
    int ret = 0;

    ret |= test_create_destroy();
    ret |= test_register_unregister_agent();
    ret |= test_update_agent_status();
    ret |= test_schedule_task();
    ret |= test_health_check();
    ret |= test_get_stats();
    ret |= test_reload_config();

    if (ret == 0) {
        printf("All tests passed!\n");
    } else {
        printf("Some tests failed!\n");
    }

    return ret;
}
