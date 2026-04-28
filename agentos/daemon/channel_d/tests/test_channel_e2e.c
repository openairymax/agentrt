/**
 * @file test_channel_e2e.c
 * @brief channel_d 统一通道服务端到端通信测试 (P3-B01)
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 验收标准: 往返延迟 < 10ms, 全部协议路由方法可用
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#ifdef USE_CMOCKA_STUB
#include "cmocka_stub.h"
#else
#include <cmocka.h>
#endif

#include "channel_service.h"

#define TEST_SOCKET_DIR "/tmp/agentos/channel_e2e_test"
#define TEST_CHANNEL_ID  "e2e_test_ch_001"
#define TEST_CHANNEL_NAME "E2E-TestChannel"
#define TEST_DATA_PAYLOAD "Hello from E2E test! This is a payload for latency measurement."
#define LATENCY_ITERATIONS 100

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void cleanup_test_dir(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s && rm -rf %s", TEST_SOCKET_DIR, TEST_SOCKET_DIR);
    system(cmd);
}

/* ========================================================================
 * 测试组1: 服务生命周期
 * ======================================================================== */

static void test_lifecycle_create_destroy(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_non_null(svc);

    bool healthy = channel_service_is_healthy(svc);
    assert_true(healthy);

    channel_service_destroy(svc);
}

static void test_lifecycle_start_stop(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    strncpy(config.socket_dir, TEST_SOCKET_DIR, sizeof(config.socket_dir) - 1);
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_non_null(svc);

    int rc = channel_service_start(svc);
    assert_int_equal(rc, 0);

    rc = channel_service_stop(svc);
    assert_int_equal(rc, 0);

    channel_service_destroy(svc);
    cleanup_test_dir();
}

static void test_lifecycle_double_start(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    strncpy(config.socket_dir, TEST_SOCKET_DIR, sizeof(config.socket_dir) - 1);

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);
    assert_int_equal(channel_service_start(svc), 0);

    channel_service_stop(svc);
    channel_service_destroy(svc);
    cleanup_test_dir();
}

/* ========================================================================
 * 测试组2: 健康检查
 * ======================================================================== */

static void test_health_check(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;

    channel_service_t* svc = channel_service_create(&config);
    assert_true(channel_service_is_healthy(svc));

    assert_false(channel_service_is_healthy(NULL));

    channel_service_destroy(svc);
}

/* ========================================================================
 * 测试组3: 通道打开/关闭/列表
 * ======================================================================== */

static void test_channel_open_close_socket(void **state) {
    (void)state;
    cleanup_test_dir();
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    strncpy(config.socket_dir, TEST_SOCKET_DIR, sizeof(config.socket_dir) - 1);
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    int rc = channel_service_open(svc, TEST_CHANNEL_ID,
                                  TEST_CHANNEL_NAME,
                                  CHANNEL_TYPE_SOCKET, NULL);
    assert_int_equal(rc, 0);

    channel_info_t info;
    rc = channel_service_get_info(svc, TEST_CHANNEL_ID, &info);
    assert_int_equal(rc, 0);
    assert_string_equal(info.channel_id, TEST_CHANNEL_ID);
    assert_string_equal(info.name, TEST_CHANNEL_NAME);
    assert_int_equal(info.type, CHANNEL_TYPE_SOCKET);
    assert_int_equal(info.status, CHANNEL_STATUS_OPEN);

    size_t count = 0;
    channel_info_t list[16];
    rc = channel_service_list(svc, list, 16, &count);
    assert_int_equal(rc, 0);
    assert_int_equal(count, 1);

    rc = channel_service_close(svc, TEST_CHANNEL_ID);
    assert_int_equal(rc, 0);

    count = 0;
    rc = channel_service_list(svc, list, 16, &count);
    assert_int_equal(rc, 0);
    assert_int_equal(count, 0);

    channel_service_stop(svc);
    channel_service_destroy(svc);
    cleanup_test_dir();
}

static void test_channel_open_close_shm(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    int rc = channel_service_open(svc, "shm_e2e_test",
                                  "SHM-E2E",
                                  CHANNEL_TYPE_SHM, NULL);
    assert_int_equal(rc, 0);

    channel_info_t info;
    rc = channel_service_get_info(svc, "shm_e2e_test", &info);
    assert_int_equal(rc, 0);
    assert_int_equal(info.type, CHANNEL_TYPE_SHM);

    channel_service_close(svc, "shm_e2e_test");
    channel_service_stop(svc);
    channel_service_destroy(svc);
}

static void test_channel_duplicate_id(void **state) {
    (void)state;
    cleanup_test_dir();
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    strncpy(config.socket_dir, TEST_SOCKET_DIR, sizeof(config.socket_dir) - 1);
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    assert_int_equal(channel_service_open(svc, "dup_id", "Ch1",
                                          CHANNEL_TYPE_SOCKET, NULL), 0);
    int rc2 = channel_service_open(svc, "dup_id", "Ch2",
                                   CHANNEL_TYPE_SOCKET, NULL);
    assert_true(rc2 != 0);

    channel_service_close(svc, "dup_id");
    channel_service_stop(svc);
    channel_service_destroy(svc);
    cleanup_test_dir();
}

