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

#include "../../../../commons/utils/memory/include/memory_compat.h"
#include "../../../../commons/utils/string/include/string_compat.h"
#include "error.h"
#include "task.h"
#include "mem.h"
#include "ipc.h"
#include "time.h"

int test_error_strerror_coverage(void) {
    printf("  测试错误字符串覆盖...\n");

    const char* errors[] = {
        agentos_error_string(AGENTOS_SUCCESS),
        agentos_error_string(AGENTOS_EINVAL),
        agentos_error_string(AGENTOS_ENOMEM),
        agentos_error_string(AGENTOS_EBUSY),
        agentos_error_string(AGENTOS_ENOENT),
        agentos_error_string(AGENTOS_EPERM),
        agentos_error_string(AGENTOS_ETIMEDOUT),
        agentos_error_string(AGENTOS_EEXIST),
        agentos_error_string(AGENTOS_ECANCELED),
        agentos_error_string(AGENTOS_ENOTSUP),
        agentos_error_string(AGENTOS_EIO),
        agentos_error_string(AGENTOS_EINTR),
        agentos_error_string(AGENTOS_EOVERFLOW),
        agentos_error_string(AGENTOS_EBADF)
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

    int err = agentos_mem_pool_create(pool_size, &pool);
    if (err != AGENTOS_SUCCESS && pool != NULL) {
        printf("    内存池创建失败: %d\n", err);
        return 1;
    }

    void* block1 = agentos_mem_pool_alloc(pool, 256);
    if (block1 == NULL) {
        printf("    从池中分配失败\n");
        agentos_mem_pool_destroy(pool);
        return 1;
    }

    memset(block1, 0xAB, 256);

    void* block2 = agentos_mem_pool_alloc(pool, 512);
    if (block2 == NULL) {
        printf("    第二次分配失败\n");
        agentos_mem_pool_destroy(pool);
        return 1;
    }

    memset(block2, 0xCD, 512);

    agentos_mem_pool_free(pool, block2);
    agentos_mem_pool_free(pool, block1);
    agentos_mem_pool_destroy(pool);

    printf("    内存池基本操作测试通过\n");
    return 0;
}

int test_memory_guard(void) {
    printf("  测试内存保护机制...\n");

    char* buffer = (char*)agentos_malloc(100);
    if (buffer == NULL) {
        printf("    分配失败\n");
        return 1;
    }

    memset(buffer, 'A', 99);
    buffer[99] = '\0';

    agentos_write_guard(buffer, 100, 0xDE, 0, 100);
    
    unsigned char expected = agentos_read_guard(buffer, 50);
    if (expected != 0xDE) {
        printf("    守卫值不正确\n");
        agentos_free(buffer);
        return 1;
    }

    agentos_free(buffer);
    printf("    内存保护机制测试通过\n");
    return 0;
}

int test_time_clock(void) {
    printf("  测试时钟接口...\n");

    uint64_t t1 = agentos_clock_now();
    if (t1 == 0) {
        printf("    时钟返回0\n");
        return 1;
    }

    for (volatile int i = 0; i < 100000; i++);

    uint64_t t2 = agentos_clock_now();
    if (t2 <= t1) {
        printf("    时钟应单调递增\n");
        return 1;
    }

    printf("    时钟接口测试通过\n");
    return 0;
}

int test_time_timer_creation(void) {
    printf("  测试定时器创建...\n");

    agentos_timer_t* timer = NULL;
    int err = agentos_timer_create(&timer, 1000, NULL);
    if (err != AGENTOS_SUCCESS || timer == NULL) {
        printf("    定时器创建失败\n");
        return 1;
    }

    err = agentos_timer_start(timer);
    if (err != AGENTOS_SUCCESS) {
        printf("    定时器启动失败\n");
        agentos_timer_destroy(timer);
        return 1;
    }

    agentos_task_sleep(50);

    err = agentos_timer_stop(timer);
    if (err != AGENTOS_SUCCESS) {
        printf("    定时器停止失败\n");
        agentos_timer_destroy(timer);
        return 1;
    }

    agentos_timer_destroy(timer);
    printf("    定时器创建测试通过\n");
    return 0;
}

int test_ipc_buffer_operations(void) {
    printf("  测试IPC缓冲区操作...\n");

    agentos_ipc_buffer_t* buffer = NULL;
    int err = agentos_ipc_buffer_create(4096, &buffer);
    if (err != AGENTOS_SUCCESS || buffer == NULL) {
        printf("    IPC缓冲区创建失败\n");
        return 1;
    }

    const char* test_data = "IPC test data";
    size_t data_len = strlen(test_data) + 1;

    err = agentos_ipc_buffer_write(buffer, test_data, data_len);
    if (err != AGENTOS_SUCCESS) {
        printf("    IPC缓冲区写入失败\n");
        agentos_ipc_buffer_destroy(buffer);
        return 1;
    }

    char read_data[256];
    size_t read_len = 0;
    err = agentos_ipc_buffer_read(buffer, read_data, sizeof(read_data), &read_len);
    if (err != AGENTOS_SUCCESS) {
        printf("    IPC缓冲区读取失败\n");
        agentos_ipc_buffer_destroy(buffer);
        return 1;
    }

    if (read_len != data_len || memcmp(read_data, test_data, data_len) != 0) {
        printf("    读取数据不匹配\n");
        agentos_ipc_buffer_destroy(buffer);
        return 1;
    }

    agentos_ipc_buffer_destroy(buffer);
    printf("    IPC缓冲区操作测试通过\n");
    return 0;
}

int test_ipc_channel_creation(void) {
    printf("  测试IPC通道创建...\n");

    agentos_ipc_channel_t* channel = NULL;
    int err = agentos_ipc_channel_create(&channel, 8192, 4);
    if (err != AGENTOS_SUCCESS || channel == NULL) {
        printf("    IPC通道创建失败\n");
        return 1;
    }

    agentos_ipc_channel_destroy(channel);
    printf("    IPC通道创建测试通过\n");
    return 0;
}

int test_task_scheduler_init(void) {
    printf("  测试任务调度器初始化...\n");

    int err = agentos_task_init();
    if (err != AGENTOS_SUCCESS) {
        printf("    任务调度器初始化失败: %d\n", err);
        return 1;
    }

    printf("    任务调度器初始化测试通过\n");
    return 0;
}

int test_task_thread_properties(void) {
    printf("  测试线程属性设置...\n");

    agentos_thread_attr_t attr = {0};
    snprintf(attr.name, sizeof(attr.name), "%s", "test_property_thread");
    attr.priority = AGENTOS_TASK_PRIORITY_HIGH;
    attr.stack_size = 2048 * 1024;

    if (attr.priority != AGENTOS_TASK_PRIORITY_HIGH) {
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

    agentos_thread_t threads[THREAD_COUNT];
    int indices[THREAD_COUNT];

    for (int i = 0; i < THREAD_COUNT; i++) {
        indices[i] = i;
        agentos_thread_attr_t attr = {0};
        snprintf(attr.name, sizeof(attr.name), "%s", "worker");
        attr.stack_size = 128 * 1024;

        int err = agentos_thread_create(&threads[i], &attr, thread_func, &indices[i]);
        if (err != AGENTOS_SUCCESS) {
            printf("    创建线程 %d 失败\n", i);
            return 1;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        void* result;
        int err = agentos_thread_join(&threads[i], &result);
        if (err != AGENTOS_SUCCESS) {
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
