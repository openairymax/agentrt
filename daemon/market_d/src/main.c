/**
 * @file main.c
 * @brief 市场服务主实现
 * @details 实现市场服务的核心功能，包括 Agent 和 Skill 的注册、发现、安装和管理
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "market_service.h"

/**
 * @brief 市场服务内部结构
 */
struct market_service {
    market_config_t manager;           /**< 配置信息 */
    agent_info_t** agents;            /**< Agent 列表 */
    size_t agent_count;               /**< Agent 数量 */
    size_t agent_capacity;            /**< Agent 容量 */
    skill_info_t** skills;            /**< Skill 列表 */
    size_t skill_count;               /**< Skill 数量 */
    size_t skill_capacity;            /**< Skill 容量 */
    bool is_running;                  /**< 服务是否运行 */
};

/**
 * @brief 创建市场服务
 * @param manager 配置信息
 * @param service 输出参数，返回创建的服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_create(const market_config_t* manager, market_service_t** service) {
    if (!manager || !service) {
        return -1;
    }

    market_service_t* new_service = (market_service_t*)malloc(sizeof(market_service_t));
    if (!new_service) {
        return -2;
    }

    // 复制配置
    new_service->manager = *manager;
    new_service->agents = NULL;
    new_service->agent_count = 0;
    new_service->agent_capacity = 1024;
    new_service->skills = NULL;
    new_service->skill_count = 0;
    new_service->skill_capacity = 1024;
    new_service->is_running = true;

    // 分配内存
    new_service->agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * new_service->agent_capacity);
    if (!new_service->agents) {
        free(new_service);
        return -2;
    }

    new_service->skills = (skill_info_t**)malloc(sizeof(skill_info_t*) * new_service->skill_capacity);
    if (!new_service->skills) {
        free(new_service->agents);
        free(new_service);
        return -2;
    }

    *service = new_service;
    return 0;
}

/**
 * @brief 销毁市场服务
 * @param service 服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_destroy(market_service_t* service) {
    if (!service) {
        return -1;
    }

    // 释放 Agent 数据
    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]) {
            free(service->agents[i]->agent_id);
            free(service->agents[i]->name);
            free(service->agents[i]->version);
            free(service->agents[i]->description);
            free(service->agents[i]->author);
            free(service->agents[i]->repository);
            free(service->agents[i]->dependencies);
            free(service->agents[i]);
        }
    }
    free(service->agents);

    // 释放 Skill 数据
    for (size_t i = 0; i < service->skill_count; i++) {
        if (service->skills[i]) {
            free(service->skills[i]->skill_id);
            free(service->skills[i]->name);
            free(service->skills[i]->version);
            free(service->skills[i]->description);
            free(service->skills[i]->author);
            free(service->skills[i]->repository);
            free(service->skills[i]->dependencies);
            free(service->skills[i]);
        }
    }
    free(service->skills);

    free(service);
    return 0;
}

/**
 * @brief 注册 Agent
 * @param service 服务句柄
 * @param agent_info Agent 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_register_agent(market_service_t* service, const agent_info_t* agent_info) {
    if (!service || !agent_info) {
        return -1; // 无效参数
    }

    // 验证 Agent 信息
    if (!agent_info->agent_id || strlen(agent_info->agent_id) == 0) {
        return -3; // 无效的 Agent ID
    }

    if (!agent_info->name || strlen(agent_info->name) == 0) {
        return -4; // 无效的 Agent 名称
    }

    if (!agent_info->version || strlen(agent_info->version) == 0) {
        return -5; // 无效的版本号
    }

    if (agent_info->type < 0 || agent_info->type >= AGENT_TYPE_COUNT) {
        return -6; // 无效的 Agent 类型
    }

    if (agent_info->status < 0 || agent_info->status >= AGENT_STATUS_COUNT) {
        return -7; // 无效的 Agent 状态
    }

    // 检查容量
    if (service->agent_count >= service->agent_capacity) {
        size_t new_capacity = service->agent_capacity * 2;
        agent_info_t** new_agents = (agent_info_t**)realloc(service->agents, sizeof(agent_info_t*) * new_capacity);
        if (!new_agents) {
            return -2; // 内存分配失败
        }
        service->agents = new_agents;
        service->agent_capacity = new_capacity;
    }

    // 复制 Agent 信息
    agent_info_t* new_agent = (agent_info_t*)malloc(sizeof(agent_info_t));
    if (!new_agent) {
        return -2; // 内存分配失败
    }

    new_agent->agent_id = strdup(agent_info->agent_id);
    if (!new_agent->agent_id) {
        free(new_agent);
        return -2; // 内存分配失败
    }

    new_agent->name = strdup(agent_info->name);
    if (!new_agent->name) {
        free(new_agent->agent_id);
        free(new_agent);
        return -2; // 内存分配失败
    }

    new_agent->version = strdup(agent_info->version);
    if (!new_agent->version) {
        free(new_agent->agent_id);
        free(new_agent->name);
        free(new_agent);
        return -2; // 内存分配失败
    }

    new_agent->description = agent_info->description ? strdup(agent_info->description) : NULL;
    new_agent->type = agent_info->type;
    new_agent->status = agent_info->status;
    new_agent->author = agent_info->author ? strdup(agent_info->author) : NULL;
    new_agent->repository = agent_info->repository ? strdup(agent_info->repository) : NULL;
    new_agent->dependencies = agent_info->dependencies ? strdup(agent_info->dependencies) : NULL;
    new_agent->rating = agent_info->rating;
    new_agent->download_count = agent_info->download_count;
    new_agent->last_updated = agent_info->last_updated;

    service->agents[service->agent_count++] = new_agent;
    return 0;
}

/**
 * @brief 注册 Skill
 * @param service 服务句柄
 * @param skill_info Skill 信息
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_register_skill(market_service_t* service, const skill_info_t* skill_info) {
    if (!service || !skill_info) {
        return -1;
    }

    // 检查容量
    if (service->skill_count >= service->skill_capacity) {
        size_t new_capacity = service->skill_capacity * 2;
        skill_info_t** new_skills = (skill_info_t**)realloc(service->skills, sizeof(skill_info_t*) * new_capacity);
        if (!new_skills) {
            return -2;
        }
        service->skills = new_skills;
        service->skill_capacity = new_capacity;
    }

    // 复制 Skill 信息
    skill_info_t* new_skill = (skill_info_t*)malloc(sizeof(skill_info_t));
    if (!new_skill) {
        return -2;
    }

    new_skill->skill_id = strdup(skill_info->skill_id);
    new_skill->name = strdup(skill_info->name);
    new_skill->version = strdup(skill_info->version);
    new_skill->description = strdup(skill_info->description);
    new_skill->type = skill_info->type;
    new_skill->author = strdup(skill_info->author);
    new_skill->repository = strdup(skill_info->repository);
    new_skill->dependencies = strdup(skill_info->dependencies);
    new_skill->rating = skill_info->rating;
    new_skill->download_count = skill_info->download_count;
    new_skill->last_updated = skill_info->last_updated;

    service->skills[service->skill_count++] = new_skill;
    return 0;
}

/**
 * @brief 搜索 Agent
 * @param service 服务句柄
 * @param params 搜索参数
 * @param agents 输出参数，返回 Agent 信息数组
 * @param count 输出参数，返回 Agent 数量
 * @return 0 表示成功，非 0 表示错误码
 */
