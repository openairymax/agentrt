﻿﻿﻿﻿﻿﻿﻿﻿﻿/**
 * @file test_advanced_storage.c
 * @brief 高级存储单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>
#include <assert.h>

/* 包含必要的头文件 */
#include "layer1_raw.h"
#include "agentos.h"

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

/**
 * @brief 测试高级存储基本功能
 * @return 0表示成功，非0表示失败
 */
int test_advanced_storage_basic(void) {
    printf("  测试高级存储基本功能...\n");

    /* 创建一个简单的存储引擎 */
    agentos_layer1_raw_t* engine = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/test_storage", get_test_temp_dir());
    agentos_error_t err = agentos_layer1_raw_create(base_path, 1024 * 1024, &engine);

    if (err != AGENTOS_SUCCESS) {
        printf("    创建存储引擎失败: %d\n", err);
        return 1;
    }

    /* 测试数据 */
    const char* test_id = "test_key_123";
    const char* test_data = "Hello, AgentOS!";
    size_t data_len = strlen(test_data) + 1;

    /* 写入数据 */
    err = agentos_layer1_raw_write(engine, test_id, test_data, data_len);
    if (err != AGENTOS_SUCCESS) {
        printf("    写入数据失败: %d\n", err);
        agentos_layer1_raw_destroy(engine);
        return 1;
    }

    /* 读取数据 */
    size_t read_len;
    void* read_data = agentos_layer1_raw_read(engine, test_id, &read_len);
    if (read_data == NULL) {
        printf("    读取数据失败\n");
        agentos_layer1_raw_destroy(engine);
        return 1;
    }

    /* 验证数据 */
    if (read_len != data_len || memcmp(read_data, test_data, data_len) != 0) {
        printf("    读取的数据不匹配\n");
        AGENTOS_FREE(read_data);
        agentos_layer1_raw_destroy(engine);
        return 1;
    }

    AGENTOS_FREE(read_data);
    agentos_layer1_raw_destroy(engine);

    printf("    高级存储基本功能测试通过\n");
    return 0;
}

/**
 * @brief 测试边缘情况
 * @return 0表示成功，非0表示失败
 */
int test_advanced_storage_edge_cases(void) {
    printf("  测试高级存储边缘情况...\n");

    int failures = 0;

    /* 测试1：空ID */
    {
        agentos_layer1_raw_t* engine = NULL;
        agentos_error_t err = agentos_layer1_raw_create("/tmp/test_storage2", 1024 * 1024, &engine);
        if (err == AGENTOS_SUCCESS) {
            err = agentos_layer1_raw_write(engine, NULL, "data", 5);
            if (err != AGENTOS_EINVAL) {
                printf("    空ID应该返回EINVAL，实际返�? %d\n", err);
                failures++;
            }
            agentos_layer1_raw_destroy(engine);
        }
    }

    /* 测试2：空数据 */
    {
        agentos_layer1_raw_t* engine = NULL;
        agentos_error_t err = agentos_layer1_raw_create("/tmp/test_storage3", 1024 * 1024, &engine);
        if (err == AGENTOS_SUCCESS) {
            err = agentos_layer1_raw_write(engine, "test_id", NULL, 0);
            if (err != AGENTOS_EINVAL) {
                printf("    空数据应该返回EINVAL，实际返�? %d\n", err);
                failures++;
            }
            agentos_layer1_raw_destroy(engine);
        }
    }

    /* 测试3：非常大的数据（边界测试�?*/
    {
        agentos_layer1_raw_t* engine = NULL;
        agentos_error_t err = agentos_layer1_raw_create("/tmp/test_storage4", 1024 * 1024, &engine);
        if (err == AGENTOS_SUCCESS) {
            char* large_data = AGENTOS_MALLOC(1024 * 1024);
            if (large_data) {
                memset(large_data, 0xAA, 1024 * 1024);
                err = agentos_layer1_raw_write(engine, "large_data", large_data, 1024 * 1024);
                /* 可能成功也可能失败，取决于实现，但不应崩�?*/
                printf("    大数据写入测试完成（返回�? %d）\n", err);
                AGENTOS_FREE(large_data);
            }
            agentos_layer1_raw_destroy(engine);
        }
    }

    if (failures == 0) {
        printf("    所有边缘情况测试通过\n");
    }

    return failures;
}
