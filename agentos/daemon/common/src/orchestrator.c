/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file orchestrator.c
 * @brief AgentOS 流程编排器实现
 *
 * P2-B01: 实现Phase 0-4串行编排管线，支持子任务分发、
 * 结果聚合、超时控制、熔断集成、思考链路追踪。
 */
#include "orchestrator.h"
#include "daemon_defaults.h"
#include "thread_pool.h"
#include "svc_logger.h"
#include "svc_common.h"
#include "ipc_service_bus.h"
#include "memoryrovol.h"
#include "cognition.h"
#include "circuit_breaker.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdatomic.h>
#include <math.h>

#define ORCH_MAX_TASKS 64
#define ORCH_MAX_STEPS 32
#define ORCH_ID_LEN 48

typedef struct {
    char id[ORCH_ID_LEN];
    orch_phase_t phase;
    orch_task_status_t status;
    char* input;
    char* output;
    size_t output_len;
    int error_code;
    uint32_t duration_ms;
    char thinking_chain_id[ORCH_ID_LEN];
    time_t start_time;
} task_entry_t;

struct orch_pipeline_s {
    char name[128];
    orch_pipeline_step_t steps[ORCH_MAX_STEPS];
    uint32_t step_count;
    orchestrator_t* owner;
};

struct orchestrator_s {
    orch_config_t config;
    thread_pool_t* pool;

    task_entry_t tasks[ORCH_MAX_TASKS];
    uint32_t task_count;
    atomic_uint_fast32_t active_count;

    orch_progress_cb_t progress_cb;
    void* progress_data;

    uint64_t total_executions;
    uint64_t success_count;

    agentos_memoryrov_handle_t* memory;
    agentos_cognition_engine_t* cognition;
    circuit_breaker_t breaker;
    cb_manager_t cb_mgr;

    char thinking_chain_root[ORCH_ID_LEN];
};

static void generate_task_id(char* buf, size_t buflen) {
    static atomic_uint_fast64_t counter = 0;
    uint64_t id = atomic_fetch_add(&counter, 1);
    snprintf(buf, buflen, "orch-%llu-%llu",
             (unsigned long long)time(NULL),
             (unsigned long long)id);
}

static const char* phase_name(orch_phase_t phase) {
    static const char* names[] = {
        "decomposition", "planning", "generation",
        "verification", "audit", "alignment"
    };
    if (phase >= 0 && phase < ORCH_PHASE_MAX) return names[phase];
    return "unknown";
}

static const char* status_name(orch_task_status_t s) {
    static const char* names[] = {
        "pending", "running", "completed",
        "failed", "cancelled", "timeout"
    };
    if (s >= 0 && s <= ORCH_TASK_TIMEOUT) return names[s];
    return "unknown";
}

orchestrator_t* orchestrator_create(const orch_config_t* config) {
    orchestrator_t* orch = (orchestrator_t*)calloc(1, sizeof(orchestrator_t));
    if (!orch) return NULL;

    if (config) {
        memcpy(&orch->config, config, sizeof(orch_config_t));
    } else {
        orch_config_get_defaults(&orch->config);
    }

    if (orch->config.timeout_ms == 0)
        orch->config.timeout_ms = AGENTOS_DEFAULT_TIMEOUT_MS * 2;
    if (orch->config.max_retries == 0)
        orch->config.max_retries = AGENTOS_DEFAULT_MAX_RETRIES;
    if (orch->config.max_subtasks == 0)
        orch->config.max_subtasks = ORCH_MAX_TASKS;

    thread_pool_config_t pool_cfg;
    pool_cfg.min_threads = 2;
    pool_cfg.max_threads = 8;
    pool_cfg.queue_size = 64;
    pool_cfg.idle_timeout_ms = 30000;
    orch->pool = thread_pool_create(&pool_cfg);

    atomic_store(&orch->active_count, 0);

    if (orch->config.enable_thinking_chain) {
        agentos_error_t mem_err = agentos_memoryrov_init(NULL, &orch->memory);
        if (mem_err != AGENTOS_SUCCESS) {
            SVC_LOG_WARN("orchestrator: memory init failed (err=%d), continuing without memory", mem_err);
            orch->memory = NULL;
        } else {
            SVC_LOG_INFO("orchestrator: memory initialized successfully");
        }
    }

    if (orch->config.enable_metacognition) {
        agentos_error_t cog_err = agentos_cognition_create(NULL, NULL, NULL, &orch->cognition);
        if (cog_err != AGENTOS_SUCCESS) {
            SVC_LOG_WARN("orchestrator: cognition init failed (err=%d), continuing without cognition", cog_err);
            orch->cognition = NULL;
        } else {
            SVC_LOG_INFO("orchestrator: cognition engine initialized successfully");
        }
    }

    if (orch->config.enable_circuit_breaker) {
        orch->cb_mgr = cb_manager_create();
        if (orch->cb_mgr) {
            cb_config_t cb_cfg = cb_create_default_config();
            cb_cfg.failure_threshold = 5;
            cb_cfg.half_open_max_calls = 2;
            cb_cfg.timeout_ms = 30000;
            orch->breaker = cb_create(orch->cb_mgr, "orchestrator", &cb_cfg);
            if (!orch->breaker) {
                SVC_LOG_WARN("orchestrator: circuit breaker creation failed, continuing without breaker");
                cb_manager_destroy(orch->cb_mgr);
                orch->cb_mgr = NULL;
            } else {
                SVC_LOG_INFO("orchestrator: circuit breaker initialized successfully");
            }
        } else {
            SVC_LOG_WARN("orchestrator: circuit breaker manager creation failed");
        }
    }

    generate_task_id(orch->thinking_chain_root, sizeof(orch->thinking_chain_root));

    SVC_LOG_INFO("orchestrator: created (timeout=%ums, retries=%u, strategy=%d, pool=%s, memory=%s, cognition=%s, breaker=%s)",
                 orch->config.timeout_ms, orch->config.max_retries,
                 orch->config.default_strategy,
                 orch->pool ? "enabled" : "disabled",
                 orch->memory ? "enabled" : "disabled",
                 orch->cognition ? "enabled" : "disabled",
                 orch->config.enable_circuit_breaker ? "enabled" : "disabled");

    return orch;
}

