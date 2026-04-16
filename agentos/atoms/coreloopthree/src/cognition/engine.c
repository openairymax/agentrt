/**
 * @file engine.c
 * @brief 认知引擎核心实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "cognition.h"
#include "agentos.h"
#include "logger.h"
#include "id_utils.h"
#include "error_utils.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

/* JSON解析库 - 条件编译 */
#ifdef AGENTOS_HAS_CJSON
#include <cjson/cJSON.h>
#else
typedef struct cJSON { int type; char* valuestring; double valuedouble; struct cJSON* child; struct cJSON* next; } cJSON;
#define cJSON_NULL 0
#define cJSON_False 1
#define cJSON_True 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6
static int g_cjson_available = 0;

static inline cJSON* cJSON_CreateObject(void) {
    fprintf(stderr, "[WARN] cJSON not available: cJSON_CreateObject() returns NULL\n");
    return NULL;
}
static inline void cJSON_Delete(cJSON* item) { (void)item; }
static inline cJSON* cJSON_CreateArray(void) {
    fprintf(stderr, "[WARN] cJSON not available: cJSON_CreateArray() returns NULL\n");
    return NULL;
}
static inline void cJSON_AddItemToArray(cJSON* a, cJSON* i) { (void)a;(void)i; }
static inline void cJSON_AddItemToObject(cJSON* o, const char* k, cJSON* i) { (void)o;(void)k;(void)i; }
static inline void cJSON_AddStringToObject(cJSON* o, const char* k, const char* v) { (void)o;(void)k;(void)v; }
static inline void cJSON_AddNumberToObject(cJSON* o, const char* k, double v) { (void)o;(void)k;(void)v; }
static inline char* cJSON_PrintUnformatted(const cJSON* i) {
    (void)i;
    fprintf(stderr, "[WARN] cJSON not available: cJSON_PrintUnformatted() returns NULL\n");
    return NULL;
}
#endif /* AGENTOS_HAS_CJSON */

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include <agentos/atomic_compat.h>

/* 平台特定头文件 */
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

struct agentos_cognition_engine {
    agentos_plan_strategy_t* plan_strat;
    agentos_plan_strategy_t* fallback_plan_strat;
    agentos_coordinator_strategy_t* coord_strat;
    agentos_dispatching_strategy_t* disp_strat;
    void* context;
    void (*context_destroy)(void*);
    agentos_mutex_t* lock;
    uint32_t stats_processed;
    uint64_t stats_total_time_ns;
    agentos_cognition_config_t manager;
    agentos_feedback_callback_t feedback_cb;
    void* feedback_user_data;
    uint64_t stats_success_count;
    uint64_t stats_failure_count;
    uint64_t stats_total_retries;
};

/**
 * @brief 触发反馈回调
 * @param engine 认知引擎
 * @param level 反馈级别（1=实时，2=轮次内，3=跨轮次）
 * @param event 事件类型
 * @param data 反馈数据（JSON 格式）
 */
static void trigger_feedback(
    agentos_cognition_engine_t* engine,
    int level,
    const char* event,
    const char* data) {

    if (engine && engine->feedback_cb) {
        engine->feedback_cb(
            level,
            "cognition",
            event,
            data,
            data ? strlen(data) : 0,
            engine->feedback_user_data
        );
    }
}

/**
 * @brief 创建认知引擎
 *
 * @param plan_strategy 规划策略（可为NULL�?
 * @param coord_strategy 协调策略（可为NULL�?
 * @param disp_strategy 分发策略（可为NULL�?
 * @param out_engine 输出认知引擎指针
 * @return agentos_error_t 错误�?
 *
 * @note 使用默认配置创建认知引擎，如需自定义配置请使用 agentos_cognition_create_ex
 */
agentos_error_t agentos_cognition_create(
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine) {

    return agentos_cognition_create_ex(NULL, plan_strategy, coord_strategy, disp_strategy, out_engine);
}

