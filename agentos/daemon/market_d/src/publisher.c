/**
 * @file publisher.c
 * @brief 发布管理模块
 * @details 负责 Agent 和 Skill 的发布和更新
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "market_service.h"

/**
 * @brief 发布 Agent 到市场
 * @param service 市场服务句柄
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int publisher_publish_agent(market_service_t* service, const agent_info_t* agent_info) {
    if (!service || !agent_info) {
        return -1;
    }

    // 检查是否已存在
    for (size_t i = 0; i < service->agent_count; i++) {
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

            printf("更新 Agent: %s 版本: %s\n", agent_info->name, agent_info->version);
            return 0;
        }
    }

    // 注册新 Agent
    int ret = market_service_register_agent(service, agent_info);
    if (ret == 0) {
        printf("发布新 Agent: %s 版本: %s\n", agent_info->name, agent_info->version);
    }

    return ret;
}

/**
 * @brief 发布 Skill 到市场
 * @param service 市场服务句柄
 * @param skill_info Skill 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int publisher_publish_skill(market_service_t* service, const skill_info_t* skill_info) {
    if (!service || !skill_info) {
        return -1;
    }

    // 检查是否已存在
    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_info->skill_id) == 0) {
            // 更新现有 Skill 信息
            free(service->skills[i]->name);
            free(service->skills[i]->version);
            free(service->skills[i]->description);
            free(service->skills[i]->author);
            free(service->skills[i]->repository);
            free(service->skills[i]->dependencies);

            service->skills[i]->name = strdup(skill_info->name);
            service->skills[i]->version = strdup(skill_info->version);
            service->skills[i]->description = strdup(skill_info->description);
            service->skills[i]->type = skill_info->type;
            service->skills[i]->author = strdup(skill_info->author);
            service->skills[i]->repository = strdup(skill_info->repository);
            service->skills[i]->dependencies = strdup(skill_info->dependencies);
            service->skills[i]->rating = skill_info->rating;
            service->skills[i]->download_count = skill_info->download_count;
            service->skills[i]->last_updated = skill_info->last_updated;

            printf("更新 Skill: %s 版本: %s\n", skill_info->name, skill_info->version);
            return 0;
        }
    }

    // 注册新 Skill
    int ret = market_service_register_skill(service, skill_info);
    if (ret == 0) {
        printf("发布新 Skill: %s 版本: %s\n", skill_info->name, skill_info->version);
    }

    return ret;
}

/**
 * @brief 检查 Agent 是否有更新
 * @param service 市场服务句柄
 * @param agent_id Agent ID
 * @param has_update 输出参数，返回是否有更新
 * @param latest_version 输出参数，返回最新版本
 * @return 0 表示成功，非 0 表示错误码
 */
int publisher_check_agent_update(market_service_t* service, const char* agent_id, bool* has_update, char** latest_version) {
    if (!service || !agent_id || !has_update || !latest_version) {
        return -1;
    }

    // 查找 Agent
    agent_info_t* agent = NULL;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_id) == 0) {
            agent = service->agents[i];
            break;
        }
    }

    if (!agent) {
        return -3; // 未找到
    }

    // 模拟检查更新
    *has_update = false;
    *latest_version = strdup(agent->version);

    return 0;
}

/**
 * @brief 检查 Skill 是否有更新
 * @param service 市场服务句柄
 * @param skill_id Skill ID
 * @param has_update 输出参数，返回是否有更新
 * @param latest_version 输出参数，返回最新版本
 * @return 0 表示成功，非 0 表示错误码
 */
int publisher_check_skill_update(market_service_t* service, const char* skill_id, bool* has_update, char** latest_version) {
    if (!service || !skill_id || !has_update || !latest_version) {
        return -1;
    }

    // 查找 Skill
    skill_info_t* skill = NULL;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_id) == 0) {
            skill = service->skills[i];
            break;
        }
    }

    if (!skill) {
        return -3; // 未找到
    }

    // 模拟检查更新
    *has_update = false;
    *latest_version = strdup(skill->version);

    return 0;
}

/**
 * @brief 同步市场数据到远程注册中心
 * @param service 市场服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int publisher_sync_to_registry(market_service_t* service) {
    if (!service) {
        return -1;
    }

    if (!service->manager.enable_remote_registry) {
        return 0;
    }

    // 模拟同步过程
    printf("同步市场数据到远程注册中心: %s\n", service->manager.registry_url);
    printf("同步 %zu 个 Agent 和 %zu 个 Skill\n", service->agent_count, service->skill_count);

    return 0;
}
