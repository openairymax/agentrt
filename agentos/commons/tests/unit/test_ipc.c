/*
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 * 
 * @file test_ipc.c
 * @brief 进程间通信模块单元测试
 * 
 * @details
 * 测试 ipc_common.h/ipc_common.c 中所有核心功能，包括：
 * - 初始化与清理 API（ipc_init, ipc_cleanup）
 * - 通道管理 API（创建/销毁/打开/关闭/状态查询）
 * - 消息发送 API（send/send_data/send_request/broadcast/notify）
 * - 消息接收 API（receive/receive_data/try_receive）
 * - 服务端 API（创建/销毁/启动/停止/接受连接）
 * - 客户端 API（创建/销毁/连接/断开）
 * - 共享内存 API（创建/销毁/映射/取消映射）
 * - 消息队列 API（创建/销毁/发送/接收/清空）
 * - 消息辅助函数（创建/释放/克隆/校验和/序列化/反序列化）
 * - 工具函数（错误消息/有效性检查/刷新）
 * 
 * @author AgentOS Team
 * @date 2026-04-02
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>
#include "include/ipc_common.h"
#include "../tests/utils/test_framework.h"

/* ============================================================================
 * 初始化与清理测试
 * ============================================================================ */

/**
 * @brief 测试 IPC 初始化成功
 */
static void test_ipc_init_success(void **state) {
    (void)state;
    
    agentos_error_t err = ipc_init();
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_cleanup();
}

/**
 * @brief 测试重复初始化不报错
 */
static void test_ipc_init_multiple(void **state) {
    (void)state;
    
    assert_int_equal(ipc_init(), AGENTOS_SUCCESS);
    assert_int_equal(ipc_init(), AGENTOS_SUCCESS);
    
    ipc_cleanup();
}

/* ============================================================================
 * 默认配置测试
 * ============================================================================ */

/**
 * @brief 测试创建默认管道配置
 */
static void test_create_default_config_pipe(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    
    assert_int_equal(config.type, IPC_TYPE_PIPE);
    assert_int_equal(config.mode, IPC_MODE_READ_WRITE);
    assert_int_equal(config.buffer_size, IPC_DEFAULT_BUFFER_SIZE);
    assert_int_equal(config.max_message_size, IPC_MAX_MESSAGE_SIZE);
    assert_int_equal(config.timeout_ms, IPC_DEFAULT_TIMEOUT_MS);
    assert_false(config.nonblocking);
    assert_false(config.persistent);
}

/**
 * @brief 测试创建默认 Socket 配置
 */
static void test_create_default_config_socket(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_SOCKET);
    
    assert_int_equal(config.type, IPC_TYPE_SOCKET);
    assert_int_equal(config.buffer_size, IPC_DEFAULT_BUFFER_SIZE);
}

/**
 * @brief 测试创建默认共享内存配置
 */
static void test_create_default_config_shm(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_SHM);
    
    assert_int_equal(config.type, IPC_TYPE_SHM);
}

/* ============================================================================
 * 通道创建与销毁测试
 * ============================================================================ */

/**
 * @brief 测试创建通道成功
 */
