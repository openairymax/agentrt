// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file workflow_patterns.c
 * @brief 基础工作流模式实现
 */

#include "workflow_patterns.h"
#include "taskflow.h"
#include "graph_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// 内部数据结构
// ============================================================================

// 工作流执行状态
typedef enum {
    EXECUTION_NOT_STARTED = 0,
    EXECUTION_RUNNING,
    EXECUTION_PAUSED,
    EXECUTION_COMPLETED,
    EXECUTION_FAILED,
    EXECUTION_CANCELLED
} execution_state_t;

// 工作流节点执行状态
typedef struct {
    vertex_id_t node_id;
    bool executed;
    bool succeeded;
    void* result;
    size_t result_size;
    uint64_t start_time;
    uint64_t end_time;
} node_execution_state_t;

// 工作流内部上下文
typedef struct {
    execution_state_t state;
    vertex_id_t current_node;
    node_execution_state_t* node_states;
    size_t completed_nodes;
    size_t total_nodes;
    void* user_context;
    bool async_mode;
    void (*completion_callback)(taskflow_error_t, void*);
    void* callback_data;
} workflow_internal_state_t;

// ============================================================================
// 静态辅助函数
// ============================================================================

// 查找节点索引
static size_t find_node_index(const workflow_context_t* context, vertex_id_t node_id) {
    if (!context || !context->nodes) return SIZE_MAX;
    
    for (size_t i = 0; i < context->node_count; i++) {
        if (context->nodes[i].node_id == node_id) {
            return i;
        }
    }
    
    return SIZE_MAX;
}

// 查找节点执行状态索引
static size_t find_node_state_index(const workflow_internal_state_t* state, vertex_id_t node_id) {
    if (!state || !state->node_states) return SIZE_MAX;
    
    for (size_t i = 0; i < state->total_nodes; i++) {
        if (state->node_states[i].node_id == node_id) {
            return i;
        }
    }
    
    return SIZE_MAX;
}

// 执行节点任务
static taskflow_error_t execute_node_task(workflow_context_t* context,
                                         workflow_internal_state_t* internal_state,
                                         vertex_id_t node_id) {
    if (!context || !internal_state) return TASKFLOW_ERROR_INVALID_ARG;
    
    size_t node_idx = find_node_index(context, node_id);
    if (node_idx == SIZE_MAX) return TASKFLOW_ERROR_INVALID_ARG;
    
    workflow_node_t* node = &context->nodes[node_idx];
    
    // 更新内部状态
    size_t state_idx = find_node_state_index(internal_state, node_id);
    if (state_idx != SIZE_MAX) {
        internal_state->node_states[state_idx].start_time = 0; // TODO: 获取时间戳
        internal_state->node_states[state_idx].executed = true;
    }
    
    internal_state->current_node = node_id;
    
    // 执行任务
    if (node->task_executor && node->task_data) {
        node->task_executor(node->task_data);
        
        // 标记为成功
        if (state_idx != SIZE_MAX) {
            internal_state->node_states[state_idx].succeeded = true;
            internal_state->node_states[state_idx].end_time = 0; // TODO: 获取时间戳
        }
    }
    
    // 更新完成计数
    internal_state->completed_nodes++;
    
    return TASKFLOW_SUCCESS;
}

// 获取下一个可执行节点
static vertex_id_t get_next_executable_node(const workflow_context_t* context,
                                           const workflow_internal_state_t* internal_state,
                                           vertex_id_t current_node) {
    if (!context || !internal_state) return 0;
    
    // 简单实现：基于图的拓扑顺序
    // 实际实现应考虑工作流模式（顺序、并行等）
    
    // 获取当前节点的出边
    const size_t MAX_EDGES = 16;
    workflow_edge_t outgoing_edges[MAX_EDGES];
    size_t edge_count = 0;
    
    for (size_t i = 0; i < context->edge_count && edge_count < MAX_EDGES; i++) {
        if (context->edges[i].source_node == current_node) {
            outgoing_edges[edge_count++] = context->edges[i];
        }
    }
    
    if (edge_count == 0) {
        return 0; // 没有出边，结束
    }
    
    // 检查条件边
    for (size_t i = 0; i < edge_count; i++) {
        workflow_edge_t* edge = &outgoing_edges[i];
        
        if (edge->condition_func) {
            // 检查条件
            if (edge->condition_func(edge->condition_context)) {
                return edge->target_node;
            }
        } else {
            // 无条件边，返回第一个
            return edge->target_node;
        }
    }
    
    return 0; // 没有符合条件的边
}