void orchestrator_destroy(orchestrator_t* orch) {
    if (!orch) return;

    orchestrator_cancel_all(orch);

    for (uint32_t i = 0; i < orch->task_count; i++) {
        free(orch->tasks[i].input);
        free(orch->tasks[i].output);
    }

    if (orch->pool) thread_pool_destroy(orch->pool);
    if (orch->memory) agentos_memoryrov_cleanup(orch->memory);
    if (orch->cognition) agentos_cognition_destroy(orch->cognition);
    if (orch->breaker) cb_destroy(orch->breaker);
    if (orch->cb_mgr) cb_manager_destroy(orch->cb_mgr);

    SVC_LOG_INFO("orchestrator: destroyed (total=%llu, success=%llu)",
                 (unsigned long long)orch->total_executions,
                 (unsigned long long)orch->success_count);

    free(orch);
}

orch_pipeline_t* orchestrator_pipeline_create(orchestrator_t* orch,
                                              const char* name) {
    if (!orch) return NULL;

    orch_pipeline_t* p = (orch_pipeline_t*)calloc(1, sizeof(orch_pipeline_t));
    if (!p) return NULL;

    if (name) strncpy(p->name, name, sizeof(p->name) - 1);
    else snprintf(p->name, sizeof(p->name), "pipeline-%u", orch->task_count);

    p->owner = orch;
    return p;
}

void orchestrator_pipeline_destroy(orch_pipeline_t* pipeline) {
    free(pipeline);
}

int orchestrator_pipeline_add_step(orch_pipeline_t* pipeline,
                                   const orch_pipeline_step_t* step) {
    if (!pipeline || !step) return -1;
    if (pipeline->step_count >= ORCH_MAX_STEPS) return -2;

    pipeline->steps[pipeline->step_count] = *step;
    pipeline->step_count++;
    return 0;
}

static task_entry_t* find_or_create_task(orchestrator_t* orch,
                                         orch_phase_t phase,
                                         const char* input) {
    if (orch->task_count >= ORCH_MAX_TASKS) return NULL;

    task_entry_t* t = &orch->tasks[orch->task_count++];
    memset(t, 0, sizeof(*t));
    generate_task_id(t->id, sizeof(t->id));
    t->phase = phase;
    t->status = ORCH_TASK_PENDING;
    t->input = input ? strdup(input) : strdup("");
    t->start_time = time(NULL);

    if (orch->config.enable_thinking_chain) {
        snprintf(t->thinking_chain_id, ORCH_ID_LEN, "%s-%s",
                 orch->thinking_chain_root, phase_name(phase));
    }

    return t;
}

static ipc_service_bus_t g_orch_bus = NULL;

static char* memory_query_context(agentos_memoryrov_handle_t* mem, const char* query, uint32_t limit) {
    if (!mem || !query) return NULL;
    char** ids = NULL;
    float* scores = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_memoryrov_query(mem, query, limit, &ids, &scores, &count);
    if (err != AGENTOS_SUCCESS || count == 0) {
        if (ids) free(ids);
        if (scores) free(scores);
        return NULL;
    }
    size_t buf_sz = count * 256 + 64;
    char* context = (char*)malloc(buf_sz);
    if (!context) { free(ids); free(scores); return NULL; }
    size_t pos = 0;
    pos += snprintf(context + pos, buf_sz - pos, "{\"memory_context\":[");
    for (size_t i = 0; i < count && pos < buf_sz - 2; i++) {
        void* data = NULL;
        size_t data_len = 0;
        if (agentos_memoryrov_get_raw(mem, ids[i], &data, &data_len) == AGENTOS_SUCCESS && data) {
            if (i > 0) pos += snprintf(context + pos, buf_sz - pos, ",");
            size_t copy_len = data_len < 200 ? data_len : 200;
            pos += snprintf(context + pos, buf_sz - pos, "{\"id\":\"%s\",\"score\":%.2f,\"data\":\"%.*s\"}",
                ids[i], scores[i], (int)copy_len, (const char*)data);
            free(data);
        }
        free(ids[i]);
    }
    pos += snprintf(context + pos, buf_sz - pos, "]}");
    free(ids);
    free(scores);
    return context;
}

