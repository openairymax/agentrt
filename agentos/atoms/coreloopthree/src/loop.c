/**
 * @file loop.c
 * @brief 三层核心运行时主循环实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "loop.h"

#include "agentos.h"
#include "atomic_compat.h"
#include "check.h"
#include "checkpoint.h"
#include "cognition.h"
#include "execution.h"
#include "memory.h"
#include "memory_common.h"
#include "memory_compat.h"
#include "platform.h"
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "agentos_dirent.h"
#include <sys/stat.h>

/**
 * @brief 核心循环结构体
 */
struct agentos_core_loop {
    agentos_cognition_engine_t *cognition;
    agentos_execution_engine_t *execution;
    agentos_memory_engine_t *memory;
    agentos_loop_config_t manager;
    volatile int running;
    volatile int stop_requested;
    volatile int task_pending;
    agentos_mutex_t *lock;
    agentos_cond_t *cond;

    int checkpoint_initialized;
    uint64_t last_checkpoint_time_ms;
    uint64_t checkpoint_seq;
    char current_task_id[128];
    char current_session_id[128];
    char **completed_node_ids;
    size_t completed_node_count;
    size_t completed_node_capacity;
    char *persistent_original_input;
};

/* 辅助函数声明 - 用于重构降低圈复杂度 */
static agentos_error_t validate_loop_parameters(const agentos_loop_config_t *manager,
                                                agentos_core_loop_t **out_loop);
static agentos_core_loop_t *allocate_loop_memory(void);
static agentos_error_t initialize_loop_resources(agentos_core_loop_t *loop,
                                                 const agentos_loop_config_t *manager);
static agentos_error_t create_loop_engines(agentos_core_loop_t *loop);
static void cleanup_loop_resources(agentos_core_loop_t *loop);
static void free_loop_memory(agentos_core_loop_t *loop);

/* 提取的辅助函数 - 降低 agentos_loop_submit 圈复杂度 */
static char *build_enhanced_input(const char *input, size_t input_len,
                                  agentos_memory_record_t **records, size_t record_count,
                                  size_t max_memories);
static void free_memories(agentos_memory_record_t **records, size_t record_count);
static void loop_checkpoint_auto_hook(const char *task_id, const char *state_json, void *user_data);
static void add_completed_node(agentos_core_loop_t *loop, const char *node_id);
static void clear_completed_nodes(agentos_core_loop_t *loop);
static agentos_error_t save_incremental_checkpoint(agentos_core_loop_t *loop, const char *task_id,
                                                    const char *session_id, const char *node_id);

/* 默认配置 */
static void init_default_config(agentos_loop_config_t *manager)
{
    memset(manager, 0, sizeof(agentos_loop_config_t));
    manager->loop_config_cognition_threads = 4;
    manager->loop_config_execution_threads = 8;
    manager->loop_config_memory_threads = 2;
    manager->loop_config_max_queued_tasks = 1000;
    manager->loop_config_stats_interval_ms = 60000;
    manager->loop_config_memory_query_limit = 5;
    manager->loop_config_task_timeout_ms = 30000;
    manager->loop_config_memory_importance = 0.7f;
    manager->loop_config_checkpoint_enabled = 0;
    snprintf(manager->loop_config_checkpoint_path, sizeof(manager->loop_config_checkpoint_path),
             "./data/checkpoints");
    manager->loop_config_checkpoint_interval_ms = 30000;
}

/* ==================== 辅助函数实现 - 用于降低圈复杂度 ==================== */

/**
 * @brief 验证循环创建参数
 * @param manager 配置参数指针（可为 NULL）
 * @param out_loop 输出循环指针
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t validate_loop_parameters(const agentos_loop_config_t *manager,
                                                agentos_core_loop_t **out_loop)
{
    CHECK_NULL(out_loop);

    if (manager) {
        if (manager->loop_config_cognition_threads > 1024 ||
            manager->loop_config_execution_threads > 1024 ||
            manager->loop_config_memory_threads > 1024) {
            return AGENTOS_EINVAL;
        }

        if (manager->loop_config_max_queued_tasks == 0 ||
            manager->loop_config_max_queued_tasks > 100000) {
            return AGENTOS_EINVAL;
        }

        if (manager->loop_config_stats_interval_ms > 3600000) {
            return AGENTOS_EINVAL;
        }
    }

    return AGENTOS_SUCCESS;
}

static agentos_core_loop_t *allocate_loop_memory(void)
{
    agentos_core_loop_t *loop =
        (agentos_core_loop_t *)AGENTOS_CALLOC(1, sizeof(agentos_core_loop_t));
    if (loop) {
        memset(loop, 0, sizeof(agentos_core_loop_t));
    }
    return loop;
}

/**
 * @brief 初始化循环资源（互斥锁和条件变量）
 * @param loop 循环结构体指针
 * @param manager 配置参数指针（可为 NULL）
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t initialize_loop_resources(agentos_core_loop_t *loop,
                                                 const agentos_loop_config_t *manager)
{
    if (manager) {
        memcpy(&loop->manager, manager, sizeof(agentos_loop_config_t));
    } else {
        init_default_config(&loop->manager);
    }

    loop->lock = agentos_mutex_create();
    if (!loop->lock)
        return AGENTOS_ENOMEM;

    loop->cond = agentos_cond_create();
    if (!loop->cond) {
        agentos_mutex_free(loop->lock);
        return AGENTOS_ENOMEM;
    }

    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建循环引擎（认知、执行、记忆）
 * @param loop 循环结构体指针
 * @return 错误码，成功返回 AGENTOS_SUCCESS
 */
