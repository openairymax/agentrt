// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
/**
 * @file service_discovery_helper.c
 * @brief C-L08: ServiceDiscovery → daemon 自动注册便捷层实现
 *
 * 封装 service_discovery 核心 API，提供 daemon 一键注册、心跳管理、
 * 服务发现和负载均衡选择的便捷接口。
 *
 * @see service_discovery_helper.h
 * @see P1.7 C-L08 连接线
 */

#include "service_discovery_helper.h"

#include "memory_compat.h"
#include "platform.h"
#include "safe_string_utils.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 内部常量 ==================== */

#define SDH_MAX_ENDPOINT_LEN 256
#define SDH_DEFAULT_TTL_MS   30000

/* ==================== 内部数据结构 ==================== */

struct sd_helper_s {
    service_discovery_t sd;           /* 底层服务发现实例 */
    sd_config_t           config;     /* 配置副本 */

    /* 注册信息（用于心跳和注销） */
    char service_name[SD_MAX_NAME_LEN];
    char instance_id[SD_MAX_NAME_LEN];
    bool registered;

    /* 心跳线程 */
    agentos_thread_t heartbeat_thread;  /* 平台线程句柄 */
    volatile bool heartbeat_running;
    uint32_t heartbeat_interval_ms;
};

/* ==================== 心跳线程 ==================== */

static void *sd_helper_heartbeat_loop(void *arg) {
    sd_helper_t *sdh = (sd_helper_t *)arg;
    if (!sdh) return NULL;

    SVC_LOG_INFO("Heartbeat thread started for service '%s' (interval=%ums)",
                 sdh->service_name, sdh->heartbeat_interval_ms);

    while (sdh->heartbeat_running) {
        agentos_sleep_ms(sdh->heartbeat_interval_ms);

        if (!sdh->heartbeat_running) break;

        if (sdh->registered && sdh->service_name[0] != '\0') {
            agentos_error_t err = sd_heartbeat(sdh->sd, sdh->service_name,
                                               sdh->instance_id);
            if (err != AGENTOS_SUCCESS) {
                SVC_LOG_WARN("Heartbeat failed for service '%s' (err=%d)",
                             sdh->service_name, err);
            }
        }
    }

    SVC_LOG_INFO("Heartbeat thread stopped for service '%s'", sdh->service_name);
    return NULL;
}

/* ==================== 生命周期 ==================== */

sd_helper_t *sd_helper_init(const sd_config_t *config) {
    sd_helper_t *sdh = (sd_helper_t *)AGENTOS_CALLOC(1, sizeof(sd_helper_t));
    if (!sdh) {
        SVC_LOG_ERROR("Failed to allocate sd_helper_t");
        return NULL;
    }

    /* 复制或使用默认配置 */
    if (config) {
        AGENTOS_MEMCPY(&sdh->config, config, sizeof(sd_config_t));
    } else {
        sdh->config = sd_create_default_config();
    }

    sdh->heartbeat_interval_ms = sdh->config.heartbeat_interval_ms;

    /* 创建底层服务发现实例 */
    sdh->sd = sd_create(&sdh->config);
    if (!sdh->sd) {
        SVC_LOG_ERROR("Failed to create service_discovery instance");
        AGENTOS_FREE(sdh);
        return NULL;
    }

    /* 启动服务发现 */
    agentos_error_t err = sd_start(sdh->sd);
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to start service_discovery (err=%d)", err);
        sd_destroy(sdh->sd);
        AGENTOS_FREE(sdh);
        return NULL;
    }

    sdh->registered = false;
    sdh->heartbeat_running = false;
    sdh->heartbeat_thread = AGENTOS_INVALID_THREAD;

    SVC_LOG_INFO("Service discovery helper initialized");
    return sdh;
}

void sd_helper_shutdown(sd_helper_t *sdh) {
    if (!sdh) return;

    /* 停止心跳 */
    sd_helper_stop_heartbeat(sdh);

    /* 注销服务 */
    if (sdh->registered && sdh->service_name[0] != '\0') {
        sd_deregister(sdh->sd, sdh->service_name, sdh->instance_id);
        sdh->registered = false;
        SVC_LOG_INFO("Service '%s' deregistered", sdh->service_name);
    }

    /* 销毁底层实例 */
    sd_destroy(sdh->sd);
    sdh->sd = NULL;

    AGENTOS_FREE(sdh);
    SVC_LOG_INFO("Service discovery helper shutdown complete");
}

