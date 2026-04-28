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

/** @brief 高级工作流引擎版本号 */
#define TASKFLOW_ADV_VERSION       "1.0.0"
/** @brief 单个工作流最大节点数 */
#define TASKFLOW_MAX_NODES         1024
/** @brief 单个工作流最大边数 */
#define TASKFLOW_MAX_EDGES         4096
/** @brief 最大子工作流嵌套深度 */
#define TASKFLOW_MAX_SUBFLOWS      64
/** @brief 最大检查点数量 */
#define TASKFLOW_MAX_CHECKPOINTS   256
/** @brief 默认最大重试次数 */
#define TASKFLOW_MAX_RETRIES       5
/** @brief 默认最大并行度 */
#define TASKFLOW_MAX_PARALLEL      32

/**
 * @brief 工作流节点类型枚举
 */
typedef enum {
    TASKFLOW_NODE_TASK = 0,       /**< 普通任务节点 */
    TASKFLOW_NODE_CONDITION,      /**< 条件分支节点 */
    TASKFLOW_NODE_FORK,           /**< 并行分叉节点 */
    TASKFLOW_NODE_JOIN,           /**< 并行汇聚节点 */
    TASKFLOW_NODE_SUBFLOW,        /**< 子工作流节点 */
    TASKFLOW_NODE_LOOP,           /**< 循环节点 */
    TASKFLOW_NODE_DELAY,          /**< 延时节点 */
    TASKFLOW_NODE_EVENT_WAIT,     /**< 事件等待节点 */
    TASKFLOW_NODE_TRANSFORM       /**< 数据变换节点 */
} taskflow_node_type_t;

/**
 * @brief 工作流执行状态枚举
 */
typedef enum {
    TASKFLOW_STATE_PENDING = 0,   /**< 等待中 */
    TASKFLOW_STATE_READY,         /**< 就绪 */
    TASKFLOW_STATE_RUNNING,       /**< 运行中 */
    TASKFLOW_STATE_WAITING,       /**< 等待事件/条件 */
    TASKFLOW_STATE_COMPLETED,     /**< 已完成 */
    TASKFLOW_STATE_FAILED,        /**< 已失败 */
    TASKFLOW_STATE_CANCELED,      /**< 已取消 */
    TASKFLOW_STATE_SKIPPED,       /**< 已跳过 */
    TASKFLOW_STATE_RETRYING       /**< 重试中 */
} taskflow_state_t;

/**
 * @brief 错误处理策略枚举
 */
typedef enum {
    TASKFLOW_ERROR_RETRY = 0,     /**< 重试执行 */
    TASKFLOW_ERROR_ROLLBACK,      /**< 回滚到上一个检查点 */
    TASKFLOW_ERROR_SKIP,          /**< 跳过当前节点 */
    TASKFLOW_ERROR_ABORT,         /**< 终止整个工作流 */
    TASKFLOW_ERROR_FALLBACK       /**< 降级到备用处理器 */
} taskflow_error_strategy_t;

/**
 * @brief 循环类型枚举
 */
typedef enum {
    TASKFLOW_LOOP_COUNT = 0,      /**< 计数循环 */
    TASKFLOW_LOOP_CONDITION,      /**< 条件循环 */
    TASKFLOW_LOOP_FOREACH         /**< 迭代循环 */
} taskflow_loop_type_t;

/**
 * @brief 工作流节点结构体
 */
typedef struct {
    char id[64];                          /**< 节点唯一标识 */
    char name[128];                       /**< 节点名称 */
    taskflow_node_type_t type;            /**< 节点类型 */
    taskflow_state_t state;               /**< 当前状态 */
    char* task_handler_name;              /**< 任务处理器名称 */
    char* config_json;                    /**< 节点配置(JSON) */
    char* input_transform_json;           /**< 输入变换表达式(JSON) */
    char* output_transform_json;          /**< 输出变换表达式(JSON) */
    int32_t timeout_ms;                   /**< 超时时间(毫秒) */
    int32_t max_retries;                  /**< 最大重试次数 */
    int32_t retry_count;                  /**< 当前重试计数 */
    int32_t retry_delay_ms;               /**< 重试间隔(毫秒) */
    taskflow_error_strategy_t error_strategy; /**< 错误处理策略 */
    char* fallback_handler_name;          /**< 降级处理器名称 */
    char* condition_expr;                 /**< 条件表达式 */
    char* subflow_id;                     /**< 子工作流ID */
    taskflow_loop_type_t loop_type;       /**< 循环类型 */
    int32_t loop_count;                   /**< 循环次数(计数循环) */
    char* loop_condition_expr;            /**< 循环条件表达式 */
    char* loop_foreach_json;              /**< 迭代数据(JSON数组) */
    int32_t delay_ms;                     /**< 延时时间(毫秒) */
    char* event_type;                     /**< 等待的事件类型 */
    void* user_data;                      /**< 用户自定义数据 */
} taskflow_node_t;

