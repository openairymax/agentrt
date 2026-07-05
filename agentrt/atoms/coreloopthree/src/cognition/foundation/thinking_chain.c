/**
 * @file thinking_chain.c
 * @brief 思考链路模块完整实现 - DS-001
 * @copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * 实现Thinkdual的三大核心组件:
 * 1. Context Window - Token预算管理+滑动窗口
 * 2. Working Memory - 短期键值缓存+LRU淘汰
 * 3. Thinking Step DAG - 推理步骤依赖链
 */

#include "thinking_chain.h"

#include "agentrt.h"
#include "logger.h"
#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t tc_time_now_ns(void)
{
    return agentrt_time_ns();
}

static size_t estimate_token_count(const char *data, size_t len)
{
    if (!data || len == 0)
        return 0;
    size_t tokens = 0;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == ' ' || data[i] == '\n' || data[i] == '\t')
            tokens++;
    }
    return (tokens > 0) ? tokens : (len / 4); /* 英文约4字符/token，中文更少 */
}

/* ============================================================================
 * Context Window 实现
 * ============================================================================ */

#define CW_BUFFER_MIN_SIZE 65536

agentrt_error_t agentrt_tc_context_window_create(size_t max_tokens,
                                                 agentrt_context_window_t **out_window)
{

    if (!out_window) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_create: NULL out_window parameter");
        return AGENTRT_EINVAL;
    }

    agentrt_context_window_t *w =
        (agentrt_context_window_t *)AGENTRT_CALLOC(1, sizeof(agentrt_context_window_t));
    if (!w) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_create: allocation failed for context window (max_tokens=%zu)", max_tokens);
        return AGENTRT_ENOMEM;
    }

    w->max_tokens = (max_tokens > 0) ? max_tokens : TC_MAX_TOKENS_DEFAULT;
    w->used_tokens = 0;
    w->chunk_size = TC_CHUNK_SIZE_DEFAULT;
    w->buffer_capacity = w->max_tokens * 8; /* 每token约8字节估计 */
    if (w->buffer_capacity < CW_BUFFER_MIN_SIZE)
        w->buffer_capacity = CW_BUFFER_MIN_SIZE;

    w->buffer = (char *)AGENTRT_CALLOC(1, w->buffer_capacity);
    if (!w->buffer) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_create: buffer allocation failed (capacity=%zu)", w->buffer_capacity);
        AGENTRT_FREE(w);
        return AGENTRT_ENOMEM;
    }

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
    return AGENTRT_SUCCESS;
}

void agentrt_tc_context_window_destroy(agentrt_context_window_t *window)
{
    if (!window)
        return;
    if (window->buffer)
        AGENTRT_FREE(window->buffer);
    AGENTRT_FREE(window);
}

ssize_t agentrt_tc_context_window_append(agentrt_context_window_t *window, const char *data,
                                         size_t len)
{

    if (!window || !data || len == 0) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_append: NULL/invalid params (window=%p data=%p len=%zu)", (void *)window, (void *)data, len);
        return (ssize_t)AGENTRT_EINVAL;
    }

    size_t new_tokens = estimate_token_count(data, len);

    if (window->used_tokens + new_tokens > window->max_tokens) {
        AGENTRT_LOG_WARN("agentrt_tc_context_window_append: token limit exceeded (used=%zu + new=%zu > max=%zu), sliding window eviction triggered", window->used_tokens, new_tokens, window->max_tokens);
        /* 滑动窗口：丢弃最旧的数据以腾出空间 */
        size_t to_evict = (window->used_tokens + new_tokens) - window->max_tokens;
        size_t evicted_bytes = 0;
        while (evicted_bytes < to_evict * 4 && window->buffer_used > 0) {
            char c = window->buffer[window->buffer_tail];
            window->buffer_tail = (window->buffer_tail + 1) % window->buffer_capacity;
            window->buffer_used--;
            evicted_bytes++;
            if (c == ' ' || c == '\n')
                window->used_tokens--;
        }
    }

    for (size_t i = 0; i < len; i++) {
        window->buffer[window->buffer_head] = data[i];
        window->buffer_head = (window->buffer_head + 1) % window->buffer_capacity;
        window->buffer_used++;
        if (window->buffer_used >= window->buffer_capacity) {
            AGENTRT_LOG_WARN("agentrt_tc_context_window_append: buffer capacity reached, truncating (buffer_used=%zu capacity=%zu input_len=%zu)", window->buffer_used, window->buffer_capacity, len);
            break;
        }
    }

    window->used_tokens += new_tokens;
    window->total_tokens_generated += new_tokens;
    return (ssize_t)(window->used_tokens);
}

agentrt_error_t agentrt_tc_context_window_get_recent(agentrt_context_window_t *window,
                                                     size_t token_count, char **out_data,
                                                     size_t *out_len)
{

    if (!window || !out_data) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_get_recent: NULL params (window=%p out_data=%p)", (void *)window, (void *)out_data);
        return AGENTRT_EINVAL;
    }

    size_t avail =
        (token_count > 0 && token_count < window->used_tokens) ? token_count : window->used_tokens;
    if (avail == 0) {
        *out_data = AGENTRT_STRDUP("");
        if (out_len)
            *out_len = 0;
        return AGENTRT_SUCCESS;
    }

    size_t est_bytes = avail * 4;
    char *result = (char *)AGENTRT_MALLOC(est_bytes + 1);
    if (!result) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_get_recent: allocation failed (est_bytes=%zu)", est_bytes + 1);
        return AGENTRT_ENOMEM;
    }

    size_t read_pos = (window->buffer_head > avail * 4) ? (window->buffer_head - avail * 4) : 0;
    if (read_pos >= window->buffer_capacity)
        read_pos = 0;

    size_t count = 0;
    size_t written = 0;
    size_t start = read_pos;

    do {
        if (written >= est_bytes)
            break;
        result[written++] = window->buffer[read_pos];
        read_pos = (read_pos + 1) % window->buffer_capacity;
        count++;
        if (window->buffer[read_pos - 1] == ' ' || window->buffer[read_pos - 1] == '\n')
            avail--;
    } while (count < window->buffer_used && read_pos != start && avail > 0);

    result[written] = '\0';
    *out_data = result;
    if (out_len)
        *out_len = written;
    return AGENTRT_SUCCESS;
}

int agentrt_tc_context_window_has_space(agentrt_context_window_t *window, size_t needed_tokens)
{

    if (!window)
        return 0;
    return (int)(window->used_tokens + needed_tokens <= window->max_tokens);
}

agentrt_error_t agentrt_tc_context_window_stats(agentrt_context_window_t *window, char **out_json)
{

    if (!window || !out_json) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_stats: NULL params (window=%p out_json=%p)", (void *)window, (void *)out_json);
        return AGENTRT_EINVAL;
    }

    char buf[512];
    int __attribute__((unused)) len = snprintf(
        buf, sizeof(buf),
        "{\"max_tokens\":%zu,\"used_tokens\":%zu,"
        "\"generated\":%llu,\"corrections\":%llu,"
        "\"steps\":{\"total\":%u,\"completed\":%u},"
        "\"utilization_pct\":%.1f}",
        window->max_tokens, window->used_tokens, (unsigned long long)window->total_tokens_generated,
        (unsigned long long)window->total_corrections, window->total_steps, window->completed_steps,
        window->max_tokens > 0 ? (float)window->used_tokens / (float)window->max_tokens * 100.0f
                               : 0.0f);

    char *result = AGENTRT_STRDUP(buf);
    if (!result) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_stats: STRDUP failed for stats JSON");
        return AGENTRT_ENOMEM;
    }
    *out_json = result;
    return AGENTRT_SUCCESS;
}

/* ============================================================================
 * Working Memory 实现
 * ============================================================================ */

