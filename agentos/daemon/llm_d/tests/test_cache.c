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

    cache_t* cache = cache_create(100, 3600);
    assert(cache != NULL);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_put_get(void) {
    printf("  test_cache_put_get...\n");

    cache_t* cache = cache_create(100, 3600);
    assert(cache != NULL);

    const char* key = "test_key_123";
    const char* value = "test_response_content";

    cache_put(cache, key, value);

    char* retrieved = NULL;
    int ret = cache_get(cache, key, &retrieved);
    assert(ret == 0);
    assert(retrieved != NULL);
    assert(strcmp(retrieved, value) == 0);

    free(retrieved);
    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_miss(void) {
    printf("  test_cache_miss...\n");

    cache_t* cache = cache_create(100, 3600);
    assert(cache != NULL);

    char* retrieved = NULL;
    int ret = cache_get(cache, "nonexistent_key", &retrieved);
    assert(ret != 0 || retrieved == NULL);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_clear(void) {
    printf("  test_cache_clear...\n");

    cache_t* cache = cache_create(100, 3600);
    assert(cache != NULL);

    cache_put(cache, "key1", "value1");
    cache_put(cache, "key2", "value2");
    cache_put(cache, "key3", "value3");

    cache_clear(cache);

    char* retrieved = NULL;
    assert(cache_get(cache, "key1", &retrieved) != 0 || retrieved == NULL);
    assert(cache_get(cache, "key2", &retrieved) != 0 || retrieved == NULL);
    assert(cache_get(cache, "key3", &retrieved) != 0 || retrieved == NULL);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_size(void) {
    printf("  test_cache_size...\n");

    cache_t* cache = cache_create(100, 3600);
    assert(cache != NULL);
    assert(cache_capacity(cache) == 100);

    cache_put(cache, "key1", "value1");
    cache_put(cache, "key2", "value2");

    assert(cache_size(cache) == 2);

    cache_destroy(cache);

    printf("    PASSED\n");
}

static void test_cache_ttl(void) {
    printf("  test_cache_ttl...\n");

    cache_t* cache = cache_create(100, 1);
    assert(cache != NULL);

    const char* key = "ttl_test_key";
    const char* value = "ttl_test_value";

    cache_put(cache, key, value);

    char* retrieved = NULL;
    int ret = cache_get(cache, key, &retrieved);
    if (ret == 0 && retrieved != NULL) {
        free(retrieved);
    }

#ifdef _WIN32
    Sleep(2000);
#else
    sleep(2);
#endif

    retrieved = NULL;
    ret = cache_get(cache, key, &retrieved);
    assert(ret != 0 || retrieved == NULL);

    cache_destroy(cache);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  LLM Cache Unit Tests\n");
    printf("=========================================\n");

    test_cache_create_destroy();
    test_cache_put_get();
    test_cache_miss();
    test_cache_clear();
    test_cache_size();
    test_cache_ttl();

    printf("\nAll LLM cache tests PASSED\n");
    return 0;
}
