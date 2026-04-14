/**
 * @file test_cache.c
 * @brief Tool 缓存模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "cache.h"

static void test_cache_create_destroy(void) {
    printf("  test_cache_create_destroy...\n");

    tool_cache_t* cache = tool_cache_create(100, 3600);
    assert(cache != NULL);

    tool_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_key_generation(void) {
    printf("  test_cache_key_generation...\n");

    const char* tool_id = "test_tool";
    const char* params = "{\"arg\": \"value\"}";

    char* key = tool_cache_key(tool_id, params);
    assert(key != NULL);
    assert(strstr(key, tool_id) != NULL);
    assert(strstr(key, params) != NULL);

    free(key);

    printf("    PASSED\n");
}

static void test_cache_key_null_inputs(void) {
    printf("  test_cache_key_null_inputs...\n");

    char* key = tool_cache_key(NULL, "params");
    assert(key == NULL);

    key = tool_cache_key("tool_id", NULL);
    assert(key == NULL);

    key = tool_cache_key(NULL, NULL);
    assert(key == NULL);

    printf("    PASSED\n");
}

static void test_cache_set_get(void) {
    printf("  test_cache_set_get...\n");

    tool_cache_t* cache = tool_cache_create(100, 3600);
    assert(cache != NULL);

    const char* key = "test_key_123";
    const char* value = "cached_result_data";

    int ret = tool_cache_set(cache, key, value, strlen(value) + 1);
    assert(ret == 0);

    char* retrieved = tool_cache_get(cache, key);
    assert(retrieved != NULL);
    assert(strcmp(retrieved, value) == 0);

    free(retrieved);
    tool_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_miss(void) {
    printf("  test_cache_miss...\n");

    tool_cache_t* cache = tool_cache_create(100, 3600);
    assert(cache != NULL);

    char* retrieved = tool_cache_get(cache, "nonexistent_key");
    assert(retrieved == NULL);

    tool_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_delete(void) {
    printf("  test_cache_delete...\n");

    tool_cache_t* cache = tool_cache_create(100, 3600);
    assert(cache != NULL);

    const char* key = "delete_test_key";
    const char* value = "to_be_deleted";

    tool_cache_set(cache, key, value, strlen(value) + 1);

    int ret = tool_cache_delete(cache, key);
    assert(ret == 0);

    char* retrieved = tool_cache_get(cache, key);
    assert(retrieved == NULL);

    tool_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_clear(void) {
    printf("  test_cache_clear...\n");

    tool_cache_t* cache = tool_cache_create(100, 3600);
    assert(cache != NULL);

    tool_cache_set(cache, "key1", "value1", 7);
    tool_cache_set(cache, "key2", "value2", 7);
    tool_cache_set(cache, "key3", "value3", 7);

    int ret = tool_cache_clear(cache);
    assert(ret == 0);

    assert(tool_cache_get(cache, "key1") == NULL);
    assert(tool_cache_get(cache, "key2") == NULL);
    assert(tool_cache_get(cache, "key3") == NULL);

    tool_cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_stats(void) {
    printf("  test_cache_stats...\n");

    tool_cache_t* cache = tool_cache_create(100, 3600);
    assert(cache != NULL);

    tool_cache_set(cache, "key1", "value1", 7);
    tool_cache_set(cache, "key2", "value2", 7);

    tool_cache_get(cache, "key1");
    tool_cache_get(cache, "key1");
    tool_cache_get(cache, "nonexistent");

    cache_stats_t stats;
    tool_cache_get_stats(cache, &stats);

    assert(stats.total_entries == 2);
    assert(stats.hits == 2);
    assert(stats.misses == 1);

    tool_cache_destroy(cache);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tool Cache Unit Tests\n");
    printf("=========================================\n");

    test_cache_create_destroy();
    test_cache_key_generation();
    test_cache_key_null_inputs();
    test_cache_set_get();
    test_cache_miss();
    test_cache_delete();
    test_cache_clear();
    test_cache_stats();

    printf("\n✅ All tool cache tests PASSED\n");
    return 0;
}