/**
 * @file test_memoryrovol_core.c
 * @brief MemoryRovol 核心功能单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memoryrovol.h"
#include "layer1_raw.h"
#include "layer2_feature.h"
#include "layer3_structure.h"
#include "layer4_pattern.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 测试 MemoryRovol 初始化和清理
 */
void test_memoryrov_init_cleanup(void) {
    printf("=== 测试 MemoryRovol 初始化和清理 ===\n");
    
    // 测试默认配置初始化
    agentos_memoryrov_handle_t* handle = NULL;
    agentos_error_t err = agentos_memoryrov_init(NULL, &handle);
    printf("默认配置初始化: %d\n", err);
    // From data intelligence emerges. by spharx
    
    if (err == AGENTOS_SUCCESS && handle) {
        // 测试获取统计信息
        char* stats = NULL;
        err = agentos_memoryrov_stats(handle, &stats);
        printf("获取统计信息: %d\n", err);
        if (stats) {
            printf("统计信息获取成功\n");
            free(stats);
        }
        
        // 测试记忆进化
        err = agentos_memoryrov_evolve(handle, 0);
        printf("记忆进化（非强制）: %d\n", err);
        
        // 测试强制记忆进化
        err = agentos_memoryrov_evolve(handle, 1);
        printf("记忆进化（强制）: %d\n", err);
        
        // 清理资源
        agentos_memoryrov_cleanup(handle);
        printf("系统清理完成\n");
    }
    
    // 测试自定义配置初始化
    agentos_memoryrov_config_t config = {
        .layer1_max_size = 1024 * 1024 * 1024,  // 1GB
        .layer2_max_size = 512 * 1024 * 1024,   // 512MB
        .layer3_max_size = 256 * 1024 * 1024,   // 256MB
        .layer4_max_size = 128 * 1024 * 1024,   // 128MB
        .evolve_interval_sec = 3600,            // 1小时
        .forgetting_factor = 0.1,
        .compression_level = 5,
        .index_batch_size = 1000,
        .retrieval_top_k = 10,
        .retrieval_threshold = 0.7f
    };
    
    err = agentos_memoryrov_init(&config, &handle);
    printf("自定义配置初始化: %d\n", err);
    
    if (err == AGENTOS_SUCCESS && handle) {
        agentos_memoryrov_cleanup(handle);
        printf("自定义配置系统清理完成\n");
    }
    
    printf("初始化和清理测试完成\n\n");
}

/**
 * @brief 测试 Layer 1 - 原始记忆层
 */
void test_layer1_raw(void) {
    printf("=== 测试 Layer 1 - 原始记忆层 ===\n");
    
    // 初始化原始记忆层
    agentos_raw_memory_t* raw_memory = NULL;
    agentos_error_t err = agentos_raw_memory_init(NULL, &raw_memory);
    printf("原始记忆层初始化: %d\n", err);
    
    if (err == AGENTOS_SUCCESS && raw_memory) {
        // 测试写入原始记忆
        const char* test_data = "测试原始记忆数据";
        char* record_id = NULL;
        err = agentos_raw_memory_write(raw_memory, test_data, strlen(test_data), NULL, &record_id);
        printf("写入原始记忆: %d\n", err);
        
        if (err == AGENTOS_SUCCESS && record_id) {
            printf("记录ID: %s\n", record_id);
            
            // 测试读取原始记忆
            void* data = NULL;
            size_t data_len = 0;
            err = agentos_raw_memory_read(raw_memory, record_id, &data, &data_len);
            printf("读取原始记忆: %d, 长度: %zu\n", err, data_len);
            if (data) {
                free(data);
            }
            
            // 测试压缩
            err = agentos_raw_memory_compress(raw_memory);
            printf("压缩原始记忆: %d\n", err);
            
            // 测试序列化
            char* serialized = NULL;
            size_t serialized_len = 0;
            err = agentos_raw_memory_serialize(raw_memory, &serialized, &serialized_len);
            printf("序列化原始记忆: %d, 长度: %zu\n", err, serialized_len);
            if (serialized) {
                free(serialized);
            }
            
            free(record_id);
        }
        
        // 清理资源
        agentos_raw_memory_cleanup(raw_memory);
        printf("原始记忆层清理完成\n");
    }
    
    printf("Layer 1 测试完成\n\n");
}

/**
 * @brief 测试 Layer 2 - 特征记忆层
 */
