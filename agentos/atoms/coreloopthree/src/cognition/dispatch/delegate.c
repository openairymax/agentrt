#include "delegate.h"

#include "agentos.h"
#include "atomic_compat.h"
#include "memory_compat.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)


static agentos_mutex_t g_delegate_mutex;
static atomic_int g_delegate_mutex_initialized = 0;
static int g_delegate_depth = 0;
static uint64_t g_task_counter = 0;

static void ensure_mutex_init(void)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&g_delegate_mutex_initialized, &expected, 1,
                                                memory_order_seq_cst, memory_order_seq_cst)) {
        agentos_mutex_init(&g_delegate_mutex);
    }
}

static char *delegate_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = (char *)AGENTOS_MALLOC(len + 1);
    if (dup) {
        __builtin_memcpy(dup, s, len + 1);
    }
    return dup;
}

static char **delegate_deep_copy_tools(const char **src, size_t count, size_t *out_count)
{
    if (!src || count == 0) {
        *out_count = 0;
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_OVERFLOW, "limit exceeded");
        return NULL;
    }
    char **dst = (char **)AGENTOS_CALLOC(count, sizeof(char *));
    if (!dst) {
        *out_count = 0;
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }
    size_t copied = 0;
    for (size_t i = 0; i < count; i++) {
        if (src[i]) {
            dst[copied] = delegate_strdup(src[i]);
            if (!dst[copied]) {
                for (size_t j = 0; j < copied; j++) {
                    AGENTOS_FREE(dst[j]);
                }
                AGENTOS_FREE(dst);
                *out_count = 0;
                AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
                return NULL;
            }
            copied++;
        }
    }
    *out_count = copied;
    return dst;
}

static void delegate_free_tools(char **tools, size_t count)
{
    if (!tools)
        return;
    for (size_t i = 0; i < count; i++) {
        AGENTOS_FREE(tools[i]);
    }
    AGENTOS_FREE(tools);
}

agentos_delegate_task_t *agentos_delegate_create(const char *task_description,
                                                 const agentos_delegate_config_t *config)
{
    if (!task_description) return NULL;

    ensure_mutex_init();
    agentos_mutex_lock(&g_delegate_mutex);
    if (g_delegate_depth >= 2) {
        agentos_mutex_unlock(&g_delegate_mutex);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_UNKNOWN, "validation failed");
        return NULL;
    }
    agentos_mutex_unlock(&g_delegate_mutex);

    agentos_delegate_task_t *task =
        (agentos_delegate_task_t *)AGENTOS_CALLOC(1, sizeof(agentos_delegate_task_t));
    if (!task) return NULL;

    agentos_mutex_lock(&g_delegate_mutex);
    uint64_t my_id = g_task_counter++;
    agentos_mutex_unlock(&g_delegate_mutex);

    task->task_id = (char *)AGENTOS_MALLOC(64);
    if (!task->task_id) {
        AGENTOS_FREE(task);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }
    snprintf(task->task_id, 64, "delegate_%lu", (unsigned long)my_id);

    task->description = delegate_strdup(task_description);
    if (!task->description) {
        AGENTOS_FREE(task->task_id);
        AGENTOS_FREE(task);
        AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        return NULL;
    }

    task->status = AGENTOS_SUCCESS;
    task->state = AGENTOS_DELEGATE_IDLE;
    task->result = NULL;
    task->result_len = 0;
    task->owned_tools = NULL;
    task->owned_tool_count = 0;

    if (config) {
        task->config.focus_prompt = delegate_strdup(config->focus_prompt);
        if (config->focus_prompt && !task->config.focus_prompt) {
            AGENTOS_FREE(task->task_id);
            AGENTOS_FREE(task->description);
            AGENTOS_FREE(task);
            AGENTOS_ERROR_HANDLE(AGENTOS_ERR_INVALID_PARAM, "null parameter");
            return NULL;
        }
        task->config.allowed_tool_count = config->allowed_tool_count;
        task->config.allowed_tools = config->allowed_tools;
        task->owned_tools = delegate_deep_copy_tools(
            config->allowed_tools, config->allowed_tool_count, &task->owned_tool_count);
        task->config.max_depth = config->max_depth > 0 ? config->max_depth : 2;
        task->config.max_iterations = config->max_iterations > 0 ? config->max_iterations : 10;
        task->config.token_budget_ratio =
            config->token_budget_ratio > 0.0f ? config->token_budget_ratio : 0.3f;
    } else {
        __builtin_memset(&task->config, 0, sizeof(agentos_delegate_config_t));
        task->config.max_depth = 2;
        task->config.max_iterations = 10;
        task->config.token_budget_ratio = 0.3f;
    }

    return task;
}

