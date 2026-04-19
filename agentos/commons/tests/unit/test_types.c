/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file test_types.c
 * @brief 统一类型定义模块单元测试
 *
 * @details
 * 测试 types.h 中所有类型定义的正确性，包括：
 * - 基础类型（错误码、时间戳、UUID、优先级）
 * - 任务类型（状态、配置、结果）
 * - 记忆类型（层级、条目、搜索、结果）
 * - 会话类型（配置、信息、上下文）
 * - Agent 类型（契约、能力、成本、信任）
 * - 可观测性类型（指标、跨度、遥测）
 * - IPC 类型（消息头、消息体、配置）
 * - 网络类型（连接、HTTP 请求/响应）
 * - 辅助宏（ARRAY_SIZE, MIN/MAX, ALIGN_UP 等）
 *
 * @author Spharx AgentOS Team
 * @date 2026-04-02
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <agentos/utils/types/types.h>
#include "../tests/utils/test_framework.h"

/* ============================================================================
 * 基础类型测试
 * ============================================================================ */

/**
 * @brief 测试错误码常量值正确性
 */
static void test_error_codes_valid(void **state) {
    (void)state;

    assert_int_equal(AGENTOS_SUCCESS, 0);
    assert_true(AGENTOS_EINVAL < 0);
    assert_true(AGENTOS_ENOMEM < 0);
    assert_true(AGENTOS_ETIMEDOUT < 0);
    assert_true(AGENTOS_EOVERFLOW < 0);
    assert_true(AGENTOS_EBUSY < 0);
    assert_true(AGENTOS_ENOTCONN < 0);
    assert_true(AGENTOS_ECANCELLED < 0);
}

/**
 * @brief 测试 agentos_result_t 结构体大小和字段
 */
static void test_agentos_result_t_structure(void **state) {
    (void)state;

    agentos_result_t result = {0};
    result.success = true;
    result.error_code = AGENTOS_SUCCESS;
    strncpy(result.error_message, "Test error", sizeof(result.error_message));

    assert_true(result.success);
    assert_int_equal(result.error_code, AGENTOS_SUCCESS);
    assert_string_equal(result.error_message, "Test error");
}

/**
 * @brief 测试优先级枚举值连续性
 */
static void test_priority_enums(void **state) {
    (void)state;

    assert_int_equal(AGENTOS_PRIORITY_LOW, 0);
    assert_int_equal(AGENTOS_PRIORITY_NORMAL, 1);
    assert_int_equal(AGENTOS_PRIORITY_HIGH, 2);
    assert_int_equal(AGENTOS_PRIORITY_CRITICAL, 3);
}

/* ============================================================================
 * 任务类型测试
 * ============================================================================ */

/**
 * @brief 测试任务状态枚举
 */
static void test_task_status_enums(void **state) {
    (void)state;

    assert_int_equal(AGENTOS_TASK_PENDING, 0);
    assert_int_equal(AGENTOS_TASK_RUNNING, 1);
    assert_int_equal(AGENTOS_TASK_COMPLETED, 2);
    assert_int_equal(AGENTOS_TASK_FAILED, 3);
    assert_int_equal(AGENTOS_TASK_CANCELLED, 4);
    assert_int_equal(AGENTOS_TASK_TIMEOUT, 5);
}

/**
 * @brief 测试任务配置结构体初始化
 */
static void test_task_config_init(void **state) {
    (void)state;

    agentos_task_config_t config = {0};

    config.priority = AGENTOS_PRIORITY_NORMAL;
    config.timeout_ms = 5000;
    config.max_retries = 3;
    config.retry_delay_ms = 1000;

    assert_int_equal(config.priority, AGENTOS_PRIORITY_NORMAL);
    assert_int_equal(config.timeout_ms, 5000);
    assert_int_equal(config.max_retries, 3);
    assert_int_equal(config.retry_delay_ms, 1000);
}

/**
 * @brief 测试任务结果结构体
 */
