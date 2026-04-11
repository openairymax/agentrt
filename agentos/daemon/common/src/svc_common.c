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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
    
    agentos_error_t err = AGENTOS_SUCCESS;
    
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
    
    agentos_error_t err = AGENTOS_SUCCESS;
    
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
static void update_service_stats(agentos_service_internal_t* service, 
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
    strncpy(service->name, name, MAX_SERVICE_NAME_LEN - 1);
    service->name[MAX_SERVICE_NAME_LEN - 1] = '\0';
    
    if (config->version) {
        strncpy(service->version, config->version, MAX_SERVICE_VERSION_LEN - 1);
        service->version[MAX_SERVICE_VERSION_LEN - 1] = '\0';
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
        agentos_service_stop(service, true);  /* 强制停止 */
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

/* ==================== 模块清理 ==================== */

/* 注意：这个函数通常由 atexit 或模块卸载时调用 */
void agentos_svc_common_cleanup(void) {
    svc_common_module_cleanup();
}