/**
 * @brief 检查 Agent 是否匹配搜索条件
 * @param agent Agent 信息
 * @param params 搜索参数
 * @return true 表示匹配，false 表示不匹配
 */
static bool is_agent_matched(const agent_info_t* agent, const search_params_t* params) {
    if (!agent) {
        return false;
    }

    // 检查是否仅显示已安装的
    if (params->only_installed && agent->status != AGENT_STATUS_AVAILABLE) {
        return false;
    }

    // 检查 Agent 类型
    if (params->agent_type != AGENT_TYPE_COUNT && agent->type != params->agent_type) {
        return false;
    }

    // 检查搜索关键词
    if (params->query && strstr(agent->name, params->query) == NULL && strstr(agent->description, params->query) == NULL) {
        return false;
    }

    return true;
}

/**
 * @brief 统计匹配的 Agent 数量
 * @param service 服务句柄
 * @param params 搜索参数
 * @return 匹配的 Agent 数量
 */
static size_t count_matching_agents(market_service_t* service, const search_params_t* params) {
    size_t matched_count = 0;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (is_agent_matched(service->agents[i], params)) {
            matched_count++;
        }
    }
    return matched_count;
}

/**
 * @brief 收集匹配的 Agent
 * @param service 服务句柄
 * @param params 搜索参数
 * @param matched_count 匹配的 Agent 数量
 * @return 分配的 Agent 数组，调用者负责释放内存
 */