static agentos_error_t create_loop_engines(agentos_core_loop_t *loop)
{
    agentos_error_t err;

    err = agentos_cognition_create_ex(NULL, loop->manager.loop_config_plan_strategy,
                                      loop->manager.loop_config_coord_strategy,
                                      loop->manager.loop_config_disp_strategy, &loop->cognition);

    if (err != AGENTOS_SUCCESS)
        return err;

    err = agentos_execution_create(loop->manager.loop_config_execution_threads > 0
                                       ? loop->manager.loop_config_execution_threads
                                       : 8,
                                   &loop->execution);

    if (err != AGENTOS_SUCCESS)
        return err;

    err = agentos_memory_create(NULL, &loop->memory);
    if (err != AGENTOS_SUCCESS)
        return err;

    agentos_cognition_set_memory(loop->cognition, loop->memory);

    return AGENTOS_SUCCESS;
}

/**
 * @brief 释放循环结构体内存（支持内存池和传统分配）
 * @param loop 循环结构体指针
 */
static void free_loop_memory(agentos_core_loop_t *loop)
{
    if (loop == NULL) {
        return;
    }

    AGENTOS_FREE(loop);
}

/**
 * @brief 清理循环资源（反向释放所有资源）
 * @param loop 循环结构体指针
 */
static void cleanup_loop_resources(agentos_core_loop_t *loop)
{
    if (!loop)
        return;

    if (loop->memory) {
        agentos_memory_destroy(loop->memory);
        loop->memory = NULL;
    }

    if (loop->execution) {
        agentos_execution_destroy(loop->execution);
        loop->execution = NULL;
    }

    if (loop->cognition) {
        agentos_cognition_destroy(loop->cognition);
        loop->cognition = NULL;
    }

    if (loop->cond) {
        agentos_cond_free(loop->cond);
        loop->cond = NULL;
    }

    if (loop->lock) {
        agentos_mutex_free(loop->lock);
        loop->lock = NULL;
    }

    free_loop_memory(loop);
}

/**
 * @brief 释放记忆结果数组
 * @param memories 记忆数组指针
 * @param memory_count 记忆数量
 */
static void free_memories(agentos_memory_record_t **records, size_t record_count)
{
    if (!records)
        return;
    AGENTOS_FREE(records);
}

/**
 * @brief 构建增强输入（带记忆上下文）
 * @param input 原始输入
 * @param input_len 原始输入长度
 * @param memories 记忆数组
 * @param memory_count 记忆数量
 * @return 增强后的输入字符串，需调用者释放；失败返回NULL
 */
static char *build_enhanced_input(const char *input, size_t input_len,
                                  agentos_memory_record_t **records, size_t record_count,
                                  size_t max_memories)
{
    if (!input || record_count == 0 || !records)
        return NULL;

    size_t total_len = input_len + 1024;
    for (size_t i = 0; i < record_count; i++) {
        if (records[i] && records[i]->memory_record_data) {
            total_len += records[i]->memory_record_data_len + 64;
        }
    }

    char *enhanced_input = (char *)AGENTOS_MALLOC(total_len);
    if (!enhanced_input)
        return NULL;

    size_t pos = 0;
    pos += snprintf(enhanced_input + pos, total_len - pos, "[上下文增强]\n相关记忆数量：%zu\n\n",
                    record_count);

    for (size_t i = 0; i < record_count && i < max_memories; i++) {
        if (records[i] && records[i]->memory_record_data) {
            pos += snprintf(enhanced_input + pos, total_len - pos, "记忆 %zu: %.*s\n", i + 1,
                            (int)records[i]->memory_record_data_len,
                            (const char *)records[i]->memory_record_data);
        }
    }

    pos += snprintf(enhanced_input + pos, total_len - pos, "\n[用户输入]\n%.*s", (int)input_len,
                    input);

    return enhanced_input;
}

/* ==================== 公共 API 函数实现 ==================== */

