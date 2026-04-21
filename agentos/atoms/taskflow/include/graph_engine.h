// SPDX-FileCopyrightText: 2026 SPHARX.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file graph_engine.h
 * @brief Graph Engine for TaskFlow System
 * 
 * 图引擎模块，负责图的存储、遍历和基本操作。
 * 支持有向图、无向图、加权图等数据结构。
 */

#ifndef AGENTOS_GRAPH_ENGINE_H
#define AGENTOS_GRAPH_ENGINE_H

#include "taskflow_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 图引擎句柄
// ============================================================================

/**
 * @brief 图引擎句柄（不透明类型）
 */
typedef struct graph_engine_s* graph_engine_handle_t;

// ============================================================================
// 图引擎API
// ============================================================================

/**
 * @brief 创建图引擎实例
 * @param config 图引擎配置
 * @return 图引擎句柄，失败返回 NULL
 */
graph_engine_handle_t graph_engine_create(const taskflow_config_t* config);

/**
 * @brief 销毁图引擎实例
 * @param engine 图引擎句柄
 */
void graph_engine_destroy(graph_engine_handle_t engine);

/**
 * @brief 初始化图引擎
 * @param engine 图引擎句柄
 * @return 错误码
 */
taskflow_error_t graph_engine_init(graph_engine_handle_t engine);

/**
 * @brief 加载图数据
 * @param engine 图引擎句柄
 * @param vertices 顶点数组
 * @param vertex_count 顶点数量
 * @param edges 边数组
 * @param edge_count 边数量
 * @return 错误码
 */
taskflow_error_t graph_engine_load(graph_engine_handle_t engine,
                                  const graph_vertex_t* vertices,
                                  size_t vertex_count,
                                  const graph_edge_t* edges,
                                  size_t edge_count);

/**
 * @brief 保存图数据
 * @param engine 图引擎句柄
 * @param vertices 顶点数组（输出）
 * @param max_vertices 最大顶点容量
 * @param edges 边数组（输出）
 * @param max_edges 最大边容量
 * @param actual_vertices 实际顶点数（输出）
 * @param actual_edges 实际边数（输出）
 * @return 错误码
 */
taskflow_error_t graph_engine_save(graph_engine_handle_t engine,
                                  graph_vertex_t* vertices,
                                  size_t max_vertices,
                                  graph_edge_t* edges,
                                  size_t max_edges,
                                  size_t* actual_vertices,
                                  size_t* actual_edges);

/**
 * @brief 添加顶点
 * @param engine 图引擎句柄
 * @param vertex 顶点结构
 * @return 错误码
 */
taskflow_error_t graph_engine_add_vertex(graph_engine_handle_t engine,
                                        const graph_vertex_t* vertex);

/**
 * @brief 移除顶点
 * @param engine 图引擎句柄
 * @param vertex_id 顶点ID
 * @return 错误码
 */
taskflow_error_t graph_engine_remove_vertex(graph_engine_handle_t engine,
                                           vertex_id_t vertex_id);

/**
 * @brief 添加边
 * @param engine 图引擎句柄
 * @param edge 边结构
 * @return 错误码
 */
taskflow_error_t graph_engine_add_edge(graph_engine_handle_t engine,
                                      const graph_edge_t* edge);

/**
 * @brief 移除边
 * @param engine 图引擎句柄
 * @param edge_id 边ID
 * @return 错误码
 */
taskflow_error_t graph_engine_remove_edge(graph_engine_handle_t engine,
                                         edge_id_t edge_id);

/**
 * @brief 获取顶点
 * @param engine 图引擎句柄
 * @param vertex_id 顶点ID
 * @param vertex 顶点结构（输出）
 * @return 错误码
 */
taskflow_error_t graph_engine_get_vertex(graph_engine_handle_t engine,
                                        vertex_id_t vertex_id,
                                        graph_vertex_t* vertex);