static agent_info_t** collect_matching_agents(market_service_t* service, const search_params_t* params, size_t matched_count) {
    if (matched_count == 0) {
        return NULL;
    }

    agent_info_t** matched_agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * matched_count);
    if (!matched_agents) {
        return NULL;
    }

    size_t index = 0;
    for (size_t i = 0; i < service->agent_count && index < matched_count; i++) {
        agent_info_t* agent = service->agents[i];
        if (is_agent_matched(agent, params)) {
            matched_agents[index++] = agent;
        }
    }

    return matched_agents;
}

/**
 * @brief 比较函数：按评分降序排序
 * @param a 第一个 Agent
 * @param b 第二个 Agent
 * @return 比较结果
 */
static int compare_by_rating(const void* a, const void* b) {
    const agent_info_t* agent_a = *(const agent_info_t**)a;
    const agent_info_t* agent_b = *(const agent_info_t**)b;
    if (agent_b->rating > agent_a->rating) return 1;
    if (agent_b->rating < agent_a->rating) return -1;
    return 0;
}

/**
 * @brief 比较函数：按下载量降序排序
 * @param a 第一个 Agent
 * @param b 第二个 Agent
 * @return 比较结果
 */
static int compare_by_download(const void* a, const void* b) {
    const agent_info_t* agent_a = *(const agent_info_t**)a;
    const agent_info_t* agent_b = *(const agent_info_t**)b;
    if (agent_b->download_count > agent_a->download_count) return 1;
    if (agent_b->download_count < agent_a->download_count) return -1;
    return 0;
}

/**
 * @brief 对 Agent 数组进行排序
 * @param agents Agent 数组
 * @param count Agent 数量
 * @param params 搜索参数
 */
static void sort_agents(agent_info_t** agents, size_t count, const search_params_t* params) {
    if (!agents || count == 0) {
        return;
    }

    if (params->sort_by_rating) {
        qsort(agents, count, sizeof(agent_info_t*), compare_by_rating);
    } else if (params->sort_by_download) {
        qsort(agents, count, sizeof(agent_info_t*), compare_by_download);
    }
}

/**
 * @brief 应用分页限制
 * @param matched_agents 匹配的 Agent 数组
 * @param matched_count 匹配的 Agent 数量
 * @param params 搜索参数
 * @param result_agents 输出参数，返回分页后的 Agent 数组
 * @param result_count 输出参数，返回分页后的 Agent 数量
 * @return 0 表示成功，-2 表示内存分配失败
 */
static int apply_pagination(agent_info_t** matched_agents, size_t matched_count,
                           const search_params_t* params,
                           agent_info_t*** result_agents, size_t* result_count) {
    size_t start = params->offset;
    size_t end = start + params->limit;

    if (start >= matched_count) {
        *result_agents = NULL;
        *result_count = 0;
        free(matched_agents);
        return 0;
    }

    if (end > matched_count) {
        end = matched_count;
    }

    size_t count = end - start;
    agent_info_t** agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * count);
    if (!agents) {
        free(matched_agents);
        return -2;
    }

    memcpy(agents, matched_agents + start, sizeof(agent_info_t*) * count);
    free(matched_agents);

    *result_agents = agents;
    *result_count = count;
    return 0;
}

