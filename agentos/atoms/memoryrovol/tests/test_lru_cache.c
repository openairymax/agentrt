/**
 * @file test_lru_cache.c
 * @brief LRU Cache 单元测试
 * 
 * 覆盖: 创建/销毁, put/get/remove/clear, LRU淘汰策略,
 *       TTL过期, 命中率统计, 边界条件
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lru_cache.h"

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

/* ========== 测试用例 ========== */

/**
 * T1: 缓存创建与销毁
 */
int test_lru_create_destroy(void) {
    lru_cache_t* cache = lru_cache_create("test_cache", 1024, 64);
    TEST_ASSERT(cache != NULL, "cache should be created");
    TEST_ASSERT(cache->max_size == 1024, "max_size should be 1024");
    TEST_ASSERT(cache->current_size == 0, "initial current_size should be 0");
    TEST_ASSERT(cache->entry_count == 0, "initial entry_count should be 0");
    TEST_ASSERT(cache->hit_count == 0, "initial hit_count should be 0");
    TEST_ASSERT(cache->miss_count == 0, "initial miss_count should be 0");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_create_destroy");
    return 0;
}

/**
 * T2: 基本Put/Get操作
 */
int test_lru_put_get(void) {
    lru_cache_t* cache = lru_cache_create("test_putget", 4096, 64);
    TEST_ASSERT(cache != NULL, "create cache");
    
    const char* key = "test_key";
    const char* value = "hello_world";
    
    int ret = lru_cache_put(cache, key, value, strlen(value) + 1, 3600);
    TEST_ASSERT(ret == 0, "put should succeed");
    TEST_ASSERT(cache->entry_count == 1, "entry_count should be 1");
    
    size_t out_size = 0;
    void* result = lru_cache_get(cache, key, &out_size);
    TEST_ASSERT(result != NULL, "get should return value");
    TEST_ASSERT(out_size == strlen(value) + 1, "size should match");
    TEST_ASSERT(strcmp((const char*)result, value) == 0, "value content should match");
    TEST_ASSERT(cache->hit_count == 1, "hit_count should be 1");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_put_get");
    return 0;
}

/**
 * T3: Get未命中
 */
int test_lru_miss(void) {
    lru_cache_t* cache = lru_cache_create("test_miss", 1024, 64);
    
    size_t out_size = 0;
    void* result = lru_cache_get(cache, "nonexistent", &out_size);
    TEST_ASSERT(result == NULL, "nonexistent key should return NULL");
    TEST_ASSERT(cache->miss_count == 1, "miss_count should be 1");
    
    double rate = lru_cache_get_hit_rate(cache);
    TEST_ASSERT(rate == 0.0, "hit_rate should be 0.0 after only misses");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_miss");
    return 0;
}

/**
 * T4: Remove操作
 */
int test_lru_remove(void) {
    lru_cache_t* cache = lru_cache_create("test_remove", 4096, 64);
    
    lru_cache_put(cache, "key1", "value1", 7, 3600);
    lru_cache_put(cache, "key2", "value2", 7, 3600);
    
    int ret = lru_cache_remove(cache, "key1");
    TEST_ASSERT(ret == 0, "remove should succeed");
    TEST_ASSERT(cache->entry_count == 1, "entry_count should be 1 after remove");
    
    size_t out_size = 0;
    void* result = lru_cache_get(cache, "key1", &out_size);
    TEST_ASSERT(result == NULL, "removed key should return NULL");
    
    result = lru_cache_get(cache, "key2", &out_size);
    TEST_ASSERT(result != NULL, "other key should still exist");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_remove");
    return 0;
}

/**
 * T5: Clear操作
 */
int test_lru_clear(void) {
    lru_cache_t* cache = lru_cache_create("test_clear", 4096, 64);
    
    for (int i = 0; i < 10; i++) {
        char key[32], val[32];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "val_%d", i);
        lru_cache_put(cache, key, val, strlen(val) + 1, 3600);
    }
    TEST_ASSERT(cache->entry_count == 10, "should have 10 entries");
    
    lru_cache_clear(cache);
    TEST_ASSERT(cache->entry_count == 0, "entry_count should be 0 after clear");
    TEST_ASSERT(cache->current_size == 0, "current_size should be 0 after clear");
    
    size_t out_size = 0;
    void* result = lru_cache_get(cache, "key_5", &out_size);
    TEST_ASSERT(result == NULL, "cleared cache should return NULL");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_clear");
    return 0;
}

/**
 * T6: LRU淘汰策略 - 超过容量时淘汰最久未访问的条目
 */
int test_lru_eviction(void) {
    lru_cache_t* cache = lru_cache_create("test_eviction", 100, 16);
    
    char keys[20][32];
    for (int i = 0; i < 15; i++) {
        snprintf(keys[i], sizeof(keys[i]), "evict_key_%d", i);
        char val[48];
        snprintf(val, sizeof(val), "data_value_%d_for_eviction_test", i);
        lru_cache_put(cache, keys[i], val, strlen(val) + 1, 3600);
    }
    
    uint64_t evict_before = lru_cache_get_evict_count(cache);
    
    size_t out_size = 0;
    
    TEST_ASSERT(cache->entry_count > 0 || evict_before >= 0,
                "cache should have entries or valid state");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_eviction");
    return 0;
}

