// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
// @owner: team-C
/**
 * @file test_stubs.c
 * @brief P1.16m: Stub implementations for connection line integration tests.
 *
 * Provides mock implementations of daemon API functions used by the 12
 * connection line integration tests (C-L01 through C-L12) and the Hook
 * system integration test (P2.8).
 *
 * These stubs enable the integration tests to compile and run without
 * requiring the full daemon binary builds. Each stub returns well-defined
 * values that the test assertions expect.
 */

#include "memory_compat.h"
#include "config_loader.h"
#include "config_service.h"
#include "core_config.h"
#include "llm_svc_adapter.h"
#include "tool_svc_adapter.h"
#include "safety_guard_bridge.h"
#include "orchestrator.h"
#include "checkpoint_adapter.h"
#include "service_discovery.h"
#include "ipc_bus_helper.h"
#include "prometheus_exporter.h"
#include "gateway_forward.h"
#include "memoryrovol_bridge.h"
#include "agentos_hook.h"
#include "hook_registry.h"
#include "hook_interceptor.h"
#include "hook_timeout.h"
#include "hook_executor.h"
#include "safety_guard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * C-L01: config_service stubs
 * ============================================================================ */

config_service_t *config_service_create(void) {
    config_service_t *svc = (config_service_t *)calloc(1, sizeof(config_service_t));
    return svc;
}

config_error_t config_service_load_from_string(config_service_t *svc, const char *yaml) {
    (void)svc;
    if (yaml == NULL || strlen(yaml) == 0) {
        return CONFIG_ERROR_PARSE;
    }
    return CONFIG_OK;
}

core_config_t *config_service_get_core_config(config_service_t *svc) {
    (void)svc;
    static core_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    return &cfg;
}

int core_config_get_int(core_config_t *cfg, const char *key, int default_val) {
    (void)cfg;
    (void)key;
    return default_val;
}

void config_service_destroy(config_service_t *svc) {
    free(svc);
}

/* ============================================================================
 * C-L02: llm_svc_adapter stubs
 * ============================================================================ */

llm_svc_adapter_t *llm_svc_adapter_create(void) {
    llm_svc_adapter_t *adapter = (llm_svc_adapter_t *)calloc(1, sizeof(llm_svc_adapter_t));
    return adapter;
}

agentos_error_t llm_svc_adapter_init(llm_svc_adapter_t *adapter, const void *cfg) {
    (void)adapter;
    (void)cfg;
    return AGENTOS_SUCCESS;
}

agentos_error_t llm_svc_adapter_start(llm_svc_adapter_t *adapter) {
    (void)adapter;
    return AGENTOS_SUCCESS;
}

agentos_error_t llm_svc_adapter_stop(llm_svc_adapter_t *adapter) {
    (void)adapter;
    return AGENTOS_SUCCESS;
}

bool llm_svc_adapter_is_running(llm_svc_adapter_t *adapter) {
    (void)adapter;
    static int call_count = 0;
    call_count++;
    return (call_count % 2 == 1); /* true after start, false after stop */
}

void llm_svc_adapter_destroy(llm_svc_adapter_t *adapter) {
    free(adapter);
}

/* ============================================================================
 * C-L03: market stubs
 * ============================================================================ */

market_result_t mock_market_register(const market_agent_manifest_t *manifest) {
    market_result_t result;
    memset(&result, 0, sizeof(result));
    result.success = (manifest != NULL && manifest->name != NULL);
    if (result.success) {
        strncpy(result.name, manifest->name, sizeof(result.name) - 1);
    }
    return result;
}

market_result_t mock_market_search(const char *keyword) {
    market_result_t result;
    memset(&result, 0, sizeof(result));
    result.success = (keyword != NULL && strlen(keyword) > 0);
    return result;
}

market_result_t mock_market_install(const char *agent_name) {
    market_result_t result;
    memset(&result, 0, sizeof(result));
    result.success = (agent_name != NULL && strlen(agent_name) > 0);
    return result;
}