static void test_task_result_structure(void **state) {
    (void)state;

    agentos_task_result_t result = {0};

    result.status = AGENTOS_TASK_COMPLETED;
    result.exit_code = 0;
    result.duration_ms = 1234;

    assert_int_equal(result.status, AGENTOS_TASK_COMPLETED);
    assert_int_equal(result.exit_code, 0);
    assert_int_equal(result.duration_ms, 1234);
}

/* ============================================================================
 * 记忆类型测试
 * ============================================================================ */

/**
 * @brief 测试记忆层级枚举
 */
static void test_memory_layer_enums(void **state) {
    (void)state;

    assert_int_equal(AGENTOS_MEMORY_L1_WORKING, 0);
    assert_int_equal(AGENTOS_MEMORY_L2_EPISODIC, 1);
    assert_int_equal(AGENTOS_MEMORY_L3_SEMANTIC, 2);
    assert_int_equal(AGENTOS_MEMORY_L4_PROCEDURAL, 3);
}

/**
 * @brief 测试记忆条目结构体
 */
static void test_memory_entry_structure(void **state) {
    (void)state;

    agentos_memory_entry_t entry = {0};

    entry.layer = AGENTOS_MEMORY_L2_EPISODIC;
    entry.importance = 0.8f;
    entry.access_count = 10;
    entry.size_bytes = 1024;

    assert_int_equal(entry.layer, AGENTOS_MEMORY_L2_EPISODIC);
    assert_float_within(0.01, 0.8f, entry.importance);
    assert_int_equal(entry.access_count, 10);
    assert_int_equal(entry.size_bytes, 1024);
}

/* ============================================================================
 * 会话类型测试
 * ============================================================================ */

/**
 * @brief 测试会话配置结构体
 */
static void test_session_config_structure(void **state) {
    (void)state;

    agentos_session_config_t config = {0};

    config.session_timeout_ms = 30000;
    config.max_history_size = 100;
    config.enable_persistence = true;

    assert_int_equal(config.session_timeout_ms, 30000);
    assert_int_equal(config.max_history_size, 100);
    assert_true(config.enable_persistence);
}

/**
 * @brief 测试上下文结构体
 */
static void test_context_structure(void **state) {
    (void)state;

    agentos_context_t ctx = {0};

    ctx.agent_id = "agent_001";
    ctx.session_id = "session_abc";
    ctx.current_priority = AGENTOS_PRIORITY_HIGH;

    assert_string_equal(ctx.agent_id, "agent_001");
    assert_string_equal(ctx.session_id, "session_abc");
    assert_int_equal(ctx.current_priority, AGENTOS_PRIORITY_HIGH);
}

/* ============================================================================
 * Agent 类型测试
 * ============================================================================ */

/**
 * @brief 测试 Agent 契约结构体
 */
static void test_agent_contract_structure(void **state) {
    (void)state;

    agentos_agent_contract_t contract = {0};

    contract.agent_type = AGENTOS_AGENT_TYPE_ASSISTANT;
    contract.max_concurrent_tasks = 5;
    contract.memory_limit_mb = 256;

    assert_int_equal(contract.agent_type, AGENTOS_AGENT_TYPE_ASSISTANT);
    assert_int_equal(contract.max_concurrent_tasks, 5);
    assert_int_equal(contract.memory_limit_mb, 256);
}

/**
 * @brief 测试 Agent 能力结构体
 */
static void test_capability_structure(void **state) {
    (void)state;

    agentos_capability_t caps = {0};

    caps.has_tool_use = true;
    caps.has_file_access = false;
    caps.has_network_access = true;
    caps.max_context_tokens = 4096;

    assert_true(caps.has_tool_use);
    assert_false(caps.has_file_access);
    assert_true(caps.has_network_access);
    assert_int_equal(caps.max_context_tokens, 4096);
}

/* ============================================================================
 * 可观测性类型测试
 * ============================================================================ */

/**
 * @brief 测试指标结构体
 */