/**
 * @brief 搜索 Agent
 * @param service 服务句柄
 * @param params 搜索参数
 * @param agents 输出参数，返回 Agent 信息数组
 * @param count 输出参数，返回 Agent 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_search_agents(market_service_t* service, const search_params_t* params, agent_info_t*** agents, size_t* count) {
    if (!service || !params || !agents || !count) {
        return -1;
    }

    // 统计匹配的 Agent 数量
    size_t matched_count = count_matching_agents(service, params);
    if (matched_count == 0) {
        *agents = NULL;
        *count = 0;
        return 0;
    }

    // 收集匹配的 Agent
    agent_info_t** matched_agents = collect_matching_agents(service, params, matched_count);
    if (!matched_agents) {
        return -2;
    }

    // 排序
    sort_agents(matched_agents, matched_count, params);

    // 应用分页
    return apply_pagination(matched_agents, matched_count, params, agents, count);
}

/**
 * @brief 搜索 Skill
 * @param service 服务句柄
 * @param params 搜索参数
 * @param skills 输出参数，返回 Skill 信息数组
 * @param count 输出参数，返回 Skill 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_search_skills(market_service_t* service, const search_params_t* params, skill_info_t*** skills, size_t* count) {
    if (!service || !params || !skills || !count) {
        return -1;
    }

    // 统计匹配的 Skill 数量
    size_t matched_count = 0;
    for (size_t i = 0; i < service->skill_count; i++) {
        skill_info_t* skill = service->skills[i];
        if (!skill) {
            continue;
        }

        // 检查 Skill 类型
        if (params->skill_type != SKILL_TYPE_COUNT && skill->type != params->skill_type) {
            continue;
        }

        // 检查搜索关键词
        if (params->query && strstr(skill->name, params->query) == NULL && strstr(skill->description, params->query) == NULL) {
            continue;
        }

        matched_count++;
    }

    if (matched_count == 0) {
        *skills = NULL;
        *count = 0;
        return 0;
    }

    // 分配内存并复制匹配的 Skill
    skill_info_t** matched_skills = (skill_info_t**)malloc(sizeof(skill_info_t*) * matched_count);
    if (!matched_skills) {
        return -2;
    }

    size_t index = 0;
    for (size_t i = 0; i < service->skill_count && index < matched_count; i++) {
        skill_info_t* skill = service->skills[i];
        if (!skill) {
            continue;
        }

        // 检查 Skill 类型
        if (params->skill_type != SKILL_TYPE_COUNT && skill->type != params->skill_type) {
            continue;
        }

        // 检查搜索关键词
        if (params->query && strstr(skill->name, params->query) == NULL && strstr(skill->description, params->query) == NULL) {
            continue;
        }

        matched_skills[index++] = skill;
    }

    // 排序
    if (params->sort_by_rating) {
        // 按评分排序
        for (size_t i = 0; i < matched_count - 1; i++) {
            for (size_t j = i + 1; j < matched_count; j++) {
                if (matched_skills[i]->rating < matched_skills[j]->rating) {
                    skill_info_t* temp = matched_skills[i];
                    matched_skills[i] = matched_skills[j];
                    matched_skills[j] = temp;
                }
            }
        }
    } else if (params->sort_by_download) {
        // 按下载量排序
        for (size_t i = 0; i < matched_count - 1; i++) {
            for (size_t j = i + 1; j < matched_count; j++) {
                if (matched_skills[i]->download_count < matched_skills[j]->download_count) {
                    skill_info_t* temp = matched_skills[i];
                    matched_skills[i] = matched_skills[j];
                    matched_skills[j] = temp;
                }
            }
        }
    }

    // 应用限制和偏移
    size_t start = params->offset;
    size_t end = start + params->limit;
    if (start >= matched_count) {
        *skills = NULL;
        *count = 0;
        free(matched_skills);
        return 0;
    }

    if (end > matched_count) {
        end = matched_count;
    }

    size_t result_count = end - start;
    skill_info_t** result_skills = (skill_info_t**)malloc(sizeof(skill_info_t*) * result_count);
    if (!result_skills) {
        free(matched_skills);
        return -2;
    }

    memcpy(result_skills, matched_skills + start, sizeof(skill_info_t*) * result_count);
    free(matched_skills);

    *skills = result_skills;
    *count = result_count;
    return 0;
}

/**
 * @brief 安装 Agent
 * @param service 服务句柄
 * @param request 安装请求
 * @param result 输出参数，返回安装结果
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_install_agent(market_service_t* service, const install_request_t* request, install_result_t** result) {
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

    // 模拟安装过程
    printf("安装 Agent: %s (版本: %s)\n", agent->name, request->version ? request->version : agent->version);

    // 生成安装路径
    char install_path[256];
    if (request->install_path) {
        if (strlen(request->install_path) >= sizeof(install_path)) {
            return AGENTOS_ERR_INVALID_PARAM;
        }
        snprintf(install_path, sizeof(install_path), "%s", request->install_path);
    } else {
        snprintf(install_path, sizeof(install_path), "%s/agents/%s", service->manager.storage_path, agent->agent_id);
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
 * @param service 服务句柄
 * @param request 安装请求
 * @param result 输出参数，返回安装结果
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_install_skill(market_service_t* service, const install_request_t* request, install_result_t** result) {
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

    // 模拟安装过程
    printf("安装 Skill: %s (版本: %s)\n", skill->name, request->version ? request->version : skill->version);

    // 生成安装路径
    char install_path[256];
    if (request->install_path) {
        if (strlen(request->install_path) >= sizeof(install_path)) {
            return AGENTOS_ERR_INVALID_PARAM;
        }
        snprintf(install_path, sizeof(install_path), "%s", request->install_path);
    } else {
        snprintf(install_path, sizeof(install_path), "%s/skills/%s", service->manager.storage_path, skill->skill_id);
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
 * @param service 服务句柄
 * @param agent_id Agent ID
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_uninstall_agent(market_service_t* service, const char* agent_id) {
    if (!service || !agent_id) {
        return -1;
    }

    // 查找并卸载 Agent
    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_id) == 0) {
            // 模拟卸载过程
            printf("卸载 Agent: %s\n", service->agents[i]->name);
            
            // 更新 Agent 状态
            service->agents[i]->status = AGENT_STATUS_DISABLED;
            return 0;
        }
    }

    return -3; // Agent 不存在
}

/**
 * @brief 卸载 Skill
 * @param service 服务句柄
 * @param skill_id Skill ID
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_uninstall_skill(market_service_t* service, const char* skill_id) {
    if (!service || !skill_id) {
        return -1;
    }

    // 查找并卸载 Skill
    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_id) == 0) {
            // 模拟卸载过程
            printf("卸载 Skill: %s\n", service->skills[i]->name);
            return 0;
        }
    }

    return -3; // Skill 不存在
}

/**
 * @brief 获取已安装的 Agent 列表
 * @param service 服务句柄
 * @param agents 输出参数，返回 Agent 信息数组
 * @param count 输出参数，返回 Agent 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_get_installed_agents(market_service_t* service, agent_info_t*** agents, size_t* count) {
    if (!service || !agents || !count) {
        return -1;
    }

    // 统计已安装的 Agent 数量
    size_t installed_count = 0;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]->status == AGENT_STATUS_AVAILABLE) {
            installed_count++;
        }
    }

    if (installed_count == 0) {
        *agents = NULL;
        *count = 0;
        return 0;
    }

    // 分配内存并复制已安装的 Agent
    agent_info_t** installed_agents = (agent_info_t**)malloc(sizeof(agent_info_t*) * installed_count);
    if (!installed_agents) {
        return -2;
    }

    size_t index = 0;
    for (size_t i = 0; i < service->agent_count && index < installed_count; i++) {
        if (service->agents[i]->status == AGENT_STATUS_AVAILABLE) {
            installed_agents[index++] = service->agents[i];
        }
    }

    *agents = installed_agents;
    *count = installed_count;
    return 0;
}

/**
 * @brief 获取已安装的 Skill 列表
 * @param service 服务句柄
 * @param skills 输出参数，返回 Skill 信息数组
 * @param count 输出参数，返回 Skill 数量
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_get_installed_skills(market_service_t* service, skill_info_t*** skills, size_t* count) {
    if (!service || !skills || !count) {
        return -1;
    }

    // 简单返回所有 Skill（实际应该检查安装状态）
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

/**
 * @brief 检查更新
 * @param service 服务句柄
 * @param id Agent 或 Skill ID
 * @param has_update 输出参数，返回是否有更新
 * @param latest_version 输出参数，返回最新版本
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_check_update(market_service_t* service, const char* id, bool* has_update, char** latest_version) {
    if (!service || !id || !has_update || !latest_version) {
        return -1;
    }

    // 检查 Agent
    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, id) == 0) {
            // 模拟检查更新
            *has_update = false;
            *latest_version = strdup(service->agents[i]->version);
            return 0;
        }
    }

    // 检查 Skill
    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, id) == 0) {
            // 模拟检查更新
            *has_update = false;
            *latest_version = strdup(service->skills[i]->version);
            return 0;
        }
    }

    return -3; // 未找到
}

/**
 * @brief 重载配置
 * @param service 服务句柄
 * @param manager 新的配置信息
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_reload_config(market_service_t* service, const market_config_t* manager) {
    if (!service || !manager) {
        return -1;
    }

    service->manager = *manager;
    return 0;
}

/**
 * @brief 同步注册中心
 * @param service 服务句柄
 * @return 0 表示成功，非 0 表示错误码
 */
