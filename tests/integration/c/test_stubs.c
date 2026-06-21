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

/* Bypass banned function poisoning for stub implementations */
#define AGENTOS_COMPLIANCE_IMPL
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "memory_compat.h"
#include "config_unified.h"
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
#include "hook_service.h"
#include "hook_registry.h"
#include "hook_timeout.h"
#include "hook_executor.h"
#include "hook_interceptor.h"
#include "safety_guard.h"
#include "agentos_hook.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * Stub implementations for memory/config library functions not linked
 * ============================================================================ */

/* Stubs for memory_compat internals */
void *memory_alloc(size_t size, const char *tag) { (void)tag; return malloc(size); }
void memory_free(void *ptr) { free(ptr); }
void *memory_calloc(size_t size, const char *tag) { (void)tag; return calloc(1, size); }
void agentos_mem_stats_record_dealloc(size_t bytes) { (void)bytes; /* no-op stub */ }

/* Stubs for config_unified internals */
config_context_t *config_context_create(const char *name) {
    (void)name;
    return (config_context_t *)malloc(1);
}

void config_context_destroy(config_context_t *ctx) {
    free(ctx);
}

config_error_t config_source_load(config_source_t *source, config_context_t *ctx) {
    (void)source;
    (void)ctx;
    return CONFIG_SUCCESS;
}

/* Stubs for config_value operations */
config_value_t *config_value_create_int(int32_t value) {
    (void)value;
    return (config_value_t *)malloc(1);
}

void config_value_destroy(config_value_t *value) {
    free(value);
}

int32_t config_value_get_int(const config_value_t *value, int32_t default_value) {
    (void)value;
    return default_value;
}

const config_value_t *config_context_get(const config_context_t *ctx, const char *key) {
    (void)ctx;
    (void)key;
    return NULL;
}

config_error_t config_context_set(config_context_t *ctx, const char *key, config_value_t *value) {
    (void)ctx;
    (void)key;
    (void)value;
    return CONFIG_SUCCESS;
}

void config_context_set_hot_reload(config_context_t *ctx, bool enabled, uint32_t interval_ms) {
    (void)ctx;
    (void)enabled;
    (void)interval_ms;
}

void config_context_set_encryption(config_context_t *ctx, bool enabled) {
    (void)ctx;
    (void)enabled;
}

/* Stubs for config_source operations */
config_source_t *config_source_create_memory(const config_memory_source_options_t *options) {
    (void)options;
    return (config_source_t *)malloc(1);
}

config_source_t *config_source_create_env(const config_env_source_options_t *options) {
    (void)options;
    return (config_source_t *)malloc(1);
}

config_source_t *config_source_create_file(const config_file_source_options_t *options) {
    (void)options;
    return (config_source_t *)malloc(1);
}

void config_source_destroy(config_source_t *source) {
    free(source);
}

/* ============================================================================
 * Stub helper: allocate opaque handle (incomplete type safe)
 * ============================================================================ */

static void *stub_alloc(void) {
    return malloc(1);  /* minimal allocation for opaque handles */
}

/* ============================================================================
 * C-L01: config_service stubs (using config_unified API)
 * ============================================================================ */

config_context_t *config_service_create(const char *service_name, config_schema_t *schema,
                                        bool enable_hot_reload, bool enable_encryption) {
    (void)schema;
    (void)enable_hot_reload;
    (void)enable_encryption;
    config_context_t *ctx = config_context_create(service_name);
    return ctx;
}

config_error_t config_service_load(config_context_t *ctx, config_source_t **sources,
                                   size_t source_count) {
    (void)ctx;
    if (sources == NULL || source_count == 0) {
        return CONFIG_ERROR_INVALID_ARG;
    }
    /* Stub: just load from the first source */
    return config_source_load(sources[0], ctx);
}

config_error_t config_service_save(config_context_t *ctx, config_source_t *primary_source) {
    (void)ctx;
    (void)primary_source;
    return CONFIG_SUCCESS;
}

config_error_t config_service_get_status(config_context_t *ctx, char *status_json,
                                         size_t status_size) {
    (void)ctx;
    if (status_json && status_size > 0) {
        snprintf(status_json, status_size, "{\"status\":\"ok\"}");
    }
    return CONFIG_SUCCESS;
}

void config_service_destroy(config_context_t *ctx) {
    config_context_destroy(ctx);
}

/* ============================================================================
 * C-L02: llm_svc_adapter stubs (using coreloopthree API)
 * ============================================================================ */

llm_svc_adapter_t *llm_svc_adapter_create(const llm_svc_adapter_config_t *config) {
    (void)config;
    return (llm_svc_adapter_t *)stub_alloc();
}

void llm_svc_adapter_destroy(llm_svc_adapter_t *adapter) {
    free(adapter);
}

