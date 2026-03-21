/**
 * @file http_gateway.c
 * @brief HTTP 网关实现
 */

#include "http_gateway.h"
#include "../server.h"          // 需要访问全局 server 实例（如果需要）
#include "logger.h"
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

typedef struct http_gateway {
    gateway_t          base;
    struct MHD_Daemon* daemon;
    const char*        host;
    uint16_t           port;
} http_gateway_t;

// 请求处理回调
static enum MHD_Result handler(void* cls,
                                struct MHD_Connection* connection,
                                const char* url,
                                const char* method,
                                const char* version,
                                const char* upload_data,
                                size_t* upload_data_size,
                                void** con_cls) {
    // 只处理 POST /rpc
    if (strcmp(method, "POST") != 0 || strcmp(url, "/rpc") != 0) {
        return MHD_NO;
    }
    // 读取请求体（简化：假设一次读完）
    if (*con_cls == NULL) {
        *con_cls = (void*)1; // 标记已接收头部
        return MHD_YES;
    }
    // 这里应该解析 JSON-RPC 请求并调用 syscall
    // 示例：返回一个固定的 JSON
    const char* response_body = "{\"jsonrpc\":\"2.0\",\"result\":\"ok\",\"id\":1}";
    struct MHD_Response* response = MHD_create_response_from_buffer(
        strlen(response_body), (void*)response_body, MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(response, "Content-Type", "application/json");
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    return ret;
}

static int http_gateway_start(gateway_t* base) {
    http_gateway_t* gw = (http_gateway_t*)base;
    gw->daemon = MHD_start_daemon(MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
                                   gw->port,
                                   NULL, NULL,
                                   &handler, gw,
                                   MHD_OPTION_END);
    if (!gw->daemon) return -1;
    AGENTOS_LOG_INFO("HTTP gateway started on %s:%d", gw->host, gw->port);
    return 0;
}

static void http_gateway_stop(gateway_t* base) {
    http_gateway_t* gw = (http_gateway_t*)base;
    if (gw->daemon) {
        MHD_stop_daemon(gw->daemon);
        gw->daemon = NULL;
    }
}

static void http_gateway_destroy(gateway_t* base) {
    http_gateway_t* gw = (http_gateway_t*)base;
    http_gateway_stop(base);
    free(gw);
}

static gateway_ops_t http_ops = {
    .start = http_gateway_start,
    .stop  = http_gateway_stop,
    .destroy = http_gateway_destroy,
};

gateway_t* http_gateway_create(const char* host, uint16_t port) {
    http_gateway_t* gw = (http_gateway_t*)calloc(1, sizeof(http_gateway_t));
    if (!gw) return NULL;
    gw->base.ops = &http_ops;
    gw->host = host;
    gw->port = port;
    return (gateway_t*)gw;
}