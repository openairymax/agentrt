/**
 * @file engine.c
 * @brief 行动层执行引擎核心实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../../include/execution.h"
#include "../../include/agent_registry.h"
#include <agentos/atoms/corekern/include/agentos.h>
#include <agentos/utils/logging/logging_compat.h>
#include "../../include/id_utils.h"
#include "../../include/error_utils.h"
#include <stdlib.h>
#include <stdint.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>
#include <stdio.h>

/* JSON解析库 - 条件编译 */
#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
#else
typedef struct cJSON { int type; char* valuestring; double valuedouble; int valueint; struct cJSON* child; struct cJSON* next; } cJSON;
#define cJSON_NULL 0
#define cJSON_False 1
#define cJSON_True 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6
static inline cJSON* cJSON_CreateObject(void) { return NULL; }
static inline void cJSON_Delete(cJSON* item) { (void)item; }
static inline cJSON* cJSON_CreateArray(void) { return NULL; }
static inline void cJSON_AddItemToArray(cJSON* a, cJSON* i) { (void)a;(void)i; }
static inline void cJSON_AddItemToObject(cJSON* o, const char* k, cJSON* i) { (void)o;(void)k;(void)i; }
static inline void cJSON_AddStringToObject(cJSON* o, const char* k, const char* v) { (void)o;(void)k;(void)v; }
static inline void cJSON_AddNumberToObject(cJSON* o, const char* k, double v) { (void)o;(void)k;(void)v; }
static inline char* cJSON_PrintUnformatted(const cJSON* i) { (void)i; return NULL; }
#endif /* AGENTOS_HAS_CJSON */

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include <agentos/utils/include/atomic_compat.h>

/* 平台特定头文件 */
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <process.h>
#else
    #include <pthread.h>
    #include <unistd.h>
#endif

typedef struct task_control_block {
    char* task_id;
    agentos_task_t* task_desc;
    agentos_task_status_t status;
    uint64_t submit_time_ns;
    uint64_t start_time_ns;
    uint64_t end_time_ns;
    void* result;
    size_t result_len;
    agentos_cond_t* completed_cond;
    agentos_mutex_t* tcb_lock;
    int ref_count;
    struct task_control_block* next;
    struct task_control_block* hash_next;
} task_tcb_t;

typedef struct {
    task_tcb_t** buckets;
    size_t size;
    agentos_mutex_t* lock;
} task_hash_table_t;

struct agentos_execution_engine {
    uint32_t max_concurrency;
    uint32_t current_concurrency;
    agentos_mutex_t* queue_lock;
    agentos_mutex_t* running_lock;
    task_tcb_t* task_queue;
    task_tcb_t* running_tasks;
    task_hash_table_t* task_map;
    agentos_cond_t* task_available_cond;
    agentos_thread_t* worker_threads;
    size_t worker_count;
    volatile int running;
};

static void tcb_retain(task_tcb_t* tcb);
static void tcb_release(task_tcb_t* tcb);

/**
 * @brief 哈希函数（djb2 变体�?
 */
static size_t task_hash(const char* task_id, size_t table_size) {
    size_t hash = 5381;
    while (*task_id) {
        hash = ((hash << 5) + hash) + (unsigned char)*task_id++;
    }
    return hash % table_size;
}

/**
 * @brief 创建哈希�?
 */
static task_hash_table_t* task_hash_table_create(size_t size) {
    task_hash_table_t* table = (task_hash_table_t*)AGENTOS_CALLOC(1, sizeof(task_hash_table_t));
    if (!table) return NULL;

    table->size = size;
    table->buckets = (task_tcb_t**)AGENTOS_CALLOC(size, sizeof(task_tcb_t*));
    if (!table->buckets) {
        AGENTOS_FREE(table);
        return NULL;
    }

    table->lock = agentos_mutex_create();
    if (!table->lock) {
        AGENTOS_FREE(table->buckets);
        AGENTOS_FREE(table);
        return NULL;
    }

    return table;
}

/**
 * @brief 销毁哈希表，释放所有持有引�?
 */
