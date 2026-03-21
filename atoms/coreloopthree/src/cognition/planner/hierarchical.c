/**
 * @file hierarchical.c
 * @brief 分层规划策略：将复杂任务分解为层次化子任务
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#include "cognition.h"
#include "llm_client.h"
#include "id_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cjson/cJSON.h>

#define MAX_SUBTASKS 16
#define MAX_DEPTH 5

/**
 * @brief 分层规划私有数据
 */
typedef struct hierarchical_data {
    agentos_llm_service_t* llm;      /**< LLM服务客户端 */
    char* model_name;                /**< 模型名称（用户可配置） */
    int max_depth;                    /**< 最大分解深度 */
    agentos_mutex_t* lock;
} hierarchical_data_t;

static void hierarchical_destroy(agentos_plan_strategy_t* strategy) {
    if (!strategy) return;
    hierarchical_data_t* data = (hierarchical_data_t*)strategy->data;
    if (data) {
        if (data->model_name) free(data->model_name);
        if (data->lock) agentos_mutex_destroy(data->lock);
        free(data);
    }
    free(strategy);
}

/**
 * @brief 调用LLM生成子任务列表（JSON格式）
 */
static agentos_error_t generate_subtasks(
    agentos_llm_service_t* llm,
    const char* model_name,
    const char* goal,
    char*** out_subtasks,
    int* out_count) {

    // 构造提示词，要求输出JSON数组
    char prompt[4096];
    snprintf(prompt, sizeof(prompt),
        "请将以下目标分解为一系列子任务，确保分解合理且可执行。\n"
        "目标: %s\n"
        "要求: \n"
        "1. 输出一个JSON数组，每个元素是一个子任务的描述\n"
        "2. 子任务应该具体、可操作，避免过于抽象的描述\n"
        "3. 子任务之间应该有合理的依赖关系\n"
        "4. 分解深度适中，确保每个子任务都能在合理时间内完成\n"
        "\n"
        "示例输出格式: [\"子任务1\", \"子任务2\", \"子任务3\"]",
        goal);

    agentos_llm_request_t req;
    memset(&req, 0, sizeof(req));
    req.model = model_name; // 使用配置的模型
    req.prompt = prompt;
    req.temperature = 0.3; // 降低温度，提高稳定性
    req.max_tokens = 512; // 减少生成长度，提高速度

    agentos_llm_response_t* resp = NULL;
    agentos_error_t err = agentos_llm_complete(llm, &req, &resp);
    if (err != AGENTOS_SUCCESS) return err;

    // 使用cJSON库解析JSON，提高可靠性
    cJSON* root = cJSON_Parse(resp->text);
    if (!root) {
        agentos_llm_response_free(resp);
        return AGENTOS_EINVAL;
    }

    char** subtasks = NULL;
    int count = 0;

    if (cJSON_IsArray(root)) {
        int array_size = cJSON_GetArraySize(root);
        for (int i = 0; i < array_size; i++) {
            cJSON* item = cJSON_GetArrayItem(root, i);
            if (cJSON_IsString(item)) {
                const char* subtask_str = cJSON_GetStringValue(item);
                if (subtask_str && strlen(subtask_str) > 0) {
                    char* subtask = strdup(subtask_str);
                    if (!subtask) {
                        cJSON_Delete(root);
                        agentos_llm_response_free(resp);
                        // 清理已分配的子任务
                        for (int j = 0; j < count; j++) {
                            free(subtasks[j]);
                        }
                        free(subtasks);
                        return AGENTOS_ENOMEM;
                    }
                    subtasks = realloc(subtasks, sizeof(char*) * (count + 1));
                    if (!subtasks) {
                        free(subtask);
                        cJSON_Delete(root);
                        agentos_llm_response_free(resp);
                        // 清理已分配的子任务
                        for (int j = 0; j < count; j++) {
                            free(subtasks[j]);
                        }
                        free(subtasks);
                        return AGENTOS_ENOMEM;
                    }
                    subtasks[count++] = subtask;
                }
            }
        }
    }

    cJSON_Delete(root);
    agentos_llm_response_free(resp);
    *out_subtasks = subtasks;
    *out_count = count;
    return AGENTOS_SUCCESS;
}



/**
 * @brief 确定任务的Agent角色
 */
static char* determine_agent_role(const char* task_desc) {
    // 简单的角色映射，实际应使用更智能的方法
    if (strstr(task_desc, "代码") || strstr(task_desc, "编程") || strstr(task_desc, "开发")) {
        return strdup("developer");
    } else if (strstr(task_desc, "测试") || strstr(task_desc, "验证")) {
        return strdup("tester");
    } else if (strstr(task_desc, "分析") || strstr(task_desc, "研究")) {
        return strdup("analyst");
    } else if (strstr(task_desc, "文档") || strstr(task_desc, "报告")) {
        return strdup("writer");
    } else {
        return strdup("default");
    }
}

/**
 * @brief 递归构建任务DAG
 */