static void test_metric_structure(void **state) {
    (void)state;

    agentos_metric_t metric = {0};

    metric.name = strdup("test_metric");
    metric.value = 42.5;
    metric.unit = METRIC_UNIT_COUNT;
    metric.metric_type = METRIC_TYPE_GAUGE;

    assert_string_equal(metric.name, "test_metric");
    assert_float_within(0.001, 42.5, metric.value);
    assert_int_equal(metric.unit, METRIC_UNIT_COUNT);
    assert_int_equal(metric.metric_type, METRIC_TYPE_GAUGE);
}

/* ============================================================================
 * IPC 类型测试
 * ============================================================================ */

/**
 * @brief 测试 IPC 消息头结构体
 */
static void test_ipc_header_structure(void **state) {
    (void)state;

    agentos_ipc_header_t header = {0};

    header.magic = 0x49504300;
    header.version = 1;
    header.type = AGENTOS_IPC_PIPE;
    header.payload_len = 1024;
    header.message_id = 12345;

    assert_int_equal(header.magic, 0x49504300);
    assert_int_equal(header.version, 1);
    assert_int_equal(header.type, AGENTOS_IPC_PIPE);
    assert_int_equal(header.payload_len, 1024);
    assert_int_equal(header.message_id, 12345);
}

/**
 * @brief 测试 IPC 配置结构体
 */
static void test_ipc_config_structure(void **state) {
    (void)state;

    agentos_ipc_config_t config = {0};

    config.ipc_type = AGENTOS_IPC_SOCKET;
    config.buffer_size = 8192;
    config.timeout_ms = 5000;
    config.mode = AGENTOS_IPC_MODE_READ_WRITE;

    assert_int_equal(config.ipc_type, AGENTOS_IPC_SOCKET);
    assert_int_equal(config.buffer_size, 8192);
    assert_int_equal(config.timeout_ms, 5000);
    assert_int_equal(config.mode, AGENTOS_IPC_MODE_READ_WRITE);
}

/* ============================================================================
 * 网络类型测试
 * ============================================================================ */

/**
 * @brief 测试连接配置结构体
 */
static void test_conn_config_structure(void **state) {
    (void)state;

    agentos_conn_config_t config = {0};

    config.remote.host = strdup("localhost");
    config.port = 8080;
    config.connect_timeout_ms = 5000;
    config.read_timeout_ms = 10000;
    config.use_ssl = true;

    assert_string_equal(config.host, "localhost");
    assert_int_equal(config.port, 8080);
    assert_int_equal(config.connect_timeout_ms, 5000);
    assert_int_equal(config.read_timeout_ms, 10000);
    assert_true(config.use_ssl);
}

/**
 * @brief 测试 HTTP 请求结构体
 */
static void test_http_request_structure(void **state) {
    (void)state;

    agentos_http_request_t request = {0};

    request.method = strdup("GET");
    request.path = strdup("http://api.example.com/data");
    request.timeout_ms = 5000;

    assert_string_equal(request.method, "GET");
    assert_string_equal(request.url, "http://api.example.com/data");
    assert_int_equal(request.timeout_ms, 5000);
}

/**
 * @brief 测试 HTTP 响应结构体
 */
static void test_http_response_structure(void **state) {
    (void)state;

    agentos_http_response_t response = {0};

    response.status_code = 200;
    response.body = strdup("{\"status\":\"ok\"}");
    response.body_length = strlen(response.body);

    assert_int_equal(response.status_code, 200);
    assert_string_equal(response.body, "{\"status\":\"ok\"}");
    assert_int_equal(response.body_length, 15);
}

/* ============================================================================
 * 辅助宏测试
 * ============================================================================ */

/**
 * @brief 测试 AGENTOS_ARRAY_SIZE 宏
 */
static void test_macro_array_size(void **state) {
    (void)state;

    int array[] = {1, 2, 3, 4, 5};
    assert_int_equal(AGENTOS_ARRAY_SIZE(array), 5);
}

