// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file taskflow_advanced.h
 * @brief TaskFlow Advanced Workflow Engine for AgentOS
 *
 * 基于Pregel超步计算模型的高级工作流引擎，支持条件分支、
 * 并行汇聚、子工作流、循环迭代等复杂工作流模式。
 * 与现有Atoms任务系统通过注册机制集成，保持向后兼容。
 *
 * 核心设计:
 * 1. DAG工作流模型 — 有向无环图任务编排
 * 2. 条件分支 — 基于运行时数据的动态路由
 * 3. 并行汇聚 — Fork/Join并行执行模式
 * 4. 子工作流 — 工作流嵌套与组合
 * 5. 循环迭代 — 条件循环与计数循环
 * 6. 错误恢复 — 重试/回滚/降级策略
 * 7. 检查点 — 工作流状态持久化与恢复
 *
 * @since 2.0.0
 * @see atoms/taskflow
 */

#ifndef AGENTOS_TASKFLOW_ADVANCED_H
#define AGENTOS_TASKFLOW_ADVANCED_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TASKFLOW_ADV_VERSION       "1.0.0"
#define TASKFLOW_MAX_NODES         1024
#define TASKFLOW_MAX_EDGES         4096
#define TASKFLOW_MAX_SUBFLOWS      64
#define TASKFLOW_MAX_CHECKPOINTS   256
#define TASKFLOW_MAX_RETRIES       5
#define TASKFLOW_MAX_PARALLEL      32

typedef enum {
    TASKFLOW_NODE_TASK = 0,
    TASKFLOW_NODE_CONDITION,
    TASKFLOW_NODE_FORK,
    TASKFLOW_NODE_JOIN,
    TASKFLOW_NODE_SUBFLOW,
    TASKFLOW_NODE_LOOP,
    TASKFLOW_NODE_DELAY,
    TASKFLOW_NODE_EVENT_WAIT,
    TASKFLOW_NODE_TRANSFORM
} taskflow_node_type_t;

typedef enum {
    TASKFLOW_STATE_PENDING = 0,
    TASKFLOW_STATE_READY,
    TASKFLOW_STATE_RUNNING,
    TASKFLOW_STATE_WAITING,
    TASKFLOW_STATE_COMPLETED,
    TASKFLOW_STATE_FAILED,
    TASKFLOW_STATE_CANCELED,
    TASKFLOW_STATE_SKIPPED,
    TASKFLOW_STATE_RETRYING
} taskflow_state_t;

typedef enum {
    TASKFLOW_ERROR_RETRY = 0,
    TASKFLOW_ERROR_ROLLBACK,
    TASKFLOW_ERROR_SKIP,
    TASKFLOW_ERROR_ABORT,
    TASKFLOW_ERROR_FALLBACK
} taskflow_error_strategy_t;

typedef enum {
    TASKFLOW_LOOP_COUNT = 0,
    TASKFLOW_LOOP_CONDITION,
    TASKFLOW_LOOP_FOREACH
} taskflow_loop_type_t;

typedef struct {
    char id[64];
    char name[128];
    taskflow_node_type_t type;
    taskflow_state_t state;
    char* task_handler_name;
    char* config_json;
    char* input_transform_json;
    char* output_transform_json;
    int32_t timeout_ms;
    int32_t max_retries;
    int32_t retry_count;
    int32_t retry_delay_ms;
    taskflow_error_strategy_t error_strategy;
    char* fallback_handler_name;
    char* condition_expr;
    char* subflow_id;
    taskflow_loop_type_t loop_type;
    int32_t loop_count;
    char* loop_condition_expr;
    char* loop_foreach_json;
    int32_t delay_ms;
    char* event_type;
    void* user_data;
} taskflow_node_t;

typedef struct {
    char id[64];
    char source_node_id[64];
    char target_node_id[64];
    char condition_expr[256];
    int32_t priority;
    bool is_default;
} taskflow_edge_t;

typedef struct {
    char id[64];
    char name[128];
    char description[256];
    char version[32];
    taskflow_node_t* nodes;
    size_t node_count;
    taskflow_edge_t* edges;
    size_t edge_count;
    char* initial_node_id;
    char* input_schema_json;
    char* output_schema_json;
    int32_t default_timeout_ms;
    taskflow_error_strategy_t default_error_strategy;
    int32_t default_max_retries;
} taskflow_workflow_t;

