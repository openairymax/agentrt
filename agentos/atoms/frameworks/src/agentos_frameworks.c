// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file agentos_frameworks.c
 * @brief AgentOS统一框架抽象层实现
 *
 * @see agentos_frameworks.h
 */

#include "agentos_frameworks.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ==================== 内部常量 ==================== */

#define FW_MAX_CALLBACKS 8
#define FW_MAX_NAME_LEN  32
#define FW_MAX_VERSION_LEN 16

/* ==================== 框架描述 ==================== */

typedef struct {
    const char* name;
    const char* version;
    uint32_t default_capabilities;
} fw_descriptor_t;

static const fw_descriptor_t g_fw_descriptors[AGENTOS_FW_COUNT] = {
    {"CoreLoopThree", "2.0.0",
     AGENTOS_CAP_COGNITION | AGENTOS_CAP_EXECUTION |
     AGENTOS_CAP_PROTOCOL_MCP | AGENTOS_CAP_PROTOCOL_A2A},
    {"MemoryRovol", "2.0.0",
     AGENTOS_CAP_MEMORY_STORE | AGENTOS_CAP_MEMORY_RETRIEVE},
    {"CoreKern", "2.0.0",
     AGENTOS_CAP_TASK_SCHEDULE | AGENTOS_CAP_TASK_EXECUTE},
    {"Cupolas", "2.0.0",
     AGENTOS_CAP_SAFETY_CHECK | AGENTOS_CAP_SANDBOX},
    {"ToolD", "2.0.0",
     AGENTOS_CAP_TOOL_REGISTER | AGENTOS_CAP_TOOL_INVOKE |
     AGENTOS_CAP_PROTOCOL_MCP | AGENTOS_CAP_PROTOCOL_OPENAI},
};

/* ==================== 内部数据结构 ==================== */

typedef struct {
    agentos_fw_event_callback_t callback;
    void* user_data;
    agentos_framework_t framework_filter;
} fw_callback_entry_t;

typedef struct {
    agentos_fw_state_t state;
    agentos_fw_config_t config;
    agentos_fw_info_t info;
    bool initialized;
} fw_instance_t;

typedef struct agentos_fw_manager_s {
    fw_instance_t frameworks[AGENTOS_FW_COUNT];
    fw_callback_entry_t callbacks[FW_MAX_CALLBACKS];
    uint32_t callback_count;
    bool initialized;
} fw_manager_internal_t;

/* ==================== 辅助函数 ==================== */

static void notify_event(fw_manager_internal_t* mgr,
                         agentos_framework_t framework,
                         agentos_fw_event_type_t type,
                         const char* detail,
                         int32_t error_code) {
    agentos_fw_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.framework = framework;
    event.detail = detail;
    event.timestamp = 0;
    event.error_code = error_code;

    for (uint32_t i = 0; i < mgr->callback_count; i++) {
        fw_callback_entry_t* cb = &mgr->callbacks[i];
        if (cb->framework_filter == AGENTOS_FW_COUNT ||
            cb->framework_filter == framework) {
            cb->callback(&event, cb->user_data);
        }
    }
}

static void init_fw_info(fw_instance_t* fw, agentos_framework_t type) {
    const fw_descriptor_t* desc = &g_fw_descriptors[type];
    memset(&fw->info, 0, sizeof(agentos_fw_info_t));
    fw->info.type = type;
    safe_strcpy(fw->info.name, desc->name, FW_MAX_NAME_LEN);
    safe_strcpy(fw->info.version, desc->version, FW_MAX_VERSION_LEN);
    fw->info.state = AGENTOS_FW_STATE_UNINITIALIZED;
    fw->info.capabilities = desc->default_capabilities;
}

/* ==================== 公共API实现 ==================== */

AGENTOS_API agentos_fw_manager_t agentos_fw_manager_create(void) {
    fw_manager_internal_t* mgr =
        (fw_manager_internal_t*)AGENTOS_CALLOC(1, sizeof(fw_manager_internal_t));
    if (!mgr) return NULL;

    for (int i = 0; i < AGENTOS_FW_COUNT; i++) {
        init_fw_info(&mgr->frameworks[i], (agentos_framework_t)i);
        mgr->frameworks[i].state = AGENTOS_FW_STATE_UNINITIALIZED;
        mgr->frameworks[i].initialized = false;
    }

    mgr->callback_count = 0;
    mgr->initialized = true;

    return (agentos_fw_manager_t)mgr;
}

AGENTOS_API void agentos_fw_manager_destroy(agentos_fw_manager_t manager) {
    if (!manager) return;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;

    agentos_fw_stop_all(manager);

    for (int i = 0; i < AGENTOS_FW_COUNT; i++) {
        if (mgr->frameworks[i].initialized) {
            mgr->frameworks[i].state = AGENTOS_FW_STATE_SHUTDOWN;
            mgr->frameworks[i].initialized = false;
        }
    }

    AGENTOS_FREE(mgr);
}

