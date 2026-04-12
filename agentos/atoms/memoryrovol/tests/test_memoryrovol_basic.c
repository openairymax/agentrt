/**
 * @file test_memoryrovol_basic.c
 * @brief MemoryRovol 基础功能测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/memoryrovol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

/**
 * @brief 获取测试临时目录路径
 * @return 临时目录路径，调用者不应释放
 */
static const char* get_test_temp_dir(void) {
    static char temp_dir[256] = {0};
    if (temp_dir[0] == '\0') {
        const char* env_tmp = getenv("AGENTOS_TEST_TMPDIR");
        if (env_tmp && env_tmp[0] != '\0') {
            snprintf(temp_dir, sizeof(temp_dir), "%s", env_tmp);
        } else {
            // 默认使用 /tmp，在Windows上使用 C:\Windows\Temp
            #ifdef _WIN32
            const char* win_tmp = getenv("TEMP");
            if (win_tmp && win_tmp[0] != '\0') {
                snprintf(temp_dir, sizeof(temp_dir), "%s", win_tmp);
            } else {
                snprintf(temp_dir, sizeof(temp_dir), "C:\\Windows\\Temp");
            }
            #else
            snprintf(temp_dir, sizeof(temp_dir), "/tmp");
            #endif
        }
    }
    return temp_dir;
}

#define TEST_ASSERT(condition, msg) do { \
    if (condition) { \
        printf("  ✅ PASS: %s\n", msg); \
        tests_passed++; \
    } else { \
        printf("  ❌ FAIL: %s\n", msg); \
        tests_failed++; \
    } \
} while(0)

static void test_memoryrov_create_destroy(void) {
    printf("\n=== 测试记忆系统创建和销毁 ===\n");

    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/test_memory", get_test_temp_dir());
    agentos_memoryrov_config_t config = {0};
    config.raw_storage_path = base_path;
    config.index_path = NULL;
    config.relation_db_path = NULL;
    config.pattern_storage_path = NULL;
    config.embedding_model = NULL;
    config.llm_model = NULL;
    config.embedding_dim = 768;
    config.index_type = 0;
    config.forget_strategy = 0;
    config.forget_lambda = 0.9;
    config.forget_threshold = 0.1;

    agentos_memoryrov_handle_t handle = NULL;
    agentos_error_t err = agentos_memoryrov_init(&config, &handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS && handle != NULL, "记忆系统创建成功");

    if (handle) {
        err = agentos_memoryrov_destroy(handle);
        TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆系统销毁成功");
    }
}

static void test_memoryrov_null_params(void) {
    printf("\n=== 测试参数验证 ===\n");

    agentos_error_t err = agentos_memoryrov_create(NULL, NULL);
    TEST_ASSERT(err == AGENTOS_EINVAL, "NULL config 应返回错误");

    err = agentos_memoryrov_destroy(NULL);
    TEST_ASSERT(err == AGENTOS_EINVAL, "NULL handle 应返回错误");
}

static void test_memoryrov_add_retrieve_l1(void) {
    printf("\n=== 测试 L1 记忆添加和检索 ===\n");

    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/test_memory_l1", get_test_temp_dir());
    agentos_memoryrov_config_t config = {0};
    config.raw_storage_path = base_path;
    config.index_path = NULL;
    config.relation_db_path = NULL;
    config.pattern_storage_path = NULL;
    config.embedding_model = NULL;
    config.llm_model = NULL;
    config.embedding_dim = 768;
    config.index_type = 0;
    config.forget_strategy = 0;
    config.forget_lambda = 0.9;
    config.forget_threshold = 0.1;

    agentos_memoryrov_handle_t handle = NULL;
    agentos_error_t err = agentos_memoryrov_init(&config, &handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS && handle != NULL, "L1 记忆系统创建成功");

    if (!handle) {
        return;
    }

    const char* input = "测试记忆内容：这是一个简单的测试数据";
    char* output = NULL;
    size_t output_len = 0;

    err = agentos_memoryrov_add_memory(
        handle, input, strlen(input), NULL, 0, NULL);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆添加成功");

    err = agentos_memoryrov_retrieve(
        handle, input, strlen(input), &output, &output_len);
    TEST_ASSERT(err == AGENTOS_SUCCESS && output != NULL, "记忆检索成功");

    if (output) {
        AGENTOS_FREE(output);
    }

    err = agentos_memoryrov_destroy(handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "L1 记忆系统销毁成功");
}

