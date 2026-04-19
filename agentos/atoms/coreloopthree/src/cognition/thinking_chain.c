/**
 * @file thinking_chain.c
 * @brief 思考链路模块完整实现 - DS-001
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * 实现双思考系统的三大核心组件:
 * 1. Context Window - Token预算管理+滑动窗口
 * 2. Working Memory - 短期键值缓存+LRU淘汰
 * 3. Thinking Step DAG - 推理步骤依赖链
 */

#include "thinking_chain.h"
#include "agentos.h"
#include "include/memory_compat.h"
#include "string_compat.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

static uint64_t tc_time_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static size_t estimate_token_count(const char* data, size_t len) {
    if (!data || len == 0) return 0;
    size_t tokens = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == ' ' || data[i] == '\n' || data[i] == '\t') tokens++;
    }
    return (tokens > 0) ? tokens : (len / 4); /* 英文约4字符/token，中文更少 */
}

/* ============================================================================
 * Context Window 实现
 * ============================================================================ */

#define CW_BUFFER_MIN_SIZE 65536

agentos_error_t agentos_tc_context_window_create(
    size_t max_tokens,
    agentos_context_window_t** out_window) {

    if (!out_window) return AGENTOS_EINVAL;

    agentos_context_window_t* w = (agentos_context_window_t*)AGENTOS_CALLOC(1, sizeof(agentos_context_window_t));
    if (!w) return AGENTOS_ENOMEM;

    w->max_tokens = (max_tokens > 0) ? max_tokens : TC_MAX_TOKENS_DEFAULT;
    w->used_tokens = 0;
    w->chunk_size = TC_CHUNK_SIZE_DEFAULT;
    w->buffer_capacity = w->max_tokens * 8; /* 每token约8字节估计 */
    if (w->buffer_capacity < CW_BUFFER_MIN_SIZE) w->buffer_capacity = CW_BUFFER_MIN_SIZE;

    w->buffer = (char*)AGENTOS_CALLOC(1, w->buffer_capacity);
    if (!w->buffer) { AGENTOS_FREE(w); return AGENTOS_ENOMEM; }

    w->buffer_head = 0;
    w->buffer_tail = 0;
    w->buffer_used = 0;
    w->total_tokens_generated = 0;
    w->total_corrections = 0;
    w->total_steps = 0;
    w->completed_steps = 0;
    w->enable_dynamic_chunk = 1;
    w->low_confidence_threshold = 0.6f;
    w->high_confidence_threshold = 0.9f;
    w->max_corrections_per_chunk = TC_MAX_CORRECTIONS_DEFAULT;

    *out_window = w;
    return AGENTOS_SUCCESS;
}

void agentos_tc_context_window_destroy(agentos_context_window_t* window) {
    if (!window) return;
    if (window->buffer) AGENTOS_FREE(window->buffer);
    AGENTOS_FREE(window);
}

ssize_t agentos_tc_context_window_append(
    agentos_context_window_t* window,
    const char* data,
    size_t len) {

    if (!window || !data || len == 0) return (ssize_t)AGENTOS_EINVAL;

    size_t new_tokens = estimate_token_count(data, len);

    if (window->used_tokens + new_tokens > window->max_tokens) {
        /* 滑动窗口：丢弃最旧的数据以腾出空间 */
        size_t to_evict = (window->used_tokens + new_tokens) - window->max_tokens;
        size_t evicted_bytes = 0;
        while (evicted_bytes < to_evict * 4 && window->buffer_used > 0) {
            char c = window->buffer[window->buffer_tail];
            window->buffer_tail = (window->buffer_tail + 1) % window->buffer_capacity;
            window->buffer_used--;
            evicted_bytes++;
            if (c == ' ' || c == '\n') window->used_tokens--;
        }
    }

    for (size_t i = 0; i < len; i++) {
        window->buffer[window->buffer_head] = data[i];
        window->buffer_head = (window->buffer_head + 1) % window->buffer_capacity;
        window->buffer_used++;
        if (window->buffer_used >= window->buffer_capacity) break;
    }

    window->used_tokens += new_tokens;
    window->total_tokens_generated += new_tokens;
    return (ssize_t)(window->used_tokens);
}

agentos_error_t agentos_tc_context_window_get_recent(
    agentos_context_window_t* window,
    size_t token_count,
    char** out_data,
    size_t* out_len) {

    if (!window || !out_data) return AGENTOS_EINVAL;

    size_t avail = (token_count > 0 && token_count < window->used_tokens)
                  ? token_count : window->used_tokens;
    if (avail == 0) {
        *out_data = AGENTOS_STRDUP("");
        if (out_len) *out_len = 0;
        return AGENTOS_SUCCESS;
    }

    size_t est_bytes = avail * 4;
    char* result = (char*)AGENTOS_MALLOC(est_bytes + 1);
    if (!result) return AGENTOS_ENOMEM;

    size_t read_pos = (window->buffer_head > avail * 4)
                         ? (window->buffer_head - avail * 4)
                         : 0;
    if (read_pos >= window->buffer_capacity) read_pos = 0;

    size_t count = 0;
    size_t written = 0;
    size_t start = read_pos;

    do {
        if (written >= est_bytes) break;
        result[written++] = window->buffer[read_pos];
        read_pos = (read_pos + 1) % window->buffer_capacity;
        count++;
        if (window->buffer[read_pos - 1] == ' ' || window->buffer[read_pos - 1] == '\n')
            avail--;
    } while (count < window->buffer_used && read_pos != start && avail > 0);

    result[written] = '\0';
    *out_data = result;
    if (out_len) *out_len = written;
    return AGENTOS_SUCCESS;
}