agentrt_error_t agentrt_tc_working_memory_create(size_t capacity,
                                                 agentrt_working_memory_t **out_mem)
{

    if (!out_mem) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_create: NULL out_mem parameter");
        return AGENTRT_EINVAL;
    }

    size_t cap = (capacity > 0) ? capacity : TC_WORKING_MEM_CAPACITY;
    agentrt_working_memory_t *mem =
        (agentrt_working_memory_t *)AGENTRT_CALLOC(1, sizeof(agentrt_working_memory_t));
    if (!mem) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_create: allocation failed for working memory (capacity=%zu)", cap);
        return AGENTRT_ENOMEM;
    }

    mem->entries = (struct wm_entry *)AGENTRT_CALLOC(cap, sizeof(struct wm_entry));
    if (!mem->entries) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_create: entries allocation failed (capacity=%zu)", cap);
        AGENTRT_FREE(mem);
        return AGENTRT_ENOMEM;
    }
    mem->lru_order = (uint32_t *)AGENTRT_CALLOC(cap, sizeof(uint32_t));
    if (!mem->lru_order) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_create: lru_order allocation failed (capacity=%zu)", cap);
        AGENTRT_FREE(mem->entries);
        AGENTRT_FREE(mem);
        return AGENTRT_ENOMEM;
    }

    mem->capacity = cap;
    mem->count = 0;
    mem->lru_index = 0;
    mem->hits = 0;
    mem->misses = 0;
    mem->evictions = 0;

    *out_mem = mem;
    return AGENTRT_SUCCESS;
}

void agentrt_tc_working_memory_destroy(agentrt_working_memory_t *mem)
{
    if (!mem)
        return;
    for (size_t i = 0; i < mem->count; i++) {
        if (mem->entries[i].key)
            AGENTRT_FREE(mem->entries[i].key);
        if (mem->entries[i].value)
            AGENTRT_FREE(mem->entries[i].value);
        if (mem->entries[i].type)
            AGENTRT_FREE(mem->entries[i].type);
    }
    AGENTRT_FREE(mem->entries);
    AGENTRT_FREE(mem->lru_order);
    AGENTRT_FREE(mem);
}

static size_t wm_find_key(agentrt_working_memory_t *mem, const char *key)
{
    for (size_t i = 0; i < mem->count; i++) {
        if (mem->entries[i].key && strcmp(mem->entries[i].key, key) == 0)
            return i;
    }
    return (size_t)-1;
}

static void wm_update_lru(agentrt_working_memory_t *mem, size_t idx)
{
    if (idx >= mem->count)
        return;
    mem->entries[idx].last_accessed_ns = tc_time_now_ns();
    mem->entries[idx].access_count++;
    mem->lru_order[mem->lru_index++] = (uint32_t)idx;
    if (mem->lru_index >= mem->capacity)
        mem->lru_index = 0;
}

static void wm_evict_one(agentrt_working_memory_t *mem)
{
    if (mem->count == 0)
        return;

    size_t victim = (size_t)-1;
    uint64_t oldest_access = UINT64_MAX;

    for (size_t i = 0; i < mem->count; i++) {
        if (!mem->entries[i].pinned && mem->entries[i].last_accessed_ns < oldest_access) {
            oldest_access = mem->entries[i].last_accessed_ns;
            victim = i;
        }
    }

    if (victim != (size_t)-1) {
        AGENTRT_FREE(mem->entries[victim].key);
        AGENTRT_FREE(mem->entries[victim].value);
        if (mem->entries[victim].type)
            AGENTRT_FREE(mem->entries[victim].type);

        mem->entries[victim] = mem->entries[mem->count - 1];
        mem->count--;
        mem->evictions++;
    }
}

agentrt_error_t agentrt_tc_working_memory_store(agentrt_working_memory_t *mem, const char *key,
                                                const void *value, size_t value_size,
                                                const char *type, int pin)
{

    if (!mem || !key || !value || value_size == 0) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_store: NULL/invalid params (mem=%p key=%p value=%p value_size=%zu)", (void *)mem, (void *)key, (void *)value, value_size);
        return AGENTRT_EINVAL;
    }

    size_t existing = wm_find_key(mem, key);
    if (existing != (size_t)-1) {
        void *new_val = AGENTRT_MALLOC(value_size);
        if (!new_val) {
            AGENTRT_LOG_ERROR("agentrt_tc_working_memory_store: value update allocation failed for existing key (key=%s value_size=%zu)", key, value_size);
            return AGENTRT_ENOMEM;
        }
        __builtin_memcpy(new_val, value, value_size);
        AGENTRT_FREE(mem->entries[existing].value);
        mem->entries[existing].value = new_val;
        mem->entries[existing].value_size = value_size;
        if (type) {
            char *t = AGENTRT_STRDUP(type);
            if (t) {
                AGENTRT_FREE(mem->entries[existing].type);
                mem->entries[existing].type = t;
            }
        }
        mem->entries[existing].pinned = pin;
        wm_update_lru(mem, existing);
        return AGENTRT_SUCCESS;
    }

    if (mem->count >= mem->capacity && !pin) {
        wm_evict_one(mem);
    }
    if (mem->count >= mem->capacity) {
        AGENTRT_LOG_WARN("agentrt_tc_working_memory_store: capacity exhausted (count=%zu capacity=%zu key=%s)", mem->count, mem->capacity, key);
        return AGENTRT_ENOMEM;
    }

    struct wm_entry *e = &mem->entries[mem->count];
    e->key = AGENTRT_STRDUP(key);
    e->value = AGENTRT_MALLOC(value_size);
    if (!e->key || !e->value) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_store: key/value allocation failed (key=%s value_size=%zu)", key, value_size);
        AGENTRT_FREE(e->key);
        AGENTRT_FREE(e->value);
        e->key = NULL;
        e->value = NULL;
        return AGENTRT_ENOMEM;
    }
    __builtin_memcpy(e->value, value, value_size);
    e->value_size = value_size;
    e->type = type ? AGENTRT_STRDUP(type) : NULL;
    e->created_ns = tc_time_now_ns();
    e->last_accessed_ns = e->created_ns;
    e->access_count = 1;
    e->pinned = pin;

    wm_update_lru(mem, mem->count);
    mem->count++;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_working_memory_retrieve(agentrt_working_memory_t *mem, const char *key,
                                                   void **out_value, size_t *out_size)
{

    if (!mem || !key || !out_value) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_retrieve: NULL params (mem=%p key=%p out_value=%p)", (void *)mem, (void *)key, (void *)out_value);
        return AGENTRT_EINVAL;
    }

    size_t idx = wm_find_key(mem, key);
    if (idx == (size_t)-1) {
        mem->misses++;
        AGENTRT_LOG_WARN("agentrt_tc_working_memory_retrieve: cache miss (key=%s misses=%llu)", key, (unsigned long long)mem->misses);
        return AGENTRT_ENOENT;
    }

    mem->hits++;
    wm_update_lru(mem, idx);
    *out_value = mem->entries[idx].value;
    if (out_size)
        *out_size = mem->entries[idx].value_size;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_working_memory_remove(agentrt_working_memory_t *mem, const char *key)
{

    if (!mem || !key) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_remove: NULL params (mem=%p key=%p)", (void *)mem, (void *)key);
        return AGENTRT_EINVAL;
    }

    size_t idx = wm_find_key(mem, key);
    if (idx == (size_t)-1) {
        AGENTRT_LOG_WARN("agentrt_tc_working_memory_remove: key not found (key=%s)", key);
        return AGENTRT_ENOENT;
    }

    AGENTRT_FREE(mem->entries[idx].key);
    AGENTRT_FREE(mem->entries[idx].value);
    if (mem->entries[idx].type)
        AGENTRT_FREE(mem->entries[idx].type);

    mem->entries[idx] = mem->entries[mem->count - 1];
    mem->count--;
    return AGENTRT_SUCCESS;
}