AGENTOS_API agentos_error_t agentos_loop_create(const agentos_loop_config_t *manager,
                                                agentos_core_loop_t **out_loop)
{
    agentos_error_t err;
    agentos_core_loop_t *loop = NULL;

    err = validate_loop_parameters(manager, out_loop);
    if (err != AGENTOS_SUCCESS)
        return err;

    loop = allocate_loop_memory();
    if (!loop)
        return AGENTOS_ENOMEM;

    err = initialize_loop_resources(loop, manager);
    if (err != AGENTOS_SUCCESS) {
        cleanup_loop_resources(loop);
        return err;
    }

    err = create_loop_engines(loop);
    if (err != AGENTOS_SUCCESS) {
        cleanup_loop_resources(loop);
        return err;
    }

    loop->running = 0;
    loop->stop_requested = 0;
    loop->checkpoint_initialized = 0;
    loop->last_checkpoint_time_ms = 0;
    loop->checkpoint_seq = 0;
    memset(loop->current_task_id, 0, sizeof(loop->current_task_id));
    memset(loop->current_session_id, 0, sizeof(loop->current_session_id));
    loop->completed_node_ids = NULL;
    loop->completed_node_count = 0;
    loop->completed_node_capacity = 0;
    loop->persistent_original_input = NULL;

    if (loop->manager.loop_config_checkpoint_enabled) {
        const char *cp_path = loop->manager.loop_config_checkpoint_path;
        if (cp_path[0] == '\0') {
            cp_path = "./data/checkpoints";
        }
        agentos_error_t cp_err = agentos_checkpoint_init(cp_path);
        if (cp_err == AGENTOS_SUCCESS) {
            loop->checkpoint_initialized = 1;
            agentos_checkpoint_set_auto_hook(loop_checkpoint_auto_hook, loop,
                                             loop->manager.loop_config_checkpoint_interval_ms);
            AGENTOS_LOG_INFO("Checkpoint subsystem initialized: %s", cp_path);
        } else {
            AGENTOS_LOG_WARN("Checkpoint init failed (err=%d), persistence disabled", cp_err);
        }
    }

    *out_loop = loop;
    return AGENTOS_SUCCESS;
}

AGENTOS_API void agentos_loop_destroy(agentos_core_loop_t *loop)
{
    if (!loop)
        return;

    if (loop->running) {
        agentos_loop_stop(loop);
    }

    if (loop->checkpoint_initialized) {
        agentos_checkpoint_cleanup(86400, 100);
        agentos_checkpoint_shutdown();
        loop->checkpoint_initialized = 0;
    }

    clear_completed_nodes(loop);

    if (loop->persistent_original_input) {
        AGENTOS_FREE(loop->persistent_original_input);
        loop->persistent_original_input = NULL;
    }

    if (loop->memory) {
        agentos_memory_destroy(loop->memory);
        loop->memory = NULL;
    }
    if (loop->execution) {
        agentos_execution_destroy(loop->execution);
        loop->execution = NULL;
    }
    if (loop->cognition) {
        agentos_cognition_destroy(loop->cognition);
        loop->cognition = NULL;
    }
    if (loop->cond) {
        agentos_cond_free(loop->cond);
        loop->cond = NULL;
    }
    if (loop->lock) {
        agentos_mutex_free(loop->lock);
        loop->lock = NULL;
    }

    free_loop_memory(loop);
}

AGENTOS_API agentos_error_t agentos_loop_run(agentos_core_loop_t *loop)
{
    CHECK_NULL(loop);

    agentos_mutex_lock(loop->lock);
    loop->running = 1;
    loop->stop_requested = 0;
    agentos_mutex_unlock(loop->lock);

    uint64_t last_auto_checkpoint_ms = 0;
    uint32_t checkpoint_interval = loop->manager.loop_config_checkpoint_interval_ms;
    if (checkpoint_interval == 0)
        checkpoint_interval = 30000;

    while (1) {
        agentos_mutex_lock(loop->lock);
        while (!loop->stop_requested && !loop->task_pending) {
            agentos_cond_timedwait(loop->cond, loop->lock, 50);
        }
        if (loop->stop_requested) {
            loop->running = 0;
            loop->task_pending = 0;
            agentos_cond_broadcast(loop->cond);
            agentos_mutex_unlock(loop->lock);
            break;
        }
        if (loop->task_pending) {
            loop->task_pending = 0;
        }
        agentos_mutex_unlock(loop->lock);

        if (loop->checkpoint_initialized && loop->current_task_id[0] != '\0' &&
            checkpoint_interval > 0) {
            uint64_t now_ms = agentos_time_ms();
            if (last_auto_checkpoint_ms == 0 ||
                (now_ms - last_auto_checkpoint_ms) >= checkpoint_interval) {
                agentos_error_t cp_err = agentos_checkpoint_trigger_auto(loop->current_task_id);
                if (cp_err == AGENTOS_SUCCESS) {
                    last_auto_checkpoint_ms = now_ms;
                }
            }
        }
    }

    return AGENTOS_SUCCESS;
}

AGENTOS_API void agentos_loop_stop(agentos_core_loop_t *loop)
{
    if (!loop)
        return;

    agentos_mutex_lock(loop->lock);
    loop->stop_requested = 1;
    while (loop->running) {
        agentos_cond_wait(loop->cond, loop->lock);
    }
    agentos_mutex_unlock(loop->lock);
}