static void task_hash_table_destroy(task_hash_table_t* table) {
    if (!table) return;

    agentos_mutex_lock(table->lock);
    for (size_t i = 0; i < table->size; i++) {
        task_tcb_t* tcb = table->buckets[i];
        while (tcb) {
            task_tcb_t* next = tcb->hash_next;
            tcb->hash_next = NULL;
            tcb_release(tcb);
            tcb = next;
        }
        table->buckets[i] = NULL;
    }
    agentos_mutex_unlock(table->lock);

    agentos_mutex_destroy(table->lock);
    AGENTOS_FREE(table->buckets);
    AGENTOS_FREE(table);
}

/**
 * @brief 向哈希表插入任务（增加引用计数）
 */
static void task_hash_table_insert(task_hash_table_t* table, task_tcb_t* tcb) {
    if (!table || !tcb || !tcb->task_id) return;

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
static task_tcb_t* task_hash_table_find(task_hash_table_t* table, const char* task_id) {
    if (!table || !task_id) return NULL;

    size_t index = task_hash(task_id, table->size);
    agentos_mutex_lock(table->lock);

    task_tcb_t* tcb = table->buckets[index];
    while (tcb) {
        if (strcmp(tcb->task_id, task_id) == 0) {
            agentos_mutex_unlock(table->lock);
            return tcb;
        }
        tcb = tcb->hash_next;
    }

    agentos_mutex_unlock(table->lock);
    return NULL;
}

/**
 * @brief 从哈希表中移除任务（减少引用计数�?
 */
static void task_hash_table_remove(task_hash_table_t* table, const char* task_id) {
    if (!table || !task_id) return;

    size_t index = task_hash(task_id, table->size);
    agentos_mutex_lock(table->lock);

    task_tcb_t** p = &table->buckets[index];
    while (*p) {
        if (strcmp((*p)->task_id, task_id) == 0) {
            task_tcb_t* tcb = *p;
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
static void remove_tcb_from_list(task_tcb_t** list, task_tcb_t* target) {
    task_tcb_t** p = list;
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
static void tcb_retain(task_tcb_t* tcb) {
    if (!tcb) return;
    agentos_mutex_lock(tcb->tcb_lock);
    tcb->ref_count++;
    agentos_mutex_unlock(tcb->tcb_lock);
}

/**
 * @brief 减少任务引用计数，归零时释放
 */
static void tcb_release(task_tcb_t* tcb) {
    if (!tcb) return;
    int need_free = 0;
    agentos_mutex_lock(tcb->tcb_lock);
    tcb->ref_count--;
    if (tcb->ref_count <= 0) {
        need_free = 1;
    }
    agentos_mutex_unlock(tcb->tcb_lock);
    if (need_free) {
        AGENTOS_FREE(tcb->task_id);
        if (tcb->task_desc) {
            if (tcb->task_desc->task_id) AGENTOS_FREE(tcb->task_desc->task_id);
            if (tcb->task_desc->task_agent_id) AGENTOS_FREE(tcb->task_desc->task_agent_id);
            if (tcb->task_desc->task_input) AGENTOS_FREE(tcb->task_desc->task_input);
            AGENTOS_FREE(tcb->task_desc);
        }
        if (tcb->result) AGENTOS_FREE(tcb->result);
        if (tcb->completed_cond) agentos_cond_destroy(tcb->completed_cond);
        if (tcb->tcb_lock) agentos_mutex_destroy(tcb->tcb_lock);
        AGENTOS_FREE(tcb);
    }
}

/**
 * @brief 深拷贝任务描�?
 */
static agentos_task_t* task_desc_deep_copy(const agentos_task_t* task) {
    agentos_task_t* copy = (agentos_task_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_t));
    if (!copy) return NULL;

    if (task->task_id) {
        copy->task_id = AGENTOS_STRDUP(task->task_id);
        if (!copy->task_id) goto fail;
    }
    if (task->task_agent_id) {
        copy->task_agent_id = AGENTOS_STRDUP(task->task_agent_id);
        if (!copy->task_agent_id) goto fail;
    }
    if (task->task_input) {
        size_t input_len = strnlen((const char*)task->task_input, 65536);
        copy->task_input = AGENTOS_MALLOC(input_len + 1);
        if (!copy->task_input) goto fail;
        memcpy(copy->task_input, task->task_input, input_len + 1);
    }
    copy->task_timeout_ms = task->task_timeout_ms;
    return copy;

fail:
    if (copy->task_id) AGENTOS_FREE(copy->task_id);
    if (copy->task_agent_id) AGENTOS_FREE(copy->task_agent_id);
    if (copy->task_input) AGENTOS_FREE(copy->task_input);
    AGENTOS_FREE(copy);
    return NULL;
}

/**
 * @brief 深拷贝任务结�?
 */
static agentos_task_t* task_result_deep_copy(const task_tcb_t* tcb) {
    agentos_task_t* result = (agentos_task_t*)AGENTOS_CALLOC(1, sizeof(agentos_task_t));
    if (!result) return NULL;

    if (tcb->task_desc->task_id) {
        result->task_id = AGENTOS_STRDUP(tcb->task_desc->task_id);
        if (!result->task_id) goto fail;
    }
    if (tcb->task_desc->task_agent_id) {
        result->task_agent_id = AGENTOS_STRDUP(tcb->task_desc->task_agent_id);
        if (!result->task_agent_id) goto fail;
    }
    if (tcb->result && tcb->result_len > 0) {
        result->task_output = AGENTOS_MALLOC(tcb->result_len);
        if (!result->task_output) goto fail;
        memcpy(result->task_output, tcb->result, tcb->result_len);
    }
    return result;

fail:
    if (result->task_id) AGENTOS_FREE(result->task_id);
    if (result->task_agent_id) AGENTOS_FREE(result->task_agent_id);
    AGENTOS_FREE(result);
    return NULL;
}

/**
 * @brief 工作线程主函�?
 */
static void* worker_thread_func(void* arg) {
    agentos_execution_engine_t* engine = (agentos_execution_engine_t*)arg;

    while (engine->running) {
        task_tcb_t* tcb = NULL;
        agentos_mutex_lock(engine->queue_lock);
        while (engine->task_queue == NULL && engine->running) {
            agentos_cond_wait(engine->task_available_cond, engine->queue_lock);
        }
        if (!engine->running) {
            agentos_mutex_unlock(engine->queue_lock);
            break;
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

        agentos_execution_unit_t* unit = agentos_registry_get_unit(tcb->task_desc->task_agent_id);
        void* output = NULL;
        size_t output_len = 0;
        agentos_error_t exec_err;
        if (unit) {
            exec_err = unit->execution_unit_execute(unit, tcb->task_desc->task_input, &output);
            if (output) {
                output_len = strlen((const char*)output);
            }
        } else {
            exec_err = AGENTOS_ENOENT;
            AGENTOS_LOG_ERROR("No execution unit found for agent %s", tcb->task_desc->task_agent_id);
        }

        agentos_mutex_lock(tcb->tcb_lock);
        tcb->end_time_ns = agentos_time_monotonic_ns();
        tcb->status = (exec_err == AGENTOS_SUCCESS) ? TASK_STATUS_SUCCEEDED : TASK_STATUS_FAILED;
        tcb->result = output;
        tcb->result_len = output_len;
        agentos_cond_signal(tcb->completed_cond);
        agentos_mutex_unlock(tcb->tcb_lock);

        agentos_mutex_lock(engine->running_lock);
        remove_tcb_from_list(&engine->running_tasks, tcb);
        engine->current_concurrency--;
        agentos_mutex_unlock(engine->running_lock);

        task_hash_table_remove(engine->task_map, tcb->task_id);
        tcb_release(tcb);
    }
    return NULL;
}

/**
 * @brief 创建执行引擎
 *
 * @param max_concurrency 最大并发任务数�?表示使用默认�?�?
 * @param out_engine 输出执行引擎指针
 * @return agentos_error_t 错误�?
 *
 * @note 执行引擎负责�?
 *       1. 接收并执行任�?
 *       2. 管理任务队列和状�?
 *       3. 提供任务查询和结果获取接�?
 *       4. 使用哈希表实现O(1)任务查找
 *
 * @warning 调用者负责释放返回的执行引擎（使�?agentos_execution_destroy�?
 */
agentos_error_t agentos_execution_create(
    uint32_t max_concurrency,
    agentos_execution_engine_t** out_engine) {

    if (!out_engine) return AGENTOS_EINVAL;
    if (max_concurrency == 0) max_concurrency = 1;

    agentos_execution_engine_t* engine = (agentos_execution_engine_t*)AGENTOS_CALLOC(1, sizeof(agentos_execution_engine_t));
    if (!engine) {
        AGENTOS_LOG_ERROR("Failed to allocate execution engine");
        return AGENTOS_ENOMEM;
    }

    engine->max_concurrency = max_concurrency;
    engine->queue_lock = agentos_mutex_create();
    engine->running_lock = agentos_mutex_create();
    engine->task_available_cond = agentos_cond_create();
    if (!engine->queue_lock || !engine->running_lock || !engine->task_available_cond) {
        if (engine->queue_lock) agentos_mutex_destroy(engine->queue_lock);
        if (engine->running_lock) agentos_mutex_destroy(engine->running_lock);
        if (engine->task_available_cond) agentos_cond_destroy(engine->task_available_cond);
        AGENTOS_FREE(engine);
        AGENTOS_LOG_ERROR("Failed to create synchronization primitives");
        return AGENTOS_ENOMEM;
    }

    engine->running = 1;
    engine->worker_count = max_concurrency;
    engine->worker_threads = (agentos_thread_t*)AGENTOS_MALLOC(engine->worker_count * sizeof(agentos_thread_t));
    if (!engine->worker_threads) {
        agentos_mutex_destroy(engine->queue_lock);
        agentos_mutex_destroy(engine->running_lock);
        agentos_cond_destroy(engine->task_available_cond);
        AGENTOS_FREE(engine);
        AGENTOS_LOG_ERROR("Failed to allocate worker threads array");
        return AGENTOS_ENOMEM;
    }

    // 创建任务哈希表，大小为最大并发数�?倍，减少冲突
    engine->task_map = task_hash_table_create(max_concurrency * 2);
    if (!engine->task_map) {
        AGENTOS_FREE(engine->worker_threads);
        agentos_mutex_destroy(engine->queue_lock);
        agentos_mutex_destroy(engine->running_lock);
        agentos_cond_destroy(engine->task_available_cond);
        AGENTOS_FREE(engine);
        AGENTOS_LOG_ERROR("Failed to create task hash table");
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < engine->worker_count; i++) {
        if (agentos_thread_create(&engine->worker_threads[i], worker_thread_func, engine) != AGENTOS_SUCCESS) {
            engine->running = 0;
            for (size_t j = 0; j < i; j++) {
                agentos_thread_join(engine->worker_threads[j], NULL);
            }
            AGENTOS_FREE(engine->worker_threads);
            agentos_mutex_destroy(engine->queue_lock);
            agentos_mutex_destroy(engine->running_lock);
            agentos_cond_destroy(engine->task_available_cond);
            AGENTOS_FREE(engine);
            AGENTOS_LOG_ERROR("Failed to create worker thread %zu", i);
            return AGENTOS_ENOMEM;
        }
    }

    *out_engine = engine;
    return AGENTOS_SUCCESS;
}

void agentos_execution_destroy(agentos_execution_engine_t* engine) {
    if (!engine) return;
    engine->running = 0;
    agentos_cond_broadcast(engine->task_available_cond);

    for (size_t i = 0; i < engine->worker_count; i++) {
        agentos_thread_join(engine->worker_threads[i], NULL);
    }
    AGENTOS_FREE(engine->worker_threads);

    // 清理等待队列
    agentos_mutex_lock(engine->queue_lock);
    task_tcb_t* tcb = engine->task_queue;
    while (tcb) {
        task_tcb_t* next = tcb->next;
        tcb_release(tcb);  // 释放引用（将递减引用计数并可能释放）
        tcb = next;
    }
    engine->task_queue = NULL;
    agentos_mutex_unlock(engine->queue_lock);

    // 清理运行中链�?
    agentos_mutex_lock(engine->running_lock);
    tcb = engine->running_tasks;
    while (tcb) {
        task_tcb_t* next = tcb->next;
        tcb_release(tcb);
        tcb = next;
    }
    engine->running_tasks = NULL;
    agentos_mutex_unlock(engine->running_lock);

    agentos_mutex_destroy(engine->queue_lock);
    agentos_mutex_destroy(engine->running_lock);
    agentos_cond_destroy(engine->task_available_cond);
    task_hash_table_destroy(engine->task_map);
    AGENTOS_FREE(engine);
}

/**
 * @brief 提交任务到执行引�?
 *
 * @param engine 执行引擎
 * @param task 任务描述
 * @param out_task_id 输出任务ID（需调用者释放）
 * @return agentos_error_t 错误�?
 *
 * @note 提交流程�?
 *       1. 参数验证
 *       2. 复制任务描述
 *       3. 创建任务控制块（TCB�?
 *       4. 生成唯一任务ID
 *       5. 初始化任务状态和资源
 *       6. 将任务添加到队列和哈希表
 *       7. 通知工作线程有新任务
 *
 * @warning 返回的任务ID需要调用者使�?AGENTOS_FREE() 释放
 */
agentos_error_t agentos_execution_submit(
    agentos_execution_engine_t* engine,
    const agentos_task_t* task,
    char** out_task_id) {

    if (!engine || !task || !out_task_id) {
        AGENTOS_LOG_ERROR("Invalid parameters to execution_submit: engine=%p, task=%p, out_task_id=%p",
                         (void*)engine, (void*)task, (void*)out_task_id);
        return AGENTOS_EINVAL;
    }

    agentos_task_t* task_copy = task_desc_deep_copy(task);
    if (!task_copy) {
        AGENTOS_LOG_ERROR("Failed to deep copy task description");
        return AGENTOS_ENOMEM;
    }

    task_tcb_t* tcb = (task_tcb_t*)AGENTOS_CALLOC(1, sizeof(task_tcb_t));
    if (!tcb) {
        AGENTOS_FREE(task_copy);
        AGENTOS_LOG_ERROR("Failed to allocate task control block: %s (code %d)",
                        agentos_error_string(AGENTOS_ENOMEM), AGENTOS_ENOMEM);
        return AGENTOS_ENOMEM;
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
                         (void*)tcb->task_id, (void*)tcb->completed_cond, (void*)tcb->tcb_lock);
        if (tcb->task_id) AGENTOS_FREE(tcb->task_id);
        if (tcb->completed_cond) agentos_cond_destroy(tcb->completed_cond);
        if (tcb->tcb_lock) agentos_mutex_destroy(tcb->tcb_lock);
        AGENTOS_FREE(tcb);
        AGENTOS_FREE(task_copy);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(engine->queue_lock);
    tcb->next = engine->task_queue;
    engine->task_queue = tcb;
    agentos_cond_signal(engine->task_available_cond);
    agentos_mutex_unlock(engine->queue_lock);

    // 将任务插入到哈希表中，便于快速查�?
    task_hash_table_insert(engine->task_map, tcb);

    *out_task_id = AGENTOS_STRDUP(tcb->task_id);
    if (!*out_task_id) {
        AGENTOS_LOG_ERROR("Failed to duplicate task_id");
        // 任务已入队，无法回滚，返回错�?
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_execution_query(
    agentos_execution_engine_t* engine,
    const char* task_id,
    agentos_task_status_t* out_status) {

    if (!engine || !task_id || !out_status) return AGENTOS_EINVAL;

    // 使用哈希表快速查找任�?
    task_tcb_t* tcb = task_hash_table_find(engine->task_map, task_id);
    if (tcb) {
        agentos_mutex_lock(tcb->tcb_lock);
        *out_status = tcb->status;
        agentos_mutex_unlock(tcb->tcb_lock);
        return AGENTOS_SUCCESS;
    }

    return AGENTOS_ENOENT;
}

agentos_error_t agentos_execution_wait(
    agentos_execution_engine_t* engine,
    const char* task_id,
    uint32_t timeout_ms,
    agentos_task_t** out_result) {

    if (!engine || !task_id) return AGENTOS_EINVAL;

    // 使用哈希表快速查找任�?
    task_tcb_t* tcb = task_hash_table_find(engine->task_map, task_id);
    if (!tcb) return AGENTOS_ENOENT;

    tcb_retain(tcb);  // 增加引用，防止等待期间被释放

    agentos_cond_t* cond = tcb->completed_cond;
    agentos_mutex_lock(tcb->tcb_lock);
    while (tcb->status == TASK_STATUS_PENDING || tcb->status == TASK_STATUS_RUNNING) {
        if (timeout_ms == 0) {
            agentos_cond_wait(cond, tcb->tcb_lock);
        } else {
            int err = agentos_cond_timedwait(cond, tcb->tcb_lock, timeout_ms);
            if (err != 0) {
                agentos_mutex_unlock(tcb->tcb_lock);
                tcb_release(tcb);
                return AGENTOS_ETIMEDOUT;
            }
        }
    }
    agentos_mutex_unlock(tcb->tcb_lock);

    if (out_result) {
        agentos_task_t* result_copy = task_result_deep_copy(tcb);
        if (!result_copy) {
            tcb_release(tcb);
            return AGENTOS_ENOMEM;
        }
        *out_result = result_copy;
    }

    tcb_release(tcb);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_execution_cancel(
    agentos_execution_engine_t* engine,
    const char* task_id) {

    if (!engine || !task_id) return AGENTOS_EINVAL;

    // 使用哈希表快速查找任�?
    task_tcb_t* tcb = task_hash_table_find(engine->task_map, task_id);
    if (!tcb) return AGENTOS_ENOENT;

    // 检查任务是否在队列�?
    agentos_mutex_lock(engine->queue_lock);
    task_tcb_t** p = &engine->task_queue;
    while (*p) {
        if (*p == tcb) {
            *p = tcb->next;
            tcb->next = NULL;
            agentos_mutex_unlock(engine->queue_lock);

            // 从哈希表中移除任�?
            task_hash_table_remove(engine->task_map, task_id);

            tcb_retain(tcb);
            agentos_mutex_lock(tcb->tcb_lock);
            tcb->status = TASK_STATUS_CANCELLED;
            tcb->end_time_ns = agentos_time_monotonic_ns();
            agentos_cond_signal(tcb->completed_cond);
            agentos_mutex_unlock(tcb->tcb_lock);
            tcb_release(tcb);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(engine->queue_lock);
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_execution_get_result(
    agentos_execution_engine_t* engine,
    const char* task_id,
    agentos_task_t** out_result) {

    if (!engine || !task_id || !out_result) return AGENTOS_EINVAL;

    // 使用哈希表快速查找任�?
    task_tcb_t* tcb = task_hash_table_find(engine->task_map, task_id);
    if (!tcb) return AGENTOS_ENOENT;

    tcb_retain(tcb);

    agentos_mutex_lock(tcb->tcb_lock);
    if (tcb->status != TASK_STATUS_SUCCEEDED && tcb->status != TASK_STATUS_FAILED) {
        agentos_mutex_unlock(tcb->tcb_lock);
        tcb_release(tcb);
        return AGENTOS_EBUSY;
    }
    agentos_mutex_unlock(tcb->tcb_lock);

    agentos_task_t* result_copy = task_result_deep_copy(tcb);
    if (!result_copy) {
        tcb_release(tcb);
        return AGENTOS_ENOMEM;
    }
    *out_result = result_copy;

    tcb_release(tcb);
    return AGENTOS_SUCCESS;
}

void agentos_task_free(agentos_task_t* task) {
    if (!task) return;
    if (task->task_id) AGENTOS_FREE(task->task_id);
    if (task->task_agent_id) AGENTOS_FREE(task->task_agent_id);
    if (task->task_input) AGENTOS_FREE(task->task_input);
    if (task->task_output) AGENTOS_FREE(task->task_output);
    AGENTOS_FREE(task);
}

agentos_error_t agentos_execution_health_check(
    agentos_execution_engine_t* engine,
    char** out_json) {

    if (!engine || !out_json) return AGENTOS_EINVAL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;

    agentos_mutex_lock(engine->queue_lock);
    size_t queue_len = 0;
    task_tcb_t* t = engine->task_queue;
    while (t) { queue_len++; t = t->next; }
    agentos_mutex_unlock(engine->queue_lock);

    agentos_mutex_lock(engine->running_lock);
    size_t running_len = 0;
    t = engine->running_tasks;
    while (t) { running_len++; t = t->next; }
    uint32_t cur = engine->current_concurrency;
    agentos_mutex_unlock(engine->running_lock);

    cJSON_AddStringToObject(root, "status", "healthy");
    cJSON_AddNumberToObject(root, "task_queue_length", queue_len);
    cJSON_AddNumberToObject(root, "running_tasks", running_len);
    cJSON_AddNumberToObject(root, "current_concurrency", cur);
    cJSON_AddNumberToObject(root, "max_concurrency", engine->max_concurrency);
    cJSON_AddNumberToObject(root, "running", engine->running);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return AGENTOS_ENOMEM;

    *out_json = json;
    return AGENTOS_SUCCESS;
}