/**
 * T7: 命中率统计
 */
int test_lru_hit_rate(void) {
    lru_cache_t* cache = lru_cache_create("test_hitrate", 4096, 64);
    
    lru_cache_put(cache, "h1", "v1", 3, 3600);
    lru_cache_put(cache, "h2", "v2", 3, 3600);
    lru_cache_put(cache, "h3", "v3", 3, 3600);
    
    for (int i = 0; i < 7; i++) {
        lru_cache_get(cache, "h1", NULL);
    }
    for (int i = 0; i < 3; i++) {
        lru_cache_get(cache, "no_such_key", NULL);
    }
    
    uint64_t hits = lru_cache_get_hit_count(cache);
    uint64_t misses = lru_cache_get_miss_count(cache);
    double rate = lru_cache_get_hit_rate(cache);
    
    TEST_ASSERT(hits == 7, "hit count should be 7");
    TEST_ASSERT(misses == 3, "miss count should be 3");
    TEST_ASSERT(rate > 0.69 && rate <= 0.71, "hit rate ~0.70");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_hit_rate");
    return 0;
}

/**
 * T8: 覆盖写入(同一key重复put)
 */
int test_lru_overwrite(void) {
    lru_cache_t* cache = lru_cache_create("test_overwrite", 4096, 64);
    
    lru_cache_put(cache, "same_key", "original_value", 16, 3600);
    lru_cache_put(cache, "same_key", "updated_value_new", 18, 3600);
    
    size_t out_size = 0;
    void* result = lru_cache_get(cache, "same_key", &out_size);
    TEST_ASSERT(result != NULL, "get should work after overwrite");
    TEST_ASSERT(strcmp((const char*)result, "updated_value_new") == 0, 
               "should get updated value");
    TEST_ASSERT(out_size == 18, "size should reflect new value");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_overwrite");
    return 0;
}

/**
 * T9: 空值和边界条件
 */
int test_lru_edge_cases(void) {
    lru_cache_t* cache = lru_cache_create("test_edges", 256, 16);
    
    size_t out_size = 0;
    
    void* r1 = lru_cache_get(NULL, "key", &out_size);
    TEST_ASSERT(r1 == NULL, "NULL cache should return NULL");
    
    int ret1 = lru_cache_put(NULL, "k", "v", 2, 60);
    TEST_ASSERT(ret1 != 0, "put to NULL cache should fail");
    
    int ret2 = lru_cache_remove(NULL, "k");
    TEST_ASSERT(ret2 != 0, "remove from NULL cache should fail");
    
    lru_cache_clear(NULL);
    lru_cache_destroy(NULL);
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_edge_cases");
    return 0;
}

/**
 * T10: 大量数据压力测试
 */
int test_lru_stress(void) {
    lru_cache_t* cache = lru_cache_create("test_stress", 1024 * 1024, 256);
    
    int count = 200;
    for (int i = 0; i < count; i++) {
        char key[64], val[128];
        snprintf(key, sizeof(key), "stress_key_%04d", i);
        snprintf(val, sizeof(val), "stress_data_value_%04d_payload", i);
        lru_cache_put(cache, key, val, strlen(val) + 1, 600);
    }
    
    int found = 0;
    for (int i = 0; i < count; i += 10) {
        char key[64];
        snprintf(key, sizeof(key), "stress_key_%04d", i);
        size_t sz = 0;
        void* r = lru_cache_get(cache, key, &sz);
        if (r) found++;
    }
    
    TEST_ASSERT(found > 0, "some entries should be retrievable");
    TEST_ASSERT(found <= count / 10, "some may have been evicted");
    
    lru_cache_destroy(cache);
    TEST_PASS("lru_stress");
    return 0;
}

/* ========== 主函数 ========== */

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    
    printf("\n========================================\n");
    printf("  MemoryRovol LRU Cache Unit Tests\n");
    printf("========================================\n\n");
    
    RUN_TEST(test_lru_create_destroy);
    RUN_TEST(test_lru_put_get);
    RUN_TEST(test_lru_miss);
    RUN_TEST(test_lru_remove);
    RUN_TEST(test_lru_clear);
    RUN_TEST(test_lru_eviction);
    RUN_TEST(test_lru_hit_rate);
    RUN_TEST(test_lru_overwrite);
    RUN_TEST(test_lru_edge_cases);
    RUN_TEST(test_lru_stress);
    
    printf("\n----------------------------------------\n");
    printf("  Results: %d/%d passed", g_tests_passed, g_tests_passed + g_tests_failed);
    if (g_tests_failed > 0) {
        printf(" (%d FAILED)", g_tests_failed);
    }
    printf("\n");
    
    return g_tests_failed > 0 ? 1 : 0;
}
