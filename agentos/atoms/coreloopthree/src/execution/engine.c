/**
 * @file engine.c
 * @brief 行动层执行引擎核心实?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../../include/agent_registry.h"
#include "../../include/error_utils.h"
#include "../../include/execution.h"
#include "../../include/id_utils.h"
#include "agentos.h"
#include "logging_compat.h"
#include "platform.h"

#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "memory_compat.h"
#include "string_compat.h"

#include <stdio.h>
#include <string.h>


/* JSON解析库 */
#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
#endif

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include "atomic_compat.h"
#include "platform.h"
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)

/* 执行单元注册表最大容量 */
#define AGENTOS_EXECUTION_MAX_UNITS 64


typedef struct task_control_block {
    char *task_id;
    agentos_task_t *task_desc;
    agentos_task_status_t status;
    uint64_t submit_time_ns;
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    void *result;
    size_t result_len;
    agentos_cond_t *completed_cond;
    agentos_mutex_t *tcb_lock;
    int ref_count;
    struct task_control_block *next;
    struct task_control_block *hash_next;
} task_tcb_t;

typedef struct {
    task_tcb_t **buckets;
    size_t size;
    agentos_mutex_t *lock;
} task_hash_table_t;

struct agentos_execution_engine {
    uint32_t max_concurrency;
    uint32_t current_concurrency;
    agentos_mutex_t *queue_lock;
    agentos_mutex_t *running_lock;
    task_tcb_t *task_queue;
    task_tcb_t *running_tasks;
    task_hash_table_t *task_map;
    agentos_cond_t *task_available_cond;
    agentos_thread_t *worker_threads;
    size_t worker_count;
    atomic_int running;
    /* 执行单元注册表 */
    agentos_execution_unit_t registered_units[AGENTOS_EXECUTION_MAX_UNITS];
    char *registered_unit_names[AGENTOS_EXECUTION_MAX_UNITS];
    uint32_t registered_unit_count;
    agentos_mutex_t *registry_lock;
    /* 反馈回调 */
    agentos_feedback_callback_t feedback_callback;
    void *feedback_user_data;
};

static void tcb_retain(task_tcb_t *tcb);
static void tcb_release(task_tcb_t *tcb);

/**
 * @brief 哈希函数（djb2 变体?
 */
static size_t task_hash(const char *task_id, size_t table_size)
{
    size_t hash = 5381;
    while (*task_id) {
        hash = ((hash << 5) + hash) + (unsigned char)*task_id++;
    }
    return hash % table_size;
}

/**
 * @brief 创建哈希?
 */
static task_hash_table_t *task_hash_table_create(size_t size)
{
    task_hash_table_t *table = (task_hash_table_t *)AGENTOS_CALLOC(1, sizeof(task_hash_table_t));
    if (!table) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        }

    table->size = size;
    table->buckets = (task_tcb_t **)AGENTOS_CALLOC(size, sizeof(task_tcb_t *));
    if (!table->buckets) {
        AGENTOS_FREE(table);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    table->lock = agentos_mutex_create();
    if (!table->lock) {
        AGENTOS_FREE(table->buckets);
        AGENTOS_FREE(table);
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
    }

    return table;
}

/**
 * @brief 销毁哈希表，释放所有持有引?
 */
static void task_hash_table_destroy(task_hash_table_t *table)
{
    if (!table)
        return;

    agentos_mutex_lock(table->lock);
    for (size_t i = 0; i < table->size; i++) {
        task_tcb_t *tcb = table->buckets[i];
        while (tcb) {
            task_tcb_t *next = tcb->hash_next;
            tcb->hash_next = NULL;
            tcb_release(tcb);
            tcb = next;
        }
        table->buckets[i] = NULL;
    }
    agentos_mutex_unlock(table->lock);

    agentos_mutex_free(table->lock);
    AGENTOS_FREE(table->buckets);
    AGENTOS_FREE(table);
}

/**
 * @brief 向哈希表插入任务（增加引用计数）
 */
static void task_hash_table_insert(task_hash_table_t *table, task_tcb_t *tcb)
{
    if (!table || !tcb || !tcb->task_id)
        return;

    tcb_retain(tcb);

    size_t index = task_hash(tcb->task_id, table->size);
    agentos_mutex_lock(table->lock);
    tcb->hash_next = table->buckets[index];
    table->buckets[index] = tcb;
    agentos_mutex_unlock(table->lock);
}

/**
 * @brief 在哈希表中查找任务（不增加引用计数）
 */