typedef struct {
    char execution_id[64];
    char workflow_id[64];
    taskflow_state_t state;
    char* current_node_id;
    char* input_json;
    char* output_json;
    char* error_message;
    uint64_t started_at;
    uint64_t completed_at;
    double progress;
    int32_t superstep;
    size_t completed_nodes;
    size_t total_nodes;
    char* variables_json;
} taskflow_execution_t;

typedef struct {
    char id[64];
    char execution_id[64];
    char workflow_id[64];
    char node_id[64];
    taskflow_state_t state;
    char* snapshot_json;
    uint64_t timestamp;
} taskflow_checkpoint_t;

typedef struct taskflow_engine_s taskflow_engine_t;

typedef int (*taskflow_task_handler_t)(taskflow_engine_t* engine,
                                        const char* node_id,
                                        const char* input_json,
                                        char** output_json,
                                        void* user_data);

typedef bool (*taskflow_condition_fn)(const char* expression,
                                       const char* variables_json,
                                       void* user_data);

typedef void (*taskflow_progress_callback_t)(const char* execution_id,
                                               const char* node_id,
                                               taskflow_state_t state,
                                               double progress,
                                               void* user_data);

typedef void (*taskflow_event_callback_t)(const char* execution_id,
                                            const char* event_type,
                                            const char* data_json,
                                            void* user_data);

taskflow_engine_t* taskflow_engine_create(void);
void taskflow_engine_destroy(taskflow_engine_t* engine);

int taskflow_engine_register_handler(taskflow_engine_t* engine,
                                       const char* name,
                                       taskflow_task_handler_t handler,
                                       void* user_data);

int taskflow_engine_unregister_handler(taskflow_engine_t* engine, const char* name);

int taskflow_engine_register_workflow(taskflow_engine_t* engine,
                                       const taskflow_workflow_t* workflow);

int taskflow_engine_unregister_workflow(taskflow_engine_t* engine, const char* workflow_id);

int taskflow_engine_load_workflow_json(taskflow_engine_t* engine,
                                        const char* workflow_json);

int taskflow_engine_start(taskflow_engine_t* engine,
                            const char* workflow_id,
                            const char* input_json,
                            char** execution_id);

int taskflow_engine_cancel(taskflow_engine_t* engine, const char* execution_id);

int taskflow_engine_pause(taskflow_engine_t* engine, const char* execution_id);

int taskflow_engine_resume(taskflow_engine_t* engine, const char* execution_id);

int taskflow_engine_get_execution(taskflow_engine_t* engine,
                                    const char* execution_id,
                                    taskflow_execution_t** execution);

int taskflow_engine_step(taskflow_engine_t* engine, const char* execution_id);

int taskflow_engine_run_to_completion(taskflow_engine_t* engine,
                                        const char* execution_id);

int taskflow_engine_create_checkpoint(taskflow_engine_t* engine,
                                        const char* execution_id,
                                        char** checkpoint_id);

int taskflow_engine_restore_checkpoint(taskflow_engine_t* engine,
                                         const char* checkpoint_id);

int taskflow_engine_list_checkpoints(taskflow_engine_t* engine,
                                       const char* execution_id,
                                       taskflow_checkpoint_t** checkpoints,
                                       size_t* count);

int taskflow_engine_set_condition_fn(taskflow_engine_t* engine,
                                       taskflow_condition_fn fn,
                                       void* user_data);

int taskflow_engine_set_progress_callback(taskflow_engine_t* engine,
                                            taskflow_progress_callback_t callback,
                                            void* user_data);

int taskflow_engine_set_event_callback(taskflow_engine_t* engine,
                                         taskflow_event_callback_t callback,
                                         void* user_data);

int taskflow_engine_notify_event(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* event_type,
                                   const char* data_json);

int taskflow_engine_set_variable(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* key,
                                   const char* value_json);

int taskflow_engine_get_variable(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* key,
                                   char** value_json);

size_t taskflow_engine_get_workflow_count(taskflow_engine_t* engine);
size_t taskflow_engine_get_execution_count(taskflow_engine_t* engine);

void taskflow_workflow_destroy(taskflow_workflow_t* workflow);
void taskflow_execution_destroy(taskflow_execution_t* execution);
void taskflow_checkpoint_destroy(taskflow_checkpoint_t* checkpoint);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TASKFLOW_ADVANCED_H */
