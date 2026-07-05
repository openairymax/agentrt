#include "delegate.h"

#include "agentrt.h"
#include "atomic_compat.h"
#include "logging.h"
#include "memory_compat.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


static agentrt_mutex_t g_delegate_mutex;
static atomic_int g_delegate_mutex_initialized = 0;
static int g_delegate_depth = 0;
static uint64_t g_task_counter = 0;

static void ensure_mutex_init(void)
{
    int expected = 0;
    if (atomic_compare_exchange_strong_explicit(&g_delegate_mutex_initialized, &expected, 1,
                                                memory_order_seq_cst, memory_order_seq_cst)) {
        agentrt_mutex_init(&g_delegate_mutex);
    }
}

static char *delegate_strdup(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = (char *)AGENTRT_MALLOC(len + 1);
    if (dup) {
        __builtin_memcpy(dup, s, len + 1);
    }
    return dup;
}

static char **delegate_deep_copy_tools(const char **src, size_t count, size_t *out_count)
{
    if (!src || count == 0) {
        *out_count = 0;
        AGENTRT_ERROR_NULL(AGENTRT_ERR_OVERFLOW, "limit exceeded");
    }
    char **dst = (char **)AGENTRT_CALLOC(count, sizeof(char *));
    if (!dst) {
        *out_count = 0;
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    size_t copied = 0;
    for (size_t i = 0; i < count; i++) {
        if (src[i]) {
            dst[copied] = delegate_strdup(src[i]);
            if (!dst[copied]) {
                for (size_t j = 0; j < copied; j++) {
                    AGENTRT_FREE(dst[j]);
                }
                AGENTRT_FREE(dst);
                *out_count = 0;
                AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
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
        AGENTRT_FREE(tools[i]);
    }
    AGENTRT_FREE(tools);
}

agentrt_delegate_task_t *agentrt_delegate_create(const char *task_description,
                                                 const agentrt_delegate_config_t *config)
{
    if (!task_description) return NULL;

    ensure_mutex_init();
    agentrt_mutex_lock(&g_delegate_mutex);
    if (g_delegate_depth >= 2) {
        agentrt_mutex_unlock(&g_delegate_mutex);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "validation failed");
    }
    agentrt_mutex_unlock(&g_delegate_mutex);

    agentrt_delegate_task_t *task =
        (agentrt_delegate_task_t *)AGENTRT_CALLOC(1, sizeof(agentrt_delegate_task_t));
    if (!task) return NULL;

    agentrt_mutex_lock(&g_delegate_mutex);
    uint64_t my_id = g_task_counter++;
    agentrt_mutex_unlock(&g_delegate_mutex);

    task->task_id = (char *)AGENTRT_MALLOC(64);
    if (!task->task_id) {
        AGENTRT_FREE(task);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }
    snprintf(task->task_id, 64, "delegate_%lu", (unsigned long)my_id);

    task->description = delegate_strdup(task_description);
    if (!task->description) {
        AGENTRT_FREE(task->task_id);
        AGENTRT_FREE(task);
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
    }

    task->status = AGENTRT_SUCCESS;
    task->state = AGENTRT_DELEGATE_IDLE;
    task->result = NULL;
    task->result_len = 0;
    task->owned_tools = NULL;
    task->owned_tool_count = 0;

    if (config) {
        task->config.focus_prompt = delegate_strdup(config->focus_prompt);
        if (config->focus_prompt && !task->config.focus_prompt) {
            AGENTRT_FREE(task->task_id);
            AGENTRT_FREE(task->description);
            AGENTRT_FREE(task);
            AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
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
        __builtin_memset(&task->config, 0, sizeof(agentrt_delegate_config_t));
        task->config.max_depth = 2;
        task->config.max_iterations = 10;
        task->config.token_budget_ratio = 0.3f;
    }

    return task;
}

void agentrt_delegate_destroy(agentrt_delegate_task_t *task)
{
    if (!task)
        return;
    if (task->task_id)
        AGENTRT_FREE(task->task_id);
    if (task->description)
        AGENTRT_FREE(task->description);
    if (task->result)
        AGENTRT_FREE(task->result);
    if (task->config.focus_prompt)
        AGENTRT_FREE((void *)task->config.focus_prompt);
    delegate_free_tools(task->owned_tools, task->owned_tool_count);
    AGENTRT_FREE(task);
}

agentrt_error_t agentrt_delegate_assign(agentrt_delegate_task_t *task,
                                        agentrt_tool_execute_fn executor, void *user_data)
{
    if (!task || !executor)
        ATM_RET_ERR(AGENTRT_EINVAL);

    ensure_mutex_init();
    agentrt_mutex_lock(&g_delegate_mutex);
    if (task->state != AGENTRT_DELEGATE_IDLE) {
        agentrt_mutex_unlock(&g_delegate_mutex);
        ATM_RET_ERR(AGENTRT_EBUSY);
    }
    if (g_delegate_depth >= task->config.max_depth) {
        agentrt_mutex_unlock(&g_delegate_mutex);
        ATM_RET_ERR(AGENTRT_EPERM);
    }

    task->state = AGENTRT_DELEGATE_RUNNING;
    g_delegate_depth++;
    agentrt_mutex_unlock(&g_delegate_mutex);

    char *output = NULL;
    size_t output_len = 0;
    agentrt_error_t err = executor("delegate_task", task->description,
                                   task->description ? strlen(task->description) : 0, &output,
                                   &output_len, user_data);

    agentrt_mutex_lock(&g_delegate_mutex);
    g_delegate_depth--;
    agentrt_mutex_unlock(&g_delegate_mutex);

    task->result = output;
    task->result_len = output_len;
    task->status = err;

    if (task->state == AGENTRT_DELEGATE_CANCELLED) {
        if (output) {
            AGENTRT_FREE(output);
            task->result = NULL;
            task->result_len = 0;
        }
    } else if (err == AGENTRT_SUCCESS) {
        task->state = AGENTRT_DELEGATE_COMPLETED;
    } else {
        task->state = AGENTRT_DELEGATE_FAILED;
    }

    return err;
}

agentrt_error_t agentrt_delegate_collect(agentrt_delegate_task_t *task, char **out_result,
                                         size_t *out_result_len)
{
    if (!task || !out_result)
        ATM_RET_ERR(AGENTRT_EINVAL);

    ensure_mutex_init();
    agentrt_mutex_lock(&g_delegate_mutex);
    agentrt_delegate_state_t st = task->state;
    agentrt_mutex_unlock(&g_delegate_mutex);

    if (st == AGENTRT_DELEGATE_IDLE || st == AGENTRT_DELEGATE_RUNNING) {
        ATM_RET_ERR(AGENTRT_EBUSY);
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

agentrt_error_t agentrt_delegate_cancel(agentrt_delegate_task_t *task)
{
    if (!task)
        ATM_RET_ERR(AGENTRT_EINVAL);

    ensure_mutex_init();
    agentrt_mutex_lock(&g_delegate_mutex);
    if (task->state == AGENTRT_DELEGATE_COMPLETED || task->state == AGENTRT_DELEGATE_FAILED ||
        task->state == AGENTRT_DELEGATE_CANCELLED) {
        agentrt_mutex_unlock(&g_delegate_mutex);
        ATM_RET_ERR(AGENTRT_EINVAL);
    }

    task->state = AGENTRT_DELEGATE_CANCELLED;
    task->status = AGENTRT_ECANCELLED;
    agentrt_mutex_unlock(&g_delegate_mutex);

    return AGENTRT_SUCCESS;
}

void delegate_shutdown(void)
{
    if (g_delegate_mutex_initialized) {
        agentrt_mutex_destroy(&g_delegate_mutex);
        g_delegate_mutex_initialized = 0;
        log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, "[delegate] g_delegate_mutex destroyed");
    }
}

agentrt_delegate_state_t agentrt_delegate_get_state(const agentrt_delegate_task_t *task)
{
    if (!task)
        return AGENTRT_DELEGATE_IDLE;
    return task->state;
}
