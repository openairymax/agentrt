/**
 * @file round_robin.c
 * @brief 轮询调度策略实现
 * @details 按照注册顺序依次选择可用的 Agent
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scheduler_service.h"
#include "strategy_interface.h"

/**
 * @brief 错误码定义
 */
#define ROUND_ROBIN_SUCCESS 0
#define ROUND_ROBIN_ERROR_INVALID_PARAM -1
#define ROUND_ROBIN_ERROR_NO_AGENT -2
#define ROUND_ROBIN_ERROR_AGENT_NOT_FOUND -3
#define ROUND_ROBIN_ERROR_NO_AVAILABLE_AGENT -4
#define ROUND_ROBIN_ERROR_MEMORY -5
#define ROUND_ROBIN_ERROR_MAX_AGENTS -6

/**
 * @brief 轮询调度策略数据
 */
typedef struct {
    agent_info_t** agents;      /**< Agent 列表 */
    size_t agent_count;         /**< Agent 数量 */
    size_t current_index;       /**< 当前索引 */
    size_t max_agents;          /**< 最大 Agent 数量 */
} round_robin_data_t;

/**
 * @brief 安全复制字符串
 * @param src 源字符串
 * @return 复制后的字符串，失败返回NULL
 */
static char* safe_strdup(const char* src) {
    if (!src) {
        return NULL;
    }
    char* dest = strdup(src);
    return dest;
}

/**
 * @brief 释放Agent信息
 * @param agent Agent指针
 */
static void free_agent_info(agent_info_t* agent) {
    if (!agent) return;
    if (agent->agent_id) {
        free(agent->agent_id);
        agent->agent_id = NULL;
    }
    if (agent->agent_name) {
        free(agent->agent_name);
        agent->agent_name = NULL;
    }
    free(agent);
}

/**
 * @brief 复制Agent信息
 * @param src 源Agent信息
 * @return 复制后的Agent信息，失败返回NULL
 */
static agent_info_t* clone_agent_info(const agent_info_t* src) {
    if (!src) {
        return NULL;
    }

    agent_info_t* dest = (agent_info_t*)malloc(sizeof(agent_info_t));
    if (!dest) {
        return NULL;
    }
    memset(dest, 0, sizeof(agent_info_t));

    if (src->agent_id) {
        dest->agent_id = safe_strdup(src->agent_id);
        if (!dest->agent_id) {
            free(dest);
            return NULL;
        }
    }

    if (src->agent_name) {
        dest->agent_name = safe_strdup(src->agent_name);
        if (!dest->agent_name) {
            free(dest->agent_id);
            free(dest);
            return NULL;
        }
    }

    dest->load_factor = src->load_factor;
    dest->success_rate = src->success_rate;
    dest->avg_response_time_ms = src->avg_response_time_ms;
    dest->is_available = src->is_available;
    dest->weight = src->weight;

    return dest;
}

/**
 * @brief 创建轮询调度策略
 * @param config 配置信息
 * @param data 输出参数，返回策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_create(const sched_config_t* config, void** data) {
    if (!config || !data) {
        return ROUND_ROBIN_ERROR_INVALID_PARAM;
    }

    *data = NULL;

    round_robin_data_t* rrd = (round_robin_data_t*)malloc(sizeof(round_robin_data_t));
    if (!rrd) {
        return ROUND_ROBIN_ERROR_MEMORY;
    }
    memset(rrd, 0, sizeof(round_robin_data_t));

    rrd->max_agents = config->max_agents > 0 ? config->max_agents : 100;
    rrd->agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * rrd->max_agents);
    if (!rrd->agents) {
        free(rrd);
        return ROUND_ROBIN_ERROR_MEMORY;
    }
    memset(rrd->agents, 0, sizeof(agent_info_t*) * rrd->max_agents);

    rrd->agent_count = 0;
    rrd->current_index = 0;

    *data = rrd;
    return ROUND_ROBIN_SUCCESS;
}

/**
 * @brief 销毁轮询调度策略
 * @param data 策略数据
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_destroy(void* data) {
    if (!data) {
        return ROUND_ROBIN_SUCCESS;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;
    
    if (rrd->agents) {
        for (size_t i = 0; i < rrd->agent_count; i++) {
            free_agent_info(rrd->agents[i]);
            rrd->agents[i] = NULL;
        }
        free(rrd->agents);
        rrd->agents = NULL;
    }

    free(rrd);
    return ROUND_ROBIN_SUCCESS;
}

/**
 * @brief 注册 Agent
 * @param data 策略数据
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_register_agent(void* data, const agent_info_t* agent_info) {
    if (!data || !agent_info) {
        return ROUND_ROBIN_ERROR_INVALID_PARAM;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;

    if (!agent_info->agent_id) {
        return ROUND_ROBIN_ERROR_INVALID_PARAM;
    }

    if (rrd->agent_count >= rrd->max_agents) {
        return ROUND_ROBIN_ERROR_MAX_AGENTS;
    }

    for (size_t i = 0; i < rrd->agent_count; i++) {
        if (rrd->agents[i] && rrd->agents[i]->agent_id &&
            strcmp(rrd->agents[i]->agent_id, agent_info->agent_id) == 0) {
            if (rrd->agents[i]->agent_name) {
                free(rrd->agents[i]->agent_name);
                rrd->agents[i]->agent_name = NULL;
            }
            if (agent_info->agent_name) {
                rrd->agents[i]->agent_name = safe_strdup(agent_info->agent_name);
                if (!rrd->agents[i]->agent_name) {
                    return ROUND_ROBIN_ERROR_MEMORY;
                }
            }
            rrd->agents[i]->load_factor = agent_info->load_factor;
            rrd->agents[i]->success_rate = agent_info->success_rate;
            rrd->agents[i]->avg_response_time_ms = agent_info->avg_response_time_ms;
            rrd->agents[i]->is_available = agent_info->is_available;
            rrd->agents[i]->weight = agent_info->weight;
            return ROUND_ROBIN_SUCCESS;
        }
    }

    agent_info_t* new_agent = clone_agent_info(agent_info);
    if (!new_agent) {
        return ROUND_ROBIN_ERROR_MEMORY;
    }

    rrd->agents[rrd->agent_count++] = new_agent;
    return ROUND_ROBIN_SUCCESS;
}

/**
 * @brief 注销 Agent
 * @param data 策略数据
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_unregister_agent(void* data, const char* agent_id) {
    if (!data || !agent_id) {
        return ROUND_ROBIN_ERROR_INVALID_PARAM;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;

    for (size_t i = 0; i < rrd->agent_count; i++) {
        if (rrd->agents[i] && rrd->agents[i]->agent_id &&
            strcmp(rrd->agents[i]->agent_id, agent_id) == 0) {
            free_agent_info(rrd->agents[i]);

            for (size_t j = i; j < rrd->agent_count - 1; j++) {
                rrd->agents[j] = rrd->agents[j + 1];
            }
            rrd->agents[rrd->agent_count - 1] = NULL;
            rrd->agent_count--;

            if (rrd->current_index >= rrd->agent_count && rrd->agent_count > 0) {
                rrd->current_index = 0;
            }

            return ROUND_ROBIN_SUCCESS;
        }
    }

    return ROUND_ROBIN_ERROR_AGENT_NOT_FOUND;
}

/**
 * @brief 更新 Agent 状态
 * @param data 策略数据
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_update_agent_status(void* data, const agent_info_t* agent_info) {
    return round_robin_register_agent(data, agent_info);
}

/**
 * @brief 执行轮询调度
 * @param data 策略数据
 * @param task_info 任务信息
 * @param result 输出参数，返回调度结果
 * @return 0 表示成功，非 0 表示错误码
 */
