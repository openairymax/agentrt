/* SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_sanitizer_cache.c - Sanitizer Cache with LRU Unit Tests
 */

#include "sanitizer_cache.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_cache_create_destroy(void) {
    printf("Test: cache_create_destroy... ");
    
    sanitizer_cache_t* cache = sanitizer_cache_create(10);
    assert(cache != NULL);
    
    sanitizer_cache_destroy(cache);
    printf("PASS\n");
}

static void test_cache_put_get(void) {
    printf("Test: cache_put_get... ");
    
    sanitizer_cache_t* cache = sanitizer_cache_create(10);
    assert(cache != NULL);
    
    sanitizer_cache_put(cache, "input1", "output1", SANITIZE_LEVEL_NORMAL);
    
    char* result = sanitizer_cache_get(cache, "input1", SANITIZE_LEVEL_NORMAL);
    assert(result != NULL);
    assert(strcmp(result, "output1") == 0);
    free(result);
    
    sanitizer_cache_destroy(cache);
    printf("PASS\n");
}

static void test_cache_lru_eviction(void) {
    printf("Test: cache_lru_eviction... ");
    
    sanitizer_cache_t* cache = sanitizer_cache_create(3);
    assert(cache != NULL);
    
    sanitizer_cache_put(cache, "key1", "val1", SANITIZE_LEVEL_NORMAL);
    sanitizer_cache_put(cache, "key2", "val2", SANITIZE_LEVEL_NORMAL);
    sanitizer_cache_put(cache, "key3", "val3", SANITIZE_LEVEL_NORMAL);
    
    char* r1 = sanitizer_cache_get(cache, "key1", SANITIZE_LEVEL_NORMAL);
    assert(r1 != NULL);
    assert(strcmp(r1, "val1") == 0);
    free(r1);
    
    sanitizer_cache_put(cache, "key4", "val4", SANITIZE_LEVEL_NORMAL);
    
    char* r2 = sanitizer_cache_get(cache, "key2", SANITIZE_LEVEL_NORMAL);
    assert(r2 == NULL);
    
    char* r3 = sanitizer_cache_get(cache, "key1", SANITIZE_LEVEL_NORMAL);
    assert(r3 != NULL);
    assert(strcmp(r3, "val1") == 0);
    free(r3);
    
    char* r4 = sanitizer_cache_get(cache, "key4", SANITIZE_LEVEL_NORMAL);
    assert(r4 != NULL);
    assert(strcmp(r4, "val4") == 0);
    free(r4);
    
    sanitizer_cache_destroy(cache);
    printf("PASS\n");
}

static void test_cache_update_existing(void) {
    printf("Test: cache_update_existing... ");
    
    sanitizer_cache_t* cache = sanitizer_cache_create(10);
    assert(cache != NULL);
    
    sanitizer_cache_put(cache, "key1", "val1", SANITIZE_LEVEL_NORMAL);
    sanitizer_cache_put(cache, "key1", "val1_updated", SANITIZE_LEVEL_NORMAL);
    
    char* result = sanitizer_cache_get(cache, "key1", SANITIZE_LEVEL_NORMAL);
    assert(result != NULL);
    assert(strcmp(result, "val1_updated") == 0);
    free(result);
    
    sanitizer_cache_destroy(cache);
    printf("PASS\n");
}

static void test_cache_clear(void) {
    printf("Test: cache_clear... ");
    
    sanitizer_cache_t* cache = sanitizer_cache_create(10);
    assert(cache != NULL);
    
    sanitizer_cache_put(cache, "key1", "val1", SANITIZE_LEVEL_NORMAL);
    sanitizer_cache_put(cache, "key2", "val2", SANITIZE_LEVEL_NORMAL);
    
    sanitizer_cache_clear(cache);
    
    char* r1 = sanitizer_cache_get(cache, "key1", SANITIZE_LEVEL_NORMAL);
    assert(r1 == NULL);
    
    sanitizer_cache_destroy(cache);
    printf("PASS\n");
}

static void test_cache_different_levels(void) {
    printf("Test: cache_different_levels... ");
    
    sanitizer_cache_t* cache = sanitizer_cache_create(10);
    assert(cache != NULL);
    
    sanitizer_cache_put(cache, "input", "output_strict", SANITIZE_LEVEL_STRICT);
    sanitizer_cache_put(cache, "input", "output_normal", SANITIZE_LEVEL_NORMAL);
    sanitizer_cache_put(cache, "input", "output_relaxed", SANITIZE_LEVEL_RELAXED);
    
    char* r1 = sanitizer_cache_get(cache, "input", SANITIZE_LEVEL_STRICT);
    assert(r1 != NULL);
    assert(strcmp(r1, "output_strict") == 0);
    free(r1);
    
    char* r2 = sanitizer_cache_get(cache, "input", SANITIZE_LEVEL_NORMAL);
    assert(r2 != NULL);
    assert(strcmp(r2, "output_normal") == 0);
    free(r2);
    
    char* r3 = sanitizer_cache_get(cache, "input", SANITIZE_LEVEL_RELAXED);
    assert(r3 != NULL);
    assert(strcmp(r3, "output_relaxed") == 0);
    free(r3);
    
    sanitizer_cache_destroy(cache);
    printf("PASS\n");
}

static void test_cache_null_handling(void) {
    printf("Test: cache_null_handling... ");
    
    sanitizer_cache_t* cache = sanitizer_cache_create(10);
    assert(cache != NULL);
    
    char* r1 = sanitizer_cache_get(NULL, "input", SANITIZE_LEVEL_NORMAL);
    assert(r1 == NULL);
    
    char* r2 = sanitizer_cache_get(cache, NULL, SANITIZE_LEVEL_NORMAL);
    assert(r2 == NULL);
    
    sanitizer_cache_put(NULL, "input", "output", SANITIZE_LEVEL_NORMAL);
    sanitizer_cache_put(cache, NULL, "output", SANITIZE_LEVEL_NORMAL);
    sanitizer_cache_put(cache, "input", NULL, SANITIZE_LEVEL_NORMAL);
    
    sanitizer_cache_clear(NULL);
    sanitizer_cache_destroy(NULL);
    
    sanitizer_cache_destroy(cache);
    printf("PASS\n");
}

int main(void) {
    printf("=== Sanitizer Cache LRU Tests ===\n");
    
    test_cache_create_destroy();
    test_cache_put_get();
    test_cache_lru_eviction();
    test_cache_update_existing();
    test_cache_clear();
    test_cache_different_levels();
    test_cache_null_handling();
    
    printf("=== All tests PASSED ===\n");
    return 0;
}