/* ==================== 服务注册（P1.7.1） ==================== */

static void build_endpoint(char *buf, size_t buf_size,
                           const char *host, uint16_t port) {
    snprintf(buf, buf_size, "%s:%u", host, (unsigned int)port);
}

static void build_instance_id(char *buf, size_t buf_size,
                              const char *name, const char *endpoint) {
    uint32_t pid = 0;
#ifdef _WIN32
    pid = (uint32_t)GetCurrentProcessId();
#else
    pid = (uint32_t)getpid();
#endif
    snprintf(buf, buf_size, "%s-%s-%u", name, endpoint, pid);
}

int sd_helper_register(sd_helper_t *sdh, const char *name, const char *type,
                       const char *host, uint16_t port, const char *tags,
                       uint32_t ttl_ms) {
    if (!sdh || !name || !type || !host) return -1;

    char endpoint[SDH_MAX_ENDPOINT_LEN];
    build_endpoint(endpoint, sizeof(endpoint), host, port);

    char instance_id[SD_MAX_NAME_LEN];
    build_instance_id(instance_id, sizeof(instance_id), name, endpoint);

    /* 填充实例信息 */
    sd_instance_t inst;
    AGENTOS_MEMSET(&inst, 0, sizeof(inst));
    safe_strcpy(inst.instance_id, instance_id, sizeof(inst.instance_id));
    safe_strcpy(inst.endpoint, endpoint, sizeof(inst.endpoint));
    inst.state = AGENTOS_SVC_STATE_RUNNING;
    inst.healthy = true;
    inst.weight = 100;
    inst.active_connections = 0;
    inst.max_connections = 1024;
    inst.last_heartbeat = agentos_platform_get_time_ms();
    inst.register_time = agentos_platform_get_time_ms();
    inst.pid = 0;
#ifdef _WIN32
    inst.pid = (uint32_t)GetCurrentProcessId();
#else
    inst.pid = (uint32_t)getpid();
#endif

    /* 使用配置的 TTL 或默认值 */
    uint32_t effective_ttl = ttl_ms > 0 ? ttl_ms : SDH_DEFAULT_TTL_MS;
    (void)effective_ttl; /* TTL 由 sd_config 的 expire_timeout_ms 控制 */

    agentos_error_t err = sd_register(sdh->sd, name, type, &inst,
                                      tags ? tags : "", "");
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to register service '%s' (err=%d)", name, err);
        return -1;
    }

    /* 保存注册信息 */
    safe_strcpy(sdh->service_name, name, sizeof(sdh->service_name));
    safe_strcpy(sdh->instance_id, instance_id, sizeof(sdh->instance_id));
    sdh->registered = true;

    SVC_LOG_INFO("Service '%s' registered (type=%s, endpoint=%s, instance=%s)",
                 name, type, endpoint, instance_id);
    return 0;
}

int sd_helper_register_unix(sd_helper_t *sdh, const char *name, const char *type,
                            const char *socket_path, const char *tags,
                            uint32_t ttl_ms) {
    if (!sdh || !name || !type || !socket_path) return -1;

    char instance_id[SD_MAX_NAME_LEN];
    uint32_t pid = 0;
#ifdef _WIN32
    pid = (uint32_t)GetCurrentProcessId();
#else
    pid = (uint32_t)getpid();
#endif
    snprintf(instance_id, sizeof(instance_id), "%s-%s-%u", name, socket_path, pid);

    sd_instance_t inst;
    AGENTOS_MEMSET(&inst, 0, sizeof(inst));
    safe_strcpy(inst.instance_id, instance_id, sizeof(inst.instance_id));
    safe_strcpy(inst.endpoint, socket_path, sizeof(inst.endpoint));
    inst.state = AGENTOS_SVC_STATE_RUNNING;
    inst.healthy = true;
    inst.weight = 100;
    inst.active_connections = 0;
    inst.max_connections = 1024;
    inst.last_heartbeat = agentos_platform_get_time_ms();
    inst.register_time = agentos_platform_get_time_ms();
    inst.pid = pid;

    agentos_error_t err = sd_register(sdh->sd, name, type, &inst,
                                      tags ? tags : "", "");
    if (err != AGENTOS_SUCCESS) {
        SVC_LOG_ERROR("Failed to register Unix service '%s' (err=%d)", name, err);
        return -1;
    }

    safe_strcpy(sdh->service_name, name, sizeof(sdh->service_name));
    safe_strcpy(sdh->instance_id, instance_id, sizeof(sdh->instance_id));
    sdh->registered = true;

    SVC_LOG_INFO("Service '%s' registered (type=%s, socket=%s, instance=%s)",
                 name, type, socket_path, instance_id);
    return 0;
}