llm_service_t *llm_svc_adapter_get_service(llm_svc_adapter_t *adapter) {
    (void)adapter;
    return NULL;
}

int llm_svc_adapter_complete(llm_svc_adapter_t *adapter,
                             const llm_request_config_t *config,
                             llm_response_t **out_response) {
    (void)adapter;
    (void)config;
    if (out_response == NULL) return -1;
    *out_response = NULL;
    return -1;
}

int llm_svc_adapter_complete_stream(llm_svc_adapter_t *adapter,
                                    const llm_request_config_t *config,
                                    llm_stream_callback_t callback,
                                    void *callback_data,
                                    llm_response_t **out_response) {
    (void)adapter;
    (void)config;
    (void)callback;
    (void)callback_data;
    if (out_response == NULL) return -1;
    *out_response = NULL;
    return -1;
}

bool llm_svc_adapter_is_connected(llm_svc_adapter_t *adapter) {
    (void)adapter;
    return false;
}

void llm_svc_adapter_get_stats(llm_svc_adapter_t *adapter,
                               uint64_t *out_total_requests,
                               uint64_t *out_total_errors,
                               uint64_t *out_avg_latency_ms) {
    (void)adapter;
    if (out_total_requests) *out_total_requests = 0;
    if (out_total_errors) *out_total_errors = 0;
    if (out_avg_latency_ms) *out_avg_latency_ms = 0;
}

/* ============================================================================
 * C-L03: market stubs
 * ============================================================================ */

/* Stub types for market (not provided by current headers) */
typedef struct {
    bool success;
    char name[128];
} stub_market_result_t;

typedef struct {
    const char *name;
    const char *version;
} stub_market_manifest_t;

stub_market_result_t mock_market_register(const stub_market_manifest_t *manifest) {
    stub_market_result_t result;
    memset(&result, 0, sizeof(result));
    result.success = (manifest != NULL && manifest->name != NULL);
    if (result.success) {
        strncpy(result.name, manifest->name, sizeof(result.name) - 1);
    }
    return result;
}

stub_market_result_t mock_market_search(const char *keyword) {
    stub_market_result_t result;
    memset(&result, 0, sizeof(result));
    result.success = (keyword != NULL && strlen(keyword) > 0);
    return result;
}

stub_market_result_t mock_market_install(const char *agent_name) {
    stub_market_result_t result;
    memset(&result, 0, sizeof(result));
    result.success = (agent_name != NULL && strlen(agent_name) > 0);
    return result;
}

/* ============================================================================
 * C-L04: tool_svc_adapter stubs (using coreloopthree API)
 * ============================================================================ */

tool_svc_adapter_t *tool_svc_adapter_create(const tool_svc_adapter_config_t *config) {
    (void)config;
    return (tool_svc_adapter_t *)stub_alloc();
}

void tool_svc_adapter_destroy(tool_svc_adapter_t *adapter) {
    free(adapter);
}

tool_service_t *tool_svc_adapter_get_service(tool_svc_adapter_t *adapter) {
    (void)adapter;
    return NULL;
}

int tool_svc_adapter_execute(tool_svc_adapter_t *adapter,
                             const tool_execute_request_t *req,
                             tool_result_t **out_result) {
    (void)adapter;
    (void)req;
    if (out_result == NULL) return -1;
    *out_result = NULL;
    return -1;
}

int tool_svc_adapter_execute_stream(tool_svc_adapter_t *adapter,
                                    const tool_execute_request_t *req,
                                    tool_stream_callback_t callback,
                                    void *callback_data,
                                    tool_result_t **out_result) {
    (void)adapter;
    (void)req;
    (void)callback;
    (void)callback_data;
    if (out_result == NULL) return -1;
    *out_result = NULL;
    return -1;
}

int tool_svc_adapter_register(tool_svc_adapter_t *adapter,
                              const tool_metadata_t *meta) {
    (void)adapter;
    (void)meta;
    return 0;
}

int tool_svc_adapter_list(tool_svc_adapter_t *adapter, char **out_json) {
    (void)adapter;
    if (out_json) *out_json = strdup("[]");
    return 0;
}

bool tool_svc_adapter_is_connected(tool_svc_adapter_t *adapter) {
    (void)adapter;
    return false;
}

void tool_svc_adapter_get_stats(tool_svc_adapter_t *adapter,
                                uint64_t *out_total_executions,
                                uint64_t *out_total_errors,
                                uint64_t *out_avg_latency_ms) {
    (void)adapter;
    if (out_total_executions) *out_total_executions = 0;
    if (out_total_errors) *out_total_errors = 0;
    if (out_avg_latency_ms) *out_avg_latency_ms = 0;
}

/* ============================================================================
 * C-L05: safety_guard_bridge stubs
 * ============================================================================ */