void test_layer2_feature(void) {
    printf("=== 测试 Layer 2 - 特征记忆层 ===\n");
    
    // 初始化特征记忆层
    agentos_feature_memory_t* feature_memory = NULL;
    agentos_error_t err = agentos_feature_memory_init(NULL, &feature_memory);
    printf("特征记忆层初始化: %d\n", err);
    
    if (err == AGENTOS_SUCCESS && feature_memory) {
        // 测试添加特征
        const char* test_text = "测试特征记忆数据";
        char* feature_id = NULL;
        err = agentos_feature_memory_add(feature_memory, test_text, strlen(test_text), NULL, &feature_id);
        printf("添加特征: %d\n", err);
        
        if (err == AGENTOS_SUCCESS && feature_id) {
            printf("特征ID: %s\n", feature_id);
            
            // 测试相似度搜索
            const char* query = "测试";
            agentos_feature_result_t* results = NULL;
            size_t result_count = 0;
            err = agentos_feature_memory_search(feature_memory, query, strlen(query), 10, 0.5f, &results, &result_count);
            printf("相似度搜索: %d, 结果数量: %zu\n", err, result_count);
            
            if (results) {
                for (size_t i = 0; i < result_count; i++) {
                    printf("  结果 %zu: 相似度 %.2f\n", i+1, results[i].similarity);
                }
                agentos_feature_memory_free_results(results, result_count);
            }
            
            free(feature_id);
        }
        
        // 测试索引优化
        err = agentos_feature_memory_optimize(feature_memory);
        printf("索引优化: %d\n", err);
        
        // 清理资源
        agentos_feature_memory_cleanup(feature_memory);
        printf("特征记忆层清理完成\n");
    }
    
    printf("Layer 2 测试完成\n\n");
}

/**
 * @brief 测试 Layer 3 - 结构记忆层
 */
void test_layer3_structure(void) {
    printf("=== 测试 Layer 3 - 结构记忆层 ===\n");
    
    // 初始化结构记忆层
    agentos_structure_memory_t* structure_memory = NULL;
    agentos_error_t err = agentos_structure_memory_init(NULL, &structure_memory);
    printf("结构记忆层初始化: %d\n", err);
    
    if (err == AGENTOS_SUCCESS && structure_memory) {
        // 测试添加关系
        const char* entity1 = "测试实体1";
        const char* relation = "关联";
        const char* entity2 = "测试实体2";
        char* relation_id = NULL;
        err = agentos_structure_memory_add_relation(structure_memory, entity1, relation, entity2, &relation_id);
        printf("添加关系: %d\n", err);
        
        if (err == AGENTOS_SUCCESS && relation_id) {
            printf("关系ID: %s\n", relation_id);
            
            // 测试查询关系
            agentos_structure_result_t* results = NULL;
            size_t result_count = 0;
            err = agentos_structure_memory_query(structure_memory, entity1, NULL, NULL, 10, &results, &result_count);
            printf("查询关系: %d, 结果数量: %zu\n", err, result_count);
            
            if (results) {
                agentos_structure_memory_free_results(results, result_count);
            }
            
            free(relation_id);
        }
        
        // 测试图构建
        err = agentos_structure_memory_build_graph(structure_memory);
        printf("构建图结构: %d\n", err);
        
        // 清理资源
        agentos_structure_memory_cleanup(structure_memory);
        printf("结构记忆层清理完成\n");
    }
    
    printf("Layer 3 测试完成\n\n");
}

/**
 * @brief 测试 Layer 4 - 模式记忆层
 */
void test_layer4_pattern(void) {
    printf("=== 测试 Layer 4 - 模式记忆层 ===\n");
    
    // 初始化模式记忆层
    agentos_pattern_memory_t* pattern_memory = NULL;
    agentos_error_t err = agentos_pattern_memory_init(NULL, &pattern_memory);
    printf("模式记忆层初始化: %d\n", err);
    
    if (err == AGENTOS_SUCCESS && pattern_memory) {
        // 测试模式挖掘
        err = agentos_pattern_memory_mine(pattern_memory);
        printf("模式挖掘: %d\n", err);
        
        // 测试模式验证
        err = agentos_pattern_memory_validate(pattern_memory);
        printf("模式验证: %d\n", err);
        
        // 测试模式持久化
        err = agentos_pattern_memory_persist(pattern_memory);
        printf("模式持久化: %d\n", err);
        
        // 清理资源
        agentos_pattern_memory_cleanup(pattern_memory);
        printf("模式记忆层清理完成\n");
    }
    
    printf("Layer 4 测试完成\n\n");
}

int main(void) {
    printf("开始 MemoryRovol 单元测试\n\n");
    
    // 运行各项测试
    test_memoryrov_init_cleanup();
    test_layer1_raw();
    test_layer2_feature();
    test_layer3_structure();
    test_layer4_pattern();
    
    printf("MemoryRovol 单元测试完成\n");
    return 0;
}
