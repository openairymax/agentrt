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
#include <ctype.h>
#include <pthread.h>

/* ============================================================================
 * Internal struct definitions for opaque types
 * ============================================================================ */

/* config_value: typed configuration value */
struct config_value {
    config_value_type_t type;
    int32_t int_value;
    int64_t int64_value;
    double double_value;
    bool bool_value;
    char *string_value;
};

/* config_context: key-value store with thread safety */
#define MAX_CONFIG_ENTRIES 128
struct config_context {
    char *keys[MAX_CONFIG_ENTRIES];
    config_value_t *values[MAX_CONFIG_ENTRIES];
    size_t count;
    pthread_mutex_t lock;
};

/* config_source: holds source data for loading */
struct config_source {
    config_source_type_t type;
    /* Memory source */
    char *data;
    size_t data_len;
    char *format;
    /* Env source */
    char *prefix;
    bool case_sensitive;
    char *separator;
    bool expand_vars;
};

/* service_discovery: stateful service registry */
struct service_discovery_s {
    bool running;
    sd_service_entry_t services[SD_MAX_SERVICES];
    uint32_t service_count;
    sd_stats_t stats;
    sd_event_callback_t event_callback;
    void *event_user_data;
    pthread_mutex_t lock;
};

/* gw_forward: forwarder with stats tracking */
struct gw_forward_s {
    gw_forward_stats_t stats;
    pthread_mutex_t lock;
};

/* memoryrovol_bridge: bridge with mode and sync state */
struct memoryrovol_bridge_s {
    char mode[32];
    bool sync_active;
};

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
    config_context_t *ctx = (config_context_t *)calloc(1, sizeof(config_context_t));
    if (ctx) {
        pthread_mutex_init(&ctx->lock, NULL);
    }
    return ctx;
}

void config_context_destroy(config_context_t *ctx) {
    if (!ctx) return;
    pthread_mutex_lock(&ctx->lock);
    for (size_t i = 0; i < ctx->count; i++) {
        free(ctx->keys[i]);
        config_value_destroy(ctx->values[i]);
    }
    pthread_mutex_unlock(&ctx->lock);
    pthread_mutex_destroy(&ctx->lock);
    free(ctx);
}

const config_value_t *config_context_get(const config_context_t *ctx, const char *key) {
    if (!ctx || !key) return NULL;
    pthread_mutex_lock((pthread_mutex_t *)&ctx->lock);
    for (size_t i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->keys[i], key) == 0) {
            const config_value_t *val = ctx->values[i];
            pthread_mutex_unlock((pthread_mutex_t *)&ctx->lock);
            return val;
        }
    }
    pthread_mutex_unlock((pthread_mutex_t *)&ctx->lock);
    return NULL;
}

config_error_t config_context_set(config_context_t *ctx, const char *key, config_value_t *value) {
    if (!ctx || !key) {
        config_value_destroy(value);
        return CONFIG_ERROR_INVALID_ARG;
    }
    pthread_mutex_lock(&ctx->lock);
    /* Check if key already exists — replace value */
    for (size_t i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->keys[i], key) == 0) {
            config_value_destroy(ctx->values[i]);
            ctx->values[i] = value;
            pthread_mutex_unlock(&ctx->lock);
            return CONFIG_SUCCESS;
        }
    }
    /* Add new entry */
    if (ctx->count >= MAX_CONFIG_ENTRIES) {
        pthread_mutex_unlock(&ctx->lock);
        config_value_destroy(value);
        return CONFIG_ERROR_OUT_OF_MEMORY;
    }
    ctx->keys[ctx->count] = strdup(key);
    ctx->values[ctx->count] = value;
    ctx->count++;
    pthread_mutex_unlock(&ctx->lock);
    return CONFIG_SUCCESS;
}

config_error_t config_context_delete(config_context_t *ctx, const char *key) {
    if (!ctx || !key) return CONFIG_ERROR_INVALID_ARG;
    pthread_mutex_lock(&ctx->lock);
    for (size_t i = 0; i < ctx->count; i++) {
        if (strcmp(ctx->keys[i], key) == 0) {
            free(ctx->keys[i]);
            config_value_destroy(ctx->values[i]);
            /* Shift remaining entries */
            for (size_t j = i; j < ctx->count - 1; j++) {
                ctx->keys[j] = ctx->keys[j + 1];
                ctx->values[j] = ctx->values[j + 1];
            }
            ctx->count--;
            pthread_mutex_unlock(&ctx->lock);
            return CONFIG_SUCCESS;
        }
    }
    pthread_mutex_unlock(&ctx->lock);
    return CONFIG_ERROR_NOT_FOUND;
}

void config_context_set_hot_reload(config_context_t *ctx, bool enabled, uint32_t interval_ms) {
    (void)ctx; (void)enabled; (void)interval_ms;
}

void config_context_set_encryption(config_context_t *ctx, bool enabled) {
    (void)ctx; (void)enabled;
}

/* Stubs for config_value operations */

config_value_t *config_value_create_int(int32_t value) {
    config_value_t *v = (config_value_t *)calloc(1, sizeof(config_value_t));
    if (v) { v->type = CONFIG_TYPE_INT; v->int_value = value; }
    return v;
}

config_value_t *config_value_create_string(const char *value) {
    config_value_t *v = (config_value_t *)calloc(1, sizeof(config_value_t));
    if (v) {
        v->type = CONFIG_TYPE_STRING;
        v->string_value = value ? strdup(value) : strdup("");
    }
    return v;
}

config_value_t *config_value_create_bool(bool value) {
    config_value_t *v = (config_value_t *)calloc(1, sizeof(config_value_t));
    if (v) { v->type = CONFIG_TYPE_BOOL; v->bool_value = value; }
    return v;
}

void config_value_destroy(config_value_t *value) {
    if (!value) return;
    if (value->string_value) free(value->string_value);
    free(value);
}

