/**
 * @file monitor.c
 * @brief 监控模块实现
 * @details 监控 Agent 健康状态和系统运行状态
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "scheduler_service.h"

/**
 * @brief 监控数据
 */
typedef struct {
    uint64_t total_tasks;       /**< 总任务数 */
    uint64_t successful_tasks;  /**< 成功任务数 */
    uint64_t failed_tasks;      /**< 失败任务数 */
    uint64_t total_execution_time_ms; /**< 总执行时间（毫秒） */
    time_t last_health_check;   /**< 上次健康检查时间 */
    time_t last_stats_report;   /**< 上次统计报告时间 */
    size_t available_agents;    /**< 可用 Agent 数量 */
    size_t total_agents;        /**< 总 Agent 数量 */
    // From data intelligence emerges. by spharx
} monitor_data_t;

/**
 * @brief 创建监控模块
 * @param data 输出参数，返回监控数据
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_create(void** data) {
    monitor_data_t* md = (monitor_data_t*)malloc(sizeof(monitor_data_t));
    if (!md) {
        return -1;
    }

    md->total_tasks = 0;
    md->successful_tasks = 0;
    md->failed_tasks = 0;
    md->total_execution_time_ms = 0;
    md->last_health_check = time(NULL);
    md->last_stats_report = time(NULL);
    md->available_agents = 0;
    md->total_agents = 0;

    *data = md;
    return 0;
}

/**
 * @brief 销毁监控模块
 * @param data 监控数据
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_destroy(void* data) {
    if (!data) {
        return 0;
    }

    free(data);
    return 0;
}

/**
 * @brief 记录任务执行结果
 * @param data 监控数据
 * @param success 是否成功
 * @param execution_time_ms 执行时间（毫秒）
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_record_task(void* data, bool success, uint32_t execution_time_ms) {
    if (!data) {
        return -1;
    }

    monitor_data_t* md = (monitor_data_t*)data;

    md->total_tasks++;
    if (success) {
        md->successful_tasks++;
    } else {
        md->failed_tasks++;
    }
    md->total_execution_time_ms += execution_time_ms;

    return 0;
}

/**
 * @brief 更新 Agent 状态
 * @param data 监控数据
 * @param available_count 可用 Agent 数量
 * @param total_count 总 Agent 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_update_agent_status(void* data, size_t available_count, size_t total_count) {
    if (!data) {
        return -1;
    }

    monitor_data_t* md = (monitor_data_t*)data;
    md->available_agents = available_count;
    md->total_agents = total_count;

    return 0;
}

/**
 * @brief 执行健康检查
 * @param data 监控数据
 * @param health_status 输出参数，返回健康状态
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_health_check(void* data, bool* health_status) {
    if (!data || !health_status) {
        return -1;
    }

    monitor_data_t* md = (monitor_data_t*)data;

    // 简单的健康检查逻辑
    // 1. 检查是否有可用的 Agent
    // 2. 检查任务失败率是否过高
    // 3. 检查系统是否响应

    bool healthy = true;

    // 检查可用 Agent
    if (md->total_agents > 0 && md->available_agents == 0) {
        healthy = false;
        printf("Health check failed: No available agents\n");
    }

    // 检查任务失败率
    if (md->total_tasks > 10) {  // 至少有 10 个任务
        float failure_rate = (float)md->failed_tasks / md->total_tasks;
        if (failure_rate > 0.5) {  // 失败率超过 50%
            healthy = false;
            printf("Health check failed: High failure rate (%.2f)\n", failure_rate);
        }
    }

    // 更新最后检查时间
    md->last_health_check = time(NULL);

    *health_status = healthy;
    return 0;
}

/**
 * @brief 获取统计信息
 * @param data 监控数据
 * @param stats 输出参数，返回统计信息
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_get_stats(void* data, void** stats) {
    if (!data || !stats) {
        return -1;
    }

    monitor_data_t* md = (monitor_data_t*)data;

    // 创建统计信息字符串
    char* stats_str = (char*)malloc(512);
    if (!stats_str) {
        return -1;
    }

    float avg_execution_time = 0.0f;
    if (md->total_tasks > 0) {
        avg_execution_time = (float)md->total_execution_time_ms / md->total_tasks;
    }

    float success_rate = 0.0f;
    if (md->total_tasks > 0) {
        success_rate = (float)md->successful_tasks / md->total_tasks;
    }

    snprintf(stats_str, 512, 
             "Total Tasks: %llu\n" 
             "Successful Tasks: %llu\n" 
             "Failed Tasks: %llu\n" 
             "Success Rate: %.2f%%\n" 
             "Average Execution Time: %.2f ms\n" 
             "Available Agents: %zu/%zu\n" 
             "Last Health Check: %s" 
             "Last Stats Report: %s",
             md->total_tasks,
             md->successful_tasks,
             md->failed_tasks,
             success_rate * 100.0f,
             avg_execution_time,
             md->available_agents,
             md->total_agents,
             ctime(&md->last_health_check),
             ctime(&md->last_stats_report));

    *stats = stats_str;
    return 0;
}

/**
 * @brief 生成统计报告
 * @param data 监控数据
 * @return 0 表示成功，非 0 表示错误码
 */
int monitor_generate_report(void* data) {
    if (!data) {
        return -1;
    }

    monitor_data_t* md = (monitor_data_t*)data;

    // 生成统计报告
    void* stats = NULL;
    if (monitor_get_stats(data, &stats) == 0) {
        printf("\n=== Scheduler Stats Report ===\n");
        printf("%s\n", (char*)stats);
        printf("=============================\n");
        free(stats);
    }

    // 更新最后报告时间
    md->last_stats_report = time(NULL);

    return 0;
}