void agentrt_tc_working_memory_clear_unpinned(agentrt_working_memory_t *mem)
{
    if (!mem)
        return;
    size_t i = 0;
    while (i < mem->count) {
        if (!mem->entries[i].pinned) {
            AGENTRT_FREE(mem->entries[i].key);
            AGENTRT_FREE(mem->entries[i].value);
            if (mem->entries[i].type)
                AGENTRT_FREE(mem->entries[i].type);
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

agentrt_error_t agentrt_tc_step_create(agentrt_thinking_chain_t *chain, tc_step_type_t type,
                                       const char *input, size_t input_len,
                                       const uint32_t *depends_on, size_t depends_count,
                                       agentrt_thinking_step_t **out_step)
{

    if (!chain || !out_step) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_create: NULL params (chain=%p out_step=%p)", (void *)chain, (void *)out_step);
        return AGENTRT_EINVAL;
    }

    if (chain->step_count >= TC_MAX_THINKING_STEPS) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_create: max thinking steps exceeded (step_count=%zu max=%d type=%d)", chain->step_count, TC_MAX_THINKING_STEPS, (int)type);
        return AGENTRT_ERANGE;
    }

    if (chain->step_count >= chain->step_capacity) {
        size_t new_cap = chain->step_capacity * 2;
        agentrt_thinking_step_t *new_steps = (agentrt_thinking_step_t *)AGENTRT_REALLOC(
            chain->steps, new_cap * sizeof(agentrt_thinking_step_t));
        if (!new_steps) {
            AGENTRT_LOG_ERROR("agentrt_tc_step_create: steps REALLOC failed (new_cap=%zu)", new_cap);
            return AGENTRT_ENOMEM;
        }
        chain->steps = new_steps;
        chain->step_capacity = new_cap;
    }

    agentrt_thinking_step_t *step = &chain->steps[chain->step_count];
    __builtin_memset(step, 0, sizeof(agentrt_thinking_step_t));

    step->step_id = chain->next_step_id++;
    step->type = type;
    step->status = TC_STATUS_PENDING;
    step->start_time_ns = 0;
    step->end_time_ns = 0;
    step->confidence = 0.0f;
    step->correction_count = 0;
    step->verify_result = TC_VERIFY_ACCEPT;

    if (input && input_len > 0) {
        step->raw_input = (char *)AGENTRT_MALLOC(input_len + 1);
        if (step->raw_input) {
            __builtin_memcpy(step->raw_input, input, input_len);
            step->raw_input[input_len] = '\0';
            step->raw_input_len = input_len;
        }
    }

    if (depends_on && depends_count > 0) {
        SAFE_MALLOC_ARRAY(step->depends_on, depends_count, sizeof(uint32_t));
        if (step->depends_on) {
            __builtin_memcpy(step->depends_on, depends_on, depends_count * sizeof(uint32_t));
            step->depends_count = depends_count;
        }
    }

    chain->step_count++;
    if (chain->ctx_window)
        chain->ctx_window->total_steps++;

    *out_step = step;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_step_complete(agentrt_thinking_step_t *step, const char *content,
                                         size_t content_len, float confidence, const char *role)
{

    if (!step || !content || content_len == 0) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_complete: NULL/invalid params (step=%p content=%p content_len=%zu)", (void *)step, (void *)content, content_len);
        return AGENTRT_EINVAL;
    }

    if (step->status == TC_STATUS_COMPLETED || step->status == TC_STATUS_CORRECTED) {
        AGENTRT_LOG_WARN("agentrt_tc_step_complete: state transition error, step already in terminal state (step_id=%u status=%d)", step->step_id, (int)step->status);
    }

    step->content = (char *)AGENTRT_MALLOC(content_len + 1);
    if (!step->content) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_complete: content allocation failed (step_id=%u content_len=%zu)", step->step_id, content_len);
        return AGENTRT_ENOMEM;
    }
    __builtin_memcpy(step->content, content, content_len);
    step->content[content_len] = '\0';
    step->content_len = content_len;

    step->confidence = (confidence >= 0.0f && confidence <= 1.0f) ? confidence : 0.5f;
    step->status = TC_STATUS_COMPLETED;
    step->end_time_ns = tc_time_now_ns();

    if (role)
        step->role = AGENTRT_STRDUP(role);

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_step_verify(agentrt_thinking_step_t *step, int *is_valid,
                                       const char *critique, size_t critique_len)
{

    if (!step || !is_valid) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_verify: NULL params (step=%p is_valid=%p)", (void *)step, (void *)is_valid);
        return AGENTRT_EINVAL;
    }

    if (critique && critique_len > 0) {
        step->critique = (char *)AGENTRT_MALLOC(critique_len + 1);
        if (step->critique) {
            __builtin_memcpy(step->critique, critique, critique_len);
            step->critique[critique_len] = '\0';
            step->critique_len = critique_len;
        }
    }

    *is_valid =
        (step->verify_result == TC_VERIFY_ACCEPT || step->verify_result == TC_VERIFY_MINOR_FIX) ? 1
                                                                                                : 0;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_step_correct(agentrt_thinking_step_t *step,
                                        const char *corrected_content, size_t corrected_len)
{

    if (!step || !corrected_content || corrected_len == 0) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_correct: NULL/invalid params (step=%p corrected_content=%p corrected_len=%zu)", (void *)step, (void *)corrected_content, corrected_len);
        return AGENTRT_EINVAL;
    }
    if (step->status == TC_STATUS_PENDING) {
        AGENTRT_LOG_WARN("agentrt_tc_step_correct: state transition error, correcting a PENDING step (step_id=%u)", step->step_id);
    }
    if (step->correction_count >= (step->chain_ref
                                       ? step->chain_ref->ctx_window->max_corrections_per_chunk
                                       : TC_MAX_CORRECTIONS_DEFAULT)) {
        AGENTRT_LOG_WARN("agentrt_tc_step_correct: max corrections exceeded (step_id=%u correction_count=%d)", step->step_id, step->correction_count);
        step->status = TC_STATUS_SKIPPED;
        return AGENTRT_ERANGE;
    }

    char **new_history = (char **)AGENTRT_REALLOC(
        step->correction_history, (step->correction_history_count + 1) * sizeof(char *));
    if (!new_history && step->correction_history_count > 0) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_correct: correction_history REALLOC failed (step_id=%u count=%zu)", step->step_id, step->correction_history_count);
        return AGENTRT_ENOMEM;
    }
    step->correction_history = new_history;

    if (step->content) {
        step->correction_history[step->correction_history_count++] = step->content;
    }

    step->content = (char *)AGENTRT_MALLOC(corrected_len + 1);
    if (!step->content) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_correct: content allocation failed (step_id=%u corrected_len=%zu)", step->step_id, corrected_len);
        return AGENTRT_ENOMEM;
    }
    __builtin_memcpy(step->content, corrected_content, corrected_len);
    step->content[corrected_len] = '\0';
    step->content_len = corrected_len;
    step->correction_count++;
    step->status = TC_STATUS_CORRECTED;

    if (step->chain_ref && step->chain_ref->ctx_window) {
        step->chain_ref->ctx_window->total_corrections++;
    }

    return AGENTRT_SUCCESS;
}

int agentrt_tc_step_is_ready(const agentrt_thinking_step_t *step,
                             const agentrt_thinking_chain_t *chain)
{

    if (!step || !chain) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_is_ready: NULL params (step=%p chain=%p)", (void *)step, (void *)chain);
        return AGENTRT_EINVAL;
    }

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
        if (!found_completed)
            return 0;
    }
    return 1;
}

/* ============================================================================
 * Thinking Chain 编排实现
 * ============================================================================ */

