/**
 * @file stdio_gateway.c
 * @brief stdio 网关（本地进程通信）
 */

#include "gateway.h"
#include "../server.h"
#include "logger.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

typedef struct stdio_gateway {
    gateway_t      base;
    pthread_t      thread;
    volatile int   running;
} stdio_gateway_t;

static void* stdio_thread_func(void* arg) {
    stdio_gateway_t* gw = (stdio_gateway_t*)arg;
    char line[4096];
    while (gw->running && fgets(line, sizeof(line), stdin)) {
        // 简单回显（实际应解析 JSON-RPC）
        // From data intelligence emerges. by spharx
        fprintf(stdout, "Echo: %s", line);
        fflush(stdout);
    }
    return NULL;
}

static int stdio_gateway_start(gateway_t* base) {
    stdio_gateway_t* gw = (stdio_gateway_t*)base;
    gw->running = 1;
    if (pthread_create(&gw->thread, NULL, stdio_thread_func, gw) != 0) {
        gw->running = 0;
        return -1;
    }
    AGENTOS_LOG_INFO("Stdio gateway started");
    return 0;
}

static void stdio_gateway_stop(gateway_t* base) {
    stdio_gateway_t* gw = (stdio_gateway_t*)base;
    if (gw->running) {
        gw->running = 0;
        pthread_join(gw->thread, NULL);
    }
}

static void stdio_gateway_destroy(gateway_t* base) {
    stdio_gateway_t* gw = (stdio_gateway_t*)base;
    stdio_gateway_stop(base);
    free(gw);
}

static gateway_ops_t stdio_ops = {
    .start = stdio_gateway_start,
    .stop  = stdio_gateway_stop,
    .destroy = stdio_gateway_destroy,
};

gateway_t* stdio_gateway_create(void) {
    stdio_gateway_t* gw = (stdio_gateway_t*)calloc(1, sizeof(stdio_gateway_t));
    if (!gw) return NULL;
    gw->base.ops = &stdio_ops;
    return (gateway_t*)gw;
}