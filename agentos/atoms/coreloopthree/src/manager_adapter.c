/**
 * @file manager_adapter.c
 * @brief C-L01: Manager → CoreLoopThree 连接桥梁实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 实现 Manager（Python 管理层）与 CoreLoopThree（C 核心引擎）之间的
 * 配置管理桥梁。整合 config_loader 和 yaml_loader，
 * 提供统一的配置管理接口。
 *
 * 关键功能：
 *   - P1.1.1: YAML 配置加载（通过 yaml_loader）
 *   - P1.1.2: config_loader ↔ yaml_loader 连接
 *   - P1.1.3: 环境变量覆盖（通过 yaml_loader）
 *   - P1.1.4: 配置热重载（通过 config_loader）
 *   - P1.12.4: 平台路径映射
 */

#include "manager_adapter.h"

#include "logging_compat.h"
#include "memory_compat.h"
#include "string_compat.h"
#include "agentos_quality.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ================================================================
 * 适配器内部结构
 * ================================================================ */

#define MANAGER_ADAPTER_MAX_CALLBACKS 16

struct agentos_manager_adapter_s {
    char yaml_path[256];                     /* agentos.yaml 路径 */
    bool config_loaded;                      /* 配置是否已加载 */
    bool watch_running;                      /* 热重载监听是否运行中 */

    /* 配置变更回调 */
    manager_adapter_config_change_cb_t callbacks[MANAGER_ADAPTER_MAX_CALLBACKS];
    void *callback_userdata[MANAGER_ADAPTER_MAX_CALLBACKS];
    size_t callback_count;
};

/* ================================================================
 * 内部：配置重载通知桥接
 * ================================================================ */

/**
 * @brief config_loader 回调 → manager_adapter 回调的桥接函数
 */
static void bridge_reload_callback(const agentos_yaml_config_t *old_config,
                                   const agentos_yaml_config_t *new_config,
                                   void *user_data)
{
    agentos_manager_adapter_t *adapter =
        (agentos_manager_adapter_t *)user_data;
    if (!adapter) return;

    AGENTOS_LOG_INFO("ManagerAdapter: config reload detected, "
                     "notifying %zu callbacks", adapter->callback_count);

    /* 通知所有注册的 manager_adapter 级别回调 */
    for (size_t i = 0; i < adapter->callback_count; i++) {
        if (adapter->callbacks[i]) {
            adapter->callbacks[i](old_config, new_config,
                                  adapter->callback_userdata[i]);
        }
    }
}

/* ================================================================
 * 生命周期实现
 * ================================================================ */

int manager_adapter_init(const char *yaml_path,
                         agentos_manager_adapter_t **out_adapter)
{
    if (!out_adapter) return -1;

    agentos_manager_adapter_t *adapter =
        (agentos_manager_adapter_t *)AGENTOS_CALLOC(1, sizeof(*adapter));
    if (!adapter) {
        AGENTOS_LOG_ERROR("ManagerAdapter: init FAILED (OOM)");
        return -1;
    }

    const char *path = yaml_path ? yaml_path : "./agentos.yaml";
    safe_strcpy(adapter->yaml_path, sizeof(adapter->yaml_path), path);

    AGENTOS_LOG_INFO("ManagerAdapter: init START (path=%s)", path);

    /* P1.1.1 + P1.1.2: 通过 config_loader 加载 YAML 配置 */
    int ret = agentos_config_init(path);
    if (ret != 0) {
        AGENTOS_LOG_WARN("ManagerAdapter: config_init failed for %s, "
                         "using defaults", path);
        /* 非致命：使用默认配置继续 */
    }

    adapter->config_loaded = true;
    adapter->watch_running = false;
    adapter->callback_count = 0;

    /* 注册 config_loader 级别的重载回调，桥接到 manager_adapter */
    agentos_config_on_reload(bridge_reload_callback, adapter);

    AGENTOS_LOG_INFO("ManagerAdapter: init OK (path=%s, config_loaded=%d)",
                     path, adapter->config_loaded);

    *out_adapter = adapter;
    return 0;
}