static void test_memoryrov_forget(void) {
    printf("\n=== 测试遗忘机制 ===\n");

    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/test_memory_forget", get_test_temp_dir());
    agentos_memoryrov_config_t config = {
        .base_path = base_path,
        .max_size_mb = 50,
        .l1_enabled = true,
        .l2_enabled = false,
        .l3_enabled = false,
        .l4_enabled = false
    };

    agentos_memoryrov_handle_t handle = NULL;
    agentos_error_t err = agentos_memoryrov_create(&config, &handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS && handle != NULL, "记忆系统创建成功");

    if (!handle) {
        return;
    }

    const char* test_id = "mem_test_forget_001";
    const char* input = "测试遗忘的记忆内容";

    err = agentos_memoryrov_add_memory(
        handle, input, strlen(input), NULL, 0, test_id);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆添加成功");

    err = agentos_memoryrov_forget(
        handle, test_id, AGENTOS_FORGET_STRATEGY_EBINGHAUS);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆遗忘成功");

    char* output = NULL;
    size_t output_len = 0;
    err = agentos_memoryrov_retrieve(
        handle, test_id, strlen(test_id), &output, &output_len);
    TEST_ASSERT(err == AGENTOS_ENOTFOUND || output == NULL, "遗忘后的记忆应无法检索");

    if (output) {
        AGENTOS_FREE(output);
    }

    err = agentos_memoryrov_destroy(handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆系统销毁成功");
}

static void test_memoryrov_evolve(void) {
    printf("\n=== 测试记忆演化 ===\n");

    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/test_memory_evolve", get_test_temp_dir());
    agentos_memoryrov_config_t config = {
        .base_path = base_path,
        .max_size_mb = 50,
        .l1_enabled = true,
        .l2_enabled = false,
        .l3_enabled = false,
        .l4_enabled = false
    };

    agentos_memoryrov_handle_t handle = NULL;
    agentos_error_t err = agentos_memoryrov_create(&config, &handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS && handle != NULL, "记忆系统创建成功");

    if (!handle) {
        return;
    }

    const char* input = "测试演化的记忆内容";

    err = agentos_memoryrov_add_memory(
        handle, input, strlen(input), NULL, 0, NULL);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆添加成功");

    err = agentos_memoryrov_evolve(handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆演化成功");

    err = agentos_memoryrov_destroy(handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆系统销毁成功");
}

static void test_memoryrov_query_stats(void) {
    printf("\n=== 测试记忆统计查询 ===\n");

    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/test_memory_stats", get_test_temp_dir());
    agentos_memoryrov_config_t config = {
        .base_path = base_path,
        .max_size_mb = 50,
        .l1_enabled = true,
        .l2_enabled = false,
        .l3_enabled = false,
        .l4_enabled = false
    };

    agentos_memoryrov_handle_t handle = NULL;
    agentos_error_t err = agentos_memoryrov_create(&config, &handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS && handle != NULL, "记忆系统创建成功");

    if (!handle) {
        return;
    }

    agentos_memoryrov_stats_t stats = {0};
    err = agentos_memoryrov_query_stats(handle, &stats);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "统计信息查询成功");

    err = agentos_memoryrov_destroy(handle);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "记忆系统销毁成功");
}

int main(void) {
    printf("========================================\n");
    printf("  MemoryRovol 基础功能测试\n");
    printf("========================================\n");

    test_memoryrov_create_destroy();
    test_memoryrov_null_params();
    test_memoryrov_add_retrieve_l1();
    test_memoryrov_forget();
    test_memoryrov_evolve();
    test_memoryrov_query_stats();

    printf("\n========================================\n");
    printf("  测试结果汇总\n");
    printf("========================================\n");
    printf("  ✅ 通过：%d\n", tests_passed);
    printf("  ❌ 失败：%d\n", tests_failed);
    printf("  📊 总计：%d\n", tests_passed + tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