static task_tcb_t *task_hash_table_find(task_hash_table_t *table, const char *task_id)
{
    if (!table || !task_id) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        }

    size_t index = task_hash(task_id, table->size);
    agentos_mutex_lock(table->lock);

    task_tcb_t *tcb = table->buckets[index];
    while (tcb) {
        if (strcmp(tcb->task_id, task_id) == 0) {
            tcb_retain(tcb);
            agentos_mutex_unlock(table->lock);
            return tcb;
        }
        tcb = tcb->hash_next;
    }

    agentos_mutex_unlock(table->lock);
    AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "operation failed");
}

/**
 * @brief 从哈希表中移除任务（减少引用计数?
 */
static void task_hash_table_remove(task_hash_table_t *table, const char *task_id)
{
    if (!table || !task_id)
        return;

    size_t index = task_hash(task_id, table->size);
    agentos_mutex_lock(table->lock);

    task_tcb_t **p = &table->buckets[index];
    while (*p) {
        if (strcmp((*p)->task_id, task_id) == 0) {
            task_tcb_t *tcb = *p;
            *p = tcb->hash_next;
            tcb->hash_next = NULL;
            agentos_mutex_unlock(table->lock);
            tcb_release(tcb);
            return;
        }
        p = &(*p)->hash_next;
    }

    agentos_mutex_unlock(table->lock);
}

/**
 * @brief 从链表中移除指定 TCB
 */
static void remove_tcb_from_list(task_tcb_t **list, task_tcb_t *target)
{
    task_tcb_t **p = list;
    while (*p) {
        if (*p == target) {
            *p = target->next;
            target->next = NULL;
            return;
        }
        p = &(*p)->next;
    }
}

/**
 * @brief 增加任务引用计数
 */
static void tcb_retain(task_tcb_t *tcb)
{
    if (!tcb)
        return;
    agentos_mutex_lock(tcb->tcb_lock);
    tcb->ref_count++;
    agentos_mutex_unlock(tcb->tcb_lock);
}

/**
 * @brief 减少任务引用计数，归零时释放
 */
static void tcb_release(task_tcb_t *tcb)
{
    if (!tcb)
        return;
    int need_free = 0;
    agentos_mutex_lock(tcb->tcb_lock);
    if (tcb->ref_count <= 0) {
        agentos_mutex_unlock(tcb->tcb_lock);
        AGENTOS_LOG_ERROR("tcb_release: ref_count already zero for task %s",
                          tcb->task_id ? tcb->task_id : "(null)");
        return;
    }
    tcb->ref_count--;
    if (tcb->ref_count <= 0) {
        need_free = 1;
    }
    agentos_mutex_unlock(tcb->tcb_lock);
    if (need_free) {
        AGENTOS_FREE(tcb->task_id);
        if (tcb->task_desc) {
            if (tcb->task_desc->task_id)
                AGENTOS_FREE(tcb->task_desc->task_id);
            if (tcb->task_desc->task_agent_id)
                AGENTOS_FREE(tcb->task_desc->task_agent_id);
            if (tcb->task_desc->task_input)
                AGENTOS_FREE(tcb->task_desc->task_input);
            AGENTOS_FREE(tcb->task_desc);
        }
        if (tcb->result)
            AGENTOS_FREE(tcb->result);
        if (tcb->completed_cond)
            agentos_cond_free(tcb->completed_cond);
        if (tcb->tcb_lock)
            agentos_mutex_free(tcb->tcb_lock);
        AGENTOS_FREE(tcb);
    }
}

/**
 * @brief 深拷贝任务描?
 */
static agentos_task_t *task_desc_deep_copy(const agentos_task_t *task)
{
    agentos_task_t *copy = (agentos_task_t *)AGENTOS_CALLOC(1, sizeof(agentos_task_t));
    if (!copy) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        }

    if (task->task_id) {
        copy->task_id = AGENTOS_STRDUP(task->task_id);
        if (!copy->task_id)
            goto fail;
    }
    if (task->task_agent_id) {
        copy->task_agent_id = AGENTOS_STRDUP(task->task_agent_id);
        if (!copy->task_agent_id)
            goto fail;
    }
    if (task->task_input) {
        size_t input_len = strnlen((const char *)task->task_input, 65536);
        copy->task_input = AGENTOS_MALLOC(input_len + 1);
        if (!copy->task_input)
            goto fail;
        __builtin_memcpy(copy->task_input, task->task_input, input_len + 1);
    }
    copy->task_timeout_ms = task->task_timeout_ms;
    return copy;

fail:
    if (copy->task_id)
        AGENTOS_FREE(copy->task_id);
    if (copy->task_agent_id)
        AGENTOS_FREE(copy->task_agent_id);
    if (copy->task_input)
        AGENTOS_FREE(copy->task_input);
    AGENTOS_FREE(copy);
    AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "operation failed");
}

