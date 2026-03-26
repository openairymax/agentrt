/**
 * @file test_ipc.c
 * @brief IPC 单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "ipc.h"
#include "error.h"
#include "task.h"
#include "time.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define TEST_CHANNEL_NAME "test_channel"

void test_ipc_thread(void* arg) {
    // 打开通道
    agentos_ipc_channel_t* channel = NULL;
    agentos_error_t err = agentos_ipc_open(TEST_CHANNEL_NAME, &channel);
    if (err != AGENTOS_SUCCESS || !channel) {
        printf("Thread failed to open channel: %d\n", err);
        return;
    }
    
    // 接收消息
    // From data intelligence emerges. by spharx
    agentos_ipc_message_t msg = {0};
    char buffer[256];
    msg.data = buffer;
    msg.size = sizeof(buffer);
    err = agentos_ipc_recv(channel, &msg, 5000);
    if (err != AGENTOS_SUCCESS) {
        printf("Thread failed to receive message: %d\n", err);
        agentos_ipc_close(channel);
        return;
    }
    
    printf("Thread received: %s\n", (char*)msg.data);
    
    // 发送响应
    const char* response = "Hello from thread!";
    agentos_ipc_message_t resp_msg = {0};
    resp_msg.data = (void*)response;
    resp_msg.size = strlen(response) + 1;
    err = agentos_ipc_send(channel, &resp_msg);
    if (err != AGENTOS_SUCCESS) {
        printf("Thread failed to send response: %d\n", err);
        agentos_ipc_close(channel);
        return;
    }
    
    agentos_ipc_close(channel);
}

int test_ipc_basic() {
    printf("Testing basic IPC functionality...\n");
    
    // 初始化 IPC
    agentos_error_t err = agentos_ipc_init();
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to initialize IPC: %d\n", err);
        return 1;
    }
    
    // 创建通道
    agentos_ipc_channel_t* channel = NULL;
    err = agentos_ipc_create_channel(TEST_CHANNEL_NAME, NULL, NULL, &channel);
    if (err != AGENTOS_SUCCESS || !channel) {
        printf("Failed to create channel: %d\n", err);
        agentos_ipc_cleanup();
        return 1;
    }
    
    // 创建测试线程
    agentos_thread_t thread;
    agentos_thread_attr_t attr = {0};
    strcpy(attr.name, "test_thread");
    attr.priority = AGENTOS_TASK_PRIORITY_NORMAL;
    attr.stack_size = 1024 * 1024;
    err = agentos_thread_create(&thread, &attr, test_ipc_thread, NULL);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to create thread: %d\n", err);
        agentos_ipc_close(channel);
        agentos_ipc_cleanup();
        return 1;
    }
    
    // 等待线程启动
    agentos_task_sleep(100);
    
    // 发送消息
    const char* message = "Hello from main!";
    agentos_ipc_message_t msg = {0};
    msg.data = (void*)message;
    msg.size = strlen(message) + 1;
    err = agentos_ipc_send(channel, &msg);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to send message: %d\n", err);
        agentos_ipc_close(channel);
        agentos_ipc_cleanup();
        return 1;
    }
    
    // 接收响应
    agentos_ipc_message_t response = {0};
    char buffer[256];
    response.data = buffer;
    response.size = sizeof(buffer);
    err = agentos_ipc_recv(channel, &response, 5000);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to receive response: %d\n", err);
        agentos_ipc_close(channel);
        agentos_ipc_cleanup();
        return 1;
    }
    
    printf("Main received: %s\n", (char*)response.data);
    
    // 等待线程结束
    agentos_thread_join(&thread, NULL);
    
    // 关闭通道
    agentos_ipc_close(channel);
    
    // 清理 IPC
    agentos_ipc_cleanup();
    
    printf("Basic IPC test passed\n");
    return 0;
}

int test_ipc_error_handling() {
    printf("Testing IPC error handling...\n");
    
    // 初始化 IPC
    agentos_error_t err = agentos_ipc_init();
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to initialize IPC: %d\n", err);
        return 1;
    }
    
    // 测试空指针操作
    agentos_ipc_message_t msg = {0};
    agentos_ipc_send(NULL, &msg);
    
    // 测试关闭空通道
    agentos_ipc_close(NULL);
    
    // 测试打开不存在的通道
    agentos_ipc_channel_t* channel = NULL;
    err = agentos_ipc_open("non_existent_channel", &channel);
    if (err == AGENTOS_SUCCESS && channel) {
        printf("Should not be able to open non-existent channel\n");
        agentos_ipc_close(channel);
    }
    
    // 测试创建通道时的参数错误
    err = agentos_ipc_create_channel(NULL, NULL, NULL, &channel);
    if (err == AGENTOS_SUCCESS) {
        printf("Should not be able to create channel with NULL name\n");
    }
    
    // 清理 IPC
    agentos_ipc_cleanup();
    
    printf("IPC error handling test passed\n");
    return 0;
}

int main() {
    int result = 0;
    
    result |= test_ipc_basic();
    result |= test_ipc_error_handling();
    
    if (result == 0) {
        printf("All IPC tests passed!\n");
    } else {
        printf("Some IPC tests failed!\n");
    }
    
    return result;
}