agentrt_error_t agentrt_tc_chain_create(const char *goal, size_t max_tokens, size_t wm_capacity,
                                        agentrt_thinking_chain_t **out_chain)
{

    if (!out_chain) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_create: NULL out_chain parameter");
        return AGENTRT_EINVAL;
    }

    agentrt_thinking_chain_t *chain =
        (agentrt_thinking_chain_t *)AGENTRT_CALLOC(1, sizeof(agentrt_thinking_chain_t));
    if (!chain) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_create: chain allocation failed (max_tokens=%zu wm_capacity=%zu)", max_tokens, wm_capacity);
        return AGENTRT_ENOMEM;
    }

    chain->session_id = tc_time_now_ns();
    chain->session_goal = goal ? AGENTRT_STRDUP(goal) : AGENTRT_STRDUP("");
    chain->active = 0;
    chain->next_step_id = 0;
    chain->created_ns = tc_time_now_ns();
    chain->last_activity_ns = chain->created_ns;
    chain->on_step_completed = NULL;
    chain->on_correction = NULL;
    chain->callback_user_data = NULL;

    agentrt_error_t err = agentrt_tc_context_window_create(max_tokens, &chain->ctx_window);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_create: context_window creation failed (err=%d max_tokens=%zu)", (int)err, max_tokens);
        if (chain->session_goal)
            AGENTRT_FREE(chain->session_goal);
        AGENTRT_FREE(chain);
        return err;
    }

    err = agentrt_tc_working_memory_create(wm_capacity, &chain->working_mem);
    if (err != AGENTRT_SUCCESS) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_create: working_memory creation failed (err=%d wm_capacity=%zu)", (int)err, wm_capacity);
        agentrt_tc_context_window_destroy(chain->ctx_window);
        if (chain->session_goal)
            AGENTRT_FREE(chain->session_goal);
        AGENTRT_FREE(chain);
        return err;
    }

    chain->step_capacity = 32;
    chain->steps = (agentrt_thinking_step_t *)AGENTRT_CALLOC(chain->step_capacity,
                                                             sizeof(agentrt_thinking_step_t));
    if (!chain->steps) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_create: steps allocation failed (step_capacity=%zu)", chain->step_capacity);
        agentrt_tc_working_memory_destroy(chain->working_mem);
        agentrt_tc_context_window_destroy(chain->ctx_window);
        if (chain->session_goal)
            AGENTRT_FREE(chain->session_goal);
        AGENTRT_FREE(chain);
        return AGENTRT_ENOMEM;
    }
    chain->step_count = 0;

    *out_chain = chain;
    return AGENTRT_SUCCESS;
}

void agentrt_tc_chain_destroy(agentrt_thinking_chain_t *chain)
{
    if (!chain)
        return;

    for (size_t i = 0; i < chain->step_count; i++) {
        agentrt_thinking_step_t *s = &chain->steps[i];
        if (s->raw_input)
            AGENTRT_FREE(s->raw_input);
        if (s->content)
            AGENTRT_FREE(s->content);
        if (s->critique)
            AGENTRT_FREE(s->critique);
        if (s->role)
            AGENTRT_FREE(s->role);
        if (s->depends_on)
            AGENTRT_FREE(s->depends_on);
        if (s->dependents)
            AGENTRT_FREE(s->dependents);
        if (s->correction_history) {
            for (size_t c = 0; c < s->correction_history_count; c++)
                AGENTRT_FREE(s->correction_history[c]);
            AGENTRT_FREE(s->correction_history);
        }
    }
    AGENTRT_FREE(chain->steps);

    agentrt_tc_context_window_destroy(chain->ctx_window);
    agentrt_tc_working_memory_destroy(chain->working_mem);
    if (chain->session_goal)
        AGENTRT_FREE(chain->session_goal);
    AGENTRT_FREE(chain);
}

agentrt_error_t agentrt_tc_chain_start(agentrt_thinking_chain_t *chain)
{
    if (!chain) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_start: NULL chain parameter");
        return AGENTRT_EINVAL;
    }
    chain->active = 1;
    chain->last_activity_ns = tc_time_now_ns();
    return AGENTRT_SUCCESS;
}

void agentrt_tc_chain_stop(agentrt_thinking_chain_t *chain)
{
    if (!chain)
        return;
    chain->active = 0;
}

agentrt_error_t agentrt_tc_chain_next_ready_step(agentrt_thinking_chain_t *chain,
                                                 agentrt_thinking_step_t **out_step)
{

    if (!chain || !out_step) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_next_ready_step: NULL params (chain=%p out_step=%p)", (void *)chain, (void *)out_step);
        return AGENTRT_EINVAL;
    }

    for (size_t i = 0; i < chain->step_count; i++) {
        agentrt_thinking_step_t *s = &chain->steps[i];
        if (s->status == TC_STATUS_PENDING) {
            int ready = agentrt_tc_step_is_ready(s, chain);
            if (ready > 0) {
                s->status = TC_STATUS_EXECUTING;
                s->start_time_ns = tc_time_now_ns();
                s->chain_ref = chain;
                *out_step = s;
                chain->last_activity_ns = tc_time_now_ns();

                if (chain->ctx_window && s->raw_input) {
                    agentrt_tc_context_window_append(chain->ctx_window, s->raw_input,
                                                     s->raw_input_len);
                }

                return AGENTRT_SUCCESS;
            }
        }
    }

    *out_step = NULL;
    AGENTRT_LOG_WARN("agentrt_tc_chain_next_ready_step: no ready step found (step_count=%zu)", chain->step_count);
    return AGENTRT_ENOENT;
}

agentrt_error_t agentrt_tc_chain_stats(agentrt_thinking_chain_t *chain, char **out_json,
                                       size_t *out_len)
{

    if (!chain || !out_json) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_stats: NULL params (chain=%p out_json=%p)", (void *)chain, (void *)out_json);
        return AGENTRT_EINVAL;
    }

    char *cw_json = NULL;
    agentrt_tc_context_window_stats(chain->ctx_window, &cw_json);

    uint32_t pending = 0, executing = 0, completed = 0, corrected = 0, failed = 0;
    for (size_t i = 0; i < chain->step_count; i++) {
        switch (chain->steps[i].status) {
        case TC_STATUS_PENDING:
            pending++;
            break;
        case TC_STATUS_EXECUTING:
            executing++;
            break;
        case TC_STATUS_COMPLETED:
            completed++;
            break;
        case TC_STATUS_CORRECTED:
            corrected++;
            break;
        case TC_STATUS_FAILED:
            failed++;
            break;
        default:
            break;
        }
    }

    char buf[1024];
    int len = snprintf(
        buf, sizeof(buf),
        "{\"session_id\":%llu,"
        "\"goal\":\"%s\","
        "\"active\":%d,"
        "\"steps\":{\"total\":%zu,\"pending\":%u,\"executing\":%u,"
        "\"completed\":%u,\"corrected\":%u,\"failed\":%u},"
        "\"context_window\":%s,"
        "\"working_memory\":{\"capacity\":%zu,\"count\":%zu,\"hits\":%llu,\"misses\":%llu}}",
        (unsigned long long)chain->session_id, chain->session_goal ? chain->session_goal : "",
        chain->active, chain->step_count, pending, executing, completed, corrected, failed,
        cw_json ? cw_json : "{}", chain->working_mem->capacity, chain->working_mem->count,
        (unsigned long long)chain->working_mem->hits,
        (unsigned long long)chain->working_mem->misses);

    if (cw_json) {
        AGENTRT_FREE(cw_json);
        cw_json = NULL;
    }

    char *result = (char *)AGENTRT_MALLOC(len + 1);
    if (!result) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_stats: result allocation failed (len=%d)", len);
        return AGENTRT_ENOMEM;
    }
    __builtin_memcpy(result, buf, len + 1);
    *out_json = result;
    if (out_len)
        *out_len = (size_t)len;
    return AGENTRT_SUCCESS;
}

void agentrt_tc_chain_set_step_callback(
    agentrt_thinking_chain_t *chain, void (*on_step_completed)(agentrt_thinking_step_t *, void *),
    void (*on_correction)(agentrt_thinking_step_t *, const char *, void *), void *user_data)
{

    if (!chain)
        return;
    chain->on_step_completed = on_step_completed;
    chain->on_correction = on_correction;
    chain->callback_user_data = user_data;
}

/* ============================================================================
 * P2-B03: MemoryRovol 集成 - 7个连接点
 * ============================================================================ */

#include "memory.h"
#include "metacognition.h"

void agentrt_tc_chain_set_memory(agentrt_thinking_chain_t *chain, agentrt_memory_engine_t *memory)
{
    if (!chain)
        return;
    chain->memory = memory;
}