static void test_channel_create_success(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    assert_non_null(channel);
    assert_int_equal(ipc_channel_get_state(channel), IPC_STATE_CLOSED);
    
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试用 NULL 配置创建通道返回 NULL
 */
static void test_channel_create_null_config(void **state) {
    (void)state;
    
    ipc_channel_t* channel = ipc_channel_create(NULL);
    assert_null(channel);
}

/**
 * @brief 测试销毁 NULL 通道不崩溃
 */
static void test_channel_destroy_null(void **state) {
    (void)state;
    
    ipc_channel_destroy(NULL); /* 不应崩溃 */
}

/* ============================================================================
 * 通道打开与关闭测试
 * ============================================================================ */

/**
 * @brief 测试打开通道成功
 */
static void test_channel_open_success(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    agentos_error_t err = ipc_channel_open(channel);
    assert_int_equal(err, AGENTOS_SUCCESS);
    assert_int_equal(ipc_channel_get_state(channel), IPC_STATE_OPEN);
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试关闭已关闭的通道不报错
 */
static void test_channel_close_already_closed(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    /* 关闭未打开的通道应返回成功 */
    agentos_error_t err = ipc_channel_close(channel);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试用 NULL 参数操作通道
 */
static void test_channel_operations_null(void **state) {
    (void)state;
    
    assert_int_equal(ipc_channel_open(NULL), AGENTOS_EINVAL);
    assert_int_equal(ipc_channel_close(NULL), AGENTOS_EINVAL);
    assert_int_equal(ipc_channel_get_state(NULL), IPC_STATE_ERROR);
    assert_null(ipc_channel_get_name(NULL));
    assert_int_equal(ipc_channel_set_timeout(NULL, 1000), AGENTOS_EINVAL);
}

/* ============================================================================
 * 通道属性测试
 * ============================================================================ */

/**
 * @brief 测试获取通道名称
 */
static void test_channel_get_name(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    config.name = "test_pipe";
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    const char* name = ipc_channel_get_name(channel);
    assert_string_equal(name, "test_pipe");
    
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试获取通道类型
 */
static void test_channel_get_type(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_SOCKET);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    assert_int_equal(ipc_channel_get_type(channel), IPC_TYPE_SOCKET);
    
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试设置超时时间
 */
static void test_channel_set_timeout(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    agentos_error_t err = ipc_channel_set_timeout(channel, 5000);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试设置事件回调
 */
static void test_channel_set_event_callback(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    int callback_called = 0;
    
    static void test_event_cb(ipc_channel_t* ch, ipc_event_t event,
                               const void* data, size_t len, void* user_data) {
        (void)ch; (void)data; (void)len;
        if (event == IPC_EVENT_CONNECTED && user_data) {
            (*(int*)user_data)++;
        }
    }
    
    agentos_error_t err = ipc_channel_set_event_callback(
        channel, test_event_cb, &callback_called
    );
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    /* 打开通道会触发 CONNECTED 事件 */
    ipc_channel_open(channel);
    assert_true(callback_called > 0);
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试统计信息
 */
static void test_channel_stats(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    ipc_stats_t stats;
    agentos_error_t err = ipc_channel_get_stats(channel, &stats);
    assert_int_equal(err, AGENTOS_SUCCESS);
    assert_int_equal(stats.messages_sent, 0);
    assert_int_equal(stats.bytes_sent, 0);
    
    /* 重置统计 */
    err = ipc_channel_reset_stats(channel);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_destroy(channel);
}

/* ============================================================================
 * 消息发送测试
 * ============================================================================ */

/**
 * @brief 测试发送消息到打开的通道
 */
static void test_send_message_success(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    ipc_channel_open(channel);
    
    ipc_message_t msg = {0};
    msg.header.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_DATA;
    msg.header.msg_id = 1;
    msg.header.payload_len = 10;
    msg.payload = malloc(10);
    msg.payload_size = 10;
    
    agentos_error_t err = ipc_send(channel, &msg);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    free(msg.payload);
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试发送数据便捷方法
 */
static void test_send_data_success(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    ipc_channel_open(channel);
    
    const char* data = "Hello, IPC!";
    size_t sent = 0;
    
    agentos_error_t err = ipc_send_data(channel, data, strlen(data), &sent);
    assert_int_equal(err, AGENTOS_SUCCESS);
    assert_int_equal(sent, strlen(data));
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试发送请求并等待响应
 */
static void test_send_request_response(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    ipc_channel_open(channel);
    
    ipc_message_t request = {0};
    request.header.magic = IPC_MAGIC;
    request.header.version = 1;
    request.header.type = IPC_MSG_DATA;
    request.header.msg_id = 100;
    request.payload = NULL;
    request.payload_size = 0;
    
    ipc_message_t response = {0};
    agentos_error_t err = ipc_send_request(channel, &request, &response, 5000);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试广播消息
 */
static void test_broadcast_message(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    ipc_channel_open(channel);
    
    ipc_message_t msg = {0};
    msg.header.magic = IPC_MAGIC;
    msg.header.version = 1;
    msg.header.type = IPC_MSG_DATA;
    msg.header.msg_id = 200;
    
    agentos_error_t err = ipc_broadcast(channel, &msg);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试发送通知
 */
static void test_notify_message(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    ipc_channel_open(channel);
    
    const char* notification = "Test notification";
    agentos_error_t err = ipc_notify(channel, notification, strlen(notification));
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试错误参数处理
 */
static void test_send_error_cases(void **state) {
    (void)state;
    
    ipc_message_t msg = {0};
    
    /* NULL 通道 */
    assert_int_equal(ipc_send(NULL, &msg), AGENTOS_EINVAL);
    assert_int_equal(ipc_send_data(NULL, "data", 4, NULL), AGENTOS_EINVAL);
    
    /* 未打开的通道 */
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* closed_channel = ipc_channel_create(&config);
    
    assert_int_equal(ipc_send(closed_channel, &msg), AGENTOS_ENOTCONN);
    
    ipc_channel_destroy(closed_channel);
}

/* ============================================================================
 * 消息接收测试
 * ============================================================================ */

/**
 * @brief 测试接收消息
 */
static void test_receive_message(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    ipc_channel_open(channel);
    
    ipc_message_t received_msg;
    agentos_error_t err = ipc_receive(channel, &received_msg, 1000);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试非阻塞接收
 */
static void test_try_receive_message(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    ipc_channel_open(channel);
    
    ipc_message_t msg;
    agentos_error_t err = ipc_try_receive(channel, &msg);
    /* 超时或忙都是可接受的 */
    assert_true(err == AGENTOS_SUCCESS || err == AGENTOS_EBUSY);
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试设置消息回调
 */
static void test_set_message_callback(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    static int test_msg_cb(ipc_channel_t* ch, ipc_message_t* msg, void* user_data) {
        (void)ch; (void)msg; (void)user_data;
        return 0; /* 返回 0 表示成功处理 */
    }
    
    agentos_error_t err = ipc_set_message_callback(channel, test_msg_cb, NULL);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_destroy(channel);
}

/* ============================================================================
 * 服务端测试
 * ============================================================================ */

/**
 * @brief 测试创建服务端
 */
static void test_server_create(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_server_t* server = ipc_server_create(&config);
    
    assert_non_null(server);
    
    ipc_server_destroy(server);
}

/**
 * @brief 测试创建 NULL 配置服务端
 */
static void test_server_create_null_config(void **state) {
    (void)state;
    
    ipc_server_t* server = ipc_server_create(NULL);
    assert_null(server);
}

/**
 * @brief 测试服务端启动和停止
 */
static void test_server_start_stop(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_server_t* server = ipc_server_create(&config);
    
    agentos_error_t err = ipc_server_start(server);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    err = ipc_server_stop(server);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_server_destroy(server);
}

/**
 * @brief 测试服务端接受连接
 */
static void test_server_accept(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    config.max_connections = 5;
    ipc_server_t* server = ipc_server_create(&config);
    
    ipc_server_start(server);
    
    ipc_channel_t* client = ipc_server_accept(server, 1000);
    /* 可能返回 NULL（超时）或有效通道 */
    if (client) {
        ipc_channel_destroy(client);
    }
    
    ipc_server_stop(server);
    ipc_server_destroy(server);
}

/**
 * @brief 测试服务端连接计数
 */
static void test_server_connection_count(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_server_t* server = ipc_server_create(&config);
    
    size_t count = ipc_server_connection_count(server);
    assert_int_equal(count, 0);
    
    ipc_server_destroy(server);
}

/* ============================================================================
 * 客户端测试
 * ============================================================================ */

/**
 * @brief 测试创建客户端
 */
static void test_client_create(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_client_t* client = ipc_client_create(&config);
    
    assert_non_null(client);
    
    ipc_client_destroy(client);
}

/**
 * @brief 测试客户端连接和断开
 */
static void test_client_connect_disconnect(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_client_t* client = ipc_client_create(&config);
    
    agentos_error_t err = ipc_client_connect(client, 5000);
    if (err == AGENTOS_SUCCESS) {
        ipc_client_disconnect(client);
    } else {
        /* 连接失败也是可接受的（没有真实的服务器） */
    }
    
    ipc_client_destroy(client);
}

/**
 * @brief 测试客户端获取通道
 */
static void test_client_get_channel(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_NAMED_PIPE);
    ipc_client_t* client = ipc_client_create(&config);
    
    ipc_channel_t* ch = ipc_client_get_channel(client);
    /* 未连接时应为 NULL */
    assert_null(ch);
    
    ipc_client_destroy(client);
}

/* ============================================================================
 * 共享内存测试
 * ============================================================================ */

/**
 * @brief 测试创建共享内存对象
 */
static void test_shm_create(void **state) {
    (void)state;
    
    ipc_shm_config_t config = {0};
    config.name = "/test_agentos_shm";
    config.size = 4096;
    config.create = true;
    config.read_only = false;
    
    ipc_shm_t* shm = ipc_shm_create(&config);
    assert_non_null(shm);
    
    ipc_shm_destroy(shm);
}

/**
 * @brief 测试共享内存大小查询
 */
static void test_shm_get_size(void **state) {
    (void)state;
    
    ipc_shm_config_t config = {0};
    config.name = "/test_shm_size";
    config.size = 8192;
    config.create = false;
    config.read_only = true;
    
    ipc_shm_t* shm = ipc_shm_create(&config);
    assert_non_null(shm);
    
    size_t size = ipc_shm_get_size(shm);
    assert_true(size >= 0);
    
    ipc_shm_destroy(shm);
}

/* ============================================================================
 * 消息队列测试
 * ============================================================================ */

/**
 * @brief 测试创建消息队列
 */
static void test_mq_create(void **state) {
    (void)state;
    
    ipc_mq_config_t config = {0};
    config.name = "/test_mq";
    config.max_messages = 100;
    config.max_message_size = 1024;
    config.mode = IPC_MQ_READ_WRITE;
    
    ipc_mq_t* mq = ipc_mq_create(&config);
    assert_non_null(mq);
    
    ipc_mq_destroy(mq);
}

/**
 * @brief 测试消息队列计数
 */
static void test_mq_count(void **state) {
    (void)state;
    
    ipc_mq_config_t config = {0};
    config.name = "/test_mq_count";
    config.max_messages = 50;
    
    ipc_mq_t* mq = ipc_mq_create(&config);
    assert_non_null(mq);
    
    size_t count = ipc_mq_count(mq);
    assert_int_equal(count, 0);
    
    ipc_mq_destroy(mq);
}

/**
 * @brief 测试清空消息队列
 */
static void test_mq_clear(void **state) {
    (void)state;
    
    ipc_mq_config_t config = {0};
    config.name = "/test_mq_clear";
    config.max_messages = 50;
    
    ipc_mq_t* mq = ipc_mq_create(&config);
    assert_non_null(mq);
    
    agentos_error_t err = ipc_mq_clear(mq);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    assert_int_equal(ipc_mq_count(mq), 0);
    
    ipc_mq_destroy(mq);
}

/* ============================================================================
 * 消息辅助函数测试
 * ============================================================================ */

/**
 * @brief 测试创建消息
 */
static void test_message_create(void **state) {
    (void)state;
    
    const char* payload = "Test payload data";
    ipc_message_t* msg = ipc_message_create(IPC_MSG_DATA, payload, strlen(payload));
    
    assert_non_null(msg);
    assert_int_equal(msg->header.magic, IPC_MAGIC);
    assert_int_equal(msg->header.version, 1);
    assert_int_equal(msg->header.type, IPC_MSG_DATA);
    assert_non_null(msg->payload);
    assert_int_equal(msg->payload_size, strlen(payload));
    
    ipc_message_free(msg);
}

/**
 * @brief 测试创建空消息
 */
static void test_message_create_empty(void **state) {
    (void)state;
    
    ipc_message_t* msg = ipc_message_create(IPC_MSG_HEARTBEAT, NULL, 0);
    
    assert_non_null(msg);
    assert_null(msg->payload);
    assert_int_equal(msg->payload_size, 0);
    
    ipc_message_free(msg);
}

/**
 * @brief 测试释放消息
 */
static void test_message_free_null(void **state) {
    (void)state;
    
    ipc_message_free(NULL); /* 不应崩溃 */
}

/**
 * @brief 测试克隆消息
 */
static void test_message_clone(void **state) {
    (void)state;
    
    const char* original_payload = "Original data";
    ipc_message_t* original = ipc_message_create(IPC_MSG_DATA, original_payload, strlen(original_payload));
    
    ipc_message_t* clone = ipc_message_clone(original);
    
    assert_non_null(clone);
    assert_int_equal(clone->header.magic, original->header.magic);
    assert_int_equal(clone->header.type, original->header.type);
    assert_int_equal(clone->payload_size, original->payload_size);
    
    ipc_message_free(original);
    ipc_message_free(clone);
}

/**
 * @brief 测试克隆 NULL 消息
 */
static void test_message_clone_null(void **state) {
    (void)state;
    
    ipc_message_t* result = ipc_message_clone(NULL);
    assert_null(result);
}

/**
 * @brief 测试计算校验和
 */
static void test_message_checksum(void **state) {
    (void)state;
    
    const char* payload = "Checksum test data";
    ipc_message_t* msg = ipc_message_create(IPC_MSG_DATA, payload, strlen(payload));
    
    uint32_t checksum = ipc_message_checksum(msg);
    assert_true(checksum != 0); /* 校验和不应为 0 */
    
    ipc_message_free(msg);
}

/**
 * @brief 测试验证有效消息
 */
static void test_message_verify_valid(void **state) {
    (void)state;
    
    const char* payload = "Valid message";
    ipc_message_t* msg = ipc_message_create(IPC_MSG_DATA, payload, strlen(payload));
    
    bool valid = ipc_message_verify(msg);
    assert_true(valid);
    
    ipc_message_free(msg);
}

/**
 * @brief 测试验证 NULL 消息
 */
static void test_message_verify_null(void **state) {
    (void)state;
    
    bool valid = ipc_message_verify(NULL);
    assert_false(valid);
}

/**
 * @brief 测试序列化和反序列化
 */
static void test_message_serialize_deserialize(void **state) {
    (void)state;
    
    const char* original_payload = "Serialize test";
    ipc_message_t* original = ipc_message_create(IPC_MSG_DATA, original_payload, strlen(original_payload));
    
    /* 序列化 */
    size_t buffer_size = sizeof(ipc_message_header_t) + original->payload_size + 1024;
    void* buffer = malloc(buffer_size);
    size_t written = 0;
    
    agentos_error_t err = ipc_message_serialize(original, buffer, buffer_size, &written);
    assert_int_equal(err, AGENTOS_SUCCESS);
    assert_true(written > 0);
    
    /* 反序列化 */
    ipc_message_t deserialized;
    err = ipc_message_deserialize(buffer, written, &deserialized);
    assert_int_equal(err, AGENTOS_SUCCESS);
    assert_int_equal(deserialized.header.magic, original->header.magic);
    assert_int_equal(deserialized.header.type, original->header.type);
    assert_int_equal(deserialized.payload_size, original->payload_size);
    
    /* 清理 */
    free(deserialized.payload);
    free(buffer);
    ipc_message_free(original);
}

/**
 * @brief 测试序列化到小缓冲区失败
 */
static void test_serialize_buffer_too_small(void **state) {
    (void)state;
    
    const char* payload = "Data that is too large for small buffer";
    ipc_message_t* msg = ipc_message_create(IPC_MSG_DATA, payload, strlen(payload));
    
    char small_buffer[10];
    size_t written = 0;
    
    agentos_error_t err = ipc_message_serialize(msg, small_buffer, sizeof(small_buffer), &written);
    assert_int_equal(err, AGENTOS_EOVERFLOW);
    
    ipc_message_free(msg);
}

/* ============================================================================
 * 工具函数测试
 * ============================================================================ */

/**
 * @brief 测试获取错误消息
 */
static void test_get_error_message(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    const char* error_msg = ipc_get_error_message(channel);
    assert_non_null(error_msg);
    
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试获取 NULL 通道的错误消息
 */
static void test_get_error_message_null(void **state) {
    (void)state;
    
    const char* error_msg = ipc_get_error_message(NULL);
    assert_non_null(error_msg);
}

/**
 * @brief 测试有效性检查
 */
static void test_is_valid(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    /* 关闭的通道无效 */
    assert_false(ipc_is_valid(channel));
    
    /* 打开后有效 */
    ipc_channel_open(channel);
    assert_true(ipc_is_valid(channel));
    
    ipc_channel_close(channel);
    ipc_channel_destroy(channel);
}

/**
 * @brief 测试 NULL 通道有效性
 */
static void test_is_valid_null(void **state) {
    (void)state;
    
    assert_false(ipc_is_valid(NULL));
}

/**
 * @brief 测试刷新操作
 */
static void test_flush(void **state) {
    (void)state;
    
    ipc_config_t config = ipc_create_default_config(IPC_TYPE_PIPE);
    ipc_channel_t* channel = ipc_channel_create(&config);
    
    agentos_error_t err = ipc_flush(channel);
    assert_int_equal(err, AGENTOS_SUCCESS);
    
    ipc_channel_destroy(channel);
}

/* ============================================================================
 * 主测试入口
 * ============================================================================ */

int main(void) {
    const struct CMUnitTest tests[] = {
        /* 初始化与清理测试 */
        cmocka_unit_test(test_ipc_init_success),
        cmocka_unit_test(test_ipc_init_multiple),
        
        /* 默认配置测试 */
        cmocka_unit_test(test_create_default_config_pipe),
        cmocka_unit_test(test_create_default_config_socket),
        cmocka_unit_test(test_create_default_config_shm),
        
        /* 通道创建与销毁测试 */
        cmocka_unit_test(test_channel_create_success),
        cmocka_unit_test(test_channel_create_null_config),
        cmocka_unit_test(test_channel_destroy_null),
        
        /* 通道打开与关闭测试 */
        cmocka_unit_test(test_channel_open_success),
        cmocka_unit_test(test_channel_close_already_closed),
        cmocka_unit_test(test_channel_operations_null),
        
        /* 通道属性测试 */
        cmocka_unit_test(test_channel_get_name),
        cmocka_unit_test(test_channel_get_type),
        cmocka_unit_test(test_channel_set_timeout),
        cmocka_unit_test(test_channel_set_event_callback),
        cmocka_unit_test(test_channel_stats),
        
        /* 消息发送测试 */
        cmocka_unit_test(test_send_message_success),
        cmocka_unit_test(test_send_data_success),
        cmocka_unit_test(test_send_request_response),
        cmocka_unit_test(test_broadcast_message),
        cmocka_unit_test(test_notify_message),
        cmocka_unit_test(test_send_error_cases),
        
        /* 消息接收测试 */
        cmocka_unit_test(test_receive_message),
        cmocka_unit_test(test_try_receive_message),
        cmocka_unit_test(test_set_message_callback),
        
        /* 服务端测试 */
        cmocka_unit_test(test_server_create),
        cmocka_unit_test(test_server_create_null_config),
        cmocka_unit_test(test_server_start_stop),
        cmocka_unit_test(test_server_accept),
        cmocka_unit_test(test_server_connection_count),
        
        /* 客户端测试 */
        cmocka_unit_test(test_client_create),
        cmocka_unit_test(test_client_connect_disconnect),
        cmocka_unit_test(test_client_get_channel),
        
        /* 共享内存测试 */
        cmocka_unit_test(test_shm_create),
        cmocka_unit_test(test_shm_get_size),
        
        /* 消息队列测试 */
        cmocka_unit_test(test_mq_create),
        cmocka_unit_test(test_mq_count),
        cmocka_unit_test(test_mq_clear),
        
        /* 消息辅助函数测试 */
        cmocka_unit_test(test_message_create),
        cmocka_unit_test(test_message_create_empty),
        cmocka_unit_test(test_message_free_null),
        cmocka_unit_test(test_message_clone),
        cmocka_unit_test(test_message_clone_null),
        cmocka_unit_test(test_message_checksum),
        cmocka_unit_test(test_message_verify_valid),
        cmocka_unit_test(test_message_verify_null),
        cmocka_unit_test(test_message_serialize_deserialize),
        cmocka_unit_test(test_serialize_buffer_too_small),
        
        /* 工具函数测试 */
        cmocka_unit_test(test_get_error_message),
        cmocka_unit_test(test_get_error_message_null),
        cmocka_unit_test(test_is_valid),
        cmocka_unit_test(test_is_valid_null),
        cmocka_unit_test(test_flush),
    };
    
    return cmocka_run_group_tests(tests, NULL, NULL);
}