// 执行顺序工作流
static taskflow_error_t execute_sequential_workflow(workflow_context_t* context,
                                                   workflow_internal_state_t* internal_state) {
    if (!context || !internal_state) return TASKFLOW_ERROR_INVALID_ARG;
    
    vertex_id_t current_node = context->start_node;
    
    while (current_node != 0 && current_node != context->end_node) {
        // 检查是否暂停或停止
        if (internal_state->state == EXECUTION_PAUSED) {
            return TASKFLOW_SUCCESS; // 正常暂停
        }
        if (internal_state->state == EXECUTION_CANCELLED) {
            return TASKFLOW_SUCCESS; // 已取消
        }
        
        // 执行当前节点
        taskflow_error_t result = execute_node_task(context, internal_state, current_node);
        if (result != TASKFLOW_SUCCESS) {
            internal_state->state = EXECUTION_FAILED;
            return result;
        }
        
        // 获取下一个节点
        current_node = get_next_executable_node(context, internal_state, current_node);
    }
    
    // 执行结束节点（如果存在且不是0）
    if (context->end_node != 0 && context->end_node != current_node) {
        execute_node_task(context, internal_state, context->end_node);
    }
    
    internal_state->state = EXECUTION_COMPLETED;
    return TASKFLOW_SUCCESS;
}

// ============================================================================
// 公共API实现
// ============================================================================

workflow_context_t* workflow_context_create(
    const workflow_pattern_config_t* config,
    taskflow_handle_t taskflow_engine)
{
    if (!config) return NULL;
    
    workflow_context_t* context = (workflow_context_t*)calloc(1, sizeof(workflow_context_t));
    if (!context) return NULL;
    
    // 复制配置
    context->config = *config;
    
    // 分配节点数组
    if (config->max_nodes > 0) {
        context->nodes = (workflow_node_t*)calloc(config->max_nodes, sizeof(workflow_node_t));
        if (!context->nodes) {
            free(context);
            return NULL;
        }
    }
    
    // 分配边数组
    if (config->max_edges > 0) {
        context->edges = (workflow_edge_t*)calloc(config->max_edges, sizeof(workflow_edge_t));
        if (!context->edges) {
            if (context->nodes) free(context->nodes);
            free(context);
            return NULL;
        }
    }
    
    // 创建或使用提供的TaskFlow引擎
    if (taskflow_engine) {
        context->taskflow_engine = taskflow_engine;
    } else {
        taskflow_config_t tf_config = {
            .max_vertices = config->max_nodes,
            .max_edges = config->max_edges,
            .max_workers = 4,
            .enable_checkpoint = config->enable_checkpoint
        };
        
        context->taskflow_engine = taskflow_engine_create(&tf_config);
        if (!context->taskflow_engine) {
            if (context->edges) free(context->edges);
            if (context->nodes) free(context->nodes);
            free(context);
            return NULL;
        }
        
        taskflow_engine_init(context->taskflow_engine);
    }
    
    // 创建工作流图
    context->workflow_graph = taskflow_graph_create(context->taskflow_engine);
    if (!context->workflow_graph) {
        if (!taskflow_engine) {
            taskflow_engine_destroy(context->taskflow_engine);
        }
        if (context->edges) free(context->edges);
        if (context->nodes) free(context->nodes);
        free(context);
        return NULL;
    }
    
    return context;
}