AGENTOS_API agentos_error_t agentos_loop_submit(agentos_core_loop_t *loop, const char *input,
                                                size_t input_len, char **out_task_id)
{
    if (!loop || !input || !out_task_id)
        return AGENTOS_EINVAL;
    if (!loop->cognition || !loop->execution || !loop->memory)
        return AGENTOS_ENOTINIT;

    /* 步骤 1: 从记忆中检索相关上下文 */
    agentos_memory_query_t query = {0};
    query.memory_query_text = (char *)input;
    query.memory_query_text_len = input_len;
    query.memory_query_limit = 5;
    query.memory_query_include_raw = 1;
    agentos_memory_result_ext_t *result = NULL;
    agentos_error_t err = agentos_memory_query(loop->memory, &query, &result);

    size_t memory_count = 0;
    agentos_memory_record_t **memories = NULL;
    if (err == AGENTOS_SUCCESS && result && result->memory_result_count > 0) {
        memory_count = result->memory_result_count;
        memories = (agentos_memory_record_t **)AGENTOS_CALLOC(memory_count,
                                                              sizeof(agentos_memory_record_t *));
        if (memories) {
            for (size_t i = 0; i < memory_count; i++) {
                memories[i] = result->memory_result_items[i]->memory_result_item_record;
            }
        } else {
            memory_count = 0;
        }
    }

    /* 步骤 2: 构建增强输入（如果有相关记忆） */
    char *enhanced_input = NULL;
    if (err == AGENTOS_SUCCESS && memory_count > 0) {
        enhanced_input = build_enhanced_input(input, input_len, memories, memory_count,
                                              loop->manager.loop_config_memory_query_limit);
    }

    /* 释放记忆结果（无论是否构建了增强输入） */
    if (result)
        agentos_memory_result_free(result);
    free_memories(memories, memory_count);

    /* 步骤 3: 认知层处理（带上下文增强） */
    const char *process_input = enhanced_input ? enhanced_input : input;
    size_t process_len = enhanced_input ? strlen(enhanced_input) : input_len;

    agentos_task_plan_t *plan = NULL;
    err = agentos_cognition_process(loop->cognition, process_input, process_len, &plan);
    if (err != AGENTOS_SUCCESS) {
        if (enhanced_input)
            AGENTOS_FREE(enhanced_input);
        return err;
    }

    if (!plan || plan->task_plan_node_count == 0) {
        agentos_task_plan_free(plan);
        if (enhanced_input)
            AGENTOS_FREE(enhanced_input);
        return AGENTOS_EINVAL;
    }

    /* 步骤 4: 执行层按计划节点提交任务 */
    agentos_error_t first_err = AGENTOS_SUCCESS;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentos_task_node_t *node = plan->task_plan_nodes[i];
        if (!node)
            continue;

        agentos_task_t task;
        memset(&task, 0, sizeof(task));
        task.task_input = node->task_node_input ? node->task_node_input : (void *)process_input;
        task.task_timeout_ms = node->task_node_timeout_ms > 0
                                   ? node->task_node_timeout_ms
                                   : loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentos_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTOS_SUCCESS && first_err == AGENTOS_SUCCESS) {
            first_err = err;
        }
        if (node_task_id)
            AGENTOS_FREE(node_task_id);
    }

    if (first_err == AGENTOS_SUCCESS && out_task_id) {
        *out_task_id = NULL;
    }

    if (first_err == AGENTOS_SUCCESS) {
        agentos_mutex_lock(loop->lock);
        loop->task_pending = 1;
        agentos_cond_signal(loop->cond);
        agentos_mutex_unlock(loop->lock);
    }

    /* 清理临时资源 */
    agentos_task_plan_free(plan);
    if (enhanced_input)
        AGENTOS_FREE(enhanced_input);

    return first_err;
}

AGENTOS_API agentos_error_t agentos_loop_wait(agentos_core_loop_t *loop, const char *task_id,
                                              uint32_t timeout_ms, char **out_result,
                                              size_t *out_result_len)
{
    if (!loop || !task_id || !out_result || !out_result_len)
        return AGENTOS_EINVAL;
    if (!loop->execution || !loop->memory)
        return AGENTOS_ENOTINIT;

    /* 等待执行完成 */
    agentos_task_t *result_task = NULL;
    agentos_error_t err =
        agentos_execution_wait(loop->execution, task_id, timeout_ms, &result_task);

    if (err == AGENTOS_SUCCESS && result_task) {
        if (result_task->task_output) {
            size_t len = 0;
            const char *output = (const char *)result_task->task_output;
            while (output[len] != '\0')
                len++;
            *out_result = AGENTOS_STRDUP(output);
            *out_result_len = len;
        } else {
            *out_result = AGENTOS_STRDUP("");
            *out_result_len = 0;
        }

        if (!*out_result) {
            agentos_task_free(result_task);
            return AGENTOS_ENOMEM;
        }

        if (*out_result_len > 0) {
            agentos_memory_record_t record = {0};
            record.memory_record_data = *out_result;
            record.memory_record_data_len = *out_result_len;
            record.memory_record_type = AGENTOS_MEMTYPE_TEXT;
            record.memory_record_importance = loop->manager.loop_config_memory_importance;
            char *new_record_id = NULL;
            agentos_error_t store_err = agentos_memory_write(loop->memory, &record, &new_record_id);
            if (new_record_id)
                AGENTOS_FREE(new_record_id);
            if (store_err != AGENTOS_SUCCESS) {
                AGENTOS_LOG_WARN("Failed to store execution result to memory: %d", store_err);
            } else {
                AGENTOS_LOG_INFO("Successfully stored execution result to memory");
            }
        }

        agentos_task_free(result_task);

        if (!*out_result)
            return AGENTOS_ENOMEM;
    }

    return err;
}