AGENTOS_API int32_t agentos_fw_init(agentos_fw_manager_t manager,
                                    agentos_framework_t framework,
                                    const agentos_fw_config_t* config) {
    if (!manager || framework < 0 || framework >= AGENTOS_FW_COUNT)
        return AGENTOS_FW_INVALID_ARG;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;
    fw_instance_t* fw = &mgr->frameworks[framework];

    if (fw->initialized) return AGENTOS_FW_OK;

    if (config) {
        memcpy(&fw->config, config, sizeof(agentos_fw_config_t));
    } else {
        fw->config = agentos_fw_create_default_config(framework);
    }

    fw->state = AGENTOS_FW_STATE_INITIALIZED;
    fw->info.state = AGENTOS_FW_STATE_INITIALIZED;
    fw->info.init_time_ms = 0;
    fw->initialized = true;

    notify_event(mgr, framework, AGENTOS_FW_EVENT_INIT,
                 "Framework initialized", AGENTOS_FW_OK);

    return AGENTOS_FW_OK;
}

AGENTOS_API int32_t agentos_fw_init_all(agentos_fw_manager_t manager) {
    if (!manager) return AGENTOS_FW_INVALID_ARG;

    int32_t count = 0;
    for (int i = 0; i < AGENTOS_FW_COUNT; i++) {
        int32_t result = agentos_fw_init(manager, (agentos_framework_t)i, NULL);
        if (result == AGENTOS_FW_OK) count++;
    }

    return count;
}

AGENTOS_API int32_t agentos_fw_start(agentos_fw_manager_t manager,
                                     agentos_framework_t framework) {
    if (!manager || framework < 0 || framework >= AGENTOS_FW_COUNT)
        return AGENTOS_FW_INVALID_ARG;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;
    fw_instance_t* fw = &mgr->frameworks[framework];

    if (!fw->initialized) return AGENTOS_FW_NOT_INIT;
    if (fw->state == AGENTOS_FW_STATE_RUNNING) return AGENTOS_FW_OK;

    fw->state = AGENTOS_FW_STATE_RUNNING;
    fw->info.state = AGENTOS_FW_STATE_RUNNING;

    notify_event(mgr, framework, AGENTOS_FW_EVENT_START,
                 "Framework started", AGENTOS_FW_OK);

    return AGENTOS_FW_OK;
}

AGENTOS_API int32_t agentos_fw_start_all(agentos_fw_manager_t manager) {
    if (!manager) return AGENTOS_FW_INVALID_ARG;

    int32_t count = 0;
    for (int i = 0; i < AGENTOS_FW_COUNT; i++) {
        if (agentos_fw_start(manager, (agentos_framework_t)i) == AGENTOS_FW_OK)
            count++;
    }

    return count;
}

AGENTOS_API int32_t agentos_fw_stop(agentos_fw_manager_t manager,
                                    agentos_framework_t framework) {
    if (!manager || framework < 0 || framework >= AGENTOS_FW_COUNT)
        return AGENTOS_FW_INVALID_ARG;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;
    fw_instance_t* fw = &mgr->frameworks[framework];

    if (!fw->initialized) return AGENTOS_FW_NOT_INIT;

    agentos_fw_state_t old_state = fw->state;
    fw->state = AGENTOS_FW_STATE_INITIALIZED;
    fw->info.state = AGENTOS_FW_STATE_INITIALIZED;

    if (old_state == AGENTOS_FW_STATE_RUNNING) {
        notify_event(mgr, framework, AGENTOS_FW_EVENT_STOP,
                     "Framework stopped", AGENTOS_FW_OK);
    }

    return AGENTOS_FW_OK;
}

AGENTOS_API void agentos_fw_stop_all(agentos_fw_manager_t manager) {
    if (!manager) return;
    for (int i = 0; i < AGENTOS_FW_COUNT; i++) {
        agentos_fw_stop(manager, (agentos_framework_t)i);
    }
}

/* ==================== 框架查询 ==================== */

AGENTOS_API int32_t agentos_fw_get_info(agentos_fw_manager_t manager,
                                        agentos_framework_t framework,
                                        agentos_fw_info_t* info) {
    if (!manager || framework < 0 || framework >= AGENTOS_FW_COUNT || !info)
        return AGENTOS_FW_INVALID_ARG;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;
    memcpy(info, &mgr->frameworks[framework].info, sizeof(agentos_fw_info_t));

    return AGENTOS_FW_OK;
}

AGENTOS_API int32_t agentos_fw_get_all_info(agentos_fw_manager_t manager,
                                            agentos_fw_info_t* infos,
                                            uint32_t max_count,
                                            uint32_t* found_count) {
    if (!manager || !infos || !found_count) return AGENTOS_FW_INVALID_ARG;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;

    uint32_t count = max_count < AGENTOS_FW_COUNT ? max_count : AGENTOS_FW_COUNT;
    for (uint32_t i = 0; i < count; i++) {
        memcpy(&infos[i], &mgr->frameworks[i].info, sizeof(agentos_fw_info_t));
    }
    *found_count = count;

    return AGENTOS_FW_OK;
}

