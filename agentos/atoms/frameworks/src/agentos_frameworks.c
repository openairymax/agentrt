// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file agentos_frameworks.c
 * @brief Airymax 外部 AI 框架适配器层实现
 *
 * @details
 * 提供线程安全的适配器注册表和框架实例生命周期管理。
 * 本模块仅做注册表+生命周期调度，实际框架逻辑由适配器回调实现。
 *
 * @see agentos_frameworks.h
 */

#include "agentos_frameworks.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ── 兼容层 ── */
#ifndef AGENTOS_CALLOC
#define AGENTOS_CALLOC(nmemb, size) calloc(nmemb, size)
#endif
#ifndef AGENTOS_FREE
#define AGENTOS_FREE(ptr) free(ptr)
#endif

/* ── 线程安全原语（POSIX fallback，避免引入 sync 依赖） ── */
#ifdef _WIN32
#include <windows.h>
static CRITICAL_SECTION g_fw_mutex;
static bool g_fw_mutex_init = false;
#define FW_LOCK()   do { if (!g_fw_mutex_init) { InitializeCriticalSection(&g_fw_mutex); g_fw_mutex_init = true; } EnterCriticalSection(&g_fw_mutex); } while(0)
#define FW_UNLOCK() LeaveCriticalSection(&g_fw_mutex)
#else
#include <pthread.h>
static pthread_mutex_t g_fw_mutex = PTHREAD_MUTEX_INITIALIZER;
#define FW_LOCK()   pthread_mutex_lock(&g_fw_mutex)
#define FW_UNLOCK() pthread_mutex_unlock(&g_fw_mutex)
#endif

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 适配器注册表条目
 */
typedef struct {
    const agentos_framework_adapter_t *adapter;  /*!< 适配器指针（外部拥有） */
    bool used;                                   /*!< 槽位是否占用 */
} fw_adapter_slot_t;

/**
 * @brief 框架实例条目
 */
typedef struct {
    char name[AGENTOS_FW_MAX_NAME_LEN];          /*!< 实例名 */
    const agentos_framework_adapter_t *adapter;  /*!< 关联适配器 */
    void *handle;                                /*!< 适配器返回的实例句柄 */
    agentos_fw_state_t state;                    /*!< 当前状态 */
    agentos_fw_config_t config;                  /*!< 配置副本 */
    uint64_t init_time_ms;                       /*!< 初始化时间戳 */
    uint64_t request_count;                      /*!< 累计请求数 */
    uint64_t error_count;                        /*!< 累计错误数 */
    bool used;                                   /*!< 槽位是否占用 */
} fw_instance_slot_t;

/**
 * @brief 全局注册表状态
 */
static struct {
    fw_adapter_slot_t adapters[AGENTOS_FW_MAX_ADAPTERS];  /*!< 适配器注册表 */
    fw_instance_slot_t instances[AGENTOS_FW_MAX_ADAPTERS]; /*!< 实例表 */
    uint32_t adapter_count;                       /*!< 已注册适配器数 */
    uint32_t instance_count;                      /*!< 活跃实例数 */
    uint32_t instance_seq;                        /*!< 实例名序号（自动命名用） */
    bool initialized;                             /*!< 注册表是否已初始化 */
} g_registry;

/* ==================== 辅助函数 ==================== */