void workflow_context_destroy(workflow_context_t* context)
{
    if (!context) return;
    
    // 销毁工作流图
    if (context->workflow_graph) {
        taskflow_graph_destroy(context->workflow_graph);
    }
    
    // 如果内部创建了TaskFlow引擎，销毁它
    // TODO: 需要跟踪是否内部创建
    
    // 释放节点数组
    if (context->nodes) {
        for (size_t i = 0; i < context->node_count; i++) {
            if (context->nodes[i].node_name) free(context->nodes[i].node_name);
            if (context->nodes[i].task_data) free(context->nodes[i].task_data);
        }
        free(context->nodes);
    }
    
    // 释放边数组
    if (context->edges) {
        for (size_t i = 0; i < context->edge_count; i++) {
            if (context->edges[i].edge_label) free(context->edges[i].edge_label);
        }
        free(context->edges);
    }
    
    // 释放执行状态
    if (context->execution_state) {
        workflow_internal_state_t* state = (workflow_internal_state_t*)context->execution_state;
        if (state->node_states) free(state->node_states);
        free(state);
    }
    
    free(context);
}

taskflow_error_t workflow_add_node(
    workflow_context_t* context,
    const workflow_node_t* node)
{
    if (!context || !node || !context->nodes) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    if (context->node_count >= context->config.max_nodes) {
        return TASKFLOW_ERROR_GRAPH_TOO_LARGE;
    }
    
    // 复制节点
    workflow_node_t* dest = &context->nodes[context->node_count];
    dest->node_id = node->node_id;
    dest->node_type = node->node_type;
    dest->task_executor = node->task_executor;
    dest->user_context = node->user_context;
    
    // 复制节点名称
    if (node->node_name) {
        dest->node_name = strdup(node->node_name);
        if (!dest->node_name) return TASKFLOW_ERROR_MEMORY;
    }
    
    // 复制任务数据
    if (node->task_data && node->task_data_size > 0) {
        dest->task_data = malloc(node->task_data_size);
        if (!dest->task_data) {
            if (dest->node_name) free(dest->node_name);
            return TASKFLOW_ERROR_MEMORY;
        }
        memcpy(dest->task_data, node->task_data, node->task_data_size);
        dest->task_data_size = node->task_data_size;
    }
    
    // 添加到TaskFlow图
    graph_vertex_t vertex = {
        .id = node->node_id,
        .state = VERTEX_ACTIVE,
        .value = NULL,
        .value_size = 0,
        .out_degree = 0,
        .in_degree = 0,
        .user_data = (void*)node
    };
    
    taskflow_error_t result = taskflow_graph_add_vertex(context->workflow_graph, &vertex);
    if (result != TASKFLOW_SUCCESS) {
        if (dest->task_data) free(dest->task_data);
        if (dest->node_name) free(dest->node_name);
        return result;
    }
    
    context->node_count++;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t workflow_add_edge(
    workflow_context_t* context,
    const workflow_edge_t* edge)
{
    if (!context || !edge || !context->edges) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    if (context->edge_count >= context->config.max_edges) {
        return TASKFLOW_ERROR_GRAPH_TOO_LARGE;
    }
    
    // 复制边
    workflow_edge_t* dest = &context->edges[context->edge_count];
    dest->edge_id = edge->edge_id;
    dest->source_node = edge->source_node;
    dest->target_node = edge->target_node;
    dest->condition_func = edge->condition_func;
    dest->condition_context = edge->condition_context;
    
    // 复制边标签
    if (edge->edge_label) {
        dest->edge_label = strdup(edge->edge_label);
        if (!dest->edge_label) return TASKFLOW_ERROR_MEMORY;
    }
    
    // 添加到TaskFlow图
    graph_edge_t graph_edge = {
        .id = edge->edge_id,
        .source = edge->source_node,
        .target = edge->target_node,
        .weight = NULL,
        .weight_size = 0,
        .user_data = (void*)edge
    };
    
    taskflow_error_t result = taskflow_graph_add_edge(context->workflow_graph, &graph_edge);
    if (result != TASKFLOW_SUCCESS) {
        if (dest->edge_label) free(dest->edge_label);
        return result;
    }
    
    context->edge_count++;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t workflow_set_start_node(
    workflow_context_t* context,
    vertex_id_t start_node_id)
{
    if (!context) return TASKFLOW_ERROR_INVALID_ARG;
    
    // 验证节点是否存在
    if (find_node_index(context, start_node_id) == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    context->start_node = start_node_id;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t workflow_set_end_node(
    workflow_context_t* context,
    vertex_id_t end_node_id)
{
    if (!context) return TASKFLOW_ERROR_INVALID_ARG;
    
    // 验证节点是否存在
    if (end_node_id != 0 && find_node_index(context, end_node_id) == SIZE_MAX) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    context->end_node = end_node_id;
    return TASKFLOW_SUCCESS;
}

taskflow_error_t workflow_build_sequential(
    workflow_context_t* context,
    const vertex_id_t* node_ids,
    size_t node_count)
{
    if (!context || !node_ids || node_count < 2) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 验证所有节点存在
    for (size_t i = 0; i < node_count; i++) {
        if (find_node_index(context, node_ids[i]) == SIZE_MAX) {
            return TASKFLOW_ERROR_INVALID_ARG;
        }
    }
    
    // 设置起始和结束节点
    context->start_node = node_ids[0];
    context->end_node = node_ids[node_count - 1];
    
    // 添加顺序边
    for (size_t i = 0; i < node_count - 1; i++) {
        workflow_edge_t edge = {
            .edge_id = (edge_id_t)(1000 + i), // 简单ID生成
            .source_node = node_ids[i],
            .target_node = node_ids[i + 1],
            .condition_func = NULL,
            .condition_context = NULL,
            .edge_label = "sequence"
        };
        
        taskflow_error_t result = workflow_add_edge(context, &edge);
        if (result != TASKFLOW_SUCCESS) return result;
    }
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t workflow_build_parallel(
    workflow_context_t* context,
    vertex_id_t start_node_id,
    const vertex_id_t* parallel_node_ids,
    size_t parallel_count,
    vertex_id_t end_node_id)
{
    if (!context || !parallel_node_ids || parallel_count == 0) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 验证所有节点存在
    if (find_node_index(context, start_node_id) == SIZE_MAX) return TASKFLOW_ERROR_INVALID_ARG;
    if (end_node_id != 0 && find_node_index(context, end_node_id) == SIZE_MAX) return TASKFLOW_ERROR_INVALID_ARG;
    
    for (size_t i = 0; i < parallel_count; i++) {
        if (find_node_index(context, parallel_node_ids[i]) == SIZE_MAX) return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 添加从起始节点到所有并行节点的边
    for (size_t i = 0; i < parallel_count; i++) {
        workflow_edge_t edge = {
            .edge_id = (edge_id_t)(2000 + i),
            .source_node = start_node_id,
            .target_node = parallel_node_ids[i],
            .condition_func = NULL,
            .condition_context = NULL,
            .edge_label = "fork"
        };
        
        taskflow_error_t result = workflow_add_edge(context, &edge);
        if (result != TASKFLOW_SUCCESS) return result;
    }
    
    // 添加从所有并行节点到结束节点的边
    for (size_t i = 0; i < parallel_count; i++) {
        workflow_edge_t edge = {
            .edge_id = (edge_id_t)(3000 + i),
            .source_node = parallel_node_ids[i],
            .target_node = end_node_id,
            .condition_func = NULL,
            .condition_context = NULL,
            .edge_label = "join"
        };
        
        taskflow_error_t result = workflow_add_edge(context, &edge);
        if (result != TASKFLOW_SUCCESS) return result;
    }
    
    context->start_node = start_node_id;
    context->end_node = end_node_id;
    
    return TASKFLOW_SUCCESS;
}

taskflow_error_t workflow_build_conditional(
    workflow_context_t* context,
    vertex_id_t condition_node_id,
    vertex_id_t true_branch_node_id,
    vertex_id_t false_branch_node_id,
    vertex_id_t merge_node_id)
{
    // 简化实现
    return TASKFLOW_ERROR_INTERNAL; // 暂未实现
}

taskflow_error_t workflow_build_loop(
    workflow_context_t* context,
    vertex_id_t loop_start_node_id,
    vertex_id_t loop_body_node_id,
    vertex_id_t loop_condition_node_id,
    vertex_id_t loop_end_node_id)
{
    // 简化实现
    return TASKFLOW_ERROR_INTERNAL; // 暂未实现
}

taskflow_error_t workflow_execute_sync(
    workflow_context_t* context,
    size_t max_iterations)
{
    if (!context) return TASKFLOW_ERROR_INVALID_ARG;
    
    if (context->start_node == 0) {
        return TASKFLOW_ERROR_INVALID_ARG; // 未设置起始节点
    }
    
    // 创建或重置内部状态
    workflow_internal_state_t* internal_state = NULL;
    if (context->execution_state) {
        internal_state = (workflow_internal_state_t*)context->execution_state;
    } else {
        internal_state = (workflow_internal_state_t*)calloc(1, sizeof(workflow_internal_state_t));
        if (!internal_state) return TASKFLOW_ERROR_MEMORY;
        
        // 初始化节点状态数组
        internal_state->total_nodes = context->node_count;
        if (context->node_count > 0) {
            internal_state->node_states = (node_execution_state_t*)calloc(
                context->node_count, sizeof(node_execution_state_t));
            if (!internal_state->node_states) {
                free(internal_state);
                return TASKFLOW_ERROR_MEMORY;
            }
            
            // 初始化节点状态
            for (size_t i = 0; i < context->node_count; i++) {
                internal_state->node_states[i].node_id = context->nodes[i].node_id;
                internal_state->node_states[i].executed = false;
                internal_state->node_states[i].succeeded = false;
            }
        }
        
        context->execution_state = internal_state;
    }
    
    internal_state->state = EXECUTION_RUNNING;
    internal_state->async_mode = false;
    
    // 根据配置的模式执行
    taskflow_error_t result = TASKFLOW_SUCCESS;
    
    switch (context->config.pattern_type) {
        case WORKFLOW_SEQUENTIAL:
            result = execute_sequential_workflow(context, internal_state);
            break;
            
        case WORKFLOW_PARALLEL:
            // TODO: 实现并行执行
            result = TASKFLOW_ERROR_INTERNAL;
            break;
            
        default:
            // 默认使用顺序执行
            result = execute_sequential_workflow(context, internal_state);
            break;
    }
    
    return result;
}

taskflow_error_t workflow_execute_async(
    workflow_context_t* context,
    size_t max_iterations,
    void (*callback)(taskflow_error_t result, void* user_data),
    void* user_data)
{
    // 简化实现：调用同步版本
    taskflow_error_t result = workflow_execute_sync(context, max_iterations);
    
    if (callback) {
        callback(result, user_data);
    }
    
    return result;
}

taskflow_error_t workflow_pause(workflow_context_t* context)
{
    if (!context || !context->execution_state) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    workflow_internal_state_t* state = (workflow_internal_state_t*)context->execution_state;
    
    if (state->state == EXECUTION_RUNNING) {
        state->state = EXECUTION_PAUSED;
        return TASKFLOW_SUCCESS;
    }
    
    return TASKFLOW_ERROR_INVALID_ARG; // 不在运行状态
}

taskflow_error_t workflow_resume(workflow_context_t* context)
{
    if (!context || !context->execution_state) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    workflow_internal_state_t* state = (workflow_internal_state_t*)context->execution_state;
    
    if (state->state == EXECUTION_PAUSED) {
        state->state = EXECUTION_RUNNING;
        return TASKFLOW_SUCCESS;
    }
    
    return TASKFLOW_ERROR_INVALID_ARG; // 不在暂停状态
}

taskflow_error_t workflow_stop(workflow_context_t* context)
{
    if (!context || !context->execution_state) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    workflow_internal_state_t* state = (workflow_internal_state_t*)context->execution_state;
    
    if (state->state == EXECUTION_RUNNING || state->state == EXECUTION_PAUSED) {
        state->state = EXECUTION_CANCELLED;
        return TASKFLOW_SUCCESS;
    }
    
    return TASKFLOW_ERROR_INVALID_ARG; // 不在可停止状态
}

taskflow_error_t workflow_get_status(
    workflow_context_t* context,
    size_t* completed_nodes,
    size_t* total_nodes,
    vertex_id_t* current_node)
{
    if (!context || !context->execution_state) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    workflow_internal_state_t* state = (workflow_internal_state_t*)context->execution_state;
    
    if (completed_nodes) *completed_nodes = state->completed_nodes;
    if (total_nodes) *total_nodes = state->total_nodes;
    if (current_node) *current_node = state->current_node;
    
    return TASKFLOW_SUCCESS;
}