AGENTOS_API void agentos_loop_get_engines(agentos_core_loop_t *loop,
                                          agentos_cognition_engine_t **out_cognition,
                                          agentos_execution_engine_t **out_execution,
                                          agentos_memory_engine_t **out_memory)
{
    if (!loop)
        return;

    if (out_cognition)
        *out_cognition = loop->cognition;
    if (out_execution)
        *out_execution = loop->execution;
    if (out_memory)
        *out_memory = loop->memory;
}

static uint64_t get_time_ms(void)
{
    return agentos_time_ms();
}

static void generate_task_id(char *buf, size_t buf_size)
{
    snprintf(buf, buf_size, "task-%016lx", (unsigned long)(agentos_time_ns() & 0xFFFFFFFF));
}

static void loop_checkpoint_auto_hook(const char *task_id, const char *state_json, void *user_data)
{
    agentos_core_loop_t *loop = (agentos_core_loop_t *)user_data;
    if (!loop || !loop->checkpoint_initialized || !task_id)
        return;

    char state[8192];
    if (state_json && state_json[0]) {
        snprintf(state, sizeof(state), "%s", state_json);
    } else {
        int written = snprintf(state, sizeof(state),
                               "{\"type\":\"auto\",\"task_id\":\"%s\","
                               "\"session_id\":\"%s\","
                               "\"completed_nodes\":%zu,"
                               "\"checkpoint_seq\":%lu}",
                               task_id,
                               loop->current_session_id,
                               loop->completed_node_count,
                               (unsigned long)loop->checkpoint_seq);
        if (written <= 0 || (size_t)written >= sizeof(state)) {
            snprintf(state, sizeof(state),
                     "{\"state\":\"auto\",\"task_id\":\"%s\",\"session_id\":\"%s\"}", task_id,
                     loop->current_session_id);
        }
    }

    loop->checkpoint_seq++;
    agentos_task_checkpoint_t *checkpoint = NULL;
    agentos_error_t err =
        agentos_checkpoint_create(task_id, loop->current_session_id, loop->checkpoint_seq, state,
                                  loop->completed_node_ids, loop->completed_node_count,
                                  NULL, 0, &checkpoint);
    if (err == AGENTOS_SUCCESS && checkpoint) {
        err = agentos_checkpoint_save(checkpoint);
        if (err == AGENTOS_SUCCESS) {
            AGENTOS_LOG_INFO("Auto-checkpoint saved for task %s (seq=%lu, completed=%zu)", task_id,
                             (unsigned long)loop->checkpoint_seq, loop->completed_node_count);
        }
        agentos_checkpoint_destroy(checkpoint);
    }
    loop->last_checkpoint_time_ms = get_time_ms();
}

static agentos_error_t save_plan_checkpoint(agentos_core_loop_t *loop,
                                            const agentos_task_plan_t *plan, const char *task_id,
                                            const char *session_id, const char *original_input)
{
    if (!loop->checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!plan || !task_id)
        return AGENTOS_EINVAL;

    size_t completed_count = 0;
    size_t pending_count = plan->task_plan_node_count;
    char **completed_nodes = NULL;
    char **pending_nodes = NULL;

    if (pending_count > 0) {
        pending_nodes = (char **)AGENTOS_CALLOC(pending_count, sizeof(char *));
        if (!pending_nodes)
            return AGENTOS_ENOMEM;
        for (size_t i = 0; i < pending_count; i++) {
            if (plan->task_plan_nodes[i] && plan->task_plan_nodes[i]->task_node_id) {
                pending_nodes[i] = AGENTOS_STRDUP(plan->task_plan_nodes[i]->task_node_id);
            }
        }
    }

    char state_json[8192];
    const char *safe_input = original_input ? original_input : "";
    int json_len = snprintf(state_json, sizeof(state_json),
                            "{\"plan_id\":\"%s\",\"node_count\":%zu,\"session_id\":\"%s\","
                            "\"original_input\":\"%.4096s\"}",
                            plan->task_plan_id ? plan->task_plan_id : "",
                            plan->task_plan_node_count, session_id ? session_id : "", safe_input);

    if (json_len < 0 || json_len >= (int)sizeof(state_json)) {
        if (pending_nodes) {
            for (size_t i = 0; i < pending_count; i++)
                AGENTOS_FREE(pending_nodes[i]);
            AGENTOS_FREE(pending_nodes);
        }
        return AGENTOS_EOVERFLOW;
    }

    loop->checkpoint_seq++;
    agentos_task_checkpoint_t *checkpoint = NULL;
    agentos_error_t err = agentos_checkpoint_create(
        task_id, session_id ? session_id : "default", loop->checkpoint_seq, state_json,
        completed_nodes, completed_count, pending_nodes, pending_count, &checkpoint);

    if (pending_nodes) {
        for (size_t i = 0; i < pending_count; i++)
            AGENTOS_FREE(pending_nodes[i]);
        AGENTOS_FREE(pending_nodes);
    }

    if (err != AGENTOS_SUCCESS || !checkpoint) {
        AGENTOS_LOG_WARN("Failed to create checkpoint for task %s: %d", task_id, err);
        return err;
    }

    err = agentos_checkpoint_save(checkpoint);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Failed to save checkpoint for task %s: %d", task_id, err);
    } else {
        AGENTOS_LOG_INFO("Checkpoint saved for task %s (seq=%lu)", task_id,
                         (unsigned long)loop->checkpoint_seq);
    }

    agentos_checkpoint_destroy(checkpoint);
    loop->last_checkpoint_time_ms = get_time_ms();
    return err;
}

