/**
 * @file task.c
 * @brief 任务管理系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "coreloopthree/cognition.h"
#include "coreloopthree/execution.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <errno.h>
#include <cjson/cJSON.h>

static agentos_cognition_engine_t* g_cognition = NULL;
static agentos_execution_engine_t* g_execution = NULL;

void agentos_sys_init(void* cognition, void* execution, void* memory) {
    g_cognition = (agentos_cognition_engine_t*)cognition;
    g_execution = (agentos_execution_engine_t*)execution;
    // memory �?memory.c 使用，通过单独�?setter 设置
}

/* -------------------- 辅助函数 -------------------- */
// From data intelligence emerges. by spharx

/**
 * 拓扑排序：根据依赖关系返回可执行的任务顺序（简单实现，假设无环�?
 * 返回节点索引数组，需调用�?free
 */
static int* topological_sort(agentos_task_plan_t* plan, size_t* out_count) {
    size_t n = plan->task_plan_node_count;
    int* order = (int*)AGENTOS_MALLOC(n * sizeof(int));
    if (!order) return NULL;
    int* indeg = (int*)AGENTOS_CALLOC(n, sizeof(int));
    if (!indeg) {
        AGENTOS_FREE(order);
        return NULL;
    }

    // 建立节点名到索引的映�?
    cJSON* name_to_idx = cJSON_CreateObject();
    for (size_t i = 0; i < n; i++) {
        char idx_str[16];
        snprintf(idx_str, sizeof(idx_str), "%zu", i);
        cJSON_AddStringToObject(name_to_idx, plan->task_plan_nodes[i]->task_node_id, idx_str);
    }

    // 计算入度
    for (size_t i = 0; i < n; i++) {
        agentos_task_node_t* node = plan->task_plan_nodes[i];
        for (size_t j = 0; j < node->task_node_depends_count; j++) {
            cJSON* dep_idx = cJSON_GetObjectItem(name_to_idx, node->task_node_depends_on[j]);
            if (dep_idx && cJSON_IsString(dep_idx)) {
                int idx = atoi(dep_idx->valuestring);
                if (idx >= 0 && idx < (int)n) indeg[idx]++;
            } else {
                AGENTOS_LOG_WARN("Dependency %s not found in plan", node->task_node_depends_on[j]);
            }
        }
    }

    // Kahn 算法
    size_t pos = 0;
    int* queue = (int*)AGENTOS_MALLOC(n * sizeof(int));
    if (!queue) {
        cJSON_Delete(name_to_idx);
        AGENTOS_FREE(indeg);
        AGENTOS_FREE(order);
        return NULL;
    }
    int qhead = 0, qtail = 0;
    for (size_t i = 0; i < n; i++) {
        if (indeg[i] == 0) queue[qtail++] = i;
    }
    while (qhead < qtail) {
        int u = queue[qhead++];
        order[pos++] = u;
        agentos_task_node_t* node = plan->task_plan_nodes[u];
        for (size_t j = 0; j < node->task_node_depends_count; j++) {
            cJSON* dep_idx = cJSON_GetObjectItem(name_to_idx, node->task_node_depends_on[j]);
            if (dep_idx && cJSON_IsString(dep_idx)) {
                char* endptr;
                errno = 0;
                long v_val = strtol(dep_idx->valuestring, &endptr, 10);
                if (endptr != dep_idx->valuestring && *endptr == '\0' && errno == 0 &&
                    v_val >= 0 && v_val < (int)n) {
                    int v = (int)v_val;
                    if (--indeg[v] == 0) {
                        if (qtail >= (int)n) {
                            // 队列溢出，图有问题（可能有环或无效依赖）
                            AGENTOS_LOG_ERROR("Topological sort queue overflow during processing");
                            AGENTOS_FREE(queue);
                            cJSON_Delete(name_to_idx);
                            AGENTOS_FREE(indeg);
                            AGENTOS_FREE(order);
                            return NULL;
                        }
                        queue[qtail++] = v;
                    }
                } else {
                    AGENTOS_LOG_WARN("Invalid dependency index: %s", dep_idx->valuestring);
                }
            }
        }
    }
    AGENTOS_FREE(queue);
    cJSON_Delete(name_to_idx);
    AGENTOS_FREE(indeg);

    if (pos != n) {
        // 有环
        AGENTOS_FREE(order);
        return NULL;
    }
    *out_count = n;
    return order;
}

