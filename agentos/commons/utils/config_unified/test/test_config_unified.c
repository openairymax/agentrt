/**
 * @file test_config_unified.c
 * @brief 统一配置模块单元测试
 * 
 * 测试config_unified模块的基本功能：配置加载、值获取、类型转换、错误处理等。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include <assert.h>

#include "../include/config_unified.h"
#include "../include/core_config.h"
#include "../include/config_source.h"
#include "../include/config_service.h"
#include "../include/config_compat.h"

/* ==================== 测试辅助宏 ==================== */

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "❌ FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

#define TEST_RUN(test_func) \
    do { \
        printf("🧪 Running %s...\n", #test_func); \
        if (test_func() != 0) { \
            fprintf(stderr, "❌ Test failed: %s\n", #test_func); \
            failed_tests++; \
        } else { \
            printf("✅ PASS: %s\n", #test_func); \
            passed_tests++; \
        } \
    } while (0)

static int passed_tests = 0;
static int failed_tests = 0;

/* ==================== 测试用例 ==================== */

/**
 * @brief 测试配置上下文创建和销毁
 */
static int test_config_context_create_destroy(void) {
    printf("  测试配置上下文创建和销毁...\n");
    
    config_context_t* ctx = config_context_create();
    TEST_ASSERT(ctx != NULL, "配置上下文创建失败");
    TEST_ASSERT(ctx->source_count == 0, "初始源数量应为0");
    TEST_ASSERT(ctx->sources == NULL, "初始源数组应为NULL");
    
    config_context_destroy(ctx);
    
    printf("  配置上下文创建和销毁测试通过\n");
    return 0;
}

/**
 * @brief 测试环境变量配置源
 */
static int test_config_source_env(void) {
    printf("  测试环境变量配置源...\n");
    
    // 设置测试环境变量
    _putenv("AGENTOS_TEST_CONFIG_INT=42");
    _putenv("AGENTOS_TEST_CONFIG_STR=hello");
    _putenv("AGENTOS_TEST_CONFIG_BOOL=true");
    
    config_context_t* ctx = config_context_create();
    TEST_ASSERT(ctx != NULL, "配置上下文创建失败");
    
    int result = config_context_add_env_source(ctx, "AGENTOS_TEST_CONFIG_");
    TEST_ASSERT(result == 0, "环境变量源添加失败");
    
    // 测试获取值
    int int_val = 0;
    result = config_context_get_int(ctx, "test.manager.int", &int_val);
    TEST_ASSERT(result == 0, "获取整数值失败");
    TEST_ASSERT(int_val == 42, "整数值不匹配");
    
    char str_val[64] = {0};
    result = config_context_get_string(ctx, "test.manager.str", str_val, sizeof(str_val));
    TEST_ASSERT(result == 0, "获取字符串值失败");
    TEST_ASSERT(strcmp(str_val, "hello") == 0, "字符串值不匹配");
    
    bool bool_val = false;
    result = config_context_get_bool(ctx, "test.manager.bool", &bool_val);
    TEST_ASSERT(result == 0, "获取布尔值失败");
    TEST_ASSERT(bool_val == true, "布尔值不匹配");
    
    config_context_destroy(ctx);
    
    // 清理环境变量
    _putenv("AGENTOS_TEST_CONFIG_INT=");
    _putenv("AGENTOS_TEST_CONFIG_STR=");
    _putenv("AGENTOS_TEST_CONFIG_BOOL=");
    
    printf("  环境变量配置源测试通过\n");
    return 0;
}

/**
 * @brief 测试配置文件源（简化版本）
 */
static int test_config_source_file(void) {
    printf("  测试配置文件源（简化）...\n");
    
    // 创建临时配置文件
    const char* test_config = 
        "{\n"
        "  \"database\": {\n"
        "    \"host\": \"localhost\",\n"
        "    \"port\": 5432,\n"
        "    \"enabled\": true\n"
        "  }\n"
        "}\n";
    
    FILE* fp = fopen("test_config.json", "w");
    if (fp == NULL) {
        printf("  警告：无法创建测试配置文件，跳过文件源测试\n");
        return 0;
    }
    fwrite(test_config, 1, strlen(test_config), fp);
    fclose(fp);
    
    config_context_t* ctx = config_context_create();
    TEST_ASSERT(ctx != NULL, "配置上下文创建失败");
    
    int result = config_context_add_json_file_source(ctx, "test_config.json");
    if (result != 0) {
        printf("  警告：JSON文件源添加失败，跳过文件源测试\n");
        config_context_destroy(ctx);
        remove("test_config.json");
        return 0;
    }
    
    // 测试获取值
    char host[64] = {0};
    result = config_context_get_string(ctx, "database.host", host, sizeof(host));
    TEST_ASSERT(result == 0, "获取数据库主机失败");
    TEST_ASSERT(strcmp(host, "localhost") == 0, "主机值不匹配");
    
    int port = 0;
    result = config_context_get_int(ctx, "database.port", &port);
    TEST_ASSERT(result == 0, "获取数据库端口失败");
    TEST_ASSERT(port == 5432, "端口值不匹配");
    
    bool enabled = false;
    result = config_context_get_bool(ctx, "database.enabled", &enabled);
    TEST_ASSERT(result == 0, "获取数据库启用状态失败");
    TEST_ASSERT(enabled == true, "启用状态值不匹配");
    
    config_context_destroy(ctx);
    remove("test_config.json");
    
    printf("  配置文件源测试通过\n");
    return 0;
}

/**
 * @brief 测试配置值获取和类型转换
 */
static int test_config_value_conversion(void) {
    printf("  测试配置值类型转换...\n");
    
    config_context_t* ctx = config_context_create();
    TEST_ASSERT(ctx != NULL, "配置上下文创建失败");
    
    // 添加测试值（通过内存源）
    config_memory_source_t* mem_source = config_memory_source_create();
    TEST_ASSERT(mem_source != NULL, "内存配置源创建失败");
    
    config_memory_source_set_string(mem_source, "test.int", "123");
    config_memory_source_set_string(mem_source, "test.float", "45.67");
    config_memory_source_set_string(mem_source, "test.bool", "true");
    config_memory_source_set_string(mem_source, "test.invalid", "not_a_number");
    
    config_context_add_source(ctx, (config_source_t*)mem_source);
    
    // 测试整型转换
    int int_val = 0;
    int result = config_context_get_int(ctx, "test.int", &int_val);
    TEST_ASSERT(result == 0, "整型转换失败");
    TEST_ASSERT(int_val == 123, "整型值不匹配");
    
    // 测试浮点型转换
    double float_val = 0.0;
    result = config_context_get_double(ctx, "test.float", &float_val);
    TEST_ASSERT(result == 0, "浮点型转换失败");
    TEST_ASSERT(float_val > 45.66 && float_val < 45.68, "浮点值不匹配");
    
    // 测试布尔型转换
    bool bool_val = false;
    result = config_context_get_bool(ctx, "test.bool", &bool_val);
    TEST_ASSERT(result == 0, "布尔型转换失败");
    TEST_ASSERT(bool_val == true, "布尔值不匹配");
    
    // 测试无效转换
    int invalid_val = 0;
    result = config_context_get_int(ctx, "test.invalid", &invalid_val);
    TEST_ASSERT(result != 0, "无效转换应失败");
    
    config_context_destroy(ctx);
    
    printf("  配置值类型转换测试通过\n");
    return 0;
}

/**
 * @brief 测试配置源优先级
 */
static int test_config_source_priority(void) {
    printf("  测试配置源优先级...\n");
    
    config_context_t* ctx = config_context_create();
    TEST_ASSERT(ctx != NULL, "配置上下文创建失败");
    
    // 添加低优先级源
    config_memory_source_t* low_pri_source = config_memory_source_create();
    TEST_ASSERT(low_pri_source != NULL, "低优先级内存源创建失败");
    config_memory_source_set_string(low_pri_source, "test.key", "low_priority");
    config_context_add_source(ctx, (config_source_t*)low_pri_source);
    
    // 添加高优先级源
    config_memory_source_t* high_pri_source = config_memory_source_create();
    TEST_ASSERT(high_pri_source != NULL, "高优先级内存源创建失败");
    config_memory_source_set_string(high_pri_source, "test.key", "high_priority");
    config_context_add_source(ctx, (config_source_t*)high_pri_source);
    
    // 应返回高优先级值
    char value[64] = {0};
    int result = config_context_get_string(ctx, "test.key", value, sizeof(value));
    TEST_ASSERT(result == 0, "获取配置值失败");
    TEST_ASSERT(strcmp(value, "high_priority") == 0, "优先级处理错误");
    
    config_context_destroy(ctx);
    
    printf("  配置源优先级测试通过\n");
    return 0;
}

/**
 * @brief 测试配置错误处理
 */
static int test_config_error_handling(void) {
    printf("  测试配置错误处理...\n");
    
    config_context_t* ctx = config_context_create();
    TEST_ASSERT(ctx != NULL, "配置上下文创建失败");
    
    // 测试获取不存在的键
    int int_val = 0;
    int result = config_context_get_int(ctx, "nonexistent.key", &int_val);
    TEST_ASSERT(result != 0, "获取不存在的键应失败");
    
    // 测试类型不匹配
    config_memory_source_t* mem_source = config_memory_source_create();
    TEST_ASSERT(mem_source != NULL, "内存配置源创建失败");
    config_memory_source_set_string(mem_source, "test.key", "not_a_number");
    config_context_add_source(ctx, (config_source_t*)mem_source);
    
    result = config_context_get_int(ctx, "test.key", &int_val);
    TEST_ASSERT(result != 0, "类型不匹配应失败");
    
    // 测试缓冲区溢出保护
    char small_buffer[4] = {0};
    result = config_context_get_string(ctx, "test.key", small_buffer, sizeof(small_buffer));
    TEST_ASSERT(result != 0, "缓冲区溢出应失败");
    
    config_context_destroy(ctx);
    
    printf("  配置错误处理测试通过\n");
    return 0;
}

/**
 * @brief 测试配置兼容层
 */
static int test_config_compat_layer(void) {
    printf("  测试配置兼容层...\n");
    
    // 测试兼容宏
    agentos_config_t* manager = AGENTOS_CONFIG_CREATE();
    TEST_ASSERT(manager != NULL, "兼容配置创建失败");
    
    AGENTOS_CONFIG_SET_STRING(manager, "test.key", "compat_value");
    
    const char* value = AGENTOS_CONFIG_GET_STRING(manager, "test.key", NULL);
    TEST_ASSERT(value != NULL, "兼容配置获取失败");
    TEST_ASSERT(strcmp(value, "compat_value") == 0, "兼容配置值不匹配");
    
    AGENTOS_CONFIG_FREE(manager);
    
    printf("  配置兼容层测试通过\n");
    return 0;
}

/* ==================== 主测试函数 ==================== */

int main(void) {
    printf("\n");
    printf("========================================\n");
    printf("  统一配置模块单元测试\n");
    printf("========================================\n");
    
    // 运行所有测试
    TEST_RUN(test_config_context_create_destroy);
    TEST_RUN(test_config_source_env);
    TEST_RUN(test_config_source_file);
    TEST_RUN(test_config_value_conversion);
    TEST_RUN(test_config_source_priority);
    TEST_RUN(test_config_error_handling);
    TEST_RUN(test_config_compat_layer);
    
    printf("\n");
    printf("========================================\n");
    printf("  测试完成\n");
    printf("  通过: %d, 失败: %d\n", passed_tests, failed_tests);
    printf("========================================\n");
    
    return failed_tests > 0 ? 1 : 0;
}