agentrt_error_t agentrt_tc_context_window_prepopulate(agentrt_thinking_chain_t *chain,
                                                      const char *query_text, size_t query_len,
                                                      uint32_t limit)
{

    if (!chain || !query_text || !chain->memory || !chain->ctx_window) {
        AGENTRT_LOG_ERROR("agentrt_tc_context_window_prepopulate: NULL/invalid params (chain=%p query_text=%p memory=%p ctx_window=%p)", (void *)chain, (void *)query_text, (void *)(chain ? chain->memory : NULL), (void *)(chain ? chain->ctx_window : NULL));
        return AGENTRT_EINVAL;
    }

    agentrt_memory_query_t query;
    __builtin_memset(&query, 0, sizeof(query));
    query.memory_query_text = (char *)query_text;
    query.memory_query_text_len = query_len;
    query.memory_query_limit = limit > 0 ? limit : 5;
    /* P3.11-C1: 要求 query 填充 memory_result_item_record（含记录内容），
     * 否则 prepopulate 拿不到 rec->memory_record_data，记忆内容无法注入 context window。 */
    query.memory_query_include_raw = 1;

    agentrt_memory_result_ext_t *result = NULL;
    agentrt_error_t err = agentrt_memory_query(chain->memory, &query, &result);
    if (err != AGENTRT_SUCCESS || !result || result->memory_result_count == 0) {
        AGENTRT_LOG_WARN("agentrt_tc_context_window_prepopulate: memory query failed or empty (err=%d result=%p count=%zu)", (int)err, (void *)result, result ? result->memory_result_count : 0);
        if (result)
            agentrt_memory_result_free(result);
        return err == AGENTRT_SUCCESS ? AGENTRT_ENOENT : err;
    }

    for (size_t i = 0; i < result->memory_result_count; i++) {
        agentrt_memory_record_t *rec = result->memory_result_items[i]->memory_result_item_record;
        if (!rec || !rec->memory_record_data)
            continue;

        char prefix[128];
        int plen = snprintf(prefix, sizeof(prefix), "[Memory#%zu score=%.2f] ", i,
                            result->memory_result_items[i]->memory_result_item_score);

        size_t total_len = (size_t)plen + rec->memory_record_data_len + 1;
        char *buf = (char *)AGENTRT_MALLOC(total_len);
        if (!buf)
            continue;

        __builtin_memcpy(buf, prefix, (size_t)plen);
        __builtin_memcpy(buf + plen, rec->memory_record_data, rec->memory_record_data_len);
        buf[total_len - 1] = '\n';

        agentrt_tc_context_window_append(chain->ctx_window, buf, total_len);
        AGENTRT_FREE(buf);
    }

    agentrt_memory_result_free(result);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_working_memory_sync_to_persistent(agentrt_thinking_chain_t *chain,
                                                             float min_importance)
{

    if (!chain || !chain->working_mem || !chain->memory) {
        AGENTRT_LOG_ERROR("agentrt_tc_working_memory_sync_to_persistent: NULL params (chain=%p working_mem=%p memory=%p)", (void *)chain, (void *)(chain ? chain->working_mem : NULL), (void *)(chain ? chain->memory : NULL));
        return AGENTRT_EINVAL;
    }

    uint32_t synced = 0;
    for (size_t i = 0; i < chain->working_mem->count; i++) {
        struct wm_entry *e = &chain->working_mem->entries[i];
        if (e->pinned && e->value && e->value_size > 0) {
            agentrt_memory_record_t rec;
            __builtin_memset(&rec, 0, sizeof(rec));
            rec.memory_record_type = AGENTRT_MEMTYPE_TEXT;
            rec.memory_record_data = e->value;
            rec.memory_record_data_len = e->value_size;
            rec.memory_record_importance = min_importance > 0.5f ? 0.8f : 0.6f;
            rec.memory_record_source_agent = "thinking_chain_wm";
            rec.memory_record_trace_id = chain->session_goal ? chain->session_goal : "unknown";

            char *record_id = NULL;
            agentrt_error_t err = agentrt_memory_write(chain->memory, &rec, &record_id);
            if (err == AGENTRT_SUCCESS && record_id) {
                synced++;
                AGENTRT_FREE(record_id);
                record_id = NULL;
            }
        }
    }

    return (synced > 0) ? AGENTRT_SUCCESS : AGENTRT_ENOENT;
}

agentrt_error_t agentrt_tc_step_write_to_memory(agentrt_thinking_chain_t *chain,
                                                agentrt_thinking_step_t *step)
{

    if (!chain || !step || !chain->memory) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_write_to_memory: NULL params (chain=%p step=%p memory=%p)", (void *)chain, (void *)step, (void *)(chain ? chain->memory : NULL));
        return AGENTRT_EINVAL;
    }
    if (!step->content || step->content_len == 0) {
        AGENTRT_LOG_WARN("agentrt_tc_step_write_to_memory: step has no content (step_id=%u)", step->step_id);
        return AGENTRT_EINVAL;
    }
    if (step->confidence < 0.6f) {
        AGENTRT_LOG_WARN("agentrt_tc_step_write_to_memory: step confidence too low (step_id=%u confidence=%.2f)", step->step_id, step->confidence);
        return AGENTRT_EINVAL;
    }

    agentrt_memory_record_t rec;
    __builtin_memset(&rec, 0, sizeof(rec));
    rec.memory_record_type = AGENTRT_MEMTYPE_TEXT;
    rec.memory_record_data = step->content;
    rec.memory_record_data_len = step->content_len;
    rec.memory_record_importance = step->confidence;
    rec.memory_record_source_agent = step->role ? step->role : "t2-generator";
    rec.memory_record_access_count = (uint32_t)(step->correction_count + 1);

    char trace_id[64];
    snprintf(trace_id, sizeof(trace_id), "step_%u", step->step_id);
    rec.memory_record_trace_id = trace_id;

    char *record_id = NULL;
    agentrt_error_t err = agentrt_memory_write(chain->memory, &rec, &record_id);
    if (err == AGENTRT_SUCCESS && record_id && chain->memory) {
        agentrt_memory_mount(chain->memory, record_id,
                             chain->session_goal ? chain->session_goal : "");
        AGENTRT_FREE(record_id);
    }

    return err;
}

agentrt_error_t agentrt_tc_metacognition_inform_memory(agentrt_thinking_chain_t *chain,
                                                       const void *eval,
                                                       agentrt_thinking_step_t *step)
{

    if (!chain || !eval || !step || !chain->memory) {
        AGENTRT_LOG_ERROR("agentrt_tc_metacognition_inform_memory: NULL params (chain=%p eval=%p step=%p memory=%p)", (void *)chain, (void *)eval, (void *)step, (void *)(chain ? chain->memory : NULL));
        return AGENTRT_EINVAL;
    }
    const mc_evaluation_result_t *eval_typed = (const mc_evaluation_result_t *)eval;
    if (eval_typed->strategy == MC_CORRECT_NONE)
        return AGENTRT_SUCCESS;

    float importance = eval_typed->overall_score;
    if (importance > 0.9f)
        importance = 0.95f;
    else if (importance > 0.7f)
        importance = 0.8f;
    else
        importance = 0.5f;

    if (eval_typed->critique_text && eval_typed->critique_len > 0) {
        agentrt_memory_record_t rec;
        __builtin_memset(&rec, 0, sizeof(rec));
        rec.memory_record_type = AGENTRT_MEMTYPE_TEXT;
        rec.memory_record_data = (void *)eval_typed->critique_text;
        rec.memory_record_data_len = eval_typed->critique_len;
        rec.memory_record_importance = importance;
        rec.memory_record_source_agent = "s1-metacognition";
        rec.memory_record_trace_id = chain->session_goal ? chain->session_goal : "";

        char *record_id = NULL;
        agentrt_error_t err = agentrt_memory_write(chain->memory, &rec, &record_id);
        if (err == AGENTRT_SUCCESS && record_id) {
            AGENTRT_FREE(record_id);
            record_id = NULL;
        }
    }

    if (chain->working_mem && step->confidence >= 0.7f) {
        char key[64];
        snprintf(key, sizeof(key), "eval_%u", step->step_id);
        char val_buf[32];
        int vlen = snprintf(val_buf, sizeof(val_buf), "%.3f", eval_typed->overall_score);
        agentrt_tc_working_memory_store(chain->working_mem, key, val_buf, (size_t)vlen + 1,
                                        "evaluation_score", 1);
    }

    return AGENTRT_SUCCESS;
}

