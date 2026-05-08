/**
 * @file service.c
 * @brief 网关服务核心实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "gateway_service.h"
#include "platform.h"
#ifdef GATEWAY_HAS_HTTP
#include "http_gateway.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef enum {
    GW_STATE_CREATED = 0,
    GW_STATE_INITIALIZED,
    GW_STATE_RUNNING,
    GW_STATE_STOPPED
} gw_state_t;

struct gateway_service_s {
    gateway_service_config_t config;
    gw_state_t state;
    uint64_t requests_total;
    uint64_t requests_failed;
#ifdef GATEWAY_HAS_HTTP
    gateway_t* http_gateway;
#endif
};

void gateway_service_get_default_config(gateway_service_config_t* config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));

    config->name = "agentos-gateway";
    config->version = "0.0.4";

    config->http.type = GATEWAY_DAEMON_TYPE_HTTP;
    config->http.host = "0.0.0.0";
    config->http.port = 8080;
    config->http.enabled = true;
    config->http.max_request_size = 1048576;
    config->http.timeout_ms = 30000;

    config->ws.type = GATEWAY_DAEMON_TYPE_WS;
    config->ws.host = "0.0.0.0";
    config->ws.port = 8081;
    config->ws.enabled = true;
    config->ws.max_request_size = 1048576;
    config->ws.timeout_ms = 30000;

    config->stdio.type = GATEWAY_DAEMON_TYPE_STDIO;
    config->stdio.enabled = true;
    config->stdio.max_request_size = 1048576;
    config->stdio.timeout_ms = 30000;

    config->enable_metrics = true;
    config->enable_tracing = false;
    config->shutdown_timeout_ms = 5000;
}

agentos_error_t gateway_service_load_config(
    gateway_service_config_t* config,
    const char* config_path) {
    if (!config) return AGENTOS_EINVAL;
    gateway_service_get_default_config(config);
    if (!config_path || config_path[0] == '\0') return AGENTOS_SUCCESS;

    FILE* f = fopen(config_path, "r");
    if (!f) return AGENTOS_SUCCESS;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char* nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char* cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        if (line[0] == '#' || line[0] == '\0') continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;

        if (strcmp(key, "http.port") == 0) {
            config->http.port = (uint16_t)atoi(val);
        } else if (strcmp(key, "http.host") == 0) {
            free((void*)config->http.host);
            config->http.host = strdup(val);
        } else if (strcmp(key, "http.enabled") == 0) {
            config->http.enabled = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        } else if (strcmp(key, "stdio.max_request_size") == 0) {
            config->stdio.max_request_size = (size_t)atol(val);
        } else if (strcmp(key, "stdio.timeout_ms") == 0) {
            config->stdio.timeout_ms = (uint32_t)atoi(val);
        } else if (strcmp(key, "enable_metrics") == 0) {
            config->enable_metrics = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        } else if (strcmp(key, "enable_tracing") == 0) {
            config->enable_tracing = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        } else if (strcmp(key, "shutdown_timeout_ms") == 0) {
            config->shutdown_timeout_ms = (uint32_t)atoi(val);
        }
    }

    fclose(f);
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_service_create(
    gateway_service_t* service,
    const gateway_service_config_t* config) {
    if (!service) return AGENTOS_EINVAL;
    gateway_service_t svc = (gateway_service_t)calloc(1, sizeof(struct gateway_service_s));
    if (!svc) return AGENTOS_ENOMEM;
    if (config) {
        memcpy(&svc->config, config, sizeof(gateway_service_config_t));
    } else {
        gateway_service_get_default_config(&svc->config);
    }
    svc->state = GW_STATE_CREATED;
    *service = svc;
    return AGENTOS_SUCCESS;
}

void gateway_service_destroy(gateway_service_t service) {
    if (!service) return;
    if (service->state == GW_STATE_RUNNING) {
        gateway_service_stop(service, true);
    }
#ifdef GATEWAY_HAS_HTTP
    if (service->http_gateway) {
        gateway_destroy(service->http_gateway);
    }
#endif
    free(service);
}

agentos_error_t gateway_service_init(gateway_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    if (service->state != GW_STATE_CREATED) return AGENTOS_EPERM;
    service->state = GW_STATE_INITIALIZED;
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_service_start(gateway_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    if (service->state == GW_STATE_RUNNING) return AGENTOS_SUCCESS;
    if (service->state != GW_STATE_INITIALIZED && service->state != GW_STATE_STOPPED) {
        return AGENTOS_EPERM;
    }
    service->state = GW_STATE_RUNNING;

#ifdef GATEWAY_HAS_HTTP
    if (service->config.http.enabled) {
        service->http_gateway = http_gateway_create(
            service->config.http.host,
            service->config.http.port);
        if (!service->http_gateway) {
            service->state = GW_STATE_STOPPED;
            return AGENTOS_ENOMEM;
        }
        gateway_start(service->http_gateway);
    }
#endif
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_service_stop(gateway_service_t service, bool force __attribute__((unused))) {
    if (!service) return AGENTOS_EINVAL;
    if (service->state != GW_STATE_RUNNING) return AGENTOS_SUCCESS;
#ifdef GATEWAY_HAS_HTTP
    if (service->http_gateway) {
        gateway_destroy(service->http_gateway);
        service->http_gateway = NULL;
    }
#endif
    service->state = GW_STATE_STOPPED;
    return AGENTOS_SUCCESS;
}

agentos_svc_state_t gateway_service_get_state(gateway_service_t service) {
    if (!service) return AGENTOS_SVC_STATE_NONE;
    switch (service->state) {
        case GW_STATE_CREATED:     return AGENTOS_SVC_STATE_CREATED;
        case GW_STATE_INITIALIZED: return AGENTOS_SVC_STATE_READY;
        case GW_STATE_RUNNING:     return AGENTOS_SVC_STATE_RUNNING;
        case GW_STATE_STOPPED:     return AGENTOS_SVC_STATE_STOPPED;
        default:                   return AGENTOS_SVC_STATE_ERROR;
    }
}

bool gateway_service_is_running(gateway_service_t service) {
    if (!service) return false;
    return service->state == GW_STATE_RUNNING;
}

agentos_error_t gateway_service_get_stats(
    gateway_service_t service,
    agentos_svc_stats_t* stats) {
    if (!service || !stats) return AGENTOS_EINVAL;
    memset(stats, 0, sizeof(*stats));
    stats->request_count = service->requests_total;
    stats->error_count = service->requests_failed;
    return AGENTOS_SUCCESS;
}

agentos_error_t gateway_service_healthcheck(gateway_service_t service) {
    if (!service) return AGENTOS_EINVAL;
    return (service->state == GW_STATE_RUNNING) ? AGENTOS_SUCCESS : AGENTOS_EALREADY;
}
