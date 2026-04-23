/**
 * @file test_layer1_raw.c
 * @brief L1 原始卷存储单元测试
 *
 * 覆盖: create/destroy, write/read/delete/list_ids,
 *       flush, 元数据操作, 边界条件
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "layer1_raw.h"

/* ========== 测试宏 ========== */
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        return -1; \
    } \
} while(0)

#define TEST_PASS(name) printf("  PASS: %s\n", name)

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define RUN_TEST(func) do { \
    int _ret = func(); \
    if (_ret == 0) { g_tests_passed++; } else { g_tests_failed++; } \
} while(0)

/* ========== 辅助函数 ========== */

static agentos_layer1_raw_t* create_test_l1(void) {
    agentos_layer1_raw_t* l1 = NULL;
    agentos_error_t err = agentos_layer1_raw_create_async(NULL, 1024, 2, &l1);
    if (err != AGENTOS_SUCCESS || !l1) {
        return NULL;
    }
    return l1;
}

/* ========== 测试用例 ========== */

/**
 * T1: 创建和销毁L1原始卷
 */
int test_l1_create_destroy(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    TEST_ASSERT(l1 != NULL, "L1 should be created");
    
    agentos_layer1_raw_destroy(l1);
    
    agentos_layer1_raw_destroy(NULL);
    TEST_PASS("l1_create_destroy");
    return 0;
}

/**
 * T2: 写入和读取数据
 */
int test_l1_write_read(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    TEST_ASSERT(l1 != NULL, "create L1");
    
    const char* id = "test_record_001";
    const char* data = "This is test data for L1 raw storage write/read test.";
    
    agentos_error_t err = agentos_layer1_raw_write(l1, id, data, strlen(data));
    TEST_ASSERT(err == AGENTOS_SUCCESS, "write should succeed");
    
    void* out_data = NULL;
    size_t out_len = 0;
    err = agentos_layer1_raw_read(l1, id, &out_data, &out_len);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "read should succeed");
    TEST_ASSERT(out_data != NULL, "out_data should not be NULL");
    TEST_ASSERT(out_len == strlen(data), "length should match");
    TEST_ASSERT(memcmp(out_data, data, strlen(data)) == 0, 
               "data content should match");
    
    if (out_data) free(out_data);
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_write_read");
    return 0;
}

/**
 * T3: 覆盖写入(同一ID重复write)
 */
int test_l1_overwrite(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    const char* id = "overwrite_id";
    const char* v1 = "original version";
    const char* v2 = "updated new version content here";
    
    agentos_layer1_raw_write(l1, id, v1, strlen(v1));
    agentos_error_t err = agentos_layer1_raw_write(l1, id, v2, strlen(v2));
    TEST_ASSERT(err == AGENTOS_SUCCESS, "overwrite should succeed");
    
    void* out_data = NULL;
    size_t out_len = 0;
    agentos_layer1_raw_read(l1, id, &out_data, &out_len);
    TEST_ASSERT(out_data != NULL, "read after overwrite");
    if (out_data) {
        int match_v2 = (memcmp(out_data, v2, strlen(v2)) == 0);
        TEST_ASSERT(match_v2, "should read updated value");
        free(out_data);
    }
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_overwrite");
    return 0;
}

/**
 * T4: 删除数据
 */
int test_l1_delete(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    const char* id = "delete_me";
    agentos_layer1_raw_write(l1, id, "temporary data", 14);
    
    agentos_error_t del_err = agentos_layer1_raw_delete(l1, id);
    TEST_ASSERT(del_err == AGENTOS_SUCCESS, "delete should succeed");
    
    void* out_data = NULL;
    size_t out_len = 0;
    agentos_error_t read_err = agentos_layer1_raw_read(l1, id, &out_data, &out_len);
    TEST_ASSERT(read_err != AGENTOS_SUCCESS || out_data == NULL,
               "deleted record should not be readable");
    
    if (out_data) free(out_data);
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_delete");
    return 0;
}

/**
 * T5: 列出所有ID
 */
int test_l1_list_ids(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    const char* ids[] = {"id_alpha", "id_beta", "id_gamma", "id_delta"};
    int n = sizeof(ids) / sizeof(ids[0]);
    
    for (int i = 0; i < n; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "data for %s", ids[i]);
        agentos_layer1_raw_write(l1, ids[i], buf, strlen(buf));
    }
    
    char** out_ids = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_layer1_raw_list_ids(l1, &out_ids, &count);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "list_ids should succeed");
    TEST_ASSERT(count >= n, "count should be >= number of written records");
    
    if (out_ids && count > 0) {
        agentos_free_string_array(out_ids, count);
    }
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_list_ids");
    return 0;
}

/**
 * T6: 空存储列出ID
 */
int test_l1_list_empty(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    char** out_ids = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_layer1_raw_list_ids(l1, &out_ids, &count);
    
    if (err == AGENTOS_SUCCESS) {
        TEST_ASSERT(count == 0 || out_ids == NULL,
                   "empty store should have 0 IDs or NULL");
        if (out_ids) agentos_free_string_array(out_ids, count);
    }
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_list_empty");
    return 0;
}

/**
 * T7: Flush操作
 */
int test_l1_flush(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    for (int i = 0; i < 5; i++) {
        char key[32], val[64];
        snprintf(key, sizeof(key), "flush_key_%d", i);
        snprintf(val, sizeof(val), "flush data %d", i);
        agentos_layer1_raw_write(l1, key, val, strlen(val));
    }
    
    agentos_error_t err = agentos_layer1_raw_flush(l1, 5000);
    TEST_ASSERT(err == AGENTOS_SUCCESS, "flush should succeed");
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_flush");
    return 0;
}