/* ========================================================================
 * 测试组4: 发送/接收 + 延迟测量 (核心验收标准: <10ms)
 * ======================================================================== */

static void test_send_receive_socket_roundtrip(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    strncpy(config.socket_dir, TEST_SOCKET_DIR, sizeof(config.socket_dir) - 1);
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    const char* ch_id = "rt_sock";
    assert_int_equal(channel_service_open(svc, ch_id, "RT-Socket",
                                          CHANNEL_TYPE_SOCKET, NULL), 0);

    const char* send_data = TEST_DATA_PAYLOAD;
    size_t send_len = strlen(send_data);

    uint64_t t0 = get_time_ns();
    int rc = channel_service_send(svc, ch_id, send_data, send_len);
    uint64_t t1 = get_time_ns();
    uint64_t send_ns = t1 - t0;

    assert_int_equal(rc, 0);

    void* recv_buf = NULL;
    size_t recv_len = 0;
    uint64_t t2 = get_time_ns();
    rc = channel_service_receive(svc, ch_id, &recv_buf, &recv_len);
    uint64_t t3 = get_time_ns();
    uint64_t recv_ns = t3 - t2;

    assert_int_equal(rc, 0);
    assert_non_null(recv_buf);
    assert_int_equal((int)recv_len, (int)send_len);
    assert_memory_equal(recv_buf, send_data, send_len);

    double total_ms = (double)(send_ns + recv_ns) / 1000000.0;
    printf("  [SOCKET RT] send=%.1fus recv=%.1fus total=%.2fms\n",
           (double)send_ns / 1000.0, (double)recv_ns / 1000.0, total_ms);

    assert_true(total_ms < 10.0);

    free(recv_buf);
    channel_service_close(svc, ch_id);
    channel_service_stop(svc);
    channel_service_destroy(svc);
    cleanup_test_dir();
}

static void test_send_receive_shm_roundtrip(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    const char* ch_id = "rt_shm";
    assert_int_equal(channel_service_open(svc, ch_id, "RT-SHM",
                                          CHANNEL_TYPE_SHM, NULL), 0);

    const char* send_data = "SHM payload for e2e test";
    size_t send_len = strlen(send_data);

    uint64_t t0 = get_time_ns();
    int rc = channel_service_send(svc, ch_id, send_data, send_len);
    uint64_t t1 = get_time_ns();

    assert_int_equal(rc, 0);

    void* recv_buf = NULL;
    size_t recv_len = 0;
    rc = channel_service_receive(svc, ch_id, &recv_buf, &recv_len);
    uint64_t t2 = get_time_ns();

    assert_int_equal(rc, 0);
    assert_non_null(recv_buf);
    assert_memory_equal(recv_buf, send_data, send_len);

    double total_ms = (double)(t2 - t0) / 1000000.0;
    printf("  [SHM   RT] total=%.2fus (%.3fms)\n",
           (double)(t2 - t0) / 1000.0, total_ms);
    assert_true(total_ms < 10.0);

    free(recv_buf);
    channel_service_close(svc, ch_id);
    channel_service_stop(svc);
    channel_service_destroy(svc);
}

static void test_latency_stress(void **state) {
    (void)state;
    cleanup_test_dir();
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    strncpy(config.socket_dir, TEST_SOCKET_DIR, sizeof(config.socket_dir) - 1);
    config.max_channels = 32;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    const char* ch_id = "stress_ch";
    assert_int_equal(channel_service_open(svc, ch_id, "Stress",
                                          CHANNEL_TYPE_SOCKET, NULL), 0);

    uint64_t total_ns = 0;
    int success_count = 0;
    const char* data = "ping";

    for (int i = 0; i < LATENCY_ITERATIONS; i++) {
        uint64_t t0 = get_time_ns();
        int rc = channel_service_send(svc, ch_id, data, strlen(data));
        if (rc != 0) continue;

        void* buf = NULL;
        size_t len = 0;
        rc = channel_service_receive(svc, ch_id, &buf, &len);
        if (rc == 0 && buf) {
            uint64_t t1 = get_time_ns();
            total_ns += (t1 - t0);
            success_count++;
            free(buf);
        }
    }

    assert_true(success_count > LATENCY_ITERATIONS * 80 / 100);

    double avg_us = (double)total_ns / (double)success_count / 1000.0;
    printf("  [STRESS] %d/%d iterations, avg=%.1fus max_allowed=10000us\n",
           success_count, LATENCY_ITERATIONS, avg_us);
    assert_true(avg_us < 10000.0);

    channel_service_close(svc, ch_id);
    channel_service_stop(svc);
    channel_service_destroy(svc);
    cleanup_test_dir();
}