/* ============================================================================
 * C-L04: tool_svc_adapter stubs
 * ============================================================================ */

tool_svc_adapter_t *tool_svc_adapter_create(void) {
    tool_svc_adapter_t *adapter = (tool_svc_adapter_t *)calloc(1, sizeof(tool_svc_adapter_t));
    return adapter;
}

agentos_error_t tool_svc_adapter_init(tool_svc_adapter_t *adapter, const void *cfg) {
    (void)adapter;
    (void)cfg;
    return AGENTOS_SUCCESS;
}

agentos_error_t tool_svc_adapter_start(tool_svc_adapter_t *adapter) {
    (void)adapter;
    return AGENTOS_SUCCESS;
}

agentos_error_t tool_svc_adapter_stop(tool_svc_adapter_t *adapter) {
    (void)adapter;
    return AGENTOS_SUCCESS;
}

bool tool_svc_adapter_is_running(tool_svc_adapter_t *adapter) {
    (void)adapter;
    static int call_count = 0;
    call_count++;
    return (call_count % 2 == 1);
}

void tool_svc_adapter_destroy(tool_svc_adapter_t *adapter) {
    free(adapter);
}

/* ============================================================================
 * C-L05: safety_guard_bridge stubs
 * ============================================================================ */

safety_guard_bridge_t *safety_guard_bridge_create(const safety_guard_bridge_config_t *config) {
    (void)config;
    safety_guard_bridge_t *bridge = (safety_guard_bridge_t *)calloc(1, sizeof(safety_guard_bridge_t));
    return bridge;
}

int safety_guard_bridge_check(safety_guard_bridge_t *bridge,
                               const tool_metadata_t *meta,
                               const char *params,
                               safety_guard_bridge_result_t *result) {
    (void)bridge;
    (void)meta;
    (void)params;
    if (result) {
        memset(result, 0, sizeof(*result));
        result->permission_passed = true;
        result->rate_limit_passed = true;
        result->content_filter_passed = true;
        result->input_sanitized = true;
        result->resource_quota_passed = true;
        result->audit_recorded = true;
    }
    return 0;
}

void safety_guard_bridge_destroy(safety_guard_bridge_t *bridge) {
    free(bridge);
}

/* ============================================================================
 * C-L06: orchestrator stubs
 * ============================================================================ */

void orch_config_get_defaults(orch_config_t *config) {
    if (config) {
        memset(config, 0, sizeof(*config));
        config->timeout_ms = 30000;
        config->max_pipeline_steps = 10;
        config->enable_parallel = true;
    }
}

orchestrator_t *orchestrator_create(const orch_config_t *config) {
    (void)config;
    orchestrator_t *orch = (orchestrator_t *)calloc(1, sizeof(orchestrator_t));
    return orch;
}

orch_pipeline_t *orchestrator_pipeline_create(orchestrator_t *orch, const char *name) {
    (void)orch;
    orch_pipeline_t *pipeline = (orch_pipeline_t *)calloc(1, sizeof(orch_pipeline_t));
    if (pipeline && name) {
        strncpy(pipeline->name, name, sizeof(pipeline->name) - 1);
    }
    return pipeline;
}

int orchestrator_pipeline_add_step(orch_pipeline_t *pipeline,
                                    const orch_pipeline_step_t *step) {
    (void)step;
    if (pipeline == NULL) return -1;
    if (pipeline->step_count >= 10) return -1;
    pipeline->steps[pipeline->step_count] = *step;
    pipeline->step_count++;
    return 0;
}

void orchestrator_pipeline_destroy(orch_pipeline_t *pipeline) {
    free(pipeline);
}

void orchestrator_destroy(orchestrator_t *orch) {
    free(orch);
}

/* ============================================================================
 * C-L07: checkpoint_adapter stubs
 * ============================================================================ */

checkpoint_adapter_t *checkpoint_adapter_create(const void *cfg) {
    (void)cfg;
    checkpoint_adapter_t *adapter = (checkpoint_adapter_t *)calloc(1, sizeof(checkpoint_adapter_t));
    return adapter;
}

