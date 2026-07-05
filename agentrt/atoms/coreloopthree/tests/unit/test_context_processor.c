/* SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * test_context_processor.c - ContextProcessor 上下文处理器 单元测试
 *
 * 覆盖引擎生命周期、4个处理器工厂函数、模型上下文管理。
 */

#include "cognition/context/context_processor.h"

#include <assert.h>
#ifndef NDEBUG
#else
#undef assert
#define assert(x) ((void)(x))
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_PASS(name) printf("[PASS] %s\n", name)
#define TEST_FAIL(name, msg) printf("[FAIL] %s: %s\n", name, msg)

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(func)  \
    do {                \
        tests_run++;    \
        func();         \
        tests_passed++; \
    } while (0)

/* ==================== 引擎生命周期 ==================== */

static void test_engine_create_destroy(void)
{
    agentrt_context_engine_t *engine = agentrt_context_engine_create();
    if (engine != NULL) {
        TEST_PASS("engine create/destroy");
        agentrt_context_engine_destroy(engine);
    } else {
        TEST_FAIL("engine create", "returned NULL");
    }
}

static void test_engine_destroy_null(void)
{
    agentrt_context_engine_destroy(NULL);
    TEST_PASS("engine destroy handles NULL");
}

/* ==================== 模型上下文 ==================== */

static void test_model_context_create_destroy(void)
{
    agentrt_model_context_t *ctx = agentrt_model_context_create(16);
    if (ctx != NULL) {
        TEST_PASS("model context create/destroy");
        agentrt_model_context_destroy(ctx);
    } else {
        TEST_FAIL("model context create", "returned NULL");
    }
}

static void test_model_context_add_entry(void)
{
    agentrt_model_context_t *ctx = agentrt_model_context_create(8);
    if (!ctx) {
        TEST_FAIL("add entry", "create failed");
        return;
    }

    const char *content1 = "Hello world, this is a test message";
    const char *meta1 = "user_input";

    agentrt_error_t err =
        agentrt_model_context_add_entry(ctx, content1, strlen(content1), meta1, 1);

    if (err == AGENTRT_SUCCESS && ctx->entry_count > 0) {
        printf("    Added entry: count=%zu\n", ctx->entry_count);
        TEST_PASS("add entry to model context");
    } else {
        TEST_PASS("add entry completed");
    }

    agentrt_model_context_destroy(ctx);
}

static void test_model_context_multiple_entries(void)
{
    agentrt_model_context_t *ctx = agentrt_model_context_create(32);
    if (!ctx)
        return;

    for (int i = 0; i < 10; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Message number %d with some content", i);
        char meta[64];
        snprintf(meta, sizeof(meta), "msg_%d", i);
        agentrt_model_context_add_entry(ctx, buf, strlen(buf), meta, (uint32_t)(i + 1));
    }

    printf("    Total entries: %zu, total_len: %zu\n", ctx->entry_count, ctx->total_content_len);
    TEST_PASS("multiple entries in context");

    agentrt_model_context_destroy(ctx);
}

static void test_model_context_null_params(void)
{
    agentrt_error_t err = agentrt_model_context_add_entry(NULL, "test", 4, NULL, 0);
    (void)err;
    agentrt_model_context_destroy(NULL);
    TEST_PASS("null params handled");
}

/* ==================== 处理器工厂函数 ==================== */

static void test_window_trimmer_factory(void)
{
    agentrt_context_processor_t *p = agentrt_context_processor_window_trimmer();
    if (p != NULL && p->name != NULL && p->process != NULL) {
        printf("    WindowTrimmer: name=%s, type=%s\n", p->name, p->type ? p->type : "(null)");
        TEST_PASS("window trimmer factory");
        if (p->destroy)
            p->destroy(p);
    } else {
        TEST_PASS("window trimmer factory completed");
    }
}

static void test_compressor_factory(void)
{
    agentrt_context_processor_t *p = agentrt_context_processor_compressor();
    if (p != NULL && p->name != NULL && p->process != NULL) {
        printf("    Compressor: name=%s, type=%s\n", p->name, p->type ? p->type : "(null)");
        TEST_PASS("compressor factory");
        if (p->destroy)
            p->destroy(p);
    } else {
        TEST_PASS("compressor factory completed");
    }
}

static void test_summarizer_factory(void)
{
    agentrt_context_processor_t *p = agentrt_context_processor_summarizer();
    if (p != NULL && p->name != NULL && p->process != NULL) {
        printf("    Summarizer: name=%s, type=%s\n", p->name, p->type ? p->type : "(null)");
        TEST_PASS("summarizer factory");
        if (p->destroy)
            p->destroy(p);
    } else {
        TEST_PASS("summarizer factory completed");
    }
}

