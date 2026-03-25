/**
 * @file main.c
 * @brief 调度服务主程序
 * @details 负责初始化和运行调度服务
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "scheduler_service.h"
#include "strategy_interface.h"
#include "monitor_service.h"

/**
 * @brief 调度服务结构
 */
typedef struct sched_service {
    sched_config_t config;              /**< 配置信息 */
    void* strategy_data;                /**< 策略数据 */
    monitor_service_t* monitor;         /**< 监控服务 */
    const strategy_interface_t* strategy; /**< 策略接口 */
} sched_service_t;

// From data intelligence emerges. by spharx
/**
 * @brief 获取策略接口
 * @param strategy 策略类型
 * @return 策略接口
 */
static const strategy_interface_t* get_strategy_by_type(sched_strategy_t strategy) {
    switch (strategy) {
        case SCHED_STRATEGY_ROUND_ROBIN:
            return get_round_robin_strategy();
        case SCHED_STRATEGY_WEIGHTED:
            return get_weighted_strategy();
        case SCHED_STRATEGY_ML_BASED:
            return get_ml_based_strategy();
        default:
            return NULL;
    }
}

/**
 * @brief 创建调度服务
 * @param config 配置信息
 * @param service 输出参数，返回创建的服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_create(const sched_config_t* config, sched_service_t** service) {
    if (!config || !service) {
        return -1;
    }

    sched_service_t* s = (sched_service_t*)malloc(sizeof(sched_service_t));
    if (!s) {
        return -1;
    }

    // 复制配置
    s->config = *config;

    // 初始化监控服务
    monitor_config_t monitor_config = {
        .metrics_collection_interval_ms = 5000,
        .health_check_interval_ms = config->health_check_interval_ms,
        .log_flush_interval_ms = 30000,
        .alert_check_interval_ms = 5000,
        .log_file_path = "sched_monitor.log",
        .metrics_storage_path = "sched_metrics",
        .enable_tracing = true,
        .enable_alerting = true
    };

    if (monitor_service_create(&monitor_config, &s->monitor) != 0) {
        free(s);
        return -2;
    }

    // 获取策略接口
    s->strategy = get_strategy_by_type(config->strategy);
    if (!s->strategy) {
        monitor_service_destroy(s->monitor);
        free(s);
        return -3;
    }

    // 创建策略
    if (s->strategy->create(config, &s->strategy_data) != 0) {
        monitor_service_destroy(s->monitor);
        free(s);
        return -4;
    }

    printf("Scheduler service created with strategy: %s\n", s->strategy->get_name());
    *service = s;
    return 0;
}

/**
 * @brief 销毁调度服务
 * @param service 服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_destroy(sched_service_t* service) {
    if (!service) {
        return 0;
    }

    // 销毁策略
    if (service->strategy && service->strategy->destroy) {
        service->strategy->destroy(service->strategy_data);
    }

    // 销毁监控服务
    if (service->monitor) {
        monitor_service_destroy(service->monitor);
    }

    free(service);
    printf("Scheduler service destroyed\n");
    return 0;
}

/**
 * @brief 注册 Agent
 * @param service 服务句柄
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_register_agent(sched_service_t* service, const agent_info_t* agent_info) {
    if (!service || !agent_info) {
        return -1;
    }

    int ret = service->strategy->register_agent(service->strategy_data, agent_info);
    if (ret == 0) {
        printf("Agent registered: %s\n", agent_info->agent_id);
        
        // 更新监控数据
        size_t available_count = service->strategy->get_available_agent_count(service->strategy_data);
        size_t total_count = service->strategy->get_total_agent_count(service->strategy_data);
        
        // 记录指标
        metric_info_t metric = {
            .name = "sched.agent.count",
            .description = "Agent 数量",
            .type = METRIC_TYPE_GAUGE,
            .labels = NULL,
            .label_count = 0,
            .value = (double)total_count,
            .timestamp = (uint64_t)time(NULL) * 1000
        };
        monitor_service_record_metric(service->monitor, &metric);
        
        metric.name = "sched.agent.available";
        metric.description = "可用 Agent 数量";
        metric.value = (double)available_count;
        monitor_service_record_metric(service->monitor, &metric);
    }

    return ret;
}

/**
 * @brief 注销 Agent
 * @param service 服务句柄
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_unregister_agent(sched_service_t* service, const char* agent_id) {
    if (!service || !agent_id) {
        return -1;
    }

    int ret = service->strategy->unregister_agent(service->strategy_data, agent_id);
    if (ret == 0) {
        printf("Agent unregistered: %s\n", agent_id);
        
        // 更新监控数据
        size_t available_count = service->strategy->get_available_agent_count(service->strategy_data);
        size_t total_count = service->strategy->get_total_agent_count(service->strategy_data);
        
        // 记录指标
        metric_info_t metric = {
            .name = "sched.agent.count",
            .description = "Agent 数量",
            .type = METRIC_TYPE_GAUGE,
            .labels = NULL,
            .label_count = 0,
            .value = (double)total_count,
            .timestamp = (uint64_t)time(NULL) * 1000
        };
        monitor_service_record_metric(service->monitor, &metric);
        
        metric.name = "sched.agent.available";
        metric.description = "可用 Agent 数量";
        metric.value = (double)available_count;
        monitor_service_record_metric(service->monitor, &metric);
    }

    return ret;
}

/**
 * @brief 更新 Agent 状态
 * @param service 服务句柄
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_update_agent_status(sched_service_t* service, const agent_info_t* agent_info) {
    if (!service || !agent_info) {
        return -1;
    }

    int ret = service->strategy->update_agent_status(service->strategy_data, agent_info);
    if (ret == 0) {
        printf("Agent status updated: %s\n", agent_info->agent_id);
        
        // 更新监控数据
        size_t available_count = service->strategy->get_available_agent_count(service->strategy_data);
        size_t total_count = service->strategy->get_total_agent_count(service->strategy_data);
        
        // 记录指标
        metric_info_t metric = {
            .name = "sched.agent.count",
            .description = "Agent 数量",
            .type = METRIC_TYPE_GAUGE,
            .labels = NULL,
            .label_count = 0,
            .value = (double)total_count,
            .timestamp = (uint64_t)time(NULL) * 1000
        };
        monitor_service_record_metric(service->monitor, &metric);
        
        metric.name = "sched.agent.available";
        metric.description = "可用 Agent 数量";
        metric.value = (double)available_count;
        monitor_service_record_metric(service->monitor, &metric);
        
        // 记录 Agent 状态指标
        metric.name = "sched.agent.load";
        metric.description = "Agent 负载";
        metric.value = agent_info->load_factor;
        monitor_service_record_metric(service->monitor, &metric);
        
        metric.name = "sched.agent.success_rate";
        metric.description = "Agent 成功率";
        metric.value = agent_info->success_rate;
        monitor_service_record_metric(service->monitor, &metric);
    }

    return ret;
}

/**
 * @brief 调度任务
 * @param service 服务句柄
 * @param task_info 任务信息
 * @param result 输出参数，返回调度结果
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_schedule_task(sched_service_t* service, const task_info_t* task_info, sched_result_t** result) {
    if (!service || !task_info || !result) {
        return -1;
    }

    int ret = service->strategy->schedule(service->strategy_data, task_info, result);
    if (ret == 0) {
        printf("Task scheduled: %s -> Agent: %s (Confidence: %.2f)\n", 
               task_info->task_id, (*result)->selected_agent_id, (*result)->confidence);
        
        // 记录任务指标
        metric_info_t metric = {
            .name = "sched.task.scheduled",
            .description = "调度任务数",
            .type = METRIC_TYPE_COUNTER,
            .labels = NULL,
            .label_count = 0,
            .value = 1.0,
            .timestamp = (uint64_t)time(NULL) * 1000
        };
        monitor_service_record_metric(service->monitor, &metric);
        
        metric.name = "sched.task.estimated_time";
        metric.description = "任务估计执行时间";
        metric.type = METRIC_TYPE_GAUGE;
        metric.value = (double)(*result)->estimated_time_ms;
        monitor_service_record_metric(service->monitor, &metric);
        
        metric.name = "sched.task.confidence";
        metric.description = "调度置信度";
        metric.value = (*result)->confidence;
        monitor_service_record_metric(service->monitor, &metric);
        
        // 记录日志
        log_info_t log = {
            .level = LOG_LEVEL_INFO,
            .message = "Task scheduled successfully",
            .service_name = "sched_d",
            .file = __FILE__,
            .line = __LINE__,
            .function = __func__,
            .timestamp = (uint64_t)time(NULL) * 1000,
            .context = NULL,
            .context_count = 0
        };
        monitor_service_log(service->monitor, &log);
    } else {
        // 记录调度失败日志
        log_info_t log = {
            .level = LOG_LEVEL_ERROR,
            .message = "Task scheduling failed",
            .service_name = "sched_d",
            .file = __FILE__,
            .line = __LINE__,
            .function = __func__,
            .timestamp = (uint64_t)time(NULL) * 1000,
            .context = NULL,
            .context_count = 0
        };
        monitor_service_log(service->monitor, &log);
    }

    return ret;
}

/**
 * @brief 获取调度统计信息
 * @param service 服务句柄
 * @param stats 输出参数，返回统计信息
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_get_stats(sched_service_t* service, void** stats) {
    if (!service || !stats) {
        return -1;
    }

    // 生成监控报告
    char* report = NULL;
    int ret = monitor_service_generate_report(service->monitor, &report);
    if (ret == 0 && report) {
        *stats = report;
    }

    return ret;
}

/**
 * @brief 健康检查
 * @param service 服务句柄
 * @param health_status 输出参数，返回健康状态
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_health_check(sched_service_t* service, bool* health_status) {
    if (!service || !health_status) {
        return -1;
    }

    // 更新 Agent 状态到监控模块
    size_t available_count = service->strategy->get_available_agent_count(service->strategy_data);
    size_t total_count = service->strategy->get_total_agent_count(service->strategy_data);
    
    // 记录指标
    metric_info_t metric = {
        .name = "sched.agent.count",
        .description = "Agent 数量",
        .type = METRIC_TYPE_GAUGE,
        .labels = NULL,
        .label_count = 0,
        .value = (double)total_count,
        .timestamp = (uint64_t)time(NULL) * 1000
    };
    monitor_service_record_metric(service->monitor, &metric);
    
    metric.name = "sched.agent.available";
    metric.description = "可用 Agent 数量";
    metric.value = (double)available_count;
    monitor_service_record_metric(service->monitor, &metric);

    // 执行健康检查
    health_check_result_t* result = NULL;
    int ret = monitor_service_health_check(service->monitor, "sched_d", &result);
    if (ret == 0 && result) {
        *health_status = result->is_healthy;
        
        // 记录健康检查结果
        log_info_t log = {
            .level = LOG_LEVEL_INFO,
            .message = result->status_message,
            .service_name = "sched_d",
            .file = __FILE__,
            .line = __LINE__,
            .function = __func__,
            .timestamp = (uint64_t)time(NULL) * 1000,
            .context = NULL,
            .context_count = 0
        };
        monitor_service_log(service->monitor, &log);
        
        // 释放结果
        free(result->service_name);
        free(result->status_message);
        free(result);
    }

    return ret;
}

/**
 * @brief 重载配置
 * @param service 服务句柄
 * @param config 新的配置信息
 * @return 0 表示成功，非 0 表示错误码
 */
