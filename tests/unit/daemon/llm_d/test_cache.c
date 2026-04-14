/**
 * @file test_cache.c
 * @brief LLM 缓存模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cache.h"

static void test_cache_create_destroy(void) {
    printf("  test_cache_create_destroy...\n");

    llm_cache_t* cache = llm_cache_create(100, 3600);
    assert(cache != NULL);

    llm_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_set_get(void) {
    printf("  test_cache_set_get...\n");

    llm_cache_t* cache = llm_cache_create(100, 3600);
    assert(cache != NULL);

    const char* key = "test_key_123";
    const char* value = "test_response_content";

    int ret = llm_cache_set(cache, key, value, strlen(value) + 1);
    assert(ret == 0);

    char* retrieved = llm_cache_get(cache, key);
    assert(retrieved != NULL);
    assert(strcmp(retrieved, value) == 0);

    free(retrieved);
    llm_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_delete(void) {
    printf("  test_cache_delete...\n");

    llm_cache_t* cache = llm_cache_create(100, 3600);
    assert(cache != NULL);

    const char* key = "test_key_delete";
    const char* value = "to_be_deleted";

    llm_cache_set(cache, key, value, strlen(value) + 1);

    int ret = llm_cache_delete(cache, key);
    assert(ret == 0);

    char* retrieved = llm_cache_get(cache, key);
    assert(retrieved == NULL);

    llm_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_clear(void) {
    printf("  test_cache_clear...\n");

    llm_cache_t* cache = llm_cache_create(100, 3600);
    assert(cache != NULL);

    llm_cache_set(cache, "key1", "value1", 7);
    llm_cache_set(cache, "key2", "value2", 7);
    llm_cache_set(cache, "key3", "value3", 7);

    int ret = llm_cache_clear(cache);
    assert(ret == 0);

    assert(llm_cache_get(cache, "key1") == NULL);
    assert(llm_cache_get(cache, "key2") == NULL);
    assert(llm_cache_get(cache, "key3") == NULL);

    llm_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_stats(void) {
    printf("  test_cache_stats...\n");

    llm_cache_t* cache = llm_cache_create(100, 3600);
    assert(cache != NULL);

    llm_cache_set(cache, "key1", "value1", 7);
    llm_cache_set(cache, "key2", "value2", 7);

    llm_cache_get(cache, "key1");
    llm_cache_get(cache, "key1");
    llm_cache_get(cache, "nonexistent");

    cache_stats_t stats;
    llm_cache_get_stats(cache, &stats);

    assert(stats.total_entries == 2);
    assert(stats.hits == 2);
    assert(stats.misses == 1);

    llm_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_ttl(void) {
    printf("  test_cache_ttl...\n");

    llm_cache_t* cache = llm_cache_create(100, 1);
    assert(cache != NULL);

    const char* key = "ttl_test_key";
    const char* value = "ttl_test_value";

    llm_cache_set(cache, key, value, strlen(value) + 1);

    char* retrieved = llm_cache_get(cache, key);
    assert(retrieved != NULL);
    free(retrieved);

#ifdef _WIN32
    Sleep(2000);
#else
    sleep(2);
#endif

    retrieved = llm_cache_get(cache, key);
    assert(retrieved == NULL);

    llm_cache_destroy(cache);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  LLM Cache Unit Tests\n");
    printf("=========================================\n");

    test_cache_create_destroy();
    test_cache_set_get();
    test_cache_delete();
    test_cache_clear();
    test_cache_stats();
    test_cache_ttl();

    printf("\n✅ All LLM cache tests PASSED\n");
    return 0;
}