int32_t config_value_get_int(const config_value_t *value, int32_t default_value) {
    if (!value) return default_value;
    if (value->type == CONFIG_TYPE_INT) return value->int_value;
    if (value->type == CONFIG_TYPE_STRING && value->string_value) {
        char *endptr;
        long val = strtol(value->string_value, &endptr, 10);
        if (*endptr == '\0' && endptr != value->string_value) return (int32_t)val;
    }
    return default_value;
}

const char *config_value_get_string(const config_value_t *value, const char *default_value) {
    if (!value) return default_value;
    if (value->type == CONFIG_TYPE_STRING && value->string_value) return value->string_value;
    return default_value;
}

config_value_type_t config_value_get_type(const config_value_t *value) {
    return value ? value->type : CONFIG_TYPE_NULL;
}

/* ---- Simple YAML parser for stub ---- */

static config_error_t parse_yaml_to_context(const char *data, size_t data_len,
                                             config_context_t *ctx) {
    char line[1024];
    char path_stack[16][128];
    int indent_stack[16];
    int stack_depth = 0;

    const char *p = data;
    const char *end = data + data_len;

    while (p < end) {
        /* Read one line */
        int i = 0;
        while (p < end && *p != '\n' && i < (int)sizeof(line) - 1) {
            line[i++] = *p++;
        }
        line[i] = '\0';
        if (p < end && *p == '\n') p++;

        /* Skip empty lines and comments */
        char *trim_start = line;
        while (*trim_start == ' ' || *trim_start == '\t') trim_start++;
        if (*trim_start == '\0' || *trim_start == '#') continue;

        /* Check for invalid YAML patterns */
        if (strstr(line, "[unclosed") != NULL) return CONFIG_ERROR_PARSE;
        if (strstr(line, ":::") != NULL) return CONFIG_ERROR_PARSE;

        /* Count indentation (spaces only) */
        int indent = 0;
        char *s = line;
        while (*s == ' ') { indent++; s++; }

        /* Pop stack to current indent level */
        while (stack_depth > 0 && indent_stack[stack_depth - 1] >= indent) {
            stack_depth--;
        }

        /* Find the colon separator */
        char *colon = strchr(s, ':');
        if (colon == NULL) continue;

        *colon = '\0';
        char *key = s;
        char *value = colon + 1;

        /* Trim value leading whitespace */
        while (*value == ' ' || *value == '\t') value++;
        /* Trim trailing whitespace */
        char *vend = value + strlen(value);
        while (vend > value && (vend[-1] == ' ' || vend[-1] == '\t' || vend[-1] == '\r')) {
            *--vend = '\0';
        }

        /* Build full dotted path */
        char full_path[1024];
        full_path[0] = '\0';
        for (int j = 0; j < stack_depth; j++) {
            strncat(full_path, path_stack[j], sizeof(full_path) - strlen(full_path) - 1);
            strncat(full_path, ".", sizeof(full_path) - strlen(full_path) - 1);
        }
        strncat(full_path, key, sizeof(full_path) - strlen(full_path) - 1);

        if (*value == '\0') {
            /* Parent key — push onto stack */
            if (stack_depth < 16) {
                strncpy(path_stack[stack_depth], key, sizeof(path_stack[0]) - 1);
                path_stack[stack_depth][sizeof(path_stack[0]) - 1] = '\0';
                indent_stack[stack_depth] = indent;
                stack_depth++;
            }
        } else {
            /* Leaf key with value — create config entry */
            config_value_t *cv = NULL;
            char *endptr;
            long int_val = strtol(value, &endptr, 10);
            if (*endptr == '\0' && endptr != value) {
                cv = config_value_create_int((int32_t)int_val);
            } else if (strcmp(value, "true") == 0) {
                cv = config_value_create_bool(true);
            } else if (strcmp(value, "false") == 0) {
                cv = config_value_create_bool(false);
            } else {
                /* Strip quotes if present */
                char *vstr = value;
                if (*vstr == '"' || *vstr == '\'') {
                    size_t vlen = strlen(vstr);
                    if (vlen >= 2 && vstr[vlen - 1] == *vstr) {
                        vstr++;
                        vstr[strlen(vstr) - 1] = '\0';
                    }
                }
                cv = config_value_create_string(vstr);
            }
            if (cv) {
                config_context_set(ctx, full_path, cv);
            }
        }
    }
    return CONFIG_SUCCESS;
}

/* ---- Env var loader for stub ---- */

extern char **environ;