int agentos_tc_context_window_has_space(
    agentos_context_window_t* window,
    size_t needed_tokens) {

    if (!window) return 0;
    return (int)(window->used_tokens + needed_tokens <= window->max_tokens);
}

agentos_error_t agentos_tc_context_window_stats(
    agentos_context_window_t* window,
    char** out_json) {

    if (!window || !out_json) return AGENTOS_EINVAL;

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{\"max_tokens\":%zu,\"used_tokens\":%zu,"
        "\"generated\":%llu,\"corrections\":%llu,"
        "\"steps\":{\"total\":%u,\"completed\":%u},"
        "\"utilization_pct\":%.1f}",
        window->max_tokens, window->used_tokens,
        (unsigned long long)window->total_tokens_generated,
        (unsigned long long)window->total_corrections,
        window->total_steps, window->completed_steps,
        window->max_tokens > 0 ? (float)window->used_tokens / (float)window->max_tokens * 100.0f : 0.0f);
    (void)len;

    char* result = AGENTOS_STRDUP(buf);
    if (!result) return AGENTOS_ENOMEM;
    *out_json = result;
    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * Working Memory 实现
 * ============================================================================ */

agentos_error_t agentos_tc_working_memory_create(
    size_t capacity,
    agentos_working_memory_t** out_mem) {

    if (!out_mem) return AGENTOS_EINVAL;

    size_t cap = (capacity > 0) ? capacity : TC_WORKING_MEM_CAPACITY;
    agentos_working_memory_t* mem = (agentos_working_memory_t*)AGENTOS_CALLOC(1, sizeof(agentos_working_memory_t));
    if (!mem) return AGENTOS_ENOMEM;

    mem->entries = (struct wm_entry*)AGENTOS_CALLOC(cap, sizeof(struct wm_entry));
    if (!mem->entries) { AGENTOS_FREE(mem); return AGENTOS_ENOMEM; }
    mem->lru_order = (uint32_t*)AGENTOS_CALLOC(cap, sizeof(uint32_t));
    if (!mem->lru_order) { AGENTOS_FREE(mem->entries); AGENTOS_FREE(mem); return AGENTOS_ENOMEM; }

    mem->capacity = cap;
    mem->count = 0;
    mem->lru_index = 0;
    mem->hits = 0;
    mem->misses = 0;
    mem->evictions = 0;

    *out_mem = mem;
    return AGENTOS_SUCCESS;
}

void agentos_tc_working_memory_destroy(agentos_working_memory_t* mem) {
    if (!mem) return;
    for (size_t i = 0; i < mem->count; i++) {
        if (mem->entries[i].key) AGENTOS_FREE(mem->entries[i].key);
        if (mem->entries[i].value) AGENTOS_FREE(mem->entries[i].value);
        if (mem->entries[i].type) AGENTOS_FREE(mem->entries[i].type);
    }
    AGENTOS_FREE(mem->entries);
    AGENTOS_FREE(mem->lru_order);
    AGENTOS_FREE(mem);
}

static size_t wm_find_key(agentos_working_memory_t* mem, const char* key) {
    for (size_t i = 0; i < mem->count; i++) {
        if (mem->entries[i].key && strcmp(mem->entries[i].key, key) == 0)
            return i;
    }
    return (size_t)-1;
}

static void wm_update_lru(agentos_working_memory_t* mem, size_t idx) {
    if (idx >= mem->count) return;
    mem->entries[idx].last_accessed_ns = tc_time_now_ns();
    mem->entries[idx].access_count++;
    mem->lru_order[mem->lru_index++] = (uint32_t)idx;
    if (mem->lru_index >= mem->capacity) mem->lru_index = 0;
}

static void wm_evict_one(agentos_working_memory_t* mem) {
    if (mem->count == 0) return;

    size_t victim = (size_t)-1;
    uint64_t oldest_access = UINT64_MAX;

    for (size_t i = 0; i < mem->count; i++) {
        if (!mem->entries[i].pinned && mem->entries[i].last_accessed_ns < oldest_access) {
            oldest_access = mem->entries[i].last_accessed_ns;
            victim = i;
        }
    }

    if (victim != (size_t)-1) {
        AGENTOS_FREE(mem->entries[victim].key);
        AGENTOS_FREE(mem->entries[victim].value);
        if (mem->entries[victim].type) AGENTOS_FREE(mem->entries[victim].type);

        mem->entries[victim] = mem->entries[mem->count - 1];
        mem->count--;
        mem->evictions++;
    }
}

agentos_error_t agentos_tc_working_memory_store(
    agentos_working_memory_t* mem,
    const char* key,
    const void* value,
    size_t value_size,
    const char* type,
    int pin) {

    if (!mem || !key || !value || value_size == 0) return AGENTOS_EINVAL;

    size_t existing = wm_find_key(mem, key);
    if (existing != (size_t)-1) {
        void* new_val = AGENTOS_MALLOC(value_size);
        if (!new_val) return AGENTOS_ENOMEM;
        memcpy(new_val, value, value_size);
        AGENTOS_FREE(mem->entries[existing].value);
        mem->entries[existing].value = new_val;
        mem->entries[existing].value_size = value_size;
        if (type) {
            char* t = AGENTOS_STRDUP(type);
            if (t) { AGENTOS_FREE(mem->entries[existing].type); mem->entries[existing].type = t; }
        }
        mem->entries[existing].pinned = pin;
        wm_update_lru(mem, existing);
        return AGENTOS_SUCCESS;
    }

    if (mem->count >= mem->capacity && !pin) {
        wm_evict_one(mem);
    }
    if (mem->count >= mem->capacity) return AGENTOS_ENOMEM;

    struct wm_entry* e = &mem->entries[mem->count];
    e->key = AGENTOS_STRDUP(key);
    e->value = AGENTOS_MALLOC(value_size);
    if (!e->key || !e->value) {
        AGENTOS_FREE(e->key); AGENTOS_FREE(e->value);
        e->key = NULL; e->value = NULL;
        return AGENTOS_ENOMEM;
    }
    memcpy(e->value, value, value_size);
    e->value_size = value_size;
    e->type = type ? AGENTOS_STRDUP(type) : NULL;
    e->created_ns = tc_time_now_ns();
    e->last_accessed_ns = e->created_ns;
    e->access_count = 1;
    e->pinned = pin;

    wm_update_lru(mem, mem->count);
    mem->count++;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_tc_working_memory_retrieve(
    agentos_working_memory_t* mem,
    const char* key,
    void** out_value,
    size_t* out_size) {

    if (!mem || !key || !out_value) return AGENTOS_EINVAL;

    size_t idx = wm_find_key(mem, key);
    if (idx == (size_t)-1) {
        mem->misses++;
        return AGENTOS_ENOENT;
    }

    mem->hits++;
    wm_update_lru(mem, idx);
    *out_value = mem->entries[idx].value;
    if (out_size) *out_size = mem->entries[idx].value_size;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_tc_working_memory_remove(
    agentos_working_memory_t* mem,
    const char* key) {

    if (!mem || !key) return AGENTOS_EINVAL;

    size_t idx = wm_find_key(mem, key);
    if (idx == (size_t)-1) return AGENTOS_ENOENT;

    AGENTOS_FREE(mem->entries[idx].key);
    AGENTOS_FREE(mem->entries[idx].value);
    if (mem->entries[idx].type) AGENTOS_FREE(mem->entries[idx].type);

    mem->entries[idx] = mem->entries[mem->count - 1];
    mem->count--;
    return AGENTOS_SUCCESS;
}

void agentos_tc_working_memory_clear_unpinned(agentos_working_memory_t* mem) {
    if (!mem) return;
    size_t i = 0;
    while (i < mem->count) {
        if (!mem->entries[i].pinned) {
            AGENTOS_FREE(mem->entries[i].key);
            AGENTOS_FREE(mem->entries[i].value);
            if (mem->entries[i].type) AGENTOS_FREE(mem->entries[i].type);
            mem->entries[i] = mem->entries[mem->count - 1];
            mem->count--;
        } else {
            i++;
        }
    }
}

/* ============================================================================
 * Thinking Step 实现
 * ============================================================================ */

agentos_error_t agentos_tc_step_create(
    agentos_thinking_chain_t* chain,
    tc_step_type_t type,
    const char* input,
    size_t input_len,
    const uint32_t* depends_on,
    size_t depends_count,
    agentos_thinking_step_t** out_step) {

    if (!chain || !out_step) return AGENTOS_EINVAL;

    if (chain->step_count >= TC_MAX_THINKING_STEPS) return AGENTOS_ERANGE;

    if (chain->step_count >= chain->step_capacity) {
        size_t new_cap = chain->step_capacity * 2;
        agentos_thinking_step_t* new_steps = (agentos_thinking_step_t*)AGENTOS_REALLOC(
            chain->steps, new_cap * sizeof(agentos_thinking_step_t));
        if (!new_steps) return AGENTOS_ENOMEM;
        chain->steps = new_steps;
        chain->step_capacity = new_cap;
    }

    agentos_thinking_step_t* step = &chain->steps[chain->step_count];
    memset(step, 0, sizeof(agentos_thinking_step_t));

    step->step_id = chain->next_step_id++;
    step->type = type;
    step->status = TC_STATUS_PENDING;
    step->start_time_ns = 0;
    step->end_time_ns = 0;
    step->confidence = 0.0f;
    step->correction_count = 0;
    step->verify_result = TC_VERIFY_ACCEPT;

    if (input && input_len > 0) {
        step->raw_input = (char*)AGENTOS_MALLOC(input_len + 1);
        if (step->raw_input) {
            memcpy(step->raw_input, input, input_len);
            step->raw_input[input_len] = '\0';
            step->raw_input_len = input_len;
        }
    }

    if (depends_on && depends_count > 0) {
        step->depends_on = (uint32_t*)AGENTOS_MALLOC(depends_count * sizeof(uint32_t));
        if (step->depends_on) {
            memcpy(step->depends_on, depends_on, depends_count * sizeof(uint32_t));
            step->depends_count = depends_count;
        }
    }

    chain->step_count++;
    if (chain->ctx_window) chain->ctx_window->total_steps++;

    *out_step = step;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_tc_step_complete(
    agentos_thinking_step_t* step,
    const char* content,
    size_t content_len,
    float confidence,
    const char* role) {

    if (!step || !content || content_len == 0) return AGENTOS_EINVAL;

    step->content = (char*)AGENTOS_MALLOC(content_len + 1);
    if (!step->content) return AGENTOS_ENOMEM;
    memcpy(step->content, content, content_len);
    step->content[content_len] = '\0';
    step->content_len = content_len;

    step->confidence = (confidence >= 0.0f && confidence <= 1.0f) ? confidence : 0.5f;
    step->status = TC_STATUS_COMPLETED;
    step->end_time_ns = tc_time_now_ns();

    if (role) step->role = AGENTOS_STRDUP(role);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_tc_step_verify(
    agentos_thinking_step_t* step,
    int* is_valid,
    const char* critique,
    size_t critique_len) {

    if (!step || !is_valid) return AGENTOS_EINVAL;

    if (critique && critique_len > 0) {
        step->critique = (char*)AGENTOS_MALLOC(critique_len + 1);
        if (step->critique) {
            memcpy(step->critique, critique, critique_len);
            step->critique[critique_len] = '\0';
            step->critique_len = critique_len;
        }
    }

    *is_valid = (step->verify_result == TC_VERIFY_ACCEPT ||
                 step->verify_result == TC_VERIFY_MINOR_FIX) ? 1 : 0;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_tc_step_correct(
    agentos_thinking_step_t* step,
    const char* corrected_content,
    size_t corrected_len) {

    if (!step || !corrected_content || corrected_len == 0) return AGENTOS_EINVAL;
    if (step->correction_count >= (step->chain_ref ?
         step->chain_ref->ctx_window->max_corrections_per_chunk :
         TC_MAX_CORRECTIONS_DEFAULT)) {
        step->status = TC_STATUS_SKIPPED;
        return AGENTOS_ERANGE;
    }

    char** new_history = (char**)AGENTOS_REALLOC(
        step->correction_history,
        (step->correction_history_count + 1) * sizeof(char*));
    if (!new_history && step->correction_history_count > 0) return AGENTOS_ENOMEM;
    step->correction_history = new_history;

    if (step->content) {
        step->correction_history[step->correction_history_count++] = step->content;
    }

    step->content = (char*)AGENTOS_MALLOC(corrected_len + 1);
    if (!step->content) return AGENTOS_ENOMEM;
    memcpy(step->content, corrected_content, corrected_len);
    step->content[corrected_len] = '\0';
    step->content_len = corrected_len;
    step->correction_count++;
    step->status = TC_STATUS_CORRECTED;

    if (step->chain_ref && step->chain_ref->ctx_window) {
        step->chain_ref->ctx_window->total_corrections++;
    }

    return AGENTOS_SUCCESS;
}

int agentos_tc_step_is_ready(const agentos_thinking_step_t* step,
                              const agentos_thinking_chain_t* chain) {

    if (!step || !chain) return -1;

    for (size_t d = 0; d < step->depends_count; d++) {
        uint32_t dep_id = step->depends_on[d];
        int found_completed = 0;
        for (size_t s = 0; s < chain->step_count; s++) {
            if (chain->steps[s].step_id == dep_id &&
                (chain->steps[s].status == TC_STATUS_COMPLETED ||
                 chain->steps[s].status == TC_STATUS_CORRECTED)) {
                found_completed = 1;
                break;
            }
        }
        if (!found_completed) return 0;
    }
    return 1;
}

/* ============================================================================
 * Thinking Chain 编排实现
 * ============================================================================ */

agentos_error_t agentos_tc_chain_create(
    const char* goal,
    size_t max_tokens,
    size_t wm_capacity,
    agentos_thinking_chain_t** out_chain) {

    if (!out_chain) return AGENTOS_EINVAL;

    agentos_thinking_chain_t* chain = (agentos_thinking_chain_t*)AGENTOS_CALLOC(1, sizeof(agentos_thinking_chain_t));
    if (!chain) return AGENTOS_ENOMEM;

    chain->session_id = tc_time_now_ns();
    chain->session_goal = goal ? AGENTOS_STRDUP(goal) : AGENTOS_STRDUP("");
    chain->active = 0;
    chain->next_step_id = 0;
    chain->created_ns = tc_time_now_ns();
    chain->last_activity_ns = chain->created_ns;
    chain->on_step_completed = NULL;
    chain->on_correction = NULL;
    chain->callback_user_data = NULL;

    agentos_error_t err = agentos_tc_context_window_create(max_tokens, &chain->ctx_window);
    if (err != AGENTOS_SUCCESS) {
        if (chain->session_goal) AGENTOS_FREE(chain->session_goal);
        AGENTOS_FREE(chain);
        return err;
    }

    err = agentos_tc_working_memory_create(wm_capacity, &chain->working_mem);
    if (err != AGENTOS_SUCCESS) {
        agentos_tc_context_window_destroy(chain->ctx_window);
        if (chain->session_goal) AGENTOS_FREE(chain->session_goal);
        AGENTOS_FREE(chain);
        return err;
    }

    chain->step_capacity = 32;
    chain->steps = (agentos_thinking_step_t*)AGENTOS_CALLOC(chain->step_capacity, sizeof(agentos_thinking_step_t));
    if (!chain->steps) {
        agentos_tc_working_memory_destroy(chain->working_mem);
        agentos_tc_context_window_destroy(chain->ctx_window);
        if (chain->session_goal) AGENTOS_FREE(chain->session_goal);
        AGENTOS_FREE(chain);
        return AGENTOS_ENOMEM;
    }
    chain->step_count = 0;

    *out_chain = chain;
    return AGENTOS_SUCCESS;
}

void agentos_tc_chain_destroy(agentos_thinking_chain_t* chain) {
    if (!chain) return;

    for (size_t i = 0; i < chain->step_count; i++) {
        agentos_thinking_step_t* s = &chain->steps[i];
        if (s->raw_input) AGENTOS_FREE(s->raw_input);
        if (s->content) AGENTOS_FREE(s->content);
        if (s->critique) AGENTOS_FREE(s->critique);
        if (s->role) AGENTOS_FREE(s->role);
        if (s->depends_on) AGENTOS_FREE(s->depends_on);
        if (s->dependents) AGENTOS_FREE(s->dependents);
        if (s->correction_history) {
            for (size_t c = 0; c < s->correction_history_count; c++)
                AGENTOS_FREE(s->correction_history[c]);
            AGENTOS_FREE(s->correction_history);
        }
    }
    AGENTOS_FREE(chain->steps);

    agentos_tc_context_window_destroy(chain->ctx_window);
    agentos_tc_working_memory_destroy(chain->working_mem);
    if (chain->session_goal) AGENTOS_FREE(chain->session_goal);
    AGENTOS_FREE(chain);
}

agentos_error_t agentos_tc_chain_start(agentos_thinking_chain_t* chain) {
    if (!chain) return AGENTOS_EINVAL;
    chain->active = 1;
    chain->last_activity_ns = tc_time_now_ns();
    return AGENTOS_SUCCESS;
}

void agentos_tc_chain_stop(agentos_thinking_chain_t* chain) {
    if (!chain) return;
    chain->active = 0;
}

agentos_error_t agentos_tc_chain_next_ready_step(
    agentos_thinking_chain_t* chain,
    agentos_thinking_step_t** out_step) {

    if (!chain || !out_step) return AGENTOS_EINVAL;

    for (size_t i = 0; i < chain->step_count; i++) {
        agentos_thinking_step_t* s = &chain->steps[i];
        if (s->status == TC_STATUS_PENDING) {
            int ready = agentos_tc_step_is_ready(s, chain);
            if (ready > 0) {
                s->status = TC_STATUS_EXECUTING;
                s->start_time_ns = tc_time_now_ns();
                s->chain_ref = chain;
                *out_step = s;
                chain->last_activity_ns = tc_time_now_ns();

                if (chain->ctx_window && s->raw_input) {
                    agentos_tc_context_window_append(chain->ctx_window, s->raw_input, s->raw_input_len);
                }

                return AGENTOS_SUCCESS;
            }
        }
    }

    *out_step = NULL;
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_tc_chain_stats(
    agentos_thinking_chain_t* chain,
    char** out_json,
    size_t* out_len) {

    if (!chain || !out_json) return AGENTOS_EINVAL;

    char* cw_json = NULL;
    agentos_tc_context_window_stats(chain->ctx_window, &cw_json);

    uint32_t pending = 0, executing = 0, completed = 0, corrected = 0, failed = 0;
    for (size_t i = 0; i < chain->step_count; i++) {
        switch (chain->steps[i].status) {
            case TC_STATUS_PENDING: pending++; break;
            case TC_STATUS_EXECUTING: executing++; break;
            case TC_STATUS_COMPLETED: completed++; break;
            case TC_STATUS_CORRECTED: corrected++; break;
            case TC_STATUS_FAILED: failed++; break;
            default: break;
        }
    }

    char buf[1024];
    int len = snprintf(buf, sizeof(buf),
        "{\"session_id\":%llu,"
        "\"goal\":\"%s\","
        "\"active\":%d,"
        "\"steps\":{\"total\":%zu,\"pending\":%u,\"executing\":%u,"
        "\"completed\":%u,\"corrected\":%u,\"failed\":%u},"
        "\"context_window\":%s,"
        "\"working_memory\":{\"capacity\":%zu,\"count\":%zu,\"hits\":%llu,\"misses\":%llu}}",
        (unsigned long long)chain->session_id,
        chain->session_goal ? chain->session_goal : "",
        chain->active,
        chain->step_count, pending, executing, completed, corrected, failed,
        cw_json ? cw_json : "{}",
        chain->working_mem->capacity, chain->working_mem->count,
        (unsigned long long)chain->working_mem->hits,
        (unsigned long long)chain->working_mem->misses);

    if (cw_json) AGENTOS_FREE(cw_json);

    char* result = (char*)AGENTOS_MALLOC(len + 1);
    if (!result) return AGENTOS_ENOMEM;
    memcpy(result, buf, len + 1);
    *out_json = result;
    if (out_len) *out_len = (size_t)len;
    return AGENTOS_SUCCESS;
}

void agentos_tc_chain_set_step_callback(
    agentos_thinking_chain_t* chain,
    void (*on_step_completed)(agentos_thinking_step_t*, void*),
    void (*on_correction)(agentos_thinking_step_t*, const char*, void*),
    void* user_data) {

    if (!chain) return;
    chain->on_step_completed = on_step_completed;
    chain->on_correction = on_correction;
    chain->callback_user_data = user_data;
}

/* ============================================================================
 * DS-007: 执行监控实现
 * ============================================================================ */

static float compute_repetition_score(const char* content, size_t len) {
    if (!content || len < 20) return 0.0f;

    size_t window = len / 2;
    if (window < 10) window = 10;
    if (window > len) window = len;

    int matching_bigrams = 0;
    int total_bigrams = 0;

    for (size_t i = 1; i + window <= len; i++) {
        total_bigrams++;
        if (i + window * 2 <= len &&
            memcmp(content + i, content + i + window, window) == 0) {
            matching_bigrams++;
        }
    }

    if (total_bigrams == 0) return 0.0f;
    return (float)matching_bigrams / (float)total_bigrams;
}

agentos_error_t agentos_tc_step_monitor(
    const agentos_thinking_step_t* step,
    const tc_monitor_config_t* config,
    tc_monitor_result_t* out_result)
{
    if (!step || !out_result) return AGENTOS_EINVAL;

    tc_monitor_config_t defaults = TC_MONITOR_DEFAULTS;
    if (!config) config = &defaults;

    memset(out_result, 0, sizeof(tc_monitor_result_t));
    out_result->anomaly = TC_ANOMALY_NONE;
    out_result->is_critical = 0;
    out_result->severity_score = 0.0f;

    /* 检查1: 超时检测 */
    if (step->status == TC_STATUS_EXECUTING && step->start_time_ns > 0) {
        uint64_t elapsed_ms = (tc_time_now_ns() - step->start_time_ns) / 1000000ULL;
        if (elapsed_ms > config->default_timeout_ms) {
            out_result->anomaly = TC_ANOMALY_TIMEOUT;
            out_result->is_critical = 1;
            out_result->severity_score = 0.95f;
            char desc[128];
            int dlen = snprintf(desc, sizeof(desc),
                "Step#%u timed out: %llums > %ums limit",
                step->step_id,
                (unsigned long long)elapsed_ms, config->default_timeout_ms);
            out_result->description = (char*)AGENTOS_MALLOC(dlen + 1);
            if (out_result->description) {
                memcpy(out_result->description, desc, dlen + 1);
                out_result->description_len = (size_t)dlen;
            }
            return AGENTOS_SUCCESS;
        }
    }

    /* 检查2: 空输出 */
    if (step->content_len == 0 || !step->content) {
        out_result->anomaly = TC_ANOMALY_EMPTY_OUTPUT;
        out_result->severity_score = 0.8f;
        out_result->is_critical = 1;
        const char* desc = "Empty output detected";
        out_result->description = AGENTOS_STRDUP(desc);
        out_result->description_len = strlen(desc);
        return AGENTOS_SUCCESS;
    }

    /* 检查3: 截断输出（过短） */
    if (step->content_len < config->min_output_chars &&
        step->type != TC_STEP_VERIFICATION) {
        out_result->anomaly = TC_ANOMALY_TRUNCATED_OUTPUT;
        out_result->severity_score = 0.5f;
        char desc[96];
        int dlen = snprintf(desc, sizeof(desc),
            "Output too short: %zu chars < %zu minimum",
            step->content_len, config->min_output_chars);
        out_result->description = (char*)AGENTOS_MALLOC(dlen + 1);
        if (out_result->description) {
            memcpy(out_result->description, desc, dlen + 1);
            out_result->description_len = (size_t)dlen;
        }
    }

    /* 检查4: 过长输出 */
    if (step->content_len > config->max_output_chars) {
        float prev_sev = out_result->severity_score;
        out_result->anomaly = TC_ANOMALY_EXCESSIVE_OUTPUT;
        out_result->severity_score = (prev_sev > 0.6f) ? prev_sev : 0.4f;
        if (!out_result->description) {
            char desc[96];
            int dlen = snprintf(desc, sizeof(desc),
                "Output excessive: %zu chars > %zu maximum",
                step->content_len, config->max_output_chars);
            out_result->description = (char*)AGENTOS_MALLOC(dlen + 1);
            if (out_result->description) {
                memcpy(out_result->description, desc, dlen + 1);
                out_result->description_len = (size_t)dlen;
            }
        }
    }

    /* 检查5: 重复内容检测 */
    float rep_score = compute_repetition_score(step->content, step->content_len);
    if (rep_score > config->repetition_threshold) {
        float prev_sev = out_result->severity_score;
        if (prev_sev < 0.5f) {
            out_result->anomaly = TC_ANOMALY_REPETITIVE_CONTENT;
            out_result->severity_score = rep_score;
        } else {
            out_result->severity_score = (prev_sev + rep_score) / 2.0f;
        }
        if (!out_result->description) {
            char desc[96];
            int dlen = snprintf(desc, sizeof(desc),
                "Repetitive content detected (score=%.2f)", rep_score);
            out_result->description = (char*)AGENTOS_MALLOC(dlen + 1);
            if (out_result->description) {
                memcpy(out_result->description, desc, dlen + 1);
                out_result->description_len = (size_t)dlen;
            }
        }
    }

    /* 检查6: 质量门禁（置信度+状态综合判断） */
    if (config->enable_quality_gate) {
        int quality_ok = (step->confidence >= config->quality_gate_threshold) ||
                         (step->status == TC_STATUS_COMPLETED &&
                          step->correction_count == 0);

        if (!quality_ok && step->confidence < config->quality_gate_threshold) {
            out_result->anomaly = TC_ANOMALY_CONFIDENCE_DROP;
            float prev_sev = out_result->severity_score;
            out_result->severity_score = (prev_sev > 0.3f) ? prev_sev : 0.35f;
            out_result->is_critical = (step->confidence < 0.15f) ? 1 : 0;
            if (!out_result->description) {
                char desc[96];
                int dlen = snprintf(desc, sizeof(desc),
                    "Low confidence %.2f below threshold %.2f",
                    step->confidence, config->quality_gate_threshold);
                out_result->description = (char*)AGENTOS_MALLOC(dlen + 1);
                if (out_result->description) {
                    memcpy(out_result->description, desc, dlen + 1);
                    out_result->description_len = (size_t)dlen;
                }
            }
        }
    }

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_tc_chain_health_check(
    const agentos_thinking_chain_t* chain,
    size_t* out_anomaly_count,
    int* out_has_critical)
{
    if (!chain || !out_anomaly_count || !out_has_critical) return AGENTOS_EINVAL;

    *out_anomaly_count = 0;
    *out_has_critical = 0;

    tc_monitor_config_t defaults = TC_MONITOR_DEFAULTS;

    for (size_t i = 0; i < chain->step_count; i++) {
        if (chain->steps[i].status == TC_STATUS_PENDING) continue;

        tc_monitor_result_t mon;
        agentos_error_t err = agentos_tc_step_monitor(&chain->steps[i], &defaults, &mon);
        if (err == AGENTOS_SUCCESS && mon.anomaly != TC_ANOMALY_NONE) {
            (*out_anomaly_count)++;
            if (mon.is_critical) *out_has_critical = 1;
            if (mon.description) AGENTOS_FREE(mon.description);
        }
    }

    return AGENTOS_SUCCESS;
}

/* ============================================================================
 * DS-008: 异常恢复实现
 * ============================================================================ */

#define TC_MAX_RECOVERY_ATTEMPTS   3
#define TC_RETRY_BACKOFF_BASE_MS   500
#define TC_RETRY_BACKOFF_MAX_MS    8000

/* 内部检查点结构（存储在chain中） */
typedef struct tc_checkpoint {
    uint32_t checkpoint_id;
    size_t step_snapshot_count;     /**< 快照时的步骤数 */
    uint64_t timestamp_ns;
} tc_checkpoint_t;

#define TC_MAX_CHECKPOINTS  16

static tc_checkpoint_t* get_checkpoint_storage(agentos_thinking_chain_t* chain) {
    static tc_checkpoint_t s_checkpoints[TC_MAX_CHECKPOINTS] = {{0}};
    static int initialized = 0;
    if (!initialized) { memset(s_checkpoints, 0, sizeof(s_checkpoints)); initialized = 1; }
    return s_checkpoints;
}

agentos_error_t agentos_tc_step_recover(
    agentos_thinking_chain_t* chain,
    agentos_thinking_step_t* failed_step,
    const tc_monitor_result_t* monitor_result,
    agentos_error_t (*corrector_fn)(const char*, size_t, char**, size_t*, void*),
    void* user_data,
    tc_recovery_result_t* out_result)
{
    if (!chain || !failed_step || !out_result) return AGENTOS_EINVAL;

    memset(out_result, 0, sizeof(tc_recovery_result_t));
    out_result->strategy_used = TC_RECOVER_ABORT;
    out_result->success = 0;

    tc_monitor_result_t local_mon;
    if (!monitor_result) {
        agentos_tc_step_monitor(failed_step, NULL, &local_mon);
        monitor_result = &local_mon;
    }

    size_t log_buf_size = 512;
    char* log_buf = (char*)AGENTOS_MALLOC(log_buf_size);
    if (!log_buf) return AGENTOS_ENOMEM;
    int log_pos = 0;

    log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                        "Recovery for step#%u (anomaly=%d): ",
                        failed_step->step_id, monitor_result->anomaly);

    /* 策略1: 重试（最多TC_MAX_RECOVERY_ATTEMPTS次，指数退避） */
    if (corrector_fn && failed_step->raw_input && failed_step->raw_input_len > 0) {
        for (uint32_t attempt = 0; attempt < TC_MAX_RECOVERY_ATTEMPTS; attempt++) {
            out_result->attempts_made++;

            char* new_content = NULL;
            size_t new_len = 0;
            agentos_error_t err = corrector_fn(
                failed_step->raw_input, failed_step->raw_input_len,
                &new_content, &new_len, user_data);

            if (err == AGENTOS_SUCCESS && new_content && new_len > 0) {
                agentos_tc_step_correct(failed_step, new_content, new_len);
                AGENTOS_FREE(new_content);

                /* 验证修正后是否通过质量门禁 */
                tc_monitor_result_t post_mon;
                agentos_tc_step_monitor(failed_step, NULL, &post_mon);
                if (post_mon.anomaly == TC_ANOMALY_NONE ||
                    post_mon.severity_score < 0.4f) {
                    out_result->strategy_used = TC_RECOVER_RETRY;
                    out_result->success = 1;
                    log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                        "retry#%u succeeded", attempt + 1);
                    if (post_mon.description) AGENTOS_FREE(post_mon.description);
                    goto recovery_done;
                }
                if (post_mon.description) AGENTOS_FREE(post_mon.description);
            }

            /* 指数退避等待 */
            unsigned backoff = TC_RETRY_BACKOFF_BASE_MS << attempt;
            if (backoff > TC_RETRY_BACKOFF_MAX_MS) backoff = TC_RETRY_BACKOFF_MAX_MS;
            struct timespec ts = { .tv_sec = (time_t)(backoff / 1000),
                                   .tv_nsec = (long)((backoff % 1000) * 1000000L) };
            nanosleep(&ts, NULL);
        }
        log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                            ", %u retries exhausted", out_result->attempts_made);
    }

    /* 策略2: 降级 —— 标记为SKIPPED并接受当前最佳内容 */
    if (failed_step->content && failed_step->content_len > 0) {
        failed_step->status = TC_STATUS_SKIPPED;
        out_result->strategy_used = TC_RECOVER_DEGRADE;
        out_result->success = 1;
        log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                            ", degraded to skip (kept existing content)");
        goto recovery_done;
    }

    /* 策略3: 回滚到上一个检查点 */
    tc_checkpoint_t* checkpoints = get_checkpoint_storage(chain);
    uint32_t best_cp = 0;
    for (int c = TC_MAX_CHECKPOINTS - 1; c >= 0; c--) {
        if (checkpoints[c].checkpoint_id > 0 &&
            checkpoints[c].step_snapshot_count < chain->step_count) {
            best_cp = checkpoints[c].checkpoint_id;
            break;
        }
    }

    if (best_cp > 0) {
        size_t removed = agentos_tc_chain_rollback(chain, best_cp);
        out_result->strategy_used = TC_RECOVER_ROLLBACK;
        out_result->success = (removed > 0) ? 1 : 0;
        log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                            ", rolled back to cp#%u (%zu steps removed)",
                            best_cp, removed);
        goto recovery_done;
    }

    /* 所有策略失败 → 中止 */
    log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                        ", all strategies failed -> ABORT");
    failed_step->status = TC_STATUS_FAILED;

recovery_done:
    out_result->recovery_log = log_buf;
    out_result->recovery_log_len = (size_t)log_pos;

    AGENTOS_LOG_INFO("TC Recovery: step#%u strategy=%d success=%d attempts=%u",
                    failed_step->step_id, out_result->strategy_used,
                    out_result->success, out_result->attempts_made);
    return AGENTOS_SUCCESS;
}

uint32_t agentos_tc_chain_checkpoint(agentos_thinking_chain_t* chain) {
    if (!chain) return 0;

    tc_checkpoint_t* checkpoints = get_checkpoint_storage(chain);

    uint32_t next_id = 0;
    for (int c = 0; c < TC_MAX_CHECKPOINTS; c++) {
        if (checkpoints[c].checkpoint_id >= next_id)
            next_id = checkpoints[c].checkpoint_id + 1;
    }
    if (next_id == 0) next_id = 1;

    int slot = -1;
    for (int c = 0; c < TC_MAX_CHECKPOINTS; c++) {
        if (checkpoints[c].checkpoint_id == 0) { slot = c; break; }
    }
    if (slot < 0) {
        slot = 0;
        memmove(&checkpoints[0], &checkpoints[1],
                (TC_MAX_CHECKPOINTS - 1) * sizeof(tc_checkpoint_t));
        checkpoints[TC_MAX_CHECKPOINTS - 1].checkpoint_id = 0;
    }

    checkpoints[slot].checkpoint_id = next_id;
    checkpoints[slot].step_snapshot_count = chain->step_count;
    checkpoints[slot].timestamp_ns = tc_time_now_ns();

    AGENTOS_LOG_INFO("TC Checkpoint#%u created at step_count=%zu",
                    next_id, chain->step_count);
    return next_id;
}

size_t agentos_tc_chain_rollback(
    agentos_thinking_chain_t* chain,
    uint32_t checkpoint_id)
{
    if (!chain || checkpoint_id == 0) return 0;

    tc_checkpoint_t* checkpoints = get_checkpoint_storage(chain);
    size_t target_steps = 0;

    for (int c = 0; c < TC_MAX_CHECKPOINTS; c++) {
        if (checkpoints[c].checkpoint_id == checkpoint_id) {
            target_steps = checkpoints[c].step_snapshot_count;
            break;
        }
    }

    if (target_steps == 0 || target_steps >= chain->step_count) return 0;

    size_t removed = chain->step_count - target_steps;

    for (size_t i = target_steps; i < chain->step_count; i++) {
        agentos_thinking_step_t* step = &chain->steps[i];
        if (step->content) AGENTOS_FREE(step->content);
        if (step->raw_input) AGENTOS_FREE(step->raw_input);
        if (step->critique) AGENTOS_FREE(step->critique);
        if (step->role) AGENTOS_FREE(step->role);
        if (step->depends_on) {
            for (size_t d = 0; d < step->depends_count; d++)
                AGENTOS_FREE(step->depends_on[d]);
            AGENTOS_FREE(step->depends_on);
        }
        if (step->dependents) AGENTOS_FREE(step->dependents);
        if (step->correction_history) {
            for (size_t h = 0; h < step->correction_history_count; h++)
                AGENTOS_FREE(step->correction_history[h]);
            AGENTOS_FREE(step->correction_history);
        }
        memset(step, 0, sizeof(agentos_thinking_step_t));
    }

    chain->step_count = target_steps;
    chain->next_step_id = (uint32_t)target_steps + 1;

    AGENTOS_LOG_INFO("TC Rollback to cp#%u: removed %zu steps (now %zu)",
                    checkpoint_id, removed, chain->step_count);
    return removed;
}