static int round_robin_schedule(void* data, const task_info_t* task_info, sched_result_t** result) {
    if (!data || !task_info || !result) {
        return ROUND_ROBIN_ERROR_INVALID_PARAM;
    }

    *result = NULL;

    round_robin_data_t* rrd = (round_robin_data_t*)data;

    if (rrd->agent_count == 0) {
        return ROUND_ROBIN_ERROR_NO_AGENT;
    }

    size_t __attribute__((unused)) start_index = rrd->current_index;
    size_t attempts = 0;

    while (attempts < rrd->agent_count) {
        agent_info_t* agent = rrd->agents[rrd->current_index];

        if (agent && agent->is_available && agent->load_factor < 0.9) {
            sched_result_t* res = (sched_result_t*)malloc(sizeof(sched_result_t));
            if (!res) {
                return ROUND_ROBIN_ERROR_MEMORY;
            }
            memset(res, 0, sizeof(sched_result_t));

            if (agent->agent_id) {
                res->selected_agent_id = safe_strdup(agent->agent_id);
                if (!res->selected_agent_id) {
                    free(res);
                    return ROUND_ROBIN_ERROR_MEMORY;
                }
            } else {
                res->selected_agent_id = NULL;
            }

            res->confidence = 0.7f;
            res->estimated_time_ms = agent->avg_response_time_ms;

            rrd->current_index = (rrd->current_index + 1) % rrd->agent_count;

            *result = res;
            return ROUND_ROBIN_SUCCESS;
        }

        rrd->current_index = (rrd->current_index + 1) % rrd->agent_count;
        attempts++;
    }

    return ROUND_ROBIN_ERROR_NO_AVAILABLE_AGENT;
}

/**
 * @brief 获取轮询调度策略名称
 * @return 策略名称
 */
static const char* round_robin_get_name(void) {
    return "round_robin";
}

/**
 * @brief 获取可用 Agent 数量
 * @param data 策略数据
 * @return 可用 Agent 数量
 */
static size_t round_robin_get_available_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;
    size_t count = 0;

    for (size_t i = 0; i < rrd->agent_count; i++) {
        if (rrd->agents[i] && rrd->agents[i]->is_available) {
            count++;
        }
    }

    return count;
}

/**
 * @brief 获取总 Agent 数量
 * @param data 策略数据
 * @return 总 Agent 数量
 */
static size_t round_robin_get_total_agent_count(void* data) {
    if (!data) {
        return 0;
    }

    round_robin_data_t* rrd = (round_robin_data_t*)data;
    return rrd->agent_count;
}

/**
 * @brief 轮询调度策略接口
 */
static const strategy_interface_t round_robin_strategy = {
    .create = round_robin_create,
    .destroy = round_robin_destroy,
    .register_agent = round_robin_register_agent,
    .unregister_agent = round_robin_unregister_agent,
    .update_agent_status = round_robin_update_agent_status,
    .schedule = round_robin_schedule,
    .get_name = round_robin_get_name,
    .get_available_agent_count = round_robin_get_available_agent_count,
    .get_total_agent_count = round_robin_get_total_agent_count
};

/**
 * @brief 获取轮询调度策略接口
 * @return 轮询调度策略接口
 */
const strategy_interface_t* get_round_robin_strategy(void) {
    return &round_robin_strategy;
}
