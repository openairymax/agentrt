/**
 * @file test_time.c
 * @brief 时间服务单元测试
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "corekern/include/time.h"
#include "corekern/include/error.h"
#include <stdio.h>

int test_time_monotonic() {
    printf("Testing monotonic time functions...\n");
    
    // 测试单调时间（纳秒）
    uint64_t start_ns = agentos_time_monotonic_ns();
    if (start_ns == 0) {
        printf("Failed to get monotonic time (ns)\n");
        return 1;
    }
    
    // 测试单调时间（毫秒）
    uint64_t start_ms = agentos_time_monotonic_ms();
    if (start_ms == 0) {
        printf("Failed to get monotonic time (ms)\n");
        return 1;
        // From data intelligence emerges. by spharx
    }
    
    // 验证时间递增
    uint64_t end_ns = agentos_time_monotonic_ns();
    if (end_ns < start_ns) {
        printf("Monotonic time (ns) did not increase\n");
        return 1;
    }
    
    uint64_t end_ms = agentos_time_monotonic_ms();
    if (end_ms < start_ms) {
        printf("Monotonic time (ms) did not increase\n");
        return 1;
    }
    
    // 验证时间单位转换
    if (end_ms != end_ns / 1000000ULL) {
        printf("Time unit conversion failed\n");
        return 1;
    }
    
    printf("Monotonic time test passed\n");
    return 0;
}

int test_time_current() {
    printf("Testing current time functions...\n");
    
    // 测试当前时间（纳秒）
    uint64_t start_ns = agentos_time_current_ns();
    if (start_ns == 0) {
        printf("Failed to get current time (ns)\n");
        return 1;
    }
    
    // 测试当前时间（毫秒）
    uint64_t start_ms = agentos_time_current_ms();
    if (start_ms == 0) {
        printf("Failed to get current time (ms)\n");
        return 1;
    }
    
    // 验证时间单位转换
    if (start_ms != start_ns / 1000000ULL) {
        printf("Time unit conversion failed\n");
        return 1;
    }
    
    // 测试 realtime 函数（应该与 current 函数结果一致）
    uint64_t realtime_ns = agentos_time_realtime_ns();
    if (realtime_ns == 0) {
        printf("Failed to get realtime (ns)\n");
        return 1;
    }
    
    // 允许一定的时间差（最多1秒）
    if (realtime_ns < start_ns - 1000000000ULL || realtime_ns > start_ns + 1000000000ULL) {
        printf("Realtime and current time differ significantly\n");
        return 1;
    }
    
    printf("Current time test passed\n");
    return 0;
}

int test_time_sleep() {
    printf("Testing sleep function...\n");
    
    // 测试纳秒睡眠
    uint64_t start = agentos_time_monotonic_ns();
    agentos_error_t err = agentos_time_nanosleep(100000000ULL); // 100ms
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to sleep: %d\n", err);
        return 1;
    }
    uint64_t end = agentos_time_monotonic_ns();
    uint64_t elapsed = end - start;
    
    // 允许一定的误差（最多50ms）
    if (elapsed < 50000000ULL || elapsed > 150000000ULL) {
        printf("Sleep duration incorrect: %llu ns\n", elapsed);
        return 1;
    }
    
    printf("Sleep test passed\n");
    return 0;
}

int timer_callback_called = 0;

void timer_callback(void* userdata) {
    timer_callback_called = 1;
    printf("Timer callback called!\n");
}

int test_timer() {
    printf("Testing timer functionality...\n");
    
    // 创建定时器
    agentos_timer_t* timer = agentos_timer_create(timer_callback, NULL);
    if (!timer) {
        printf("Failed to create timer\n");
        return 1;
    }
    
    // 测试启动定时器
    agentos_error_t err = agentos_timer_start(timer, 100, 1); // 100ms, one-shot
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to start timer: %d\n", err);
        agentos_timer_destroy(timer);
        return 1;
    }
    
    // 等待定时器触发
    agentos_task_sleep(200);
    
    // 检查回调是否被调用
    if (!timer_callback_called) {
        printf("Timer callback not called\n");
        agentos_timer_destroy(timer);
        return 1;
    }
    
    // 测试停止定时器
    err = agentos_timer_stop(timer);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to stop timer: %d\n", err);
        agentos_timer_destroy(timer);
        return 1;
    }
    
    // 测试销毁定时器
    agentos_timer_destroy(timer);
    
    printf("Timer test passed\n");
    return 0;
}

int test_event() {
    printf("Testing event functionality...\n");
    
    // 创建事件
    agentos_event_t* event = agentos_event_create();
    if (!event) {
        printf("Failed to create event\n");
        return 1;
    }
    
    // 测试等待事件（超时）
    agentos_error_t err = agentos_event_wait(event, 100);
    if (err != AGENTOS_ETIMEDOUT) {
        printf("Event wait should have timed out: %d\n", err);
        agentos_event_destroy(event);
        return 1;
    }
    
    // 测试触发事件
    err = agentos_event_signal(event);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to signal event: %d\n", err);
        agentos_event_destroy(event);
        return 1;
    }
    
    // 测试等待事件（应该立即返回）
    err = agentos_event_wait(event, 100);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to wait for event: %d\n", err);
        agentos_event_destroy(event);
        return 1;
    }
    
    // 测试重置事件
    err = agentos_event_reset(event);
    if (err != AGENTOS_SUCCESS) {
        printf("Failed to reset event: %d\n", err);
        agentos_event_destroy(event);
        return 1;
    }
    
    // 测试销毁事件
    agentos_event_destroy(event);
    
    printf("Event test passed\n");
    return 0;
}

int main() {
    int result = 0;
    
    result |= test_time_monotonic();
    result |= test_time_current();
    result |= test_time_sleep();
    result |= test_timer();
    result |= test_event();
    
    if (result == 0) {
        printf("All time tests passed!\n");
    } else {
        printf("Some time tests failed!\n");
    }
    
    return result;
}