/**
 * 执行单个任务节点，返回输出字符串（需释放�?
 */
static char* execute_node(agentos_task_node_t* node, uint32_t timeout_ms) {
    agentos_task_t task;
    memset(&task, 0, sizeof(task));
    task.task_agent_id = node->task_node_agent_role;
    task.task_input = node->task_node_input;
    task.task_timeout_ms = node->task_node_timeout_ms ? node->task_node_timeout_ms : timeout_ms;

    char* task_id = NULL;
    agentos_error_t err = agentos_execution_submit(g_execution, &task, &task_id);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to submit node %s", node->task_node_id);
        return NULL;
    }

    agentos_task_t* result_task = NULL;
    err = agentos_execution_wait(g_execution, task_id, timeout_ms, &result_task);
    AGENTOS_FREE(task_id);
    if (err != AGENTOS_SUCCESS || !result_task) {
        AGENTOS_LOG_ERROR("Node %s execution failed", node->task_node_id);
        return NULL;
    }

    char* output = NULL;
    if (result_task->task_output) {
        output = AGENTOS_STRDUP((char*)result_task->task_output);
    } else {
        output = AGENTOS_STRDUP("");
    }
    agentos_task_free(result_task);
    return output;
}

/* -------------------- 公共接口 -------------------- */

agentos_error_t agentos_sys_task_submit(const char* input, size_t input_len,
                                        uint32_t timeout_ms, char** out_result) {
    if (!input || !out_result) return AGENTOS_EINVAL;
    if (!g_cognition || !g_execution) return AGENTOS_ENOTINIT;

    agentos_task_plan_t* plan = NULL;
    agentos_error_t err = agentos_cognition_process(g_cognition, input, input_len, &plan);
    if (err != AGENTOS_SUCCESS) return err;
    if (!plan || plan->task_plan_node_count == 0) {
        agentos_task_plan_free(plan);
        return AGENTOS_EINVAL;
    }

    // 拓扑排序
    size_t order_count = 0;
    int* order = topological_sort(plan, &order_count);
    if (!order) {
        agentos_task_plan_free(plan);
        return AGENTOS_EINVAL;
    }

    // 按顺序执行节�?
    cJSON* result_obj = cJSON_CreateObject();
    for (size_t i = 0; i < order_count; i++) {
        int idx = order[i];
        agentos_task_node_t* node = plan->task_plan_nodes[idx];
        char* output = execute_node(node, timeout_ms);
        if (output) {
            cJSON_AddStringToObject(result_obj, node->task_node_id, output);
            AGENTOS_FREE(output);
        } else {
            cJSON_AddStringToObject(result_obj, node->task_node_id, "ERROR");
        }
    }
    AGENTOS_FREE(order);
    agentos_task_plan_free(plan);

    char* json = cJSON_PrintUnformatted(result_obj);
    cJSON_Delete(result_obj);
    if (!json) return AGENTOS_ENOMEM;
    *out_result = json;
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sys_task_query(const char* task_id, int* out_status) {
    if (!task_id || !out_status) return AGENTOS_EINVAL;
    if (!g_execution) return AGENTOS_ENOTINIT;
    agentos_task_status_t status;
    agentos_error_t err = agentos_execution_query(g_execution, task_id, &status);
    if (err == AGENTOS_SUCCESS) {
        *out_status = (int)status;
    }
    return err;
}

agentos_error_t agentos_sys_task_wait(const char* task_id, uint32_t timeout_ms, char** out_result) {
    if (!task_id || !out_result) return AGENTOS_EINVAL;
    if (!g_execution) return AGENTOS_ENOTINIT;
    agentos_task_t* result_task = NULL;
    agentos_error_t err = agentos_execution_wait(g_execution, task_id, timeout_ms, &result_task);
    if (err == AGENTOS_SUCCESS && result_task) {
        if (result_task->task_output) {
            *out_result = AGENTOS_STRDUP((char*)result_task->task_output);
        } else {
            *out_result = AGENTOS_STRDUP("");
        }
        agentos_task_free(result_task);
    }
    return err;
}

agentos_error_t agentos_sys_task_cancel(const char* task_id) {
    if (!task_id) return AGENTOS_EINVAL;
    if (!g_execution) return AGENTOS_ENOTINIT;
    return agentos_execution_cancel(g_execution, task_id);
}