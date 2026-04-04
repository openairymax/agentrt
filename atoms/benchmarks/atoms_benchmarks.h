/**
 * @file atoms_benchmarks.h
 * @brief AgentOS Atoms模块性能基准测试接口
 *
 * 提供标准化的性能基准测试接口，用于验证系统是否满足性能SLA要求：
 * - IPC延迟: <1μs
 * - 任务切换延迟: <1ms
 * - 记忆检索延迟: <10ms
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

/**
 * @addtogroup benchmarks
 * @{
 */

/**
 * @brief 基准测试结果结构
 */
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

/**
 * @brief 基准测试配置
 */
typedef struct {
    uint64_t warmup_iterations;
    uint64_t test_iterations;
    uint64_t timeout_ms;
    bool verbose;
} benchmark_config_t;

/**
 * @brief IPC延迟基准测试
 * @param[out] result 测试结果
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 0成功，非0失败
 *
 * @details
 * 测试原子内核IPC机制的平均延迟，验证是否满足<1μs的SLA要求。
 * 测试场景：
 * 1. 单次消息发送和接收延迟
 * 2. 批量消息发送延迟
 * 3. 高并发下的IPC延迟
 */
int benchmark_ipc_latency(benchmark_result_t* result, const benchmark_config_t* config);

/**
 * @brief 任务切换延迟基准测试
 * @param[out] result 测试结果
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 0成功，非0失败
 *
 * @details
 * 测试任务切换的平均延迟，验证是否满足<1ms的SLA要求。
 * 测试场景：
 * 1. 空任务切换延迟
 * 2. 带状态的任务切换延迟
 * 3. 跨线程的任务切换延迟
 */
int benchmark_task_switch_latency(benchmark_result_t* result, const benchmark_config_t* config);

/**
 * @brief 记忆检索延迟基准测试
 * @param[out] result 测试结果
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 0成功，非0失败
 *
 * @details
 * 测试记忆系统检索的平均延迟，验证是否满足<10ms的SLA要求。
 * 测试场景：
 * 1. 精确匹配检索延迟
 * 2. 相似度检索延迟
 * 3. 混合检索延迟
 */
int benchmark_memory_retrieval_latency(benchmark_result_t* result, const benchmark_config_t* config);

/**
 * @brief 记忆写入吞吐量基准测试
 * @param[out] result 测试结果
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 0成功，非0失败
 *
 * @details
 * 测试记忆系统的写入吞吐量。
 * 测试场景：
 * 1. 单条记录写入吞吐量
 * 2. 批量写入吞吐量
 * 3. 不同重要性级别的写入性能
 */
int benchmark_memory_write_throughput(benchmark_result_t* result, const benchmark_config_t* config);

/**
 * @brief 调度器吞吐量基准测试
 * @param[out] result 测试结果
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 0成功，非0失败
 *
 * @details
 * 测试任务调度器的吞吐量。
 * 测试场景：
 * 1. 任务提交吞吐量
 * 2. 任务完成通知吞吐量
 * 3. 高优先级任务调度性能
 */
int benchmark_scheduler_throughput(benchmark_result_t* result, const benchmark_config_t* config);

/**
 * @brief 执行引擎吞吐量基准测试
 * @param[out] result 测试结果
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 0成功，非0失败
 *
 * @details
 * 测试执行引擎的任务处理吞吐量。
 * 测试场景：
 * 1. 空任务执行吞吐量
 * 2. 计算密集型任务执行吞吐量
 * 3. IO密集型任务执行吞吐量
 */
int benchmark_execution_throughput(benchmark_result_t* result, const benchmark_config_t* config);

/**
 * @brief 并发性能基准测试
 * @param[out] result 测试结果
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 0成功，非0失败
 *
 * @details
 * 测试系统在并发负载下的性能表现。
 * 测试场景：
 * 1. 多线程并发IPC性能
 * 2. 多任务并发执行性能
 * 3. 混合负载下的系统稳定性
 */
int benchmark_concurrent_performance(benchmark_result_t* result, const benchmark_config_t* config);

/**
 * @brief 内存使用效率基准测试
 * @param[out] result 测试结果
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 0成功，非0失败
 *
 * @details
 * 测试系统的内存使用效率。
 * 测试场景：
 * 1. 内存分配性能
 * 2. 内存池使用效率
 * 3. 内存碎片化程度
 */
int benchmark_memory_efficiency(benchmark_result_t* result, const benchmark_config_t* config);

/**
 * @brief 运行所有基准测试
 * @param[in] config 测试配置（可为NULL，使用默认配置）
 * @return 通过的测试数量
 *
 * @details
 * 运行完整的基准测试套件，验证系统是否满足所有性能SLA要求。
 * 输出格式化的测试报告。
 */
int benchmark_run_all(const benchmark_config_t* config);

/**
 * @brief 打印基准测试结果
 * @param[in] result 测试结果
 *
 * @details
 * 以人类可读的格式打印基准测试结果。
 */
void benchmark_print_result(const benchmark_result_t* result);

/**
 * @brief 生成JSON格式的基准测试报告
 * @param[in] results 测试结果数组
 * @param[in] count 结果数量
 * @param[out] json_output JSON字符串（调用者负责释放）
 * @return 0成功，非0失败
 */
int benchmark_generate_json_report(const benchmark_result_t* results, size_t count, char** json_output);

/**
 * @brief 获取默认的基准测试配置
 * @return 默认配置
 */
benchmark_config_t benchmark_get_default_config(void);

/**
 * @brief 验证测试结果是否满足SLA
 * @param[in] result 测试结果
 * @param[in] sla_threshold_ns SLA阈值（纳秒）
 * @return true如果满足SLA，false否则
 */
bool benchmark_verify_sla(const benchmark_result_t* result, uint64_t sla_threshold_ns);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_ATOMS_BENCHMARKS_H */