/**
 * @brief 工作流边结构体
 */
typedef struct {
    char id[64];                          /**< 边唯一标识 */
    char source_node_id[64];              /**< 源节点ID */
    char target_node_id[64];              /**< 目标节点ID */
    char condition_expr[256];             /**< 边条件表达式 */
    int32_t priority;                     /**< 优先级(数值越大越优先) */
    bool is_default;                      /**< 是否为默认边(无条件匹配时选择) */
} taskflow_edge_t;

/**
 * @brief 工作流定义结构体
 */
typedef struct {
    char id[64];                          /**< 工作流唯一标识 */
    char name[128];                       /**< 工作流名称 */
    char description[256];                /**< 工作流描述 */
    char version[32];                     /**< 工作流版本 */
    taskflow_node_t* nodes;               /**< 节点数组 */
    size_t node_count;                    /**< 节点数量 */
    taskflow_edge_t* edges;               /**< 边数组 */
    size_t edge_count;                    /**< 边数量 */
    char* initial_node_id;                /**< 起始节点ID */
    char* input_schema_json;              /**< 输入数据模式(JSON Schema) */
    char* output_schema_json;             /**< 输出数据模式(JSON Schema) */
    int32_t default_timeout_ms;           /**< 默认超时(毫秒) */
    taskflow_error_strategy_t default_error_strategy; /**< 默认错误策略 */
    int32_t default_max_retries;          /**< 默认最大重试次数 */
} taskflow_workflow_t;

/**
 * @brief 工作流执行实例结构体
 */
typedef struct {
    char execution_id[64];                /**< 执行实例ID */
    char workflow_id[64];                 /**< 所属工作流ID */
    taskflow_state_t state;               /**< 当前执行状态 */
    char* current_node_id;                /**< 当前执行节点ID */
    char* input_json;                     /**< 输入数据(JSON) */
    char* output_json;                    /**< 输出数据(JSON) */
    char* error_message;                  /**< 错误信息 */
    uint64_t started_at;                  /**< 启动时间(ns) */
    uint64_t completed_at;                /**< 完成时间(ns) */
    double progress;                      /**< 执行进度(0.0~1.0) */
    int32_t superstep;                    /**< 当前超步编号 */
    size_t completed_nodes;               /**< 已完成节点数 */
    size_t total_nodes;                   /**< 总节点数 */
    char* variables_json;                 /**< 工作流变量(JSON) */
} taskflow_execution_t;

/**
 * @brief 工作流检查点结构体
 */
typedef struct {
    char id[64];                          /**< 检查点ID */
    char execution_id[64];                /**< 所属执行实例ID */
    char workflow_id[64];                 /**< 所属工作流ID */
    char node_id[64];                     /**< 快照所在节点ID */
    taskflow_state_t state;               /**< 快照时执行状态 */
    char* snapshot_json;                  /**< 状态快照(JSON) */
    uint64_t timestamp;                   /**< 快照时间(ns) */
} taskflow_checkpoint_t;

/**
 * @brief 高级工作流引擎句柄(不透明类型)
 */
typedef struct taskflow_engine_s taskflow_engine_t;

/**
 * @brief 任务处理器回调函数类型
 * @param engine 引擎句柄
 * @param node_id 当前节点ID
 * @param input_json 输入数据(JSON字符串)
 * @param output_json 输出数据(调用者负责free)
 * @param user_data 用户自定义数据
 * @return 0表示成功，非0表示失败
 */
typedef int (*taskflow_task_handler_t)(taskflow_engine_t* engine,
                                        const char* node_id,
                                        const char* input_json,
                                        char** output_json,
                                        void* user_data);

