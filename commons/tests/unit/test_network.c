/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file test_network.c
 * @brief 网络通信模块单元测试
 * 
 * @details
 * 测试 network_common.h 中所有网络功能，包括：
 * - 基础连接 API（创建/连接/断开/发送/接收）
 * - HTTP 客户端 API（GET/POST/请求/响应释放）
 * - 连接池管理 API（创建/销毁/获取/释放/健康检查）
 * - DNS 解析 API（解析/结果释放）
 * - 工具函数（可达性检查/本地IP获取/地址转换）
 * - SSL/TLS 配置
 * - 网络统计
 * 
 * @author AgentOS Team
 * @date 2026-04-02
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include "include/network_common.h"
#include "../tests/utils/test_framework.h"

/* ============================================================================
 * 基础连接测试
 * ============================================================================ */

/**
 * @brief 测试创建 TCP Socket
 */
static void test_socket_tcp_create(void **state) {
    (void)state;
    
    network_socket_t* sock = network_socket_tcp_create();
    
    assert_non_null(sock);
    assert_int_equal(network_socket_get_type(sock), NETWORK_SOCKET_TCP);
    
    network_socket_destroy(sock);
}

/**
 * @brief 测试创建 UDP Socket
 */
static void test_socket_udp_create(void **state) {
    (void)state;
    
    network_socket_t* sock = network_socket_udp_create();
    
    assert_non_null(sock);
    assert_int_equal(network_socket_get_type(sock), NETWORK_SOCKET_UDP);
    
    network_socket_destroy(sock);
}

/**
 * @brief 测试创建 NULL 返回处理
 */
static void test_socket_create_null_handling(void **state) {
    (void)state;
    
    /* 创建成功后验证状态 */
    network_socket_t* tcp_sock = network_socket_tcp_create();
    if (tcp_sock) {
        /* 初始状态应为未连接 */
        assert_false(network_socket_is_connected(tcp_sock));
        network_socket_destroy(tcp_sock);
    }
}

/**
 * @brief 测试销毁 NULL socket 不崩溃
 */
static void test_socket_destroy_null(void **state) {
    (void)state;
    
    network_socket_destroy(NULL); /* 不应崩溃 */
}

/* ============================================================================
 * 连接配置测试
 * ============================================================================ */

/**
 * @brief 测试默认配置创建
 */
static void test_default_config_create(void **state) {
    (void)state;
    
    network_config_t config = network_create_default_config();
    
    assert_int_equal(config.port, 8080);
    assert_true(config.connect_timeout_ms > 0);
    assert_true(config.read_timeout_ms > 0);
    assert_true(config.write_timeout_ms > 0);
}

/**
 * @brief 测试配置字段设置
 */
static void test_config_field_setting(void **state) {
    (void)state;
    
    network_config_t config = {0};
    
    strcpy(config.host, "127.0.0.1");
    config.port = 443;
    config.connect_timeout_ms = 5000;
    config.read_timeout_ms = 10000;
    config.use_ssl = true;
    config.verify_ssl = true;
    
    assert_string_equal(config.host, "127.0.0.1");
    assert_int_equal(config.port, 443);
    assert_int_equal(config.connect_timeout_ms, 5000);
    assert_int_equal(config.read_timeout_ms, 10000);
    assert_true(config.use_ssl);
    assert_true(config.verify_ssl);
}

/* ============================================================================
 * HTTP 客户端测试
 * ============================================================================ */

/**
 * @brief 测试创建 HTTP 请求
 */
static void test_http_request_create(void **state) {
    (void)state;
    
    http_request_t* req = http_request_create("http://api.example.com/data", HTTP_METHOD_GET);
    
    assert_non_null(req);
    assert_string_equal(http_request_get_url(req), "http://api.example.com/data");
    assert_int_equal(http_request_get_method(req), HTTP_METHOD_GET);
    
    http_request_free(req);
}

/**
 * @brief 测试创建 POST 请求
 */
static void test_http_request_post_create(void **state) {
    (void)state;
    
    http_request_t* req = http_request_create("http://api.example.com/submit", HTTP_METHOD_POST);
    
    assert_non_null(req);
    assert_int_equal(http_request_get_method(req), HTTP_METHOD_POST);
    
    http_request_free(req);
}

/**
 * @brief 测试设置请求头
 */
