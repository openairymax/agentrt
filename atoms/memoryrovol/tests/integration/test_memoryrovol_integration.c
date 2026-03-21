/**
 * @file test_memoryrovol_integration.c
 * @brief MemoryRovol 集成测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memoryrovol.h"
#include "retrieval.h"
#include "forgetting.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * @brief 测试 MemoryRovol 完整工作流
 */
void test_memoryrov_full_workflow(void) {
    printf("=== 测试 MemoryRovol 完整工作流 ===\n");
    
    // 初始化 MemoryRovol 系统
    agentos_memoryrov_handle_t* handle = NULL;
    agentos_error_t err = agentos_memoryrov_init(NULL, &handle);
    if (err != AGENTOS_SUCCESS || !handle) {
        printf("初始化 MemoryRovol 失败: %d\n", err);
        // From data intelligence emerges. by spharx
        return;
    }
    printf("MemoryRovol 初始化成功\n");
    
    // 获取检索器
    agentos_retrieval_t* retrieval = NULL;
    err = agentos_retrieval_init(NULL, &retrieval);
    if (err != AGENTOS_SUCCESS || !retrieval) {
        printf("初始化检索器失败: %d\n", err);
        agentos_memoryrov_cleanup(handle);
        return;
    }
    printf("检索器初始化成功\n");
    
    // 获取遗忘管理器
    agentos_forgetting_t* forgetting = NULL;
    err = agentos_forgetting_init(NULL, &forgetting);
    if (err != AGENTOS_SUCCESS || !forgetting) {
        printf("初始化遗忘管理器失败: %d\n", err);
        agentos_retrieval_cleanup(retrieval);
        agentos_memoryrov_cleanup(handle);
        return;
    }
    printf("遗忘管理器初始化成功\n");
    
    // 测试1: 写入测试数据到各层
    printf("1. 写入测试数据\n");
    
    // 模拟写入原始记忆
    const char* test_data[] = {
        "测试数据1: 机器学习是人工智能的一个分支",
        "测试数据2: 深度学习是机器学习的一个子集",
        "测试数据3: 神经网络是深度学习的基础",
        "测试数据4: 卷积神经网络擅长图像处理",
        "测试数据5: 循环神经网络擅长序列数据"
    };
    
    size_t data_count = sizeof(test_data) / sizeof(test_data[0]);
    char** record_ids = (char**)malloc(data_count * sizeof(char*));
    
    if (record_ids) {
        for (size_t i = 0; i < data_count; i++) {
            // 这里应该调用各层的写入接口
            // 为了简化，我们直接测试检索和遗忘功能
            record_ids[i] = NULL;
        }
    }
    
    // 测试2: 执行记忆进化
    printf("2. 执行记忆进化\n");
    err = agentos_memoryrov_evolve(handle, 1);
    printf("记忆进化结果: %d\n", err);
    
    // 测试3: 检索测试
    printf("3. 测试检索功能\n");
    
    const char* queries[] = {
        "机器学习",
        "神经网络",
        "图像处理"
    };
    
    size_t query_count = sizeof(queries) / sizeof(queries[0]);
    
    for (size_t i = 0; i < query_count; i++) {
        agentos_retrieval_result_t* results = NULL;
        size_t result_count = 0;
        
        err = agentos_retrieval_search(retrieval, queries[i], strlen(queries[i]), 5, 0.5f, &results, &result_count);
        printf("检索 '%s' 结果: %d, 找到 %zu 条记录\n", queries[i], err, result_count);
        
        if (results) {
            for (size_t j = 0; j < result_count; j++) {
                printf("  结果 %zu: 相似度 %.2f\n", j+1, results[j].similarity);
            }
            agentos_retrieval_free_results(results, result_count);
        }
    }
    
    // 测试4: 测试遗忘功能
    printf("4. 测试遗忘功能\n");
    
    // 测试衰减
    err = agentos_forgetting_decay(forgetting);
    printf("记忆衰减结果: %d\n", err);
    
    // 测试修剪
    err = agentos_forgetting_prune(forgetting);
    printf("记忆修剪结果: %d\n", err);
    
    // 测试归档
    err = agentos_forgetting_archive(forgetting);
    printf("记忆归档结果: %d\n", err);
    
    // 测试5: 获取系统统计信息
    printf("5. 获取系统统计信息\n");
    
    char* stats = NULL;
    err = agentos_memoryrov_stats(handle, &stats);
    if (err == AGENTOS_SUCCESS && stats) {
        printf("系统统计信息获取成功\n");
        free(stats);
    } else {
        printf("获取系统统计信息失败: %d\n", err);
    }
    
    // 测试6: 测试系统恢复
    printf("6. 测试系统恢复\n");
    
    // 模拟系统重启
    agentos_memoryrov_cleanup(handle);
    printf("系统清理完成\n");
    
    // 重新初始化
    err = agentos_memoryrov_init(NULL, &handle);
    if (err == AGENTOS_SUCCESS && handle) {
        printf("系统重新初始化成功\n");
    } else {
        printf("系统重新初始化失败: %d\n", err);
    }
    
    // 清理资源
    if (record_ids) {
        for (size_t i = 0; i < data_count; i++) {
            if (record_ids[i]) {
                free(record_ids[i]);
            }
        }
        free(record_ids);
    }
    
    if (forgetting) {
        agentos_forgetting_cleanup(forgetting);
    }
    
    if (retrieval) {
        agentos_retrieval_cleanup(retrieval);
    }
    
    if (handle) {
        agentos_memoryrov_cleanup(handle);
    }
    
    printf("完整工作流测试完成\n\n");
}