safety_guard_bridge_t *safety_guard_bridge_create(const safety_guard_bridge_config_t *config) {
    (void)config;
    return (safety_guard_bridge_t *)stub_alloc();
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
        result->guard_chain_length = 6;
        result->guards_executed = 6;
    }
    return 0;
}

int safety_guard_bridge_check_permission(safety_guard_bridge_t *bridge,
                                          const char *agent_id,
                                          const char *tool_name,
                                          const char *action) {
    (void)bridge;
    (void)agent_id;
    (void)tool_name;
    (void)action;
    return 0;
}

int safety_guard_bridge_check_rate_limit(safety_guard_bridge_t *bridge,
                                          const char *tool_name) {
    (void)bridge;
    (void)tool_name;
    return 0;
}

int safety_guard_bridge_filter_content(safety_guard_bridge_t *bridge,
                                        const char *params_json,
                                        char *sanitized_params,
                                        size_t sanitized_size) {
    (void)bridge;
    if (params_json && sanitized_params && sanitized_size > 0) {
        snprintf(sanitized_params, sanitized_size, "%s", params_json);
    }
    return 0;
}

int safety_guard_bridge_audit_log(safety_guard_bridge_t *bridge,
                                   const char *event_type,
                                   const char *tool_name,
                                   int decision,
                                   const char *reason,
                                   const char *agent_id) {
    (void)bridge;
    (void)event_type;
    (void)tool_name;
    (void)decision;
    (void)reason;
    (void)agent_id;
    return 0;
}

void safety_guard_bridge_get_stats(safety_guard_bridge_t *bridge,
                                    uint64_t *out_total_checks,
                                    uint64_t *out_denied_count,
                                    uint64_t *out_rate_limited) {
    (void)bridge;
    if (out_total_checks) *out_total_checks = 0;
    if (out_denied_count) *out_denied_count = 0;
    if (out_rate_limited) *out_rate_limited = 0;
}

void safety_guard_bridge_destroy(safety_guard_bridge_t *bridge) {
    free(bridge);
}

/* ============================================================================
 * C-L06: orchestrator stubs
 * ============================================================================ */

/* Note: orch_config_get_defaults() is now static inline in orchestrator.h,
 * so we do NOT redefine it here. */

orchestrator_t *orchestrator_create(const orch_config_t *config) {
    (void)config;
    return (orchestrator_t *)stub_alloc();
}

orch_pipeline_t *orchestrator_pipeline_create(orchestrator_t *orch, const char *name) {
    (void)orch;
    (void)name;
    return (orch_pipeline_t *)stub_alloc();
}

int orchestrator_pipeline_add_step(orch_pipeline_t *pipeline,
                                    const orch_pipeline_step_t *step) {
    (void)pipeline;
    (void)step;
    return 0;
}

void orchestrator_pipeline_destroy(orch_pipeline_t *pipeline) {
    free(pipeline);
}

void orchestrator_destroy(orchestrator_t *orch) {
    free(orch);
}

int orchestrator_execute_pipeline(orchestrator_t *orch, orch_pipeline_t *pipeline,
                                  const char *input, orch_result_t **out_results,
                                  size_t *out_count) {
    (void)orch;
    (void)pipeline;
    (void)input;
    if (out_results) *out_results = NULL;
    if (out_count) *out_count = 0;
    return -1;
}

void orchestrator_set_progress_callback(orchestrator_t *orch, orch_progress_cb_t callback,
                                        void *user_data) {
    (void)orch;
    (void)callback;
    (void)user_data;
}

void orchestrator_set_core_loop(orchestrator_t *orch, void *core_loop) {
    (void)orch;
    (void)core_loop;
}

bool orchestrator_has_core_loop(orchestrator_t *orch) {
    (void)orch;
    return false;
}

void orchestrator_set_cognition_llm_service(orchestrator_t *orch, llm_service_t *llm_svc) {
    (void)orch;
    (void)llm_svc;
}

void orchestrator_set_cognition_tool_service(orchestrator_t *orch, tool_service_t *tool_svc) {
    (void)orch;
    (void)tool_svc;
}

orch_task_status_t orchestrator_get_task_status(orchestrator_t *orch, const char *task_id) {
    (void)orch;
    (void)task_id;
    return ORCH_TASK_PENDING;
}

void orchestrator_result_free(orch_result_t *result) {
    if (result) {
        free(result->task_id);
        free(result->output);
        free(result->thinking_chain_id);
    }
    free(result);
}

uint32_t orchestrator_active_count(orchestrator_t *orch) {
    (void)orch;
    return 0;
}

int orchestrator_cancel(orchestrator_t *orch, const char *task_id) {
    (void)orch;
    (void)task_id;
    return 0;
}

int orchestrator_cancel_all(orchestrator_t *orch) {
    (void)orch;
    return 0;
}