void agentos_delegate_destroy(agentos_delegate_task_t *task)
{
    if (!task)
        return;
    if (task->task_id)
        AGENTOS_FREE(task->task_id);
    if (task->description)
        AGENTOS_FREE(task->description);
    if (task->result)
        AGENTOS_FREE(task->result);
    if (task->config.focus_prompt)
        AGENTOS_FREE((void *)task->config.focus_prompt);
    delegate_free_tools(task->owned_tools, task->owned_tool_count);
    AGENTOS_FREE(task);
}

agentos_error_t agentos_delegate_assign(agentos_delegate_task_t *task,
                                        agentos_tool_execute_fn executor, void *user_data)
{
    if (!task || !executor)
        ATM_RET_ERR(AGENTOS_EINVAL);

    ensure_mutex_init();
    agentos_mutex_lock(&g_delegate_mutex);
    if (task->state != AGENTOS_DELEGATE_IDLE) {
        agentos_mutex_unlock(&g_delegate_mutex);
        ATM_RET_ERR(AGENTOS_EBUSY);
    }
    if (g_delegate_depth >= task->config.max_depth) {
        agentos_mutex_unlock(&g_delegate_mutex);
        ATM_RET_ERR(AGENTOS_EPERM);
    }

    task->state = AGENTOS_DELEGATE_RUNNING;
    g_delegate_depth++;
    agentos_mutex_unlock(&g_delegate_mutex);

    char *output = NULL;
    size_t output_len = 0;
    agentos_error_t err = executor("delegate_task", task->description,
                                   task->description ? strlen(task->description) : 0, &output,
                                   &output_len, user_data);

    agentos_mutex_lock(&g_delegate_mutex);
    g_delegate_depth--;
    agentos_mutex_unlock(&g_delegate_mutex);

    task->result = output;
    task->result_len = output_len;
    task->status = err;

    if (task->state == AGENTOS_DELEGATE_CANCELLED) {
        if (output) {
            AGENTOS_FREE(output);
            task->result = NULL;
            task->result_len = 0;
        }
    } else if (err == AGENTOS_SUCCESS) {
        task->state = AGENTOS_DELEGATE_COMPLETED;
    } else {
        task->state = AGENTOS_DELEGATE_FAILED;
    }

    return err;
}

agentos_error_t agentos_delegate_collect(agentos_delegate_task_t *task, char **out_result,
                                         size_t *out_result_len)
{
    if (!task || !out_result)
        ATM_RET_ERR(AGENTOS_EINVAL);

    ensure_mutex_init();
    agentos_mutex_lock(&g_delegate_mutex);
    agentos_delegate_state_t st = task->state;
    agentos_mutex_unlock(&g_delegate_mutex);

    if (st == AGENTOS_DELEGATE_IDLE || st == AGENTOS_DELEGATE_RUNNING) {
        ATM_RET_ERR(AGENTOS_EBUSY);
    }

    if (task->result && task->result_len > 0) {
        *out_result = task->result;
        if (out_result_len)
            *out_result_len = task->result_len;
        task->result = NULL;
        task->result_len = 0;
    } else {
        *out_result = NULL;
        if (out_result_len)
            *out_result_len = 0;
    }

    return task->status;
}

agentos_error_t agentos_delegate_cancel(agentos_delegate_task_t *task)
{
    if (!task)
        ATM_RET_ERR(AGENTOS_EINVAL);

    ensure_mutex_init();
    agentos_mutex_lock(&g_delegate_mutex);
    if (task->state == AGENTOS_DELEGATE_COMPLETED || task->state == AGENTOS_DELEGATE_FAILED ||
        task->state == AGENTOS_DELEGATE_CANCELLED) {
        agentos_mutex_unlock(&g_delegate_mutex);
        ATM_RET_ERR(AGENTOS_EINVAL);
    }

    task->state = AGENTOS_DELEGATE_CANCELLED;
    task->status = AGENTOS_ECANCELLED;
    agentos_mutex_unlock(&g_delegate_mutex);

    return AGENTOS_SUCCESS;
}

void delegate_shutdown(void)
{
    if (g_delegate_mutex_initialized) {
        agentos_mutex_destroy(&g_delegate_mutex);
        g_delegate_mutex_initialized = 0;
        SVC_LOG_INFO("delegate: g_delegate_mutex destroyed");
    }
}

agentos_delegate_state_t agentos_delegate_get_state(const agentos_delegate_task_t *task)
{
    if (!task)
        return AGENTOS_DELEGATE_IDLE;
    return task->state;
}
