// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file service_discovery.c
 * @brief 跨进程服务发现机制实现
 *
 * 基于共享内存的跨进程服务注册中心实现。
 *
 * @see service_discovery.h
 */

#include "service_discovery.h"
#include "svc_logger.h"
#include "platform.h"
#include "error.h"
#include "daemon_errors.h"
#include "safe_string_utils.h"

#include "include/memory_compat.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================== 内部常量 ==================== */

#define SD_MAX_CALLBACKS    8
#define SD_REGISTRY_VERSION 1
#define SD_SHM_DEFAULT_SIZE (1024 * 1024)

/* ==================== 共享内存注册表头 ==================== */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t service_count;
    uint32_t total_instances;
    uint64_t last_modified;
    uint32_t checksum;
    agentos_platform_mutex_t shm_mutex;
} sd_registry_header_t;

#define SD_REGISTRY_MAGIC 0x53445247

/* ==================== 内部数据结构 ==================== */

typedef struct {
    sd_event_callback_t callback;
    void* user_data;
} sd_callback_entry_t;

typedef struct service_discovery_s {
    sd_config_t config;
    sd_service_entry_t services[SD_MAX_SERVICES];
    uint32_t service_count;
    sd_callback_entry_t callbacks[SD_MAX_CALLBACKS];
    uint32_t callback_count;
    sd_stats_t stats;
    bool running;
    agentos_platform_mutex_t mutex;
    uint32_t rr_counter;
    void* shm_handle;
    void* shm_ptr;
    bool is_shm_owner;
} sd_internal_t;

/* ==================== 辅助函数 ==================== */

static int32_t find_service_index(sd_internal_t* sd, const char* name) {
    for (uint32_t i = 0; i < sd->service_count; i++) {
        if (strcmp(sd->services[i].name, name) == 0)
            return (int32_t)i;
    }
    return -1;
}

static int32_t find_instance_index(sd_service_entry_t* entry, const char* instance_id) {
    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (strcmp(entry->instances[i].instance_id, instance_id) == 0)
            return (int32_t)i;
    }
    return -1;
}

static void notify_event(
    sd_internal_t* sd,
    sd_event_type_t event,
    const char* service_name,
    const sd_instance_t* instance
) {
    for (uint32_t i = 0; i < sd->callback_count; i++) {
        if (sd->callbacks[i].callback) {
            sd->callbacks[i].callback(event, service_name, instance,
                                      sd->callbacks[i].user_data);
        }
    }
}

static bool is_instance_expired(const sd_instance_t* inst, uint32_t expire_ms) {
    if (expire_ms == 0) return false;
    uint64_t now = agentos_platform_get_time_ms();
    return (now - inst->last_heartbeat) > expire_ms;
}

static void expire_stale_instances(sd_internal_t* sd) {
    if (!sd->config.enable_auto_expire) return;

    uint64_t now = agentos_platform_get_time_ms();
    for (uint32_t i = 0; i < sd->service_count; i++) {
        sd_service_entry_t* entry = &sd->services[i];
        for (uint32_t j = 0; j < entry->instance_count; ) {
            if (is_instance_expired(&entry->instances[j], sd->config.expire_timeout_ms)) {
                sd_instance_t expired = entry->instances[j];
                LOG_WARN("Instance '%s' of service '%s' expired (last heartbeat %llums ago)",
                         expired.instance_id, entry->name,
                         (unsigned long long)(now - expired.last_heartbeat));

                if (j < entry->instance_count - 1) {
                    entry->instances[j] = entry->instances[entry->instance_count - 1];
                }
                memset(&entry->instances[entry->instance_count - 1], 0, sizeof(sd_instance_t));
                entry->instance_count--;
                sd->stats.expirations++;

                notify_event(sd, SD_EVENT_EXPIRED, entry->name, &expired);
            } else {
                j++;
            }
        }

        if (entry->instance_count == 0 && sd->config.enable_auto_expire) {
            if (now - entry->last_updated > sd->config.expire_timeout_ms * 2) {
                if (i < sd->service_count - 1) {
                    sd->services[i] = sd->services[sd->service_count - 1];
                }
                memset(&sd->services[sd->service_count - 1], 0, sizeof(sd_service_entry_t));
                sd->service_count--;
                i--;
            }
        }
    }
}

/* ==================== 负载均衡选择 ==================== */

