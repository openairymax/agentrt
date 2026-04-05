/**
 * @file atoms_benchmarks.h
 * @brief AgentOS Atoms模块性能基准测试接口
 *
 * 提供标准化的性能基准测试接口，用于验证系统是否满足性能SLA要求：
 * - IPC延迟: <1μs
 * - 任务切换延迟: <1ms
 * - 记忆检索延迟: <10ms
 *
 * @note 本文件从 agentos/atoms/benchmarks/ 迁移至 tests/benchmarks/agentos/atoms/
 *       符合项目原始设计规范：atoms仅包含5个核心子模块
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_ATOMS_BENCHMARKS_H
#define AGENTOS_ATOMS_BENCHMARKS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* test_name;
    uint64_t iterations;
    uint64_t total_time_ns;
    uint64_t min_time_ns;
    uint64_t max_time_ns;
    double avg_time_ns;
    double throughput;
    bool passed;
    const char* sla_requirement;
    const char* failure_reason;
} benchmark_result_t;

typedef struct {
    uint64_t warmup_iterations;
    uint64_t test_iterations;
    uint64_t timeout_ms;
    bool verbose;
} benchmark_config_t;

int benchmark_ipc_latency(benchmark_result_t* result, const benchmark_config_t* config);
int benchmark_task_switch_latency(benchmark_result_t* result, const benchmark_config_t* config);
int benchmark_memory_retrieval_latency(benchmark_result_t* result, const benchmark_config_t* config);
int benchmark_memory_write_throughput(benchmark_result_t* result, const benchmark_config_t* config);
int benchmark_scheduler_throughput(benchmark_result_t* result, const benchmark_config_t* config);
int benchmark_execution_throughput(benchmark_result_t* result, const benchmark_config_t* config);
int benchmark_concurrent_performance(benchmark_result_t* result, const benchmark_config_t* config);
int benchmark_memory_efficiency(benchmark_result_t* result, const benchmark_config_t* config);
int benchmark_run_all(const benchmark_config_t* config);
void benchmark_print_result(const benchmark_result_t* result);
int benchmark_generate_json_report(const benchmark_result_t* results, size_t count, char** json_output);
benchmark_config_t benchmark_get_default_config(void);
bool benchmark_verify_sla(const benchmark_result_t* result, uint64_t sla_threshold_ns);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ATOMS_BENCHMARKS_H */
