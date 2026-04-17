/**
 * @file installer.c
 * @brief 安装管理模块
 * @details 基于 market_service 公共 API 实现安装和卸载功能
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "market_service.h"

static int create_directory(const char* path) {
    #ifdef _WIN32
    return mkdir(path);
    #else
    return mkdir(path, 0755);
    #endif
}

static bool directory_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int installer_install_agent(market_service_t* service, const install_request_t* request, install_result_t** result) {
    if (!service || !request || !result) {
        return -1;
    }
    return market_service_install_agent(service, request, result);
}

int installer_install_skill(market_service_t* service, const install_request_t* request, install_result_t** result) {
    if (!service || !request || !result) {
        return -1;
    }
    return market_service_install_skill(service, request, result);
}

int installer_uninstall_agent(market_service_t* service, const char* agent_id) {
    if (!service || !agent_id) {
        return -1;
    }
    return market_service_uninstall_agent(service, agent_id);
}

int installer_uninstall_skill(market_service_t* service, const char* skill_id) {
    if (!service || !skill_id) {
        return -1;
    }
    return market_service_uninstall_skill(service, skill_id);
}
