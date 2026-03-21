/**
 * @file ws_gateway.c
 * @brief WebSocket 网关
 */

#include "gateway.h"
#include "../server.h"
#include "logger.h"
#include <libwebsockets.h>
#include <stdlib.h>
#include <string.h>

typedef struct ws_gateway {
    gateway_t          base;
    struct lws_context* context;
    const char*        host;
    uint16_t           port;
    pthread_t          service_thread;
    volatile int       running;
} ws_gateway_t;

static int ws_callback(struct lws* wsi, enum lws_callback_reasons reason,
                       void* user, void* in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            AGENTOS_LOG_DEBUG("WebSocket connection established");
            break;
        case LWS_CALLBACK_RECEIVE:
            // 简单回显
            lws_write(wsi, in, len, LWS_WRITE_TEXT);
            break;
        case LWS_CALLBACK_CLOSED:
            AGENTOS_LOG_DEBUG("WebSocket connection closed");
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    { "agentos-ws", ws_callback, 0, 4096 },
    { NULL, NULL, 0, 0 }
};

static void* service_thread_func(void* arg) {
    ws_gateway_t* gw = (ws_gateway_t*)arg;
    while (gw->running) {
        lws_service(gw->context, 50);
    }
    return NULL;
}

static int ws_gateway_start(gateway_t* base) {
    ws_gateway_t* gw = (ws_gateway_t*)base;
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = gw->port;
    info.iface = gw->host;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    gw->context = lws_create_context(&info);
    if (!gw->context) return -1;
    gw->running = 1;
    if (pthread_create(&gw->service_thread, NULL, service_thread_func, gw) != 0) {
        lws_context_destroy(gw->context);
        gw->context = NULL;
        return -1;
    }
    AGENTOS_LOG_INFO("WebSocket gateway started on %s:%d", gw->host, gw->port);
    return 0;
}

static void ws_gateway_stop(gateway_t* base) {
    ws_gateway_t* gw = (ws_gateway_t*)base;
    if (gw->running) {
        gw->running = 0;
        pthread_join(gw->service_thread, NULL);
        lws_context_destroy(gw->context);
        gw->context = NULL;
    }
}

static void ws_gateway_destroy(gateway_t* base) {
    ws_gateway_t* gw = (ws_gateway_t*)base;
    ws_gateway_stop(base);
    free(gw);
}

static gateway_ops_t ws_ops = {
    .start = ws_gateway_start,
    .stop  = ws_gateway_stop,
    .destroy = ws_gateway_destroy,
};

gateway_t* ws_gateway_create(const char* host, uint16_t port) {
    ws_gateway_t* gw = (ws_gateway_t*)calloc(1, sizeof(ws_gateway_t));
    if (!gw) return NULL;
    gw->base.ops = &ws_ops;
    gw->host = host;
    gw->port = port;
    return (gateway_t*)gw;
}