/**
 * @brief 条件求值回调函数类型
 * @param expression 条件表达式
 * @param variables_json 当前变量(JSON字符串)
 * @param user_data 用户自定义数据
 * @return true表示条件成立，false表示不成立
 */
typedef bool (*taskflow_condition_fn)(const char* expression,
                                       const char* variables_json,
                                       void* user_data);

/**
 * @brief 进度回调函数类型
 * @param execution_id 执行实例ID
 * @param node_id 当前节点ID
 * @param state 节点状态
 * @param progress 执行进度(0.0~1.0)
 * @param user_data 用户自定义数据
 */
typedef void (*taskflow_progress_callback_t)(const char* execution_id,
                                               const char* node_id,
                                               taskflow_state_t state,
                                               double progress,
                                               void* user_data);

/**
 * @brief 事件回调函数类型
 * @param execution_id 执行实例ID
 * @param event_type 事件类型
 * @param data_json 事件数据(JSON字符串)
 * @param user_data 用户自定义数据
 */
typedef void (*taskflow_event_callback_t)(const char* execution_id,
                                            const char* event_type,
                                            const char* data_json,
                                            void* user_data);

/**
 * @brief 创建高级工作流引擎实例
 * @return 引擎句柄，失败返回NULL
 */
taskflow_engine_t* taskflow_engine_create(void);

/**
 * @brief 销毁高级工作流引擎实例
 * @param engine 引擎句柄
 */
void taskflow_engine_destroy(taskflow_engine_t* engine);

/**
 * @brief 注册任务处理器
 * @param engine 引擎句柄
 * @param name 处理器名称(与节点task_handler_name匹配)
 * @param handler 处理器回调函数
 * @param user_data 用户自定义数据
 * @return 0成功，非0失败
 */
int taskflow_engine_register_handler(taskflow_engine_t* engine,
                                       const char* name,
                                       taskflow_task_handler_t handler,
                                       void* user_data);

/**
 * @brief 注销任务处理器
 * @param engine 引擎句柄
 * @param name 处理器名称
 * @return 0成功，非0失败
 */
int taskflow_engine_unregister_handler(taskflow_engine_t* engine, const char* name);

/**
 * @brief 注册工作流定义
 * @param engine 引擎句柄
 * @param workflow 工作流定义结构体
 * @return 0成功，非0失败
 */
int taskflow_engine_register_workflow(taskflow_engine_t* engine,
                                       const taskflow_workflow_t* workflow);

/**
 * @brief 注销工作流定义
 * @param engine 引擎句柄
 * @param workflow_id 工作流ID
 * @return 0成功，非0失败
 */
int taskflow_engine_unregister_workflow(taskflow_engine_t* engine, const char* workflow_id);

/**
 * @brief 从JSON字符串加载并注册工作流
 * @param engine 引擎句柄
 * @param workflow_json 工作流定义JSON字符串
 * @return 0成功，非0失败
 */
int taskflow_engine_load_workflow_json(taskflow_engine_t* engine,
                                        const char* workflow_json);

/**
 * @brief 启动工作流执行
 * @param engine 引擎句柄
 * @param workflow_id 工作流ID
 * @param input_json 输入数据(JSON字符串)
 * @param execution_id 执行实例ID(输出，调用者负责free)
 * @return 0成功，非0失败
 */
int taskflow_engine_start(taskflow_engine_t* engine,
                            const char* workflow_id,
                            const char* input_json,
                            char** execution_id);

/**
 * @brief 取消工作流执行
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @return 0成功，非0失败
 */
int taskflow_engine_cancel(taskflow_engine_t* engine, const char* execution_id);

/**
 * @brief 暂停工作流执行
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @return 0成功，非0失败
 */
int taskflow_engine_pause(taskflow_engine_t* engine, const char* execution_id);

/**
 * @brief 恢复工作流执行
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @return 0成功，非0失败
 */
int taskflow_engine_resume(taskflow_engine_t* engine, const char* execution_id);

/**
 * @brief 获取执行实例信息
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @param execution 执行实例结构体指针(输出)
 * @return 0成功，非0失败
 */
int taskflow_engine_get_execution(taskflow_engine_t* engine,
                                    const char* execution_id,
                                    taskflow_execution_t** execution);