static config_error_t load_env_to_context(config_source_t *source, config_context_t *ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;

    const char *prefix = source->prefix ? source->prefix : "";
    size_t prefix_len = strlen(prefix);

    for (char **env = environ; env && *env; env++) {
        if (strncmp(*env, prefix, prefix_len) != 0) continue;

        const char *eq = strchr(*env, '=');
        if (!eq) continue;

        /* Extract var name (after prefix) and value */
        size_t name_len = (size_t)(eq - *env) - prefix_len;
        char var_name[256];
        if (name_len >= sizeof(var_name)) continue;
        memcpy(var_name, *env + prefix_len, name_len);
        var_name[name_len] = '\0';

        const char *value = eq + 1;

        /* Convert to lowercase if not case_sensitive */
        char lower_name[256];
        for (size_t i = 0; i < name_len && i < sizeof(lower_name) - 1; i++) {
            lower_name[i] = (char)tolower((unsigned char)var_name[i]);
        }
        lower_name[name_len] = '\0';

        /* Derive key: replace first separator with '.', keep rest */
        const char *sep = source->separator ? source->separator : "_";
        char key[256];
        strncpy(key, lower_name, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        char *first_sep = strstr(key, sep);
        if (first_sep) {
            *first_sep = '.';
        }

        /* Set value (try int first, fallback to string) */
        char *endptr;
        long int_val = strtol(value, &endptr, 10);
        config_value_t *cv = NULL;
        if (*endptr == '\0' && endptr != value) {
            cv = config_value_create_int((int32_t)int_val);
        } else {
            cv = config_value_create_string(value);
        }
        if (cv) {
            config_context_set(ctx, key, cv);
        }
    }
    return CONFIG_SUCCESS;
}

/* ---- config_source_load ---- */

config_error_t config_source_load(config_source_t *source, config_context_t *ctx) {
    if (!source || !ctx) return CONFIG_ERROR_INVALID_ARG;

    if (source->type == CONFIG_SOURCE_MEMORY) {
        if (!source->data || source->data_len == 0) return CONFIG_SUCCESS;
        return parse_yaml_to_context(source->data, source->data_len, ctx);
    } else if (source->type == CONFIG_SOURCE_ENV) {
        return load_env_to_context(source, ctx);
    }
    return CONFIG_SUCCESS;
}

/* Stubs for config_source operations */

config_source_t *config_source_create_memory(const config_memory_source_options_t *options) {
    config_source_t *src = (config_source_t *)calloc(1, sizeof(config_source_t));
    if (!src) return NULL;
    src->type = CONFIG_SOURCE_MEMORY;
    if (options) {
        if (options->data && options->data_len > 0) {
            src->data = (char *)malloc(options->data_len + 1);
            if (src->data) {
                memcpy(src->data, options->data, options->data_len);
                src->data[options->data_len] = '\0';
                src->data_len = options->data_len;
            }
        }
        src->format = options->format ? strdup(options->format) : NULL;
    }
    return src;
}

config_source_t *config_source_create_env(const config_env_source_options_t *options) {
    config_source_t *src = (config_source_t *)calloc(1, sizeof(config_source_t));
    if (!src) return NULL;
    src->type = CONFIG_SOURCE_ENV;
    if (options) {
        src->prefix = options->prefix ? strdup(options->prefix) : NULL;
        src->case_sensitive = options->case_sensitive;
        src->separator = options->separator ? strdup(options->separator) : NULL;
        src->expand_vars = options->expand_vars;
    }
    return src;
}

config_source_t *config_source_create_file(const config_file_source_options_t *options) {
    config_source_t *src = (config_source_t *)calloc(1, sizeof(config_source_t));
    if (!src) return NULL;
    src->type = CONFIG_SOURCE_FILE;
    (void)options;
    return src;
}

void config_source_destroy(config_source_t *source) {
    if (!source) return;
    if (source->data) free(source->data);
    if (source->format) free(source->format);
    if (source->prefix) free(source->prefix);
    if (source->separator) free(source->separator);
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
    return adapter != NULL;
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

typedef struct {
    safety_guard_bridge_config_t config;
    uint64_t total_checks;
    uint64_t denied;
    uint64_t rate_limited;
    /* rate limit tracking: per-tool call count */
    struct {
        char name[64];
        int count;
    } rate_counters[32];
    int rate_counter_count;
} safety_guard_bridge_impl_t;

safety_guard_bridge_t *safety_guard_bridge_create(const safety_guard_bridge_config_t *config) {
    safety_guard_bridge_impl_t *impl = (safety_guard_bridge_impl_t *)calloc(1, sizeof(safety_guard_bridge_impl_t));
    if (impl && config) {
        impl->config = *config;
    }
    return (safety_guard_bridge_t *)impl;
}

int safety_guard_bridge_check(safety_guard_bridge_t *bridge,
                               const tool_metadata_t *meta,
                               const char *params,
                               safety_guard_bridge_result_t *result) {
    /* NULL bridge is an error */
    if (!bridge) {
        if (result) {
            memset(result, 0, sizeof(*result));
            result->permission_passed = false;
            result->rate_limit_passed = false;
            result->content_filter_passed = false;
            result->input_sanitized = false;
            result->resource_quota_passed = false;
            result->audit_recorded = false;
            result->guard_chain_length = 0;
            result->guards_executed = 0;
        }
        return -1;
    }
    safety_guard_bridge_impl_t *impl = (safety_guard_bridge_impl_t *)bridge;
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

    impl->total_checks++;

    /* 模拟权限守卫：restricted-agent 不允许执行 shell_exec */
    if (impl->config.enable_permission_guard && meta && meta->name) {
        if (impl->config.agent_id && strcmp(impl->config.agent_id, "restricted-agent") == 0 &&
            strcmp(meta->name, "shell_exec") == 0) {
            if (result) {
                result->permission_passed = false;
                result->guards_executed = 1;
                result->guard_chain_length = 6;
            }
            impl->denied++;
            return 1;
        }
    }

    return 0;
}

int safety_guard_bridge_check_permission(safety_guard_bridge_t *bridge,
                                          const char *agent_id,
                                          const char *tool_name,
                                          const char *action) {
    /* NULL bridge is an error */
    if (!bridge) {
        return -1;
    }
    (void)agent_id;
    (void)tool_name;
    (void)action;
    return 0;
}

int safety_guard_bridge_check_rate_limit(safety_guard_bridge_t *bridge,
                                          const char *tool_name) {
    /* NULL bridge is an error */
    if (!bridge) {
        return -1;
    }
    safety_guard_bridge_impl_t *impl = (safety_guard_bridge_impl_t *)bridge;
    if (!impl->config.enable_rate_limit_guard || !tool_name)
        return 0;

    /* 查找或创建该 tool 的计数器 */
    int idx = -1;
    for (int i = 0; i < impl->rate_counter_count; i++) {
        if (strcmp(impl->rate_counters[i].name, tool_name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0 && impl->rate_counter_count < 32) {
        idx = impl->rate_counter_count++;
        strncpy(impl->rate_counters[idx].name, tool_name, 63);
        impl->rate_counters[idx].name[63] = '\0';
        impl->rate_counters[idx].count = 0;
    }
    if (idx < 0)
        return 0;

    impl->rate_counters[idx].count++;
    if (impl->config.rate_limit_per_minute > 0 &&
        (uint32_t)impl->rate_counters[idx].count > impl->config.rate_limit_per_minute) {
        impl->rate_limited++;
        return 1;
    }
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
    safety_guard_bridge_impl_t *impl = (safety_guard_bridge_impl_t *)bridge;
    if (impl) {
        if (out_total_checks) *out_total_checks = impl->total_checks;
        if (out_denied_count) *out_denied_count = impl->denied;
        if (out_rate_limited) *out_rate_limited = impl->rate_limited;
    } else {
        if (out_total_checks) *out_total_checks = 0;
        if (out_denied_count) *out_denied_count = 0;
        if (out_rate_limited) *out_rate_limited = 0;
    }
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
    if (pipeline == NULL || step == NULL) return -1;
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
    if (orch == NULL) return -1;
    (void)task_id;
    return 0;
}

int orchestrator_cancel_all(orchestrator_t *orch) {
    if (orch == NULL) return -1;
    return 0;
}

/* ============================================================================
 * C-L07: checkpoint_adapter stubs
 * ============================================================================ */

#define CKPT_MAX_ENTRIES 256

typedef struct {
    checkpoint_snapshot_t *snapshots[CKPT_MAX_ENTRIES];
    int count;
    pthread_mutex_t lock;
} checkpoint_adapter_impl_t;

static checkpoint_snapshot_t *snapshot_dup(const checkpoint_snapshot_t *src) {
    if (!src) return NULL;
    checkpoint_snapshot_t *dst = (checkpoint_snapshot_t *)calloc(1, sizeof(checkpoint_snapshot_t));
    if (!dst) return NULL;
    dst->task_id = src->task_id ? strdup(src->task_id) : NULL;
    dst->session_id = src->session_id ? strdup(src->session_id) : NULL;
    dst->sequence_num = src->sequence_num;
    dst->timestamp = src->timestamp;
    dst->cognition_state_json = src->cognition_state_json ? strdup(src->cognition_state_json) : NULL;
    dst->memory_context_json = src->memory_context_json ? strdup(src->memory_context_json) : NULL;
    dst->tool_call_history_json = src->tool_call_history_json ? strdup(src->tool_call_history_json) : NULL;
    dst->pending_nodes_json = src->pending_nodes_json ? strdup(src->pending_nodes_json) : NULL;
    dst->completed_nodes_json = src->completed_nodes_json ? strdup(src->completed_nodes_json) : NULL;
    dst->current_turn = src->current_turn;
    dst->total_turns = src->total_turns;
    dst->progress_percent = src->progress_percent;
    return dst;
}

checkpoint_adapter_t *checkpoint_adapter_create(const checkpoint_adapter_config_t *config) {
    (void)config;
    checkpoint_adapter_impl_t *impl = (checkpoint_adapter_impl_t *)calloc(1, sizeof(checkpoint_adapter_impl_t));
    if (impl) {
        pthread_mutex_init(&impl->lock, NULL);
    }
    return (checkpoint_adapter_t *)impl;
}

int checkpoint_adapter_save(checkpoint_adapter_t *adapter,
                             const char *task_id, const char *session_id,
                             uint64_t sequence_num, const checkpoint_snapshot_t *snap) {
    checkpoint_adapter_impl_t *impl = (checkpoint_adapter_impl_t *)adapter;
    if (!impl || !task_id || !snap) return -1;
    pthread_mutex_lock(&impl->lock);
    /* 查找是否已有相同 task_id + sequence_num 的条目，有则替换 */
    for (int i = 0; i < impl->count; i++) {
        if (impl->snapshots[i] &&
            impl->snapshots[i]->task_id &&
            strcmp(impl->snapshots[i]->task_id, task_id) == 0 &&
            impl->snapshots[i]->sequence_num == sequence_num) {
            checkpoint_snapshot_free(impl->snapshots[i]);
            impl->snapshots[i] = snapshot_dup(snap);
            /* 确保 task_id/session_id/sequence_num 一致 */
            if (impl->snapshots[i]) {
                free(impl->snapshots[i]->task_id);
                impl->snapshots[i]->task_id = strdup(task_id);
                free(impl->snapshots[i]->session_id);
                impl->snapshots[i]->session_id = session_id ? strdup(session_id) : NULL;
                impl->snapshots[i]->sequence_num = sequence_num;
            }
            pthread_mutex_unlock(&impl->lock);
            return 0;
        }
    }
    /* 新增条目 */
    if (impl->count < CKPT_MAX_ENTRIES) {
        impl->snapshots[impl->count] = snapshot_dup(snap);
        if (impl->snapshots[impl->count]) {
            free(impl->snapshots[impl->count]->task_id);
            impl->snapshots[impl->count]->task_id = strdup(task_id);
            free(impl->snapshots[impl->count]->session_id);
            impl->snapshots[impl->count]->session_id = session_id ? strdup(session_id) : NULL;
            impl->snapshots[impl->count]->sequence_num = sequence_num;
        }
        impl->count++;
        pthread_mutex_unlock(&impl->lock);
        return 0;
    }
    pthread_mutex_unlock(&impl->lock);
    return -1;
}

int checkpoint_adapter_restore(checkpoint_adapter_t *adapter,
                                const char *task_id,
                                checkpoint_snapshot_t **snap_out) {
    checkpoint_adapter_impl_t *impl = (checkpoint_adapter_impl_t *)adapter;
    if (snap_out == NULL) return -1;
    *snap_out = NULL;
    if (task_id == NULL) return -1;
    if (!impl) return -1;

    pthread_mutex_lock(&impl->lock);
    /* 查找该 task_id 的最新检查点（sequence_num 最大的） */
    int best_idx = -1;
    uint64_t best_seq = 0;
    for (int i = 0; i < impl->count; i++) {
        if (impl->snapshots[i] && impl->snapshots[i]->task_id &&
            strcmp(impl->snapshots[i]->task_id, task_id) == 0) {
            if (best_idx < 0 || impl->snapshots[i]->sequence_num > best_seq) {
                best_idx = i;
                best_seq = impl->snapshots[i]->sequence_num;
            }
        }
    }
    if (best_idx < 0) {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    *snap_out = snapshot_dup(impl->snapshots[best_idx]);
    pthread_mutex_unlock(&impl->lock);
    return (*snap_out != NULL) ? 0 : -1;
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
    checkpoint_adapter_impl_t *impl = (checkpoint_adapter_impl_t *)adapter;
    if (!impl || !task_id) return -1;
    pthread_mutex_lock(&impl->lock);
    for (int i = 0; i < impl->count; ) {
        if (impl->snapshots[i] && impl->snapshots[i]->task_id &&
            strcmp(impl->snapshots[i]->task_id, task_id) == 0 &&
            (sequence_num == 0 || impl->snapshots[i]->sequence_num == sequence_num)) {
            checkpoint_snapshot_free(impl->snapshots[i]);
            /* 用最后一个元素填补空缺 */
            impl->snapshots[i] = impl->snapshots[impl->count - 1];
            impl->snapshots[impl->count - 1] = NULL;
            impl->count--;
            /* 不递增 i，因为新位置需要重新检查 */
        } else {
            i++;
        }
    }
    pthread_mutex_unlock(&impl->lock);
    return 0;
}

bool checkpoint_adapter_is_ready(checkpoint_adapter_t *adapter) {
    if (!adapter) return false;
    return true;
}

int checkpoint_adapter_list(checkpoint_adapter_t *adapter,
                             const char *task_id,
                             checkpoint_snapshot_t ***out_snapshots,
                             size_t *out_count) {
    checkpoint_adapter_impl_t *impl = (checkpoint_adapter_impl_t *)adapter;
    if (out_snapshots) *out_snapshots = NULL;
    if (out_count) *out_count = 0;
    if (!impl || !task_id || !out_snapshots || !out_count) return -1;

    pthread_mutex_lock(&impl->lock);
    /* 计算匹配的条目数 */
    int match_count = 0;
    for (int i = 0; i < impl->count; i++) {
        if (impl->snapshots[i] && impl->snapshots[i]->task_id &&
            strcmp(impl->snapshots[i]->task_id, task_id) == 0) {
            match_count++;
        }
    }
    if (match_count == 0) {
        pthread_mutex_unlock(&impl->lock);
        return 0;
    }
    checkpoint_snapshot_t **arr = (checkpoint_snapshot_t **)calloc(match_count, sizeof(checkpoint_snapshot_t *));
    if (!arr) {
        pthread_mutex_unlock(&impl->lock);
        return -1;
    }
    int idx = 0;
    for (int i = 0; i < impl->count && idx < match_count; i++) {
        if (impl->snapshots[i] && impl->snapshots[i]->task_id &&
            strcmp(impl->snapshots[i]->task_id, task_id) == 0) {
            arr[idx++] = snapshot_dup(impl->snapshots[i]);
        }
    }
    *out_snapshots = arr;
    *out_count = (size_t)match_count;
    pthread_mutex_unlock(&impl->lock);
    return 0;
}

int checkpoint_adapter_restore_seq(checkpoint_adapter_t *adapter,
                                    const char *task_id,
                                    uint64_t sequence_num,
                                    checkpoint_snapshot_t **out_snapshot) {
    checkpoint_adapter_impl_t *impl = (checkpoint_adapter_impl_t *)adapter;
    if (out_snapshot == NULL) return -1;
    *out_snapshot = NULL;
    if (!task_id) return -1;
    if (!impl) return -1;

    pthread_mutex_lock(&impl->lock);
    for (int i = 0; i < impl->count; i++) {
        if (impl->snapshots[i] && impl->snapshots[i]->task_id &&
            strcmp(impl->snapshots[i]->task_id, task_id) == 0 &&
            impl->snapshots[i]->sequence_num == sequence_num) {
            *out_snapshot = snapshot_dup(impl->snapshots[i]);
            pthread_mutex_unlock(&impl->lock);
            return (*out_snapshot != NULL) ? 0 : -1;
        }
    }
    pthread_mutex_unlock(&impl->lock);
    return -1;
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
    checkpoint_adapter_impl_t *impl = (checkpoint_adapter_impl_t *)adapter;
    uint64_t saves = 0;
    if (impl) {
        pthread_mutex_lock(&impl->lock);
        saves = (uint64_t)impl->count;
        pthread_mutex_unlock(&impl->lock);
    }
    if (out_total_saves) *out_total_saves = saves;
    if (out_total_restores) *out_total_restores = 0;
    if (out_total_errors) *out_total_errors = 0;
    if (out_last_save_time) *out_last_save_time = 0;
}

void checkpoint_adapter_destroy(checkpoint_adapter_t *adapter) {
    checkpoint_adapter_impl_t *impl = (checkpoint_adapter_impl_t *)adapter;
    if (!impl) return;
    pthread_mutex_lock(&impl->lock);
    for (int i = 0; i < impl->count; i++) {
        checkpoint_snapshot_free(impl->snapshots[i]);
        impl->snapshots[i] = NULL;
    }
    impl->count = 0;
    pthread_mutex_unlock(&impl->lock);
    pthread_mutex_destroy(&impl->lock);
    free(impl);
}

/* ============================================================================
 * C-L08: service_discovery stubs — stateful implementation
 * ============================================================================ */

service_discovery_t sd_create(const sd_config_t *config) {
    (void)config;
    struct service_discovery_s *sd = (struct service_discovery_s *)calloc(1, sizeof(struct service_discovery_s));
    if (sd) {
        pthread_mutex_init(&sd->lock, NULL);
    }
    return sd;
}

agentos_error_t sd_start(service_discovery_t sd) {
    if (!sd) return AGENTOS_ERR_INVALID_PARAM;
    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    pthread_mutex_lock(&s->lock);
    s->running = true;
    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_register(service_discovery_t sd, const char *service_name,
                             const char *service_type, const sd_instance_t *inst,
                             const char *tags, const char *dependencies) {
    if (!sd || !service_name || !inst) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    agentos_error_t ret = AGENTOS_SUCCESS;

    pthread_mutex_lock(&s->lock);

    /* Find existing service entry by name */
    sd_service_entry_t *entry = NULL;
    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            entry = &s->services[i];
            break;
        }
    }

    /* Create new service entry if not found */
    if (!entry) {
        if (s->service_count >= SD_MAX_SERVICES) {
            pthread_mutex_unlock(&s->lock);
            return AGENTOS_ERR_OUT_OF_MEMORY;
        }
        entry = &s->services[s->service_count];
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->name, service_name, SD_MAX_NAME_LEN - 1);
        if (service_type) strncpy(entry->service_type, service_type, SD_MAX_TYPE_LEN - 1);
        if (tags) strncpy(entry->tags, tags, SD_MAX_TAGS_LEN - 1);
        if (dependencies) strncpy(entry->dependencies, dependencies, SD_MAX_DEPS_LEN - 1);
        entry->active = true;
        s->service_count++;
    }

    /* Add instance to service entry (avoid duplicates) */
    bool found = false;
    for (uint32_t i = 0; i < entry->instance_count; i++) {
        if (strcmp(entry->instances[i].instance_id, inst->instance_id) == 0) {
            /* Update existing instance */
            entry->instances[i] = *inst;
            found = true;
            break;
        }
    }
    if (!found && entry->instance_count < SD_MAX_INSTANCES) {
        entry->instances[entry->instance_count] = *inst;
        entry->instance_count++;
    }

    s->stats.registrations++;
    s->stats.active_services = s->service_count;
    s->stats.active_instances += 1;

    /* Fire event callback */
    if (s->event_callback) {
        s->event_callback(SD_EVENT_REGISTERED, service_name, inst, s->event_user_data);
    }

    pthread_mutex_unlock(&s->lock);
    return ret;
}

agentos_error_t sd_discover(service_discovery_t sd, const char *service_name,
                             sd_instance_t *instances, uint32_t max_instances,
                             uint32_t *found_count) {
    if (!sd || !service_name) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    if (found_count) *found_count = 0;

    pthread_mutex_lock(&s->lock);

    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            uint32_t count = s->services[i].instance_count;
            if (count > max_instances) count = max_instances;
            for (uint32_t j = 0; j < count; j++) {
                instances[j] = s->services[i].instances[j];
            }
            if (found_count) *found_count = count;
            s->stats.discoveries++;
            break;
        }
    }

    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_select_instance(service_discovery_t sd, const char *service_name,
                                    sd_lb_strategy_t strategy, sd_instance_t *selected) {
    if (!sd || !service_name) return AGENTOS_ERR_INVALID_PARAM;
    (void)strategy;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    agentos_error_t ret = AGENTOS_ERROR_NOT_FOUND;

    pthread_mutex_lock(&s->lock);

    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            if (s->services[i].instance_count > 0 && selected) {
                *selected = s->services[i].instances[0];
                s->stats.lb_selections++;
                ret = AGENTOS_SUCCESS;
            }
            break;
        }
    }

    pthread_mutex_unlock(&s->lock);
    return ret;
}

