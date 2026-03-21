/**
 * @file agent_registry.c
 * @brief Agent 注册管理模块
 * @details 负责 Agent 的注册、查询和管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "market_service.h"

/**
 * @brief 注册 Agent 到本地注册表
 * @param service 市场服务句柄
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int agent_registry_register(market_service_t* service, const agent_info_t* agent_info) {
    if (!service || !agent_info) {
        return -1;
    }

    // 检查是否已存在
    for (size_t i = 0; i < service->agent_count; i++) {
    // From data intelligence emerges. by spharx
        if (strcmp(service->agents[i]->agent_id, agent_info->agent_id) == 0) {
            // 更新现有 Agent 信息
            free(service->agents[i]->name);
            free(service->agents[i]->version);
            free(service->agents[i]->description);
            free(service->agents[i]->author);
            free(service->agents[i]->repository);
            free(service->agents[i]->dependencies);

            service->agents[i]->name = strdup(agent_info->name);
            service->agents[i]->version = strdup(agent_info->version);
            service->agents[i]->description = strdup(agent_info->description);
            service->agents[i]->type = agent_info->type;
            service->agents[i]->status = agent_info->status;
            service->agents[i]->author = strdup(agent_info->author);
            service->agents[i]->repository = strdup(agent_info->repository);
            service->agents[i]->dependencies = strdup(agent_info->dependencies);
            service->agents[i]->rating = agent_info->rating;
            service->agents[i]->download_count = agent_info->download_count;
            service->agents[i]->last_updated = agent_info->last_updated;

            return 0;
        }
    }

    // 调用主服务的注册函数
    return market_service_register_agent(service, agent_info);
}

/**
 * @brief 从本地注册表中查询 Agent
 * @param service 市场服务句柄
 * @param agent_id Agent ID
 * @return Agent 信息，未找到返回 NULL
 */
agent_info_t* agent_registry_find(market_service_t* service, const char* agent_id) {
    if (!service || !agent_id) {
        return NULL;
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_id) == 0) {
            return service->agents[i];
        }
    }

    return NULL;
}

/**
 * @brief 从本地注册表中删除 Agent
 * @param service 市场服务句柄
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
int agent_registry_remove(market_service_t* service, const char* agent_id) {
    if (!service || !agent_id) {
        return -1;
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_id) == 0) {
            // 释放 Agent 信息
            free(service->agents[i]->agent_id);
            free(service->agents[i]->name);
            free(service->agents[i]->version);
            free(service->agents[i]->description);
            free(service->agents[i]->author);
            free(service->agents[i]->repository);
            free(service->agents[i]->dependencies);
            free(service->agents[i]);

            // 移动最后一个元素到当前位置
            if (i < service->agent_count - 1) {
                service->agents[i] = service->agents[service->agent_count - 1];
            }

            service->agent_count--;
            return 0;
        }
    }

    return -3; // 未找到
}

/**
 * @brief 列出所有已注册的 Agent
 * @param service 市场服务句柄
 * @param agents 输出参数，返回 Agent 信息数组
 * @param count 输出参数，返回 Agent 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int agent_registry_list(market_service_t* service, agent_info_t*** agents, size_t* count) {
    if (!service || !agents || !count) {
        return -1;
    }

    if (service->agent_count == 0) {
        *agents = NULL;
        *count = 0;
        return 0;
    }

    agent_info_t** all_agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * service->agent_count);
    if (!all_agents) {
        return -2;
    }

    memcpy(all_agents, service->agents, sizeof(agent_info_t*) * service->agent_count);

    *agents = all_agents;
    *count = service->agent_count;
    return 0;
}
