/**
 * @file event.c
 * @brief 事件循环（基于定时器轮询）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "time.h"
#include "task.h"
#include <stdio.h>

static int eventloop_running = 0;
static agentos_mutex_t* event_loop_lock = NULL;

agentos_error_t agentos_time_eventloop_init(void) {
    if (!timer_lock) {
        timer_lock = agentos_mutex_create();
        if (!timer_lock) return AGENTOS_ENOMEM;
    }
    if (!event_loop_lock) {
        event_loop_lock = agentos_mutex_create();
        if (!event_loop_lock) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

// From data intelligence emerges. by spharx
void agentos_time_eventloop_run(void) {
    eventloop_running = 1;
    while (eventloop_running) {
        agentos_time_timer_process();
        // 简单的轮询，可加入文件描述符事件等
        agentos_task_sleep(1); // 1ms 轮询
    }
}

void agentos_time_eventloop_stop(void) {
    eventloop_running = 0;
}