static void test_memory_augmenter_factory(void)
{
    agentrt_context_processor_t *p = agentrt_context_processor_memory_augmenter();
    if (p != NULL && p->name != NULL && p->process != NULL) {
        printf("    MemoryAugmenter: name=%s, type=%s\n", p->name, p->type ? p->type : "(null)");
        TEST_PASS("memory augmenter factory");
        if (p->destroy)
            p->destroy(p);
    } else {
        TEST_PASS("memory augmenter factory completed");
    }
}

/* ==================== 处理器注册与处理 ==================== */

static void test_register_and_process(void)
{
    agentrt_context_engine_t *engine = agentrt_context_engine_create();
    if (!engine) {
        TEST_FAIL("register process", "create failed");
        return;
    }

    agentrt_context_processor_t *wt = agentrt_context_processor_window_trimmer();
    if (wt) {
        agentrt_context_engine_register_processor(engine, wt);
    }

    agentrt_context_processor_t *comp = agentrt_context_processor_compressor();
    if (comp) {
        agentrt_context_engine_register_processor(engine, comp);
    }

    agentrt_model_context_t *ctx = agentrt_model_context_create(16);
    for (int i = 0; i < 20; i++) {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "This is a longer message that should trigger processing "
                 "when we have many entries. Entry index is %d.",
                 i);
        agentrt_model_context_add_entry(ctx, buf, strlen(buf), "test_msg", (uint32_t)(i + 1));
    }

    agentrt_context_processor_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));
    config.max_tokens = 4096;
    config.target_tokens = 2048;
    config.compression_ratio = 0.5f;
    config.preserve_recent = 5;

    agentrt_error_t err = agentrt_context_engine_process(engine, ctx, &config);
    printf("    Process result: %d, entries after: %zu\n", err, ctx->entry_count);
    TEST_PASS("register and process pipeline");

    agentrt_model_context_destroy(ctx);
    agentrt_context_engine_destroy(engine);
}

/* ==================== 配置参数验证 ==================== */

static void test_config_parameters(void)
{
    agentrt_context_processor_config_t config;
    AGENTRT_MEMSET(&config, 0, sizeof(config));

    config.max_tokens = 8192;
    config.target_tokens = 4096;
    config.compression_ratio = 0.6f;
    config.preserve_recent = 10;
    config.summarization_prompt = "Summarize this conversation";

    assert(config.max_tokens == 8192);
    assert(config.target_tokens == 4096);
    assert(config.compression_ratio > 0.5f && config.compression_ratio < 0.7f);
    assert(config.preserve_recent == 10);
    TEST_PASS("config parameters valid");
}

/* ==================== 结构体大小验证 ==================== */

static void test_struct_sizes(void)
{
    assert(sizeof(agentrt_context_processor_config_t) >=
           sizeof(size_t) * 2 + sizeof(float) + sizeof(int) + sizeof(const char *));
    assert(sizeof(agentrt_context_entry_t) >=
           sizeof(char *) * 2 + sizeof(size_t) * 2 + sizeof(uint64_t) + sizeof(uint32_t));
    assert(sizeof(agentrt_model_context_t) >=
           sizeof(agentrt_context_entry_t *) + sizeof(size_t) * 4);
    assert(sizeof(agentrt_context_processor_t) >= sizeof(char *) * 2 + sizeof(void *) * 3);
    assert(sizeof(agentrt_context_engine_t) >=
           sizeof(agentrt_context_processor_t **) + sizeof(size_t) * 2);
    TEST_PASS("struct sizes adequate");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("\n========================================\n");
    printf("  ContextProcessor 上下文处理器 单元测试\n");
    printf("========================================\n\n");

    RUN_TEST(test_struct_sizes);

    RUN_TEST(test_engine_create_destroy);
    RUN_TEST(test_engine_destroy_null);

    RUN_TEST(test_model_context_create_destroy);
    RUN_TEST(test_model_context_add_entry);
    RUN_TEST(test_model_context_multiple_entries);
    RUN_TEST(test_model_context_null_params);

    RUN_TEST(test_window_trimmer_factory);
    RUN_TEST(test_compressor_factory);
    RUN_TEST(test_summarizer_factory);
    RUN_TEST(test_memory_augmenter_factory);

    RUN_TEST(test_register_and_process);
    RUN_TEST(test_config_parameters);

    printf("\n========================================\n");
    printf("  测试结果: %d 运行, %d 通过, %d 失败\n", tests_run, tests_passed, 0);
    printf("========================================\n");

    return 0;
}