AGENTOS_API bool agentos_fw_has_capability(agentos_fw_manager_t manager,
                                           agentos_framework_t framework,
                                           agentos_capability_t capability) {
    if (!manager || framework < 0 || framework >= AGENTOS_FW_COUNT)
        return false;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;
    return (mgr->frameworks[framework].info.capabilities & (uint32_t)capability) != 0;
}

AGENTOS_API agentos_fw_state_t agentos_fw_get_state(agentos_fw_manager_t manager,
                                                    agentos_framework_t framework) {
    if (!manager || framework < 0 || framework >= AGENTOS_FW_COUNT)
        return AGENTOS_FW_STATE_UNINITIALIZED;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;
    return mgr->frameworks[framework].state;
}

/* ==================== 框架事件 ==================== */

AGENTOS_API int32_t agentos_fw_register_event_callback(
    agentos_fw_manager_t manager,
    agentos_framework_t framework,
    agentos_fw_event_callback_t callback,
    void* user_data) {
    if (!manager || !callback) return AGENTOS_FW_INVALID_ARG;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;

    if (mgr->callback_count >= FW_MAX_CALLBACKS) return AGENTOS_FW_BUSY;

    fw_callback_entry_t* entry = &mgr->callbacks[mgr->callback_count];
    entry->callback = callback;
    entry->user_data = user_data;
    entry->framework_filter = framework;
    mgr->callback_count++;

    return AGENTOS_FW_OK;
}

/* ==================== 框架健康检查 ==================== */

AGENTOS_API int32_t agentos_fw_health_check(agentos_fw_manager_t manager,
                                            agentos_framework_t framework) {
    if (!manager || framework < 0 || framework >= AGENTOS_FW_COUNT)
        return AGENTOS_FW_INVALID_ARG;

    fw_manager_internal_t* mgr = (fw_manager_internal_t*)manager;
    fw_instance_t* fw = &mgr->frameworks[framework];

    if (!fw->initialized) return AGENTOS_FW_NOT_INIT;
    if (fw->state == AGENTOS_FW_STATE_ERROR) return AGENTOS_FW_ERROR;
    if (fw->state != AGENTOS_FW_STATE_RUNNING) return AGENTOS_FW_OK;

    return AGENTOS_FW_OK;
}

AGENTOS_API int32_t agentos_fw_health_check_all(agentos_fw_manager_t manager) {
    if (!manager) return AGENTOS_FW_INVALID_ARG;

    int32_t healthy = 0;
    for (int i = 0; i < AGENTOS_FW_COUNT; i++) {
        if (agentos_fw_health_check(manager, (agentos_framework_t)i) == AGENTOS_FW_OK)
            healthy++;
    }

    return healthy;
}

/* ==================== 工具函数 ==================== */

AGENTOS_API const char* agentos_fw_type_to_string(agentos_framework_t framework) {
    static const char* names[] = {
        "Agent", "Memory", "Task", "Safety", "Tool"
    };
    if (framework < 0 || framework >= AGENTOS_FW_COUNT) return "Unknown";
    return names[framework];
}

AGENTOS_API const char* agentos_fw_state_to_string(agentos_fw_state_t state) {
    static const char* states[] = {
        "Uninitialized", "Initialized", "Running", "Paused", "Error", "Shutdown"
    };
    if (state < 0 || state > AGENTOS_FW_STATE_SHUTDOWN) return "Unknown";
    return states[state];
}

AGENTOS_API const char* agentos_fw_error_to_string(int32_t code) {
    switch (code) {
        case AGENTOS_FW_OK:           return "OK";
        case AGENTOS_FW_ERROR:        return "General error";
        case AGENTOS_FW_NOT_INIT:     return "Not initialized";
        case AGENTOS_FW_INVALID_ARG:  return "Invalid argument";
        case AGENTOS_FW_TIMEOUT:      return "Timeout";
        case AGENTOS_FW_BUSY:         return "Busy";
        case AGENTOS_FW_NOT_FOUND:    return "Not found";
        case AGENTOS_FW_DENIED:       return "Permission denied";
        default:                      return "Unknown error";
    }
}

AGENTOS_API agentos_fw_config_t agentos_fw_create_default_config(
    agentos_framework_t framework) {
    agentos_fw_config_t config;
    memset(&config, 0, sizeof(agentos_fw_config_t));
    config.type = framework;
    config.max_retries = 3;
    config.timeout_ms = 5000;
    config.enable_metrics = true;
    config.enable_tracing = true;
    config.enable_protocol_support = true;
    return config;
}
