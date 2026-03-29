/**
 * @file installer.c
 * @brief 安装管理模块
 * @details 负责 Agent 和 Skill 的安装和卸载
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "market_service.h"

/**
 * @brief 创建目录
 * @param path 目录路径
 * @return 0 表示成功，非 0 表示错误码
 */
static int create_directory(const char* path) {
    #ifdef _WIN32
    return mkdir(path);
    #else
    return mkdir(path, 0755);
    #endif
}
// From data intelligence emerges. by spharx

/**
 * @brief 检查目录是否存在
 * @param path 目录路径
 * @return true 表示存在，false 表示不存在
 */
static bool directory_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/**
 * @brief 安装 Agent
 * @param service 市场服务句柄
 * @param request 安装请求
 * @param result 输出参数，返回安装结果
 * @return 0 表示成功，非 0 表示错误码
 */
int installer_install_agent(market_service_t* service, const install_request_t* request, install_result_t** result) {
    if (!service || !request || !result) {
        return -1;
    }

    install_result_t* new_result = (install_result_t*)malloc(sizeof(install_result_t));
    if (!new_result) {
        return -2;
    }

    // 查找 Agent
    agent_info_t* agent = NULL;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, request->id) == 0) {
            agent = service->agents[i];
            break;
        }
    }

    if (!agent) {
        new_result->success = false;
        new_result->message = strdup("Agent not found");
        new_result->installed_version = NULL;
        new_result->install_path = NULL;
        new_result->error_code = -3;
        *result = new_result;
        return 0;
    }

    // 生成安装路径
    char install_path[256];
    if (request->install_path) {
        if (strlen(request->install_path) >= sizeof(install_path)) {
            new_result->success = false;
            new_result->error_code = AGENTOS_ERR_INVALID_PARAM;
            new_result->error_message = strdup("Install path too long");
            return new_result;
        }
        snprintf(install_path, sizeof(install_path), "%s", request->install_path);
    } else {
        snprintf(install_path, sizeof(install_path), "%s/agents/%s", service->manager.storage_path, agent->agent_id);
    }

    // 创建安装目录
    if (!directory_exists(install_path)) {
        // 创建父目录
        char parent_path[256];
        snprintf(parent_path, sizeof(parent_path), "%s", install_path);
        char* last_slash = strrchr(parent_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (!directory_exists(parent_path)) {
                if (create_directory(parent_path) != 0) {
                    new_result->success = false;
                    new_result->message = strdup("Failed to create parent directory");
                    new_result->installed_version = NULL;
                    new_result->install_path = NULL;
                    new_result->error_code = -4;
                    *result = new_result;
                    return 0;
                }
            }
        }

        // 创建安装目录
        if (create_directory(install_path) != 0) {
            new_result->success = false;
            new_result->message = strdup("Failed to create install directory");
            new_result->installed_version = NULL;
            new_result->install_path = NULL;
            new_result->error_code = -5;
            *result = new_result;
            return 0;
        }
    }

    // 模拟安装过程
    printf("安装 Agent: %s (版本: %s) 到 %s\n", agent->name, request->version ? request->version : agent->version, install_path);

    // 创建版本文件
    char version_file[256];
    snprintf(version_file, sizeof(version_file), "%s/version.txt", install_path);
    FILE* fp = fopen(version_file, "w");
    if (fp) {
        fprintf(fp, "%s\n", request->version ? request->version : agent->version);
        fclose(fp);
    }

    // 更新 Agent 状态
    agent->status = AGENT_STATUS_AVAILABLE;
    agent->download_count++;

    new_result->success = true;
    new_result->message = strdup("Agent installed successfully");
    new_result->installed_version = strdup(request->version ? request->version : agent->version);
    new_result->install_path = strdup(install_path);
    new_result->error_code = 0;

    *result = new_result;
    return 0;
}

/**
 * @brief 安装 Skill
 * @param service 市场服务句柄
 * @param request 安装请求
 * @param result 输出参数，返回安装结果
 * @return 0 表示成功，非 0 表示错误码
 */