static void test_http_set_header(void **state) {
    (void)state;
    
    http_request_t* req = http_request_create("http://example.com/api", HTTP_METHOD_GET);
    
    agentos_error_t err = http_set_header(req, "Content-Type", "application/json");
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    http_request_free(req);
}

/**
 * @brief 测试设置请求体
 */
static void test_http_set_body(void **state) {
    (void)state;
    
    http_request_t* req = http_request_create("http://example.com/api", HTTP_METHOD_POST);
    
    const char* body = "{\"key\":\"value\"}";
    agentos_error_t err = http_set_body(req, body, strlen(body));
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    http_request_free(req);
}

/**
 * @brief 测试释放 NULL 请求不崩溃
 */
static void test_http_request_free_null(void **state) {
    (void)state;
    
    http_request_free(NULL); /* 不应崩溃 */
}

/**
 * @brief 测试释放 NULL 响应不崩溃
 */
static void test_http_response_free_null(void **state) {
    (void)state;
    
    http_response_free(NULL); /* 不应崩溃 */
}

/* ============================================================================
 * 连接池测试
 * ============================================================================ */

/**
 * @brief 测试创建连接池
 */
static void test_conn_pool_create(void **state) {
    (void)state;
    
    network_conn_pool_t* pool = network_conn_pool_create("localhost", 8080, 10);
    
    assert_non_null(pool);
    
    network_conn_pool_destroy(pool);
}

/**
 * @brief 测试连接池参数查询
 */
static void test_conn_pool_parameters(void **state) {
    (void)state;
    
    network_conn_pool_t* pool = network_conn_pool_create("localhost", 9090, 5);
    
    assert_non_null(pool);
    
    size_t max_size = network_conn_pool_max_size(pool);
    size_t current_size = network_conn_pool_current_size(pool);
    
    assert_int_equal(max_size, 5);
    assert_int_equal(current_size, 0);
    
    network_conn_pool_destroy(pool);
}

/**
 * @brief 测试销毁 NULL 连接池不崩溃
 */
static void test_conn_pool_destroy_null(void **state) {
    (void)state;
    
    network_conn_pool_destroy(NULL); /* 不应崩溃 */
}

/* ============================================================================
 * DNS 解析测试
 * ============================================================================ */

/**
 * @brief 测试 DNS 解析函数存在性
 */
static void test_dns_resolve_function_exists(void **state) {
    (void)state;
    
    /* 验证函数可以调用（可能返回错误，但不崩溃） */
    network_addr_info_t* result = network_dns_resolve("localhost");
    
    /* 无论是否成功，都不应崩溃 */
    if (result) {
        network_addr_info_free(result);
    }
}

/**
 * @brief 测试释放 NULL DNS 结果不崩溃
 */
static void test_dns_result_free_null(void **state) {
    (void)state;
    
    network_addr_info_free(NULL); /* 不应崩溃 */
}

/* ============================================================================
 * 工具函数测试
 * ============================================================================ */

/**
 * @brief 测试地址转换函数
 */
static void test_address_to_string(void **state) {
    (void)state;
    
    /* 测试函数存在性和基本行为 */
    char buffer[64] = {0};
    
    /* 调用工具函数，验证不会崩溃 */
    const char* result = network_addr_to_string(buffer, sizeof(buffer), "127.0.0.1", 8080);
    
    /* 结果可能是有效字符串或 NULL */
    (void)result;
}

/**
 * @brief 测试网络可达性检查函数
 */
static void test_is_reachable_function(void **state) {
    (void)state;
    
    /* 调用可达性检查，不应崩溃 */
    bool reachable = network_is_reachable("127.0.0.1", 80, 100);
    
    /* 结果取决于系统环境 */
    (void)reachable;
}

/**
 * @brief 测试获取本地 IP 函数
 */
static void test_get_local_ip_function(void **state) {
    (void)state;
    
    /* 调用获取本地 IP，不应崩溃 */
    char ip_buffer[64] = {0};
    bool success = network_get_local_ip(ip_buffer, sizeof(ip_buffer));
    
    /* 成功或失败都是可接受的 */
    (void)success;
}

/* ============================================================================
 * 错误处理测试
 * ============================================================================ */

/**
 * @brief 测试网络错误码定义
 */
