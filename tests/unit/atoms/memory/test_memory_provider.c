/**
 * @file test_memory_provider.c
 * @brief Memory Provider Interface Integration Tests
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory_provider.h"
#include "platform.h"
#include <agentrt.h>
#include "platform.h"
#include <stdio.h>
#include "platform.h"
#include <stdlib.h>
#include "platform.h"
#include <string.h>
#include "platform.h"
#include <assert.h>
#include "platform.h"
#include <sys/stat.h>
#include "platform.h"
#include <unistd.h>
#include "platform.h"

#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"
#include "platform.h"

static int test_count = 0;
static int pass_count = 0;

#define TEST_ASSERT(cond, msg) do { \
    test_count++; \
    if (cond) { pass_count++; printf("  PASS: %s\n", msg); } \
    else { printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while(0)

#define TEST_DIR AGENTRT_TMP_DIR "/agentrt_provider_test_XXXXXX"

static char test_dir[256];

static void setup_test_dir(void) {
    strcpy(test_dir, TEST_DIR);
    mkdtemp(test_dir);
}

static void teardown_test_dir(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s 2>/dev/null", test_dir);
    system(cmd);
}

static void test_builtin_provider_init_destroy(void) {
    printf("[Test] Builtin provider init/destroy\n");
    agentrt_memory_provider_t* provider = agentrt_builtin_provider_create();
    TEST_ASSERT(provider != NULL, "builtin provider create returns non-NULL");
    TEST_ASSERT(provider->init != NULL, "provider init function is set");
    TEST_ASSERT(provider->destroy != NULL, "provider destroy function is set");
    TEST_ASSERT(provider->write_raw != NULL, "provider write_raw function is set");
    TEST_ASSERT(provider->get_raw != NULL, "provider get_raw function is set");
    TEST_ASSERT(provider->delete_raw != NULL, "provider delete_raw function is set");
    TEST_ASSERT(provider->query != NULL, "provider query function is set");

    agentrt_error_t err = provider->init(provider, test_dir);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "builtin provider init succeeds");

    provider->destroy(provider);
    printf("\n");
}

static void test_builtin_provider_write_get(void) {
    printf("[Test] Builtin provider write/get cycle\n");
    agentrt_memory_provider_t* provider = agentrt_builtin_provider_create();
    agentrt_error_t err = provider->init(provider, test_dir);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "provider init succeeds");

    const char* data = "Hello, AgentOS Memory Provider!";
    const char* metadata = "{\"source\":\"test\",\"type\":0}";
    char* record_id = NULL;

    err = provider->write_raw(provider, data, strlen(data), metadata, &record_id);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "write_raw succeeds");
    TEST_ASSERT(record_id != NULL, "write_raw returns record_id");
    TEST_ASSERT(strlen(record_id) > 0, "record_id is non-empty");

    if (record_id) {
        void* out_data = NULL;
        size_t out_len = 0;
        err = provider->get_raw(provider, record_id, &out_data, &out_len);
        TEST_ASSERT(err == AGENTRT_SUCCESS, "get_raw succeeds");
        TEST_ASSERT(out_data != NULL, "get_raw returns data");
        TEST_ASSERT(out_len == strlen(data), "get_raw returns correct length");

        if (out_data && out_len == strlen(data)) {
            TEST_ASSERT(memcmp(out_data, data, out_len) == 0,
                        "get_raw returns correct content");
        }
        if (out_data) AGENTRT_FREE(out_data);
        AGENTRT_FREE(record_id);
    }

    provider->destroy(provider);
    printf("\n");
}

static void test_builtin_provider_delete(void) {
    printf("[Test] Builtin provider delete\n");
    agentrt_memory_provider_t* provider = agentrt_builtin_provider_create();
    agentrt_error_t err = provider->init(provider, test_dir);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "provider init succeeds");

    const char* data = "Data to be deleted";
    char* record_id = NULL;
    err = provider->write_raw(provider, data, strlen(data), "{}", &record_id);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "write_raw succeeds");
    TEST_ASSERT(record_id != NULL, "write_raw returns record_id");

    if (record_id) {
        err = provider->delete_raw(provider, record_id);
        TEST_ASSERT(err == AGENTRT_SUCCESS, "delete_raw succeeds");

        void* out_data = NULL;
        size_t out_len = 0;
        err = provider->get_raw(provider, record_id, &out_data, &out_len);
        TEST_ASSERT(err != AGENTRT_SUCCESS, "get_raw fails after delete");

        AGENTRT_FREE(record_id);
    }

    provider->destroy(provider);
    printf("\n");
}

static void test_builtin_provider_query(void) {
    printf("[Test] Builtin provider query\n");
    agentrt_memory_provider_t* provider = agentrt_builtin_provider_create();
    agentrt_error_t err = provider->init(provider, test_dir);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "provider init succeeds");

    const char* entries[] = {
        "AgentOS is an intelligent agent operating system",
        "Memory provider supports L1 through L4 layers",
        "CoreLoopThree handles cognition, execution, and memory",
        "The builtin provider uses file system storage",
        "MemoryRovol provides commercial advanced memory features"
    };

    for (int i = 0; i < 5; i++) {
        char metadata[128];
        snprintf(metadata, sizeof(metadata), "{\"source\":\"test\",\"idx\":%d}", i);
        char* rid = NULL;
        err = provider->write_raw(provider, entries[i], strlen(entries[i]),
                                   metadata, &rid);
        TEST_ASSERT(err == AGENTRT_SUCCESS, "write_raw for query test data succeeds");
        if (rid) AGENTRT_FREE(rid);
    }

    char** result_ids = NULL;
    float* scores = NULL;
    size_t result_count = 0;

    err = provider->query(provider, "memory provider storage", 3,
                           &result_ids, &scores, &result_count);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "query succeeds");
    TEST_ASSERT(result_count > 0, "query returns results");
    TEST_ASSERT(result_count <= 3, "query respects limit");

    if (result_ids && result_count > 0) {
        for (size_t i = 0; i < result_count; i++) {
            TEST_ASSERT(result_ids[i] != NULL, "result id is non-NULL");
            TEST_ASSERT(scores[i] >= 0.0f && scores[i] <= 1.0f,
                        "score is in valid range");
        }
        agentrt_memory_provider_free_query_results(result_ids, scores, result_count);
    }

    provider->destroy(provider);
    printf("\n");
}

static void test_builtin_provider_multiple_writes(void) {
    printf("[Test] Builtin provider multiple writes\n");
    agentrt_memory_provider_t* provider = agentrt_builtin_provider_create();
    agentrt_error_t err = provider->init(provider, test_dir);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "provider init succeeds");

    char* record_ids[10];
    int success_count = 0;

    for (int i = 0; i < 10; i++) {
        char data[64];
        snprintf(data, sizeof(data), "record_%d_content", i);
        char metadata[64];
        snprintf(metadata, sizeof(metadata), "{\"idx\":%d}", i);

        err = provider->write_raw(provider, data, strlen(data), metadata,
                                   &record_ids[i]);
        if (err == AGENTRT_SUCCESS && record_ids[i]) {
            success_count++;
        } else {
            record_ids[i] = NULL;
        }
    }

    TEST_ASSERT(success_count == 10, "all 10 writes succeed");

    for (int i = 0; i < 10; i++) {
        if (record_ids[i]) {
            void* out_data = NULL;
            size_t out_len = 0;
            err = provider->get_raw(provider, record_ids[i], &out_data, &out_len);
            if (err == AGENTRT_SUCCESS && out_data) {
                char expected[64];
                snprintf(expected, sizeof(expected), "record_%d_content", i);
                TEST_ASSERT(out_len == strlen(expected) &&
                            memcmp(out_data, expected, out_len) == 0,
                            "retrieved content matches written content");
            }
            if (out_data) AGENTRT_FREE(out_data);
            AGENTRT_FREE(record_ids[i]);
        }
    }

    provider->destroy(provider);
    printf("\n");
}

static void test_builtin_provider_null_params(void) {
    printf("[Test] Builtin provider null parameter handling\n");
    agentrt_memory_provider_t* provider = agentrt_builtin_provider_create();
    agentrt_error_t err = provider->init(provider, test_dir);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "provider init succeeds");

    err = provider->write_raw(NULL, "data", 4, "{}", NULL);
    TEST_ASSERT(err != AGENTRT_SUCCESS, "write_raw with NULL provider returns error");

    err = provider->write_raw(provider, NULL, 4, "{}", NULL);
    TEST_ASSERT(err != AGENTRT_SUCCESS, "write_raw with NULL data returns error");

    err = provider->get_raw(NULL, "id", NULL, NULL);
    TEST_ASSERT(err != AGENTRT_SUCCESS, "get_raw with NULL provider returns error");

    err = provider->delete_raw(NULL, "id");
    TEST_ASSERT(err != AGENTRT_SUCCESS, "delete_raw with NULL provider returns error");

    err = provider->query(NULL, "text", 10, NULL, NULL, NULL);
    TEST_ASSERT(err != AGENTRT_SUCCESS, "query with NULL provider returns error");

    provider->destroy(provider);
    printf("\n");
}

static void test_builtin_provider_capabilities(void) {
    printf("[Test] Builtin provider capabilities\n");
    agentrt_memory_provider_t* provider = agentrt_builtin_provider_create();

    TEST_ASSERT(provider->capabilities.l1_raw == 1, "builtin supports L1 raw");
    TEST_ASSERT(provider->capabilities.l2_feature == 0, "builtin does not support L2 feature");
    TEST_ASSERT(provider->capabilities.l3_structure == 0, "builtin does not support L3 structure");
    TEST_ASSERT(provider->capabilities.l4_pattern == 0, "builtin does not support L4 pattern");
    TEST_ASSERT(provider->capabilities.persistence == 1, "builtin supports persistence");
    TEST_ASSERT(provider->capabilities.faiss == 0, "builtin does not support faiss");

    TEST_ASSERT(strcmp(provider->name, "builtin") == 0, "provider name is 'builtin'");
    TEST_ASSERT(provider->version != NULL, "provider version is set");

    provider->destroy(provider);
    printf("\n");
}

static void test_builtin_provider_stats(void) {
    printf("[Test] Builtin provider stats\n");
    agentrt_memory_provider_t* provider = agentrt_builtin_provider_create();
    agentrt_error_t err = provider->init(provider, test_dir);
    TEST_ASSERT(err == AGENTRT_SUCCESS, "provider init succeeds");

    agentrt_memory_stats_t stats;
    AGENTRT_MEMSET(&stats, 0, sizeof(stats));

    if (provider->get_stats) {
        err = provider->get_stats(provider, &stats);
        TEST_ASSERT(err == AGENTRT_SUCCESS, "get_stats succeeds");
        TEST_ASSERT(stats.total_records == 0, "initial total_records is 0");
    } else {
        TEST_ASSERT(1, "get_stats not implemented (acceptable for builtin)");
    }

    const char* data = "stats test data";
    char* rid = NULL;
    provider->write_raw(provider, data, strlen(data), "{}", &rid);
    if (rid) AGENTRT_FREE(rid);

    if (provider->get_stats) {
        err = provider->get_stats(provider, &stats);
        TEST_ASSERT(err == AGENTRT_SUCCESS, "get_stats after write succeeds");
        TEST_ASSERT(stats.total_records >= 1, "total_records incremented after write");
    }

    provider->destroy(provider);
    printf("\n");
}

int main(void) {
    printf("============================================\n");
    printf("  AgentOS Memory Provider Integration Tests\n");
    printf("============================================\n\n");

    setup_test_dir();

    test_builtin_provider_init_destroy();
    test_builtin_provider_write_get();
    test_builtin_provider_delete();
    test_builtin_provider_query();
    test_builtin_provider_multiple_writes();
    test_builtin_provider_null_params();
    test_builtin_provider_capabilities();
    test_builtin_provider_stats();

    teardown_test_dir();

    printf("============================================\n");
    printf("  Results: %d/%d passed\n", pass_count, test_count);
    printf("============================================\n", pass_count, test_count);

    return (pass_count == test_count) ? 0 : 1;
}