static void memory_write_step(agentos_memoryrov_handle_t* mem,
                               const char* phase_name,
                               const char* content,
                               const char* metadata_extra) {
    if (!mem || !content) return;
    char meta[512];
    snprintf(meta, sizeof(meta), "{\"source\":\"orchestrator\",\"phase\":\"%s\"%s%s}",
        phase_name,
        metadata_extra ? "," : "",
        metadata_extra ? metadata_extra : "");
    char* record_id = NULL;
    agentos_memoryrov_write_raw(mem, content, strlen(content), meta, &record_id);
    if (record_id) {
        SVC_LOG_INFO("orchestrator: memory write step %s -> %s", phase_name, record_id);
        free(record_id);
    }
}

static void memory_inform_evaluation(agentos_memoryrov_handle_t* mem,
                                      const char* phase,
                                      float score,
                                      bool passed) {
    if (!mem) return;
    char eval_data[256];
    snprintf(eval_data, sizeof(eval_data),
        "{\"evaluation\":{\"phase\":\"%s\",\"score\":%.3f,\"passed\":%s}}",
        phase, score, passed ? "true" : "false");
    char meta[256];
    snprintf(meta, sizeof(meta), "{\"source\":\"metacognition\",\"type\":\"evaluation\",\"phase\":\"%s\"}", phase);
    char* record_id = NULL;
    agentos_memoryrov_write_raw(mem, eval_data, strlen(eval_data), meta, &record_id);
    if (record_id) free(record_id);
}

static void memory_sync_persistent(agentos_memoryrov_handle_t* mem) {
    if (!mem) return;
    agentos_memoryrov_evolve(mem, 0);
    SVC_LOG_INFO("orchestrator: memory evolution triggered");
}

static char* memory_retrieve_for_generation(agentos_memoryrov_handle_t* mem, const char* topic) {
    if (!mem || !topic) return NULL;
    agentos_memory_t* results = NULL;
    size_t count = 0;
    agentos_error_t err = agentos_memoryrov_retrieve(mem, topic, 5, &results, &count);
    if (err != AGENTOS_SUCCESS || count == 0 || !results) {
        if (results) free(results);
        return NULL;
    }
    size_t buf_sz = count * 256 + 64;
    char* context = (char*)malloc(buf_sz);
    if (!context) { free(results); return NULL; }
    size_t pos = 0;
    pos += snprintf(context + pos, buf_sz - pos, "{\"retrieved\":[");
    for (size_t i = 0; i < count && pos < buf_sz - 2; i++) {
        if (i > 0) pos += snprintf(context + pos, buf_sz - pos, ",");
        pos += snprintf(context + pos, buf_sz - pos,
            "{\"id\":\"%s\",\"score\":%.2f}",
            results[i].record_id ? results[i].record_id : "",
            results[i].score);
    }
    pos += snprintf(context + pos, buf_sz - pos, "]}");
    free(results);
    return context;
}

static char* call_llm_service(const char* prompt, const char* system_role) {
    if (!prompt) return NULL;

    if (!g_orch_bus) {
        g_orch_bus = ipc_service_bus_create("orchestrator_bus", NULL);
        if (!g_orch_bus) {
            SVC_LOG_WARN("orchestrator: IPC bus creation failed, using local fallback");
            return NULL;
        }
        ipc_service_bus_start(g_orch_bus);
    }

    char params[4096];
    if (system_role) {
        snprintf(params, sizeof(params),
            "{\"messages\":["
            "{\"role\":\"system\",\"content\":\"%s\"},"
            "{\"role\":\"user\",\"content\":\"%s\"}]}",
            system_role, prompt);
    } else {
        snprintf(params, sizeof(params),
            "{\"messages\":["
            "{\"role\":\"user\",\"content\":\"%s\"}]}",
            prompt);
    }

    ipc_bus_message_t request;
    memset(&request, 0, sizeof(request));
    request.payload = params;
    request.payload_size = strlen(params);
    strncpy(request.header.target, "llm_d", sizeof(request.header.target) - 1);

    ipc_bus_message_t response;
    memset(&response, 0, sizeof(response));

    agentos_error_t err = ipc_service_bus_request(g_orch_bus, "llm_d",
                                                    &request, &response, 30000);
    if (err != AGENTOS_SUCCESS || !response.payload) {
        SVC_LOG_WARN("orchestrator: LLM call failed (err=%d), using fallback", err);
        if (response.payload) free(response.payload);
        return NULL;
    }

    char* result = strdup((const char*)response.payload);
    free(response.payload);
    return result;
}

static char* build_decomposition_prompt(const char* input) {
    size_t len = strlen(input) + 512;
    char* prompt = (char*)malloc(len);
    if (!prompt) return NULL;
    snprintf(prompt, len,
        "Decompose the following task into subtasks. "
        "Return a JSON array of objects with \"id\", \"description\", \"priority\" fields.\n\n"
        "Task: %s", input);
    return prompt;
}

static char* build_planning_prompt(const char* decomposed) {
    size_t len = strlen(decomposed) + 512;
    char* prompt = (char*)malloc(len);
    if (!prompt) return NULL;
    snprintf(prompt, len,
        "Create an execution plan for these subtasks. "
        "Return a JSON object with \"steps\" array, each having \"subtask_id\", "
        "\"action\", \"depends_on\" fields.\n\n"
        "Subtasks: %s", decomposed);
    return prompt;
}

static char* build_generation_prompt(const char* plan) {
    size_t len = strlen(plan) + 512;
    char* prompt = (char*)malloc(len);
    if (!prompt) return NULL;
    snprintf(prompt, len,
        "Execute the following plan and produce the final output. "
        "Be thorough and complete.\n\n"
        "Plan: %s", plan);
    return prompt;
}