/**
 * @brief 获取边
 * @param engine 图引擎句柄
 * @param edge_id 边ID
 * @param edge 边结构（输出）
 * @return 错误码
 */
taskflow_error_t graph_engine_get_edge(graph_engine_handle_t engine,
                                      edge_id_t edge_id,
                                      graph_edge_t* edge);

/**
 * @brief 获取顶点的出边
 * @param engine 图引擎句柄
 * @param vertex_id 顶点ID
 * @param edges 边数组（输出）
 * @param max_edges 最大边容量
 * @return 实际边数量
 */
size_t graph_engine_get_out_edges(graph_engine_handle_t engine,
                                 vertex_id_t vertex_id,
                                 graph_edge_t* edges,
                                 size_t max_edges);

/**
 * @brief 获取顶点的入边
 * @param engine 图引擎句柄
 * @param vertex_id 顶点ID
 * @param edges 边数组（输出）
 * @param max_edges 最大边容量
 * @return 实际边数量
 */
size_t graph_engine_get_in_edges(graph_engine_handle_t engine,
                                vertex_id_t vertex_id,
                                graph_edge_t* edges,
                                size_t max_edges);

/**
 * @brief 获取邻居顶点
 * @param engine 图引擎句柄
 * @param vertex_id 顶点ID
 * @param neighbors 邻居顶点ID数组（输出）
 * @param max_neighbors 最大邻居容量
 * @return 实际邻居数量
 */
size_t graph_engine_get_neighbors(graph_engine_handle_t engine,
                                 vertex_id_t vertex_id,
                                 vertex_id_t* neighbors,
                                 size_t max_neighbors);

/**
 * @brief 广度优先遍历
 * @param engine 图引擎句柄
 * @param start_vertex 起始顶点ID
 * @param visitor 访问者回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
taskflow_error_t graph_engine_bfs(graph_engine_handle_t engine,
                                 vertex_id_t start_vertex,
                                 void (*visitor)(vertex_id_t vertex_id, void* user_data),
                                 void* user_data);

/**
 * @brief 深度优先遍历
 * @param engine 图引擎句柄
 * @param start_vertex 起始顶点ID
 * @param visitor 访问者回调函数
 * @param user_data 用户数据
 * @return 错误码
 */
taskflow_error_t graph_engine_dfs(graph_engine_handle_t engine,
                                 vertex_id_t start_vertex,
                                 void (*visitor)(vertex_id_t vertex_id, void* user_data),
                                 void* user_data);

/**
 * @brief 获取图统计信息
 * @param engine 图引擎句柄
 * @param vertex_count 顶点数量（输出）
 * @param edge_count 边数量（输出）
 * @param max_out_degree 最大出度（输出）
 * @param max_in_degree 最大入度（输出）
 * @return 错误码
 */
taskflow_error_t graph_engine_get_stats(graph_engine_handle_t engine,
                                       size_t* vertex_count,
                                       size_t* edge_count,
                                       uint32_t* max_out_degree,
                                       uint32_t* max_in_degree);

/**
 * @brief 清空图数据
 * @param engine 图引擎句柄
 * @return 错误码
 */
taskflow_error_t graph_engine_clear(graph_engine_handle_t engine);

/**
 * @brief 检查图是否为空
 * @param engine 图引擎句柄
 * @return 是否为空
 */
bool graph_engine_is_empty(graph_engine_handle_t engine);

/**
 * @brief 获取所有顶点ID列表
 * @param engine 图引擎句柄
 * @param out_ids 顶点ID数组（输出）
 * @param max_count 数组最大容量
 * @param out_actual 实际获取的顶点数量（输出）
 * @return 错误码
 */
taskflow_error_t graph_engine_get_vertex_ids(graph_engine_handle_t engine,
                                             vertex_id_t* out_ids,
                                             size_t max_count,
                                             size_t* out_actual);

#ifdef __cplusplus
}
#endif

#endif // AGENTOS_GRAPH_ENGINE_H