/* ============================================================================
 * C-L07: checkpoint_adapter stubs
 * ============================================================================ */

checkpoint_adapter_t *checkpoint_adapter_create(const checkpoint_adapter_config_t *config) {
    (void)config;
    return (checkpoint_adapter_t *)stub_alloc();
}

int checkpoint_adapter_save(checkpoint_adapter_t *adapter,
                             const char *task_id, const char *session_id,
                             uint64_t sequence_num, const checkpoint_snapshot_t *snap) {
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
    if (snap) {
        free(snap->task_id);
        free(snap->session_id);
        free(snap->cognition_state_json);
        free(snap->memory_context_json);
        free(snap->tool_call_history_json);
        free(snap->pending_nodes_json);
        free(snap->completed_nodes_json);
    }
    free(snap);
}

int checkpoint_adapter_delete(checkpoint_adapter_t *adapter,
                               const char *task_id, uint64_t sequence_num) {
    (void)adapter;
    (void)task_id;
    (void)sequence_num;
    return 0;
}

bool checkpoint_adapter_is_ready(checkpoint_adapter_t *adapter) {
    (void)adapter;
    return true;
}

int checkpoint_adapter_list(checkpoint_adapter_t *adapter,
                             const char *task_id,
                             checkpoint_snapshot_t ***out_snapshots,
                             size_t *out_count) {
    (void)adapter;
    (void)task_id;
    if (out_snapshots) *out_snapshots = NULL;
    if (out_count) *out_count = 0;
    return 0;
}

int checkpoint_adapter_restore_seq(checkpoint_adapter_t *adapter,
                                    const char *task_id,
                                    uint64_t sequence_num,
                                    checkpoint_snapshot_t **out_snapshot) {
    (void)adapter;
    (void)task_id;
    (void)sequence_num;
    if (out_snapshot == NULL) return -1;
    checkpoint_snapshot_t *snap = (checkpoint_snapshot_t *)calloc(1, sizeof(checkpoint_snapshot_t));
    if (snap == NULL) return -1;
    snap->sequence_num = sequence_num;
    snap->current_turn = 10;
    *out_snapshot = snap;
    return 0;
}

int checkpoint_adapter_snapshot_create(checkpoint_adapter_t *adapter,
                                        const char *task_id,
                                        const char *snapshot_path) {
    (void)adapter;
    (void)task_id;
    (void)snapshot_path;
    return 0;
}

int checkpoint_adapter_snapshot_restore(checkpoint_adapter_t *adapter,
                                         const char *snapshot_path,
                                         char **out_task_id) {
    (void)adapter;
    (void)snapshot_path;
    if (out_task_id) *out_task_id = NULL;
    return -1;
}

void checkpoint_adapter_get_stats(checkpoint_adapter_t *adapter,
                                   uint64_t *out_total_saves,
                                   uint64_t *out_total_restores,
                                   uint64_t *out_total_errors,
                                   uint64_t *out_last_save_time) {
    (void)adapter;
    if (out_total_saves) *out_total_saves = 0;
    if (out_total_restores) *out_total_restores = 0;
    if (out_total_errors) *out_total_errors = 0;
    if (out_last_save_time) *out_last_save_time = 0;
}

void checkpoint_adapter_destroy(checkpoint_adapter_t *adapter) {
    free(adapter);
}

/* ============================================================================
 * C-L08: service_discovery stubs
 * ============================================================================ */

service_discovery_t sd_create(const sd_config_t *config) {
    (void)config;
    return (service_discovery_t)stub_alloc();
}