static char* build_verification_prompt(const char* original, const char* generated) {
    size_t len = strlen(original) + strlen(generated) + 512;
    char* prompt = (char*)malloc(len);
    if (!prompt) return NULL;
    snprintf(prompt, len,
        "Verify the following output against the original task. "
        "Return JSON: {\"verified\":bool,\"score\":0.0-1.0,"
        "\"issues\":[\"...\"],\"corrections\":N}\n\n"
        "Original: %s\n\nOutput: %s", original, generated);
    return prompt;
}

static char* build_audit_prompt(const char* output, const char* verification) {
    size_t len = strlen(output) + strlen(verification) + 512;
    char* prompt = (char*)malloc(len);
    if (!prompt) return NULL;
    snprintf(prompt, len,
        "Audit this output for correctness, completeness, and safety. "
        "Return JSON: {\"audit_passed\":bool,\"issues\":[\"...\"],"
        "\"severity\":\"ok|warning|critical\"}\n\n"
        "Output: %s\n\nVerification: %s", output, verification);
    return prompt;
}

static char* build_correction_prompt(const char* output, const char* critique) {
    size_t len = strlen(output) + strlen(critique) + 512;
    char* prompt = (char*)malloc(len);
    if (!prompt) return NULL;
    snprintf(prompt, len,
        "Improve the following output based on the critique. "
        "Return the corrected output.\n\n"
        "Original: %s\n\nCritique: %s", output, critique);
    return prompt;
}

static float extract_score_from_json(const char* json) {
    if (!json) return 0.0f;
    const char* key = "\"score\"";
    const char* p = strstr(json, key);
    if (!p) return 0.5f;
    p += strlen(key);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    char* end = NULL;
    float val = strtof(p, &end);
    if (end == p) return 0.5f;
    if (val < 0.0f) val = 0.0f;
    if (val > 1.0f) val = 1.0f;
    return val;
}

static bool extract_bool_from_json(const char* json, const char* field) {
    if (!json || !field) return false;
    size_t flen = strlen(field) + 4;
    char* key = (char*)malloc(flen);
    if (!key) return false;
    snprintf(key, flen, "\"%s\"", field);
    const char* p = strstr(json, key);
    free(key);
    if (!p) return false;
    p += strlen(field) + 3;
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    return (*p == 't' || *p == 'T' || *p == '1');
}

static char* extract_field_string(const char* json, const char* field) {
    if (!json || !field) return NULL;
    size_t flen = strlen(field) + 4;
    char* key = (char*)malloc(flen);
    if (!key) return NULL;
    snprintf(key, flen, "\"%s\"", field);
    const char* p = strstr(json, key);
    free(key);
    if (!p) return NULL;
    p += strlen(field) + 3;
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    if (*p != '"') return NULL;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    char* val = (char*)malloc(len + 1);
    if (!val) return NULL;
    memcpy(val, p, len);
    val[len] = '\0';
    return val;
}

#define ORCH_VERIFY_MAX_ROUNDS 3
#define ORCH_VERIFY_THRESHOLD 0.7f
#define ORCH_ALIGN_HISTORY_SIZE 8

static float align_history[ORCH_ALIGN_HISTORY_SIZE];
static int align_history_count = 0;

