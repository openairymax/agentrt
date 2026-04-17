/**
 * @file publisher.c
 * @brief 发布管理模块
 * @details 基于 market_service 公共 API 实现发布和更新功能
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "market_service.h"

int publisher_publish_agent(market_service_t* service, const agent_info_t* agent_info) {
    if (!service || !agent_info) {
        return -1;
    }
    return market_service_register_agent(service, agent_info);
}

int publisher_publish_skill(market_service_t* service, const skill_info_t* skill_info) {
    if (!service || !skill_info) {
        return -1;
    }
    return market_service_register_skill(service, skill_info);
}

int publisher_check_agent_update(market_service_t* service, const char* agent_id, bool* has_update, char** latest_version) {
    if (!service || !agent_id || !has_update || !latest_version) {
        return -1;
    }
    return market_service_check_update(service, agent_id, has_update, latest_version);
}

int publisher_check_skill_update(market_service_t* service, const char* skill_id, bool* has_update, char** latest_version) {
    if (!service || !skill_id || !has_update || !latest_version) {
        return -1;
    }
    return market_service_check_update(service, skill_id, has_update, latest_version);
}

int publisher_sync_to_registry(market_service_t* service) {
    if (!service) {
        return -1;
    }
    return market_service_sync_registry(service);
}
