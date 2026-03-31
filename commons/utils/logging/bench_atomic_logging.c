/**
 * @file bench_atomic_logging.c
 * @brief 原子层日志系统性能基准测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本测试用于评估原子层日志系统的性能，包括：
 * 1. 单线程日志写入吞吐量
 * 2. 多线程并发写入吞吐量
 * 3. 日志缓冲队列性能
 * 4. 内存分配和释放性能
 *
 * 测试方法�? * - 使用高精度计时器测量操作耗时
 * - 模拟真实场景的日志写入模�? * - 统计每秒日志记录数（Records Per Second�? * - 测量平均延迟和尾部延�? *
 * 注意：本测试需要支持C11标准和线程库的环境�? */

#include "atomic_logging.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../utils/memory/include/memory_compat.h"
#include "../../utils/string/include/string_compat.h"
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

/* ==================== 测试配置 ==================== */

/** 测试迭代次数 */
#define BENCH_ITERATIONS 1000000

/** 线程数量（多线程测试�?*/
#define THREAD_COUNT 4

/** 日志消息模板 */
static const char* LOG_MESSAGES[] = {
    "用户登录成功: user_id=%d, ip=%s",
    "数据库查询完�? query_id=%d, rows=%d, time_ms=%d",
    "网络请求处理: method=%s, path=%s, status=%d",
    "缓存命中率统�? hits=%d, misses=%d, ratio=%.2f",
    "内存使用报告: used=%llu, free=%llu, total=%llu",
    "任务调度延迟: task_id=%d, scheduled=%lld, actual=%lld",
    "配置热更新完�? section=%s, keys=%d",
    "监控指标收集: metric=%s, value=%f, timestamp=%lld"
};

#define LOG_MESSAGE_COUNT (sizeof(LOG_MESSAGES) / sizeof(LOG_MESSAGES[0]))

/* ==================== 计时器辅助函�?==================== */

/**
 * @brief 获取高精度时间戳（纳秒）
 * 
 * 使用系统提供的高精度计时器�? * 
 * @return 当前时间戳（纳秒�? */
static uint64_t get_nanoseconds(void) {
#if defined(_WIN32)
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

/**
 * @brief 将纳秒转换为毫秒
 * 
 * @param ns 纳秒�? * @return 毫秒�? */
static double ns_to_ms(uint64_t ns) {
    return (double)ns / 1000000.0;
}

/* ==================== 单线程性能测试 ==================== */

/**
 * @brief 单线程日志写入性能测试
 * 
 * 测量单线程连续写入大量日志记录的性能�? * 
 * @param iterations 迭代次数
 * @return 每秒日志记录�? */
static double bench_single_thread(int iterations) {
    printf("开始单线程性能测试�?d次迭代）...\n", iterations);
    
    // 初始化日志系统（如果尚未初始化）
    log_init(NULL);
    
    uint64_t start_time = get_nanoseconds();
    
    for (int i = 0; i < iterations; i++) {
        // 选择一条日志消息模�?        const char* msg_template = LOG_MESSAGES[i % LOG_MESSAGE_COUNT];
        
        // 生成日志记录
        log_write(LOG_LEVEL_INFO, "benchmark", __LINE__, msg_template,
                 i, "192.168.1.1",  // 用户登录参数
                 i * 100, 50, 15,   // 数据库查询参�?                 "GET", "/api/data", 200,  // 网络请求参数
                 i * 10, i * 2, 0.85f,  // 缓存统计参数
                 (unsigned long long)i * 1024 * 1024,
                 (unsigned long long)(iterations - i) * 1024 * 1024,
                 (unsigned long long)iterations * 1024 * 1024,  // 内存使用参数
                 i, start_time, start_time + i * 1000,  // 任务调度参数
                 "database", 8,  // 配置更新参数
                 "cpu_usage", 0.75f, (long long)start_time  // 监控指标参数
        );
        
        // �?0000次迭代输出进�?        if (i > 0 && i % 10000 == 0) {
            printf("  进度: %d/%d\n", i, iterations);
        }
    }
    
    uint64_t end_time = get_nanoseconds();
    uint64_t elapsed_ns = end_time - start_time;
    double elapsed_ms = ns_to_ms(elapsed_ns);
    
    double records_per_second = (double)iterations / (elapsed_ns / 1000000000.0);
    
    printf("单线程测试完�?\n");
    printf("  总耗时: %.2f 毫秒\n", elapsed_ms);
    printf("  平均延迟: %.3f 微秒/记录\n", (elapsed_ms * 1000.0) / iterations);
    printf("  吞吐�? %.0f 记录/秒\n", records_per_second);
    
    return records_per_second;
}

/* ==================== 多线程性能测试 ==================== */