static agentos_error_t lb_round_robin(
    sd_internal_t* sd,
    const sd_service_entry_t* entry,
    sd_instance_t* result
) {
    if (entry->instance_count == 0) return AGENTOS_ENOENT;

    uint32_t start = sd->rr_counter % entry->instance_count;
    for (uint32_t i = 0; i < entry->instance_count; i++) {
        uint32_t idx = (start + i) % entry->instance_count;
        if (entry->instances[idx].healthy) {
            memcpy(result, &entry->instances[idx], sizeof(sd_instance_t));
            sd->rr_counter = idx + 1;
            return AGENTOS_SUCCESS;
        }
    }
    return AGENTOS_ENOENT;
}

static agentos_error_t lb_weighted(
    const sd_service_entry_t* entry,
    sd_instance_t* result
) {
    uint32_t total_weight = 0;
    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (entry->instances[i].healthy) {
            total_weight += entry->instances[i].weight;
        }
    }
    if (total_weight == 0) return AGENTOS_ENOENT;

    uint32_t random_val = (uint32_t)(agentos_platform_get_time_ms() % total_weight);
    uint32_t cumulative = 0;
    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (!entry->instances[i].healthy) continue;
        cumulative += entry->instances[i].weight;
        if (random_val < cumulative) {
            memcpy(result, &entry->instances[i], sizeof(sd_instance_t));
            return AGENTOS_SUCCESS;
        }
    }

    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (entry->instances[i].healthy) {
            memcpy(result, &entry->instances[i], sizeof(sd_instance_t));
            return AGENTOS_SUCCESS;
        }
    }
    return AGENTOS_ENOENT;
}

static agentos_error_t lb_least_connection(
    const sd_service_entry_t* entry,
    sd_instance_t* result
) {
    int32_t best_idx = -1;
    uint32_t min_conn = UINT32_MAX;

    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (!entry->instances[i].healthy) continue;
        if (entry->instances[i].active_connections < min_conn) {
            min_conn = entry->instances[i].active_connections;
            best_idx = (int32_t)i;
        }
    }

    if (best_idx < 0) return AGENTOS_ENOENT;
    memcpy(result, &entry->instances[best_idx], sizeof(sd_instance_t));
    return AGENTOS_SUCCESS;
}

static agentos_error_t lb_random(
    const sd_service_entry_t* entry,
    sd_instance_t* result
) {
    uint32_t healthy_count = 0;
    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (entry->instances[i].healthy) healthy_count++;
    }
    if (healthy_count == 0) return AGENTOS_ENOENT;

    uint32_t idx = (uint32_t)(agentos_platform_get_time_ms() % healthy_count);
    uint32_t count = 0;
    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (entry->instances[i].healthy) {
            if (count == idx) {
                memcpy(result, &entry->instances[i], sizeof(sd_instance_t));
                return AGENTOS_SUCCESS;
            }
            count++;
        }
    }
    return AGENTOS_ENOENT;
}

static agentos_error_t lb_least_load(
    const sd_service_entry_t* entry,
    sd_instance_t* result
) {
    int32_t best_idx = -1;
    uint32_t min_load = UINT32_MAX;

    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (!entry->instances[i].healthy) continue;
        uint32_t load = entry->instances[i].max_connections > 0
            ? entry->instances[i].active_connections * 100 / entry->instances[i].max_connections
            : 0;
        if (load < min_load) {
            min_load = load;
            best_idx = (int32_t)i;
        }
    }

    if (best_idx < 0) return AGENTOS_ENOENT;
    memcpy(result, &entry->instances[best_idx], sizeof(sd_instance_t));
    return AGENTOS_SUCCESS;
}

/* ==================== 公共API实现 ==================== */

AGENTOS_API sd_config_t sd_create_default_config(void) {
    sd_config_t config;
    memset(&config, 0, sizeof(sd_config_t));
    config.heartbeat_interval_ms = SD_DEFAULT_HEARTBEAT_MS;
    config.expire_timeout_ms = SD_DEFAULT_EXPIRE_MS;
    config.default_lb_strategy = SD_LB_ROUND_ROBIN;
    config.enable_auto_expire = true;
    config.enable_health_propagation = true;
    safe_strcpy(config.shm_name, SD_SHM_NAME, sizeof(config.shm_name));
    config.shm_size = SD_SHM_DEFAULT_SIZE;
    return config;
}

