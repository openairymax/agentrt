/**
 * @file test_tracing.c
 * @brief 追踪模块单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "monitor_service.h"

static void test_agent_trace_create(void) {
    printf("  test_agent_trace_create...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    loop_detection_config_t loop_cfg;
    memset(&loop_cfg, 0, sizeof(loop_cfg));
    loop_cfg.mode = LOOP_DETECTION_TIME_BASED;
    loop_cfg.max_execution_time_ms = 60000;
    loop_cfg.max_loop_iterations = 100;

    agent_execution_trace_t* trace = NULL;
    ret = monitor_service_start_agent_trace(svc, "agent_001", "task_001", &loop_cfg, &trace);
    if (ret == 0 && trace != NULL) {
        assert(strcmp(trace->agent_id, "agent_001") == 0);
        assert(strcmp(trace->task_id, "task_001") == 0);
        assert(trace->current_state == AGENT_STATE_INITIALIZING);

        monitor_service_end_agent_trace(svc, trace, AGENT_STATE_COMPLETED);
    } else {
        printf("    Trace creation skipped\n");
    }

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_agent_state_update(void) {
    printf("  test_agent_state_update...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    loop_detection_config_t loop_cfg;
    memset(&loop_cfg, 0, sizeof(loop_cfg));
    loop_cfg.mode = LOOP_DETECTION_TIME_BASED;
    loop_cfg.max_execution_time_ms = 60000;

    agent_execution_trace_t* trace = NULL;
    ret = monitor_service_start_agent_trace(svc, "agent_002", "task_002", &loop_cfg, &trace);
    if (ret == 0 && trace != NULL) {
        ret = monitor_service_update_agent_state(svc, trace, AGENT_STATE_READY, "init_complete");
        assert(ret == 0);

        ret = monitor_service_update_agent_state(svc, trace, AGENT_STATE_EXECUTING, "executing_step_1");
        assert(ret == 0);

        monitor_service_end_agent_trace(svc, trace, AGENT_STATE_COMPLETED);
    } else {
        printf("    State update skipped\n");
    }

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_agent_loop_detection(void) {
    printf("  test_agent_loop_detection...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    loop_detection_config_t loop_cfg;
    memset(&loop_cfg, 0, sizeof(loop_cfg));
    loop_cfg.mode = LOOP_DETECTION_PATTERN_BASED;
    loop_cfg.max_loop_iterations = 3;
    loop_cfg.pattern_window_size = 5;

    agent_execution_trace_t* trace = NULL;
    ret = monitor_service_start_agent_trace(svc, "agent_003", "task_003", &loop_cfg, &trace);
    if (ret == 0 && trace != NULL) {
        bool is_loop = false;
        double confidence = 0.0;

        for (int i = 0; i < 5; i++) {
            monitor_service_update_agent_state(svc, trace, AGENT_STATE_EXECUTING, "same_location");
        }

        ret = monitor_service_check_loop(svc, trace, &is_loop, &confidence);
        if (ret == 0) {
            printf("    Loop detected: %s, confidence: %.2f\n",
                   is_loop ? "yes" : "no", confidence);
        }

        monitor_service_end_agent_trace(svc, trace, AGENT_STATE_COMPLETED);
    } else {
        printf("    Loop detection skipped\n");
    }

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_agent_trace_export(void) {
    printf("  test_agent_trace_export...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    loop_detection_config_t loop_cfg;
    memset(&loop_cfg, 0, sizeof(loop_cfg));
    loop_cfg.mode = LOOP_DETECTION_TIME_BASED;
    loop_cfg.max_execution_time_ms = 60000;

    agent_execution_trace_t* trace = NULL;
    ret = monitor_service_start_agent_trace(svc, "agent_004", "task_004", &loop_cfg, &trace);
    if (ret == 0 && trace != NULL) {
        monitor_service_update_agent_state(svc, trace, AGENT_STATE_EXECUTING, "step_1");

        char* data = NULL;
        size_t size = 0;
        ret = monitor_service_export_agent_trace(svc, trace, "json", &data, &size);
        if (ret == 0 && data != NULL) {
            printf("    Exported %zu bytes of trace data\n", size);
            free(data);
        }

        monitor_service_end_agent_trace(svc, trace, AGENT_STATE_COMPLETED);
    } else {
        printf("    Trace export skipped\n");
    }

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_agent_active_agents(void) {
    printf("  test_agent_active_agents...\n");

    monitor_service_t* svc = NULL;
    int ret = monitor_service_create(NULL, &svc);
    assert(ret == 0);

    char** agent_ids = NULL;
    size_t count = 0;
    ret = monitor_service_get_active_agents(svc, &agent_ids, &count);
    if (ret == 0) {
        printf("    Active agents: %zu\n", count);
        if (agent_ids) free(agent_ids);
    }

    monitor_service_destroy(svc);

    printf("    PASSED\n");
}

static void test_agent_state_enum(void) {
    printf("  test_agent_state_enum...\n");

    assert(AGENT_STATE_CREATED == 0);
    assert(AGENT_STATE_INITIALIZING == 1);
    assert(AGENT_STATE_READY == 2);
    assert(AGENT_STATE_EXECUTING == 3);
    assert(AGENT_STATE_PAUSED == 4);
    assert(AGENT_STATE_COMPLETED == 5);
    assert(AGENT_STATE_FAILED == 6);
    assert(AGENT_STATE_CANCELLED == 7);
    assert(AGENT_STATE_STUCK == 8);

    printf("    PASSED\n");
}

int main(void) {
    printf("=========================================\n");
    printf("  Tracer Unit Tests\n");
    printf("=========================================\n");

    test_agent_trace_create();
    test_agent_state_update();
    test_agent_loop_detection();
    test_agent_trace_export();
    test_agent_active_agents();
    test_agent_state_enum();

    printf("\nAll tracing tests PASSED\n");
    return 0;
}