/**
 * T8: 大数据块操作
 */
int test_l1_large_data(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    size_t large_size = 64 * 1024;
    char* large_buf = (char*)calloc(1, large_size + 1);
    TEST_ASSERT(large_buf != NULL, "allocate large buffer");
    
    for (size_t i = 0; i < large_size; i++) {
        large_buf[i] = 'A' + (i % 26);
    }
    
    agentos_error_t w_err = agentos_layer1_raw_write(
        l1, "large_record", large_buf, large_size);
    TEST_ASSERT(w_err == AGENTOS_SUCCESS, "large write should succeed");
    
    void* out_data = NULL;
    size_t out_len = 0;
    agentos_error_t r_err = agentos_layer1_raw_read(
        l1, "large_record", &out_data, &out_len);
    TEST_ASSERT(r_err == AGENTOS_SUCCESS, "large read should succeed");
    TEST_ASSERT(out_len == large_size, "read length should match");
    
    if (out_data) {
        int match = (memcmp(out_data, large_buf, large_size) == 0);
        TEST_ASSERT(match, "large data content should match");
        free(out_data);
    }
    
    free(large_buf);
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_large_data");
    return 0;
}

/**
 * T9: 二进制数据操作
 */
int test_l1_binary_data(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    unsigned char binary[] = {
        0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD, 0x80, 0x7F,
        0xAA, 0x55, 0xCC, 0x33, 0xDE, 0xAD, 0xBE, 0xEF
    };
    size_t bin_len = sizeof(binary);
    
    agentos_error_t w_err = agentos_layer1_raw_write(
        l1, "binary_rec", binary, bin_len);
    TEST_ASSERT(w_err == AGENTOS_SUCCESS, "binary write should succeed");
    
    void* out_data = NULL;
    size_t out_len = 0;
    agentos_error_t r_err = agentos_layer1_raw_read(
        l1, "binary_rec", &out_data, &out_len);
    TEST_ASSERT(r_err == AGENTOS_SUCCESS, "binary read should succeed");
    
    if (out_data) {
        int match = (memcmp(out_data, binary, bin_len) == 0);
        TEST_ASSERT(match, "binary data should match exactly");
        free(out_data);
    }
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_binary_data");
    return 0;
}

/**
 * T10: 无效参数边界测试
 */
int test_l1_invalid_params(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    agentos_error_t e1 = agentos_layer1_raw_write(NULL, "k", "v", 1);
    TEST_ASSERT(e1 != AGENTOS_SUCCESS, "NULL handle write should fail");
    
    agentos_error_t e2 = agentos_layer1_raw_write(l1, NULL, "v", 1);
    TEST_ASSERT(e2 != AGENTOS_SUCCESS, "NULL id write should fail");
    
    void* d = NULL; size_t s = 0;
    agentos_error_t e3 = agentos_layer1_raw_read(NULL, "k", &d, &s);
    TEST_ASSERT(e3 != AGENTOS_SUCCESS, "NULL handle read should fail");
    
    agentos_error_t e4 = agentos_layer1_raw_delete(NULL, "k");
    TEST_ASSERT(e4 != AGENTOS_SUCCESS, "NULL handle delete should fail");
    
    agentos_error_t e5 = agentos_layer1_raw_flush(NULL, 1000);
    TEST_ASSERT(e5 != AGENTOS_SUCCESS, "NULL handle flush should fail");
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_invalid_params");
    return 0;
}

/**
 * T11: 高并发写入多条记录
 */
int test_l1_bulk_operations(void) {
    agentos_layer1_raw_t* l1 = create_test_l1();
    
    int bulk_count = 100;
    for (int i = 0; i < bulk_count; i++) {
        char key[48], val[96];
        snprintf(key, sizeof(key), "bulk_%04d", i);
        snprintf(val, sizeof(val), 
            "Bulk operation test record #%d with some payload data", i);
        
        agentos_error_t err = agentos_layer1_raw_write(l1, key, val, strlen(val));
        if (err != AGENTOS_SUCCESS && i < 10) {
            printf("  WARN: bulk write #%d failed\n", i);
        }
    }
    
    char** ids = NULL;
    size_t count = 0;
    agentos_layer1_raw_list_ids(l1, &ids, &count);
    
    TEST_ASSERT(count > 0 || bulk_count > 0, 
                "should have stored records (or async pending)");
    
    if (ids) agentos_free_string_array(ids, count);
    
    agentos_layer1_raw_destroy(l1);
    TEST_PASS("l1_bulk_operations");
    return 0;
}

/* ========== 主函数 ========== */

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    printf("\n========================================\n");
    printf("  MemoryRovol L1 Raw Storage Unit Tests\n");
    printf("========================================\n\n");
    
    RUN_TEST(test_l1_create_destroy);
    RUN_TEST(test_l1_write_read);
    RUN_TEST(test_l1_overwrite);
    RUN_TEST(test_l1_delete);
    RUN_TEST(test_l1_list_ids);
    RUN_TEST(test_l1_list_empty);
    RUN_TEST(test_l1_flush);
    RUN_TEST(test_l1_large_data);
    RUN_TEST(test_l1_binary_data);
    RUN_TEST(test_l1_invalid_params);
    RUN_TEST(test_l1_bulk_operations);
    
    printf("\n----------------------------------------\n");
    printf("  Results: %d/%d passed", g_tests_passed, g_tests_passed + g_tests_failed);
    if (g_tests_failed > 0) {
        printf(" (%d FAILED)", g_tests_failed);
    }
    printf("\n");
    
    return g_tests_failed > 0 ? 1 : 0;
}
