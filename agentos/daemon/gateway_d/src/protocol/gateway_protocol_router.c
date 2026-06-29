#include "gateway_protocol_router.h"

#include "gateway_a2a_handler.h"
#include "gateway_mcp_server.h"
#include "gateway_openai_compat.h"
#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "error.h"

#include "logging_compat.h"

/**
 * @brief 将协议类型枚举转换为可读字符串
 */
static const char *proto_type_name(gw_proto_detect_result_t proto_type)
{
    switch (proto_type) {
    case GW_PROTO_DETECT_MCP:     return "MCP";
    case GW_PROTO_DETECT_A2A:     return "A2A";
    case GW_PROTO_DETECT_OPENAI:  return "OpenAI";
    case GW_PROTO_DETECT_JSONRPC: return "JSON-RPC";
    default:                      return "Unknown";
    }
}

typedef struct {
    gw_proto_detect_result_t proto_type;
    gw_proto_request_handler_t handler;
    void *user_data;
    bool registered;
} gw_proto_adapter_entry_t;

struct gw_proto_router {
    gw_proto_adapter_entry_t adapters[GW_PROTO_MAX_ADAPTERS];
    size_t adapter_count;
    gw_proto_router_stats_t stats;
    bool initialized;
    bool healthy;

    gw_mcp_server_t *mcp_server;
    gw_a2a_handler_t *a2a_handler;
    gw_openai_compat_t *openai_compat;
};

static gw_proto_detect_result_t detect_from_content_type(const char *content_type)
{
    if (!content_type)
        return GW_PROTO_DETECT_UNKNOWN;
    if (strstr(content_type, "application/json")) {
        return GW_PROTO_DETECT_JSONRPC;
    }
    if (strstr(content_type, "text/event-stream")) {
        return GW_PROTO_DETECT_OPENAI;
    }
    return GW_PROTO_DETECT_UNKNOWN;
}

static gw_proto_detect_result_t detect_from_path(const char *path)
{
    if (!path)
        return GW_PROTO_DETECT_UNKNOWN;
    if (strncmp(path, "/mcp", 4) == 0)
        return GW_PROTO_DETECT_MCP;
    if (strncmp(path, "/a2a", 4) == 0)
        return GW_PROTO_DETECT_A2A;
    if (strncmp(path, "/v1/", 4) == 0)
        return GW_PROTO_DETECT_OPENAI;
    if (strncmp(path, "/openai", 7) == 0)
        return GW_PROTO_DETECT_OPENAI;
    if (strncmp(path, "/api/", 5) == 0)
        return GW_PROTO_DETECT_JSONRPC;
    return GW_PROTO_DETECT_UNKNOWN;
}

static gw_proto_detect_result_t detect_from_body(const char *body)
{
    if (!body)
        return GW_PROTO_DETECT_UNKNOWN;

    if (strstr(body, "\"jsonrpc\"") && strstr(body, "\"2.0\"")) {
        if (strstr(body, "\"tools/") || strstr(body, "\"resources/") ||
            strstr(body, "\"prompts/")) {
            return GW_PROTO_DETECT_MCP;
        }
        return GW_PROTO_DETECT_JSONRPC;
    }

    if (strstr(body, "\"model\"") && strstr(body, "\"messages\"")) {
        return GW_PROTO_DETECT_OPENAI;
    }

    if (strstr(body, "\"agentCard\"") || strstr(body, "\"task\"") || strstr(body, "\"a2a\"")) {
        return GW_PROTO_DETECT_A2A;
    }

    return GW_PROTO_DETECT_UNKNOWN;
}

gw_proto_router_t *gw_proto_router_create(void)
{
    gw_proto_router_t *router = (gw_proto_router_t *)AGENTOS_CALLOC(1, sizeof(gw_proto_router_t));
    if (!router) {
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "validation failed");

        return NULL;
    }
    router->adapter_count = 0;
    router->initialized = false;
    router->healthy = false;
    router->mcp_server = NULL;
    router->a2a_handler = NULL;
    router->openai_compat = NULL;
    return router;
}