/* ============================================================================
 * DS-007: 执行监控实现
 * ============================================================================ */

static float compute_repetition_score(const char *content, size_t len)
{
    if (!content || len < 20)
        return 0.0f;

    size_t window = len / 2;
    if (window < 10)
        window = 10;
    if (window > len)
        window = len;

    int matching_bigrams = 0;
    int total_bigrams = 0;

    for (size_t i = 1; i + window <= len; i++) {
        total_bigrams++;
        if (i + window * 2 <= len && memcmp(content + i, content + i + window, window) == 0) {
            matching_bigrams++;
        }
    }

    if (total_bigrams == 0)
        return 0.0f;
    return (float)matching_bigrams / (float)total_bigrams;
}

agentrt_error_t agentrt_tc_step_monitor(const agentrt_thinking_step_t *step,
                                        const tc_monitor_config_t *config,
                                        tc_monitor_result_t *out_result)
{
    if (!step || !out_result) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_monitor: NULL params (step=%p out_result=%p)", (void *)step, (void *)out_result);
        return AGENTRT_EINVAL;
    }

    tc_monitor_config_t defaults = TC_MONITOR_DEFAULTS;
    if (!config)
        config = &defaults;

    __builtin_memset(out_result, 0, sizeof(tc_monitor_result_t));
    out_result->anomaly = TC_ANOMALY_NONE;
    out_result->is_critical = 0;
    out_result->severity_score = 0.0f;

    /* 检查1: 超时检测 */
    if (step->status == TC_STATUS_EXECUTING && step->start_time_ns > 0) {
        uint64_t elapsed_ms = (tc_time_now_ns() - step->start_time_ns) / 1000000ULL;
        if (elapsed_ms > config->default_timeout_ms) {
            AGENTRT_LOG_ERROR("agentrt_tc_step_monitor: timeout detected (step_id=%u elapsed=%llums limit=%ums)", step->step_id, (unsigned long long)elapsed_ms, config->default_timeout_ms);
            out_result->anomaly = TC_ANOMALY_TIMEOUT;
            out_result->is_critical = 1;
            out_result->severity_score = 0.95f;
            char desc[128];
            int dlen =
                snprintf(desc, sizeof(desc), "Step#%u timed out: %llums > %ums limit",
                         step->step_id, (unsigned long long)elapsed_ms, config->default_timeout_ms);
            out_result->description = (char *)AGENTRT_MALLOC(dlen + 1);
            if (out_result->description) {
                __builtin_memcpy(out_result->description, desc, dlen + 1);
                out_result->description_len = (size_t)dlen;
            }
            return AGENTRT_SUCCESS;
        }
    }

    /* 检查2: 空输出 */
    if (step->content_len == 0 || !step->content) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_monitor: empty output detected (step_id=%u type=%d)", step->step_id, (int)step->type);
        out_result->anomaly = TC_ANOMALY_EMPTY_OUTPUT;
        out_result->severity_score = 0.8f;
        out_result->is_critical = 1;
        const char *desc = "Empty output detected";
        out_result->description = AGENTRT_STRDUP(desc);
        out_result->description_len = strlen(desc);
        return AGENTRT_SUCCESS;
    }

    /* 检查3: 截断输出（过短） */
    if (step->content_len < config->min_output_chars && step->type != TC_STEP_VERIFICATION) {
        out_result->anomaly = TC_ANOMALY_TRUNCATED_OUTPUT;
        out_result->severity_score = 0.5f;
        char desc[96];
        int dlen = snprintf(desc, sizeof(desc), "Output too short: %zu chars < %zu minimum",
                            step->content_len, config->min_output_chars);
        out_result->description = (char *)AGENTRT_MALLOC(dlen + 1);
        if (out_result->description) {
            __builtin_memcpy(out_result->description, desc, dlen + 1);
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
            int dlen = snprintf(desc, sizeof(desc), "Output excessive: %zu chars > %zu maximum",
                                step->content_len, config->max_output_chars);
            out_result->description = (char *)AGENTRT_MALLOC(dlen + 1);
            if (out_result->description) {
                __builtin_memcpy(out_result->description, desc, dlen + 1);
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
            int dlen =
                snprintf(desc, sizeof(desc), "Repetitive content detected (score=%.2f)", rep_score);
            out_result->description = (char *)AGENTRT_MALLOC(dlen + 1);
            if (out_result->description) {
                __builtin_memcpy(out_result->description, desc, dlen + 1);
                out_result->description_len = (size_t)dlen;
            }
        }
    }

    /* 检查6: 质量门禁（置信度+状态综合判断） */
    if (config->enable_quality_gate) {
        int quality_ok = (step->confidence >= config->quality_gate_threshold) ||
                         (step->status == TC_STATUS_COMPLETED && step->correction_count == 0);

        if (!quality_ok && step->confidence < config->quality_gate_threshold) {
            out_result->anomaly = TC_ANOMALY_CONFIDENCE_DROP;
            float prev_sev = out_result->severity_score;
            out_result->severity_score = (prev_sev > 0.3f) ? prev_sev : 0.35f;
            out_result->is_critical = (step->confidence < 0.15f) ? 1 : 0;
            if (!out_result->description) {
                char desc[96];
                int dlen = snprintf(desc, sizeof(desc), "Low confidence %.2f below threshold %.2f",
                                    step->confidence, config->quality_gate_threshold);
                out_result->description = (char *)AGENTRT_MALLOC(dlen + 1);
                if (out_result->description) {
                    __builtin_memcpy(out_result->description, desc, dlen + 1);
                    out_result->description_len = (size_t)dlen;
                }
            }
        }
    }

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_chain_health_check(const agentrt_thinking_chain_t *chain,
                                              size_t *out_anomaly_count, int *out_has_critical)
{
    if (!chain || !out_anomaly_count || !out_has_critical) {
        AGENTRT_LOG_ERROR("agentrt_tc_chain_health_check: NULL params (chain=%p out_anomaly_count=%p out_has_critical=%p)", (void *)chain, (void *)out_anomaly_count, (void *)out_has_critical);
        return AGENTRT_EINVAL;
    }

    *out_anomaly_count = 0;
    *out_has_critical = 0;

    tc_monitor_config_t defaults = TC_MONITOR_DEFAULTS;

    for (size_t i = 0; i < chain->step_count; i++) {
        if (chain->steps[i].status == TC_STATUS_PENDING)
            continue;

        tc_monitor_result_t mon;
        agentrt_error_t err = agentrt_tc_step_monitor(&chain->steps[i], &defaults, &mon);
        if (err == AGENTRT_SUCCESS && mon.anomaly != TC_ANOMALY_NONE) {
            (*out_anomaly_count)++;
            if (mon.is_critical)
                *out_has_critical = 1;
            if (mon.description)
                AGENTRT_FREE(mon.description);
        }
    }

    return AGENTRT_SUCCESS;
}

/* ============================================================================
 * DS-008: 异常恢复实现
 * ============================================================================ */

#define TC_MAX_RECOVERY_ATTEMPTS 3
#define TC_RETRY_BACKOFF_BASE_MS 500
#define TC_RETRY_BACKOFF_MAX_MS 8000

/* 内部检查点结构（存储在chain中） */
typedef struct tc_checkpoint {
    uint32_t checkpoint_id;
    size_t step_snapshot_count; /**< 快照时的步骤数 */
    uint64_t timestamp_ns;
} tc_checkpoint_t;

#define TC_MAX_CHECKPOINTS 16

static tc_checkpoint_t *get_checkpoint_storage(agentrt_thinking_chain_t *chain)
{
    static tc_checkpoint_t s_checkpoints[TC_MAX_CHECKPOINTS] = {{0}};
    return s_checkpoints;
}

agentrt_error_t agentrt_tc_step_recover(agentrt_thinking_chain_t *chain,
                                        agentrt_thinking_step_t *failed_step,
                                        const tc_monitor_result_t *monitor_result,
                                        agentrt_error_t (*corrector_fn)(const char *, size_t,
                                                                        char **, size_t *, void *),
                                        void *user_data, tc_recovery_result_t *out_result)
{
    if (!chain || !failed_step || !out_result) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_recover: NULL params (chain=%p failed_step=%p out_result=%p)", (void *)chain, (void *)failed_step, (void *)out_result);
        return AGENTRT_EINVAL;
    }

    __builtin_memset(out_result, 0, sizeof(tc_recovery_result_t));
    out_result->strategy_used = TC_RECOVER_ABORT;
    out_result->success = 0;

    tc_monitor_result_t local_mon;
    if (!monitor_result) {
        agentrt_tc_step_monitor(failed_step, NULL, &local_mon);
        monitor_result = &local_mon;
    }

    size_t log_buf_size = 512;
    char *log_buf = (char *)AGENTRT_MALLOC(log_buf_size);
    if (!log_buf) {
        AGENTRT_LOG_ERROR("agentrt_tc_step_recover: log buffer allocation failed (size=%zu)", log_buf_size);
        return AGENTRT_ENOMEM;
    }
    int log_pos = 0;

    log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                        "Recovery for step#%u (anomaly=%d): ", failed_step->step_id,
                        monitor_result->anomaly);

    /* 策略1: 重试（最多TC_MAX_RECOVERY_ATTEMPTS次，指数退避） */
    if (corrector_fn && failed_step->raw_input && failed_step->raw_input_len > 0) {
        for (uint32_t attempt = 0; attempt < TC_MAX_RECOVERY_ATTEMPTS; attempt++) {
            out_result->attempts_made++;

            char *new_content = NULL;
            size_t new_len = 0;
            agentrt_error_t err = corrector_fn(failed_step->raw_input, failed_step->raw_input_len,
                                               &new_content, &new_len, user_data);

            if (err == AGENTRT_SUCCESS && new_content && new_len > 0) {
                agentrt_tc_step_correct(failed_step, new_content, new_len);
                AGENTRT_FREE(new_content);
                new_content = NULL;

                /* 验证修正后是否通过质量门禁 */
                tc_monitor_result_t post_mon;
                agentrt_tc_step_monitor(failed_step, NULL, &post_mon);
                if (post_mon.anomaly == TC_ANOMALY_NONE || post_mon.severity_score < 0.4f) {
                    out_result->strategy_used = TC_RECOVER_RETRY;
                    out_result->success = 1;
                    log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                                        "retry#%u succeeded", attempt + 1);
                    if (post_mon.description)
                        AGENTRT_FREE(post_mon.description);
                    goto recovery_done;
                }
                if (post_mon.description)
                    AGENTRT_FREE(post_mon.description);
            }

            /* 指数退避等待 */
            unsigned backoff = TC_RETRY_BACKOFF_BASE_MS << attempt;
            if (backoff > TC_RETRY_BACKOFF_MAX_MS)
                backoff = TC_RETRY_BACKOFF_MAX_MS;
            struct timespec ts = {.tv_sec = (time_t)(backoff / 1000),
                                  .tv_nsec = (long)((backoff % 1000) * 1000000L)};
            nanosleep(&ts, NULL);
        }
        log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos, ", %u retries exhausted",
                            out_result->attempts_made);
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
    tc_checkpoint_t *checkpoints = get_checkpoint_storage(chain);
    uint32_t best_cp = 0;
    for (int c = TC_MAX_CHECKPOINTS - 1; c >= 0; c--) {
        if (checkpoints[c].checkpoint_id > 0 &&
            checkpoints[c].step_snapshot_count < chain->step_count) {
            best_cp = checkpoints[c].checkpoint_id;
            break;
        }
    }

    if (best_cp > 0) {
        size_t removed = agentrt_tc_chain_rollback(chain, best_cp);
        out_result->strategy_used = TC_RECOVER_ROLLBACK;
        out_result->success = (removed > 0) ? 1 : 0;
        log_pos += snprintf(log_buf + log_pos, log_buf_size - log_pos,
                            ", rolled back to cp#%u (%zu steps removed)", best_cp, removed);
        goto recovery_done;
    }

    /* 所有策略失败 → 中止 */
    log_pos +=
        snprintf(log_buf + log_pos, log_buf_size - log_pos, ", all strategies failed -> ABORT");
    failed_step->status = TC_STATUS_FAILED;