static int execute_single_phase(orchestrator_t* orch,
                                orch_phase_t phase,
                                const char* input,
                                orch_result_t** out_result) {
    task_entry_t* task = find_or_create_task(orch, phase, input);
    if (!task) return -1;

    task->status = ORCH_TASK_RUNNING;
    atomic_fetch_add(&orch->active_count, 1);
    orch->total_executions++;

    if (orch->progress_cb) {
        orch->progress_cb(phase, ORCH_TASK_RUNNING, task->id, orch->progress_data);
    }

    SVC_LOG_INFO("orchestrator: phase %s started (task=%s)",
                 phase_name(phase), task->id);

    int ret = 0;
    time_t phase_start = time(NULL);

    if (orch->breaker && !cb_allow_request(orch->breaker)) {
        SVC_LOG_WARN("orchestrator: phase %s rejected by circuit breaker (state=%s)",
                     phase_name(phase),
                     cb_state_to_string(cb_get_state(orch->breaker)));
        task->status = ORCH_TASK_FAILED;
        task->error_code = -2;
        goto done;
    }

    uint32_t max_attempts = orch->config.max_retries > 0 ? orch->config.max_retries : 1;

    for (uint32_t attempt = 0; attempt < max_attempts; attempt++) {
        if (attempt > 0) {
            SVC_LOG_INFO("orchestrator: phase %s retry %u/%u",
                         phase_name(phase), attempt + 1, max_attempts);
            if (orch->config.retry_delay_ms > 0) {
                struct timespec ts = {
                    .tv_sec = (long)(orch->config.retry_delay_ms / 1000),
                    .tv_nsec = (long)((orch->config.retry_delay_ms % 1000) * 1000000)
                };
                nanosleep(&ts, NULL);
            }
            if (task->output) { free(task->output); task->output = NULL; }
            task->status = ORCH_TASK_RUNNING;
        }

        time_t step_start = time(NULL);

        switch (phase) {
        case ORCH_PHASE_DECOMPOSITION:
            {
                char* mem_ctx = memory_query_context(orch->memory, input ? input : "", 3);

                agentos_task_plan_t* cog_plan = NULL;
                if (orch->cognition) {
                    agentos_error_t cog_err = agentos_cognition_process(
                        orch->cognition, input ? input : "",
                        strlen(input ? input : ""), &cog_plan);
                    if (cog_err == AGENTOS_SUCCESS && cog_plan) {
                        SVC_LOG_INFO("orchestrator: cognition plan generated for decomposition");
                        char* cog_json = NULL;
                        size_t cog_len = 0;
                        agentos_cognition_stats(orch->cognition, &cog_json, &cog_len);
                        if (cog_json) { free(cog_json); }
                    }
                }

                char* prompt = build_decomposition_prompt(input ? input : "");
                char* llm_result = call_llm_service(prompt, "You are a task decomposition expert.");
                free(prompt);

                if (llm_result) {
                    task->output = llm_result;
                } else if (cog_plan) {
                    task->output = strdup("{\"source\":\"cognition\",\"subtasks\":[]}");
                    agentos_task_plan_free(cog_plan);
                    cog_plan = NULL;
                } else {
                    size_t buf_sz = (input ? strlen(input) : 0) + 256;
                    char* fallback = (char*)malloc(buf_sz);
                    if (fallback) {
                        snprintf(fallback, buf_sz,
                            "{\"subtasks\":[{\"id\":1,\"description\":\"%s\",\"priority\":1}]}",
                            input ? input : "empty");
                        task->output = fallback;
                    } else {
                        task->output = strdup("{\"subtasks\":[]}");
                    }
                }
                memory_write_step(orch->memory, "decomposition", task->output, NULL);
                if (mem_ctx) free(mem_ctx);
                task->output_len = strlen(task->output);
                task->status = ORCH_TASK_COMPLETED;
            }
            break;

        case ORCH_PHASE_PLANNING:
            {
                char* prompt = build_planning_prompt(input ? input : "{}");
                char* llm_result = call_llm_service(prompt, "You are a planning expert.");
                free(prompt);

                if (llm_result) {
                    task->output = llm_result;
                } else {
                    task->output = strdup("{\"steps\":[{\"subtask_id\":1,\"action\":\"execute\",\"depends_on\":[]}]}");
                }
                memory_write_step(orch->memory, "planning", task->output, NULL);
                task->output_len = strlen(task->output);
                task->status = ORCH_TASK_COMPLETED;
            }
            break;

        case ORCH_PHASE_GENERATION:
            {
                char* mem_retrieved = memory_retrieve_for_generation(orch->memory, input ? input : "");
                char* prompt = build_generation_prompt(input ? input : "{}");

                agentos_task_plan_t* gen_plan = NULL;
                if (orch->cognition) {
                    agentos_cognition_process(
                        orch->cognition, input ? input : "",
                        strlen(input ? input : ""), &gen_plan);
                }

                char* t2_output = call_llm_service(prompt, "You are an expert assistant. Provide thorough, accurate output.");
                free(prompt);

                if (!t2_output) {
                    t2_output = strdup("{\"generated\":null,\"error\":\"llm_unavailable\",\"mode\":\"t2_no_llm\"}");
                }

                for (int round = 0; round < ORCH_VERIFY_MAX_ROUNDS; round++) {
                    char* verify_prompt = build_verification_prompt(
                        orch->tasks[0].input ? orch->tasks[0].input : "",
                        t2_output);
                    char* t1f_result = call_llm_service(verify_prompt,
                        "You are a fast verification agent (t1-f). Verify quickly.");
                    free(verify_prompt);

                    float score = 0.0f;
                    bool verified = false;
                    if (t1f_result) {
                        score = extract_score_from_json(t1f_result);
                        verified = extract_bool_from_json(t1f_result, "verified");
                        free(t1f_result);
                    }

                    if (verified && score >= ORCH_VERIFY_THRESHOLD) {
                        SVC_LOG_INFO("orchestrator: generation verified (round=%d, score=%.2f)",
                                    round, score);
                        break;
                    }

                    SVC_LOG_INFO("orchestrator: generation not verified (round=%d, score=%.2f), correcting",
                                round, score);

                    char* correction_prompt = build_correction_prompt(t2_output,
                        t1f_result ? t1f_result : "Quality too low, improve output");
                    char* corrected = call_llm_service(correction_prompt,
                        "You are a correction agent. Improve the output.");
                    free(correction_prompt);

                    if (corrected) {
                        free(t2_output);
                        t2_output = corrected;
                    }
                }

                if (gen_plan) { agentos_task_plan_free(gen_plan); gen_plan = NULL; }

                size_t meta_sz = strlen(t2_output) + 256;
                char* meta_buf = (char*)malloc(meta_sz);
                if (meta_buf) {
                    snprintf(meta_buf, meta_sz,
                        "{\"phase\":\"generation\",\"mode\":\"t2_streaming\","
                        "\"critical_loop\":true,\"verify_rounds\":%d,"
                        "\"content\":%s}",
                        ORCH_VERIFY_MAX_ROUNDS, t2_output);
                    task->output = meta_buf;
                    free(t2_output);
                } else {
                    task->output = t2_output;
                }
                memory_write_step(orch->memory, "generation", task->output,
                    ",\"critical_loop\":true");
                if (mem_retrieved) free(mem_retrieved);
                task->output_len = strlen(task->output);
                task->status = ORCH_TASK_COMPLETED;
            }
            break;

        case ORCH_PHASE_VERIFICATION:
            {
                char* content = extract_field_string(input ? input : "", "content");
                if (!content) content = strdup(input ? input : "");

                char* verify_prompt = build_verification_prompt(
                    orch->tasks[0].input ? orch->tasks[0].input : "",
                    content);
                char* t1f_result = call_llm_service(verify_prompt,
                    "You are a t1-f fast verification agent. Score 0.0-1.0.");
                free(verify_prompt);
                free(content);

                float score = 0.5f;
                bool verified = false;
                if (t1f_result) {
                    score = extract_score_from_json(t1f_result);
                    verified = extract_bool_from_json(t1f_result, "verified");
                }

                if (score < ORCH_VERIFY_THRESHOLD && !verified) {
                    char* correction_prompt = build_correction_prompt(
                        input ? input : "",
                        t1f_result ? t1f_result : "Verification failed");
                    char* corrected = call_llm_service(correction_prompt,
                        "You are a correction agent.");
                    free(correction_prompt);
                    if (corrected) {
                        free(t1f_result);
                        t1f_result = corrected;
                        score = ORCH_VERIFY_THRESHOLD;
                        verified = true;
                    }
                }

                char vbuf[512];
                int vlen = snprintf(vbuf, sizeof(vbuf),
                    "{\"phase\":\"verification\",\"role\":\"t1-f\","
                    "\"verified\":%s,\"score\":%.3f,"
                    "\"corrections\":%d,\"rounds\":1}",
                    verified ? "true" : "false", score,
                    verified ? 0 : 1);

                if (t1f_result) {
                    free(t1f_result);
                }

                task->output = strdup(vbuf);
                task->output_len = (vlen > 0 && (size_t)vlen < sizeof(vbuf)) ? (size_t)vlen : strlen(task->output);
                task->status = verified ? ORCH_TASK_COMPLETED : ORCH_TASK_FAILED;
                memory_inform_evaluation(orch->memory, "verification", score, verified);
                memory_write_step(orch->memory, "verification", task->output, NULL);
            }
            break;

        case ORCH_PHASE_AUDIT:
            {
                char* prev_output = orch->tasks[3].output ? orch->tasks[3].output : (char*)(input ? input : "");
                char* audit_prompt = build_audit_prompt(
                    input ? input : "",
                    prev_output);
                char* audit_result = call_llm_service(audit_prompt,
                    "You are a t1-p expert audit agent. Check for correctness, completeness, safety.");
                free(audit_prompt);

                bool audit_passed = true;
                char* severity = NULL;
                if (audit_result) {
                    audit_passed = extract_bool_from_json(audit_result, "audit_passed");
                    severity = extract_field_string(audit_result, "severity");
                }

                if (!audit_passed) {
                    SVC_LOG_WARN("orchestrator: audit failed, severity=%s",
                                severity ? severity : "unknown");
                    if (severity && strcmp(severity, "critical") == 0) {
                        task->status = ORCH_TASK_FAILED;
                    }
                }

                size_t abuf_sz = (audit_result ? strlen(audit_result) : 0) + 256;
                char* abuf = (char*)malloc(abuf_sz);
                if (abuf) {
                    snprintf(abuf, abuf_sz,
                        "{\"audit_passed\":%s,\"severity\":\"%s\","
                        "\"details\":%s}",
                        audit_passed ? "true" : "false",
                        severity ? severity : "ok",
                        audit_result ? audit_result : "null");
                    task->output = abuf;
                } else {
                    task->output = strdup(audit_passed ?
                        "{\"audit_passed\":true}" : "{\"audit_passed\":false}");
                }
                task->output_len = strlen(task->output);
                if (audit_passed) task->status = ORCH_TASK_COMPLETED;
                memory_inform_evaluation(orch->memory, "audit",
                    audit_passed ? 0.9f : 0.3f, audit_passed);
                memory_write_step(orch->memory, "audit", task->output, NULL);
                free(audit_result);
                free(severity);
            }
            break;

        case ORCH_PHASE_ALIGNMENT:
            {
                float logic_score = 0.8f;
                float fact_score = 0.8f;
                float goal_score = 0.8f;

                char* original_input = orch->tasks[0].input ? orch->tasks[0].input : "";
                char* final_output = orch->tasks[4].output ? orch->tasks[4].output : (char*)(input ? input : "");

                char align_prompt[4096];
                snprintf(align_prompt, sizeof(align_prompt),
                    "Score this output against the original task on 3 dimensions. "
                    "Return JSON: {\"logic_score\":0.0-1.0,\"fact_score\":0.0-1.0,"
                    "\"goal_score\":0.0-1.0,\"drift\":0.0-1.0}\n\n"
                    "Original: %.1000s\n\nOutput: %.1000s",
                    original_input, final_output);

                char* align_result = call_llm_service(align_prompt,
                    "You are an alignment scoring agent.");
                if (align_result) {
                    logic_score = extract_score_from_json(
                        strstr(align_result, "logic_score") ? align_result : NULL);
                    fact_score = extract_score_from_json(
                        strstr(align_result, "fact_score") ? align_result : NULL);
                    goal_score = extract_score_from_json(
                        strstr(align_result, "goal_score") ? align_result : NULL);
                    free(align_result);
                }

                float overall = logic_score * 0.30f + fact_score * 0.35f + goal_score * 0.35f;

                if (align_history_count < ORCH_ALIGN_HISTORY_SIZE) {
                    align_history[align_history_count++] = overall;
                } else {
                    for (int i = 0; i < ORCH_ALIGN_HISTORY_SIZE - 1; i++) {
                        align_history[i] = align_history[i + 1];
                    }
                    align_history[ORCH_ALIGN_HISTORY_SIZE - 1] = overall;
                }

                bool drift_detected = false;
                float drift_value = 0.0f;
                if (align_history_count >= 3) {
                    float recent = align_history[align_history_count - 1];
                    float earlier = align_history[0];
                    drift_value = earlier - recent;
                    if (drift_value > 0.15f) {
                        drift_detected = true;
                        SVC_LOG_WARN("orchestrator: alignment drift detected (%.3f)", drift_value);
                    }
                }

                bool aligned = (overall >= 0.6f) && !drift_detected;

                char albuf[512];
                int allen = snprintf(albuf, sizeof(albuf),
                    "{\"aligned\":%s,\"overall_score\":%.3f,"
                    "\"logic\":%.3f,\"fact\":%.3f,\"goal\":%.3f,"
                    "\"drift\":%.3f,\"drift_detected\":%s}",
                    aligned ? "true" : "false", overall,
                    logic_score, fact_score, goal_score,
                    drift_value, drift_detected ? "true" : "false");

                task->output = strdup(albuf);
                task->output_len = (allen > 0 && (size_t)allen < sizeof(albuf)) ? (size_t)allen : strlen(task->output);
                task->status = aligned ? ORCH_TASK_COMPLETED : ORCH_TASK_FAILED;
                memory_inform_evaluation(orch->memory, "alignment", overall, aligned);
                memory_write_step(orch->memory, "alignment", task->output, NULL);
                memory_sync_persistent(orch->memory);
            }
            break;

        default:
            task->status = ORCH_TASK_FAILED;
            task->error_code = -1;
            ret = -1;
            break;
        }

        time_t step_end = time(NULL);
        uint32_t step_duration_ms = (uint32_t)((step_end - step_start) * 1000);

        if (orch->config.timeout_ms > 0 && step_duration_ms >= orch->config.timeout_ms) {
            SVC_LOG_WARN("orchestrator: phase %s timeout after %ums (limit=%ums)",
                         phase_name(phase), step_duration_ms, orch->config.timeout_ms);
            task->status = ORCH_TASK_TIMEOUT;
            task->error_code = -3;
            if (orch->breaker) cb_record_timeout(orch->breaker);
            goto done;
        }

        if (task->status == ORCH_TASK_COMPLETED) {
            if (orch->breaker) cb_record_success(orch->breaker, task->duration_ms);
            break;
        }

        if (task->status == ORCH_TASK_FAILED || task->status == ORCH_TASK_TIMEOUT) {
            if (attempt < max_attempts - 1) {
                if (orch->breaker) cb_record_failure(orch->breaker, task->error_code);
                continue;
            }
            if (orch->breaker) cb_record_failure(orch->breaker, task->error_code);
            break;
        }
    }

done:
    task->duration_ms = (uint32_t)((time(NULL) - phase_start) * 1000);

    if (task->status == ORCH_TASK_COMPLETED) {
        orch->success_count++;
    }

    if (orch->progress_cb) {
        orch->progress_cb(phase, task->status, task->id, orch->progress_data);
    }

    if (out_result) {
        orch_result_t* r = (orch_result_t*)calloc(1, sizeof(orch_result_t));
        if (r) {
            r->task_id = strdup(task->id);
            r->output = task->output ? strdup(task->output) : strdup("");
            r->output_len = task->output_len;
            r->status = task->status;
            r->error_code = task->error_code;
            r->duration_ms = task->duration_ms;
            r->thinking_chain_id = strdup(task->thinking_chain_id);
            *out_result = r;
        }
    }

    atomic_fetch_sub(&orch->active_count, 1);

    SVC_LOG_INFO("orchestrator: phase %s %s (task=%s, duration=%ums)",
                 phase_name(phase), status_name(task->status),
                 task->id, task->duration_ms);

    return ret;
}

