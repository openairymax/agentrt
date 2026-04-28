// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file taskflow_execution_unit.c
 * @brief TaskFlow执行单元实现
 * 
 * 实现TaskFlow作为Atoms执行单元的适配器。
 */

#include "taskflow_integration.h"
#include "taskflow.h"
#include "graph_engine.h"
#include "../../coreloopthree/include/execution.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// 内部数据结构
// ============================================================================

// TaskFlow执行单元私有数据
typedef struct {
    taskflow_unit_config_t config;          // 单元配置
    taskflow_handle_t taskflow_engine;      // TaskFlow引擎
    agentos_execution_unit_t unit;          // 执行单元接口
    bool initialized;                       // 是否已初始化
} taskflow_unit_private_t;

// TaskFlow任务执行上下文
typedef struct {
    taskflow_unit_private_t* unit_private;  // 所属单元
    taskflow_task_input_t* input;           // 任务输入
    taskflow_task_output_t* output;         // 任务输出
    taskflow_graph_handle_t graph;          // 图句柄（临时）
    bool graph_owned;                       // 是否拥有图所有权
} taskflow_execution_context_t;

// ============================================================================
// 静态辅助函数
// ============================================================================

// 执行单元执行方法（接口实现）
static agentos_error_t taskflow_unit_execute_impl(
    agentos_execution_unit_t* unit,
    const void* input_data,
    void** out_output)
{
    if (!unit || !input_data || !out_output) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    taskflow_unit_private_t* private = (taskflow_unit_private_t*)unit->execution_unit_data;
    if (!private || !private->initialized) {
        return TASKFLOW_ERROR_NOT_INITIALIZED;
    }
    
    // 解析输入数据
    const agentos_task_t* agentos_task = (const agentos_task_t*)input_data;
    taskflow_task_input_t* task_input = taskflow_parse_task_input(agentos_task);
    if (!task_input) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 创建执行上下文
    taskflow_execution_context_t* context = 
        (taskflow_execution_context_t*)calloc(1, sizeof(taskflow_execution_context_t));
    if (!context) {
        taskflow_task_input_destroy(task_input);
        return TASKFLOW_ERROR_MEMORY;
    }
    
    context->unit_private = private;
    context->input = task_input;
    context->graph_owned = false;
    
    // 创建输出
    context->output = taskflow_task_output_create();
    if (!context->output) {
        free(context);
        taskflow_task_input_destroy(task_input);
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 执行图计算
    taskflow_error_t tf_result = TASKFLOW_ERROR_INTERNAL;
    taskflow_handle_t engine = private->taskflow_engine;
    
    // 如果有图句柄，直接使用；否则从顶点/边数据创建图
    if (task_input->graph) {
        context->graph = task_input->graph;
        context->graph_owned = false;
    } else {
        // 创建新图
        context->graph = taskflow_graph_create(engine);
        if (!context->graph) {
            tf_result = TASKFLOW_ERROR_MEMORY;
            goto cleanup;
        }
        context->graph_owned = true;
        
        // 添加顶点
        for (size_t i = 0; i < task_input->vertex_count; i++) {
            tf_result = taskflow_graph_add_vertex(context->graph, &task_input->vertices[i]);
            if (tf_result != TASKFLOW_SUCCESS) {
                goto cleanup;
            }
        }
        
        // 添加边
        for (size_t i = 0; i < task_input->edge_count; i++) {
            tf_result = taskflow_graph_add_edge(context->graph, &task_input->edges[i]);
            if (tf_result != TASKFLOW_SUCCESS) {
                goto cleanup;
            }
        }
    }
    
    // 执行计算
    tf_result = taskflow_execute_sync(engine, context->graph, task_input->max_supersteps);
    
cleanup:
    // 设置输出
    context->output->result = tf_result;
    
    if (tf_result == TASKFLOW_SUCCESS) {
        context->output->completed_supersteps = taskflow_get_current_superstep(engine);
        context->output->active_vertices = 0; // 需要从引擎获取
        
        // 获取统计信息
        taskflow_get_stats(engine, &context->output->stats);
    }
    
    // 如果拥有图所有权，销毁图
    if (context->graph_owned && context->graph) {
        taskflow_graph_destroy(context->graph);
    }
    
    // 返回输出
    *out_output = context->output;
    
    // 清理输入（输出由调用者清理）
    taskflow_task_input_destroy(task_input);
    
    // 注意：不释放context，它被包装在output中或需要单独管理
    // 将context存储在output的user_data中
    context->output->result_data = context;
    context->output->result_data_size = sizeof(taskflow_execution_context_t);
    
    return TASKFLOW_SUCCESS;
}

// 执行单元销毁方法（接口实现）
static void taskflow_unit_destroy_impl(agentos_execution_unit_t* unit)
{
    if (!unit) return;
    
    taskflow_unit_private_t* private = (taskflow_unit_private_t*)unit->execution_unit_data;
    if (!private) return;
    
    // 销毁TaskFlow引擎
    if (private->taskflow_engine) {
        taskflow_engine_destroy(private->taskflow_engine);
    }
    
    free(private);
    // 注意：不释放unit本身，因为它可能由调用者分配在栈上
}

// 执行单元获取元数据方法（接口实现）
static const char* taskflow_unit_get_metadata_impl(agentos_execution_unit_t* unit)
{
    if (!unit) return "TaskFlow Unit (NULL)";
    
    return "TaskFlow Graph Execution Unit v1.0 - Provides distributed graph computation based on Pregel model";
}

// ============================================================================
// 公共API实现
// ============================================================================

agentos_execution_unit_t* taskflow_unit_create(const taskflow_unit_config_t* config)
{
    if (!config) return NULL;
    
    // 分配私有数据
    taskflow_unit_private_t* private = 
        (taskflow_unit_private_t*)calloc(1, sizeof(taskflow_unit_private_t));
    if (!private) return NULL;
    
    // 复制配置
    private->config = *config;
    
    // 创建TaskFlow引擎
    private->taskflow_engine = taskflow_engine_create(&config->taskflow_config);
    if (!private->taskflow_engine) {
        free(private);
        return NULL;
    }
    
    // 初始化TaskFlow引擎
    taskflow_error_t result = taskflow_engine_init(private->taskflow_engine);
    if (result != TASKFLOW_SUCCESS) {
        taskflow_engine_destroy(private->taskflow_engine);
        free(private);
        return NULL;
    }
    
    // 设置执行单元接口
    private->unit.execution_unit_data = private;
    private->unit.execution_unit_execute = taskflow_unit_execute_impl;
    private->unit.execution_unit_destroy = taskflow_unit_destroy_impl;
    private->unit.execution_unit_get_metadata = taskflow_unit_get_metadata_impl;
    
    private->initialized = true;
    
    // 分配并返回执行单元
    agentos_execution_unit_t* unit = 
        (agentos_execution_unit_t*)calloc(1, sizeof(agentos_execution_unit_t));
    if (!unit) {
        taskflow_unit_destroy_impl(&private->unit);
        return NULL;
    }
    
    *unit = private->unit;
    return unit;
}

void taskflow_unit_destroy(agentos_execution_unit_t* unit)
{
    if (!unit) return;
    
    // 调用销毁实现
    taskflow_unit_destroy_impl(unit);
    free(unit);
}

taskflow_task_input_t* taskflow_task_input_create(
    taskflow_graph_handle_t graph,
    const graph_vertex_t* vertices,
    size_t vertex_count,
    const graph_edge_t* edges,
    size_t edge_count,
    size_t max_supersteps)
{
    taskflow_task_input_t* input = 
        (taskflow_task_input_t*)calloc(1, sizeof(taskflow_task_input_t));
    if (!input) return NULL;
    
    input->graph = graph;
    input->max_supersteps = max_supersteps;
    
    // 如果有顶点数据，复制它们
    if (vertices && vertex_count > 0) {
        input->vertex_count = vertex_count;
        input->vertices = (graph_vertex_t*)malloc(vertex_count * sizeof(graph_vertex_t));
        if (!input->vertices) {
            free(input);
            return NULL;
        }
        memcpy(input->vertices, vertices, vertex_count * sizeof(graph_vertex_t));
    }
    
    // 如果有边数据，复制它们
    if (edges && edge_count > 0) {
        input->edge_count = edge_count;
        input->edges = (graph_edge_t*)malloc(edge_count * sizeof(graph_edge_t));
        if (!input->edges) {
            if (input->vertices) free(input->vertices);
            free(input);
            return NULL;
        }
        memcpy(input->edges, edges, edge_count * sizeof(graph_edge_t));
    }
    
    return input;
}

void taskflow_task_input_destroy(taskflow_task_input_t* input)
{
    if (!input) return;
    
    if (input->vertices) free(input->vertices);
    if (input->edges) free(input->edges);
    
    // 注意：不销毁graph，因为它可能由外部管理
    
    free(input);
}

taskflow_task_output_t* taskflow_task_output_create(void)
{
    taskflow_task_output_t* output = 
        (taskflow_task_output_t*)calloc(1, sizeof(taskflow_task_output_t));
    if (!output) return NULL;
    
    output->result = TASKFLOW_SUCCESS;
    output->completed_supersteps = 0;
    output->active_vertices = 0;
    memset(&output->stats, 0, sizeof(output->stats));
    output->result_data = NULL;
    output->result_data_size = 0;
    
    return output;
}

void taskflow_task_output_destroy(taskflow_task_output_t* output)
{
    if (!output) return;
    
    if (output->result_data) free(output->result_data);
    free(output);
}

taskflow_task_input_t* taskflow_parse_task_input(const agentos_task_t* agentos_task)
{
    if (!agentos_task || !agentos_task->task_input) return NULL;
    
    const taskflow_task_input_t* src = (const taskflow_task_input_t*)agentos_task->task_input;
    
    taskflow_task_input_t* input = taskflow_task_input_create(
        src->graph,
        src->vertices, src->vertex_count,
        src->edges, src->edge_count,
        src->max_supersteps);
    if (!input) return NULL;
    
    input->graph_config = src->graph_config;
    input->user_context = src->user_context;
    
    return input;
}

taskflow_error_t taskflow_pack_task_output(const taskflow_task_output_t* output,
                                          agentos_task_t* agentos_task)
{
    if (!output || !agentos_task) return TASKFLOW_ERROR_INVALID_ARG;

    taskflow_task_output_t* packed = taskflow_task_output_create();
    if (!packed) return TASKFLOW_ERROR_MEMORY;

    packed->result = output->result;
    packed->completed_supersteps = output->completed_supersteps;
    packed->active_vertices = output->active_vertices;
    packed->stats = output->stats;

    if (output->result_data && output->result_data_size > 0) {
        packed->result_data = malloc(output->result_data_size);
        if (!packed->result_data) {
            taskflow_task_output_destroy(packed);
            return TASKFLOW_ERROR_MEMORY;
        }
        memcpy(packed->result_data, output->result_data, output->result_data_size);
        packed->result_data_size = output->result_data_size;
    }

    if (agentos_task->task_output) {
        taskflow_task_output_destroy((taskflow_task_output_t*)agentos_task->task_output);
    }
    agentos_task->task_output = packed;

    if (output->result == TASKFLOW_SUCCESS) {
        agentos_task->task_status = TASK_STATUS_SUCCEEDED;
    } else {
        agentos_task->task_status = TASK_STATUS_FAILED;
    }

    return TASKFLOW_SUCCESS;
}

agentos_error_t taskflow_register_unit(
    agentos_execution_engine_t* engine,
    const char* unit_name,
    const taskflow_unit_config_t* config)
{
    if (!engine || !unit_name || !config) {
        return TASKFLOW_ERROR_INVALID_ARG;
    }
    
    // 创建TaskFlow执行单元
    agentos_execution_unit_t* unit = taskflow_unit_create(config);
    if (!unit) {
        return TASKFLOW_ERROR_MEMORY;
    }
    
    // 注册到执行引擎
    agentos_error_t result = agentos_execution_register_unit(engine, unit_name, *unit);
    
    // 注意：agentos_execution_register_unit复制单元，所以我们可以释放本地副本
    taskflow_unit_destroy(unit);
    
    return result;
}

void taskflow_unregister_unit(
    agentos_execution_engine_t* engine,
    const char* unit_name)
{
    if (!engine || !unit_name) return;
    
    agentos_execution_unregister_unit(engine, unit_name);
}