/**
 * @brief 创建认知引擎（扩展版本，支持自定义配置）
 *
 * @param manager 配置参数（可为NULL，使用默认配置）
 * @param plan_strategy 规划策略（可为NULL�?
 * @param coord_strategy 协调策略（可为NULL�?
 * @param disp_strategy 分发策略（可为NULL�?
 * @param out_engine 输出认知引擎指针
 * @return agentos_error_t 错误�?
 *
 * @note 如果config为NULL，将使用默认配置�?
 *       - default_timeout_ms: 30000 (30�?
 *       - max_retries: 3
 */
agentos_error_t agentos_cognition_create_ex(
    const agentos_cognition_config_t* manager,
    agentos_plan_strategy_t* plan_strategy,
    agentos_coordinator_strategy_t* coord_strategy,
    agentos_dispatching_strategy_t* disp_strategy,
    agentos_cognition_engine_t** out_engine) {

    if (!out_engine) return AGENTOS_EINVAL;

    agentos_cognition_engine_t* engine = (agentos_cognition_engine_t*)AGENTOS_CALLOC(1, sizeof(agentos_cognition_engine_t));
    if (!engine) {
        AGENTOS_LOG_ERROR("Failed to allocate cognition engine");
        return AGENTOS_ENOMEM;
    }

    engine->plan_strat = plan_strategy;
    engine->coord_strat = coord_strategy;
    engine->disp_strat = disp_strategy;
    engine->lock = agentos_mutex_create();
    if (!engine->lock) {
        AGENTOS_LOG_ERROR("Failed to create mutex");
        AGENTOS_FREE(engine);
        return AGENTOS_ENOMEM;
    }

    // 设置配置
    if (manager) {
        engine->manager = *manager;
        engine->feedback_cb = manager->feedback_callback;
        engine->feedback_user_data = manager->feedback_user_data;
    } else {
        engine->manager.cognition_default_timeout_ms = 30000;
        engine->manager.cognition_max_retries = 3;
        engine->manager.feedback_callback = NULL;
        engine->manager.feedback_user_data = NULL;
        engine->feedback_cb = NULL;
        engine->feedback_user_data = NULL;
    }

    engine->stats_processed = 0;
    engine->stats_total_time_ns = 0;
    engine->stats_success_count = 0;
    engine->stats_failure_count = 0;
    engine->stats_total_retries = 0;

    *out_engine = engine;

    // 触发引擎创建反馈
    trigger_feedback(engine, 2, "engine_created", "{\"status\":\"initialized\"}");

    return AGENTOS_SUCCESS;
}

void agentos_cognition_destroy(agentos_cognition_engine_t* engine) {
    if (!engine) return;
    if (engine->context && engine->context_destroy) {
        engine->context_destroy(engine->context);
    }
    if (engine->lock) {
        agentos_mutex_destroy(engine->lock);
    }
    AGENTOS_FREE(engine);
}

void agentos_cognition_set_fallback_plan(
    agentos_cognition_engine_t* engine,
    agentos_plan_strategy_t* fallback) {

    if (!engine) return;
    agentos_mutex_lock(engine->lock);
    engine->fallback_plan_strat = fallback;
    agentos_mutex_unlock(engine->lock);
}

void agentos_cognition_set_context(
    agentos_cognition_engine_t* engine,
    void* context,
    void (*destroy)(void*)) {

    if (!engine) return;
    agentos_mutex_lock(engine->lock);
    if (engine->context && engine->context_destroy) {
        engine->context_destroy(engine->context);
    }
    engine->context = context;
    engine->context_destroy = destroy;
    agentos_mutex_unlock(engine->lock);
}

/**
 * @brief 处理用户输入，生成任务计�?
 *
 * @param engine 认知引擎
 * @param input 用户输入文本
 * @param input_len 输入文本长度
 * @param out_plan 输出任务计划指针
 * @return agentos_error_t 错误�?
 *
 * @note 处理流程�?
 *       1. 参数验证
 *       2. 构建意图结构
 *       3. 尝试主规划策�?
 *       4. 如果主策略失败，尝试回退策略
 *       5. 生成计划ID
 *       6. 更新统计信息
 *
 * @warning 调用者负责释放返回的任务计划（使�?agentos_task_plan_free�?
 */