AGENTOS_API agentos_error_t agentos_loop_submit_persistent(agentos_core_loop_t *loop,
                                                           const char *input, size_t input_len,
                                                           const char *session_id,
                                                           char **out_task_id)
{
    if (!loop || !input || !out_task_id)
        return AGENTOS_EINVAL;
    if (!loop->cognition || !loop->execution || !loop->memory)
        return AGENTOS_ENOTINIT;

    char task_id_buf[128];
    generate_task_id(task_id_buf, sizeof(task_id_buf));

    agentos_mutex_lock(loop->lock);
    snprintf(loop->current_task_id, sizeof(loop->current_task_id), "%s", task_id_buf);
    if (session_id) {
        snprintf(loop->current_session_id, sizeof(loop->current_session_id), "%s", session_id);
    } else {
        snprintf(loop->current_session_id, sizeof(loop->current_session_id), "sess-%016lx",
                 (unsigned long)(agentos_time_ns() & 0xFFFFFFFF));
    }
    loop->checkpoint_seq = 0;
    clear_completed_nodes(loop);
    agentos_mutex_unlock(loop->lock);

    agentos_memory_query_t query = {0};
    query.memory_query_text = (char *)input;
    query.memory_query_text_len = input_len;
    query.memory_query_limit = 5;
    query.memory_query_include_raw = 1;
    agentos_memory_result_ext_t *result = NULL;
    agentos_error_t err = agentos_memory_query(loop->memory, &query, &result);

    size_t memory_count = 0;
    agentos_memory_record_t **memories = NULL;
    if (err == AGENTOS_SUCCESS && result && result->memory_result_count > 0) {
        memory_count = result->memory_result_count;
        memories = (agentos_memory_record_t **)AGENTOS_CALLOC(memory_count,
                                                              sizeof(agentos_memory_record_t *));
        if (memories) {
            for (size_t i = 0; i < memory_count; i++) {
                memories[i] = result->memory_result_items[i]->memory_result_item_record;
            }
        } else {
            memory_count = 0;
        }
    }

    char *enhanced_input = NULL;
    if (err == AGENTOS_SUCCESS && memory_count > 0) {
        enhanced_input = build_enhanced_input(input, input_len, memories, memory_count,
                                              loop->manager.loop_config_memory_query_limit);
    }

    if (result)
        agentos_memory_result_free(result);
    free_memories(memories, memory_count);

    const char *process_input = enhanced_input ? enhanced_input : input;
    size_t process_len = enhanced_input ? strlen(enhanced_input) : input_len;

    agentos_task_plan_t *plan = NULL;
    err = agentos_cognition_process(loop->cognition, process_input, process_len, &plan);
    if (err != AGENTOS_SUCCESS) {
        if (enhanced_input)
            AGENTOS_FREE(enhanced_input);
        return err;
    }

    if (!plan || plan->task_plan_node_count == 0) {
        agentos_task_plan_free(plan);
        if (enhanced_input)
            AGENTOS_FREE(enhanced_input);
        return AGENTOS_EINVAL;
    }

    if (loop->checkpoint_initialized) {
        save_plan_checkpoint(loop, plan, task_id_buf, loop->current_session_id, process_input);

        if (loop->persistent_original_input) {
            AGENTOS_FREE(loop->persistent_original_input);
        }
        loop->persistent_original_input = AGENTOS_STRDUP(process_input);
    }

    agentos_error_t first_err = AGENTOS_SUCCESS;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentos_task_node_t *node = plan->task_plan_nodes[i];
        if (!node)
            continue;

        agentos_task_t task;
        memset(&task, 0, sizeof(task));
        task.task_input = node->task_node_input ? node->task_node_input : (void *)process_input;
        task.task_timeout_ms = node->task_node_timeout_ms > 0
                                   ? node->task_node_timeout_ms
                                   : loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentos_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTOS_SUCCESS && first_err == AGENTOS_SUCCESS) {
            first_err = err;
        }
        if (node_task_id)
            AGENTOS_FREE(node_task_id);

        if (err == AGENTOS_SUCCESS && loop->checkpoint_initialized &&
            node->task_node_id) {
            add_completed_node(loop, node->task_node_id);
            save_incremental_checkpoint(loop, task_id_buf, loop->current_session_id,
                                        node->task_node_id);
        }
    }

    if (first_err == AGENTOS_SUCCESS) {
        *out_task_id = AGENTOS_STRDUP(task_id_buf);
        if (!*out_task_id)
            first_err = AGENTOS_ENOMEM;

        agentos_mutex_lock(loop->lock);
        loop->task_pending = 1;
        agentos_cond_signal(loop->cond);
        agentos_mutex_unlock(loop->lock);
    }

    agentos_task_plan_free(plan);
    if (enhanced_input)
        AGENTOS_FREE(enhanced_input);

    return first_err;
}