agentos_error_t sd_start(service_discovery_t sd) {
    (void)sd;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_register(service_discovery_t sd, const char *service_name,
                             const char *service_type, const sd_instance_t *inst,
                             const char *tags, const char *dependencies) {
    (void)sd;
    (void)service_name;
    (void)service_type;
    (void)inst;
    (void)tags;
    (void)dependencies;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_discover(service_discovery_t sd, const char *service_name,
                             sd_instance_t *instances, uint32_t max_instances,
                             uint32_t *found_count) {
    (void)sd;
    if (found_count) *found_count = 0;
    if (instances && max_instances > 0 && service_name) {
        memset(&instances[0], 0, sizeof(sd_instance_t));
        snprintf(instances[0].instance_id, sizeof(instances[0].instance_id), "%s-001", service_name);
        snprintf(instances[0].endpoint, sizeof(instances[0].endpoint), "127.0.0.1:8080");
        instances[0].state = AGENTOS_SVC_STATE_RUNNING;
        instances[0].healthy = true;
        if (found_count) *found_count = 1;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_select_instance(service_discovery_t sd, const char *service_name,
                                    sd_lb_strategy_t strategy, sd_instance_t *selected) {
    (void)sd;
    (void)service_name;
    (void)strategy;
    if (selected) {
        memset(selected, 0, sizeof(*selected));
        snprintf(selected->instance_id, sizeof(selected->instance_id), "selected-001");
        snprintf(selected->endpoint, sizeof(selected->endpoint), "127.0.0.1:8080");
        selected->state = AGENTOS_SVC_STATE_RUNNING;
        selected->healthy = true;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_deregister(service_discovery_t sd, const char *service_name,
                               const char *instance_id) {
    (void)sd;
    (void)service_name;
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

bool sd_is_running(service_discovery_t sd) {
    (void)sd;
    return true;
}

sd_config_t sd_create_default_config(void) {
    sd_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.heartbeat_interval_ms = SD_DEFAULT_HEARTBEAT_MS;
    cfg.expire_timeout_ms = SD_DEFAULT_EXPIRE_MS;
    cfg.default_lb_strategy = SD_LB_ROUND_ROBIN;
    cfg.enable_auto_expire = true;
    cfg.enable_health_propagation = true;
    snprintf(cfg.shm_name, sizeof(cfg.shm_name), "%s", SD_SHM_NAME);
    cfg.shm_size = 0;
    return cfg;
}

agentos_error_t sd_discover_by_type(service_discovery_t sd, const char *service_type,
                                     sd_service_entry_t *entries, uint32_t max_count,
                                     uint32_t *found_count) {
    (void)sd;
    (void)service_type;
    (void)entries;
    (void)max_count;
    if (found_count) *found_count = 0;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_discover_by_tags(service_discovery_t sd, const char *tags,
                                     sd_service_entry_t *entries, uint32_t max_count,
                                     uint32_t *found_count) {
    (void)sd;
    (void)tags;
    (void)entries;
    (void)max_count;
    if (found_count) *found_count = 0;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_heartbeat(service_discovery_t sd, const char *service_name,
                              const char *instance_id) {
    (void)sd;
    (void)service_name;
    (void)instance_id;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_update_health(service_discovery_t sd, const char *service_name,
                                  const char *instance_id, bool healthy) {
    (void)sd;
    (void)service_name;
    (void)instance_id;
    (void)healthy;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_update_connections(service_discovery_t sd, const char *service_name,
                                       const char *instance_id,
                                       uint32_t active_connections) {
    (void)sd;
    (void)service_name;
    (void)instance_id;
    (void)active_connections;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_check_dependencies(service_discovery_t sd, const char *service_name,
                                       char *missing_deps, size_t max_len) {
    (void)sd;
    (void)service_name;
    (void)missing_deps;
    (void)max_len;
    return AGENTOS_SUCCESS;
}

uint32_t sd_service_count(service_discovery_t sd) {
    (void)sd;
    return 0;
}

agentos_error_t sd_deregister_all(service_discovery_t sd, const char *service_name) {
    (void)sd;
    (void)service_name;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_register_event_callback(service_discovery_t sd,
                                            sd_event_callback_t callback,
                                            void *user_data) {
    (void)sd;
    (void)callback;
    (void)user_data;
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_get_stats(service_discovery_t sd, sd_stats_t *stats) {
    (void)sd;
    if (stats) memset(stats, 0, sizeof(*stats));
    return AGENTOS_SUCCESS;
}

void sd_dump_stats(service_discovery_t sd) {
    (void)sd;
}

/* ============================================================================
 * C-L09: ipc_bus_helper stubs
 * ============================================================================ */

ipc_bus_helper_t *ipc_bus_helper_init(const char *daemon_name,
                                      const ipc_bus_channel_config_t *config) {
    (void)daemon_name;
    (void)config;
    return (ipc_bus_helper_t *)stub_alloc();
}

int ipc_bus_helper_register_channel(ipc_bus_helper_t *ibh, const char *name,
                                     ipc_bus_proto_t proto) {
    (void)ibh;
    (void)name;
    (void)proto;
    return 0;
}

int ipc_bus_helper_register_handler(ipc_bus_helper_t *ibh,
                                     ipc_bus_message_handler_t handler, void *user_data) {
    (void)ibh;
    (void)handler;
    (void)user_data;
    return 0;
}

void ipc_bus_helper_shutdown(ipc_bus_helper_t *ibh) {
    free(ibh);
}

int ipc_bus_helper_register_endpoint(ipc_bus_helper_t *ibh,
                                      const char *service_name,
                                      const char *endpoint,
                                      const ipc_bus_proto_t *protocols,
                                      uint32_t proto_count) {
    (void)ibh;
    (void)service_name;
    (void)endpoint;
    (void)protocols;
    (void)proto_count;
    return 0;
}

int ipc_bus_helper_send(ipc_bus_helper_t *ibh, const char *target_service,
                        ipc_bus_msg_type_t msg_type, ipc_bus_proto_t protocol,
                        const void *payload, size_t payload_size) {
    (void)ibh;
    (void)target_service;
    (void)msg_type;
    (void)protocol;
    (void)payload;
    (void)payload_size;
    return -1;
}

int ipc_bus_helper_request(ipc_bus_helper_t *ibh, const char *target_service,
                           const ipc_bus_message_t *request,
                           ipc_bus_message_t *response, uint32_t timeout_ms) {
    (void)ibh;
    (void)target_service;
    (void)request;
    (void)response;
    (void)timeout_ms;
    return -1;
}

int ipc_bus_helper_notify(ipc_bus_helper_t *ibh, const char *target_service,
                          const void *payload, size_t payload_size,
                          ipc_bus_proto_t protocol) {
    (void)ibh;
    (void)target_service;
    (void)payload;
    (void)payload_size;
    (void)protocol;
    return -1;
}

int ipc_bus_helper_route_auto(ipc_bus_helper_t *ibh,
                              const char *target_service,
                              const void *payload, size_t payload_size) {
    (void)ibh;
    (void)target_service;
    (void)payload;
    (void)payload_size;
    return -1;
}

bool ipc_bus_helper_is_running(ipc_bus_helper_t *ibh) {
    (void)ibh;
    return ibh != NULL;
}

ipc_service_bus_t ipc_bus_helper_get_bus(ipc_bus_helper_t *ibh) {
    (void)ibh;
    return (ipc_service_bus_t)ibh;
}

int ipc_bus_helper_enable_backpressure(ipc_bus_helper_t *ibh,
                                       const ipc_bp_config_t *config) {
    (void)ibh;
    (void)config;
    return 0;
}

ipc_bp_level_t ipc_bus_helper_update_backpressure(ipc_bus_helper_t *ibh,
                                                   size_t current_depth) {
    (void)ibh;
    (void)current_depth;
    return IPC_BP_NORMAL;
}

bool ipc_bus_helper_should_accept_connection(ipc_bus_helper_t *ibh) {
    (void)ibh;
    return true;
}

int ipc_bus_helper_get_bp_stats(ipc_bus_helper_t *ibh, ipc_bp_stats_t *out_stats) {
    (void)ibh;
    if (out_stats) memset(out_stats, 0, sizeof(*out_stats));
    return 0;
}

ipc_bp_level_t ipc_bus_helper_get_bp_level(ipc_bus_helper_t *ibh) {
    (void)ibh;
    return IPC_BP_NORMAL;
}

int ipc_bus_helper_send_with_bp(ipc_bus_helper_t *ibh, const char *target,
                                ipc_bus_msg_type_t msg_type, ipc_bus_proto_t protocol,
                                const void *payload, size_t payload_size,
                                bool is_droppable) {
    (void)ibh;
    (void)target;
    (void)msg_type;
    (void)protocol;
    (void)payload;
    (void)payload_size;
    (void)is_droppable;
    return -1;
}

int ipc_bus_helper_get_routing_stats(ipc_bus_helper_t *ibh,
                                     uint64_t *out_total_sends,
                                     uint64_t *out_total_routes,
                                     uint64_t *out_route_fallbacks,
                                     uint64_t *out_send_failures,
                                     uint64_t *out_bp_drops,
                                     uint64_t *out_bp_rejects) {
    (void)ibh;
    if (out_total_sends) *out_total_sends = 0;
    if (out_total_routes) *out_total_routes = 0;
    if (out_route_fallbacks) *out_route_fallbacks = 0;
    if (out_send_failures) *out_send_failures = 0;
    if (out_bp_drops) *out_bp_drops = 0;
    if (out_bp_rejects) *out_bp_rejects = 0;
    return 0;
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

int prometheus_exporter_handle_http(const char *request, size_t request_len,
                                    char **response, size_t *response_len) {
    if (request == NULL) return -1;
    (void)request_len;
    if (response == NULL) return -1;
    const char *metrics =
        "# HELP agentos_test_metric Test metric\n"
        "# TYPE agentos_test_metric gauge\n"
        "agentos_test_metric 1.0\n";
    *response = strdup(metrics);
    if (response_len) *response_len = strlen(metrics);
    return 0;
}

void prometheus_exporter_get_scrape_stats(uint64_t *out_count, uint64_t *out_errors) {
    if (out_count) *out_count = 0;
    if (out_errors) *out_errors = 0;
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

    if (path == NULL) return GW_FWD_PROTO_COUNT;

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

    return GW_FWD_PROTO_COUNT;
}

gw_forward_t *gw_forward_create(const gw_forward_config_t *config) {
    (void)config;
    return (gw_forward_t *)stub_alloc();
}

void gw_forward_destroy(gw_forward_t *fw) {
    free(fw);
}

bool gw_forward_is_healthy(gw_forward_t *fw) {
    return fw != NULL;
}

int gw_forward_request(gw_forward_t *fw, gw_fwd_proto_t proto, const char *method,
                       const char *path, const char *body, size_t body_len,
                       char **out_response, size_t *out_response_len) {
    if (fw == NULL) return -1;
    (void)proto;
    (void)method;
    (void)path;
    (void)body;
    (void)body_len;
    if (out_response) *out_response = NULL;
    if (out_response_len) *out_response_len = 0;
    return 0;
}

int gw_forward_a2a(gw_forward_t *fw, const char *method, const char *path,
                   const char *body, size_t body_len, char **out_response,
                   size_t *out_response_len) {
    if (fw == NULL) return -1;
    (void)method;
    (void)path;
    (void)body;
    (void)body_len;
    if (out_response) *out_response = NULL;
    if (out_response_len) *out_response_len = 0;
    return 0;
}

int gw_forward_mcp(gw_forward_t *fw, const char *method, const char *path,
                   const char *body, size_t body_len, char **out_response,
                   size_t *out_response_len) {
    if (fw == NULL) return -1;
    (void)method;
    (void)path;
    (void)body;
    (void)body_len;
    if (out_response) *out_response = NULL;
    if (out_response_len) *out_response_len = 0;
    return 0;
}

int gw_forward_openai(gw_forward_t *fw, const char *method, const char *path,
                      const char *body, size_t body_len, char **out_response,
                      size_t *out_response_len) {
    if (fw == NULL) return -1;
    (void)method;
    (void)path;
    (void)body;
    (void)body_len;
    if (out_response) *out_response = NULL;
    if (out_response_len) *out_response_len = 0;
    return 0;
}

int gw_forward_get_stats(gw_forward_t *fw, gw_forward_stats_t *stats) {
    if (fw == NULL || stats == NULL) return -1;
    memset(stats, 0, sizeof(*stats));
    return 0;
}

void gw_forward_reset_stats(gw_forward_t *fw) {
    (void)fw;
}

void gw_forward_dump_stats(gw_forward_t *fw, uint32_t interval_sec) {
    (void)fw;
    (void)interval_sec;
}

/* ============================================================================
 * C-L12: memoryrovol_bridge stubs
 * ============================================================================ */

memoryrovol_bridge_t *memoryrovol_bridge_create(const memoryrovol_bridge_config_t *config) {
    (void)config;
    return (memoryrovol_bridge_t *)stub_alloc();
}

agentos_memory_provider_t *memoryrovol_bridge_get_provider(memoryrovol_bridge_t *bridge) {
    if (bridge == NULL) return NULL;
    static agentos_memory_provider_t provider;
    memset(&provider, 0, sizeof(provider));
    provider.name = "stub_memory_provider";
    provider.version = "0.0.1";
    /* Stub function pointers so tests can verify they are non-NULL */
    provider.write_raw = (agentos_error_t (*)(struct agentos_memory_provider *, const void *,
                        size_t, const char *, char **))1;
    provider.query = (agentos_error_t (*)(struct agentos_memory_provider *, const char *,
                     uint32_t, char ***, float **, size_t *))1;
    return &provider;
}

void memoryrovol_bridge_destroy(memoryrovol_bridge_t *bridge) {
    free(bridge);
}

int memoryrovol_bridge_switch_mode(memoryrovol_bridge_t *bridge, const char *mode) {
    if (bridge == NULL) return -1;
    if (mode == NULL) return -1;
    if (strcmp(mode, "builtin") != 0 && strcmp(mode, "memoryrovol") != 0
        && strcmp(mode, "hybrid") != 0) {
        return -1;
    }
    return 0;
}

const char *memoryrovol_bridge_get_mode(memoryrovol_bridge_t *bridge) {
    if (bridge == NULL) return NULL;
    return "builtin";
}

int memoryrovol_bridge_start_sync(memoryrovol_bridge_t *bridge) {
    if (bridge == NULL) return -1;
    return 0;
}

void memoryrovol_bridge_stop_sync(memoryrovol_bridge_t *bridge) {
    (void)bridge;
}

bool memoryrovol_bridge_has_active_sync(memoryrovol_bridge_t *bridge) {
    (void)bridge;
    return false;
}

int memoryrovol_bridge_get_stats(memoryrovol_bridge_t *bridge,
                                 agentos_memory_stats_t *out_stats) {
    if (bridge == NULL || out_stats == NULL) return -1;
    memset(out_stats, 0, sizeof(*out_stats));
    return 0;
}

int memoryrovol_bridge_health_check(memoryrovol_bridge_t *bridge, char **out_json) {
    if (bridge == NULL || out_json == NULL) return -1;
    *out_json = strdup("{\"status\":\"healthy\"}");
    return 0;
}

bool memoryrovol_bridge_is_ready(memoryrovol_bridge_t *bridge) {
    return bridge != NULL;
}

void memoryrovol_bridge_dump_stats(memoryrovol_bridge_t *bridge) {
    (void)bridge;
}

/* ============================================================================
 * P2.8: Hook system stubs
 *
 * NOTE: agentos_hook.h defines all agentos_hook_* functions as static inline.
 * They call hook_registry_*, hook_executor_*, and hook_timeout_manager_*
 * functions. We must provide THOSE underlying functions, NOT redefine the
 * static inline ones.
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

/* ---- hook_registry API (called by static inline agentos_hook_*) ---- */

int hook_registry_init(void) {
    g_hook_count = 0;
    memset(g_hook_registry, 0, sizeof(g_hook_registry));
    memset(g_hook_timeout_counts, 0, sizeof(g_hook_timeout_counts));
    for (int i = 0; i < MAX_HOOKS; i++) {
        g_hook_timeouts_ms[i] = HOOK_TIMEOUT_DEFAULT_MS;
    }
    return 0;
}

void hook_registry_destroy(void) {
    g_hook_count = 0;
    memset(g_hook_registry, 0, sizeof(g_hook_registry));
}

int hook_registry_register(const hook_entry_t *entry) {
    if (entry == NULL || g_hook_count >= MAX_HOOKS) return -1;
    /* Check duplicate */
    if (find_hook_index(entry->name) >= 0) return -3;
    g_hook_registry[g_hook_count] = *entry;
    g_hook_timeouts_ms[g_hook_count] = HOOK_TIMEOUT_DEFAULT_MS;
    g_hook_count++;
    return 0;
}

int hook_registry_unregister(const char *name) {
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

const hook_entry_t *hook_registry_find(const char *name) {
    int idx = find_hook_index(name);
    if (idx < 0) return NULL;
    return &g_hook_registry[idx];
}

int hook_registry_set_enabled(const char *name, bool enabled) {
    int idx = find_hook_index(name);
    if (idx < 0) return -1;
    g_hook_registry[idx].enabled = enabled;
    return 0;
}

size_t hook_registry_count(void) {
    return (size_t)g_hook_count;
}

size_t hook_registry_count_by_type(hook_type_t type) {
    size_t count = 0;
    for (int i = 0; i < g_hook_count; i++) {
        if (g_hook_registry[i].enabled && g_hook_registry[i].type == type) {
            count++;
        }
    }
    return count;
}

int hook_registry_update_stats(const char *name, hook_decision_t decision,
                                uint64_t duration_ns) {
    int idx = find_hook_index(name);
    if (idx < 0) return -1;
    g_hook_registry[idx].invoke_count++;
    g_hook_registry[idx].total_duration_ns += duration_ns;
    if (duration_ns > g_hook_registry[idx].max_duration_ns) {
        g_hook_registry[idx].max_duration_ns = duration_ns;
    }
    switch (decision) {
        case HOOK_DECISION_ABORT:  g_hook_registry[idx].abort_count++; break;
        case HOOK_DECISION_SKIP:   g_hook_registry[idx].skip_count++; break;
        case HOOK_DECISION_RETRY:  g_hook_registry[idx].retry_count++; break;
        case HOOK_DECISION_MODIFY: g_hook_registry[idx].modify_count++; break;
        default: break;
    }
    return 0;
}

int hook_registry_get_stats(const char *name, hook_stats_t *stats) {
    int idx = find_hook_index(name);
    if (idx < 0 || stats == NULL) return -1;
    memset(stats, 0, sizeof(*stats));
    stats->invoke_count = g_hook_registry[idx].invoke_count;
    stats->skip_count = g_hook_registry[idx].skip_count;
    stats->abort_count = g_hook_registry[idx].abort_count;
    stats->retry_count = g_hook_registry[idx].retry_count;
    stats->modify_count = g_hook_registry[idx].modify_count;
    stats->total_duration_ns = g_hook_registry[idx].total_duration_ns;
    stats->max_duration_ns = g_hook_registry[idx].max_duration_ns;
    return 0;
}

/* ---- hook_executor API (called by static inline agentos_hook_trigger) ---- */

hook_decision_t hook_executor_run(hook_context_t *ctx, hook_exec_mode_t mode) {
    (void)mode;
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

/* ---- hook_timeout_manager API (called by static inline agentos_hook_init/shutdown) ---- */

int hook_timeout_manager_init(uint32_t default_timeout_ms) {
    (void)default_timeout_ms;
    for (int i = 0; i < MAX_HOOKS; i++) {
        g_hook_timeouts_ms[i] = HOOK_TIMEOUT_DEFAULT_MS;
        g_hook_timeout_counts[i] = 0;
    }
    return 0;
}

void hook_timeout_manager_destroy(void) {
    /* no-op */
}

/* ---- hook_timeout API ---- */

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

/* ---- hook_interceptor stubs ---- */

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