/**
 * @brief 测试 AGENTOS_MIN 宏
 */
static void test_macro_min(void **state) {
    (void)state;

    assert_int_equal(AGENTOS_MIN(3, 7), 3);
    assert_int_equal(AGENTOS_MIN(-1, 5), -1);
    assert_int_equal(AGENTOS_MIN(100, 100), 100);
}

/**
 * @brief 测试 AGENTOS_MAX 宏
 */
static void test_macro_max(void **state) {
    (void)state;

    assert_int_equal(AGENTOS_MAX(3, 7), 7);
    assert_int_equal(AGENTOS_MAX(-1, 5), 5);
    assert_int_equal(AGENTOS_MAX(100, 100), 100);
}

/**
 * @brief 测试 AGENTOS_ALIGN_UP 宏
 */
static void test_macro_align_up(void **state) {
    (void)state;

    assert_int_equal(AGENTOS_ALIGN_UP(0, 16), 0);
    assert_int_equal(AGENTOS_ALIGN_UP(15, 16), 16);
    assert_int_equal(AGENTOS_ALIGN_UP(16, 16), 16);
    assert_int_equal(AGENTOS_ALIGN_UP(17, 16), 32);
    assert_int_equal(AGENTOS_ALIGN_UP(31, 16), 32);
}

/**
 * @brief 测试 AGENTOS_VERSION_MAJOR/MINOR/PATCH 宏
 */
static void test_macro_version(void **state) {
    (void)state;

    assert_int_equal(AGENTOS_VERSION_MAJOR(0x01020303), 1);
    assert_int_equal(AGENTOS_VERSION_MINOR(0x01020303), 2);
    assert_int_equal(AGENTOS_VERSION_PATCH(0x01020303), 3);
}

/**
 * @brief 测试时间转换宏
 */
static void test_macro_time_conversion(void **state) {
    (void)state;

    uint64_t ns = 1500000000ULL; /* 1.5 秒 */

    assert_int_equal(AGENTOS_NS_TO_MS(ns), 1500);
    assert_int_equal(AGENTOS_MS_TO_NS(1500), ns);
    assert_int_equal(AGENTOS_NS_TO_US(ns), 1500000);
    assert_int_equal(AGENTOS_US_TO_NS(1500000), ns);
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(void) {
    const struct CMUnitTest tests[] = {
        /* 基础类型测试 */
        cmocka_unit_test(test_error_codes_valid),
        cmocka_unit_test(test_agentos_result_t_structure),
        cmocka_unit_test(test_priority_enums),

        /* 任务类型测试 */
        cmocka_unit_test(test_task_status_enums),
        cmocka_unit_test(test_task_config_init),
        cmocka_unit_test(test_task_result_structure),

        /* 记忆类型测试 */
        cmocka_unit_test(test_memory_layer_enums),
        cmocka_unit_test(test_memory_entry_structure),

        /* 会话类型测试 */
        cmocka_unit_test(test_session_config_structure),
        cmocka_unit_test(test_context_structure),

        /* Agent 类型测试 */
        cmocka_unit_test(test_agent_contract_structure),
        cmocka_unit_test(test_capability_structure),

        /* 可观测性类型测试 */
        cmocka_unit_test(test_metric_structure),

        /* IPC 类型测试 */
        cmocka_unit_test(test_ipc_header_structure),
        cmocka_unit_test(test_ipc_config_structure),

        /* 网络类型测试 */
        cmocka_unit_test(test_conn_config_structure),
        cmocka_unit_test(test_http_request_structure),
        cmocka_unit_test(test_http_response_structure),

        /* 辅助宏测试 */
        cmocka_unit_test(test_macro_array_size),
        cmocka_unit_test(test_macro_min),
        cmocka_unit_test(test_macro_max),
        cmocka_unit_test(test_macro_align_up),
        cmocka_unit_test(test_macro_version),
        cmocka_unit_test(test_macro_time_conversion),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