int market_service_sync_registry(market_service_t* service) {
    if (!service) {
        return -1;
    }

    // 模拟同步过程
    printf("同步注册中心: %s\n", service->manager.registry_url);
    return 0;
}

/**
 * @brief 市场服务主函数
 */
int main() {
    // 配置市场服务
    market_config_t manager = {
        .registry_url = "http://registry.agentos.org",
        .storage_path = "./market",
        .sync_interval_ms = 60000,
        .cache_ttl_ms = 300000,
        .enable_remote_registry = true,
        .enable_auto_update = true
    };

    // 创建市场服务
    market_service_t* service = NULL;
    int ret = market_service_create(&manager, &service);
    if (ret != 0) {
        printf("创建市场服务失败: %d\n", ret);
        return ret;
    }

    printf("市场服务已启动\n");

    // 模拟一些操作
    // 1. 注册 Agent
    agent_info_t agent = {
        .agent_id = "agent-001",
        .name = "助手 Agent",
        .version = "1.0.0",
        .description = "通用助手 Agent",
        .type = AGENT_TYPE_ASSISTANT,
        .status = AGENT_STATUS_AVAILABLE,
        .author = "SPHARX",
        .repository = "https://github.com/spharx/agent-001",
        .dependencies = "none",
        .rating = 4.5,
        .download_count = 1000,
        .last_updated = (uint64_t)time(NULL) * 1000
    };
    market_service_register_agent(service, &agent);

    // 2. 注册 Skill
    skill_info_t skill = {
        .skill_id = "skill-001",
        .name = "天气查询",
        .version = "1.0.0",
        .description = "查询天气信息",
        .type = SKILL_TYPE_TOOL,
        .author = "SPHARX",
        .repository = "https://github.com/spharx/skill-001",
        .dependencies = "none",
        .rating = 4.8,
        .download_count = 2000,
        .last_updated = (uint64_t)time(NULL) * 1000
    };
    market_service_register_skill(service, &skill);

    // 3. 搜索 Agent
    search_params_t search_params = {
        .query = "助手",
        .agent_type = AGENT_TYPE_ASSISTANT,
        .skill_type = SKILL_TYPE_COUNT,
        .only_installed = false,
        .sort_by_rating = true,
        .sort_by_download = false,
        .limit = 10,
        .offset = 0
    };

    agent_info_t** agents = NULL;
    size_t agent_count = 0;
    market_service_search_agents(service, &search_params, &agents, &agent_count);
    printf("搜索到 %zu 个 Agent\n", agent_count);
    if (agents) {
        free(agents);
    }

    // 4. 安装 Agent
    install_request_t install_request = {
        .id = "agent-001",
        .version = NULL,
        .force_update = false,
        .install_path = NULL
    };

    install_result_t* install_result = NULL;
    market_service_install_agent(service, &install_request, &install_result);
    if (install_result) {
        printf("安装结果: %s\n", install_result->success ? "成功" : "失败");
        if (install_result->message) {
            printf("消息: %s\n", install_result->message);
            free(install_result->message);
        }
        if (install_result->installed_version) {
            free(install_result->installed_version);
        }
        if (install_result->install_path) {
            free(install_result->install_path);
        }
        free(install_result);
    }

    // 5. 获取已安装的 Agent
    market_service_get_installed_agents(service, &agents, &agent_count);
    printf("已安装 %zu 个 Agent\n", agent_count);
    if (agents) {
        free(agents);
    }

    // 6. 同步注册中心
    market_service_sync_registry(service);

    // 销毁市场服务
    market_service_destroy(service);

    printf("市场服务已停止\n");
    return 0;
}