static agentos_error_t build_hierarchical_plan(
    agentos_llm_service_t* llm,
    const char* model_name,
    const char* goal,
    int depth,
    int max_depth,
    agentos_task_plan_t* plan) {

    if (depth >= max_depth) {
        // 达到最大深度，将当前目标作为一个任务节点
        agentos_task_node_t* node = (agentos_task_node_t*)calloc(1, sizeof(agentos_task_node_t));
        if (!node) return AGENTOS_ENOMEM;

        char task_id_buf[64];
        agentos_generate_task_id("task", task_id_buf, sizeof(task_id_buf));
        node->task_id = strdup(task_id_buf); // 生成唯一ID
        node->agent_role = determine_agent_role(goal); // 智能分配角色
        node->depends_on = NULL;
        node->depends_count = 0;
        node->timeout_ms = 30000;
        node->priority = 128 - depth * 10; // 深度越小优先级越高
        node->input = NULL; // 可扩展

        // 添加到计划
        plan->nodes = realloc(plan->nodes, sizeof(agentos_task_node_t*) * (plan->node_count + 1));
        if (!plan->nodes) {
            free(node->task_id);
            free(node->agent_role);
            free(node);
            return AGENTOS_ENOMEM;
        }
        plan->nodes[plan->node_count++] = node;
        return AGENTOS_SUCCESS;
    }

    // 生成子任务
    char** subtasks = NULL;
    int subtask_count = 0;
    agentos_error_t err = generate_subtasks(llm, model_name, goal, &subtasks, &subtask_count);
    if (err != AGENTOS_SUCCESS) return err;

    if (subtask_count == 0) {
        // 没有子任务，直接作为叶子
        err = build_hierarchical_plan(llm, model_name, goal, max_depth, max_depth, plan);
    } else {
        // 记录子任务开始索引
        size_t start_index = plan->node_count;
        
        // 对每个子任务递归构建
        for (int i = 0; i < subtask_count; i++) {
            err = build_hierarchical_plan(llm, model_name, subtasks[i], depth + 1, max_depth, plan);
            if (err != AGENTOS_SUCCESS) {
                for (int j = 0; j < subtask_count; j++) free(subtasks[j]);
                free(subtasks);
                return err;
            }
            free(subtasks[i]);
        }
        free(subtasks);
        
        // 为子任务建立依赖关系（简单的顺序依赖）
        for (size_t i = start_index + 1; i < plan->node_count; i++) {
            agentos_task_node_t* current = plan->nodes[i];
            current->depends_count = 1;
            current->depends_on = (char**)malloc(sizeof(char*));
            if (!current->depends_on) {
                return AGENTOS_ENOMEM;
            }
            current->depends_on[0] = plan->nodes[i - 1]->task_id;
        }
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 分层规划函数
 */
static agentos_error_t hierarchical_plan(
    const agentos_intent_t* intent,
    void* context,
    agentos_task_plan_t** out_plan) {

    hierarchical_data_t* data = (hierarchical_data_t*)context;
    if (!data || !intent || !out_plan) return AGENTOS_EINVAL;

    agentos_task_plan_t* plan = (agentos_task_plan_t*)calloc(1, sizeof(agentos_task_plan_t));
    if (!plan) return AGENTOS_ENOMEM;

    // 生成唯一计划ID
    char plan_id_buf[64];
    agentos_generate_plan_id(plan_id_buf, sizeof(plan_id_buf));
    plan->plan_id = strdup(plan_id_buf);
    if (!plan->plan_id) {
        free(plan);
        return AGENTOS_ENOMEM;
    }
    plan->nodes = NULL;
    plan->node_count = 0;
    plan->entry_points = NULL;
    plan->entry_count = 0;

    agentos_error_t err = build_hierarchical_plan(
        data->llm, data->model_name, intent->goal, 0, data->max_depth, plan);

    if (err != AGENTOS_SUCCESS) {
        // 清理
        for (size_t i = 0; i < plan->node_count; i++) {
            free(plan->nodes[i]->task_id);
            free(plan->nodes[i]->agent_role);
            if (plan->nodes[i]->depends_on) {
                free(plan->nodes[i]->depends_on);
            }
            free(plan->nodes[i]);
        }
        free(plan->nodes);
        free(plan->plan_id);
        free(plan);
        return err;
    }

    // 确定入口点（没有依赖的任务）
    size_t entry_count = 0;
    for (size_t i = 0; i < plan->node_count; i++) {
        if (plan->nodes[i]->depends_count == 0) {
            entry_count++;
        }
    }

    if (entry_count > 0) {
        plan->entry_points = (char**)malloc(sizeof(char*) * entry_count);
        if (plan->entry_points) {
            plan->entry_count = entry_count;
            size_t index = 0;
            for (size_t i = 0; i < plan->node_count; i++) {
                if (plan->nodes[i]->depends_count == 0) {
                    plan->entry_points[index++] = plan->nodes[i]->task_id;
                }
            }
        }
    }

    *out_plan = plan;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建分层规划策略
 */
agentos_plan_strategy_t* agentos_plan_hierarchical_create(
    agentos_llm_service_t* llm,
    const char* model_name,
    int max_depth) {

    if (!llm || !model_name) return NULL;
    if (max_depth <= 0) max_depth = MAX_DEPTH;

    agentos_plan_strategy_t* strat = (agentos_plan_strategy_t*)malloc(sizeof(agentos_plan_strategy_t));
    if (!strat) return NULL;

    hierarchical_data_t* data = (hierarchical_data_t*)malloc(sizeof(hierarchical_data_t));
    if (!data) {
        free(strat);
        return NULL;
    }

    data->llm = llm;
    data->model_name = strdup(model_name);
    if (!data->model_name) {
        free(data);
        free(strat);
        return NULL;
    }
    data->max_depth = max_depth;
    data->lock = agentos_mutex_create();
    if (!data->lock) {
        free(data->model_name);
        free(data);
        free(strat);
        return NULL;
    }

    strat->plan = hierarchical_plan;
    strat->destroy = hierarchical_destroy;
    strat->data = data;

    return strat;
}