int sched_service_reload_config(sched_service_t* service, const sched_config_t* config) {
    if (!service || !config) {
        return -1;
    }

    // 验证配置参数
    if (config->max_agents == 0) {
        return -4; // 无效的最大 Agent 数量
    }

    if (config->health_check_interval_ms < 1000) {
        return -5; // 健康检查间隔过小
    }

    if (config->stats_report_interval_ms < 1000) {
        return -6; // 统计报告间隔过小
    }

    // 销毁旧策略
    if (service->strategy && service->strategy->destroy) {
        int ret = service->strategy->destroy(service->strategy_data);
        if (ret != 0) {
            return -7; // 销毁旧策略失败
        }
    }

    // 获取新策略接口
    const strategy_interface_t* new_strategy = get_strategy_by_type(config->strategy);
    if (!new_strategy) {
        return -2; // 无效的策略类型
    }

    // 创建新策略
    void* new_strategy_data = NULL;
    if (new_strategy->create(config, &new_strategy_data) != 0) {
        return -3; // 创建新策略失败
    }

    // 更新配置和策略
    service->config = *config;
    service->strategy = new_strategy;
    service->strategy_data = new_strategy_data;

    // 记录配置重载日志
    log_info_t log = {
        .level = LOG_LEVEL_INFO,
        .message = "Scheduler service config reloaded successfully",
        .service_name = "sched_d",
        .file = __FILE__,
        .line = __LINE__,
        .function = __func__,
        .timestamp = (uint64_t)time(NULL) * 1000,
        .context = NULL,
        .context_count = 0
    };
    monitor_service_log(service->monitor, &log);

    printf("Scheduler service config reloaded with strategy: %s\n", service->strategy->get_name());
    return 0;
}