void manager_adapter_shutdown(agentos_manager_adapter_t *adapter)
{
    if (!adapter) return;

    AGENTOS_LOG_INFO("ManagerAdapter: shutdown START");

    /* 停止热重载监听 */
    if (adapter->watch_running) {
        manager_adapter_stop_watch(adapter);
    }

    adapter->config_loaded = false;
    adapter->callback_count = 0;

    AGENTOS_FREE(adapter);

    AGENTOS_LOG_INFO("ManagerAdapter: shutdown OK");
}

/* ================================================================
 * 配置管理实现
 * ================================================================ */

int manager_adapter_reload(agentos_manager_adapter_t *adapter)
{
    if (!adapter) return -1;

    AGENTOS_LOG_INFO("ManagerAdapter: reload triggered for %s",
                     adapter->yaml_path);

    return agentos_config_reload(adapter->yaml_path);
}

const agentos_yaml_config_t *manager_adapter_get_config(
    agentos_manager_adapter_t *adapter)
{
    if (!adapter || !adapter->config_loaded) return NULL;
    return agentos_config_get_global();
}

int manager_adapter_start_watch(agentos_manager_adapter_t *adapter,
                                uint32_t interval_ms)
{
    if (!adapter) return -1;
    if (adapter->watch_running) return 0; /* 已在运行 */

    AGENTOS_LOG_INFO("ManagerAdapter: starting config watch "
                     "(path=%s, interval=%ums)",
                     adapter->yaml_path, interval_ms);

    int ret = agentos_config_watch_start(adapter->yaml_path, interval_ms);
    if (ret == 0) {
        adapter->watch_running = true;
        AGENTOS_LOG_INFO("ManagerAdapter: config watch started OK");
    } else {
        AGENTOS_LOG_WARN("ManagerAdapter: config watch start FAILED");
    }

    return ret;
}

void manager_adapter_stop_watch(agentos_manager_adapter_t *adapter)
{
    if (!adapter || !adapter->watch_running) return;

    AGENTOS_LOG_INFO("ManagerAdapter: stopping config watch");
    agentos_config_watch_stop();
    adapter->watch_running = false;
}

/* ================================================================
 * YAML 配置 → Loop 配置转换
 * ================================================================ */

int manager_adapter_yaml_to_loop_config(
    const agentos_yaml_config_t *yaml_config,
    agentos_loop_config_t *out_loop_config)
{
    if (!yaml_config || !out_loop_config) return -1;

    /* 清零并设置默认值 */
    memset(out_loop_config, 0, sizeof(*out_loop_config));

    /* 从 YAML kernel 配置映射到 Loop 配置 */
    out_loop_config->loop_config_cognition_threads = 4;
    out_loop_config->loop_config_execution_threads = 8;
    out_loop_config->loop_config_memory_threads = 2;
    out_loop_config->loop_config_max_queued_tasks =
        yaml_config->kernel.scheduler.max_tasks > 0
            ? yaml_config->kernel.scheduler.max_tasks
            : 1000;
    out_loop_config->loop_config_stats_interval_ms = 60000;
    out_loop_config->loop_config_memory_query_limit = 5;
    out_loop_config->loop_config_task_timeout_ms =
        yaml_config->kernel.scheduler.time_slice_ms > 0
            ? yaml_config->kernel.scheduler.time_slice_ms * 100
            : 30000;
    out_loop_config->loop_config_memory_importance = 0.7f;

    /* Checkpoint 配置 */
    out_loop_config->loop_config_checkpoint_enabled = 0;
    out_loop_config->loop_config_checkpoint_interval_ms = 30000;
    out_loop_config->loop_config_checkpoint_interval_turns = 0;

    /* P1.6.1: 从 YAML multi_agent.checkpoint 映射 checkpoint 配置 */
    if (yaml_config->multi_agent.checkpoint_enabled) {
        out_loop_config->loop_config_checkpoint_enabled = 1;
    }
    if (yaml_config->multi_agent.checkpoint_interval_ms > 0) {
        out_loop_config->loop_config_checkpoint_interval_ms =
            yaml_config->multi_agent.checkpoint_interval_ms;
    }
    if (yaml_config->multi_agent.checkpoint_interval_turns > 0) {
        out_loop_config->loop_config_checkpoint_interval_turns =
            yaml_config->multi_agent.checkpoint_interval_turns;
    }

    /* 默认 checkpoint 路径 */
    if (yaml_config->multi_agent.checkpoint_path[0] != '\0') {
        snprintf(out_loop_config->loop_config_checkpoint_path,
                 sizeof(out_loop_config->loop_config_checkpoint_path),
                 "%s", yaml_config->multi_agent.checkpoint_path);
    } else {
        snprintf(out_loop_config->loop_config_checkpoint_path,
                 sizeof(out_loop_config->loop_config_checkpoint_path),
                 "./data/checkpoints");
    }

    AGENTOS_LOG_DEBUG("ManagerAdapter: yaml_to_loop_config "
                      "(max_tasks=%u, time_slice=%ums, timeout=%ums, "
                      "checkpoint_enabled=%u, checkpoint_interval_ms=%u, "
                      "checkpoint_interval_turns=%u)",
                      out_loop_config->loop_config_max_queued_tasks,
                      yaml_config->kernel.scheduler.time_slice_ms,
                      out_loop_config->loop_config_task_timeout_ms,
                      out_loop_config->loop_config_checkpoint_enabled,
                      out_loop_config->loop_config_checkpoint_interval_ms,
                      out_loop_config->loop_config_checkpoint_interval_turns);

    return 0;
}