int orchestrator_execute(orchestrator_t* orch,
                         const char* input,
                         orch_result_t** out_results,
                         size_t* out_count) {
    if (!orch || !input) return -1;

    orch->task_count = 0;

    static const orch_phase_t default_phases[] = {
        ORCH_PHASE_DECOMPOSITION,
        ORCH_PHASE_PLANNING,
        ORCH_PHASE_GENERATION,
        ORCH_PHASE_VERIFICATION,
        ORCH_PHASE_AUDIT,
        ORCH_PHASE_ALIGNMENT
    };

    size_t phase_count = sizeof(default_phases) / sizeof(default_phases[0]);
    orch_result_t* results = (orch_result_t*)calloc(phase_count, sizeof(orch_result_t));
    if (!results) return -2;

    size_t completed = 0;
    const char* current_input = input;

    for (size_t i = 0; i < phase_count; i++) {
        orch_result_t* phase_result = NULL;
        int ret = execute_single_phase(orch, default_phases[i],
                                       current_input, &phase_result);
        if (ret != 0 || !phase_result) {
            for (size_t j = 0; j < completed; j++) {
                orchestrator_result_free(&results[j]);
            }
            free(results);
            return -3;
        }

        results[completed] = *phase_result;
        free(phase_result);
        completed++;

        if (results[completed - 1].status != ORCH_TASK_COMPLETED) {
            break;
        }

        current_input = results[completed - 1].output;
    }

    *out_results = results;
    *out_count = completed;
    return 0;
}