/**
 * @brief 主函数
 * @return 0 表示成功，非 0 表示错误码
 */
int main() {
    // 初始化随机数
    srand(time(NULL));

    // 创建配置
    sched_config_t config = {
        .strategy = SCHED_STRATEGY_ROUND_ROBIN,
        .health_check_interval_ms = 5000,
        .stats_report_interval_ms = 10000,
        .enable_ml_strategy = false,
        .ml_model_path = NULL,
        .max_agents = 100
    };

    // 创建调度服务
    sched_service_t* service = NULL;
    if (sched_service_create(&config, &service) != 0) {
        printf("Failed to create scheduler service\n");
        return 1;
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

    agent_info_t agent2 = {
        .agent_id = "agent2",
        .agent_name = "Agent 2",
        .load_factor = 0.5,
        .success_rate = 0.90,
        .avg_response_time_ms = 150,
        .is_available = true,
        .weight = 1.0
    };

    agent_info_t agent3 = {
        .agent_id = "agent3",
        .agent_name = "Agent 3",
        .load_factor = 0.2,
        .success_rate = 0.98,
        .avg_response_time_ms = 80,
        .is_available = true,
        .weight = 1.0
    };

    sched_service_register_agent(service, &agent1);
    sched_service_register_agent(service, &agent2);
    sched_service_register_agent(service, &agent3);

    // 创建任务
    task_info_t task1 = {
        .task_id = "task1",
        .task_description = "Task 1",
        .priority = TASK_PRIORITY_NORMAL,
        .timeout_ms = 5000,
        .task_data = NULL,
        .task_data_size = 0
    };

    task_info_t task2 = {
        .task_id = "task2",
        .task_description = "Task 2",
        .priority = TASK_PRIORITY_HIGH,
        .timeout_ms = 3000,
        .task_data = NULL,
        .task_data_size = 0
    };

    task_info_t task3 = {
        .task_id = "task3",
        .task_description = "Task 3",
        .priority = TASK_PRIORITY_LOW,
        .timeout_ms = 10000,
        .task_data = NULL,
        .task_data_size = 0
    };

    // 调度任务
    sched_result_t* result = NULL;
    sched_service_schedule_task(service, &task1, &result);
    if (result) {
        free(result->selected_agent_id);
        free(result);
        result = NULL;
    }

    sched_service_schedule_task(service, &task2, &result);
    if (result) {
        free(result->selected_agent_id);
        free(result);
        result = NULL;
    }

    sched_service_schedule_task(service, &task3, &result);
    if (result) {
        free(result->selected_agent_id);
        free(result);
        result = NULL;
    }

    // 生成统计报告
    char* report = NULL;
    monitor_service_generate_report(service->monitor, &report);
    if (report) {
        printf("\n调度服务统计报告:\n%s\n", report);
        free(report);
    }

    // 健康检查
    bool health_status = false;
    sched_service_health_check(service, &health_status);
    printf("Health check status: %s\n", health_status ? "Healthy" : "Unhealthy");

    // 销毁调度服务
    sched_service_destroy(service);

    return 0;
}
