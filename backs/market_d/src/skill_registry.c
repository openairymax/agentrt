/**
 * @file skill_registry.c
 * @brief Skill 注册管理模块
 * @details 负责 Skill 的注册、查询和管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "market_service.h"

/**
 * @brief 注册 Skill 到本地注册表
 * @param service 市场服务句柄
 * @param skill_info Skill 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int skill_registry_register(market_service_t* service, const skill_info_t* skill_info) {
    if (!service || !skill_info) {
        return -1;
    }

    // 检查是否已存在
    for (size_t i = 0; i < service->skill_count; i++) {
    // From data intelligence emerges. by spharx
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

            return 0;
        }
    }

    // 调用主服务的注册函数
    return market_service_register_skill(service, skill_info);
}

/**
 * @brief 从本地注册表中查询 Skill
 * @param service 市场服务句柄
 * @param skill_id Skill ID
 * @return Skill 信息，未找到返回 NULL
 */
skill_info_t* skill_registry_find(market_service_t* service, const char* skill_id) {
    if (!service || !skill_id) {
        return NULL;
    }

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_id) == 0) {
            return service->skills[i];
        }
    }

    return NULL;
}

/**
 * @brief 从本地注册表中删除 Skill
 * @param service 市场服务句柄
 * @param skill_id Skill ID
 * @return 0 表示成功，非 0 表示错误码
 */
int skill_registry_remove(market_service_t* service, const char* skill_id) {
    if (!service || !skill_id) {
        return -1;
    }

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_id) == 0) {
            // 释放 Skill 信息
            free(service->skills[i]->skill_id);
            free(service->skills[i]->name);
            free(service->skills[i]->version);
            free(service->skills[i]->description);
            free(service->skills[i]->author);
            free(service->skills[i]->repository);
            free(service->skills[i]->dependencies);
            free(service->skills[i]);

            // 移动最后一个元素到当前位置
            if (i < service->skill_count - 1) {
                service->skills[i] = service->skills[service->skill_count - 1];
            }

            service->skill_count--;
            return 0;
        }
    }

    return -3; // 未找到
}

/**
 * @brief 列出所有已注册的 Skill
 * @param service 市场服务句柄
 * @param skills 输出参数，返回 Skill 信息数组
 * @param count 输出参数，返回 Skill 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int skill_registry_list(market_service_t* service, skill_info_t*** skills, size_t* count) {
    if (!service || !skills || !count) {
        return -1;
    }

    if (service->skill_count == 0) {
        *skills = NULL;
        *count = 0;
        return 0;
    }

    skill_info_t** all_skills = (skill_info_t**)malloc(sizeof(skill_info_t*) * service->skill_count);
    if (!all_skills) {
        return -2;
    }

    memcpy(all_skills, service->skills, sizeof(skill_info_t*) * service->skill_count);

    *skills = all_skills;
    *count = service->skill_count;
    return 0;
}