agentos_error_t agentos_cognition_process(
    agentos_cognition_engine_t* engine,
    const char* input,
    size_t input_len,
    agentos_task_plan_t** out_plan) {

    if (!engine || !input || !out_plan) {
        AGENTOS_LOG_ERROR("Invalid parameters to cognition_process: engine=%p, input=%p, out_plan=%p",
                         (void*)engine, (void*)input, (void*)out_plan);
        return AGENTOS_EINVAL;
    }
    if (input_len == 0) return AGENTOS_EINVAL;

    agentos_intent_t intent;
    memset(&intent, 0, sizeof(intent));
    intent.intent_raw_text = (char*)input;
    intent.intent_raw_len = input_len;
    intent.intent_goal = (char*)input;
    intent.intent_goal_len = input_len;
    intent.intent_flags = 0;
    intent.intent_context = engine->context;

    uint64_t start_ns = agentos_time_monotonic_ns();

    agentos_task_plan_t* plan = NULL;
    agentos_error_t err = AGENTOS_ENOTSUP;

    // 读取策略指针（加锁防止与 set_fallback_plan 竞态）
    agentos_plan_strategy_t* plan_strat = NULL;
    agentos_plan_strategy_t* fallback_strat = NULL;
    agentos_mutex_lock(engine->lock);
    plan_strat = engine->plan_strat;
    fallback_strat = engine->fallback_plan_strat;
    agentos_mutex_unlock(engine->lock);

    // 尝试主策�?
    if (plan_strat && plan_strat->plan) {
        err = plan_strat->plan(&intent, plan_strat->data, &plan);
    }

    // 如果主策略失败，尝试回退策略
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_WARN("Primary planning failed: %s (code %d), trying fallback",
                        agentos_error_string(err), err);

        // 触发轮次内反馈：主策略失�?
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf),
            "{\"error_code\":%d,\"error_msg\":\"%s\",\"stage\":\"primary_planning\"}",
            err, agentos_error_string(err));
        trigger_feedback(engine, 1, "planning_retry", err_buf);

        if (fallback_strat && fallback_strat->plan) {
            err = fallback_strat->plan(&intent, fallback_strat->data, &plan);
            if (err == AGENTOS_SUCCESS) {
                agentos_mutex_lock(engine->lock);
                engine->stats_total_retries++;
                agentos_mutex_unlock(engine->lock);
            }
        } else {
            AGENTOS_LOG_ERROR("No fallback planner available, primary error: %s (code %d)",
                            agentos_error_string(err), err);

            // 触发实时反馈：处理失�?
            snprintf(err_buf, sizeof(err_buf),
                "{\"error_code\":%d,\"error_msg\":\"%s\",\"stage\":\"no_fallback\"}",
                err, agentos_error_string(err));
            trigger_feedback(engine, 0, "process_failed", err_buf);

            agentos_mutex_lock(engine->lock);
            engine->stats_failure_count++;
            agentos_mutex_unlock(engine->lock);

            return err;
        }
    }

    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Planning failed: %s (code %d)", agentos_error_string(err), err);

        // 触发实时反馈：处理失�?
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf),
            "{\"error_code\":%d,\"error_msg\":\"%s\",\"stage\":\"fallback_failed\"}",
            err, agentos_error_string(err));
        trigger_feedback(engine, 0, "process_failed", err_buf);

        agentos_mutex_lock(engine->lock);
        engine->stats_failure_count++;
        agentos_mutex_unlock(engine->lock);

        return err;
    }

    if (plan && !plan->task_plan_id) {
        char id_buf[64];
        agentos_generate_plan_id(id_buf, sizeof(id_buf));
        plan->task_plan_id = AGENTOS_STRDUP(id_buf);
        if (!plan->task_plan_id) {
            AGENTOS_LOG_ERROR("Failed to allocate plan_id: %s (code %d)",
                            agentos_error_string(AGENTOS_ENOMEM), AGENTOS_ENOMEM);
            agentos_task_plan_free(plan);
            return AGENTOS_ENOMEM;
        }
    }

    uint64_t end_ns = agentos_time_monotonic_ns();
    uint64_t elapsed = end_ns - start_ns;

    agentos_mutex_lock(engine->lock);
    engine->stats_processed++;
    engine->stats_total_time_ns += elapsed;
    engine->stats_success_count++;
    agentos_mutex_unlock(engine->lock);

    // 触发实时反馈：任务处理完成
    char feedback_buf[512];
    snprintf(feedback_buf, sizeof(feedback_buf),
        "{\"plan_id\":\"%s\",\"node_count\":%zu,\"elapsed_ns\":%" PRIu64 ",\"status\":\"success\"}",
        plan->task_plan_id ? plan->task_plan_id : "unknown",
        plan->task_plan_node_count,
        elapsed);
    trigger_feedback(engine, 0, "process_complete", feedback_buf);

    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 释放任务计划及其所有资�?
 *
 * @param plan 任务计划指针
 *
 * @note 释放以下资源�?
 *       1. 计划ID
 *       2. 所有任务节�?
 *       3. 每个任务节点的任务ID、角色、依赖关�?
 *       4. 入口点数�?
 *       5. 任务节点数组
 */