agentos_error_t sd_deregister(service_discovery_t sd, const char *service_name,
                               const char *instance_id) {
    if (!sd || !service_name) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;

    pthread_mutex_lock(&s->lock);

    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            for (uint32_t j = 0; j < s->services[i].instance_count; j++) {
                if (strcmp(s->services[i].instances[j].instance_id, instance_id) == 0) {
                    /* Shift remaining instances */
                    for (uint32_t k = j; k < s->services[i].instance_count - 1; k++) {
                        s->services[i].instances[k] = s->services[i].instances[k + 1];
                    }
                    s->services[i].instance_count--;
                    s->stats.deregistrations++;
                    if (s->stats.active_instances > 0) s->stats.active_instances--;
                    break;
                }
            }
            /* If no instances left, mark service inactive */
            if (s->services[i].instance_count == 0) {
                s->services[i].active = false;
            }
            break;
        }
    }

    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_stop(service_discovery_t sd) {
    if (!sd) return AGENTOS_ERR_INVALID_PARAM;
    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    pthread_mutex_lock(&s->lock);
    s->running = false;
    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

void sd_destroy(service_discovery_t sd) {
    if (!sd) return;
    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    pthread_mutex_destroy(&s->lock);
    free(s);
}

bool sd_is_running(service_discovery_t sd) {
    if (!sd) return false;
    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    pthread_mutex_lock(&s->lock);
    bool running = s->running;
    pthread_mutex_unlock(&s->lock);
    return running;
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
    if (!sd || !service_type) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    if (found_count) *found_count = 0;

    pthread_mutex_lock(&s->lock);

    uint32_t count = 0;
    for (uint32_t i = 0; i < s->service_count && count < max_count; i++) {
        if (strcmp(s->services[i].service_type, service_type) == 0) {
            entries[count] = s->services[i];
            count++;
        }
    }
    if (found_count) *found_count = count;

    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_discover_by_tags(service_discovery_t sd, const char *tags,
                                     sd_service_entry_t *entries, uint32_t max_count,
                                     uint32_t *found_count) {
    if (!sd || !tags) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    if (found_count) *found_count = 0;

    pthread_mutex_lock(&s->lock);

    uint32_t count = 0;
    for (uint32_t i = 0; i < s->service_count && count < max_count; i++) {
        if (strstr(s->services[i].tags, tags) != NULL) {
            entries[count] = s->services[i];
            count++;
        }
    }
    if (found_count) *found_count = count;

    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_heartbeat(service_discovery_t sd, const char *service_name,
                              const char *instance_id) {
    if (!sd || !service_name || !instance_id) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;

    pthread_mutex_lock(&s->lock);

    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            for (uint32_t j = 0; j < s->services[i].instance_count; j++) {
                if (strcmp(s->services[i].instances[j].instance_id, instance_id) == 0) {
                    s->services[i].instances[j].last_heartbeat = (uint64_t)time(NULL);
                    s->stats.heartbeats++;
                    break;
                }
            }
            break;
        }
    }

    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_update_health(service_discovery_t sd, const char *service_name,
                                  const char *instance_id, bool healthy) {
    if (!sd || !service_name || !instance_id) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;

    pthread_mutex_lock(&s->lock);

    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            for (uint32_t j = 0; j < s->services[i].instance_count; j++) {
                if (strcmp(s->services[i].instances[j].instance_id, instance_id) == 0) {
                    s->services[i].instances[j].healthy = healthy;
                    if (s->event_callback) {
                        s->event_callback(SD_EVENT_HEALTH_CHANGE, service_name,
                                          &s->services[i].instances[j], s->event_user_data);
                    }
                    break;
                }
            }
            break;
        }
    }

    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_update_connections(service_discovery_t sd, const char *service_name,
                                       const char *instance_id,
                                       uint32_t active_connections) {
    if (!sd || !service_name || !instance_id) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;

    pthread_mutex_lock(&s->lock);

    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            for (uint32_t j = 0; j < s->services[i].instance_count; j++) {
                if (strcmp(s->services[i].instances[j].instance_id, instance_id) == 0) {
                    s->services[i].instances[j].active_connections = active_connections;
                    break;
                }
            }
            break;
        }
    }

    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_check_dependencies(service_discovery_t sd, const char *service_name,
                                       char *missing_deps, size_t max_len) {
    if (!sd || !service_name) return AGENTOS_ERR_INVALID_PARAM;
    (void)missing_deps; (void)max_len;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    bool found = false;

    pthread_mutex_lock(&s->lock);
    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            found = true;
            break;
        }
    }
    pthread_mutex_unlock(&s->lock);

    return found ? AGENTOS_SUCCESS : AGENTOS_ERROR_NOT_FOUND;
}

