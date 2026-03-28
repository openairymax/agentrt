/**
 * @file test_lodges_memory.c
 * @brief AgentOS 数据分区内存管理数据存储单元测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "lodges.h"
#include "lodges_memory.h"

static void test_memory_init_shutdown(void) {
    printf("Test: memory_init_shutdown...");

    lodges_error_t err = lodges_memory_init();
    assert(err == lodges_SUCCESS || err == lodges_ERR_ALREADY_INITIALIZED);

    lodges_memory_shutdown();
    printf("PASS\n");
}

static void test_memory_pool_crud(void) {
    printf("Test: memory_pool_crud...");

    lodges_memory_pool_t pool;
    memset(&pool, 0, sizeof(pool));

    snprintf(pool.pool_id, sizeof(pool.pool_id), "pool_%ld", (long)time(NULL));
    snprintf(pool.name, sizeof(pool.name), "Test Pool");
    pool.total_size = 1024 * 1024;
    pool.used_size = 512 * 1024;
    pool.block_size = 4096;
    pool.block_count = 256;
    pool.free_block_count = 128;
    pool.created_at = (uint64_t)time(NULL);
    snprintf(pool.status, sizeof(pool.status), "active");

    lodges_error_t err = lodges_memory_record_pool(&pool);
    if (err == lodges_SUCCESS) {
        lodges_memory_pool_t get_pool;
        memset(&get_pool, 0, sizeof(get_pool));

        err = lodges_memory_get_pool(pool.pool_id, &get_pool);
        assert(err == lodges_SUCCESS);
        assert(strcmp(get_pool.name, pool.name) == 0);
        assert(get_pool.total_size == pool.total_size);
        assert(get_pool.block_count == pool.block_count);

        err = lodges_memory_update_pool_usage(pool.pool_id, 768 * 1024, 64);
        assert(err == lodges_SUCCESS);
    }

    printf("PASS\n");
}

static void test_memory_allocation_crud(void) {
    printf("Test: memory_allocation_crud...");

    lodges_memory_pool_t pool;
    memset(&pool, 0, sizeof(pool));

    snprintf(pool.pool_id, sizeof(pool.pool_id), "pool_alloc_%ld", (long)time(NULL));
    snprintf(pool.name, sizeof(pool.name), "Allocation Test Pool");
    pool.total_size = 512 * 1024;
    pool.created_at = (uint64_t)time(NULL);
    snprintf(pool.status, sizeof(pool.status), "active");

    lodges_error_t err = lodges_memory_record_pool(&pool);

    lodges_memory_allocation_t allocation;
    memset(&allocation, 0, sizeof(allocation));

    snprintf(allocation.allocation_id, sizeof(allocation.allocation_id), "alloc_%ld", (long)time(NULL));
    snprintf(allocation.pool_id, sizeof(allocation.pool_id), "%s", pool.pool_id);
    allocation.size = 8192;
    allocation.address = 0x10000000;
    allocation.allocated_at = (uint64_t)time(NULL);
    snprintf(allocation.status, sizeof(allocation.status), "allocated");

    if (err == lodges_SUCCESS) {
        err = lodges_memory_record_allocation(&allocation);
        if (err == lodges_SUCCESS) {
            lodges_memory_allocation_t get_alloc;
            memset(&get_alloc, 0, sizeof(get_alloc));

            err = lodges_memory_get_allocation(allocation.allocation_id, &get_alloc);
            assert(err == lodges_SUCCESS);
            assert(strcmp(get_alloc.pool_id, allocation.pool_id) == 0);
            assert(get_alloc.size == allocation.size);
            assert(get_alloc.address == allocation.address);

            err = lodges_memory_free_allocation(allocation.allocation_id);
            assert(err == lodges_SUCCESS);
        }
    }

    printf("PASS\n");
}

static void test_memory_stats(void) {
    printf("Test: memory_stats...");

    uint32_t pool_count = 0;
    uint32_t total_allocations = 0;
    uint64_t total_size = 0;

    lodges_error_t err = lodges_memory_get_stats(&pool_count, &total_allocations, &total_size);
    assert(err == lodges_SUCCESS);

    printf("  Pools: %u, Allocations: %u, Total Size: %lu\n",
           pool_count, total_allocations, (unsigned long)total_size);

    printf("PASS\n");
}

static void test_memory_invalid_params(void) {
    printf("Test: memory_invalid_params...");

    lodges_error_t err = lodges_memory_record_pool(NULL);
    assert(err == lodges_ERR_INVALID_PARAM);

    lodges_memory_pool_t invalid_pool;
    memset(&invalid_pool, 0, sizeof(invalid_pool));
    err = lodges_memory_record_pool(&invalid_pool);
    assert(err == lodges_ERR_INVALID_PARAM);

    err = lodges_memory_record_allocation(NULL);
    assert(err == lodges_ERR_INVALID_PARAM);

    lodges_memory_allocation_t invalid_alloc;
    memset(&invalid_alloc, 0, sizeof(invalid_alloc));
    err = lodges_memory_record_allocation(&invalid_alloc);
    assert(err == lodges_ERR_INVALID_PARAM);

    err = lodges_memory_get_pool(NULL, NULL);
    assert(err == lodges_ERR_INVALID_PARAM);

    err = lodges_memory_get_allocation(NULL, NULL);
    assert(err == lodges_ERR_INVALID_PARAM);

    err = lodges_memory_update_pool_usage(NULL, 0, 0);
    assert(err == lodges_ERR_INVALID_PARAM);

    err = lodges_memory_free_allocation(NULL);
    assert(err == lodges_ERR_INVALID_PARAM);

    printf("PASS\n");
}

static void test_memory_not_found(void) {
    printf("Test: memory_not_found...");

    lodges_memory_pool_t pool;
    memset(&pool, 0, sizeof(pool));

    lodges_error_t err = lodges_memory_get_pool("nonexistent_id", &pool);
    assert(err == lodges_ERR_NOT_FOUND);

    err = lodges_memory_get_allocation("nonexistent_id", &pool);
    assert(err == lodges_ERR_NOT_FOUND);

    err = lodges_memory_update_pool_usage("nonexistent_id", 0, 0);
    assert(err == lodges_ERR_NOT_FOUND);

    err = lodges_memory_free_allocation("nonexistent_id");
    assert(err == lodges_ERR_NOT_FOUND);

    printf("PASS\n");
}

static void test_memory_update_usage(void) {
    printf("Test: memory_update_usage...");

    lodges_memory_pool_t pool;
    memset(&pool, 0, sizeof(pool));

    snprintf(pool.pool_id, sizeof(pool.pool_id), "pool_update_%ld", (long)time(NULL));
    snprintf(pool.name, sizeof(pool.name), "Update Test Pool");
    pool.total_size = 2048 * 1024;
    pool.used_size = 1024 * 1024;
    pool.block_count = 512;
    pool.free_block_count = 256;
    pool.created_at = (uint64_t)time(NULL);
    snprintf(pool.status, sizeof(pool.status), "active");

    lodges_error_t err = lodges_memory_record_pool(&pool);
    if (err == lodges_SUCCESS) {
        err = lodges_memory_update_pool_usage(pool.pool_id, 1536 * 1024, 128);
        assert(err == lodges_SUCCESS);

        lodges_memory_pool_t get_pool;
        memset(&get_pool, 0, sizeof(get_pool));

        err = lodges_memory_get_pool(pool.pool_id, &get_pool);
        assert(err == lodges_SUCCESS);
        assert(get_pool.used_size == 1536 * 1024);
        assert(get_pool.free_block_count == 128);
    }

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS lodges Memory Unit Tests ===\n\n");

    test_memory_init_shutdown();
    test_memory_pool_crud();
    test_memory_allocation_crud();
    test_memory_stats();
    test_memory_invalid_params();
    test_memory_not_found();
    test_memory_update_usage();

    printf("\n=== All Memory Tests Passed ===\n");
    return 0;
}

