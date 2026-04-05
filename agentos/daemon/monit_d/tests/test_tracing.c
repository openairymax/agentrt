/**
 * @file test_tracing.c
 * @brief 追踪模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "monitor_service.h"

static void test_tracer_create_destroy(void) {
    printf("  test_tracer_create_destroy...\n");

    tracer_t* tracer = tracer_create(NULL);
    assert(tracer != NULL);

    tracer_destroy(tracer);

    printf("    PASSED\n");
}

static void test_tracer_span_create(void) {
    printf("  test_tracer_span_create...\n");

    tracer_t* tracer = tracer_create(NULL);
    assert(tracer != NULL);

    span_t* span = tracer_span_create(tracer, "test_operation");
    assert(span != NULL);
    assert(strcmp(span->operation_name, "test_operation") == 0);

    tracer_span_destroy(span);
    tracer_destroy(tracer);

    printf("    PASSED\n");
}

static void test_tracer_span_context(void) {
    printf("  test_tracer_span_context...\n");

    tracer_t* tracer = tracer_create(NULL);
    assert(tracer != NULL);

    span_t* span = tracer_span_create(tracer, "context_test");
    assert(span != NULL);

    span_context_t* ctx = tracer_span_get_context(span);
    assert(ctx != NULL);
    assert(ctx->trace_id != NULL);
    assert(ctx->span_id != NULL);

    tracer_span_destroy(span);
    tracer_destroy(tracer);

    printf("    PASSED\n");
}

static void test_tracer_span_attributes(void) {
    printf("  test_tracer_span_attributes...\n");

    tracer_t* tracer = tracer_create(NULL);
    assert(tracer != NULL);

    span_t* span = tracer_span_create(tracer, "attr_test");
    assert(span != NULL);

    int ret = tracer_span_set_attribute(span, "user.id", "12345");
    assert(ret == 0);

    ret = tracer_span_set_attribute(span, "request.size", "1024");
    assert(ret == 0);

    const char* value = tracer_span_get_attribute(span, "user.id");
    assert(value != NULL);
    assert(strcmp(value, "12345") == 0);

    tracer_span_destroy(span);
    tracer_destroy(tracer);

    printf("    PASSED\n");
}

static void test_tracer_span_events(void) {
    printf("  test_tracer_span_events...\n");

    tracer_t* tracer = tracer_create(NULL);
    assert(tracer != NULL);

    span_t* span = tracer_span_create(tracer, "event_test");
    assert(span != NULL);

    int ret = tracer_span_add_event(span, "request_received", NULL);
    assert(ret == 0);

    ret = tracer_span_add_event(span, "processing_started", NULL);
    assert(ret == 0);

    tracer_span_destroy(span);
    tracer_destroy(tracer);

    printf("    PASSED\n");
}

static void test_tracer_span_status(void) {
    printf("  test_tracer_span_status...\n");

    tracer_t* tracer = tracer_create(NULL);
    assert(tracer != NULL);

    span_t* span = tracer_span_create(tracer, "status_test");
    assert(span != NULL);

    int ret = tracer_span_set_status(span, SPAN_STATUS_OK, "Operation successful");
    assert(ret == 0);

    span_status_t status = tracer_span_get_status(span);
    assert(status == SPAN_STATUS_OK);

    tracer_span_destroy(span);
    tracer_destroy(tracer);

    printf("    PASSED\n");
}

static void test_tracer_export_otlp(void) {
    printf("  test_tracer_export_otlp...\n");

    tracer_t* tracer = tracer_create(NULL);
    assert(tracer != NULL);

    span_t* span = tracer_span_create(tracer, "export_test");
    assert(span != NULL);

    tracer_span_set_attribute(span, "test.key", "test.value");
    tracer_span_set_status(span, SPAN_STATUS_OK, NULL);

    char* otlp_json = tracer_span_export_otlp(span);
    assert(otlp_json != NULL);

    free(otlp_json);
    tracer_span_destroy(span);
    tracer_destroy(tracer);

    printf("    PASSED\n");
}

static void test_tracer_child_span(void) {
    printf("  test_tracer_child_span...\n");

    tracer_t* tracer = tracer_create(NULL);
    assert(tracer != NULL);

    span_t* parent = tracer_span_create(tracer, "parent_operation");
    assert(parent != NULL);

    span_t* child = tracer_span_create_child(parent, "child_operation");
    assert(child != NULL);

    span_context_t* parent_ctx = tracer_span_get_context(parent);
    span_context_t* child_ctx = tracer_span_get_context(child);

    assert(strcmp(parent_ctx->trace_id, child_ctx->trace_id) == 0);

    tracer_span_destroy(child);
    tracer_span_destroy(parent);
    tracer_destroy(tracer);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tracer Unit Tests\n");
    printf("=========================================\n");

    test_tracer_create_destroy();
    test_tracer_span_create();
    test_tracer_span_context();
    test_tracer_span_attributes();
    test_tracer_span_events();
    test_tracer_span_status();
    test_tracer_export_otlp();
    test_tracer_child_span();

    printf("\n✅ All tracing tests PASSED\n");
    return 0;
}