AGENTOS_API agentos_error_t agentos_loop_restore_task(agentos_core_loop_t *loop,
                                                      const char *task_id,
                                                      char **out_restored_task_id)
{
    if (!loop || !task_id || !out_restored_task_id)
        return AGENTOS_EINVAL;
    if (!loop->checkpoint_initialized)
        return AGENTOS_ENOTINIT;
    if (!loop->cognition || !loop->execution)
        return AGENTOS_ENOTINIT;

    agentos_task_checkpoint_t **checkpoints = NULL;
    size_t cp_count = 0;
    agentos_error_t err = agentos_checkpoint_list(task_id, &checkpoints, &cp_count);
    if (err != AGENTOS_SUCCESS || cp_count == 0) {
        if (checkpoints) {
            for (size_t i = 0; i < cp_count; i++) {
                agentos_checkpoint_destroy(checkpoints[i]);
            }
            AGENTOS_FREE(checkpoints);
        }
        return AGENTOS_ENOENT;
    }

    agentos_task_checkpoint_t *latest = NULL;
    uint64_t latest_seq = 0;
    for (size_t i = 0; i < cp_count; i++) {
        if (checkpoints[i] && checkpoints[i]->sequence_num > latest_seq) {
            latest_seq = checkpoints[i]->sequence_num;
            latest = checkpoints[i];
        }
    }

    if (!latest || !latest->state_json) {
        for (size_t i = 0; i < cp_count; i++) {
            agentos_checkpoint_destroy(checkpoints[i]);
        }
        AGENTOS_FREE(checkpoints);
        return AGENTOS_ENOENT;
    }

    bool is_valid = false;
    agentos_checkpoint_verify(latest, &is_valid);
    if (!is_valid) {
        AGENTOS_LOG_WARN("Checkpoint for task %s seq %lu failed verification", task_id,
                         (unsigned long)latest_seq);
        for (size_t i = 0; i < cp_count; i++) {
            agentos_checkpoint_destroy(checkpoints[i]);
        }
        AGENTOS_FREE(checkpoints);
        return AGENTOS_EIO;
    }

    char restored_id[128];
    snprintf(restored_id, sizeof(restored_id), "task-%s-restored-%016lx", task_id,
             (unsigned long)(agentos_time_ns() & 0xFFFFFFFF));

    agentos_mutex_lock(loop->lock);
    snprintf(loop->current_task_id, sizeof(loop->current_task_id), "%s", restored_id);
    snprintf(loop->current_session_id, sizeof(loop->current_session_id), "%s", latest->session_id);
    loop->checkpoint_seq = latest->sequence_num;
    clear_completed_nodes(loop);
    for (size_t i = 0; i < latest->completed_count; i++) {
        if (latest->completed_nodes && latest->completed_nodes[i])
            add_completed_node(loop, latest->completed_nodes[i]);
    }
    agentos_mutex_unlock(loop->lock);

    char *recovered_input = NULL;
    if (loop->persistent_original_input) {
        AGENTOS_FREE(loop->persistent_original_input);
        loop->persistent_original_input = NULL;
    }

    if (latest->state_json) {
        const char *input_tag = "\"original_input\":\"";
        char *input_pos = strstr(latest->state_json, input_tag);
        if (input_pos) {
            input_pos += strlen(input_tag);
            char *input_end = strchr(input_pos, '\"');
            if (input_end && input_end > input_pos) {
                size_t input_len = (size_t)(input_end - input_pos);
                recovered_input = (char *)AGENTOS_MALLOC(input_len + 1);
                if (recovered_input) {
                    memcpy(recovered_input, input_pos, input_len);
                    recovered_input[input_len] = '\0';
                    loop->persistent_original_input = AGENTOS_STRDUP(recovered_input);
                }
            }
        }
    }

    for (size_t i = 0; i < latest->pending_count; i++) {
        if (!latest->pending_nodes || !latest->pending_nodes[i])
            continue;

        agentos_task_t task;
        memset(&task, 0, sizeof(task));
        task.task_input =
            recovered_input ? (void *)recovered_input : (void *)latest->pending_nodes[i];
        task.task_timeout_ms = loop->manager.loop_config_task_timeout_ms;

        char *node_task_id = NULL;
        err = agentos_execution_submit(loop->execution, &task, &node_task_id);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Failed to resubmit pending node %s: %d", latest->pending_nodes[i],
                             err);
        }
        if (node_task_id)
            AGENTOS_FREE(node_task_id);
    }

    if (recovered_input)
        AGENTOS_FREE(recovered_input);

    *out_restored_task_id = AGENTOS_STRDUP(restored_id);

    agentos_mutex_lock(loop->lock);
    loop->task_pending = 1;
    agentos_cond_signal(loop->cond);
    agentos_mutex_unlock(loop->lock);

    AGENTOS_LOG_INFO("Restored task %s from checkpoint seq %lu (%zu pending nodes)", task_id,
                     (unsigned long)latest_seq, latest->pending_count);

    for (size_t i = 0; i < cp_count; i++) {
        agentos_checkpoint_destroy(checkpoints[i]);
    }
    AGENTOS_FREE(checkpoints);

    return AGENTOS_SUCCESS;
}