/**
 * @brief 深拷贝任务结?
 */
static agentos_task_t *task_result_deep_copy(const task_tcb_t *tcb)
{
    agentos_task_t *result = (agentos_task_t *)AGENTOS_CALLOC(1, sizeof(agentos_task_t));
    if (!result) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        }

    if (tcb->task_desc->task_id) {
        result->task_id = AGENTOS_STRDUP(tcb->task_desc->task_id);
        if (!result->task_id)
            goto fail;
    }
    if (tcb->task_desc->task_agent_id) {
        result->task_agent_id = AGENTOS_STRDUP(tcb->task_desc->task_agent_id);
        if (!result->task_agent_id)
            goto fail;
    }
    if (tcb->result && tcb->result_len > 0) {
        result->task_output = AGENTOS_MALLOC(tcb->result_len);
        if (!result->task_output)
            goto fail;
        __builtin_memcpy(result->task_output, tcb->result, tcb->result_len);
    }
    return result;

fail:
    if (result->task_id)
        AGENTOS_FREE(result->task_id);
    if (result->task_agent_id)
        AGENTOS_FREE(result->task_agent_id);
    AGENTOS_FREE(result);
    AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "operation failed");
}

/**
 * @brief 工作线程主函?
 */
static void *worker_thread_func(void *arg)
{
    agentos_execution_engine_t *engine = (agentos_execution_engine_t *)arg;

    while (1) {
        task_tcb_t *tcb = NULL;
        agentos_mutex_lock(engine->queue_lock);
        while (engine->task_queue == NULL && engine->running) {
            agentos_cond_wait(engine->task_available_cond, engine->queue_lock);
        }
        if (!engine->running && engine->task_queue == NULL) {
            agentos_mutex_unlock(engine->queue_lock);
            break;
        }
        if (engine->task_queue == NULL) {
            agentos_mutex_unlock(engine->queue_lock);
            continue;
        }
        tcb = engine->task_queue;
        engine->task_queue = tcb->next;
        tcb->next = NULL;
        agentos_mutex_unlock(engine->queue_lock);

        agentos_mutex_lock(engine->running_lock);
        engine->current_concurrency++;
        tcb->next = engine->running_tasks;
        engine->running_tasks = tcb;
        agentos_mutex_unlock(engine->running_lock);

        agentos_mutex_lock(tcb->tcb_lock);
        tcb->start_time_ns = agentos_time_monotonic_ns();
        tcb->status = TASK_STATUS_RUNNING;
        agentos_mutex_unlock(tcb->tcb_lock);

        AGENTOS_LOG_DEBUG("ExecutionEngine: worker executing task (task_id=%s, agent=%s)",
                          tcb->task_id,
                          tcb->task_desc->task_agent_id ? tcb->task_desc->task_agent_id : "(none)");

        agentos_execution_unit_t *unit = agentos_registry_get_unit(tcb->task_desc->task_agent_id);
        void *output = NULL;
        size_t output_len = 0;
        agentos_error_t exec_err;
        if (unit) {
            exec_err = unit->execution_unit_execute(unit, tcb->task_desc->task_input, &output);
            if (output) {
                output_len = strnlen((const char *)output, AGENTOS_EXEC_MAX_OUTPUT_LEN);
            }
        } else {
            exec_err = AGENTOS_ENOENT;
            AGENTOS_LOG_ERROR("No execution unit found for agent %s",
                              tcb->task_desc->task_agent_id);
        }

        agentos_mutex_lock(tcb->tcb_lock);
        tcb->end_time_ns = agentos_time_monotonic_ns();
        tcb->status = (exec_err == AGENTOS_SUCCESS) ? TASK_STATUS_SUCCEEDED : TASK_STATUS_FAILED;
        tcb->result = output;
        tcb->result_len = output_len;
        agentos_cond_signal(tcb->completed_cond);
        agentos_mutex_unlock(tcb->tcb_lock);

        uint64_t elapsed_ns = tcb->end_time_ns - tcb->start_time_ns;
        AGENTOS_LOG_DEBUG("ExecutionEngine: worker task %s (task_id=%s, status=%s, elapsed_us=%" PRIu64 ")",
                          exec_err == AGENTOS_SUCCESS ? "DONE" : "FAILED",
                          tcb->task_id,
                          exec_err == AGENTOS_SUCCESS ? "SUCCEEDED" : "FAILED",
                          elapsed_ns / 1000);

        agentos_mutex_lock(engine->running_lock);
        remove_tcb_from_list(&engine->running_tasks, tcb);
        engine->current_concurrency--;
        agentos_mutex_unlock(engine->running_lock);

        agentos_mutex_lock(engine->queue_lock);
        int still_running = engine->running;
        agentos_mutex_unlock(engine->queue_lock);

        if (still_running) {
            task_hash_table_remove(engine->task_map, tcb->task_id);
        }
        tcb_release(tcb);
    }

    /* 线程退出前清理线程局部错误状态，避免 LSan 报告内存泄漏 */
    agentos_error_thread_cleanup();
    return NULL;
}

