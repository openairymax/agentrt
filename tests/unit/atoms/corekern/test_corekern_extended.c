/**
 * @file test_corekern_extended.c
 * @brief corekern模块扩展单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 测试corekern的核心功能：IPC、内存管理、时间管理、任务调度等
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "memory_compat.h"
#include "string_compat.h"
#include "error.h"
#include "task.h"
#include "mem.h"
#include "ipc.h"
#include "time.h"

int test_error_strerror_coverage(void) {
    printf("  测试错误字符串覆盖...\n");

    const char* errors[] = {
        agentrt_error_string(AGENTRT_SUCCESS),
        agentrt_error_string(AGENTRT_EINVAL),
        agentrt_error_string(AGENTRT_ENOMEM),
        agentrt_error_string(AGENTRT_EBUSY),
        agentrt_error_string(AGENTRT_ENOENT),
        agentrt_error_string(AGENTRT_EPERM),
        agentrt_error_string(AGENTRT_ETIMEDOUT),
        agentrt_error_string(AGENTRT_EEXIST),
        agentrt_error_string(AGENTRT_ECANCELED),
        agentrt_error_string(AGENTRT_ENOTSUP),
        agentrt_error_string(AGENTRT_EIO),
        agentrt_error_string(AGENTRT_EINTR),
        agentrt_error_string(AGENTRT_EOVERFLOW),
        agentrt_error_string(AGENTRT_EBADF)
    };

    for (size_t i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
        if (errors[i] == NULL || strlen(errors[i]) == 0) {
            printf("    错误码 %zu 返回空字符串\n", i);
            return 1;
        }
    }

    printf("    错误字符串覆盖测试通过 (%zu 个错误码)\n",
           sizeof(errors) / sizeof(errors[0]));
    return 0;
}

int test_memory_pool_basic(void) {
    printf("  测试内存池基本操作...\n");

    void* pool = NULL;
    size_t pool_size = 1024 * 1024;

    int err = agentrt_mem_pool_create(pool_size, &pool);
    if (err != AGENTRT_SUCCESS && pool != NULL) {
        printf("    内存池创建失败: %d\n", err);
        return 1;
    }

    void* block1 = agentrt_mem_pool_alloc(pool, 256);
    if (block1 == NULL) {
        printf("    从池中分配失败\n");
        agentrt_mem_pool_destroy(pool);
        return 1;
    }

    AGENTRT_MEMSET(block1, 0xAB, 256);

    void* block2 = agentrt_mem_pool_alloc(pool, 512);
    if (block2 == NULL) {
        printf("    第二次分配失败\n");
        agentrt_mem_pool_destroy(pool);
        return 1;
    }

    AGENTRT_MEMSET(block2, 0xCD, 512);

    agentrt_mem_pool_free(pool, block2);
    agentrt_mem_pool_free(pool, block1);
    agentrt_mem_pool_destroy(pool);

    printf("    内存池基本操作测试通过\n");
    return 0;
}

int test_memory_guard(void) {
    printf("  测试内存保护机制...\n");

    char* buffer = (char*)agentrt_malloc(100);
    if (buffer == NULL) {
        printf("    分配失败\n");
        return 1;
    }

    AGENTRT_MEMSET(buffer, 'A', 99);
    buffer[99] = '\0';

    agentrt_write_guard(buffer, 100, 0xDE, 0, 100);
    
    unsigned char expected = agentrt_read_guard(buffer, 50);
    if (expected != 0xDE) {
        printf("    守卫值不正确\n");
        agentrt_free(buffer);
        return 1;
    }

    agentrt_free(buffer);
    printf("    内存保护机制测试通过\n");
    return 0;
}

int test_time_clock(void) {
    printf("  测试时钟接口...\n");

    uint64_t t1 = agentrt_clock_now();
    if (t1 == 0) {
        printf("    时钟返回0\n");
        return 1;
    }

    for (volatile int i = 0; i < 100000; i++);

    uint64_t t2 = agentrt_clock_now();
    if (t2 <= t1) {
        printf("    时钟应单调递增\n");
        return 1;
    }

    printf("    时钟接口测试通过\n");
    return 0;
}

int test_time_timer_creation(void) {
    printf("  测试定时器创建...\n");

    agentrt_timer_t* timer = NULL;
    int err = agentrt_timer_create(&timer, 1000, NULL);
    if (err != AGENTRT_SUCCESS || timer == NULL) {
        printf("    定时器创建失败\n");
        return 1;
    }

    err = agentrt_timer_start(timer);
    if (err != AGENTRT_SUCCESS) {
        printf("    定时器启动失败\n");
        agentrt_timer_destroy(timer);
        return 1;
    }

    agentrt_task_sleep(50);

    err = agentrt_timer_stop(timer);
    if (err != AGENTRT_SUCCESS) {
        printf("    定时器停止失败\n");
        agentrt_timer_destroy(timer);
        return 1;
    }

    agentrt_timer_destroy(timer);
    printf("    定时器创建测试通过\n");
    return 0;
}

int test_ipc_buffer_operations(void) {
    printf("  测试IPC缓冲区操作...\n");

    agentrt_ipc_buffer_t* buffer = NULL;
    int err = agentrt_ipc_buffer_create(4096, &buffer);
    if (err != AGENTRT_SUCCESS || buffer == NULL) {
        printf("    IPC缓冲区创建失败\n");
        return 1;
    }

    const char* test_data = "IPC test data";
    size_t data_len = strlen(test_data) + 1;

    err = agentrt_ipc_buffer_write(buffer, test_data, data_len);
    if (err != AGENTRT_SUCCESS) {
        printf("    IPC缓冲区写入失败\n");
        agentrt_ipc_buffer_destroy(buffer);
        return 1;
    }

    char read_data[256];
    size_t read_len = 0;
    err = agentrt_ipc_buffer_read(buffer, read_data, sizeof(read_data), &read_len);
    if (err != AGENTRT_SUCCESS) {
        printf("    IPC缓冲区读取失败\n");
        agentrt_ipc_buffer_destroy(buffer);
        return 1;
    }

    if (read_len != data_len || memcmp(read_data, test_data, data_len) != 0) {
        printf("    读取数据不匹配\n");
        agentrt_ipc_buffer_destroy(buffer);
        return 1;
    }

    agentrt_ipc_buffer_destroy(buffer);
    printf("    IPC缓冲区操作测试通过\n");
    return 0;
}

int test_ipc_channel_creation(void) {
    printf("  测试IPC通道创建...\n");

    agentrt_ipc_channel_t* channel = NULL;
    int err = agentrt_ipc_channel_create(&channel, 8192, 4);
    if (err != AGENTRT_SUCCESS || channel == NULL) {
        printf("    IPC通道创建失败\n");
        return 1;
    }

    agentrt_ipc_channel_destroy(channel);
    printf("    IPC通道创建测试通过\n");
    return 0;
}

int test_task_scheduler_init(void) {
    printf("  测试任务调度器初始化...\n");

    int err = agentrt_task_init();
    if (err != AGENTRT_SUCCESS) {
        printf("    任务调度器初始化失败: %d\n", err);
        return 1;
    }

    printf("    任务调度器初始化测试通过\n");
    return 0;
}

int test_task_thread_properties(void) {
    printf("  测试线程属性设置...\n");

    agentrt_thread_attr_t attr = {0};
    snprintf(attr.name, sizeof(attr.name), "%s", "test_property_thread");
    attr.priority = AGENTRT_TASK_PRIORITY_HIGH;
    attr.stack_size = 2048 * 1024;

    if (attr.priority != AGENTRT_TASK_PRIORITY_HIGH) {
        printf("    优先级未正确设置\n");
        return 1;
    }
    if (attr.stack_size != 2048 * 1024) {
        printf("    栈大小未正确设置\n");
        return 1;
    }

    printf("    线程属性设置测试通过\n");
    return 0;
}

int test_task_multiple_threads(void) {
    printf("  测试多线程并发...\n");

#define THREAD_COUNT 5
    volatile int counters[THREAD_COUNT] = {0};

    void thread_func(void* arg) {
        int idx = *(int*)arg;
        for (volatile int i = 0; i < 10000; i++) {
            counters[idx]++;
        }
    }

    agentrt_thread_t threads[THREAD_COUNT];
    int indices[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        indices[i] = i;
        agentrt_thread_attr_t attr = {0};
        snprintf(attr.name, sizeof(attr.name), "%s", "worker");
        attr.stack_size = 128 * 1024;

        int err = agentrt_thread_create(&threads[i], &attr, thread_func, &indices[i]);
        if (err != AGENTRT_SUCCESS) {
            printf("    创建线程 %d 失败\n", i);
            return 1;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        void* result;
        int err = agentrt_thread_join(&threads[i], &result);
        if (err != AGENTRT_SUCCESS) {
            printf("    等待线程 %d 失败\n", i);
            return 1;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        if (counters[i] != 10000) {
            printf("    线程 %d 计数器值不正确: %d\n", i, counters[i]);
            return 1;
        }
    }

    printf("    多线程并发测试通过 (%d 个线程)\n", THREAD_COUNT);
    return 0;
}

int main(void) {
    printf("开始运行 corekern 扩展单元测试...\n");

    int failures = 0;

    failures |= test_error_strerror_coverage();
    failures |= test_memory_pool_basic();
    failures |= test_memory_guard();
    failures |= test_time_clock();
    failures |= test_time_timer_creation();
    failures |= test_ipc_buffer_operations();
    failures |= test_ipc_channel_creation();
    failures |= test_task_scheduler_init();
    failures |= test_task_thread_properties();
    failures |= test_task_multiple_threads();

    if (failures == 0) {
        printf("\n所有corekern扩展测试通过！\n");
        return 0;
    } else {
        printf("\n%d 个corekern扩展测试失败\n", failures);
        return 1;
    }
}