void gw_proto_router_destroy(gw_proto_router_t *router)
{
    if (!router)
        return;
    if (router->initialized) {
        gw_proto_router_shutdown(router);
    }
    AGENTOS_FREE(router);
}

int gw_proto_router_init(gw_proto_router_t *router)
{
    if (!router)
        return AGENTOS_ERR_INVALID_PARAM;
    if (router->initialized)
        return 0;

    __builtin_memset(&router->stats, 0, sizeof(router->stats));

    gw_mcp_server_config_t mcp_cfg = GW_MCP_SERVER_CONFIG_DEFAULTS;
    router->mcp_server = gw_mcp_server_create(&mcp_cfg);
    if (router->mcp_server) {
        gw_mcp_server_init(router->mcp_server);
        gw_proto_router_register(router, GW_PROTO_DETECT_MCP,
                                 gw_mcp_server_get_handler(router->mcp_server),
                                 gw_mcp_server_get_handler_data(router->mcp_server));
    }

    gw_a2a_handler_config_t a2a_cfg = GW_A2A_HANDLER_CONFIG_DEFAULTS;
    router->a2a_handler = gw_a2a_handler_create(&a2a_cfg);
    if (router->a2a_handler) {
        gw_a2a_handler_init(router->a2a_handler);
        gw_proto_router_register(router, GW_PROTO_DETECT_A2A,
                                 gw_a2a_handler_get_handler(router->a2a_handler),
                                 gw_a2a_handler_get_handler_data(router->a2a_handler));
    }

    gw_openai_compat_config_t openai_cfg = GW_OPENAI_COMPAT_CONFIG_DEFAULTS;
    router->openai_compat = gw_openai_compat_create(&openai_cfg);
    if (router->openai_compat) {
        gw_openai_compat_init(router->openai_compat);
        gw_proto_router_register(router, GW_PROTO_DETECT_OPENAI,
                                 gw_openai_compat_get_handler(router->openai_compat),
                                 gw_openai_compat_get_handler_data(router->openai_compat));
    }

    router->initialized = true;
    router->healthy = true;
    return 0;
}

int gw_proto_router_shutdown(gw_proto_router_t *router)
{
    if (!router || !router->initialized)
        return AGENTOS_ERR_INVALID_PARAM;

    if (router->mcp_server) {
        gw_mcp_server_shutdown(router->mcp_server);
        gw_mcp_server_destroy(router->mcp_server);
        router->mcp_server = NULL;
    }
    if (router->a2a_handler) {
        gw_a2a_handler_shutdown(router->a2a_handler);
        gw_a2a_handler_destroy(router->a2a_handler);
        router->a2a_handler = NULL;
    }
    if (router->openai_compat) {
        gw_openai_compat_shutdown(router->openai_compat);
        gw_openai_compat_destroy(router->openai_compat);
        router->openai_compat = NULL;
    }

    router->adapter_count = 0;
    router->initialized = false;
    router->healthy = false;
    return 0;
}

gw_proto_detect_result_t gw_proto_detect(const char *content_type, const char *path,
                                         const char *body)
{
    gw_proto_detect_result_t result;

    result = detect_from_path(path);
    if (result != GW_PROTO_DETECT_UNKNOWN)
        return result;

    result = detect_from_content_type(content_type);
    if (result != GW_PROTO_DETECT_UNKNOWN)
        return result;

    result = detect_from_body(body);
    return result;
}

int gw_proto_router_register(gw_proto_router_t *router, gw_proto_detect_result_t proto_type,
                             gw_proto_request_handler_t handler, void *user_data)
{
    if (!router || !handler)
        return AGENTOS_ERR_INVALID_PARAM;
    if (router->adapter_count >= GW_PROTO_MAX_ADAPTERS)
        return AGENTOS_ERR_OVERFLOW;

    for (size_t i = 0; i < router->adapter_count; i++) {
        if (router->adapters[i].proto_type == proto_type) {
            router->adapters[i].handler = handler;
            router->adapters[i].user_data = user_data;
            router->adapters[i].registered = true;
            return 0;
        }
    }

    gw_proto_adapter_entry_t *entry = &router->adapters[router->adapter_count];
    entry->proto_type = proto_type;
    entry->handler = handler;
    entry->user_data = user_data;
    entry->registered = true;
    router->adapter_count++;
    return 0;
}