/**
 * @brief 创建执行引擎
 *
 * @param max_concurrency 最大并发任务数?表示使用默认??
 * @param out_engine 输出执行引擎指针
 * @return agentos_error_t 错误?
 *
 * @note 执行引擎负责?
 *       1. 接收并执行任?
 *       2. 管理任务队列和状?
 *       3. 提供任务查询和结果获取接?
 *       4. 使用哈希表实现O(1)任务查找
 *
 * @warning 调用者负责释放返回的执行引擎（使?agentos_execution_destroy?
 */
agentos_error_t agentos_execution_create(uint32_t max_concurrency,
                                         agentos_execution_engine_t **out_engine)
{

    if (!out_engine)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to create execution engine: null out_engine");
    if (max_concurrency == 0)
        max_concurrency = 1;

    agentos_execution_engine_t *engine =
        (agentos_execution_engine_t *)AGENTOS_CALLOC(1, sizeof(agentos_execution_engine_t));
    if (!engine) {
        AGENTOS_LOG_ERROR("Failed to allocate execution engine");
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    engine->max_concurrency = max_concurrency;
    engine->queue_lock = agentos_mutex_create();
    engine->running_lock = agentos_mutex_create();
    engine->task_available_cond = agentos_cond_create();
    engine->registry_lock = agentos_mutex_create();
    if (!engine->queue_lock || !engine->running_lock || !engine->task_available_cond ||
        !engine->registry_lock) {
        if (engine->queue_lock)
            agentos_mutex_free(engine->queue_lock);
        if (engine->running_lock)
            agentos_mutex_free(engine->running_lock);
        if (engine->task_available_cond)
            agentos_cond_free(engine->task_available_cond);
        if (engine->registry_lock)
            agentos_mutex_free(engine->registry_lock);
        AGENTOS_FREE(engine);
        AGENTOS_LOG_ERROR("Failed to create synchronization primitives");
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }
    engine->registered_unit_count = 0;
    engine->feedback_callback = NULL;
    engine->feedback_user_data = NULL;

    engine->running = 1;
    engine->worker_count = max_concurrency;
    if (engine->worker_count > SIZE_MAX / sizeof(agentos_thread_t)) {
        agentos_mutex_free(engine->queue_lock);
        agentos_mutex_free(engine->running_lock);
        agentos_cond_free(engine->task_available_cond);
        agentos_mutex_free(engine->registry_lock);
        AGENTOS_FREE(engine);
        AGENTOS_LOG_ERROR("Overflow in worker threads array allocation");
        ATM_RET_ERR(AGENTOS_EOVERFLOW);
    }
    engine->worker_threads =
        (agentos_thread_t *)AGENTOS_MALLOC(engine->worker_count * sizeof(agentos_thread_t));
    if (!engine->worker_threads) {
        agentos_mutex_free(engine->queue_lock);
        agentos_mutex_free(engine->running_lock);
        agentos_cond_free(engine->task_available_cond);
        agentos_mutex_free(engine->registry_lock);
        AGENTOS_FREE(engine);
        AGENTOS_LOG_ERROR("Failed to allocate worker threads array");
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    // 创建任务哈希表，大小为最大并发数?倍，减少冲突
    engine->task_map = task_hash_table_create(max_concurrency * 2);
    if (!engine->task_map) {
        AGENTOS_FREE(engine->worker_threads);
        agentos_mutex_free(engine->queue_lock);
        agentos_mutex_free(engine->running_lock);
        agentos_cond_free(engine->task_available_cond);
        agentos_mutex_free(engine->registry_lock);
        AGENTOS_FREE(engine);
        AGENTOS_LOG_ERROR("Failed to create task hash table");
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    for (size_t i = 0; i < engine->worker_count; i++) {
        if (agentos_platform_thread_create(&engine->worker_threads[i], worker_thread_func,
                                           engine) != AGENTOS_SUCCESS) {
            engine->running = 0;
            for (size_t j = 0; j < i; j++) {
                agentos_platform_thread_join(engine->worker_threads[j], NULL);
            }
            AGENTOS_FREE(engine->worker_threads);
            agentos_mutex_free(engine->queue_lock);
            agentos_mutex_free(engine->running_lock);
            agentos_cond_free(engine->task_available_cond);
            agentos_mutex_free(engine->registry_lock);
            AGENTOS_FREE(engine);
            AGENTOS_LOG_ERROR("Failed to create worker thread %zu", i);
            ATM_RET_ERR(AGENTOS_ENOMEM);
        }
    }

    *out_engine = engine;
    AGENTOS_LOG_INFO("ExecutionEngine: created (max_concurrency=%u, workers=%zu)",
                     max_concurrency, engine->worker_count);
    return AGENTOS_SUCCESS;
}

void agentos_execution_destroy(agentos_execution_engine_t *engine)
{
    if (!engine)
        return;

    AGENTOS_LOG_INFO("ExecutionEngine: destroy START (workers=%zu, max_concurrency=%u)",
                     engine->worker_count, engine->max_concurrency);

    agentos_mutex_lock(engine->queue_lock);
    engine->running = 0;
    agentos_cond_broadcast(engine->task_available_cond);
    agentos_mutex_unlock(engine->queue_lock);

    for (size_t i = 0; i < engine->worker_count; i++) {
        agentos_platform_thread_join(engine->worker_threads[i], NULL);
    }
    AGENTOS_FREE(engine->worker_threads);
    engine->worker_threads = NULL;
    engine->worker_count = 0;

    agentos_mutex_lock(engine->queue_lock);
    task_tcb_t *tcb = engine->task_queue;
    while (tcb) {
        task_tcb_t *next = tcb->next;
        tcb_release(tcb);
        tcb = next;
    }
    engine->task_queue = NULL;
    agentos_mutex_unlock(engine->queue_lock);

    agentos_mutex_lock(engine->running_lock);
    tcb = engine->running_tasks;
    while (tcb) {
        task_tcb_t *next = tcb->next;
        tcb_release(tcb);
        tcb = next;
    }
    engine->running_tasks = NULL;
    engine->current_concurrency = 0;
    agentos_mutex_unlock(engine->running_lock);

    task_hash_table_destroy(engine->task_map);
    engine->task_map = NULL;

    /* 清理执行单元注册表 */
    agentos_mutex_lock(engine->registry_lock);
    for (uint32_t i = 0; i < engine->registered_unit_count; i++) {
        if (engine->registered_units[i].execution_unit_destroy) {
            engine->registered_units[i].execution_unit_destroy(&engine->registered_units[i]);
        }
        if (engine->registered_unit_names[i]) {
            AGENTOS_FREE(engine->registered_unit_names[i]);
            engine->registered_unit_names[i] = NULL;
        }
    }
    engine->registered_unit_count = 0;
    agentos_mutex_unlock(engine->registry_lock);

    agentos_mutex_free(engine->queue_lock);
    agentos_mutex_free(engine->running_lock);
    agentos_cond_free(engine->task_available_cond);
    agentos_mutex_free(engine->registry_lock);
    AGENTOS_FREE(engine);
    AGENTOS_LOG_DEBUG("ExecutionEngine: destroy done");
}

/**
 * @brief 注册执行单元到引擎
 *
 * 将执行单元存储到引擎注册表中，按名称索引。引擎接管单元的生命周期管理。
 * 注册的单元可通过名称查找并在任务执行路径中被调用。
 */
agentos_error_t agentos_execution_register_unit(agentos_execution_engine_t *engine,
                                                const char *name, agentos_execution_unit_t unit)
{
    if (!engine || !name)
        return AGENTOS_EINVAL;

    agentos_mutex_lock(engine->registry_lock);

    /* 检查容量 */
    if (engine->registered_unit_count >= AGENTOS_EXECUTION_MAX_UNITS) {
        agentos_mutex_unlock(engine->registry_lock);
        AGENTOS_LOG_ERROR("ExecutionEngine: unit registry full (max=%d)",
                          AGENTOS_EXECUTION_MAX_UNITS);
        return AGENTOS_EBUSY;
    }

    /* 检查重名 */
    for (uint32_t i = 0; i < engine->registered_unit_count; i++) {
        if (engine->registered_unit_names[i] &&
            strcmp(engine->registered_unit_names[i], name) == 0) {
            agentos_mutex_unlock(engine->registry_lock);
            AGENTOS_LOG_ERROR("ExecutionEngine: unit '%s' already registered", name);
            return AGENTOS_EEXIST;
        }
    }

    /* 复制名称 */
    size_t name_len = strlen(name);
    char *name_copy = (char *)AGENTOS_MALLOC(name_len + 1);
    if (!name_copy) {
        agentos_mutex_unlock(engine->registry_lock);
        AGENTOS_LOG_ERROR("ExecutionEngine: failed to allocate unit name '%s'", name);
        return AGENTOS_ENOMEM;
    }
    __builtin_memcpy(name_copy, name, name_len + 1);

    /* 存储单元 */
    uint32_t slot = engine->registered_unit_count;
    engine->registered_units[slot] = unit;
    engine->registered_unit_names[slot] = name_copy;
    engine->registered_unit_count++;

    agentos_mutex_unlock(engine->registry_lock);
    AGENTOS_LOG_INFO("ExecutionEngine: registered unit '%s' (slot=%u, total=%u)",
                     name, slot, engine->registered_unit_count);
    return AGENTOS_SUCCESS;
}

/**
 * @brief 从引擎注销执行单元
 *
 * 从注册表中移除指定名称的执行单元，并调用其 destroy 回调释放资源。
 * 使用交换删除法保持数组紧凑。
 */
void agentos_execution_unregister_unit(agentos_execution_engine_t *engine, const char *name)
{
    if (!engine || !name)
        return;

    agentos_mutex_lock(engine->registry_lock);

    for (uint32_t i = 0; i < engine->registered_unit_count; i++) {
        if (engine->registered_unit_names[i] &&
            strcmp(engine->registered_unit_names[i], name) == 0) {
            /* 调用单元的 destroy 回调 */
            if (engine->registered_units[i].execution_unit_destroy) {
                engine->registered_units[i].execution_unit_destroy(&engine->registered_units[i]);
            }
            /* 释放名称副本 */
            AGENTOS_FREE(engine->registered_unit_names[i]);

            /* 交换删除：用最后一个元素填补空位 */
            uint32_t last = engine->registered_unit_count - 1;
            if (i != last) {
                engine->registered_units[i] = engine->registered_units[last];
                engine->registered_unit_names[i] = engine->registered_unit_names[last];
            }
            engine->registered_unit_names[last] = NULL;
            engine->registered_unit_count--;

            AGENTOS_LOG_INFO("ExecutionEngine: unregistered unit '%s' (remaining=%u)",
                             name, engine->registered_unit_count);
            agentos_mutex_unlock(engine->registry_lock);
            return;
        }
    }

    agentos_mutex_unlock(engine->registry_lock);
    AGENTOS_LOG_WARN("ExecutionEngine: unit '%s' not found for unregister", name);
}

/**
 * @brief 设置反馈回调
 *
 * 存储回调函数与用户数据，引擎在任务执行路径中通过此回调
 * 向上层反馈任务状态（开始/完成/失败/重试等）。
 * 传入 NULL callback 可取消回调。
 */
void agentos_execution_set_feedback_callback(agentos_execution_engine_t *engine,
                                             agentos_feedback_callback_t callback, void *user_data)
{
    if (!engine)
        return;

    agentos_mutex_lock(engine->registry_lock);
    engine->feedback_callback = callback;
    engine->feedback_user_data = user_data;
    agentos_mutex_unlock(engine->registry_lock);

    AGENTOS_LOG_INFO("ExecutionEngine: feedback callback %s",
                     callback ? "set" : "cleared");
}

/**
 * @brief 提交任务到执行引?
 *
 * @param engine 执行引擎
 * @param task 任务描述
 * @param out_task_id 输出任务ID（需调用者释放）
 * @return agentos_error_t 错误?
 *
 * @note 提交流程?
 *       1. 参数验证
 *       2. 复制任务描述
 *       3. 创建任务控制块（TCB?
 *       4. 生成唯一任务ID
 *       5. 初始化任务状态和资源
 *       6. 将任务添加到队列和哈希表
 *       7. 通知工作线程有新任务
 *
 * @warning 返回的任务ID需要调用者使?AGENTOS_FREE() 释放
 */
agentos_error_t agentos_execution_submit(agentos_execution_engine_t *engine,
                                         const agentos_task_t *task, char **out_task_id)
{

    if (!engine || !task || !out_task_id) {
        AGENTOS_LOG_ERROR(
            "Invalid parameters to execution_submit: engine=%p, task=%p, out_task_id=%p",
            (void *)engine, (void *)task, (void *)out_task_id);
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to submit execution: null engine, task, or out_task_id");
    }

    agentos_task_t *task_copy = task_desc_deep_copy(task);
    if (!task_copy) {
        AGENTOS_LOG_ERROR("Failed to deep copy task description");
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    task_tcb_t *tcb = (task_tcb_t *)AGENTOS_CALLOC(1, sizeof(task_tcb_t));
    if (!tcb) {
        AGENTOS_FREE(task_copy);
        AGENTOS_LOG_ERROR("Failed to allocate task control block: %s (code %d)",
                          agentos_error_string(AGENTOS_ENOMEM), AGENTOS_ENOMEM);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    char id_buf[64];
    agentos_generate_task_id("task", id_buf, sizeof(id_buf));
    tcb->task_id = AGENTOS_STRDUP(id_buf);
    tcb->task_desc = task_copy;
    tcb->status = TASK_STATUS_PENDING;
    tcb->submit_time_ns = agentos_time_monotonic_ns();
    tcb->completed_cond = agentos_cond_create();
    tcb->tcb_lock = agentos_mutex_create();
    tcb->ref_count = 1;  // 初始引用

    if (!tcb->task_id || !tcb->completed_cond || !tcb->tcb_lock) {
        AGENTOS_LOG_ERROR("Failed to create task resources: task_id=%p, cond=%p, lock=%p",
                          (void *)tcb->task_id, (void *)tcb->completed_cond, (void *)tcb->tcb_lock);
        if (tcb->task_id)
            AGENTOS_FREE(tcb->task_id);
        if (tcb->completed_cond)
            agentos_cond_free(tcb->completed_cond);
        if (tcb->tcb_lock)
            agentos_mutex_free(tcb->tcb_lock);
        AGENTOS_FREE(tcb);
        AGENTOS_FREE(task_copy);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    char *dup_id = AGENTOS_STRDUP(tcb->task_id);

    agentos_mutex_lock(engine->queue_lock);
    tcb->next = engine->task_queue;
    engine->task_queue = tcb;
    task_hash_table_insert(engine->task_map, tcb);
    *out_task_id = dup_id;
    agentos_cond_signal(engine->task_available_cond);
    agentos_mutex_unlock(engine->queue_lock);

    if (!*out_task_id) {
        AGENTOS_LOG_ERROR("Failed to duplicate task_id");
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    AGENTOS_LOG_DEBUG("ExecutionEngine: task submitted (task_id=%s, agent=%s, timeout=%u)",
                      tcb->task_id,
                      task_copy->task_agent_id ? task_copy->task_agent_id : "(none)",
                      task_copy->task_timeout_ms);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_execution_query(agentos_execution_engine_t *engine, const char *task_id,
                                        agentos_task_status_t *out_status)
{

    if (!engine || !task_id || !out_status)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to query execution: null engine, task_id, or out_status");

    // 使用哈希表快速查找任?
    task_tcb_t *tcb = task_hash_table_find(engine->task_map, task_id);
    if (tcb) {
        agentos_mutex_lock(tcb->tcb_lock);
        *out_status = tcb->status;
        agentos_mutex_unlock(tcb->tcb_lock);
        tcb_release(tcb);
        return AGENTOS_SUCCESS;
    }

    ATM_RET_ERR(AGENTOS_ENOENT);
}

agentos_error_t agentos_execution_wait(agentos_execution_engine_t *engine, const char *task_id,
                                       uint32_t timeout_ms, agentos_task_t **out_result)
{

    if (!engine || !task_id)
        ATM_RET_ERR(AGENTOS_EINVAL);

    // 使用哈希表快速查找任?
    task_tcb_t *tcb = task_hash_table_find(engine->task_map, task_id);
    if (!tcb)
        ATM_RET_ERR(AGENTOS_ENOENT);

    agentos_cond_t *cond = tcb->completed_cond;
    agentos_mutex_lock(tcb->tcb_lock);
    while (tcb->status == TASK_STATUS_PENDING || tcb->status == TASK_STATUS_RUNNING) {
        if (timeout_ms == 0) {
            agentos_cond_wait(cond, tcb->tcb_lock);
        } else {
            int err = agentos_cond_timedwait(cond, tcb->tcb_lock, timeout_ms);
            if (err != 0) {
                agentos_mutex_unlock(tcb->tcb_lock);
                tcb_release(tcb);
                ATM_RET_ERR(AGENTOS_ETIMEDOUT);
            }
        }
    }
    agentos_mutex_unlock(tcb->tcb_lock);

    if (out_result) {
        agentos_task_t *result_copy = task_result_deep_copy(tcb);
        if (!result_copy) {
            tcb_release(tcb);
            ATM_RET_ERR(AGENTOS_ENOMEM);
        }
        *out_result = result_copy;
    }

    tcb_release(tcb);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_execution_cancel(agentos_execution_engine_t *engine, const char *task_id)
{

    if (!engine || !task_id)
        ATM_RET_ERR(AGENTOS_EINVAL);

    // 使用哈希表快速查找任务（task_hash_table_find 会 retain，需在所有路径 release）
    task_tcb_t *tcb = task_hash_table_find(engine->task_map, task_id);
    if (!tcb)
        ATM_RET_ERR(AGENTOS_ENOENT);

    // 检查任务是否在队列中
    agentos_mutex_lock(engine->queue_lock);
    task_tcb_t **p = &engine->task_queue;
    while (*p) {
        if (*p == tcb) {
            *p = tcb->next;
            tcb->next = NULL;
            agentos_mutex_unlock(engine->queue_lock);

            // 从哈希表中移除任务（内部会 release 一次，释放哈希表的引用）
            task_hash_table_remove(engine->task_map, task_id);

            agentos_mutex_lock(tcb->tcb_lock);
            tcb->status = TASK_STATUS_CANCELLED;
            tcb->end_time_ns = agentos_time_monotonic_ns();
            agentos_cond_signal(tcb->completed_cond);
            agentos_mutex_unlock(tcb->tcb_lock);
            /* 释放两次引用：
             * 1. task_hash_table_find 的 retain
             * 2. submit 时的初始引用 (ref_count=1)
             * task_hash_table_remove 已释放哈希表引用，故共需 2 次 release */
            tcb_release(tcb);
            tcb_release(tcb);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(engine->queue_lock);
    /* 任务不在队列中（可能已被 worker 取走），仅释放 find 的 retain */
    tcb_release(tcb);
    ATM_RET_ERR(AGENTOS_ENOENT);
}

agentos_error_t agentos_execution_get_result(agentos_execution_engine_t *engine,
                                             const char *task_id, agentos_task_t **out_result)
{

    if (!engine || !task_id || !out_result)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to get execution result: null engine, task_id, or out_result");

    // 使用哈希表快速查找任?
    task_tcb_t *tcb = task_hash_table_find(engine->task_map, task_id);
    if (!tcb)
        ATM_RET_ERR(AGENTOS_ENOENT);

    agentos_mutex_lock(tcb->tcb_lock);
    if (tcb->status != TASK_STATUS_SUCCEEDED && tcb->status != TASK_STATUS_FAILED) {
        agentos_mutex_unlock(tcb->tcb_lock);
        tcb_release(tcb);
        ATM_RET_ERR(AGENTOS_EBUSY);
    }
    agentos_mutex_unlock(tcb->tcb_lock);

    agentos_task_t *result_copy = task_result_deep_copy(tcb);
    if (!result_copy) {
        tcb_release(tcb);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }
    *out_result = result_copy;

    tcb_release(tcb);
    return AGENTOS_SUCCESS;
}

void agentos_task_free(agentos_task_t *task)
{
    if (!task)
        return;
    if (task->task_id)
        AGENTOS_FREE(task->task_id);
    if (task->task_agent_id)
        AGENTOS_FREE(task->task_agent_id);
    if (task->task_input)
        AGENTOS_FREE(task->task_input);
    if (task->task_output)
        AGENTOS_FREE(task->task_output);
    AGENTOS_FREE(task);
}

agentos_error_t agentos_execution_health_check(agentos_execution_engine_t *engine, char **out_json)
{

    if (!engine || !out_json)
        AGENTOS_ERROR(AGENTOS_EINVAL, "failed to check execution health: null engine or out_json");

    cJSON *root = cJSON_CreateObject();
    if (!root)
        ATM_RET_ERR(AGENTOS_ENOMEM);

    agentos_mutex_lock(engine->queue_lock);
    size_t queue_len = 0;
    task_tcb_t *t = engine->task_queue;
    while (t) {
        queue_len++;
        t = t->next;
    }
    agentos_mutex_unlock(engine->queue_lock);

    agentos_mutex_lock(engine->running_lock);
    size_t running_len = 0;
    t = engine->running_tasks;
    while (t) {
        running_len++;
        t = t->next;
    }
    uint32_t cur = engine->current_concurrency;
    agentos_mutex_unlock(engine->running_lock);

    cJSON_AddStringToObject(root, "status", "healthy");
    cJSON_AddNumberToObject(root, "task_queue_length", queue_len);
    cJSON_AddNumberToObject(root, "running_tasks", running_len);
    cJSON_AddNumberToObject(root, "current_concurrency", cur);
    cJSON_AddNumberToObject(root, "max_concurrency", engine->max_concurrency);
    cJSON_AddNumberToObject(root, "running", engine->running);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json)
        ATM_RET_ERR(AGENTOS_ENOMEM);

    *out_json = json;
    return AGENTOS_SUCCESS;
}