/* ========================================================================
 * 测试组5: 回调机制
 * ======================================================================== */

static int g_callback_invoked = 0;
static size_t g_callback_data_len = 0;

static void test_callback_fn(const char* ch_id, const void* data,
                              size_t len, void* user_data) {
    (void)ch_id; (void)user_data;
    g_callback_invoked++;
    g_callback_data_len = len;
}

static void test_callback_on_send(void **state) {
    (void)state;
    g_callback_invoked = 0;
    g_callback_data_len = 0;

    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    const char* ch_id = "cb_ch";
    assert_int_equal(channel_service_open(svc, ch_id, "Callback",
                                          CHANNEL_TYPE_SOCKET, NULL), 0);

    assert_int_equal(channel_service_set_callback(svc, ch_id, test_callback_fn, NULL), 0);

    const char* msg = "callback trigger";
    assert_int_equal(channel_service_send(svc, ch_id, msg, strlen(msg)), 0);

    assert_int_equal(g_callback_invoked, 1);
    assert_int_equal(g_callback_data_len, strlen(msg));

    channel_service_close(svc, ch_id);
    channel_service_stop(svc);
    channel_service_destroy(svc);
}

/* ========================================================================
 * 测试组6: 错误处理边界条件
 * ======================================================================== */

static void test_error_null_params(void **state) {
    (void)state;

    channel_service_t* svc = channel_service_create(NULL);
    assert_non_null(svc);
    channel_service_destroy(svc);

    channel_service_destroy(NULL);
    assert_int_equal(channel_service_start(NULL), -1);
    assert_int_equal(channel_service_stop(NULL), -1);
    assert_int_equal(channel_service_open(NULL, "x", "y", CHANNEL_TYPE_SOCKET, NULL), -1);
    assert_int_equal(channel_service_close(NULL, "x"), -1);
    assert_int_equal(channel_service_send(NULL, "x", "d", 1), -1);
    assert_int_equal(channel_service_receive(NULL, "x", NULL, NULL), -1);
    assert_int_equal(channel_service_list(NULL, NULL, 0, NULL), -1);
    assert_int_equal(channel_service_get_info(NULL, "x", NULL), -1);
    assert_false(channel_service_is_healthy(NULL));
}

static void test_error_invalid_channel(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    config.max_channels = 16;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    assert_int_equal(channel_service_close(svc, "nonexistent"), -2);
    assert_int_equal(channel_service_get_info(svc, "nonexistent", &(channel_info_t){0}), -2);

    void* buf = NULL;
    size_t len = 0;
    assert_int_equal(channel_service_receive(svc, "nonexistent", &buf, &len), -2);
    assert_int_equal(channel_service_send(svc, "nonexistent", "data", 4), -2);

    channel_service_stop(svc);
    channel_service_destroy(svc);
}

static void test_error_max_channels(void **state) {
    (void)state;
    channel_config_t config = CHANNEL_CONFIG_DEFAULTS;
    config.max_channels = 4;

    channel_service_t* svc = channel_service_create(&config);
    assert_int_equal(channel_service_start(svc), 0);

    for (uint32_t i = 0; i < config.max_channels; i++) {
        char id[32], name[32];
        snprintf(id, sizeof(id), "ch_%u", i);
        snprintf(name, sizeof(name), "Ch%u", i);
        assert_int_equal(channel_service_open(svc, id, name,
                                              CHANNEL_TYPE_PIPE, NULL), 0);
    }

    int rc = channel_service_open(svc, "overflow", "Overflow",
                                  CHANNEL_TYPE_PIPE, NULL);
    assert_true(rc != 0);

    channel_service_stop(svc);
    channel_service_destroy(svc);
}

/* ========================================================================
 * 主入口
 * ======================================================================== */

int main(void) {
    const struct CMUnitTest tests[] = {

        cmocka_unit_test(test_lifecycle_create_destroy),
        cmocka_unit_test(test_lifecycle_start_stop),
        cmocka_unit_test(test_lifecycle_double_start),

        cmocka_unit_test(test_health_check),

        cmocka_unit_test(test_channel_open_close_socket),
        cmocka_unit_test(test_channel_open_close_shm),
        cmocka_unit_test(test_channel_duplicate_id),

        cmocka_unit_test(test_send_receive_socket_roundtrip),
        cmocka_unit_test(test_send_receive_shm_roundtrip),
        cmocka_unit_test(test_latency_stress),

        cmocka_unit_test(test_callback_on_send),

        cmocka_unit_test(test_error_null_params),
        cmocka_unit_test(test_error_invalid_channel),
        cmocka_unit_test(test_error_max_channels),
    };

#ifdef USE_CMOCKA_STUB
    return cmocka_run_group_tests(tests,
                                   sizeof(tests) / sizeof(tests[0]),
                                   NULL, NULL);
#else
    return cmocka_run_group_tests(tests, NULL, NULL);
#endif
}