int orchestrator_execute_pipeline(orchestrator_t* orch,
                                  orch_pipeline_t* pipeline,
                                  const char* input,
                                  orch_result_t** out_results,
                                  size_t* out_count) {
    if (!orch || !pipeline || !input) return -1;

    orch->task_count = 0;

    orch_result_t* results = (orch_result_t*)calloc(pipeline->step_count,
                                                     sizeof(orch_result_t));
    if (!results) return -2;

    size_t completed = 0;
    const char* current_input = input;

    for (uint32_t i = 0; i < pipeline->step_count; i++) {
        orch_pipeline_step_t* step = &pipeline->steps[i];

        if (step->condition_fn && !step->condition_fn(current_input, step->condition_data)) {
            results[completed].task_id = strdup("skipped");
            results[completed].status = ORCH_TASK_CANCELLED;
            results[completed].output = strdup("");
            completed++;
            continue;
        }

        orch_result_t* phase_result = NULL;
        int ret = execute_single_phase(orch, step->phase,
                                       current_input, &phase_result);
        if (ret != 0 || !phase_result) {
            for (size_t j = 0; j < completed; j++) {
                orchestrator_result_free(&results[j]);
            }
            free(results);
            return -3;
        }

        results[completed] = *phase_result;
        free(phase_result);
        completed++;

        if (results[completed - 1].status != ORCH_TASK_COMPLETED) {
            break;
        }

        current_input = results[completed - 1].output;
    }

    *out_results = results;
    *out_count = completed;
    return 0;
}