static uint64_t now_ms(void)
{
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    /* Windows FILETIME 是 100ns 单位，从 1601-01-01 起 */
    return (uli.QuadPart / 10000) - 11644473600000ULL;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

static void safe_strcpy(char *dest, const char *src, size_t dest_size)
{
    if (!dest || !src || dest_size == 0)
        return;
    size_t len = strlen(src);
    if (len >= dest_size)
        len = dest_size - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static int32_t find_adapter_index(const char *name)
{
    for (uint32_t i = 0; i < AGENTOS_FW_MAX_ADAPTERS; i++) {
        if (g_registry.adapters[i].used &&
            g_registry.adapters[i].adapter &&
            g_registry.adapters[i].adapter->name &&
            strcmp(g_registry.adapters[i].adapter->name, name) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int32_t find_instance_index(const char *name)
{
    for (uint32_t i = 0; i < AGENTOS_FW_MAX_ADAPTERS; i++) {
        if (g_registry.instances[i].used &&
            strcmp(g_registry.instances[i].name, name) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

static int32_t find_free_adapter_slot(void)
{
    for (uint32_t i = 0; i < AGENTOS_FW_MAX_ADAPTERS; i++) {
        if (!g_registry.adapters[i].used)
            return (int32_t)i;
    }
    return -1;
}

static int32_t find_free_instance_slot(void)
{
    for (uint32_t i = 0; i < AGENTOS_FW_MAX_ADAPTERS; i++) {
        if (!g_registry.instances[i].used)
            return (int32_t)i;
    }
    return -1;
}

/* ==================== 注册表 API ==================== */

int32_t agentos_fw_registry_init(void)
{
    FW_LOCK();
    if (g_registry.initialized) {
        FW_UNLOCK();
        return AGENTOS_FW_OK;  /* 幂等 */
    }
    memset(&g_registry, 0, sizeof(g_registry));
    g_registry.initialized = true;
    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

int32_t agentos_fw_registry_shutdown(void)
{
    FW_LOCK();
    if (!g_registry.initialized) {
        FW_UNLOCK();
        return AGENTOS_FW_OK;
    }

    /* 销毁所有活跃实例 */
    for (uint32_t i = 0; i < AGENTOS_FW_MAX_ADAPTERS; i++) {
        fw_instance_slot_t *inst = &g_registry.instances[i];
        if (!inst->used)
            continue;

        /* 先停止 */
        if (inst->state == AGENTOS_FW_STATE_RUNNING && inst->adapter && inst->adapter->stop) {
            inst->adapter->stop(inst->handle);
        }
        /* 再销毁 */
        if (inst->adapter && inst->adapter->destroy) {
            inst->adapter->destroy(inst->handle);
        }
        inst->handle = NULL;
        inst->used = false;
    }

    /* 清空适配器注册表 */
    for (uint32_t i = 0; i < AGENTOS_FW_MAX_ADAPTERS; i++) {
        g_registry.adapters[i].adapter = NULL;
        g_registry.adapters[i].used = false;
    }

    g_registry.adapter_count = 0;
    g_registry.instance_count = 0;
    g_registry.instance_seq = 0;
    g_registry.initialized = false;
    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

int32_t agentos_fw_register_adapter(const agentos_framework_adapter_t *adapter)
{
    if (!adapter || !adapter->name || !adapter->init || !adapter->destroy) {
        return AGENTOS_FW_INVALID_ARG;
    }

    FW_LOCK();
    if (!g_registry.initialized) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_INIT;
    }

    /* 检查重名 */
    if (find_adapter_index(adapter->name) >= 0) {
        FW_UNLOCK();
        return AGENTOS_FW_ALREADY_EXISTS;
    }

    /* 找空槽 */
    int32_t slot = find_free_adapter_slot();
    if (slot < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_CAPACITY_FULL;
    }

    g_registry.adapters[slot].adapter = adapter;
    g_registry.adapters[slot].used = true;
    g_registry.adapter_count++;
    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

int32_t agentos_fw_unregister_adapter(const char *name)
{
    if (!name)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    if (!g_registry.initialized) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_INIT;
    }

    int32_t idx = find_adapter_index(name);
    if (idx < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_FOUND;
    }

    /* 检查是否有活跃实例使用此适配器 */
    bool has_active = false;
    for (uint32_t i = 0; i < AGENTOS_FW_MAX_ADAPTERS; i++) {
        if (g_registry.instances[i].used &&
            g_registry.instances[i].adapter == g_registry.adapters[idx].adapter) {
            has_active = true;
            break;
        }
    }

    if (has_active) {
        FW_UNLOCK();
        return AGENTOS_FW_BUSY;
    }

    g_registry.adapters[idx].adapter = NULL;
    g_registry.adapters[idx].used = false;
    g_registry.adapter_count--;
    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

const agentos_framework_adapter_t *agentos_fw_find_adapter(const char *name)
{
    if (!name)
        return NULL;

    FW_LOCK();
    int32_t idx = find_adapter_index(name);
    const agentos_framework_adapter_t *result =
        (idx >= 0) ? g_registry.adapters[idx].adapter : NULL;
    FW_UNLOCK();
    return result;
}

int32_t agentos_fw_list_adapters(char names[][AGENTOS_FW_MAX_NAME_LEN],
                                  uint32_t max_count, uint32_t *found_count)
{
    if (!names || !found_count)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    uint32_t count = 0;
    for (uint32_t i = 0; i < AGENTOS_FW_MAX_ADAPTERS && count < max_count; i++) {
        if (g_registry.adapters[i].used && g_registry.adapters[i].adapter) {
            safe_strcpy(names[count], g_registry.adapters[i].adapter->name,
                        AGENTOS_FW_MAX_NAME_LEN);
            count++;
        }
    }
    *found_count = count;
    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

/* ==================== 实例生命周期 API ==================== */

int32_t agentos_fw_create_instance(const char *adapter_name,
                                    const agentos_fw_config_t *config,
                                    char *out_instance_name)
{
    if (!adapter_name)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    if (!g_registry.initialized) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_INIT;
    }

    int32_t adapter_idx = find_adapter_index(adapter_name);
    if (adapter_idx < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_FOUND;
    }

    int32_t slot = find_free_instance_slot();
    if (slot < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_CAPACITY_FULL;
    }

    const agentos_framework_adapter_t *adapter = g_registry.adapters[adapter_idx].adapter;
    fw_instance_slot_t *inst = &g_registry.instances[slot];

    /* 生成实例名 */
    memset(inst, 0, sizeof(*inst));
    if (config && config->name[0]) {
        safe_strcpy(inst->name, config->name, AGENTOS_FW_MAX_NAME_LEN);
    } else {
        /* 自动命名：<adapter>-<seq> */
        snprintf(inst->name, AGENTOS_FW_MAX_NAME_LEN, "%s-%u",
                 adapter->name, g_registry.instance_seq++);
    }

    /* 检查实例名唯一性 */
    if (find_instance_index(inst->name) != slot) {
        /* 名字冲突，重试 */
        snprintf(inst->name, AGENTOS_FW_MAX_NAME_LEN, "%s-%u",
                 adapter->name, g_registry.instance_seq++);
    }

    /* 存配置副本 */
    if (config) {
        memcpy(&inst->config, config, sizeof(agentos_fw_config_t));
    } else {
        memset(&inst->config, 0, sizeof(inst->config));
        inst->config.timeout_ms = 30000;
        inst->config.max_retries = 3;
    }

    /* 调用适配器 init */
    void *handle = NULL;
    int32_t rc = adapter->init(&inst->config, &handle);
    if (rc != AGENTOS_FW_OK) {
        FW_UNLOCK();
        return rc;
    }

    inst->adapter = adapter;
    inst->handle = handle;
    inst->state = AGENTOS_FW_STATE_INITIALIZED;
    inst->init_time_ms = now_ms();
    inst->request_count = 0;
    inst->error_count = 0;
    inst->used = true;
    g_registry.instance_count++;

    if (out_instance_name) {
        safe_strcpy(out_instance_name, inst->name, AGENTOS_FW_MAX_NAME_LEN);
    }

    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

int32_t agentos_fw_start_instance(const char *instance_name)
{
    if (!instance_name)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    int32_t idx = find_instance_index(instance_name);
    if (idx < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_FOUND;
    }

    fw_instance_slot_t *inst = &g_registry.instances[idx];
    if (inst->state != AGENTOS_FW_STATE_INITIALIZED &&
        inst->state != AGENTOS_FW_STATE_PAUSED) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_INIT;
    }

    if (!inst->adapter || !inst->adapter->start) {
        FW_UNLOCK();
        return AGENTOS_FW_ERROR;
    }

    int32_t rc = inst->adapter->start(inst->handle);
    if (rc == AGENTOS_FW_OK) {
        inst->state = AGENTOS_FW_STATE_RUNNING;
    } else {
        inst->state = AGENTOS_FW_STATE_ERROR;
        inst->error_count++;
    }
    FW_UNLOCK();
    return rc;
}

int32_t agentos_fw_stop_instance(const char *instance_name)
{
    if (!instance_name)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    int32_t idx = find_instance_index(instance_name);
    if (idx < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_FOUND;
    }

    fw_instance_slot_t *inst = &g_registry.instances[idx];
    if (inst->state == AGENTOS_FW_STATE_RUNNING) {
        if (inst->adapter && inst->adapter->stop) {
            int32_t rc = inst->adapter->stop(inst->handle);
            if (rc != AGENTOS_FW_OK) {
                inst->error_count++;
                FW_UNLOCK();
                return rc;
            }
        }
        inst->state = AGENTOS_FW_STATE_INITIALIZED;
    }
    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

int32_t agentos_fw_destroy_instance(const char *instance_name)
{
    if (!instance_name)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    int32_t idx = find_instance_index(instance_name);
    if (idx < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_FOUND;
    }

    fw_instance_slot_t *inst = &g_registry.instances[idx];

    /* 先停止 */
    if (inst->state == AGENTOS_FW_STATE_RUNNING && inst->adapter && inst->adapter->stop) {
        inst->adapter->stop(inst->handle);
    }

    /* 再销毁 */
    if (inst->adapter && inst->adapter->destroy) {
        inst->adapter->destroy(inst->handle);
    }

    inst->handle = NULL;
    inst->used = false;
    inst->state = AGENTOS_FW_STATE_SHUTDOWN;
    g_registry.instance_count--;
    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

int32_t agentos_fw_health_check(const char *instance_name)
{
    if (!instance_name)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    int32_t idx = find_instance_index(instance_name);
    if (idx < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_FOUND;
    }

    fw_instance_slot_t *inst = &g_registry.instances[idx];
    int32_t result;

    if (inst->state != AGENTOS_FW_STATE_RUNNING) {
        result = AGENTOS_FW_NOT_INIT;
    } else if (inst->adapter && inst->adapter->health_check) {
        result = inst->adapter->health_check(inst->handle);
        if (result != AGENTOS_FW_OK) {
            inst->error_count++;
        }
    } else {
        /* 无 health_check 回调则仅检查状态 */
        result = AGENTOS_FW_OK;
    }
    FW_UNLOCK();
    return result;
}

int32_t agentos_fw_get_info(const char *instance_name, agentos_fw_info_t *out_info)
{
    if (!instance_name || !out_info)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    int32_t idx = find_instance_index(instance_name);
    if (idx < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_FOUND;
    }

    fw_instance_slot_t *inst = &g_registry.instances[idx];
    memset(out_info, 0, sizeof(*out_info));
    safe_strcpy(out_info->name, inst->name, AGENTOS_FW_MAX_NAME_LEN);
    if (inst->adapter) {
        safe_strcpy(out_info->adapter_name, inst->adapter->name, AGENTOS_FW_MAX_NAME_LEN);
        if (inst->adapter->version) {
            safe_strcpy(out_info->adapter_version, inst->adapter->version, AGENTOS_FW_MAX_VERSION_LEN);
        }
        out_info->capabilities = inst->adapter->capabilities;
    }
    out_info->state = inst->state;
    out_info->init_time_ms = inst->init_time_ms;
    out_info->request_count = inst->request_count;
    out_info->error_count = inst->error_count;
    FW_UNLOCK();
    return AGENTOS_FW_OK;
}

int32_t agentos_fw_process_request(const char *instance_name,
                                    const agentos_fw_request_t *request,
                                    agentos_fw_response_t *response)
{
    if (!instance_name || !request || !response)
        return AGENTOS_FW_INVALID_ARG;

    FW_LOCK();
    int32_t idx = find_instance_index(instance_name);
    if (idx < 0) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_FOUND;
    }

    fw_instance_slot_t *inst = &g_registry.instances[idx];
    if (inst->state != AGENTOS_FW_STATE_RUNNING) {
        FW_UNLOCK();
        return AGENTOS_FW_NOT_INIT;
    }

    if (!inst->adapter || !inst->adapter->process_request) {
        FW_UNLOCK();
        return AGENTOS_FW_ERROR;  /* 适配器不支持请求处理 */
    }

    memset(response, 0, sizeof(*response));
    uint64_t start = now_ms();

    /* 适配器回调在锁外执行（避免长耗时阻塞注册表） */
    const agentos_framework_adapter_t *adapter = inst->adapter;
    void *handle = inst->handle;
    inst->request_count++;
    FW_UNLOCK();

    int32_t rc = adapter->process_request(handle, request, response);
    response->latency_ms = now_ms() - start;

    FW_LOCK();
    /* 重新查找实例（可能在回调期间被销毁） */
    idx = find_instance_index(instance_name);
    if (idx >= 0) {
        if (rc != AGENTOS_FW_OK) {
            g_registry.instances[idx].error_count++;
        }
    }
    FW_UNLOCK();

    return rc;
}

/* ==================== 工具函数 ==================== */

const char *agentos_fw_state_to_string(agentos_fw_state_t state)
{
    switch (state) {
    case AGENTOS_FW_STATE_UNINITIALIZED: return "Uninitialized";
    case AGENTOS_FW_STATE_INITIALIZED:   return "Initialized";
    case AGENTOS_FW_STATE_RUNNING:       return "Running";
    case AGENTOS_FW_STATE_PAUSED:        return "Paused";
    case AGENTOS_FW_STATE_ERROR:         return "Error";
    case AGENTOS_FW_STATE_SHUTDOWN:      return "Shutdown";
    default:                             return "Unknown";
    }
}

const char *agentos_fw_error_to_string(int32_t code)
{
    switch (code) {
    case AGENTOS_FW_OK:             return "OK";
    case AGENTOS_FW_ERROR:          return "General error";
    case AGENTOS_FW_NOT_INIT:       return "Not initialized";
    case AGENTOS_FW_INVALID_ARG:    return "Invalid argument";
    case AGENTOS_FW_TIMEOUT:        return "Timeout";
    case AGENTOS_FW_BUSY:           return "Busy";
    case AGENTOS_FW_NOT_FOUND:      return "Not found";
    case AGENTOS_FW_DENIED:         return "Permission denied";
    case AGENTOS_FW_ALREADY_EXISTS: return "Already exists";
    case AGENTOS_FW_CAPACITY_FULL:  return "Registry capacity full";
    default:                         return "Unknown error";
    }
}

bool agentos_fw_has_capability(const char *instance_name, agentos_fw_capability_t capability)
{
    if (!instance_name)
        return false;

    FW_LOCK();
    int32_t idx = find_instance_index(instance_name);
    bool result = false;
    if (idx >= 0 && g_registry.instances[idx].adapter) {
        uint32_t caps = g_registry.instances[idx].adapter->capabilities;
        result = (caps & (uint32_t)capability) != 0;
    }
    FW_UNLOCK();
    return result;
}
