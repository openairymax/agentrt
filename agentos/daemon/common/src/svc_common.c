// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file svc_common.c
 * @brief 服务公共实现 - 统一服务管理框架
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 svc_common.h 中定义的统一服务管理接口。
 * 本模块为 AgentOS daemon 模块提供标准化的服务生命周期管理、
 * 状态监控、健康检查和统计收集功能。
 *
 * 设计原则：
 * 1. 统一的服务接口定义（K-2 接口契约化）
 * 2. 明确的生命周期管理
 * 3. 标准化的错误处理（E-6 错误可追溯）
 * 4. 线程安全的实现（E-5 并发安全）
 *
 * @see agentos/daemon/common/include/svc_common.h
 * @see Service_Management_Framework_Design.md
 */

#include "svc_common.h"
#include "svc_logger.h"
#include "platform.h"
#include "error.h"
#include "ipc_client.h"
#include "safe_string_utils.h"
#include "thread_pool.h"

#include "include/memory_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef AGENTOS_HAS_CURL
#include <curl/curl.h>
#endif

/* ==================== 内部常量 ==================== */

#define MAX_SERVICE_NAME_LEN    64
#define MAX_SERVICE_VERSION_LEN 32
#define MAX_SERVICES            256
#define DEFAULT_HEALTHCHECK_INTERVAL_MS 5000  /* 5秒健康检查间隔 */

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 服务实例内部结构
 */
typedef struct agentos_service_internal {
    /* 基本信息 */
    char name[MAX_SERVICE_NAME_LEN];
    char version[MAX_SERVICE_VERSION_LEN];
    
    /* 状态管理 */
    agentos_svc_state_t state;
    agentos_platform_mutex_t state_mutex;
    
    /* 配置 */
    agentos_svc_config_t config;
    uint32_t capabilities;
    
    /* 统计信息 */
    agentos_svc_stats_t stats;
    agentos_platform_mutex_t stats_mutex;
    
    /* 接口 */
    agentos_svc_interface_t iface;
    
    /* 健康检查状态 */
    uint64_t last_healthcheck_time;
    int healthcheck_failures;
    
    /* 用户上下文数据 */
    void* user_data;

    /* 并发支持 */
    void* thread_pool;

    /* 链表支持 */
    struct agentos_service_internal* next;
} agentos_service_internal_t;

/**
 * @brief 服务注册表内部状态
 */
static struct {
    agentos_service_internal_t* services;           /* 服务链表头 */
    agentos_platform_mutex_t registry_mutex;        /* 注册表互斥锁 */
    uint32_t service_count;                         /* 当前服务数 */
    int initialized;                                /* 模块初始化标志 */
} g_registry = { 
    .services = NULL,
    .service_count = 0,
    .initialized = 0 
};

/* ==================== 辅助函数 ==================== */

/**
 * @brief 初始化服务管理模块
 */