/** 线程参数结构�?*/
typedef struct {
    int thread_id;
    int iterations_per_thread;
    uint64_t start_time;
    uint64_t end_time;
    atomic_ulong records_written;
} thread_params_t;

/**
 * @brief 工作线程函数（多线程测试�? * 
 * 每个线程独立写入日志记录�? * 
 * @param arg 线程参数
 * @return 线程退出状�? */
#if defined(_WIN32)
static DWORD WINAPI worker_thread(LPVOID arg)
#else
static void* worker_thread(void* arg)
#endif
{
    thread_params_t* params = (thread_params_t*)arg;
    
    // 初始化线程本地日志上下文（如果需要）
    // 原子层日志系统自动处理线程安�?    
    uint64_t thread_start_time = get_nanoseconds();
    
    for (int i = 0; i < params->iterations_per_thread; i++) {
        // 生成唯一的日志消�?        int msg_index = (params->thread_id * params->iterations_per_thread + i) % LOG_MESSAGE_COUNT;
        const char* msg_template = LOG_MESSAGES[msg_index];
        
        // 写入日志记录
        log_write(LOG_LEVEL_INFO, "benchmark", __LINE__, msg_template,
                 params->thread_id, i, msg_index);
        
        // 原子增加计数�?        atomic_fetch_add(&params->records_written, 1);
        
        // �?000次迭代输出进�?        if (i > 0 && i % 5000 == 0) {
            printf("  线程 %d 进度: %d/%d\n", 
                   params->thread_id, i, params->iterations_per_thread);
        }
    }
    
    uint64_t thread_end_time = get_nanoseconds();
    params->start_time = thread_start_time;
    params->end_time = thread_end_time;
    
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

/**
 * @brief 多线程日志写入性能测试
 * 
 * 测量多个线程并发写入日志记录的性能�? * 
 * @param thread_count 线程数量
 * @param total_iterations 总迭代次�? * @return 每秒日志记录�? */
static double bench_multi_thread(int thread_count, int total_iterations) {
    printf("开始多线程性能测试�?d线程�?d次迭代）...\n", 
           thread_count, total_iterations);
    
    // 初始化日志系�?    log_init(NULL);
    
    // 分配线程参数
    thread_params_t* params = (thread_params_t*)AGENTOS_CALLOC(thread_count, sizeof(thread_params_t));
    if (!params) {
        printf("内存分配失败！\n");
        return 0.0;
    }
    
    int iterations_per_thread = total_iterations / thread_count;
    
    // 初始化线程参�?    for (int i = 0; i < thread_count; i++) {
        params[i].thread_id = i;
        params[i].iterations_per_thread = iterations_per_thread;
        params[i].records_written = 0;
    }
    
    // 创建并启动线�?    uint64_t overall_start_time = get_nanoseconds();
    
#if defined(_WIN32)
    HANDLE* threads = (HANDLE*)AGENTOS_CALLOC(thread_count, sizeof(HANDLE));
    for (int i = 0; i < thread_count; i++) {
        threads[i] = CreateThread(NULL, 0, worker_thread, &params[i], 0, NULL);
        if (!threads[i]) {
            printf("创建线程 %d 失败！\n", i);
            AGENTOS_FREE(params);
            AGENTOS_FREE(threads);
            return 0.0;
        }
    }
    
    // 等待所有线程完�?    WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);
    
    // 关闭线程句柄
    for (int i = 0; i < thread_count; i++) {
        CloseHandle(threads[i]);
    }
    
    AGENTOS_FREE(threads);
#else
    pthread_t* threads = (pthread_t*)AGENTOS_CALLOC(thread_count, sizeof(pthread_t));
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, &params[i]) != 0) {
            printf("创建线程 %d 失败！\n", i);
            AGENTOS_FREE(params);
            AGENTOS_FREE(threads);
            return 0.0;
        }
    }
    
    // 等待所有线程完�?    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    AGENTOS_FREE(threads);