recovery_done:
    out_result->recovery_log = log_buf;
    out_result->recovery_log_len = (size_t)log_pos;

    AGENTRT_LOG_INFO("TC Recovery: step#%u strategy=%d success=%d attempts=%u",
                     failed_step->step_id, out_result->strategy_used, out_result->success,
                     out_result->attempts_made);
    return AGENTRT_SUCCESS;
}

uint32_t agentrt_tc_chain_checkpoint(agentrt_thinking_chain_t *chain)
{
    if (!chain)
        return 0;

    tc_checkpoint_t *checkpoints = get_checkpoint_storage(chain);

    uint32_t next_id = 0;
    for (int c = 0; c < TC_MAX_CHECKPOINTS; c++) {
        if (checkpoints[c].checkpoint_id >= next_id)
            next_id = checkpoints[c].checkpoint_id + 1;
    }
    if (next_id == 0)
        next_id = 1;

    int slot = -1;
    for (int c = 0; c < TC_MAX_CHECKPOINTS; c++) {
        if (checkpoints[c].checkpoint_id == 0) {
            slot = c;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
        __builtin_memmove(&checkpoints[0], &checkpoints[1],
                (TC_MAX_CHECKPOINTS - 1) * sizeof(tc_checkpoint_t));
        checkpoints[TC_MAX_CHECKPOINTS - 1].checkpoint_id = 0;
    }

    checkpoints[slot].checkpoint_id = next_id;
    checkpoints[slot].step_snapshot_count = chain->step_count;
    checkpoints[slot].timestamp_ns = tc_time_now_ns();

    AGENTRT_LOG_INFO("TC Checkpoint#%u created at step_count=%zu", next_id, chain->step_count);
    return next_id;
}

size_t agentrt_tc_chain_rollback(agentrt_thinking_chain_t *chain, uint32_t checkpoint_id)
{
    if (!chain || checkpoint_id == 0)
        return 0;

    tc_checkpoint_t *checkpoints = get_checkpoint_storage(chain);
    size_t target_steps = 0;

    for (int c = 0; c < TC_MAX_CHECKPOINTS; c++) {
        if (checkpoints[c].checkpoint_id == checkpoint_id) {
            target_steps = checkpoints[c].step_snapshot_count;
            break;
        }
    }

    if (target_steps == 0 || target_steps >= chain->step_count)
        return 0;

    size_t removed = chain->step_count - target_steps;

    for (size_t i = target_steps; i < chain->step_count; i++) {
        agentrt_thinking_step_t *step = &chain->steps[i];
        if (step->content)
            AGENTRT_FREE(step->content);
        if (step->raw_input)
            AGENTRT_FREE(step->raw_input);
        if (step->critique)
            AGENTRT_FREE(step->critique);
        if (step->role)
            AGENTRT_FREE(step->role);
        if (step->depends_on) {
            AGENTRT_FREE(step->depends_on);
        }
        if (step->dependents)
            AGENTRT_FREE(step->dependents);
        if (step->correction_history) {
            for (size_t h = 0; h < step->correction_history_count; h++)
                AGENTRT_FREE(step->correction_history[h]);
            AGENTRT_FREE(step->correction_history);
        }
        __builtin_memset(step, 0, sizeof(agentrt_thinking_step_t));
    }

    chain->step_count = target_steps;
    chain->next_step_id = (uint32_t)target_steps + 1;

    AGENTRT_LOG_INFO("TC Rollback to cp#%u: removed %zu steps (now %zu)", checkpoint_id, removed,
                     chain->step_count);
    return removed;
}

static float get_step_weight(tc_step_type_t type, const tc_attention_weights_t *w)
{
    switch (type) {
    case TC_STEP_DECOMPOSITION:
        return w->decomposition_weight;
    case TC_STEP_PLANNING:
        return w->planning_weight;
    case TC_STEP_GENERATION:
        return w->generation_weight;
    case TC_STEP_VERIFICATION:
        return w->verification_weight;
    case TC_STEP_AUDIT:
        return w->audit_weight;
    case TC_STEP_ALIGNMENT:
        return w->alignment_weight;
    default:
        return 0.10f;
    }
}

agentrt_error_t agentrt_tc_set_attention_config(agentrt_thinking_chain_t *chain,
                                                const tc_attention_config_t *config)
{
    if (!chain || !config) {
        AGENTRT_LOG_ERROR("agentrt_tc_set_attention_config: NULL params (chain=%p config=%p)", (void *)chain, (void *)config);
        return AGENTRT_EINVAL;
    }
    chain->attention_config = *config;
    chain->attention_configured = 1;
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_allocate_attention(agentrt_thinking_chain_t *chain,
                                              agentrt_thinking_step_t *step,
                                              tc_allocation_result_t *out_alloc)
{
    if (!chain || !step || !out_alloc) {
        AGENTRT_LOG_ERROR("agentrt_tc_allocate_attention: NULL params (chain=%p step=%p out_alloc=%p)", (void *)chain, (void *)step, (void *)out_alloc);
        return AGENTRT_EINVAL;
    }

    tc_attention_config_t cfg = chain->attention_configured
                                    ? chain->attention_config
                                    : (tc_attention_config_t)TC_ATTENTION_DEFAULTS;

    float weight = get_step_weight(step->type, &cfg.weights);
    size_t base_alloc = (size_t)((float)cfg.base_tokens * weight);

    float priority = agentrt_tc_compute_priority(step, chain);

    float urgency = 0.0f;
    if (step->status == TC_STATUS_PENDING && step->dependents_count > 0)
        urgency = 0.3f + 0.7f * ((float)step->dependents_count / 10.0f);
    if (urgency > 1.0f)
        urgency = 1.0f;

    float dynamic_factor = 1.0f + (priority * 0.5f) + (urgency * 0.3f);
    size_t final_alloc = (size_t)((float)base_alloc * dynamic_factor);

    if (final_alloc < cfg.min_step_tokens)
        final_alloc = cfg.min_step_tokens;
    if (final_alloc > cfg.max_step_tokens)
        final_alloc = cfg.max_step_tokens;

    size_t avail = 0;
    if (chain->ctx_window && chain->ctx_window->max_tokens > chain->ctx_window->used_tokens)
        avail = chain->ctx_window->max_tokens - chain->ctx_window->used_tokens;
    if (avail > 0 && final_alloc > avail)
        final_alloc = avail;

    out_alloc->allocated_tokens = final_alloc;
    out_alloc->priority_score = priority;
    out_alloc->urgency_score = urgency;
    out_alloc->is_elevated = (priority > 0.8f || urgency > 0.7f) ? 1 : 0;

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_tc_adjust_dynamic_budget(agentrt_thinking_chain_t *chain,
                                                 tc_step_type_t step_type, float performance_score)
{
    if (!chain || performance_score < 0.0f || performance_score > 1.0f) {
        AGENTRT_LOG_ERROR("agentrt_tc_adjust_dynamic_budget: invalid params (chain=%p performance_score=%.2f)", (void *)chain, performance_score);
        return AGENTRT_EINVAL;
    }
    if (!chain->attention_config.enable_dynamic_adjustment)
        return AGENTRT_SUCCESS;

    float *target_weight = NULL;
    tc_attention_weights_t *w = &chain->attention_config.weights;
    switch (step_type) {
    case TC_STEP_DECOMPOSITION:
        target_weight = &w->decomposition_weight;
        break;
    case TC_STEP_PLANNING:
        target_weight = &w->planning_weight;
        break;
    case TC_STEP_GENERATION:
        target_weight = &w->generation_weight;
        break;
    case TC_STEP_VERIFICATION:
        target_weight = &w->verification_weight;
        break;
    case TC_STEP_AUDIT:
        target_weight = &w->audit_weight;
        break;
    case TC_STEP_ALIGNMENT:
        target_weight = &w->alignment_weight;
        break;
    default:
        AGENTRT_LOG_ERROR("agentrt_tc_adjust_dynamic_budget: unknown step type (step_type=%d)", (int)step_type);
        return AGENTRT_EINVAL;
    }

    float adjustment = (performance_score - 0.6f) * 0.05f;
    float new_weight = *target_weight + adjustment;
    if (new_weight < 0.03f)
        new_weight = 0.03f;
    if (new_weight > 0.50f)
        new_weight = 0.50f;

    float total = w->decomposition_weight + w->planning_weight + w->generation_weight +
                  w->verification_weight + w->audit_weight + w->alignment_weight;
    total += (new_weight - *target_weight);
    if (total > 0.01f) {
        float scale = 1.0f / total;
        w->decomposition_weight *= scale;
        w->planning_weight *= scale;
        w->generation_weight *= scale;
        w->verification_weight *= scale;
        w->audit_weight *= scale;
        w->alignment_weight *= scale;
        *target_weight = new_weight * scale;
    }

    return AGENTRT_SUCCESS;
}

float agentrt_tc_compute_priority(const agentrt_thinking_step_t *step,
                                  const agentrt_thinking_chain_t *chain)
{
    if (!step)
        return 0.0f;

    float dep_factor = (step->dependents_count > 0)
                           ? 0.2f + 0.4f * fminf((float)step->dependents_count / 5.0f, 1.0f)
                           : 0.1f;

    float type_factor = 0.15f;
    switch (step->type) {
    case TC_STEP_DECOMPOSITION:
        type_factor = 0.70f;
        break;
    case TC_STEP_PLANNING:
        type_factor = 0.65f;
        break;
    case TC_STEP_GENERATION:
        type_factor = 0.80f;
        break;
    case TC_STEP_VERIFICATION:
        type_factor = 0.55f;
        break;
    case TC_STEP_AUDIT:
        type_factor = 0.50f;
        break;
    case TC_STEP_ALIGNMENT:
        type_factor = 0.45f;
        break;
    case TC_STEP_CORRECTION:
        type_factor = 0.60f;
        break;
    }

    float conf_factor = (step->confidence > 0.0f) ? 1.0f - step->confidence : 0.5f;

    float status_factor = 0.3f;
    switch (step->status) {
    case TC_STATUS_PENDING:
        status_factor = 0.90f;
        break;
    case TC_STATUS_EXECUTING:
        status_factor = 0.95f;
        break;
    case TC_STATUS_FAILED:
        status_factor = 1.00f;
        break;
    default:
        status_factor = 0.20f;
        break;
    }

    float priority =
        dep_factor * 0.25f + type_factor * 0.35f + conf_factor * 0.20f + status_factor * 0.20f;
    if (priority > 1.0f)
        priority = 1.0f;
    if (priority < 0.0f)
        priority = 0.0f;
    return priority;
}

agentrt_error_t agentrt_tc_wm_set_priority(agentrt_working_memory_t *wm, const char *key,
                                           float priority)
{
    if (!wm || !key) {
        AGENTRT_LOG_ERROR("agentrt_tc_wm_set_priority: NULL params (wm=%p key=%p)", (void *)wm, (void *)key);
        return AGENTRT_EINVAL;
    }
    if (priority < 0.0f)
        priority = 0.0f;
    if (priority > 1.0f)
        priority = 1.0f;

    for (size_t i = 0; i < wm->count; i++) {
        if (strncmp(wm->entries[i].key, key, 255) == 0) {
            if (priority > 0.7f)
                wm->entries[i].pinned = 1;
            wm_update_lru(wm, i);
            return AGENTRT_SUCCESS;
        }
    }
    AGENTRT_LOG_WARN("agentrt_tc_wm_set_priority: key not found (key=%s)", key);
    return AGENTRT_ENOENT;
}