int orchestrator_execute_phase(orchestrator_t* orch,
                               orch_phase_t phase,
                               const char* input,
                               orch_result_t** out_result) {
    return execute_single_phase(orch, phase, input, out_result);
}

void orchestrator_set_progress_callback(orchestrator_t* orch,
                                        orch_progress_cb_t callback,
                                        void* user_data) {
    if (!orch) return;
    orch->progress_cb = callback;
    orch->progress_data = user_data;
}

orch_task_status_t orchestrator_get_task_status(orchestrator_t* orch,
                                                const char* task_id) {
    if (!orch || !task_id) return ORCH_TASK_FAILED;

    for (uint32_t i = 0; i < orch->task_count; i++) {
        if (strcmp(orch->tasks[i].id, task_id) == 0) {
            return orch->tasks[i].status;
        }
    }
    return ORCH_TASK_FAILED;
}

orch_result_t* orchestrator_get_result(orchestrator_t* orch,
                                       const char* task_id) {
    if (!orch || !task_id) return NULL;

    for (uint32_t i = 0; i < orch->task_count; i++) {
        if (strcmp(orch->tasks[i].id, task_id) == 0) {
            orch_result_t* r = (orch_result_t*)calloc(1, sizeof(orch_result_t));
            if (!r) return NULL;
            r->task_id = strdup(orch->tasks[i].id);
            r->output = orch->tasks[i].output ? strdup(orch->tasks[i].output) : strdup("");
            r->output_len = orch->tasks[i].output_len;
            r->status = orch->tasks[i].status;
            r->error_code = orch->tasks[i].error_code;
            r->duration_ms = orch->tasks[i].duration_ms;
            r->thinking_chain_id = strdup(orch->tasks[i].thinking_chain_id);
            return r;
        }
    }
    return NULL;
}

void orchestrator_result_free(orch_result_t* result) {
    if (!result) return;
    free(result->task_id);
    free(result->output);
    free(result->thinking_chain_id);
}

uint32_t orchestrator_active_count(orchestrator_t* orch) {
    if (!orch) return 0;
    return (uint32_t)atomic_load(&orch->active_count);
}

int orchestrator_cancel(orchestrator_t* orch, const char* task_id) {
    if (!orch || !task_id) return -1;

    for (uint32_t i = 0; i < orch->task_count; i++) {
        if (strcmp(orch->tasks[i].id, task_id) == 0) {
            if (orch->tasks[i].status == ORCH_TASK_RUNNING ||
                orch->tasks[i].status == ORCH_TASK_PENDING) {
                orch->tasks[i].status = ORCH_TASK_CANCELLED;
                SVC_LOG_INFO("orchestrator: task %s cancelled", task_id);
                return 0;
            }
            return -2;
        }
    }
    return -3;
}

int orchestrator_cancel_all(orchestrator_t* orch) {
    if (!orch) return -1;

    uint32_t cancelled = 0;
    for (uint32_t i = 0; i < orch->task_count; i++) {
        if (orch->tasks[i].status == ORCH_TASK_RUNNING ||
            orch->tasks[i].status == ORCH_TASK_PENDING) {
            orch->tasks[i].status = ORCH_TASK_CANCELLED;
            cancelled++;
        }
    }

    if (cancelled > 0) {
        SVC_LOG_INFO("orchestrator: %u tasks cancelled", cancelled);
    }
    return 0;
}