/**
 * @brief 单步执行(执行下一个节点)
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @return 0成功，非0失败
 */
int taskflow_engine_step(taskflow_engine_t* engine, const char* execution_id);

/**
 * @brief 运行到完成(阻塞直到工作流结束)
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @return 0成功，非0失败
 */
int taskflow_engine_run_to_completion(taskflow_engine_t* engine,
                                        const char* execution_id);

/**
 * @brief 创建执行检查点
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @param checkpoint_id 检查点ID(输出，调用者负责free)
 * @return 0成功，非0失败
 */
int taskflow_engine_create_checkpoint(taskflow_engine_t* engine,
                                        const char* execution_id,
                                        char** checkpoint_id);

/**
 * @brief 恢复到指定检查点
 * @param engine 引擎句柄
 * @param checkpoint_id 检查点ID
 * @return 0成功，非0失败
 */
int taskflow_engine_restore_checkpoint(taskflow_engine_t* engine,
                                         const char* checkpoint_id);

/**
 * @brief 列出执行实例的检查点
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @param checkpoints 检查点数组(输出)
 * @param count 检查点数量(输出)
 * @return 0成功，非0失败
 */
int taskflow_engine_list_checkpoints(taskflow_engine_t* engine,
                                       const char* execution_id,
                                       taskflow_checkpoint_t** checkpoints,
                                       size_t* count);

/**
 * @brief 设置条件求值函数
 * @param engine 引擎句柄
 * @param fn 条件求值回调
 * @param user_data 用户自定义数据
 * @return 0成功，非0失败
 */
int taskflow_engine_set_condition_fn(taskflow_engine_t* engine,
                                       taskflow_condition_fn fn,
                                       void* user_data);

/**
 * @brief 设置进度回调
 * @param engine 引擎句柄
 * @param callback 进度回调函数
 * @param user_data 用户自定义数据
 * @return 0成功，非0失败
 */
int taskflow_engine_set_progress_callback(taskflow_engine_t* engine,
                                            taskflow_progress_callback_t callback,
                                            void* user_data);

/**
 * @brief 设置事件回调
 * @param engine 引擎句柄
 * @param callback 事件回调函数
 * @param user_data 用户自定义数据
 * @return 0成功，非0失败
 */
int taskflow_engine_set_event_callback(taskflow_engine_t* engine,
                                         taskflow_event_callback_t callback,
                                         void* user_data);

/**
 * @brief 通知事件(触发EVENT_WAIT节点)
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @param event_type 事件类型
 * @param data_json 事件数据(JSON字符串)
 * @return 0成功，非0失败
 */
int taskflow_engine_notify_event(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* event_type,
                                   const char* data_json);

/**
 * @brief 设置工作流变量
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @param key 变量名
 * @param value_json 变量值(JSON字符串)
 * @return 0成功，非0失败
 */
int taskflow_engine_set_variable(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* key,
                                   const char* value_json);

/**
 * @brief 获取工作流变量
 * @param engine 引擎句柄
 * @param execution_id 执行实例ID
 * @param key 变量名
 * @param value_json 变量值(输出，调用者负责free)
 * @return 0成功，非0失败
 */
int taskflow_engine_get_variable(taskflow_engine_t* engine,
                                   const char* execution_id,
                                   const char* key,
                                   char** value_json);

/**
 * @brief 获取已注册的工作流数量
 * @param engine 引擎句柄
 * @return 工作流数量
 */
size_t taskflow_engine_get_workflow_count(taskflow_engine_t* engine);

/**
 * @brief 获取活跃的执行实例数量
 * @param engine 引擎句柄
 * @return 执行实例数量
 */
size_t taskflow_engine_get_execution_count(taskflow_engine_t* engine);

/**
 * @brief 销毁工作流定义结构体
 * @param workflow 工作流定义指针
 */
void taskflow_workflow_destroy(taskflow_workflow_t* workflow);

/**
 * @brief 销毁执行实例结构体
 * @param execution 执行实例指针
 */
void taskflow_execution_destroy(taskflow_execution_t* execution);

/**
 * @brief 销毁检查点结构体
 * @param checkpoint 检查点指针
 */
void taskflow_checkpoint_destroy(taskflow_checkpoint_t* checkpoint);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_TASKFLOW_ADVANCED_H */
