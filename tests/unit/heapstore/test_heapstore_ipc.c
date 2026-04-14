/**
 * @file test_heapstore_ipc.c
 * @brief AgentOS 数据分区 IPC 数据存储单元测试
 *
 * Copyright (c) 2026 SPHARX. All Rights Reserved.
 * "From data intelligence emerges."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "heapstore.h"
#include "heapstore_ipc.h"

static void test_ipc_init_shutdown(void) {
    printf("Test: ipc_init_shutdown...");

    heapstore_error_t err = heapstore_ipc_init();
    assert(err == heapstore_SUCCESS || err == heapstore_ERR_ALREADY_INITIALIZED);

    heapstore_ipc_shutdown();
    printf("PASS\n");
}

static void test_ipc_channel_crud(void) {
    printf("Test: ipc_channel_crud...");

    heapstore_ipc_channel_t channel;
    memset(&channel, 0, sizeof(channel));

    snprintf(channel.channel_id, sizeof(channel.channel_id), "ch_%ld", (long)time(NULL));
    snprintf(channel.name, sizeof(channel.name), "Test Channel");
    snprintf(channel.type, sizeof(channel.type), "binder");
    channel.created_at = (uint64_t)time(NULL);
    channel.last_activity_at = channel.created_at;
    channel.buffer_size = 4096;
    channel.current_usage = 1024;
    snprintf(channel.status, sizeof(channel.status), "active");

    heapstore_error_t err = heapstore_ipc_record_channel(&channel);
    if (err == heapstore_SUCCESS) {
        heapstore_ipc_channel_t get_ch;
        memset(&get_ch, 0, sizeof(get_ch));

        err = heapstore_ipc_get_channel(channel.channel_id, &get_ch);
        assert(err == heapstore_SUCCESS);
        assert(strcmp(get_ch.name, channel.name) == 0);
        assert(strcmp(get_ch.type, channel.type) == 0);
        assert(get_ch.buffer_size == channel.buffer_size);

        err = heapstore_ipc_update_channel_activity(channel.channel_id);
        assert(err == heapstore_SUCCESS);
    }

    printf("PASS\n");
}

static void test_ipc_buffer_crud(void) {
    printf("Test: ipc_buffer_crud...");

    heapstore_ipc_channel_t channel;
    memset(&channel, 0, sizeof(channel));

    snprintf(channel.channel_id, sizeof(channel.channel_id), "ch_buf_%ld", (long)time(NULL));
    snprintf(channel.name, sizeof(channel.name), "Buffer Test Channel");
    snprintf(channel.type, sizeof(channel.type), "shared_memory");
    channel.created_at = (uint64_t)time(NULL);
    snprintf(channel.status, sizeof(channel.status), "active");

    heapstore_error_t err = heapstore_ipc_record_channel(&channel);

    heapstore_ipc_buffer_t buffer;
    memset(&buffer, 0, sizeof(buffer));

    snprintf(buffer.buffer_id, sizeof(buffer.buffer_id), "buf_%ld", (long)time(NULL));
    snprintf(buffer.channel_id, sizeof(buffer.channel_id), "%s", channel.channel_id);
    buffer.created_at = (uint64_t)time(NULL);
    buffer.size = 8192;
    buffer.ref_count = 1;
    snprintf(buffer.status, sizeof(buffer.status), "active");

    if (err == heapstore_SUCCESS) {
        err = heapstore_ipc_record_buffer(&buffer);
        if (err == heapstore_SUCCESS) {
            heapstore_ipc_buffer_t get_buf;
            memset(&get_buf, 0, sizeof(get_buf));

            err = heapstore_ipc_get_buffer(buffer.buffer_id, &get_buf);
            assert(err == heapstore_SUCCESS);
            assert(strcmp(get_buf.channel_id, buffer.channel_id) == 0);
            assert(get_buf.size == buffer.size);
            assert(get_buf.ref_count == buffer.ref_count);
        }
    }

    printf("PASS\n");
}

static void test_ipc_stats(void) {
    printf("Test: ipc_stats...");

    uint32_t channel_count = 0;
    uint32_t buffer_count = 0;
    uint64_t total_size = 0;

    heapstore_error_t err = heapstore_ipc_get_stats(&channel_count, &buffer_count, &total_size);
    assert(err == heapstore_SUCCESS);

    printf("  Channels: %u, Buffers: %u, Total Size: %lu\n", channel_count, buffer_count, (unsigned long)total_size);

    printf("PASS\n");
}

static void test_ipc_invalid_params(void) {
    printf("Test: ipc_invalid_params...");

    heapstore_error_t err = heapstore_ipc_record_channel(NULL);
    assert(err == heapstore_ERR_INVALID_PARAM);

    heapstore_ipc_channel_t invalid_ch;
    memset(&invalid_ch, 0, sizeof(invalid_ch));
    err = heapstore_ipc_record_channel(&invalid_ch);
    assert(err == heapstore_ERR_INVALID_PARAM);

    err = heapstore_ipc_record_buffer(NULL);
    assert(err == heapstore_ERR_INVALID_PARAM);

    heapstore_ipc_buffer_t invalid_buf;
    memset(&invalid_buf, 0, sizeof(invalid_buf));
    err = heapstore_ipc_record_buffer(&invalid_buf);
    assert(err == heapstore_ERR_INVALID_PARAM);

    err = heapstore_ipc_get_channel(NULL, NULL);
    assert(err == heapstore_ERR_INVALID_PARAM);

    err = heapstore_ipc_get_buffer(NULL, NULL);
    assert(err == heapstore_ERR_INVALID_PARAM);

    err = heapstore_ipc_update_channel_activity(NULL);
    assert(err == heapstore_ERR_INVALID_PARAM);

    printf("PASS\n");
}

static void test_ipc_not_found(void) {
    printf("Test: ipc_not_found...");

    heapstore_ipc_channel_t channel;
    memset(&channel, 0, sizeof(channel));

    heapstore_error_t err = heapstore_ipc_get_channel("nonexistent_id", &channel);
    assert(err == heapstore_ERR_NOT_FOUND);

    err = heapstore_ipc_get_buffer("nonexistent_id", &channel);
    assert(err == heapstore_ERR_NOT_FOUND);

    err = heapstore_ipc_update_channel_activity("nonexistent_id");
    assert(err == heapstore_ERR_NOT_FOUND);

    printf("PASS\n");
}

static void test_ipc_multiple_channels(void) {
    printf("Test: ipc_multiple_channels...");

    for (int i = 0; i < 5; i++) {
        heapstore_ipc_channel_t channel;
        memset(&channel, 0, sizeof(channel));

        snprintf(channel.channel_id, sizeof(channel.channel_id), "ch_multi_%d_%ld", i, (long)time(NULL));
        snprintf(channel.name, sizeof(channel.name), "Channel %d", i);
        snprintf(channel.type, sizeof(channel.type), "type_%d", i);
        channel.created_at = (uint64_t)time(NULL);
        snprintf(channel.status, sizeof(channel.status), "active");

        heapstore_error_t err = heapstore_ipc_record_channel(&channel);
        assert(err == heapstore_SUCCESS || err == heapstore_ERR_OUT_OF_MEMORY);
    }

    uint32_t count = 0;
    heapstore_ipc_get_stats(&count, NULL, NULL);
    printf("  Total channels recorded: %u\n", count);

    printf("PASS\n");
}

int main(void) {
    printf("=== AgentOS heapstore IPC Unit Tests ===\n\n");

    test_ipc_init_shutdown();
    test_ipc_channel_crud();
    test_ipc_buffer_crud();
    test_ipc_stats();
    test_ipc_invalid_params();
    test_ipc_not_found();
    test_ipc_multiple_channels();

    printf("\n=== All IPC Tests Passed ===\n");
    return 0;
}