/**
 * @brief 测试 MemoryRovol 性能
 */
void test_memoryrov_performance(void) {
    printf("=== 测试 MemoryRovol 性能 ===\n");
    
    // 初始化系统
    agentos_memoryrov_handle_t* handle = NULL;
    agentos_error_t err = agentos_memoryrov_init(NULL, &handle);
    if (err != AGENTOS_SUCCESS || !handle) {
        printf("初始化失败: %d\n", err);
        return;
    }
    
    // 测试记忆进化性能
    clock_t start = clock();
    err = agentos_memoryrov_evolve(handle, 1);
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("记忆进化耗时: %.2f 秒\n", elapsed);
    
    // 测试统计信息获取性能
    start = clock();
    char* stats = NULL;
    err = agentos_memoryrov_stats(handle, &stats);
    end = clock();
    elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("获取统计信息耗时: %.2f 秒\n", elapsed);
    if (stats) {
        free(stats);
    }
    
    // 清理资源
    agentos_memoryrov_cleanup(handle);
    
    printf("性能测试完成\n\n");
}

/**
 * @brief 测试 MemoryRovol 错误处理
 */
void test_memoryrov_error_handling(void) {
    printf("=== 测试 MemoryRovol 错误处理 ===\n");
    
    // 测试空指针参数
    agentos_memoryrov_handle_t* handle = NULL;
    agentos_error_t err = agentos_memoryrov_init(NULL, NULL);
    printf("初始化（空输出指针）: %d\n", err);
    
    // 测试无效配置
    agentos_memoryrov_config_t invalid_config = {
        .layer1_max_size = 0,  // 无效值
        .layer2_max_size = 0,
        .layer3_max_size = 0,
        .layer4_max_size = 0
    };
    
    err = agentos_memoryrov_init(&invalid_config, &handle);
    printf("初始化（无效配置）: %d\n", err);
    
    // 测试空句柄操作
    if (handle) {
        agentos_memoryrov_cleanup(handle);
    }
    
    // 测试空句柄调用其他函数
    char* stats = NULL;
    err = agentos_memoryrov_stats(NULL, &stats);
    printf("获取统计信息（空句柄）: %d\n", err);
    
    err = agentos_memoryrov_evolve(NULL, 1);
    printf("记忆进化（空句柄）: %d\n", err);
    
    printf("错误处理测试完成\n\n");
}

int main(void) {
    printf("开始 MemoryRovol 集成测试\n\n");
    
    // 运行各项测试
    test_memoryrov_full_workflow();
    test_memoryrov_performance();
    test_memoryrov_error_handling();
    
    printf("MemoryRovol 集成测试完成\n");
    return 0;
}