void agentos_task_plan_free(agentos_task_plan_t* plan) {
    if (!plan) return;
    for (size_t i = 0; i < plan->task_plan_node_count; i++) {
        agentos_task_node_t* node = plan->task_plan_nodes[i];
        if (node) {
            if (node->task_node_id) AGENTOS_FREE(node->task_node_id);
            if (node->task_node_agent_role) AGENTOS_FREE(node->task_node_agent_role);
            if (node->task_node_depends_on) {
                for (size_t j = 0; j < node->task_node_depends_count; j++) {
                    AGENTOS_FREE(node->task_node_depends_on[j]);
                }
                AGENTOS_FREE(node->task_node_depends_on);
            }
            AGENTOS_FREE(node);
        }
    }
    AGENTOS_FREE(plan->task_plan_nodes);
    if (plan->task_plan_entry_points) AGENTOS_FREE(plan->task_plan_entry_points);
    if (plan->task_plan_id) AGENTOS_FREE(plan->task_plan_id);
    AGENTOS_FREE(plan);
}

agentos_error_t agentos_cognition_stats(
    agentos_cognition_engine_t* engine,
    char** out_stats,
    size_t* out_len) {

    if (!engine || !out_stats) return AGENTOS_EINVAL;

    agentos_mutex_lock(engine->lock);
    uint32_t processed = engine->stats_processed;
    uint64_t avg_ns = (processed > 0) ? (engine->stats_total_time_ns / processed) : 0;
    agentos_mutex_unlock(engine->lock);

    char buffer[256];
    int len = snprintf(buffer, sizeof(buffer),
        "{\"processed\":%u,\"avg_time_ns\":%" PRIu64 "}",
        processed, avg_ns);

    char* result = (char*)AGENTOS_MALLOC(len + 1);
    if (!result) return AGENTOS_ENOMEM;
    memcpy(result, buffer, len + 1);

    *out_stats = result;
    if (out_len) *out_len = len;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_cognition_health_check(
    agentos_cognition_engine_t* engine,
    char** out_json) {

    if (!engine || !out_json) return AGENTOS_EINVAL;

    cJSON* root = cJSON_CreateObject();
    if (!root) return AGENTOS_ENOMEM;

    cJSON_AddStringToObject(root, "status", "healthy");

    agentos_mutex_lock(engine->lock);
    cJSON_AddNumberToObject(root, "processed", engine->stats_processed);
    cJSON_AddNumberToObject(root, "avg_time_ns",
        (engine->stats_processed > 0) ? (engine->stats_total_time_ns / engine->stats_processed) : 0);
    agentos_mutex_unlock(engine->lock);

    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return AGENTOS_ENOMEM;

    *out_json = json;
    return AGENTOS_SUCCESS;
}