static agentos_error_t svc_common_module_init(void) {
    if (g_registry.initialized) {
        return AGENTOS_SUCCESS;
    }
    
    agentos_error_t err = AGENTOS_SUCCESS;
    
    /* 初始化注册表互斥锁 */
    err = agentos_platform_mutex_init(&g_registry.registry_mutex);
    if (err != AGENTOS_SUCCESS) {
        LOG_ERROR("Failed to initialize registry mutex: %d", err);
        return AGENTOS_EINIT;
    }
    
    g_registry.initialized = 1;
    LOG_DEBUG("Service common module initialized");
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理服务管理模块
 */
static void svc_common_module_cleanup(void) {
    if (!g_registry.initialized) {
        return;
    }
    
    /* 注意：不在这里销毁服务，应由调用者负责 */
    agentos_platform_mutex_destroy(&g_registry.registry_mutex);
    g_registry.initialized = 0;
    
    LOG_DEBUG("Service common module cleaned up");
}

/**
 * @brief 查找服务内部结构
 */
static agentos_service_internal_t* find_service_internal(const char* name) {
    if (!name || !g_registry.initialized) {
        return NULL;
    }
    
    agentos_service_internal_t* current = g_registry.services;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/**
 * @brief 注册服务到内部注册表
 */
static agentos_error_t register_service_internal(agentos_service_internal_t* service) {
    if (!service || !g_registry.initialized) {
        return AGENTOS_EINVAL;
    }
    
    agentos_platform_mutex_lock(&g_registry.registry_mutex);
    
    /* 检查服务是否已存在 */
    if (find_service_internal(service->name)) {
        agentos_platform_mutex_unlock(&g_registry.registry_mutex);
        return AGENTOS_EEXIST;
    }
    
    /* 添加到链表头部 */
    service->next = g_registry.services;
    g_registry.services = service;
    g_registry.service_count++;
    
    agentos_platform_mutex_unlock(&g_registry.registry_mutex);
    
    LOG_INFO("Service '%s' registered internally", service->name);
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 从内部注册表注销服务
 */
static agentos_error_t unregister_service_internal(agentos_service_internal_t* service) {
    if (!service || !g_registry.initialized) {
        return AGENTOS_EINVAL;
    }
    
    agentos_platform_mutex_lock(&g_registry.registry_mutex);
    
    /* 查找服务并移除 */
    agentos_service_internal_t** prev = &g_registry.services;
    agentos_service_internal_t* current = g_registry.services;
    
    while (current) {
        if (current == service) {
            *prev = current->next;
            g_registry.service_count--;
            
            agentos_platform_mutex_unlock(&g_registry.registry_mutex);
            LOG_INFO("Service '%s' unregistered internally", service->name);
            return AGENTOS_SUCCESS;
        }
        
        prev = &current->next;
        current = current->next;
    }
    
    agentos_platform_mutex_unlock(&g_registry.registry_mutex);
    
    return AGENTOS_ENOENT;  /* 服务未找到 */
}

/**
 * @brief 更新服务统计信息
 */
static void __attribute__((unused)) update_service_stats(agentos_service_internal_t* service, 
                                 bool success, 
                                 uint64_t process_time_ms) {
    if (!service) {
        return;
    }
    
    agentos_platform_mutex_lock(&service->stats_mutex);
    
    service->stats.request_count++;
    
    if (success) {
        service->stats.success_count++;
    } else {
        service->stats.error_count++;
    }
    
    service->stats.total_time_ms += process_time_ms;
    
    if (process_time_ms > service->stats.max_time_ms) {
        service->stats.max_time_ms = process_time_ms;
    }
    
    if (service->stats.min_time_ms == 0 || process_time_ms < service->stats.min_time_ms) {
        service->stats.min_time_ms = process_time_ms;
    }
    
    if (service->stats.request_count > 0) {
        service->stats.avg_time_ms = (double)service->stats.total_time_ms / 
                                      service->stats.request_count;
    }
    
    agentos_platform_mutex_unlock(&service->stats_mutex);
}

/* ==================== 公共API实现 ==================== */

/* 服务生命周期管理 */

agentos_error_t agentos_service_create(
    agentos_service_t* out_service,
    const char* name,
    const agentos_svc_interface_t* iface,
    const agentos_svc_config_t* config) {
    
    if (!out_service || !name || !iface || !config) {
        return AGENTOS_EINVAL;
    }
    
    /* 初始化模块（如果未初始化） */
    agentos_error_t err = svc_common_module_init();
    if (err != AGENTOS_SUCCESS) {
        return err;
    }
    
    /* 检查名称长度 */
    size_t name_len = strlen(name);
    if (name_len == 0 || name_len >= MAX_SERVICE_NAME_LEN) {
        return AGENTOS_EINVAL;
    }
    
    /* 分配服务结构 */
    agentos_service_internal_t* service = 
        (agentos_service_internal_t*)AGENTOS_CALLOC(1, sizeof(agentos_service_internal_t));
    if (!service) {
        return AGENTOS_ENOMEM;
    }
    
    /* 初始化基本信息 */
    if (safe_strcpy(service->name, name, MAX_SERVICE_NAME_LEN) != 0) {
        AGENTOS_FREE(service);
        return AGENTOS_EINVAL;
    }
    
    if (config->version) {
        if (safe_strcpy(service->version, config->version, MAX_SERVICE_VERSION_LEN) != 0) {
            AGENTOS_FREE(service);
            return AGENTOS_EINVAL;
        }
    }
    
    /* 初始化状态 */
    service->state = AGENTOS_SVC_STATE_CREATED;
    err = agentos_platform_mutex_init(&service->state_mutex);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(service);
        return err;
    }
    
    /* 初始化统计互斥锁 */
    err = agentos_platform_mutex_init(&service->stats_mutex);
    if (err != AGENTOS_SUCCESS) {
        agentos_platform_mutex_destroy(&service->state_mutex);
        AGENTOS_FREE(service);
        return err;
    }
    
    /* 复制配置 */
    memcpy(&service->config, config, sizeof(agentos_svc_config_t));
    service->capabilities = config->capabilities;
    
    /* 复制接口 */
    memcpy(&service->iface, iface, sizeof(agentos_svc_interface_t));
    
    /* 初始化统计信息 */
    memset(&service->stats, 0, sizeof(agentos_svc_stats_t));
    
    /* 注册到内部注册表 */
    err = register_service_internal(service);
    if (err != AGENTOS_SUCCESS) {
        agentos_platform_mutex_destroy(&service->stats_mutex);
        agentos_platform_mutex_destroy(&service->state_mutex);
        AGENTOS_FREE(service);
        return err;
    }
    
    *out_service = (agentos_service_t)service;
    
    LOG_INFO("Service '%s' created successfully", name);
    
    return AGENTOS_SUCCESS;
}

void agentos_service_destroy(agentos_service_t svc) {
    if (!svc) {
        return;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    /* 如果服务还在运行，先停止 */
    if (service->state == AGENTOS_SVC_STATE_RUNNING || 
        service->state == AGENTOS_SVC_STATE_PAUSED) {
        agentos_service_stop((agentos_service_t)service, true);  /* 强制停止 */
    }
    
    /* 从注册表注销 */
    unregister_service_internal(service);
    
    /* 调用服务的销毁函数（如果提供） */
    if (service->iface.destroy) {
        service->iface.destroy(svc);
    }
    
    /* 清理资源 */
    agentos_platform_mutex_destroy(&service->state_mutex);
    agentos_platform_mutex_destroy(&service->stats_mutex);
    
    AGENTOS_FREE(service);
    
    LOG_INFO("Service destroyed");
}

agentos_error_t agentos_service_init(agentos_service_t svc) {
    if (!svc) {
        return AGENTOS_EINVAL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    agentos_platform_mutex_lock(&service->state_mutex);
    
    /* 状态检查 */
    if (service->state != AGENTOS_SVC_STATE_CREATED) {
        agentos_platform_mutex_unlock(&service->state_mutex);
        LOG_ERROR("Service '%s' cannot initialize from state %d", 
                 service->name, service->state);
        return AGENTOS_ESTATE;
    }
    
    /* 更新状态 */
    service->state = AGENTOS_SVC_STATE_INITIALIZING;
    agentos_platform_mutex_unlock(&service->state_mutex);
    
    /* 调用服务的初始化函数 */
    agentos_error_t err = AGENTOS_SUCCESS;
    if (service->iface.init) {
        err = service->iface.init(svc, &service->config);
    }
    
    agentos_platform_mutex_lock(&service->state_mutex);
    if (err == AGENTOS_SUCCESS) {
        service->state = AGENTOS_SVC_STATE_READY;
        LOG_INFO("Service '%s' initialized successfully", service->name);
    } else {
        service->state = AGENTOS_SVC_STATE_ERROR;
        LOG_ERROR("Service '%s' initialization failed: %d", service->name, err);
    }
    
    agentos_platform_mutex_unlock(&service->state_mutex);
    
    return err;
}

agentos_error_t agentos_service_start(agentos_service_t svc) {
    if (!svc) {
        return AGENTOS_EINVAL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    agentos_platform_mutex_lock(&service->state_mutex);
    
    /* 状态检查 */
    if (service->state != AGENTOS_SVC_STATE_READY && 
        service->state != AGENTOS_SVC_STATE_STOPPED &&
        service->state != AGENTOS_SVC_STATE_PAUSED) {
        agentos_platform_mutex_unlock(&service->state_mutex);
        LOG_ERROR("Service '%s' cannot start from state %d", 
                 service->name, service->state);
        return AGENTOS_ESTATE;
    }
    
    /* 更新状态 */
    agentos_svc_state_t old_state = service->state;
    service->state = AGENTOS_SVC_STATE_RUNNING;
    agentos_platform_mutex_unlock(&service->state_mutex);
    
    /* 调用服务的启动函数 */
    agentos_error_t err = AGENTOS_SUCCESS;
    if (service->iface.start) {
        err = service->iface.start(svc);
    }
    
    if (err != AGENTOS_SUCCESS) {
        agentos_platform_mutex_lock(&service->state_mutex);
        service->state = old_state;  /* 恢复原状态 */
        agentos_platform_mutex_unlock(&service->state_mutex);
        
        LOG_ERROR("Service '%s' start failed: %d", service->name, err);
        return err;
    }
    
    LOG_INFO("Service '%s' started successfully", service->name);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_service_stop(agentos_service_t svc, bool force) {
    if (!svc) {
        return AGENTOS_EINVAL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    agentos_platform_mutex_lock(&service->state_mutex);
    
    /* 状态检查 */
    if (service->state != AGENTOS_SVC_STATE_RUNNING && 
        service->state != AGENTOS_SVC_STATE_PAUSED) {
        agentos_platform_mutex_unlock(&service->state_mutex);
        LOG_WARN("Service '%s' cannot stop from state %d", 
                service->name, service->state);
        return AGENTOS_ESTATE;
    }
    
    /* 更新状态 */
    service->state = AGENTOS_SVC_STATE_STOPPING;
    agentos_platform_mutex_unlock(&service->state_mutex);
    
    /* 调用服务的停止函数 */
    agentos_error_t err = AGENTOS_SUCCESS;
    if (service->iface.stop) {
        err = service->iface.stop(svc, force);
    }
    
    agentos_platform_mutex_lock(&service->state_mutex);
    if (err == AGENTOS_SUCCESS || force) {
        service->state = AGENTOS_SVC_STATE_STOPPED;
        LOG_INFO("Service '%s' stopped %s", 
                service->name, force ? "(forced)" : "gracefully");
    } else {
        service->state = AGENTOS_SVC_STATE_ERROR;
        LOG_ERROR("Service '%s' stop failed: %d", service->name, err);
    }
    
    agentos_platform_mutex_unlock(&service->state_mutex);
    
    return err;
}

typedef struct {
    agentos_service_t service;
    char* method;
    char* params_json;
    agentos_svc_async_complete_fn on_complete;
    void* user_data;
} async_request_context_t;

static void async_request_worker(void* arg) {
    async_request_context_t* ctx = (async_request_context_t*)arg;
    if (!ctx) return;

    char* response_json = NULL;
    agentos_error_t err = AGENTOS_EINVAL;

    agentos_service_internal_t* svc = (agentos_service_internal_t*)ctx->service;
    if (svc && svc->iface.handle_request) {
        err = svc->iface.handle_request(ctx->service, ctx->method,
                                         ctx->params_json, &response_json,
                                         svc->user_data);
    }

    if (ctx->on_complete) {
        ctx->on_complete(ctx->service, ctx->method, err, response_json, ctx->user_data);
    } else {
        if (response_json) free(response_json);
    }

    free(ctx->method);
    free(ctx->params_json);
    free(ctx);
}

agentos_error_t agentos_service_set_thread_pool(agentos_service_t svc, void* pool) {
    if (!svc) return AGENTOS_EINVAL;
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    service->thread_pool = pool;
    return AGENTOS_SUCCESS;
}

int agentos_service_handle_request_async(
    agentos_service_t service,
    const char* method,
    const char* params_json,
    agentos_svc_async_complete_fn on_complete,
    void* user_data
) {
    if (!service || !method) return -1;

    agentos_service_internal_t* svc = (agentos_service_internal_t*)service;

    async_request_context_t* ctx = (async_request_context_t*)calloc(1, sizeof(*ctx));
    if (!ctx) return -2;

    ctx->service = service;
    ctx->method = strdup(method);
    ctx->params_json = params_json ? strdup(params_json) : NULL;
    ctx->on_complete = on_complete;
    ctx->user_data = user_data;

    if (svc->thread_pool) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
        int rc = thread_pool_submit(svc->thread_pool, async_request_worker, ctx);
#pragma GCC diagnostic pop
        if (rc != 0) {
            free(ctx->method); free(ctx->params_json); free(ctx);
            return rc;
        }
        return 0;
    }

    async_request_worker(ctx);
    return 0;
}

agentos_error_t agentos_service_pause(agentos_service_t svc) {
    if (!svc) {
        return AGENTOS_EINVAL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    agentos_platform_mutex_lock(&service->state_mutex);
    
    /* 状态检查 */
    if (service->state != AGENTOS_SVC_STATE_RUNNING) {
        agentos_platform_mutex_unlock(&service->state_mutex);
        LOG_ERROR("Service '%s' cannot pause from state %d", 
                 service->name, service->state);
        return AGENTOS_ESTATE;
    }
    
    /* 检查是否支持暂停 */
    if (!(service->capabilities & AGENTOS_SVC_CAP_PAUSEABLE)) {
        agentos_platform_mutex_unlock(&service->state_mutex);
        LOG_ERROR("Service '%s' does not support pause", service->name);
        return AGENTOS_ENOTSUP;
    }
    
    /* 更新状态 */
    service->state = AGENTOS_SVC_STATE_PAUSED;
    agentos_platform_mutex_unlock(&service->state_mutex);
    
    LOG_INFO("Service '%s' paused", service->name);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_service_resume(agentos_service_t svc) {
    if (!svc) {
        return AGENTOS_EINVAL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    agentos_platform_mutex_lock(&service->state_mutex);
    
    /* 状态检查 */
    if (service->state != AGENTOS_SVC_STATE_PAUSED) {
        agentos_platform_mutex_unlock(&service->state_mutex);
        LOG_ERROR("Service '%s' cannot resume from state %d", 
                 service->name, service->state);
        return AGENTOS_ESTATE;
    }
    
    /* 更新状态 */
    service->state = AGENTOS_SVC_STATE_RUNNING;
    agentos_platform_mutex_unlock(&service->state_mutex);
    
    LOG_INFO("Service '%s' resumed", service->name);
    
    return AGENTOS_SUCCESS;
}

/* 服务状态查询 */

agentos_svc_state_t agentos_service_get_state(agentos_service_t svc) {
    if (!svc) {
        return AGENTOS_SVC_STATE_NONE;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    agentos_platform_mutex_lock(&service->state_mutex);
    agentos_svc_state_t state = service->state;
    agentos_platform_mutex_unlock(&service->state_mutex);
    
    return state;
}

bool agentos_service_is_ready(agentos_service_t svc) {
    agentos_svc_state_t state = agentos_service_get_state(svc);
    return state == AGENTOS_SVC_STATE_READY;
}

bool agentos_service_is_running(agentos_service_t svc) {
    agentos_svc_state_t state = agentos_service_get_state(svc);
    return state == AGENTOS_SVC_STATE_RUNNING;
}

const char* agentos_service_get_name(agentos_service_t svc) {
    if (!svc) {
        return NULL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    return service->name;
}

const char* agentos_service_get_version(agentos_service_t svc) {
    if (!svc) {
        return NULL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    return service->version[0] ? service->version : "1.0.0";
}

/* 服务统计 */

agentos_error_t agentos_service_get_stats(
    agentos_service_t svc,
    agentos_svc_stats_t* out_stats) {
    
    if (!svc || !out_stats) {
        return AGENTOS_EINVAL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    agentos_platform_mutex_lock(&service->stats_mutex);
    memcpy(out_stats, &service->stats, sizeof(agentos_svc_stats_t));
    agentos_platform_mutex_unlock(&service->stats_mutex);
    
    return AGENTOS_SUCCESS;
}

void agentos_service_reset_stats(agentos_service_t svc) {
    if (!svc) {
        return;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    agentos_platform_mutex_lock(&service->stats_mutex);
    memset(&service->stats, 0, sizeof(agentos_svc_stats_t));
    agentos_platform_mutex_unlock(&service->stats_mutex);
    
    LOG_DEBUG("Service '%s' stats reset", service->name);
}

/* 服务健康检查 */

agentos_error_t agentos_service_healthcheck(agentos_service_t svc) {
    if (!svc) {
        return AGENTOS_EINVAL;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    
    /* 如果服务提供了健康检查函数，则使用它 */
    if (service->iface.healthcheck) {
        agentos_error_t err = service->iface.healthcheck(svc);
        
        /* 更新健康检查状态 */
        uint64_t current_time = agentos_platform_get_time_ms();
        service->last_healthcheck_time = current_time;
        
        if (err != AGENTOS_SUCCESS) {
            service->healthcheck_failures++;
            LOG_WARN("Service '%s' health check failed: %d (failures: %d)",
                    service->name, err, service->healthcheck_failures);
        } else {
            service->healthcheck_failures = 0;
        }
        
        return err;
    }
    
    /* 默认健康检查：检查服务状态 */
    agentos_svc_state_t state = agentos_service_get_state(svc);
    
    switch (state) {
        case AGENTOS_SVC_STATE_READY:
        case AGENTOS_SVC_STATE_RUNNING:
        case AGENTOS_SVC_STATE_PAUSED:
            return AGENTOS_SUCCESS;
            
        case AGENTOS_SVC_STATE_ERROR:
            return AGENTOS_EHEALTH;
            
        default:
            return AGENTOS_ESTATE;
    }
}

/* 服务能力查询 */

bool agentos_service_has_capability(
    agentos_service_t svc,
    agentos_svc_capability_t capability) {
    
    if (!svc) {
        return false;
    }
    
    agentos_service_internal_t* service = (agentos_service_internal_t*)svc;
    return (service->capabilities & capability) != 0;
}

/* 服务状态字符串转换 */

const char* agentos_svc_state_to_string(agentos_svc_state_t state) {
    static const char* state_strings[] = {
        "NONE",
        "CREATED",
        "INITIALIZING",
        "READY",
        "RUNNING",
        "PAUSED",
        "STOPPING",
        "STOPPED",
        "ERROR"
    };
    
    if (state < AGENTOS_SVC_STATE_NONE || state > AGENTOS_SVC_STATE_ERROR) {
        return "UNKNOWN";
    }
    
    return state_strings[state];
}

agentos_svc_state_t agentos_svc_state_from_string(const char* str) {
    if (!str) {
        return AGENTOS_SVC_STATE_NONE;
    }
    
    static const struct {
        const char* name;
        agentos_svc_state_t state;
    } state_map[] = {
        {"NONE", AGENTOS_SVC_STATE_NONE},
        {"CREATED", AGENTOS_SVC_STATE_CREATED},
        {"INITIALIZING", AGENTOS_SVC_STATE_INITIALIZING},
        {"READY", AGENTOS_SVC_STATE_READY},
        {"RUNNING", AGENTOS_SVC_STATE_RUNNING},
        {"PAUSED", AGENTOS_SVC_STATE_PAUSED},
        {"STOPPING", AGENTOS_SVC_STATE_STOPPING},
        {"STOPPED", AGENTOS_SVC_STATE_STOPPED},
        {"ERROR", AGENTOS_SVC_STATE_ERROR},
        {NULL, AGENTOS_SVC_STATE_NONE}
    };
    
    for (int i = 0; state_map[i].name; i++) {
        if (strcasecmp(str, state_map[i].name) == 0) {
            return state_map[i].state;
        }
    }
    
    return AGENTOS_SVC_STATE_NONE;
}

/* 服务注册表 */

agentos_error_t agentos_service_register(agentos_service_t svc) {
    /* 在 agentos_service_create 中已经注册，这里只是兼容性包装 */
    if (!svc) {
        return AGENTOS_EINVAL;
    }
    
    /* 服务已经通过 agentos_service_create 注册到内部注册表 */
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_service_unregister(agentos_service_t svc) {
    /* 实际注销在 agentos_service_destroy 中处理 */
    if (!svc) {
        return AGENTOS_EINVAL;
    }
    
    return AGENTOS_SUCCESS;
}

agentos_service_t agentos_service_find(const char* name) {
    if (!name || !g_registry.initialized) {
        return NULL;
    }
    
    agentos_platform_mutex_lock(&g_registry.registry_mutex);
    
    agentos_service_internal_t* service = find_service_internal(name);
    
    agentos_platform_mutex_unlock(&g_registry.registry_mutex);
    
    return (agentos_service_t)service;
}

uint32_t agentos_service_count(void) {
    if (!g_registry.initialized) {
        return 0;
    }
    
    agentos_platform_mutex_lock(&g_registry.registry_mutex);
    uint32_t count = g_registry.service_count;
    agentos_platform_mutex_unlock(&g_registry.registry_mutex);
    
    return count;
}

void agentos_service_foreach(agentos_service_enum_fn callback, void* user_data) {
    if (!callback || !g_registry.initialized) {
        return;
    }
    
    agentos_platform_mutex_lock(&g_registry.registry_mutex);
    
    agentos_service_internal_t* current = g_registry.services;
    while (current) {
        callback((agentos_service_t)current, user_data);
        current = current->next;
    }
    
    agentos_platform_mutex_unlock(&g_registry.registry_mutex);
}

agentos_error_t agentos_service_set_user_data(agentos_service_t service, void* user_data) {
    if (!service) {
        return AGENTOS_EINVAL;
    }

    agentos_service_internal_t* internal = (agentos_service_internal_t*)service;
    agentos_platform_mutex_lock(&internal->state_mutex);
    internal->user_data = user_data;
    agentos_platform_mutex_unlock(&internal->state_mutex);

    return AGENTOS_SUCCESS;
}

void* agentos_service_get_user_data(agentos_service_t service) {
    if (!service) {
        return NULL;
    }

    agentos_service_internal_t* internal = (agentos_service_internal_t*)service;
    agentos_platform_mutex_lock(&internal->state_mutex);
    void* data = internal->user_data;
    agentos_platform_mutex_unlock(&internal->state_mutex);

    return data;
}

/* ==================== 模块清理 ==================== */

void agentos_svc_common_cleanup(void) {
    svc_common_module_cleanup();
}

/* ==================== 跨进程服务注册中心（Phase 3.2） ==================== */

#define MAX_REGISTRY_ENTRIES 64
#define HEARTBEAT_INTERVAL_MS 30000

typedef struct {
    agentos_service_metadata_t metadata;
    agentos_service_t service;
    bool registered;
    uint64_t register_time;
} registry_entry_t;

static struct {
    char registry_url[512];
    bool initialized;
    registry_entry_t entries[MAX_REGISTRY_ENTRIES];
    uint32_t entry_count;
    agentos_platform_mutex_t mutex;
} g_cross_registry = {0};

agentos_error_t agentos_registry_init(const char* registry_url) {
    if (!registry_url) {
        return AGENTOS_EINVAL;
    }

    agentos_error_t err = AGENTOS_SUCCESS;

    err = agentos_platform_mutex_init(&g_cross_registry.mutex);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    agentos_platform_mutex_lock(&g_cross_registry.mutex);

    if (g_cross_registry.initialized) {
        agentos_platform_mutex_unlock(&g_cross_registry.mutex);
        return AGENTOS_SUCCESS;
    }

    if (safe_strcpy(g_cross_registry.registry_url, registry_url, sizeof(g_cross_registry.registry_url)) != 0) {
        agentos_platform_mutex_unlock(&g_cross_registry.mutex);
        return AGENTOS_EINVAL;
    }
    memset(g_cross_registry.entries, 0, sizeof(g_cross_registry.entries));
    g_cross_registry.entry_count = 0;
    g_cross_registry.initialized = true;

    agentos_platform_mutex_unlock(&g_cross_registry.mutex);

    LOG_INFO("Service registry client initialized: %s", registry_url);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_registry_register(
    agentos_service_t service,
    const agentos_service_metadata_t* metadata
) {
    if (!service || !metadata) {
        return AGENTOS_EINVAL;
    }

    if (!g_cross_registry.initialized) {
        LOG_WARN("Registry not initialized, using local-only registration");
    }

    agentos_platform_mutex_lock(&g_cross_registry.mutex);

    if (g_cross_registry.entry_count >= MAX_REGISTRY_ENTRIES) {
        agentos_platform_mutex_unlock(&g_cross_registry.mutex);
        LOG_ERROR("Registry full, cannot register service '%s'", metadata->name);
        return AGENTOS_ENOMEM;
    }

    for (uint32_t i = 0; i < g_cross_registry.entry_count; i++) {
        if (g_cross_registry.entries[i].service == service) {
            memcpy(&g_cross_registry.entries[i].metadata, metadata, sizeof(agentos_service_metadata_t));
            g_cross_registry.entries[i].metadata.last_heartbeat = agentos_platform_get_time_ms();
            agentos_platform_mutex_unlock(&g_cross_registry.mutex);
            LOG_INFO("Service '%s' re-registered in cross-process registry", metadata->name);
            return AGENTOS_SUCCESS;
        }
    }

    registry_entry_t* entry = &g_cross_registry.entries[g_cross_registry.entry_count];
    memcpy(&entry->metadata, metadata, sizeof(agentos_service_metadata_t));
    entry->service = service;
    entry->registered = true;
    entry->register_time = agentos_platform_get_time_ms();
    entry->metadata.last_heartbeat = entry->register_time;
    g_cross_registry.entry_count++;

    agentos_platform_mutex_unlock(&g_cross_registry.mutex);

    LOG_INFO("Service '%s' registered in cross-process registry (type=%s, endpoint=%s)",
             metadata->name, metadata->service_type, metadata->endpoint);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_registry_deregister(agentos_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }

    agentos_platform_mutex_lock(&g_cross_registry.mutex);

    for (uint32_t i = 0; i < g_cross_registry.entry_count; i++) {
        if (g_cross_registry.entries[i].service == service) {
            LOG_INFO("Service '%s' deregistered from cross-process registry",
                     g_cross_registry.entries[i].metadata.name);

            if (i < g_cross_registry.entry_count - 1) {
                g_cross_registry.entries[i] = g_cross_registry.entries[g_cross_registry.entry_count - 1];
            }
            memset(&g_cross_registry.entries[g_cross_registry.entry_count - 1], 0, sizeof(registry_entry_t));
            g_cross_registry.entry_count--;

            agentos_platform_mutex_unlock(&g_cross_registry.mutex);
            return AGENTOS_SUCCESS;
        }
    }

    agentos_platform_mutex_unlock(&g_cross_registry.mutex);
    return AGENTOS_ENOENT;
}

static bool tag_matches(const char* filter_tags, const char* service_tags) {
    if (!filter_tags || !filter_tags[0]) return true;
    if (!service_tags || !service_tags[0]) return false;

    char filter_copy[AGENTOS_MAX_TAGS_LEN];
    if (safe_strcpy(filter_copy, filter_tags, sizeof(filter_copy)) != 0) {
        return false;
    }

    char* saveptr = NULL;
    char* token = strtok_r(filter_copy, ",", &saveptr);
    while (token) {
        while (*token == ' ') token++;
        if (strstr(service_tags, token)) {
            return true;
        }
        token = strtok_r(NULL, ",", &saveptr);
    }
    return false;
}

agentos_service_metadata_t* agentos_registry_discover(
    const char* service_type,
    const char* filter_tags,
    size_t* result_count
) {
    if (!result_count) {
        return NULL;
    }

    *result_count = 0;

    if (!g_cross_registry.initialized && g_cross_registry.entry_count == 0) {
        return NULL;
    }

    agentos_platform_mutex_lock(&g_cross_registry.mutex);

    size_t match_count = 0;
    for (uint32_t i = 0; i < g_cross_registry.entry_count; i++) {
        registry_entry_t* entry = &g_cross_registry.entries[i];
        if (!entry->registered) continue;

        bool type_match = !service_type || !service_type[0] ||
                          strcmp(entry->metadata.service_type, service_type) == 0;
        bool tag_match = tag_matches(filter_tags, entry->metadata.tags);

        if (type_match && tag_match) {
            match_count++;
        }
    }

    if (match_count == 0) {
        agentos_platform_mutex_unlock(&g_cross_registry.mutex);
        return NULL;
    }

    agentos_service_metadata_t* results = (agentos_service_metadata_t*)AGENTOS_CALLOC(
        match_count, sizeof(agentos_service_metadata_t));
    if (!results) {
        agentos_platform_mutex_unlock(&g_cross_registry.mutex);
        return NULL;
    }

    size_t idx = 0;
    for (uint32_t i = 0; i < g_cross_registry.entry_count && idx < match_count; i++) {
        registry_entry_t* entry = &g_cross_registry.entries[i];
        if (!entry->registered) continue;

        bool type_match = !service_type || !service_type[0] ||
                          strcmp(entry->metadata.service_type, service_type) == 0;
        bool tag_match = tag_matches(filter_tags, entry->metadata.tags);

        if (type_match && tag_match) {
            memcpy(&results[idx], &entry->metadata, sizeof(agentos_service_metadata_t));
            idx++;
        }
    }

    *result_count = match_count;
    agentos_platform_mutex_unlock(&g_cross_registry.mutex);

    LOG_DEBUG("Service discovery: found %zu services (type=%s, tags=%s)",
              match_count, service_type ? service_type : "*", filter_tags ? filter_tags : "*");
    return results;
}

void agentos_registry_discover_free(agentos_service_metadata_t* results) {
    if (results) {
        AGENTOS_FREE(results);
    }
}

agentos_error_t agentos_registry_heartbeat(agentos_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }

    agentos_platform_mutex_lock(&g_cross_registry.mutex);

    for (uint32_t i = 0; i < g_cross_registry.entry_count; i++) {
        if (g_cross_registry.entries[i].service == service) {
            g_cross_registry.entries[i].metadata.last_heartbeat = agentos_platform_get_time_ms();
            g_cross_registry.entries[i].metadata.state = agentos_service_get_state(service);
            g_cross_registry.entries[i].metadata.healthy =
                (agentos_service_healthcheck(service) == AGENTOS_SUCCESS);

            agentos_svc_stats_t stats;
            if (agentos_service_get_stats(service, &stats) == AGENTOS_SUCCESS) {
                g_cross_registry.entries[i].metadata.current_load =
                    stats.current_concurrent > 0 ?
                    (uint32_t)(stats.current_concurrent * 100 / (stats.peak_concurrent > 0 ? stats.peak_concurrent : 1)) :
                    0;
            }

            agentos_platform_mutex_unlock(&g_cross_registry.mutex);
            return AGENTOS_SUCCESS;
        }
    }

    agentos_platform_mutex_unlock(&g_cross_registry.mutex);
    return AGENTOS_ENOENT;
}

void agentos_registry_cleanup(void) {
    agentos_platform_mutex_lock(&g_cross_registry.mutex);

    memset(g_cross_registry.entries, 0, sizeof(g_cross_registry.entries));
    g_cross_registry.entry_count = 0;
    g_cross_registry.initialized = false;
    g_cross_registry.registry_url[0] = '\0';

    agentos_platform_mutex_unlock(&g_cross_registry.mutex);
    agentos_platform_mutex_destroy(&g_cross_registry.mutex);

    LOG_INFO("Service registry client cleaned up");
}

/* ==================== 配置管理（Phase 3.2） ==================== */

#define MAX_CONFIG_WATCHERS 32
#define MAX_CONFIG_PATH_LEN 512

typedef struct {
    char service_name[64];
    agentos_config_change_callback_t callback;
    void* user_data;
    bool active;
} config_watcher_t;

static struct {
    config_watcher_t watchers[MAX_CONFIG_WATCHERS];
    uint32_t watcher_count;
    char config_base_path[MAX_CONFIG_PATH_LEN];
    bool initialized;
    agentos_platform_mutex_t mutex;
} g_config_mgr = {0};

static agentos_error_t config_mgr_init(void) {
    if (g_config_mgr.initialized) {
        return AGENTOS_SUCCESS;
    }

    agentos_error_t err = agentos_platform_mutex_init(&g_config_mgr.mutex);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    memset(g_config_mgr.watchers, 0, sizeof(g_config_mgr.watchers));
    g_config_mgr.watcher_count = 0;
    if (safe_strcpy(g_config_mgr.config_base_path, "./config", sizeof(g_config_mgr.config_base_path)) != 0) {
        agentos_platform_mutex_destroy(&g_config_mgr.mutex);
        return AGENTOS_EINVAL;
    }
    g_config_mgr.initialized = true;

    return AGENTOS_SUCCESS;
}

static void compute_simple_checksum(const char* data, size_t len, char* out, size_t out_size) {
    uint64_t hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + (unsigned char)data[i];
    }
    snprintf(out, out_size, "%016llx", (unsigned long long)hash);
}

agentos_error_t agentos_config_load(
    const char* service_name,
    agentos_config_t** config
) {
    if (!service_name || !config) {
        return AGENTOS_EINVAL;
    }

    config_mgr_init();

    char config_path[MAX_CONFIG_PATH_LEN];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(config_path, sizeof(config_path), "%s/%s.json",
             g_config_mgr.config_base_path, service_name);

    FILE* fp = fopen(config_path, "rb");
    if (!fp) {
        snprintf(config_path, sizeof(config_path), "%s/%s.yaml",
                 g_config_mgr.config_base_path, service_name);
        fp = fopen(config_path, "rb");
    }
    if (!fp) {
        snprintf(config_path, sizeof(config_path), "%s/%s.toml",
                 g_config_mgr.config_base_path, service_name);
        fp = fopen(config_path, "rb");
    }
#pragma GCC diagnostic pop

    agentos_config_t* cfg = (agentos_config_t*)AGENTOS_CALLOC(1, sizeof(agentos_config_t));
    if (!cfg) {
        return AGENTOS_ENOMEM;
    }

    if (!fp) {
        cfg->raw_config = AGENTOS_CALLOC(1, 1);
        cfg->config_size = 0;
        cfg->version = 1;
        cfg->last_modified = time(NULL);
        cfg->checksum[0] = '\0';
        *config = cfg;
        LOG_WARN("No config file found for service '%s', using empty config", service_name);
        return AGENTOS_SUCCESS;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(fp);
        cfg->raw_config = AGENTOS_CALLOC(1, 1);
        cfg->config_size = 0;
        cfg->version = 1;
        cfg->last_modified = time(NULL);
        *config = cfg;
        return AGENTOS_SUCCESS;
    }

    cfg->raw_config = (char*)AGENTOS_CALLOC(1, file_size + 1);
    if (!cfg->raw_config) {
        fclose(fp);
        AGENTOS_FREE(cfg);
        return AGENTOS_ENOMEM;
    }

    size_t bytes_read = fread(cfg->raw_config, 1, file_size, fp);
    fclose(fp);

    cfg->config_size = bytes_read;
    cfg->raw_config[bytes_read] = '\0';
    cfg->version = 1;
    cfg->last_modified = time(NULL);
    compute_simple_checksum(cfg->raw_config, bytes_read, cfg->checksum, sizeof(cfg->checksum));

    *config = cfg;

    LOG_INFO("Config loaded for service '%s': %zu bytes from %s",
             service_name, bytes_read, config_path);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_config_watch(
    const char* service_name,
    agentos_config_change_callback_t callback,
    void* user_data
) {
    if (!service_name || !callback) {
        return AGENTOS_EINVAL;
    }

    config_mgr_init();

    agentos_platform_mutex_lock(&g_config_mgr.mutex);

    if (g_config_mgr.watcher_count >= MAX_CONFIG_WATCHERS) {
        agentos_platform_mutex_unlock(&g_config_mgr.mutex);
        return AGENTOS_ENOMEM;
    }

    for (uint32_t i = 0; i < g_config_mgr.watcher_count; i++) {
        if (g_config_mgr.watchers[i].active &&
            strcmp(g_config_mgr.watchers[i].service_name, service_name) == 0 &&
            g_config_mgr.watchers[i].callback == callback) {
            g_config_mgr.watchers[i].user_data = user_data;
            agentos_platform_mutex_unlock(&g_config_mgr.mutex);
            return AGENTOS_SUCCESS;
        }
    }

    config_watcher_t* watcher = &g_config_mgr.watchers[g_config_mgr.watcher_count];
    if (safe_strcpy(watcher->service_name, service_name, sizeof(watcher->service_name)) != 0) {
        agentos_platform_mutex_unlock(&g_config_mgr.mutex);
        return AGENTOS_EINVAL;
    }
    watcher->callback = callback;
    watcher->user_data = user_data;
    watcher->active = true;
    g_config_mgr.watcher_count++;

    agentos_platform_mutex_unlock(&g_config_mgr.mutex);

    LOG_INFO("Config watcher registered for service '%s'", service_name);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_config_unwatch(
    const char* service_name,
    agentos_config_change_callback_t callback
) {
    if (!service_name) {
        return AGENTOS_EINVAL;
    }

    if (!g_config_mgr.initialized) {
        return AGENTOS_ENOTINIT;
    }

    agentos_platform_mutex_lock(&g_config_mgr.mutex);

    for (uint32_t i = 0; i < g_config_mgr.watcher_count; i++) {
        if (g_config_mgr.watchers[i].active &&
            strcmp(g_config_mgr.watchers[i].service_name, service_name) == 0) {
            if (callback == NULL || g_config_mgr.watchers[i].callback == callback) {
                g_config_mgr.watchers[i].active = false;

                if (i < g_config_mgr.watcher_count - 1) {
                    g_config_mgr.watchers[i] = g_config_mgr.watchers[g_config_mgr.watcher_count - 1];
                }
                g_config_mgr.watcher_count--;
                if (callback == NULL) {
                    continue;
                }
            }
        }
    }

    agentos_platform_mutex_unlock(&g_config_mgr.mutex);
    return AGENTOS_SUCCESS;
}

void agentos_config_free(agentos_config_t* config) {
    if (!config) {
        return;
    }

    if (config->raw_config) {
        AGENTOS_FREE(config->raw_config);
        config->raw_config = NULL;
    }

    AGENTOS_FREE(config);
}

/* ==================== 故障恢复（Phase 3.3） ==================== */

#define MAX_MONITORED_SERVICES 32

typedef struct {
    agentos_service_t service;
    agentos_monitor_config_t config;
    agentos_degradation_handler_t degradation_handler;
    void* degradation_user_data;
    bool active;
    uint32_t consecutive_failures;
    uint32_t restart_attempts;
    uint64_t last_check_time;
    uint64_t next_restart_time;
    bool degraded;
    agentos_thread_t monitor_thread;
    volatile int stop_requested;
} monitored_service_t;

static struct {
    monitored_service_t services[MAX_MONITORED_SERVICES];
    uint32_t count;
    bool initialized;
    agentos_platform_mutex_t mutex;
} g_monitor = {0};

static agentos_error_t monitor_init(void) {
    if (g_monitor.initialized) {
        return AGENTOS_SUCCESS;
    }

    agentos_error_t err = agentos_platform_mutex_init(&g_monitor.mutex);
    if (err != AGENTOS_SUCCESS) {
        return err;
    }

    memset(g_monitor.services, 0, sizeof(g_monitor.services));
    g_monitor.count = 0;
    g_monitor.initialized = true;

    return AGENTOS_SUCCESS;
}

static void* monitor_thread_func(void* arg) {
    monitored_service_t* mon = (monitored_service_t*)arg;
    if (!mon || !mon->service) return NULL;

    const char* svc_name = agentos_service_get_name(mon->service);
    uint32_t interval_ms = mon->config.healthcheck_interval_ms;
    if (interval_ms == 0) interval_ms = 30000;

    LOG_INFO("Monitor thread started for service '%s' (interval=%ums)", svc_name, interval_ms);

    while (!mon->stop_requested && mon->active) {
        agentos_sleep_ms(interval_ms);

        if (mon->stop_requested || !mon->active) break;

        agentos_error_t err = agentos_service_healthcheck(mon->service);
        mon->last_check_time = agentos_platform_get_time_ms();

        if (err != AGENTOS_SUCCESS) {
            mon->consecutive_failures++;
            LOG_WARN("Service '%s' health check failed (consecutive: %u)",
                     svc_name, mon->consecutive_failures);

            if (mon->config.enable_degradation &&
                mon->consecutive_failures >= mon->config.degradation_threshold &&
                !mon->degraded && mon->degradation_handler) {
                mon->degraded = true;
                char reason[128];
                snprintf(reason, sizeof(reason),
                         "consecutive_failures=%u >= threshold=%u",
                         mon->consecutive_failures,
                         mon->config.degradation_threshold);
                mon->degradation_handler(mon->service, reason, mon->degradation_user_data);
                LOG_WARN("Service '%s' degraded: %s", svc_name, reason);
            }

            if (mon->config.auto_restart &&
                mon->restart_attempts < mon->config.max_restart_attempts) {
                uint64_t now = agentos_platform_get_time_ms();
                if (now >= mon->next_restart_time) {
                    mon->restart_attempts++;
                    LOG_INFO("Auto-restarting service '%s' (attempt %u/%u)",
                             svc_name, mon->restart_attempts,
                             mon->config.max_restart_attempts);
                    agentos_service_stop(mon->service, true);
                    agentos_error_t start_err = agentos_service_start(mon->service);
                    if (start_err == AGENTOS_SUCCESS) {
                        mon->consecutive_failures = 0;
                        mon->degraded = false;
                        LOG_INFO("Service '%s' restarted successfully", svc_name);
                    } else {
                        uint32_t backoff = mon->config.restart_backoff_base_ms *
                                           (1 << (mon->restart_attempts - 1));
                        if (backoff > mon->config.restart_backoff_max_ms)
                            backoff = mon->config.restart_backoff_max_ms;
                        mon->next_restart_time = now + backoff;
                        LOG_ERROR("Service '%s' restart failed, next retry in %ums",
                                  svc_name, backoff);
                    }
                }
            }
        } else {
            if (mon->consecutive_failures > 0)
                LOG_INFO("Service '%s' recovered after %u failures", svc_name, mon->consecutive_failures);
            mon->consecutive_failures = 0;
            mon->restart_attempts = 0;
            mon->degraded = false;
        }
    }

    LOG_INFO("Monitor thread stopped for service '%s'", svc_name);
    return NULL;
}

agentos_error_t agentos_service_monitor_start(
    agentos_service_t service,
    const agentos_monitor_config_t* config
) {
    if (!service || !config) {
        return AGENTOS_EINVAL;
    }

    monitor_init();

    agentos_platform_mutex_lock(&g_monitor.mutex);

    for (uint32_t i = 0; i < g_monitor.count; i++) {
        if (g_monitor.services[i].service == service) {
            g_monitor.services[i].stop_requested = 1;
            if (g_monitor.services[i].monitor_thread) {
                agentos_platform_mutex_unlock(&g_monitor.mutex);
                agentos_platform_thread_join(g_monitor.services[i].monitor_thread, NULL);
                agentos_platform_mutex_lock(&g_monitor.mutex);
            }
            memcpy(&g_monitor.services[i].config, config, sizeof(agentos_monitor_config_t));
            g_monitor.services[i].active = true;
            g_monitor.services[i].consecutive_failures = 0;
            g_monitor.services[i].restart_attempts = 0;
            g_monitor.services[i].degraded = false;
            g_monitor.services[i].stop_requested = 0;
            g_monitor.services[i].last_check_time = agentos_platform_get_time_ms();
            g_monitor.services[i].next_restart_time = 0;

            int thread_err = agentos_platform_thread_create(
                &g_monitor.services[i].monitor_thread,
                monitor_thread_func,
                &g_monitor.services[i]
            );
            if (thread_err != 0) {
                g_monitor.services[i].active = false;
                agentos_platform_mutex_unlock(&g_monitor.mutex);
                LOG_ERROR("Failed to create monitor thread for service '%s'",
                         agentos_service_get_name(service));
                return AGENTOS_EINIT;
            }

            agentos_platform_mutex_unlock(&g_monitor.mutex);
            LOG_INFO("Service monitoring updated for '%s'",
                     agentos_service_get_name(service));
            return AGENTOS_SUCCESS;
        }
    }

    if (g_monitor.count >= MAX_MONITORED_SERVICES) {
        agentos_platform_mutex_unlock(&g_monitor.mutex);
        return AGENTOS_ENOMEM;
    }

    monitored_service_t* mon = &g_monitor.services[g_monitor.count];
    mon->service = service;
    memcpy(&mon->config, config, sizeof(agentos_monitor_config_t));
    mon->degradation_handler = NULL;
    mon->degradation_user_data = NULL;
    mon->active = true;
    mon->consecutive_failures = 0;
    mon->restart_attempts = 0;
    mon->last_check_time = agentos_platform_get_time_ms();
    mon->next_restart_time = 0;
    mon->degraded = false;
    mon->stop_requested = 0;
    mon->monitor_thread = (agentos_thread_t)0;

    int thread_err = agentos_platform_thread_create(
        &mon->monitor_thread,
        monitor_thread_func,
        mon
    );
    if (thread_err != 0) {
        mon->active = false;
        agentos_platform_mutex_unlock(&g_monitor.mutex);
        LOG_ERROR("Failed to create monitor thread for service '%s'",
                 agentos_service_get_name(service));
        return AGENTOS_EINIT;
    }

    g_monitor.count++;

    agentos_platform_mutex_unlock(&g_monitor.mutex);

    LOG_INFO("Service monitoring started for '%s' (interval=%ums, auto_restart=%s)",
             agentos_service_get_name(service),
             config->healthcheck_interval_ms,
             config->auto_restart ? "true" : "false");
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_service_monitor_stop(agentos_service_t service) {
    if (!service) {
        return AGENTOS_EINVAL;
    }

    if (!g_monitor.initialized) {
        return AGENTOS_ENOTINIT;
    }

    agentos_platform_mutex_lock(&g_monitor.mutex);

    for (uint32_t i = 0; i < g_monitor.count; i++) {
        if (g_monitor.services[i].service == service) {
            g_monitor.services[i].stop_requested = 1;
            g_monitor.services[i].active = false;

            agentos_thread_t thread = g_monitor.services[i].monitor_thread;

            agentos_platform_mutex_unlock(&g_monitor.mutex);

            if (thread) {
                agentos_platform_thread_join(thread, NULL);
            }

            agentos_platform_mutex_lock(&g_monitor.mutex);

            for (uint32_t j = 0; j < g_monitor.count; j++) {
                if (g_monitor.services[j].service == service) {
                    LOG_INFO("Service monitoring stopped for '%s'",
                             agentos_service_get_name(service));
                    if (j < g_monitor.count - 1) {
                        g_monitor.services[j] = g_monitor.services[g_monitor.count - 1];
                    }
                    memset(&g_monitor.services[g_monitor.count - 1], 0, sizeof(monitored_service_t));
                    g_monitor.count--;
                    break;
                }
            }

            agentos_platform_mutex_unlock(&g_monitor.mutex);
            return AGENTOS_SUCCESS;
        }
    }

    agentos_platform_mutex_unlock(&g_monitor.mutex);
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_service_set_degradation_handler(
    agentos_service_t service,
    agentos_degradation_handler_t handler,
    void* user_data
) {
    if (!service || !handler) {
        return AGENTOS_EINVAL;
    }

    monitor_init();

    agentos_platform_mutex_lock(&g_monitor.mutex);

    for (uint32_t i = 0; i < g_monitor.count; i++) {
        if (g_monitor.services[i].service == service) {
            g_monitor.services[i].degradation_handler = handler;
            g_monitor.services[i].degradation_user_data = user_data;
            agentos_platform_mutex_unlock(&g_monitor.mutex);
            LOG_INFO("Degradation handler set for service '%s'",
                     agentos_service_get_name(service));
            return AGENTOS_SUCCESS;
        }
    }

    if (g_monitor.count < MAX_MONITORED_SERVICES) {
        monitored_service_t* mon = &g_monitor.services[g_monitor.count];
        mon->service = service;
        mon->degradation_handler = handler;
        mon->degradation_user_data = user_data;
        mon->active = false;
        mon->consecutive_failures = 0;
        mon->restart_attempts = 0;
        mon->degraded = false;
        g_monitor.count++;

        agentos_platform_mutex_unlock(&g_monitor.mutex);
        LOG_INFO("Degradation handler set for unmonitored service '%s'",
                 agentos_service_get_name(service));
        return AGENTOS_SUCCESS;
    }

    agentos_platform_mutex_unlock(&g_monitor.mutex);
    return AGENTOS_ENOMEM;
}

/* ==================== 服务间通信客户端（Phase 3.2） ==================== */

typedef struct {
    agentos_protocol_type_t protocol;
    char base_url[512];
    uint32_t default_timeout_ms;
} client_internal_t;

#ifdef AGENTOS_HAS_CURL
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} curl_response_buf_t;

static size_t curl_write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
    curl_response_buf_t* buf = (curl_response_buf_t*)userdata;
    size_t total = size * nmemb;
    if (buf->size + total + 1 > buf->capacity) {
        size_t new_cap = buf->capacity == 0 ? 4096 : buf->capacity;
        while (new_cap < buf->size + total + 1) new_cap *= 2;
        char* new_data = (char*)AGENTOS_REALLOC(buf->data, new_cap);
        if (!new_data) return 0;
        buf->data = new_data;
        buf->capacity = new_cap;
    }
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}
#endif

static agentos_error_t http_client_call(
    const char* service_name,
    const char* method,
    const char* params_json,
    char** response_json,
    uint32_t timeout_ms
) {
    if (!service_name || !method || !response_json) {
        return AGENTOS_EINVAL;
    }
    (void)timeout_ms;

    LOG_DEBUG("HTTP client call: %s/%s (timeout=%ums)", service_name, method, timeout_ms);

    *response_json = NULL;

    agentos_service_t svc = agentos_service_find(service_name);
    if (svc) {
        agentos_service_internal_t* internal = (agentos_service_internal_t*)svc;

        if (internal->iface.handle_request) {
            agentos_error_t err = internal->iface.handle_request(
                svc, method, params_json, response_json, internal->user_data);
            if (err != AGENTOS_SUCCESS) {
                LOG_WARN("Service '%s' handle_request('%s') failed: %d",
                         service_name, method, err);
                return err;
            }
            if (!*response_json) {
                *response_json = AGENTOS_CALLOC(1, 2);
                if (*response_json) {
                    (*response_json)[0] = '{';
                    (*response_json)[1] = '}';
                }
            }
            return AGENTOS_SUCCESS;
        }

        agentos_svc_state_t state = agentos_service_get_state(svc);
        if (state != AGENTOS_SVC_STATE_RUNNING) {
            LOG_WARN("Service '%s' not running (state=%s), cannot handle request",
                     service_name, agentos_svc_state_to_string(state));
            return AGENTOS_ESTATE;
        }

        LOG_WARN("Service '%s' has no handle_request callback", service_name);
        return AGENTOS_ENOTSUP;
    }

    char url[768];
    snprintf(url, sizeof(url), "http://%s/api/%s", service_name, method);

#ifdef AGENTOS_HAS_CURL
    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("Failed to initialize CURL for remote call to '%s'", service_name);
        return AGENTOS_EINIT;
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout_ms);

    if (params_json) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params_json);
    }

    curl_response_buf_t resp_buf = { NULL, 0, 0 };
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_buf);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("CURL call to '%s' failed: %s", url, curl_easy_strerror(res));
        return AGENTOS_EIO;
    }

    if (http_code >= 400) {
        LOG_ERROR("Service '%s' returned HTTP %ld", service_name, http_code);
        return AGENTOS_EIO;
    }

    if (!resp_buf.data || resp_buf.size == 0) {
        *response_json = AGENTOS_CALLOC(1, 2);
        if (*response_json) {
            (*response_json)[0] = '{';
            (*response_json)[1] = '}';
        }
    } else {
        *response_json = resp_buf.data;
    }
#else
    (void)cli_internal;
    LOG_ERROR("Remote call to '%s' failed: libcurl not available", service_name);
    return AGENTOS_EIO;
#endif

    return AGENTOS_SUCCESS;
}

static agentos_error_t http_client_stream(
    const char* service_name,
    const char* method,
    const char* params_json,
    agentos_stream_callback_t callback,
    void* user_data
) {
    if (!service_name || !method || !callback) {
        return AGENTOS_EINVAL;
    }

    LOG_DEBUG("HTTP client stream: %s/%s", service_name, method);

    agentos_service_t svc = agentos_service_find(service_name);
    if (svc) {
        agentos_service_internal_t* internal = (agentos_service_internal_t*)svc;

        if (internal->iface.handle_request) {
            char* response = NULL;
            agentos_error_t err = internal->iface.handle_request(
                svc, method, params_json, &response, internal->user_data);
            if (err == AGENTOS_SUCCESS && response) {
                callback(response, strlen(response), user_data);
                AGENTOS_FREE(response);
            } else {
                const char* err_json = "{\"error\":\"stream_failed\"}";
                callback(err_json, strlen(err_json), user_data);
            }
            return err;
        }

        const char* err_json = "{\"error\":\"no_stream_handler\"}";
        callback(err_json, strlen(err_json), user_data);
        return AGENTOS_ENOTSUP;
    }

    return AGENTOS_ENOENT;
}

static agentos_error_t memory_client_call(
    const char* service_name,
    const char* method,
    const char* params_json,
    char** response_json,
    uint32_t timeout_ms
) {
    if (!service_name || !method || !response_json) {
        return AGENTOS_EINVAL;
    }

    agentos_service_t svc = agentos_service_find(service_name);
    if (svc) {
        agentos_service_internal_t* internal = (agentos_service_internal_t*)svc;

        if (internal->iface.handle_request) {
            agentos_error_t err = internal->iface.handle_request(
                svc, method, params_json, response_json, internal->user_data);
            if (err != AGENTOS_SUCCESS) {
                LOG_WARN("Service '%s' handle_request('%s') failed: %d",
                         service_name, method, err);
                return err;
            }
            if (!*response_json) {
                *response_json = (char*)AGENTOS_CALLOC(1, 2);
                if (*response_json) {
                    (*response_json)[0] = '{';
                    (*response_json)[1] = '}';
                }
            }
            return AGENTOS_SUCCESS;
        }

        LOG_WARN("Service '%s' has no handle_request callback", service_name);
        return AGENTOS_ENOTSUP;
    }

    LOG_INFO("Memory client: service '%s' not found locally, trying IPC RPC", service_name);

    char rpc_method[256];
    snprintf(rpc_method, sizeof(rpc_method), "%s.%s", service_name, method);

    int rpc_err = svc_rpc_call(rpc_method, params_json, response_json,
                               timeout_ms ? timeout_ms : 30000);
    if (rpc_err != 0 || !(*response_json)) {
        LOG_WARN("IPC RPC call to '%s' failed: %d", rpc_method, rpc_err);
        return AGENTOS_EIO;
    }

    LOG_INFO("IPC RPC call to '%s' succeeded", rpc_method);
    return AGENTOS_SUCCESS;
}

static agentos_error_t memory_client_stream(
    const char* service_name,
    const char* method,
    const char* params_json,
    agentos_stream_callback_t callback,
    void* user_data
) {
    if (!service_name || !method || !callback) {
        return AGENTOS_EINVAL;
    }

    agentos_service_t svc = agentos_service_find(service_name);
    if (!svc) {
        return AGENTOS_ENOENT;
    }

    agentos_service_internal_t* internal = (agentos_service_internal_t*)svc;

    if (internal->iface.handle_request) {
        char* response = NULL;
        agentos_error_t err = internal->iface.handle_request(
            svc, method, params_json, &response, internal->user_data);
        if (err == AGENTOS_SUCCESS && response) {
            callback(response, strlen(response), user_data);
            AGENTOS_FREE(response);
        } else {
            const char* err_json = "{\"error\":\"stream_failed\"}";
            callback(err_json, strlen(err_json), user_data);
        }
        return err;
    }

    const char* err_json = "{\"error\":\"no_stream_handler\"}";
    callback(err_json, strlen(err_json), user_data);
    return AGENTOS_ENOTSUP;
}

agentos_error_t agentos_service_client_create(
    agentos_protocol_type_t protocol,
    const char* config,
    agentos_service_client_t** client
) {
    if (!client) {
        return AGENTOS_EINVAL;
    }

    client_internal_t* internal = (client_internal_t*)AGENTOS_CALLOC(1, sizeof(client_internal_t));
    if (!internal) {
        return AGENTOS_ENOMEM;
    }

    internal->protocol = protocol;
    internal->default_timeout_ms = 30000;

    if (config) {
        if (safe_strcpy(internal->base_url, config, sizeof(internal->base_url)) != 0) {
            AGENTOS_FREE(internal);
            return AGENTOS_EINVAL;
        }
    } else {
        if (safe_strcpy(internal->base_url, "http://localhost:8080", sizeof(internal->base_url)) != 0) {
            AGENTOS_FREE(internal);
            return AGENTOS_EINVAL;
        }
    }

    agentos_service_client_t* cli = (agentos_service_client_t*)AGENTOS_CALLOC(1, sizeof(agentos_service_client_t));
    if (!cli) {
        AGENTOS_FREE(internal);
        return AGENTOS_ENOMEM;
    }

    switch (protocol) {
        case SVC_PROTO_HTTP:
            cli->call = http_client_call;
            cli->stream = http_client_stream;
            break;
        case SVC_PROTO_MEMORY:
            cli->call = memory_client_call;
            cli->stream = memory_client_stream;
            break;
        default:
            cli->call = http_client_call;
            cli->stream = http_client_stream;
            LOG_WARN("Protocol %d not fully implemented, using HTTP fallback", protocol);
            break;
    }

    *client = cli;
    cli->internal = internal;

    LOG_INFO("Service client created (protocol=%d, base_url=%s)", protocol, internal->base_url);
    return AGENTOS_SUCCESS;
}

void agentos_service_client_destroy(agentos_service_client_t* client) {
    if (!client) {
        return;
    }
    if (client->internal) {
        AGENTOS_FREE(client->internal);
        client->internal = NULL;
    }
    AGENTOS_FREE(client);
    LOG_DEBUG("Service client destroyed");
}