int installer_install_skill(market_service_t* service, const install_request_t* request, install_result_t** result) {
    if (!service || !request || !result) {
        return -1;
    }

    install_result_t* new_result = (install_result_t*)malloc(sizeof(install_result_t));
    if (!new_result) {
        return -2;
    }

    // 查找 Skill
    skill_info_t* skill = NULL;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, request->id) == 0) {
            skill = service->skills[i];
            break;
        }
    }

    if (!skill) {
        new_result->success = false;
        new_result->message = strdup("Skill not found");
        new_result->installed_version = NULL;
        new_result->install_path = NULL;
        new_result->error_code = -3;
        *result = new_result;
        return 0;
    }

    // 生成安装路径
    char install_path[256];
    if (request->install_path) {
        size_t path_len = strlen(request->install_path);
        if (path_len >= sizeof(install_path)) {
            free(new_result);
            return AGENTOS_ERR_INVALID_PARAM;
        }
        memcpy(install_path, request->install_path, path_len + 1);
    } else {
        snprintf(install_path, sizeof(install_path), "%s/skills/%s", service->manager.storage_path, skill->skill_id);
    }

    // 创建安装目录
    if (!directory_exists(install_path)) {
        // 创建父目录
        char parent_path[256];
        size_t install_len = strlen(install_path);
        if (install_len < sizeof(parent_path)) {
            memcpy(parent_path, install_path, install_len + 1);
        } else {
            new_result->success = false;
            new_result->message = strdup("Install path too long");
            new_result->installed_version = NULL;
            new_result->install_path = NULL;
            new_result->error_code = -4;
            *result = new_result;
            return 0;
        }
        char* last_slash = strrchr(parent_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (!directory_exists(parent_path)) {
                if (create_directory(parent_path) != 0) {
                    new_result->success = false;
                    new_result->message = strdup("Failed to create parent directory");
                    new_result->installed_version = NULL;
                    new_result->install_path = NULL;
                    new_result->error_code = -4;
                    *result = new_result;
                    return 0;
                }
            }
        }

        // 创建安装目录
        if (create_directory(install_path) != 0) {
            new_result->success = false;
            new_result->message = strdup("Failed to create install directory");
            new_result->installed_version = NULL;
            new_result->install_path = NULL;
            new_result->error_code = -5;
            *result = new_result;
            return 0;
        }
    }

    // 模拟安装过程
    printf("安装 Skill: %s (版本: %s) 到 %s\n", skill->name, request->version ? request->version : skill->version, install_path);

    // 创建版本文件
    char version_file[256];
    snprintf(version_file, sizeof(version_file), "%s/version.txt", install_path);
    FILE* fp = fopen(version_file, "w");
    if (fp) {
        fprintf(fp, "%s\n", request->version ? request->version : skill->version);
        fclose(fp);
    }

    // 更新 Skill 下载次数
    skill->download_count++;

    new_result->success = true;
    new_result->message = strdup("Skill installed successfully");
    new_result->installed_version = strdup(request->version ? request->version : skill->version);
    new_result->install_path = strdup(install_path);
    new_result->error_code = 0;

    *result = new_result;
    return 0;
}

/**
 * @brief 卸载 Agent
 * @param service 市场服务句柄
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
int installer_uninstall_agent(market_service_t* service, const char* agent_id) {
    if (!service || !agent_id) {
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

    // 生成安装路径
    char install_path[256];
    snprintf(install_path, sizeof(install_path), "%s/agents/%s", service->manager.storage_path, agent_id);

    // 模拟卸载过程
    printf("卸载 Agent: %s 从 %s\n", agent->name, install_path);

    // 更新 Agent 状态
    agent->status = AGENT_STATUS_DISABLED;

    return 0;
}

/**
 * @brief 卸载 Skill
 * @param service 市场服务句柄
 * @param skill_id Skill ID
 * @return 0 表示成功，非 0 表示错误码
 */
int installer_uninstall_skill(market_service_t* service, const char* skill_id) {
    if (!service || !skill_id) {
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

    // 生成安装路径
    char install_path[256];
    snprintf(install_path, sizeof(install_path), "%s/skills/%s", service->manager.storage_path, skill_id);

    // 模拟卸载过程
    printf("卸载 Skill: %s 从 %s\n", skill->name, install_path);

    return 0;
}
