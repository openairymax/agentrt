/**
 * @file skill_registry.c
 * @brief Skill 注册管理模块
 * @details 基于 market_service 公共 API 实现 Skill 的注册、查询和管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "market_service.h"
#include "daemon_errors.h"

int skill_registry_register(market_service_t* service, const skill_info_t* skill_info) {
    if (!service || !skill_info) {
        return -1;
    }
    return market_service_register_skill(service, skill_info);
}

skill_info_t* skill_registry_find(market_service_t* service, const char* skill_id) {
    if (!service || !skill_id) {
        return NULL;
    }

    search_params_t params = {0};
    params.query = (char*)skill_id;
    params.limit = 1;

    skill_info_t** results = NULL;
    size_t count = 0;
    int ret = market_service_search_skills(service, &params, &results, &count);
    if (ret != 0 || count == 0 || !results) {
        return NULL;
    }

    skill_info_t* found = results[0];
    free(results);
    return found;
}

int skill_registry_remove(market_service_t* service, const char* skill_id) {
    if (!service || !skill_id) {
        return -1;
    }
    return market_service_uninstall_skill(service, skill_id);
}

int skill_registry_list(market_service_t* service, skill_info_t*** skills, size_t* count) {
    if (!service || !skills || !count) {
        return -1;
    }
    return market_service_get_installed_skills(service, skills, count);
}