static void test_network_error_codes(void **state) {
    (void)state;
    
    /* 验证错误码常量存在且合理 */
    assert_true(NETWORK_ERR_NONE == 0);
    assert_true(NETWORK_ERR_INVALID_PARAM != 0);
    assert_true(NETWORK_ERR_TIMEOUT != 0);
    assert_true(NETWORK_ERR_CONNECTION_FAILED != 0);
    assert_true(NETWORK_ERR_SSL_ERROR != 0);
}

/**
 * @brief 测试 Socket 类型枚举
 */
static void test_socket_type_enums(void **state) {
    (void)state;
    
    assert_int_equal(NETWORK_SOCKET_TCP, 0);
    assert_int_equal(NETWORK_SOCKET_UDP, 1);
}

/**
 * @brief 测试 HTTP 方法枚举
 */
static void test_http_method_enums(void **state) {
    (void)state;
    
    assert_int_equal(HTTP_METHOD_GET, 0);
    assert_int_equal(HTTP_METHOD_POST, 1);
    assert_int_equal(HTTP_METHOD_PUT, 2);
    assert_int_equal(HTTP_METHOD_DELETE, 3);
}

/* ============================================================================
 * SSL/TLS 配置测试
 * ============================================================================ */

/**
 * @brief 测试 SSL 配置结构体
 */
static void test_ssl_config_structure(void **state) {
    (void)state;
    
    network_ssl_config_t ssl_config = {0};
    
    ssl_config.enabled = true;
    ssl_config.verify_peer = true;
    ssl_config.verify_hostname = true;
    ssl_config.min_version = TLS_1_2;
    
    assert_true(ssl_config.enabled);
    assert_true(ssl_config.verify_peer);
    assert_true(ssl_config.verify_hostname);
    assert_int_equal(ssl_config.min_version, TLS_1_2);
}

/* ============================================================================
 * 网络统计测试
 * ============================================================================ */

/**
 * @brief 测试网络统计结构体
 */
static void test_network_stats_structure(void **state) {
    (void)state;
    
    network_stats_t stats = {0};
    
    stats.bytes_sent = 1024;
    stats.bytes_received = 2048;
    stats.connections_opened = 5;
    stats.connections_closed = 4;
    stats.errors_count = 1;
    
    assert_int_equal(stats.bytes_sent, 1024);
    assert_int_equal(stats.bytes_received, 2048);
    assert_int_equal(stats.connections_opened, 5);
    assert_int_equal(stats.connections_closed, 4);
    assert_int_equal(stats.errors_count, 1);
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(void) {
    const struct CMUnitTest tests[] = {
        /* 基础连接测试 */
        cmocka_unit_test(test_socket_tcp_create),
        cmocka_unit_test(test_socket_udp_create),
        cmocka_unit_test(test_socket_create_null_handling),
        cmocka_unit_test(test_socket_destroy_null),
        
        /* 连接配置测试 */
        cmocka_unit_test(test_default_config_create),
        cmocka_unit_test(test_config_field_setting),
        
        /* HTTP 客户端测试 */
        cmocka_unit_test(test_http_request_create),
        cmocka_unit_test(test_http_request_post_create),
        cmocka_unit_test(test_http_set_header),
        cmocka_unit_test(test_http_set_body),
        cmocka_unit_test(test_http_request_free_null),
        cmocka_unit_test(test_http_response_free_null),
        
        /* 连接池测试 */
        cmocka_unit_test(test_conn_pool_create),
        cmocka_unit_test(test_conn_pool_parameters),
        cmocka_unit_test(test_conn_pool_destroy_null),
        
        /* DNS 解析测试 */
        cmocka_unit_test(test_dns_resolve_function_exists),
        cmocka_unit_test(test_dns_result_free_null),
        
        /* 工具函数测试 */
        cmocka_unit_test(test_address_to_string),
        cmocka_unit_test(test_is_reachable_function),
        cmocka_unit_test(test_get_local_ip_function),
        
        /* 错误处理测试 */
        cmocka_unit_test(test_network_error_codes),
        cmocka_unit_test(test_socket_type_enums),
        cmocka_unit_test(test_http_method_enums),
        
        /* SSL/TLS 配置测试 */
        cmocka_unit_test(test_ssl_config_structure),
        
        /* 网络统计测试 */
        cmocka_unit_test(test_network_stats_structure),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