/* ==================== 心跳管理（P1.7.2） ==================== */

int sd_helper_start_heartbeat(sd_helper_t *sdh) {
    if (!sdh) return -1;
    if (sdh->heartbeat_running) return 0; /* 已在运行 */

    if (!sdh->registered) {
        SVC_LOG_WARN("Cannot start heartbeat: service not registered");
        return -1;
    }

    sdh->heartbeat_running = true;

    /* 创建心跳线程 */
    if (agentos_thread_create(&sdh->heartbeat_thread,
                              sd_helper_heartbeat_loop, sdh) != 0) {
        sdh->heartbeat_running = false;
        SVC_LOG_ERROR("Failed to create heartbeat thread");
        return -1;
    }

    SVC_LOG_INFO("Heartbeat started for service '%s'", sdh->service_name);
    return 0;
}

void sd_helper_stop_heartbeat(sd_helper_t *sdh) {
    if (!sdh || !sdh->heartbeat_running) return;

    sdh->heartbeat_running = false;

    if (sdh->heartbeat_thread != AGENTOS_INVALID_THREAD) {
        agentos_thread_join(sdh->heartbeat_thread, NULL);
        sdh->heartbeat_thread = AGENTOS_INVALID_THREAD;
    }

    SVC_LOG_INFO("Heartbeat stopped for service '%s'", sdh->service_name);
}

int sd_helper_send_heartbeat(sd_helper_t *sdh) {
    if (!sdh || !sdh->registered) return -1;

    agentos_error_t err = sd_heartbeat(sdh->sd, sdh->service_name,
                                       sdh->instance_id);
    return (err == AGENTOS_SUCCESS) ? 0 : -1;
}

/* ==================== 服务发现（P1.7.3） ==================== */

int sd_helper_find(sd_helper_t *sdh, const char *service_name,
                   sd_instance_t *instances, uint32_t max_count,
                   uint32_t *found_count) {
    if (!sdh || !service_name || !instances || !found_count) return -1;

    agentos_error_t err = sd_discover(sdh->sd, service_name,
                                      instances, max_count, found_count);
    return (err == AGENTOS_SUCCESS) ? 0 : -1;
}

/* ==================== 负载均衡选择（P1.7.4） ==================== */

int sd_helper_select(sd_helper_t *sdh, const char *service_name,
                     sd_instance_t *instance) {
    return sd_helper_select_with_strategy(sdh, service_name,
                                          sdh->config.default_lb_strategy,
                                          instance);
}

int sd_helper_select_with_strategy(sd_helper_t *sdh, const char *service_name,
                                   sd_lb_strategy_t strategy,
                                   sd_instance_t *instance) {
    if (!sdh || !service_name || !instance) return -1;

    agentos_error_t err = sd_select_instance(sdh->sd, service_name,
                                             strategy, instance);
    return (err == AGENTOS_SUCCESS) ? 0 : -1;
}

/* ==================== 状态查询 ==================== */

service_discovery_t sd_helper_get_sd(sd_helper_t *sdh) {
    return sdh ? sdh->sd : NULL;
}

bool sd_helper_is_running(sd_helper_t *sdh) {
    if (!sdh) return false;
    return sd_is_running(sdh->sd);
}

uint32_t sd_helper_service_count(sd_helper_t *sdh) {
    if (!sdh) return 0;
    return sd_service_count(sdh->sd);
}

/* ==================== C-L08: 统计摘要 ==================== */

void sd_helper_dump_stats(sd_helper_t *sdh)
{
    if (!sdh || !sdh->sd) {
        SVC_LOG_WARN("C-L08: SD-HELPER-STATS unavailable");
        return;
    }
    sd_dump_stats(sdh->sd);
}