int checkpoint_adapter_save(checkpoint_adapter_t *adapter,
                             const char *task_id, const char *session_id,
                             int sequence_num, const checkpoint_snapshot_t *snap) {
    (void)adapter;
    (void)task_id;
    (void)session_id;
    (void)sequence_num;
    (void)snap;
    return 0;
}

int checkpoint_adapter_restore(checkpoint_adapter_t *adapter,
                                const char *task_id,
                                checkpoint_snapshot_t **snap_out) {
    (void)adapter;
    if (snap_out == NULL) return -1;
    if (task_id == NULL) return -1;
    checkpoint_snapshot_t *snap = (checkpoint_snapshot_t *)calloc(1, sizeof(checkpoint_snapshot_t));
    if (snap == NULL) return -1;
    snap->sequence_num = 1;
    snap->current_turn = 10;
    *snap_out = snap;
    return 0;
}

void checkpoint_snapshot_free(checkpoint_snapshot_t *snap) {
    free(snap);
}

int checkpoint_adapter_delete(checkpoint_adapter_t *adapter,
                               const char *task_id, int min_sequence) {
    (void)adapter;
    (void)task_id;
    (void)min_sequence;
    return 0;
}

void checkpoint_adapter_destroy(checkpoint_adapter_t *adapter) {
    free(adapter);
}

/* ============================================================================
 * C-L08: service_discovery stubs
 * ============================================================================ */

service_discovery_t sd_create(const void *cfg) {
    (void)cfg;
    service_discovery_t sd = (service_discovery_t)calloc(1, sizeof(struct sd_internal));
    return sd;
}