AGENTOS_API service_discovery_t sd_create(const sd_config_t* config) {
    sd_internal_t* sd = (sd_internal_t*)AGENTOS_CALLOC(1, sizeof(sd_internal_t));
    if (!sd) return NULL;

    if (config) {
        memcpy(&sd->config, config, sizeof(sd_config_t));
    } else {
        sd->config = sd_create_default_config();
    }

    agentos_error_t err = agentos_platform_mutex_init(&sd->mutex);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_FREE(sd);
        return NULL;
    }

    sd->running = false;
    sd->rr_counter = 0;
    sd->shm_handle = NULL;
    sd->shm_ptr = NULL;
    sd->is_shm_owner = false;

    LOG_INFO("Service discovery instance created");
    return (service_discovery_t)sd;
}

AGENTOS_API void sd_destroy(service_discovery_t sd_handle) {
    if (!sd_handle) return;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    if (sd->running) {
        sd_stop(sd_handle);
    }

    if (sd->shm_ptr) {
        sd->shm_ptr = NULL;
    }

    agentos_platform_mutex_destroy(&sd->mutex);
    AGENTOS_FREE(sd);

    LOG_INFO("Service discovery instance destroyed");
}

AGENTOS_API agentos_error_t sd_start(service_discovery_t sd_handle) {
    if (!sd_handle) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);
    if (sd->running) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_SUCCESS;
    }

    sd->running = true;
    agentos_platform_mutex_unlock(&sd->mutex);

    LOG_INFO("Service discovery started (heartbeat=%ums, expire=%ums)",
             sd->config.heartbeat_interval_ms, sd->config.expire_timeout_ms);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_stop(service_discovery_t sd_handle) {
    if (!sd_handle) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);
    sd->running = false;
    agentos_platform_mutex_unlock(&sd->mutex);

    LOG_INFO("Service discovery stopped");
    return AGENTOS_SUCCESS;
}

/* ==================== 服务注册 ==================== */

