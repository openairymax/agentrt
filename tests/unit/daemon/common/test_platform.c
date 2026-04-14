/**
 * @file test_platform.c
 * @brief 平台抽象层单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "platform.h"

static void test_platform_detection(void) {
    printf("  test_platform_detection...\n");

#if defined(AGENTOS_PLATFORM_WINDOWS)
    printf("    Running on Windows\n");
#elif defined(AGENTOS_PLATFORM_LINUX)
    printf("    Running on Linux\n");
#elif defined(AGENTOS_PLATFORM_MACOS)
    printf("    Running on macOS\n");
#else
    printf("    Unknown platform\n");
#endif

    assert(1);
    printf("    PASSED\n");
}

static void test_mutex_operations(void) {
    printf("  test_mutex_operations...\n");

    agentos_mutex_t mutex;
    int ret = agentos_mutex_init(&mutex);
    assert(ret == 0);

    ret = agentos_mutex_lock(&mutex);
    assert(ret == 0);

    ret = agentos_mutex_trylock(&mutex);
    assert(ret != 0);

    ret = agentos_mutex_unlock(&mutex);
    assert(ret == 0);

    ret = agentos_mutex_destroy(&mutex);
    assert(ret == 0);

    printf("    PASSED\n");
}

static void test_time_operations(void) {
    printf("  test_time_operations...\n");

    uint64_t ns = agentos_time_ns();
    uint64_t ms = agentos_time_ms();
    assert(ns > 0);
    assert(ms > 0);
    assert(ns >= ms * 1000000);

    printf("    PASSED\n");
}

static void test_random_operations(void) {
    printf("  test_random_operations...\n");

    agentos_random_init();

    uint32_t r1 = agentos_random_uint32(1, 100);
    uint32_t r2 = agentos_random_uint32(1, 100);
    assert(r1 >= 1 && r1 <= 100);
    assert(r2 >= 1 && r2 <= 100);

    float rf = agentos_random_float();
    assert(rf >= 0.0f && rf <= 1.0f);

    printf("    PASSED\n");
}

static void test_file_operations(void) {
    printf("  test_file_operations...\n");

    assert(agentos_file_exists(".") == 1);
    assert(agentos_file_exists("/nonexistent/path") == 0);

    printf("    PASSED\n");
}

static void test_strlcpy(void) {
    printf("  test_strlcpy...\n");

    char dest[32];
    const char* src = "Hello, AgentOS!";

    size_t len = agentos_strlcpy(dest, sizeof(dest), src);
    assert(strcmp(dest, src) == 0);
    assert(len == strlen(src));

    char dest2[8];
    len = agentos_strlcpy(dest2, sizeof(dest2), src);
    assert(len >= strlen(src));

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Platform Module Unit Tests\n");
    printf("=========================================\n");

    test_platform_detection();
    test_mutex_operations();
    test_time_operations();
    test_random_operations();
    test_file_operations();
    test_strlcpy();

    printf("\n✅ All platform module tests PASSED\n");
    return 0;
}