static gw_proto_request_handler_t
find_handler(gw_proto_router_t *router, gw_proto_detect_result_t proto_type, void **out_user_data)
{
    for (size_t i = 0; i < router->adapter_count; i++) {
        if (router->adapters[i].proto_type == proto_type && router->adapters[i].registered) {
            if (out_user_data)
                *out_user_data = router->adapters[i].user_data;
            return router->adapters[i].handler;
        }
    }
    AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OVERFLOW, "limit exceeded");
    return NULL;
}

/**
 * @brief 统计协议请求计数
 *
 * 根据协议类型递增对应的统计计数器，并输出详细调试日志。
 *
 * @param stats 统计结构体指针
 * @param proto_type 协议类型
 */
static void record_proto_stats(gw_proto_router_stats_t *stats, gw_proto_detect_result_t proto_type)
{
    const char *name = proto_type_name(proto_type);

    switch (proto_type) {
    case GW_PROTO_DETECT_MCP:
        stats->mcp_requests++;
        AGENTOS_LOG_DEBUG("protocol=%-8s count=%llu (mcp_requests)", name,
                          (unsigned long long)stats->mcp_requests);
        break;
    case GW_PROTO_DETECT_A2A:
        stats->a2a_requests++;
        AGENTOS_LOG_DEBUG("protocol=%-8s count=%llu (a2a_requests)", name,
                          (unsigned long long)stats->a2a_requests);
        break;
    case GW_PROTO_DETECT_OPENAI:
        stats->openai_requests++;
        AGENTOS_LOG_DEBUG("protocol=%-8s count=%llu (openai_requests)", name,
                          (unsigned long long)stats->openai_requests);
        break;
    case GW_PROTO_DETECT_JSONRPC:
        stats->jsonrpc_requests++;
        AGENTOS_LOG_DEBUG("protocol=%-8s count=%llu (jsonrpc_requests)", name,
                          (unsigned long long)stats->jsonrpc_requests);
        break;
    default:
        stats->unknown_requests++;
        AGENTOS_LOG_DEBUG("protocol=%-8s count=%llu (unknown_requests)", name,
                          (unsigned long long)stats->unknown_requests);
        break;
    }
}

int gw_proto_router_route(gw_proto_router_t *router, gw_proto_detect_result_t proto_type,
                          const char *method, const char *path, const char *body_json,
                          char **response_json)
{
    if (!router || !method || !response_json)
        return AGENTOS_ERR_INVALID_PARAM;

    router->stats.total_requests++;

    void *user_data = NULL;
    gw_proto_request_handler_t handler = find_handler(router, proto_type, &user_data);

    if (!handler) {
        AGENTOS_LOG_WARN("no handler found for protocol type=%d, route_errors=%llu",
                         proto_type, (unsigned long long)router->stats.route_errors);
        router->stats.route_errors++;
        record_proto_stats(&router->stats, proto_type);
        return AGENTOS_ERR_NOT_FOUND;
    }

    record_proto_stats(&router->stats, proto_type);

    int result = handler(method, path, body_json, response_json, user_data);
    if (result != 0) {
        AGENTOS_LOG_WARN("handler returned error: proto_type=%d, result=%d, path=%s",
                         proto_type, result, path ? path : "(null)");
        router->stats.route_errors++;
    }
    return result;
}

int gw_proto_router_route_auto(gw_proto_router_t *router, const char *content_type,
                               const char *method, const char *path, const char *body_json,
                               char **response_json)
{
    if (!router)
        return AGENTOS_ERR_INVALID_PARAM;

    gw_proto_detect_result_t proto = gw_proto_detect(content_type, path, body_json);
    return gw_proto_router_route(router, proto, method, path, body_json, response_json);
}

int gw_proto_router_get_stats(gw_proto_router_t *router, gw_proto_router_stats_t *stats)
{
    if (!router || !stats)
        return AGENTOS_ERR_INVALID_PARAM;
    *stats = router->stats;
    return 0;
}

bool gw_proto_router_is_healthy(gw_proto_router_t *router)
{
    if (!router)
        return false;
    return router->healthy;
}