AGENTOS_API agentos_error_t sd_register(
    service_discovery_t sd_handle,
    const char* service_name,
    const char* service_type,
    const sd_instance_t* instance,
    const char* tags,
    const char* dependencies
) {
    if (!sd_handle || !service_name || !service_type || !instance)
        return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    int32_t svc_idx = find_service_index(sd, service_name);
    sd_service_entry_t* entry = NULL;

    if (svc_idx >= 0) {
        entry = &sd->services[svc_idx];
    } else {
        if (sd->service_count >= SD_MAX_SERVICES) {
            agentos_platform_mutex_unlock(&sd->mutex);
            LOG_ERROR("Service registry full, cannot register '%s'", service_name);
            return AGENTOS_ENOMEM;
        }

        entry = &sd->services[sd->service_count];
        memset(entry, 0, sizeof(sd_service_entry_t));
        safe_strcpy(entry->name, service_name, SD_MAX_NAME_LEN);
        safe_strcpy(entry->service_type, service_type, SD_MAX_TYPE_LEN);
        if (tags) safe_strcpy(entry->tags, tags, SD_MAX_TAGS_LEN);
        if (dependencies) safe_strcpy(entry->dependencies, dependencies, SD_MAX_DEPS_LEN);
        entry->active = true;
        entry->last_updated = agentos_platform_get_time_ms();
        sd->service_count++;
        sd->stats.registrations++;
    }

    int32_t inst_idx = find_instance_index(entry, instance->instance_id);
    if (inst_idx >= 0) {
        memcpy(&entry->instances[inst_idx], instance, sizeof(sd_instance_t));
        entry->instances[inst_idx].last_heartbeat = agentos_platform_get_time_ms();
        entry->instances[inst_idx].register_time =
            entry->instances[inst_idx].register_time > 0
                ? entry->instances[inst_idx].register_time
                : agentos_platform_get_time_ms();
    } else {
        if (entry->instance_count >= SD_MAX_INSTANCES) {
            agentos_platform_mutex_unlock(&sd->mutex);
            LOG_ERROR("Instance limit reached for service '%s'", service_name);
            return AGENTOS_ENOMEM;
        }

        memcpy(&entry->instances[entry->instance_count], instance, sizeof(sd_instance_t));
        entry->instances[entry->instance_count].last_heartbeat = agentos_platform_get_time_ms();
        entry->instances[entry->instance_count].register_time = agentos_platform_get_time_ms();
        entry->instances[entry->instance_count].pid =
#ifdef _WIN32
            (uint32_t)GetCurrentProcessId();
#else
            (uint32_t)getpid();
#endif
        entry->instance_count++;
    }

    entry->last_updated = agentos_platform_get_time_ms();
    sd->stats.active_services = sd->service_count;
    sd->stats.active_instances = 0;
    for (uint32_t i = 0; i < sd->service_count; i++) {
        sd->stats.active_instances += sd->services[i].instance_count;
    }

    agentos_platform_mutex_unlock(&sd->mutex);

    notify_event(sd, SD_EVENT_REGISTERED, service_name, instance);

    LOG_INFO("Service '%s' instance '%s' registered (type=%s, endpoint=%s)",
             service_name, instance->instance_id, service_type, instance->endpoint);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_deregister(
    service_discovery_t sd_handle,
    const char* service_name,
    const char* instance_id
) {
    if (!sd_handle || !service_name || !instance_id) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    sd_service_entry_t* entry = &sd->services[svc_idx];
    int32_t inst_idx = find_instance_index(entry, instance_id);
    if (inst_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    sd_instance_t removed = entry->instances[inst_idx];
    if ((uint32_t)inst_idx < entry->instance_count - 1) {
        entry->instances[inst_idx] = entry->instances[entry->instance_count - 1];
    }
    memset(&entry->instances[entry->instance_count - 1], 0, sizeof(sd_instance_t));
    entry->instance_count--;
    entry->last_updated = agentos_platform_get_time_ms();

    sd->stats.deregistrations++;
    sd->stats.active_instances = 0;
    for (uint32_t i = 0; i < sd->service_count; i++) {
        sd->stats.active_instances += sd->services[i].instance_count;
    }

    agentos_platform_mutex_unlock(&sd->mutex);

    notify_event(sd, SD_EVENT_DEREGISTERED, service_name, &removed);

    LOG_INFO("Service '%s' instance '%s' deregistered", service_name, instance_id);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_deregister_all(
    service_discovery_t sd_handle,
    const char* service_name
) {
    if (!sd_handle || !service_name) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    sd->services[svc_idx].instance_count = 0;
    sd->services[svc_idx].last_updated = agentos_platform_get_time_ms();

    agentos_platform_mutex_unlock(&sd->mutex);

    LOG_INFO("All instances of service '%s' deregistered", service_name);
    return AGENTOS_SUCCESS;
}

/* ==================== 服务发现 ==================== */

AGENTOS_API agentos_error_t sd_discover(
    service_discovery_t sd_handle,
    const char* service_name,
    sd_instance_t* instances,
    uint32_t max_count,
    uint32_t* found_count
) {
    if (!sd_handle || !service_name || !instances || !found_count)
        return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    expire_stale_instances(sd);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        *found_count = 0;
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    sd_service_entry_t* entry = &sd->services[svc_idx];
    uint32_t count = 0;
    for (uint32_t i = 0; i < entry->instance_count && count < max_count; i++) {
        if (entry->instances[i].healthy) {
            memcpy(&instances[count], &entry->instances[i], sizeof(sd_instance_t));
            count++;
        }
    }

    *found_count = count;
    sd->stats.discoveries++;

    agentos_platform_mutex_unlock(&sd->mutex);

    LOG_DEBUG("Discovered %u healthy instances for service '%s'", count, service_name);
    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_discover_by_type(
    service_discovery_t sd_handle,
    const char* service_type,
    sd_service_entry_t* entries,
    uint32_t max_count,
    uint32_t* found_count
) {
    if (!sd_handle || !service_type || !entries || !found_count)
        return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    expire_stale_instances(sd);

    uint32_t count = 0;
    for (uint32_t i = 0; i < sd->service_count && count < max_count; i++) {
        if (strcmp(sd->services[i].service_type, service_type) == 0) {
            memcpy(&entries[count], &sd->services[i], sizeof(sd_service_entry_t));
            count++;
        }
    }

    *found_count = count;
    sd->stats.discoveries++;

    agentos_platform_mutex_unlock(&sd->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_discover_by_tags(
    service_discovery_t sd_handle,
    const char* tags,
    sd_service_entry_t* entries,
    uint32_t max_count,
    uint32_t* found_count
) {
    if (!sd_handle || !tags || !entries || !found_count)
        return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    expire_stale_instances(sd);

    char filter_copy[SD_MAX_TAGS_LEN];
    safe_strcpy(filter_copy, tags, sizeof(filter_copy));

    uint32_t count = 0;
    char* saveptr = NULL;
    char* token = strtok_r(filter_copy, ",", &saveptr);

    while (token && count < max_count) {
        while (*token == ' ') token++;

        for (uint32_t i = 0; i < sd->service_count && count < max_count; i++) {
            if (strstr(sd->services[i].tags, token)) {
                bool already_added = false;
                for (uint32_t j = 0; j < count; j++) {
                    if (strcmp(entries[j].name, sd->services[i].name) == 0) {
                        already_added = true;
                        break;
                    }
                }
                if (!already_added) {
                    memcpy(&entries[count], &sd->services[i], sizeof(sd_service_entry_t));
                    count++;
                }
            }
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    *found_count = count;
    sd->stats.discoveries++;

    agentos_platform_mutex_unlock(&sd->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_select_instance(
    service_discovery_t sd_handle,
    const char* service_name,
    sd_lb_strategy_t strategy,
    sd_instance_t* instance
) {
    if (!sd_handle || !service_name || !instance) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    expire_stale_instances(sd);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    sd_service_entry_t* entry = &sd->services[svc_idx];
    agentos_error_t err;

    switch (strategy) {
        case SD_LB_ROUND_ROBIN:
            err = lb_round_robin(sd, entry, instance);
            break;
        case SD_LB_WEIGHTED:
            err = lb_weighted(entry, instance);
            break;
        case SD_LB_LEAST_CONNECTION:
            err = lb_least_connection(entry, instance);
            break;
        case SD_LB_RANDOM:
            err = lb_random(entry, instance);
            break;
        case SD_LB_LEAST_LOAD:
            err = lb_least_load(entry, instance);
            break;
        default:
            err = lb_round_robin(sd, entry, instance);
            break;
    }

    if (err == AGENTOS_SUCCESS) {
        sd->stats.lb_selections++;
    }

    agentos_platform_mutex_unlock(&sd->mutex);

    return err;
}

/* ==================== 心跳与健康 ==================== */

AGENTOS_API agentos_error_t sd_heartbeat(
    service_discovery_t sd_handle,
    const char* service_name,
    const char* instance_id
) {
    if (!sd_handle || !service_name || !instance_id) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    sd_service_entry_t* entry = &sd->services[svc_idx];
    int32_t inst_idx = find_instance_index(entry, instance_id);
    if (inst_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    entry->instances[inst_idx].last_heartbeat = agentos_platform_get_time_ms();
    sd->stats.heartbeats++;

    agentos_platform_mutex_unlock(&sd->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_update_health(
    service_discovery_t sd_handle,
    const char* service_name,
    const char* instance_id,
    bool healthy
) {
    if (!sd_handle || !service_name || !instance_id) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    sd_service_entry_t* entry = &sd->services[svc_idx];
    int32_t inst_idx = find_instance_index(entry, instance_id);
    if (inst_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    bool was_healthy = entry->instances[inst_idx].healthy;
    entry->instances[inst_idx].healthy = healthy;
    entry->instances[inst_idx].last_heartbeat = agentos_platform_get_time_ms();
    entry->last_updated = agentos_platform_get_time_ms();

    agentos_platform_mutex_unlock(&sd->mutex);

    if (was_healthy != healthy) {
        sd_event_type_t event = healthy ? SD_EVENT_INSTANCE_UP : SD_EVENT_INSTANCE_DOWN;
        notify_event(sd, event, service_name, &entry->instances[inst_idx]);

        if (!healthy) {
            LOG_WARN("Instance '%s' of service '%s' became unhealthy",
                     instance_id, service_name);
        } else {
            LOG_INFO("Instance '%s' of service '%s' recovered",
                     instance_id, service_name);
        }
    }

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_update_connections(
    service_discovery_t sd_handle,
    const char* service_name,
    const char* instance_id,
    uint32_t active_connections
) {
    if (!sd_handle || !service_name || !instance_id) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    sd_service_entry_t* entry = &sd->services[svc_idx];
    int32_t inst_idx = find_instance_index(entry, instance_id);
    if (inst_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    entry->instances[inst_idx].active_connections = active_connections;

    agentos_platform_mutex_unlock(&sd->mutex);

    return AGENTOS_SUCCESS;
}

/* ==================== 依赖管理 ==================== */

AGENTOS_API agentos_error_t sd_get_dependencies(
    service_discovery_t sd_handle,
    const char* service_name,
    char* dependencies,
    size_t max_len
) {
    if (!sd_handle || !service_name || !dependencies) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    safe_strcpy(dependencies, sd->services[svc_idx].dependencies, (uint32_t)max_len);

    agentos_platform_mutex_unlock(&sd->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_check_dependencies(
    service_discovery_t sd_handle,
    const char* service_name,
    char* missing_deps,
    size_t max_len
) {
    if (!sd_handle || !service_name) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    int32_t svc_idx = find_service_index(sd, service_name);
    if (svc_idx < 0) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOENT;
    }

    char deps_copy[SD_MAX_DEPS_LEN];
    safe_strcpy(deps_copy, sd->services[svc_idx].dependencies, sizeof(deps_copy));

    char missing[SD_MAX_DEPS_LEN] = {0};
    size_t missing_len = 0;

    char* saveptr = NULL;
    char* token = strtok_r(deps_copy, ",", &saveptr);
    while (token) {
        while (*token == ' ') token++;

        int32_t dep_idx = find_service_index(sd, token);
        bool dep_available = false;
        if (dep_idx >= 0) {
            for (uint32_t i = 0; i < sd->services[dep_idx].instance_count; i++) {
                if (sd->services[dep_idx].instances[i].healthy) {
                    dep_available = true;
                    break;
                }
            }
        }

        if (!dep_available) {
            size_t token_len = strlen(token);
            if (missing_len + token_len + 2 < sizeof(missing)) {
                if (missing_len > 0) {
                    safe_strcat(missing, ",", sizeof(missing));
                    missing_len++;
                }
                safe_strcat(missing, token, sizeof(missing));
                missing_len += token_len;
            }
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    agentos_platform_mutex_unlock(&sd->mutex);

    if (missing_deps && max_len > 0) {
        safe_strcpy(missing_deps, missing, (uint32_t)max_len);
    }

    return missing_len > 0 ? AGENTOS_EDEPEND : AGENTOS_SUCCESS;
}

/* ==================== 事件与统计 ==================== */

AGENTOS_API agentos_error_t sd_register_event_callback(
    service_discovery_t sd_handle,
    sd_event_callback_t callback,
    void* user_data
) {
    if (!sd_handle || !callback) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);

    if (sd->callback_count >= SD_MAX_CALLBACKS) {
        agentos_platform_mutex_unlock(&sd->mutex);
        return AGENTOS_ENOMEM;
    }

    sd->callbacks[sd->callback_count].callback = callback;
    sd->callbacks[sd->callback_count].user_data = user_data;
    sd->callback_count++;

    agentos_platform_mutex_unlock(&sd->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t sd_get_stats(
    service_discovery_t sd_handle,
    sd_stats_t* stats
) {
    if (!sd_handle || !stats) return AGENTOS_EINVAL;

    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);
    memcpy(stats, &sd->stats, sizeof(sd_stats_t));
    stats->active_services = sd->service_count;
    stats->active_instances = 0;
    for (uint32_t i = 0; i < sd->service_count; i++) {
        stats->active_instances += sd->services[i].instance_count;
    }
    agentos_platform_mutex_unlock(&sd->mutex);

    return AGENTOS_SUCCESS;
}

AGENTOS_API uint32_t sd_service_count(service_discovery_t sd_handle) {
    if (!sd_handle) return 0;
    sd_internal_t* sd = (sd_internal_t*)sd_handle;

    agentos_platform_mutex_lock(&sd->mutex);
    uint32_t count = sd->service_count;
    agentos_platform_mutex_unlock(&sd->mutex);

    return count;
}

AGENTOS_API bool sd_is_running(service_discovery_t sd_handle) {
    if (!sd_handle) return false;
    sd_internal_t* sd = (sd_internal_t*)sd_handle;
    return sd->running;
}

AGENTOS_API const char* sd_lb_strategy_to_string(sd_lb_strategy_t strategy) {
    static const char* strategy_strings[] = {
        "ROUND_ROBIN",
        "WEIGHTED",
        "LEAST_CONNECTION",
        "RANDOM",
        "LEAST_LOAD"
    };

    if (strategy < 0 || strategy > SD_LB_LEAST_LOAD) return "UNKNOWN";
    return strategy_strings[strategy];
}
