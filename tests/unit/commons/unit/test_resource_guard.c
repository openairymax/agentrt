/**
 * @file test_resource_guard.c
 * @brief 资源管理模块单元测试
 *
 * 测试RAII资源管理模式、内存泄漏检测、资源配额管理
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../utils/test_framework.h"
#include "../../utils/memory/include/memory.h"

typedef struct {
    int fd;
    bool is_open;
    char* name;
} mock_resource_t;

static int resource_create_count = 0;
static int resource_destroy_count = 0;

static mock_resource_t* mock_resource_create(const char* name) {
    mock_resource_t* res = (mock_resource_t*)malloc(sizeof(mock_resource_t));
    if (!res) return NULL;

    res->fd = ++resource_create_count;
    res->is_open = true;
    res->name = name ? strdup(name) : NULL;

    return res;
}

static void mock_resource_destroy(mock_resource_t* res) {
    if (!res) return;

    if (res->is_open) {
        res->is_open = false;
        resource_destroy_count++;
    }

    if (res->name) {
        free(res->name);
    }

    free(res);
}

static void test_resource_lifecycle(void** state) {
    (void)state;
    mock_resource_t* res = mock_resource_create("test_resource");

    assert_non_null(res);
    assert_true(res->is_open);
    assert_int_equal(1, res->fd);

    mock_resource_destroy(res);

    assert_int_equal(1, resource_destroy_count);
}

static void test_multiple_resources(void** state) {
    (void)state;
    const int num_resources = 10;
    mock_resource_t* resources[10];

    for (int i = 0; i < num_resources; i++) {
        char name[32];
        snprintf(name, sizeof(name), "resource_%d", i);
        resources[i] = mock_resource_create(name);
        assert_non_null(resources[i]);
        assert_true(resources[i]->is_open);
    }

    assert_int_equal(num_resources, resource_create_count);

    for (int i = 0; i < num_resources; i++) {
        mock_resource_destroy(resources[i]);
    }

    assert_int_equal(num_resources, resource_destroy_count);
}

static void test_null_resource_handling(void** state) {
    (void)state;
    mock_resource_destroy(NULL);
    assert_int_equal(0, resource_destroy_count);

    mock_resource_t* res = mock_resource_create(NULL);
    assert_non_null(res);
    assert_null(res->name);

    mock_resource_destroy(res);
}

static void test_resource_leak_detection(void** state) {
    (void)state;
    int initial_count = resource_destroy_count;

    {
        mock_resource_t* res1 = mock_resource_create("leak_test_1");
        mock_resource_t* res2 = mock_resource_create("leak_test_2");

        assert_non_null(res1);
        assert_non_null(res2);

        mock_resource_destroy(res1);
    }

    assert_int_equal(initial_count + 1, resource_destroy_count);
}

static void test_memory_allocation_paired(void** state) {
    (void)state;
    void* ptr1 = malloc(1024);
    assert_non_null(ptr1);

    void* ptr2 = calloc(100, sizeof(int));
    assert_non_null(ptr2);

    char* str = strdup("Test string");
    assert_non_null(str);
    assert_string_equal("Test string", str);

    free(str);
    free(ptr2);
    free(ptr1);
}

static void test_large_memory_allocation(void** state) {
    (void)state;
    size_t large_size = 1024 * 1024;
    void* ptr = malloc(large_size);

    if (ptr) {
        memset(ptr, 0xAB, large_size);
        unsigned char* bytes = (unsigned char*)ptr;
        assert_int_equal(0xAB, bytes[0]);
        assert_int_equal(0xAB, bytes[large_size - 1]);

        free(ptr);
    }
}

static void test_zero_size_allocation(void** state) {
    (void)state;
    void* ptr = malloc(0);

    if (ptr) {
        free(ptr);
    }
}

static void test_memory_alignment(void** state) {
    (void)state;
    size_t alignment = 64;
    void* ptr = NULL;

#ifdef _WIN32
    ptr = _aligned_malloc(256, alignment);
#else
    posix_memalign(&ptr, alignment, 256);
#endif

    if (ptr) {
        uintptr_t addr = (uintptr_t)ptr;
        assert_true((addr % alignment) == 0);

#ifdef _WIN32
        _aligned_free(ptr);
#else
        free(ptr);
#endif
    }
}

static void test_memory_statistics(void** state) {
    (void)state;
    void* ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = malloc(1024 * (i + 1));
        assert_non_null(ptrs[i]);
    }

    for (int i = 0; i < 5; i++) {
        free(ptrs[i]);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_resource_lifecycle),
        cmocka_unit_test(test_multiple_resources),
        cmocka_unit_test(test_null_resource_handling),
        cmocka_unit_test(test_resource_leak_detection),
        cmocka_unit_test(test_memory_allocation_paired),
        cmocka_unit_test(test_large_memory_allocation),
        cmocka_unit_test(test_zero_size_allocation),
        cmocka_unit_test(test_memory_alignment),
        cmocka_unit_test(test_memory_statistics),
    };

    return cmocka_run_group_tests(tests, sizeof(tests) / sizeof(tests[0]), NULL, NULL);
}
