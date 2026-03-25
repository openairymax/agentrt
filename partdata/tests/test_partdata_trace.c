/**
 * @file test_partdata_trace.c
 * @brief AgentOS 数据分区追踪存储单元测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "partdata.h"
#include "partdata_trace.h"

static void test_trace_init_shutdown(void) {
    printf("Test: trace_init_shutdown...");

    partdata_error_t err = partdata_trace_init();
    assert(err == PARTDATA_SUCCESS);

    partdata_trace_shutdown();

    printf("PASS\n");
}

static void test_trace_write_span(void) {
    printf("Test: trace_write_span...");

    partdata_error_t err = partdata_trace_init();
    assert(err == PARTDATA_SUCCESS);

    partdata_span_t span;
    memset(&span, 0, sizeof(span));

    snprintf(span.trace_id, sizeof(span.trace_id), "trace_%ld", (long)time(NULL));
    snprintf(span.span_id, sizeof(span.span_id), "span_001");
    snprintf(span.parent_span_id, sizeof(span.parent_span_id), "");
    snprintf(span.name, sizeof(span.name), "test_operation");
    span.start_time_ns = (uint64_t)time(NULL) * 1000000000;
    span.end_time_ns = span.start_time_ns + 100000000;
    snprintf(span.service_name, sizeof(span.service_name), "test_service");
    snprintf(span.status, sizeof(span.status), "OK");

    err = partdata_trace_write_span(&span);
    assert(err == PARTDATA_SUCCESS);

    partdata_trace_shutdown();

    printf("PASS\n");
}

static void test_trace_write_batch(void) {
    printf("Test: trace_write_batch...");

    partdata_error_t err = partdata_trace_init();
    assert(err == PARTDATA_SUCCESS);

    partdata_span_t spans[5];
    memset(spans, 0, sizeof(spans));

    for (int i = 0; i < 5; i++) {
        snprintf(spans[i].trace_id, sizeof(spans[i].trace_id), "trace_batch_%ld", (long)time(NULL));
        snprintf(spans[i].span_id, sizeof(spans[i].span_id), "span_%d", i);
        snprintf(spans[i].name, sizeof(spans[i].name), "batch_operation_%d", i);
        spans[i].start_time_ns = (uint64_t)time(NULL) * 1000000000;
        spans[i].end_time_ns = spans[i].start_time_ns + 50000000;
        snprintf(spans[i].service_name, sizeof(spans[i].service_name), "batch_service");
        snprintf(spans[i].status, sizeof(spans[i].status), "OK");
    }

    err = partdata_trace_write_spans_batch(spans, 5);
    assert(err == PARTDATA_SUCCESS);

    partdata_trace_shutdown();

    printf("PASS\n");
}

static void test_trace_flush(void) {
    printf("Test: trace_flush...");

    partdata_error_t err = partdata_trace_init();
    assert(err == PARTDATA_SUCCESS);

    partdata_span_t span;
    memset(&span, 0, sizeof(span));

    snprintf(span.trace_id, sizeof(span.trace_id), "trace_flush_%ld", (long)time(NULL));
    snprintf(span.span_id, sizeof(span.span_id), "span_flush");
    snprintf(span.name, sizeof(span.name), "flush_test");
    span.start_time_ns = (uint64_t)time(NULL) * 1000000000;
    span.end_time_ns = span.start_time_ns + 10000000;
    snprintf(span.service_name, sizeof(span.service_name), "flush_service");
    snprintf(span.status, sizeof(span.status), "OK");

    err = partdata_trace_write_span(&span);
    assert(err == PARTDATA_SUCCESS);

    err = partdata_trace_flush();
    assert(err == PARTDATA_SUCCESS);

    partdata_trace_shutdown();

    printf("PASS\n");
}

static void test_trace_invalid_params(void) {
    printf("Test: trace_invalid_params...");

    partdata_error_t err = partdata_trace_init();
    assert(err == PARTDATA_SUCCESS);

    err = partdata_trace_write_span(NULL);
    assert(err == PARTDATA_ERR_INVALID_PARAM);

    partdata_span_t invalid_span;
    memset(&invalid_span, 0, sizeof(invalid_span));
    err = partdata_trace_write_span(&invalid_span);
    assert(err == PARTDATA_ERR_INVALID_PARAM);

    err = partdata_trace_write_spans_batch(NULL, 0);
    assert(err == PARTDATA_ERR_INVALID_PARAM);

    err = partdata_trace_write_spans_batch(NULL, 10);
    assert(err == PARTDATA_ERR_INVALID_PARAM);

    partdata_trace_shutdown();

    printf("PASS\n");
}

static void test_trace_stats(void) {
    printf("Test: trace_stats...");

    partdata_error_t err = partdata_trace_init();
    assert(err == PARTDATA_SUCCESS);

    uint64_t total = 0, pending = 0, size = 0;

    err = partdata_trace_get_stats(&total, &pending, &size);
    assert(err == PARTDATA_SUCCESS);

    partdata_span_t span;
    memset(&span, 0, sizeof(span));

    snprintf(span.trace_id, sizeof(span.trace_id), "trace_stats_%ld", (long)time(NULL));
    snprintf(span.span_id, sizeof(span.span_id), "span_stats");
    snprintf(span.name, sizeof(span.name), "stats_test");
    span.start_time_ns = (uint64_t)time(NULL) * 1000000000;
    span.end_time_ns = span.start_time_ns + 10000000;
    snprintf(span.service_name, sizeof(span.service_name), "stats_service");
    snprintf(span.status, sizeof(span.status), "OK");

    err = partdata_trace_write_span(&span);
    assert(err == PARTDATA_SUCCESS);

    err = partdata_trace_get_stats(&total, &pending, &size);
    assert(err == PARTDATA_SUCCESS);

    partdata_trace_shutdown();

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS Partdata Trace Unit Tests ===\n\n");

    test_trace_init_shutdown();
    test_trace_write_span();
    test_trace_write_batch();
    test_trace_flush();
    test_trace_invalid_params();
    test_trace_stats();

    printf("\n=== All Trace Tests Passed ===\n");
    return 0;
}