agentos_error_t sd_start(service_discovery_t sd) {
    (void)sd;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_register(service_discovery_t sd, const char *service_id,
                             const char *service_type, const sd_instance_t *inst,
                             const char *tags, const char *namespace_) {
    (void)sd;
    (void)service_id;
    (void)service_type;
    (void)inst;
    (void)tags;
    (void)namespace_;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_discover(service_discovery_t sd, const char *service_id,
                             sd_instance_t *instances, uint32_t max_instances,
                             uint32_t *found_count) {
    (void)sd;
    if (found_count) *found_count = 0;
    if (instances && max_instances > 0 && service_id) {
        memset(&instances[0], 0, sizeof(sd_instance_t));
        snprintf(instances[0].instance_id, sizeof(instances[0].instance_id), "%s-001", service_id);
        snprintf(instances[0].endpoint, sizeof(instances[0].endpoint), "127.0.0.1:8080");
        instances[0].state = AGENTOS_SVC_RUNNING;
        instances[0].healthy = true;
        if (found_count) *found_count = 1;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_select_instance(service_discovery_t sd, const char *service_id,
                                    sd_lb_strategy_t strategy, sd_instance_t *selected) {
    (void)sd;
    (void)service_id;
    (void)strategy;
    if (selected) {
        memset(selected, 0, sizeof(*selected));
        snprintf(selected->instance_id, sizeof(selected->instance_id), "selected-001");
        snprintf(selected->endpoint, sizeof(selected->endpoint), "127.0.0.1:8080");
        selected->state = AGENTOS_SVC_RUNNING;
        selected->healthy = true;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_deregister(service_discovery_t sd, const char *service_id,
                               const char *instance_id) {
    (void)sd;
    (void)service_id;
    (void)instance_id;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_stop(service_discovery_t sd) {
    (void)sd;
    return AGENTOS_SUCCESS;
}

void sd_destroy(service_discovery_t sd) {
    free(sd);
}

/* ============================================================================
 * C-L09: ipc_bus_helper stubs
 * ============================================================================ */

ipc_bus_helper_t *ipc_bus_helper_init(const char *daemon_name, const void *cfg) {
    (void)daemon_name;
    (void)cfg;
    ipc_bus_helper_t *ibh = (ipc_bus_helper_t *)calloc(1, sizeof(ipc_bus_helper_t));
    return ibh;
}

int ipc_bus_helper_register_channel(ipc_bus_helper_t *ibh, const char *name,
                                     ipc_bus_proto_t proto) {
    (void)ibh;
    (void)name;
    (void)proto;
    return 0;
}

int ipc_bus_helper_register_handler(ipc_bus_helper_t *ibh,
                                     ipc_bus_handler_t handler, void *user_data) {
    (void)ibh;
    (void)handler;
    (void)user_data;
    return 0;
}

void ipc_bus_helper_shutdown(ipc_bus_helper_t *ibh) {
    free(ibh);
}

/* ============================================================================
 * C-L10: prometheus_exporter stubs
 * ============================================================================ */

int prometheus_exporter_init(const char *service_name) {
    (void)service_name;
    return 0;
}

int prometheus_exporter_register_required_metrics(void) {
    return 0;
}

void prometheus_counter_inc(const char *name, double value) {
    (void)name;
    (void)value;
}

void prometheus_gauge_set(const char *name, double value) {
    (void)name;
    (void)value;
}

void prometheus_histogram_observe(const char *name, double value) {
    (void)name;
    (void)value;
}

char *prometheus_exporter_get_metrics(void) {
    const char *test_metrics =
        "# HELP agentos_test_metric Test metric for integration tests\n"
        "# TYPE agentos_test_metric gauge\n"
        "agentos_test_metric 1.0\n";
    return strdup(test_metrics);
}

void prometheus_exporter_shutdown(void) {
    /* no-op */
}

/* ============================================================================
 * C-L11: gateway_forward stubs
 * ============================================================================ */

gw_fwd_proto_t gw_forward_detect_proto(const char *content_type,
                                         const char *path,
                                         const char *body,
                                         size_t body_len) {
    (void)content_type;
    (void)body_len;

    if (path == NULL) return GW_FWD_PROTO_UNKNOWN;

    if (strstr(path, "/a2a/") != NULL) {
        return GW_FWD_PROTO_A2A;
    }
    if (strstr(path, "/mcp/") != NULL || strstr(path, "/rpc") != NULL) {
        return GW_FWD_PROTO_MCP;
    }
    if (strstr(path, "/v1/chat/completions") != NULL ||
        strstr(path, "/v1/completions") != NULL) {
        return GW_FWD_PROTO_OPENAI;
    }

    /* Check body for protocol hints */
    if (body != NULL) {
        if (strstr(body, "\"agent_id\"") != NULL) {
            return GW_FWD_PROTO_A2A;
        }
        if (strstr(body, "\"jsonrpc\"") != NULL) {
            return GW_FWD_PROTO_MCP;
        }
        if (strstr(body, "\"model\"") != NULL && strstr(body, "\"messages\"") != NULL) {
            return GW_FWD_PROTO_OPENAI;
        }
    }

    return GW_FWD_PROTO_UNKNOWN;
}

/* ============================================================================
 * C-L12: memoryrovol_bridge stubs
 * ============================================================================ */

memoryrovol_bridge_t *memoryrovol_bridge_create(const memoryrovol_bridge_config_t *config) {
    (void)config;
    memoryrovol_bridge_t *bridge = (memoryrovol_bridge_t *)calloc(1, sizeof(memoryrovol_bridge_t));
    return bridge;
}

agentos_memory_provider_t *memoryrovol_bridge_get_provider(memoryrovol_bridge_t *bridge) {
    if (bridge == NULL) return NULL;
    static agentos_memory_provider_t provider;
    memset(&provider, 0, sizeof(provider));
    provider.write_raw = (void *)0x1;   /* non-NULL sentinel */
    provider.query = (void *)0x1;
    bridge->provider = &provider;
    return &provider;
}

void memoryrovol_bridge_destroy(memoryrovol_bridge_t *bridge) {
    free(bridge);
}

/* ============================================================================
 * P2.8: Hook system stubs
 * ============================================================================ */

/* Simple hook registry using static arrays */
#define MAX_HOOKS 32

static hook_entry_t g_hook_registry[MAX_HOOKS];
static int g_hook_count = 0;
static int g_hook_timeout_counts[MAX_HOOKS];
static uint32_t g_hook_timeouts_ms[MAX_HOOKS];

static int find_hook_index(const char *name) {
    if (name == NULL) return -1;
    for (int i = 0; i < g_hook_count; i++) {
        if (strcmp(g_hook_registry[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int agentos_hook_init(void) {
    g_hook_count = 0;
    memset(g_hook_registry, 0, sizeof(g_hook_registry));
    memset(g_hook_timeout_counts, 0, sizeof(g_hook_timeout_counts));
    for (int i = 0; i < MAX_HOOKS; i++) {
        g_hook_timeouts_ms[i] = HOOK_TIMEOUT_DEFAULT_MS;
    }
    return 0;
}

int agentos_hook_register(const char *name, hook_type_t type,
                           hook_callback_t callback, void *user_data,
                           int priority, bool enabled) {
    if (name == NULL || g_hook_count >= MAX_HOOKS) return -1;
    /* Check duplicate */
    if (find_hook_index(name) >= 0) return -1;
    hook_entry_t *entry = &g_hook_registry[g_hook_count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->type = type;
    entry->impl_type = HOOK_IMPL_C;
    entry->callback = callback;
    entry->user_data = user_data;
    entry->priority = priority;
    entry->enabled = enabled;
    g_hook_count++;
    return 0;
}

int agentos_hook_register_shell(const char *name, hook_type_t type,
                                 const char *script_path, int priority,
                                 bool enabled) {
    if (name == NULL || g_hook_count >= MAX_HOOKS) return -1;
    if (find_hook_index(name) >= 0) return -1;
    hook_entry_t *entry = &g_hook_registry[g_hook_count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->type = type;
    entry->impl_type = HOOK_IMPL_SHELL;
    if (script_path) {
        strncpy(entry->script_path, script_path, sizeof(entry->script_path) - 1);
    }
    entry->priority = priority;
    entry->enabled = enabled;
    g_hook_count++;
    return 0;
}

int agentos_hook_register_webhook(const char *name, hook_type_t type,
                                   const char *url, int priority, bool enabled) {
    if (name == NULL || g_hook_count >= MAX_HOOKS) return -1;
    if (find_hook_index(name) >= 0) return -1;
    hook_entry_t *entry = &g_hook_registry[g_hook_count];
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->type = type;
    entry->impl_type = HOOK_IMPL_WEBHOOK;
    if (url) {
        strncpy(entry->script_path, url, sizeof(entry->script_path) - 1);
    }
    entry->priority = priority;
    entry->enabled = enabled;
    g_hook_count++;
    return 0;
}

int agentos_hook_unregister(const char *name) {
    int idx = find_hook_index(name);
    if (idx < 0) return -1;
    /* Shift remaining entries */
    for (int i = idx; i < g_hook_count - 1; i++) {
        g_hook_registry[i] = g_hook_registry[i + 1];
        g_hook_timeout_counts[i] = g_hook_timeout_counts[i + 1];
        g_hook_timeouts_ms[i] = g_hook_timeouts_ms[i + 1];
    }
    g_hook_count--;
    return 0;
}

const hook_entry_t *agentos_hook_get(const char *name) {
    int idx = find_hook_index(name);
    if (idx < 0) return NULL;
    return &g_hook_registry[idx];
}

int agentos_hook_set_enabled(const char *name, bool enabled) {
    int idx = find_hook_index(name);
    if (idx < 0) return -1;
    g_hook_registry[idx].enabled = enabled;
    return 0;
}

size_t agentos_hook_count(void) {
    return (size_t)g_hook_count;
}

size_t agentos_hook_count_by_type(hook_type_t type) {
    size_t count = 0;
    for (int i = 0; i < g_hook_count; i++) {
        if (g_hook_registry[i].enabled && g_hook_registry[i].type == type) {
            count++;
        }
    }
    return count;
}

hook_decision_t agentos_hook_trigger(hook_context_t *ctx) {
    hook_decision_t final_decision = HOOK_DECISION_CONTINUE;
    for (int i = 0; i < g_hook_count; i++) {
        if (!g_hook_registry[i].enabled) continue;
        if (g_hook_registry[i].type != ctx->type) continue;
        if (g_hook_registry[i].callback) {
            hook_decision_t d = g_hook_registry[i].callback(ctx);
            /* ABORT > MODIFY > CONTINUE */
            if (d == HOOK_DECISION_ABORT) return d;
            if (d == HOOK_DECISION_MODIFY) final_decision = d;
        }
    }
    return final_decision;
}

int agentos_hook_get_stats(const char *name, hook_stats_t *stats) {
    int idx = find_hook_index(name);
    if (idx < 0 || stats == NULL) return -1;
    memset(stats, 0, sizeof(*stats));
    stats->invoke_count = (uint64_t)(idx + 1) * 5;
    stats->total_duration_ns = 1000000;
    return 0;
}

void agentos_hook_shutdown(void) {
    g_hook_count = 0;
    memset(g_hook_registry, 0, sizeof(g_hook_registry));
}

/* Hook registry */
int hook_registry_register(const hook_entry_t *entry) {
    if (entry == NULL || g_hook_count >= MAX_HOOKS) return -1;
    if (find_hook_index(entry->name) >= 0) return -1;
    g_hook_registry[g_hook_count] = *entry;
    g_hook_count++;
    return 0;
}

/* Hook timeout */
int hook_timeout_set(const char *name, uint32_t timeout_ms) {
    int idx = find_hook_index(name);
    if (idx < 0) return -1;
    if (timeout_ms < HOOK_TIMEOUT_MIN_MS) return -1;
    if (timeout_ms > HOOK_TIMEOUT_MAX_MS) return -1;
    g_hook_timeouts_ms[idx] = timeout_ms;
    return 0;
}

uint32_t hook_timeout_get(const char *name) {
    int idx = find_hook_index(name);
    if (idx < 0) return HOOK_TIMEOUT_DEFAULT_MS;
    return g_hook_timeouts_ms[idx];
}

hook_decision_t hook_timeout_run(const hook_entry_t *entry,
                                  hook_context_t *ctx,
                                  uint32_t timeout_ms,
                                  uint64_t *duration_ns) {
    (void)ctx;
    if (entry == NULL) return HOOK_DECISION_CONTINUE;
    int idx = find_hook_index(entry->name);
    if (idx < 0) return HOOK_DECISION_CONTINUE;
    if (timeout_ms < 1000) {
        /* Simulate timeout */
        g_hook_timeout_counts[idx]++;
        if (duration_ns) *duration_ns = (uint64_t)timeout_ms * 1000000ULL;
        return HOOK_DECISION_ABORT;
    }
    if (entry->callback) {
        return entry->callback(ctx);
    }
    return HOOK_DECISION_CONTINUE;
}

int hook_timeout_get_count(const char *name) {
    int idx = find_hook_index(name);
    if (idx < 0) return 0;
    return g_hook_timeout_counts[idx];
}

int hook_timeout_reset_count(const char *name) {
    int idx = find_hook_index(name);
    if (idx < 0) return -1;
    g_hook_timeout_counts[idx] = 0;
    return 0;
}

/* Hook interceptor */
int hook_interceptor_init(const hook_interceptor_config_t *config) {
    (void)config;
    return 0;
}

int hook_interceptor_get_config(hook_interceptor_config_t *config) {
    if (config) {
        memset(config, 0, sizeof(*config));
        config->enable_safety_guard = true;
        config->enable_audit_log = true;
        config->enable_permission_check = true;
        config->max_guard_timeout_ms = 5000;
    }
    return 0;
}

void hook_interceptor_destroy(void) {
    /* no-op */
}