AGENTOS_API agentos_error_t agentos_loop_list_checkpoints(agentos_core_loop_t *loop,
                                                          char ***out_task_ids, size_t *out_count)
{
    if (!loop || !out_task_ids || !out_count)
        return AGENTOS_EINVAL;
    if (!loop->checkpoint_initialized)
        return AGENTOS_ENOTINIT;

    *out_count = 0;
    *out_task_ids = NULL;

    const char *cp_dir = loop->manager.loop_config_checkpoint_path;
    if (cp_dir[0] == '\0')
        cp_dir = "./data/checkpoints";

    DIR *dir = opendir(cp_dir);
    if (!dir)
        return AGENTOS_SUCCESS;

    size_t capacity = 16;
    char **ids = (char **)AGENTOS_CALLOC(capacity, sizeof(char *));
    if (!ids) {
        closedir(dir);
        return AGENTOS_ENOMEM;
    }

    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        size_t nlen = strlen(name);

        if (nlen < 6 || strcmp(name + nlen - 5, ".json") != 0)
            continue;

        if (strncmp(name, "checkpoint_", 11) != 0)
            continue;

        char tid[128] = {0};
        size_t tid_len = nlen - 5 - 11;
        if (tid_len >= sizeof(tid))
            tid_len = sizeof(tid) - 1;
        memcpy(tid, name + 11, tid_len);
        tid[tid_len] = '\0';

        int dup = 0;
        for (size_t j = 0; j < count; j++) {
            if (ids[j] && strcmp(ids[j], tid) == 0) {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;

        if (count >= capacity) {
            capacity *= 2;
            char **new_ids = (char **)AGENTOS_REALLOC(ids, capacity * sizeof(char *));
            if (!new_ids) {
                for (size_t j = 0; j < count; j++)
                    AGENTOS_FREE(ids[j]);
                AGENTOS_FREE(ids);
                closedir(dir);
                return AGENTOS_ENOMEM;
            }
            ids = new_ids;
            memset(ids + count, 0, (capacity - count) * sizeof(char *));
        }

        ids[count] = AGENTOS_STRDUP(tid);
        if (ids[count])
            count++;
    }
    closedir(dir);

    *out_task_ids = ids;
    *out_count = count;
    return AGENTOS_SUCCESS;
}

static void add_completed_node(agentos_core_loop_t *loop, const char *node_id)
{
    if (!loop || !node_id)
        return;

    if (loop->completed_node_count >= loop->completed_node_capacity) {
        size_t new_cap = loop->completed_node_capacity == 0 ? 16 : loop->completed_node_capacity * 2;
        char **new_arr = (char **)AGENTOS_REALLOC(loop->completed_node_ids, new_cap * sizeof(char *));
        if (!new_arr)
            return;
        loop->completed_node_ids = new_arr;
        memset(loop->completed_node_ids + loop->completed_node_capacity, 0,
               (new_cap - loop->completed_node_capacity) * sizeof(char *));
        loop->completed_node_capacity = new_cap;
    }

    loop->completed_node_ids[loop->completed_node_count] = AGENTOS_STRDUP(node_id);
    if (loop->completed_node_ids[loop->completed_node_count])
        loop->completed_node_count++;
}

static void clear_completed_nodes(agentos_core_loop_t *loop)
{
    if (!loop)
        return;

    if (loop->completed_node_ids) {
        for (size_t i = 0; i < loop->completed_node_count; i++) {
            AGENTOS_FREE(loop->completed_node_ids[i]);
        }
        AGENTOS_FREE(loop->completed_node_ids);
        loop->completed_node_ids = NULL;
    }
    loop->completed_node_count = 0;
    loop->completed_node_capacity = 0;
}

static agentos_error_t save_incremental_checkpoint(agentos_core_loop_t *loop, const char *task_id,
                                                    const char *session_id, const char *node_id)
{
    if (!loop || !loop->checkpoint_initialized || !task_id)
        return AGENTOS_EINVAL;

    char state_json[8192];
    int json_len = snprintf(state_json, sizeof(state_json),
                            "{\"type\":\"incremental\",\"task_id\":\"%s\","
                            "\"session_id\":\"%s\",\"completed_node\":\"%s\","
                            "\"total_completed\":%zu}",
                            task_id, session_id ? session_id : "",
                            node_id ? node_id : "",
                            loop->completed_node_count);

    if (json_len <= 0 || (size_t)json_len >= sizeof(state_json))
        return AGENTOS_EOVERFLOW;

    loop->checkpoint_seq++;
    agentos_task_checkpoint_t *checkpoint = NULL;
    agentos_error_t err = agentos_checkpoint_create(
        task_id, session_id ? session_id : "default", loop->checkpoint_seq, state_json,
        loop->completed_node_ids, loop->completed_node_count,
        NULL, 0, &checkpoint);

    if (err == AGENTOS_SUCCESS && checkpoint) {
        err = agentos_checkpoint_save(checkpoint);
        if (err == AGENTOS_SUCCESS) {
            AGENTOS_LOG_INFO("Incremental checkpoint saved for task %s node %s (seq=%lu, completed=%zu)",
                             task_id, node_id ? node_id : "",
                             (unsigned long)loop->checkpoint_seq,
                             loop->completed_node_count);
        }
        agentos_checkpoint_destroy(checkpoint);
    }

    loop->last_checkpoint_time_ms = get_time_ms();
    return err;
}