agentos_error_t manager_adapter_create_loop(
    agentos_manager_adapter_t *adapter,
    agentos_core_loop_t **out_loop)
{
    if (!adapter || !out_loop) return AGENTOS_EINVAL;

    AGENTOS_LOG_INFO("ManagerAdapter: creating loop from YAML config");

    const agentos_yaml_config_t *yaml_config =
        manager_adapter_get_config(adapter);
    if (!yaml_config) {
        AGENTOS_LOG_WARN("ManagerAdapter: no config loaded, "
                         "creating loop with defaults");
        return agentos_loop_create(NULL, out_loop);
    }

    /* 转换 YAML 配置为 Loop 配置 */
    agentos_loop_config_t loop_config;
    manager_adapter_yaml_to_loop_config(yaml_config, &loop_config);

    /* 创建 Loop */
    agentos_error_t err = agentos_loop_create(&loop_config, out_loop);
    if (err == AGENTOS_SUCCESS) {
        AGENTOS_LOG_INFO("ManagerAdapter: loop created OK from YAML config");
    } else {
        AGENTOS_LOG_WARN("ManagerAdapter: loop creation FAILED (err=%d)", err);
    }

    return err;
}

/* ================================================================
 * 配置变更回调
 * ================================================================ */

int manager_adapter_on_config_change(
    agentos_manager_adapter_t *adapter,
    manager_adapter_config_change_cb_t callback,
    void *user_data)
{
    if (!adapter || !callback) return -1;
    if (adapter->callback_count >= MANAGER_ADAPTER_MAX_CALLBACKS) return -1;

    adapter->callbacks[adapter->callback_count] = callback;
    adapter->callback_userdata[adapter->callback_count] = user_data;
    adapter->callback_count++;

    AGENTOS_LOG_DEBUG("ManagerAdapter: registered config change callback "
                      "(total=%zu)", adapter->callback_count);

    return 0;
}

/* ================================================================
 * 状态查询
 * ================================================================ */

int manager_adapter_get_status(agentos_manager_adapter_t *adapter,
                               bool *out_config_loaded,
                               bool *out_watch_running,
                               const char **out_config_path)
{
    if (!adapter) return -1;

    if (out_config_loaded) *out_config_loaded = adapter->config_loaded;
    if (out_watch_running) *out_watch_running = adapter->watch_running;
    if (out_config_path) *out_config_path = adapter->yaml_path;

    return 0;
}