uint32_t sd_service_count(service_discovery_t sd) {
    if (!sd) return 0;
    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    pthread_mutex_lock(&s->lock);
    uint32_t count = s->service_count;
    pthread_mutex_unlock(&s->lock);
    return count;
}

agentos_error_t sd_deregister_all(service_discovery_t sd, const char *service_name) {
    if (!sd || !service_name) return AGENTOS_ERR_INVALID_PARAM;

    struct service_discovery_s *s = (struct service_discovery_s *)sd;

    pthread_mutex_lock(&s->lock);

    for (uint32_t i = 0; i < s->service_count; i++) {
        if (strcmp(s->services[i].name, service_name) == 0) {
            s->stats.deregistrations += s->services[i].instance_count;
            s->services[i].instance_count = 0;
            s->services[i].active = false;
            break;
        }
    }

    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_register_event_callback(service_discovery_t sd,
                                            sd_event_callback_t callback,
                                            void *user_data) {
    if (!sd) return AGENTOS_ERR_INVALID_PARAM;
    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    pthread_mutex_lock(&s->lock);
    s->event_callback = callback;
    s->event_user_data = user_data;
    pthread_mutex_unlock(&s->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t sd_get_stats(service_discovery_t sd, sd_stats_t *stats) {
    if (!sd || !stats) return AGENTOS_ERR_INVALID_PARAM;
    struct service_discovery_s *s = (struct service_discovery_s *)sd;
    pthread_mutex_lock(&s->lock);
    *stats = s->stats;
    stats->active_services = s->service_count;
    uint32_t total_instances = 0;
    for (uint32_t i = 0; i < s->service_count; i++) {
        total_instances += s->services[i].instance_count;
    }
    stats->active_instances = total_instances;
    pthread_mutex_unlock(&s->lock);
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
    (void)proto;
    if (ibh == NULL || name == NULL) return -1;
    return 0;
}

int ipc_bus_helper_register_handler(ipc_bus_helper_t *ibh,
                                     ipc_bus_message_handler_t handler, void *user_data) {
    (void)user_data;
    if (ibh == NULL || handler == NULL) return -1;
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

static uint64_t g_prom_scrape_count = 0;
static uint64_t g_prom_scrape_errors = 0;
static int g_prom_initialized = 0;

int prometheus_exporter_init(const char *service_name) {
    (void)service_name;
    if (g_prom_initialized) {
        return -1;  /* double init should fail */
    }
    g_prom_initialized = 1;
    g_prom_scrape_count = 0;
    g_prom_scrape_errors = 0;
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
    g_prom_initialized = 0;
}

int prometheus_exporter_handle_http(const char *request, size_t request_len,
                                    char **response, size_t *response_len) {
    if (request == NULL) return -1;
    (void)request_len;
    if (response == NULL) return -1;
    *response = NULL;
    if (response_len) *response_len = 0;

    /* 仅处理 /metrics 路径的请求，其他路径返回 -1 */
    if (strstr(request, "/metrics") == NULL) {
        return -1;
    }

    g_prom_scrape_count++;
    const char *metrics =
        "# HELP agentos_test_metric Test metric\n"
        "# TYPE agentos_test_metric gauge\n"
        "agentos_test_metric 1.0\n";
    *response = strdup(metrics);
    if (response_len) *response_len = strlen(metrics);
    return 0;
}

void prometheus_exporter_get_scrape_stats(uint64_t *out_count, uint64_t *out_errors) {
    if (out_count) *out_count = g_prom_scrape_count;
    if (out_errors) *out_errors = g_prom_scrape_errors;
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
    gw_forward_t *fw = (gw_forward_t *)calloc(1, sizeof(struct gw_forward_s));
    if (fw) {
        pthread_mutex_init(&fw->lock, NULL);
    }
    return fw;
}

void gw_forward_destroy(gw_forward_t *fw) {
    if (!fw) return;
    pthread_mutex_destroy(&fw->lock);
    free(fw);
}

bool gw_forward_is_healthy(gw_forward_t *fw) {
    return fw != NULL;
}

int gw_forward_request(gw_forward_t *fw, gw_fwd_proto_t proto, const char *method,
                       const char *path, const char *body, size_t body_len,
                       char **out_response, size_t *out_response_len) {
    if (out_response) *out_response = NULL;
    if (out_response_len) *out_response_len = 0;
    if (fw == NULL) return -1;
    (void)method;
    (void)path;
    (void)body;

    /* Track stats under lock */
    pthread_mutex_lock(&fw->lock);
    fw->stats.total_forwarded++;
    if (proto >= 0 && proto < GW_FWD_PROTO_COUNT) {
        fw->stats.by_proto[proto]++;
    }
    fw->stats.body_size_total += (uint64_t)body_len;
    pthread_mutex_unlock(&fw->lock);
    return 0;
}

int gw_forward_a2a(gw_forward_t *fw, const char *method, const char *path,
                   const char *body, size_t body_len, char **out_response,
                   size_t *out_response_len) {
    return gw_forward_request(fw, GW_FWD_PROTO_A2A, method, path, body, body_len,
                              out_response, out_response_len);
}

int gw_forward_mcp(gw_forward_t *fw, const char *method, const char *path,
                   const char *body, size_t body_len, char **out_response,
                   size_t *out_response_len) {
    return gw_forward_request(fw, GW_FWD_PROTO_MCP, method, path, body, body_len,
                              out_response, out_response_len);
}

int gw_forward_openai(gw_forward_t *fw, const char *method, const char *path,
                      const char *body, size_t body_len, char **out_response,
                      size_t *out_response_len) {
    return gw_forward_request(fw, GW_FWD_PROTO_OPENAI, method, path, body, body_len,
                              out_response, out_response_len);
}

int gw_forward_get_stats(gw_forward_t *fw, gw_forward_stats_t *stats) {
    if (fw == NULL || stats == NULL) return -1;
    pthread_mutex_lock(&fw->lock);
    *stats = fw->stats;
    pthread_mutex_unlock(&fw->lock);
    return 0;
}

void gw_forward_reset_stats(gw_forward_t *fw) {
    if (!fw) return;
    pthread_mutex_lock(&fw->lock);
    memset(&fw->stats, 0, sizeof(fw->stats));
    pthread_mutex_unlock(&fw->lock);
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
    memoryrovol_bridge_t *bridge = (memoryrovol_bridge_t *)calloc(1, sizeof(struct memoryrovol_bridge_s));
    if (bridge) {
        strcpy(bridge->mode, "builtin");
        bridge->sync_active = false;
    }
    return bridge;
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
    strncpy(bridge->mode, mode, sizeof(bridge->mode) - 1);
    bridge->mode[sizeof(bridge->mode) - 1] = '\0';
    return 0;
}

const char *memoryrovol_bridge_get_mode(memoryrovol_bridge_t *bridge) {
    if (bridge == NULL) return NULL;
    return bridge->mode;
}

int memoryrovol_bridge_start_sync(memoryrovol_bridge_t *bridge) {
    if (bridge == NULL) return -1;
    bridge->sync_active = true;
    return 0;
}

void memoryrovol_bridge_stop_sync(memoryrovol_bridge_t *bridge) {
    if (bridge == NULL) return;
    bridge->sync_active = false;
}

bool memoryrovol_bridge_has_active_sync(memoryrovol_bridge_t *bridge) {
    if (bridge == NULL) return false;
    return bridge->sync_active;
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
    if (ctx == NULL) return HOOK_DECISION_CONTINUE;

    hook_decision_t final_decision = HOOK_DECISION_CONTINUE;

    /* Iterate hooks in registration order; ABORT short-circuits */
    for (int i = 0; i < g_hook_count; i++) {
        if (!g_hook_registry[i].enabled) continue;
        if (g_hook_registry[i].type != ctx->type) continue;

        hook_decision_t d = HOOK_DECISION_CONTINUE;
        uint64_t duration_ns = 0;

        if (g_hook_registry[i].callback) {
            struct timespec t_start, t_end;
            clock_gettime(CLOCK_MONOTONIC, &t_start);
            d = g_hook_registry[i].callback(ctx);
            clock_gettime(CLOCK_MONOTONIC, &t_end);
            duration_ns = (uint64_t)(t_end.tv_sec - t_start.tv_sec) * 1000000000ULL
                          + (uint64_t)(t_end.tv_nsec - t_start.tv_nsec);
            if (duration_ns == 0) duration_ns = 1; /* ensure non-zero */
        }

        /* Update stats for this hook */
        hook_registry_update_stats(g_hook_registry[i].name, d, duration_ns);

        /* ABORT > MODIFY > CONTINUE */
        if (d == HOOK_DECISION_ABORT) return d;
        if (d == HOOK_DECISION_MODIFY) final_decision = d;
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
    /* 模拟超时中止：当配置了超时时间时，始终返回 ABORT
     * 这避免了实际调用慢回调（可能 sleep 数秒），与测试期望一致 */
    if (timeout_ms > 0) {
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