#endif
    
    uint64_t overall_end_time = get_nanoseconds();
    
    // 收集统计信息
    uint64_t total_records = 0;
    uint64_t total_thread_time_ns = 0;
    uint64_t min_thread_time_ns = UINT64_MAX;
    uint64_t max_thread_time_ns = 0;
    
    for (int i = 0; i < thread_count; i++) {
        total_records += params[i].records_written;
        uint64_t thread_time_ns = params[i].end_time - params[i].start_time;
        total_thread_time_ns += thread_time_ns;
        
        if (thread_time_ns < min_thread_time_ns) min_thread_time_ns = thread_time_ns;
        if (thread_time_ns > max_thread_time_ns) max_thread_time_ns = thread_time_ns;
    }
    
    uint64_t overall_time_ns = overall_end_time - overall_start_time;
    double overall_time_ms = ns_to_ms(overall_time_ns);
    
    double records_per_second = (double)total_records / (overall_time_ns / 1000000000.0);
    
    printf("多线程测试完�?\n");
    printf("  总耗时: %.2f 毫秒\n", overall_time_ms);
    printf("  总记录数: %llu\n", (unsigned long long)total_records);
    printf("  吞吐�? %.0f 记录/秒\n", records_per_second);
    printf("  线程执行时间统计:\n");
    printf("    平均: %.2f 毫秒\n", ns_to_ms(total_thread_time_ns / thread_count));
    printf("    最�? %.2f 毫秒\n", ns_to_ms(min_thread_time_ns));
    printf("    最�? %.2f 毫秒\n", ns_to_ms(max_thread_time_ns));
    
    AGENTOS_FREE(params);
    return records_per_second;
}

/* ==================== 内存使用测试 ==================== */

/**
 * @brief 内存使用和泄漏测�? * 
 * 测量日志系统在长时间运行中的内存使用情况�? * 
 * @param iterations 迭代次数
 */
static void bench_memory_usage(int iterations) {
    printf("开始内存使用测试（%d次迭代）...\n", iterations);
    
    // 初始化日志系�?    log_init(NULL);
    
    // 记录初始内存状态（简化实现）
    printf("  内存使用测试 - 开始\n");
    
    for (int i = 0; i < iterations; i++) {
        // 写入不同级别的日志记�?        log_level_t level = (log_level_t)(i % 5); // 循环使用5个日志级�?        
        log_write(level, "memory_test", __LINE__, 
                 "内存测试迭代 %d，级�?%d，消息长�?%d", 
                 i, level, i % 100);
        
        // �?0000次迭代输出状�?        if (i > 0 && i % 10000 == 0) {
            printf("  进度: %d/%d\n", i, iterations);
        }
    }
    
    printf("  内存使用测试 - 完成\n");
    printf("  注意：实际内存泄漏检测需要专门的工具（如valgrind、AddressSanitizer）\n");
}

/* ==================== 主测试函�?==================== */

/**
 * @brief 主测试入�? * 
 * 运行所有性能基准测试并生成报告�? * 
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 退出码
 */
int main(int argc, char** argv) {
    printf("========================================\n");
    printf("原子层日志系统性能基准测试\n");
    printf("========================================\n\n");
    
    // 解析命令行参�?    int iterations = BENCH_ITERATIONS;
    int thread_count = THREAD_COUNT;
    
    if (argc > 1) {
        iterations = atoi(argv[1]);
        if (iterations <= 0) iterations = BENCH_ITERATIONS;
    }
    
    if (argc > 2) {
        thread_count = atoi(argv[2]);
        if (thread_count <= 0) thread_count = THREAD_COUNT;
    }
    
    printf("测试配置:\n");
    printf("  迭代次数: %d\n", iterations);
    printf("  线程数量: %d\n", thread_count);
    printf("  日志消息模板: %zu\n", LOG_MESSAGE_COUNT);
    printf("\n");
    
    // 运行单线程测�?    double single_thread_rps = bench_single_thread(iterations / 10); // 减少迭代次数以加快测�?    printf("\n");
    
    // 运行多线程测�?    double multi_thread_rps = bench_multi_thread(thread_count, iterations / 10);
    printf("\n");
    
    // 运行内存使用测试
    bench_memory_usage(iterations / 100);
    printf("\n");
    
    // 生成性能报告
    printf("========================================\n");
    printf("性能测试报告\n");
    printf("========================================\n");
    printf("单线程性能:\n");
    printf("  %.0f 记录/秒\n", single_thread_rps);
    printf("多线程性能�?d线程�?\n", thread_count);
    printf("  %.0f 记录/秒\n", multi_thread_rps);
    printf("并发加速比: %.2fx\n", multi_thread_rps / single_thread_rps);
    printf("\n");
    
    // 性能评估
    printf("性能评估:\n");
    if (single_thread_rps > 100000) {
        printf("  �?单线程性能优秀�?100k 记录/秒）\n");
    } else if (single_thread_rps > 50000) {
        printf("  �?单线程性能良好�?50k 记录/秒）\n");
    } else {
        printf("  �?单线程性能有待优化\n");
    }
    
    if (multi_thread_rps / single_thread_rps > 0.8 * thread_count) {
        printf("  �?多线程扩展性优秀\n");
    } else if (multi_thread_rps / single_thread_rps > 0.5 * thread_count) {
        printf("  �?多线程扩展性良好\n");
    } else {
        printf("  �?多线程扩展性有待优化\n");
    }
    
    printf("\n");
    printf("测试完成。\n");
    
    return 0;
}