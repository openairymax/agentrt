/**
 * @file market_service_impl.c
 * @brief 市场服务核心实现
 * @details 定义 struct market_service 并实现 market_service.h 中的所有公共API
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "market_service.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_AGENTS 256
#define MAX_SKILLS 256

struct market_service {
    market_config_t config;
    agent_info_t* agents[MAX_AGENTS];
    size_t agent_count;
    skill_info_t* skills[MAX_SKILLS];
    size_t skill_count;
    int initialized;
};

int market_service_create(const market_config_t* config, market_service_t** service) {
    if (!config || !service) return -1;

    market_service_t* svc = (market_service_t*)calloc(1, sizeof(market_service_t));
    if (!svc) return -2;

    memcpy(&svc->config, config, sizeof(market_config_t));
    if (config->registry_url) svc->config.registry_url = strdup(config->registry_url);
    if (config->storage_path) svc->config.storage_path = strdup(config->storage_path);

    svc->initialized = 1;
    *service = svc;
    return 0;
}

int market_service_destroy(market_service_t* service) {
    if (!service) return -1;

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

    free((void*)service->config.registry_url);
    free((void*)service->config.storage_path);
    free(service);
    return 0;
}

int market_service_register_agent(market_service_t* service, const agent_info_t* agent_info) {
    if (!service || !agent_info || !service->initialized) return -1;
    if (service->agent_count >= MAX_AGENTS) return -2;

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_info->agent_id) == 0) {
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
            service->agents[i]->last_updated = (uint64_t)time(NULL);
            return 0;
        }
    }

    agent_info_t* new_agent = (agent_info_t*)calloc(1, sizeof(agent_info_t));
    if (!new_agent) return -3;

    new_agent->agent_id = strdup(agent_info->agent_id);
    new_agent->name = strdup(agent_info->name);
    new_agent->version = strdup(agent_info->version);
    new_agent->description = strdup(agent_info->description);
    new_agent->type = agent_info->type;
    new_agent->status = agent_info->status;
    new_agent->author = strdup(agent_info->author);
    new_agent->repository = strdup(agent_info->repository);
    new_agent->dependencies = strdup(agent_info->dependencies);
    new_agent->rating = agent_info->rating;
    new_agent->download_count = agent_info->download_count;
    new_agent->last_updated = (uint64_t)time(NULL);

    service->agents[service->agent_count++] = new_agent;
    return 0;
}

int market_service_register_skill(market_service_t* service, const skill_info_t* skill_info) {
    if (!service || !skill_info || !service->initialized) return -1;
    if (service->skill_count >= MAX_SKILLS) return -2;

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_info->skill_id) == 0) {
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
            service->skills[i]->last_updated = (uint64_t)time(NULL);
            return 0;
        }
    }

    skill_info_t* new_skill = (skill_info_t*)calloc(1, sizeof(skill_info_t));
    if (!new_skill) return -3;

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
    new_skill->last_updated = (uint64_t)time(NULL);

    service->skills[service->skill_count++] = new_skill;
    return 0;
}

int market_service_search_agents(market_service_t* service, const search_params_t* params, agent_info_t*** agents, size_t* count) {
    if (!service || !params || !agents || !count || !service->initialized) return -1;

    size_t results_size = 16;
    agent_info_t** results = (agent_info_t**)malloc(sizeof(agent_info_t*) * results_size);
    if (!results) return -2;

    size_t found = 0;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (params->query && strlen(params->query) > 0) {
            if (!strstr(service->agents[i]->agent_id, params->query) &&
                !strstr(service->agents[i]->name, params->query) &&
                !(service->agents[i]->description && strstr(service->agents[i]->description, params->query))) {
                continue;
            }
        }

        if (found >= results_size) {
            results_size *= 2;
            agent_info_t** tmp = (agent_info_t**)realloc(results, sizeof(agent_info_t*) * results_size);
            if (!tmp) { free(results); return -2; }
            results = tmp;
        }

        results[found++] = service->agents[i];
        if (params->limit > 0 && found >= params->limit) break;
    }

    *agents = results;
    *count = found;
    return 0;
}

int market_service_search_skills(market_service_t* service, const search_params_t* params, skill_info_t*** skills, size_t* count) {
    if (!service || !params || !skills || !count || !service->initialized) return -1;

    size_t results_size = 16;
    skill_info_t** results = (skill_info_t**)malloc(sizeof(skill_info_t*) * results_size);
    if (!results) return -2;

    size_t found = 0;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (params->query && strlen(params->query) > 0) {
            if (!strstr(service->skills[i]->skill_id, params->query) &&
                !strstr(service->skills[i]->name, params->query) &&
                !(service->skills[i]->description && strstr(service->skills[i]->description, params->query))) {
                continue;
            }
        }

        if (found >= results_size) {
            results_size *= 2;
            skill_info_t** tmp = (skill_info_t**)realloc(results, sizeof(skill_info_t*) * results_size);
            if (!tmp) { free(results); return -2; }
            results = tmp;
        }

        results[found++] = service->skills[i];
        if (params->limit > 0 && found >= params->limit) break;
    }

    *skills = results;
    *count = found;
    return 0;
}

int market_service_install_agent(market_service_t* service, const install_request_t* request, install_result_t** result) {
    if (!service || !request || !result || !service->initialized) return -1;

    install_result_t* res = (install_result_t*)calloc(1, sizeof(install_result_t));
    if (!res) return -2;

    agent_info_t* target = NULL;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, request->id) == 0) {
            target = service->agents[i];
            break;
        }
    }

    if (!target) {
        res->success = false;
        res->message = strdup("Agent not found");
        res->error_code = -3;
        *result = res;
        return 0;
    }

    target->status = AGENT_STATUS_AVAILABLE;
    target->download_count++;

    res->success = true;
    res->message = strdup("Agent installed successfully");
    res->installed_version = strdup(request->version ? request->version : target->version);
    res->install_path = strdup(request->install_path ? request->install_path : "./agents");
    res->error_code = 0;

    *result = res;
    return 0;
}

int market_service_install_skill(market_service_t* service, const install_request_t* request, install_result_t** result) {
    if (!service || !request || !result || !service->initialized) return -1;

    install_result_t* res = (install_result_t*)calloc(1, sizeof(install_result_t));
    if (!res) return -2;

    skill_info_t* target = NULL;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, request->id) == 0) {
            target = service->skills[i];
            break;
        }
    }

    if (!target) {
        res->success = false;
        res->message = strdup("Skill not found");
        res->error_code = -3;
        *result = res;
        return 0;
    }

    target->download_count++;

    res->success = true;
    res->message = strdup("Skill installed successfully");
    res->installed_version = strdup(request->version ? request->version : target->version);
    res->install_path = strdup(request->install_path ? request->install_path : "./skills");
    res->error_code = 0;

    *result = res;
    return 0;
}

int market_service_uninstall_agent(market_service_t* service, const char* agent_id) {
    if (!service || !agent_id || !service->initialized) return -1;

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_id) == 0) {
            service->agents[i]->status = AGENT_STATUS_DISABLED;
            return 0;
        }
    }
    return -3;
}

int market_service_uninstall_skill(market_service_t* service, const char* skill_id) {
    if (!service || !skill_id || !service->initialized) return -1;

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, skill_id) == 0) {
            free(service->skills[i]->skill_id);
            free(service->skills[i]->name);
            free(service->skills[i]->version);
            free(service->skills[i]->description);
            free(service->skills[i]->author);
            free(service->skills[i]->repository);
            free(service->skills[i]->dependencies);
            free(service->skills[i]);

            for (size_t j = i; j < service->skill_count - 1; j++) {
                service->skills[j] = service->skills[j + 1];
            }
            service->skill_count--;
            return 0;
        }
    }
    return -3;
}

int market_service_get_installed_agents(market_service_t* service, agent_info_t*** agents, size_t* count) {
    if (!service || !agents || !count || !service->initialized) return -1;

    size_t results_size = 16;
    agent_info_t** results = (agent_info_t**)malloc(sizeof(agent_info_t*) * results_size);
    if (!results) return -2;

    size_t found = 0;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]->status == AGENT_STATUS_AVAILABLE ||
            service->agents[i]->status == AGENT_STATUS_ERROR) {

            if (found >= results_size) {
                results_size *= 2;
                agent_info_t** tmp = (agent_info_t**)realloc(results, sizeof(agent_info_t*) * results_size);
                if (!tmp) { free(results); return -2; }
                results = tmp;
            }

            results[found++] = service->agents[i];
        }
    }

    *agents = results;
    *count = found;
    return 0;
}

int market_service_get_installed_skills(market_service_t* service, skill_info_t*** skills, size_t* count) {
    if (!service || !skills || !count || !service->initialized) return -1;

    size_t results_size = 16;
    skill_info_t** results = (skill_info_t**)malloc(sizeof(skill_info_t*) * results_size);
    if (!results) return -2;

    size_t found = 0;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (found >= results_size) {
            results_size *= 2;
            skill_info_t** tmp = (skill_info_t**)realloc(results, sizeof(skill_info_t*) * results_size);
            if (!tmp) { free(results); return -2; }
            results = tmp;
        }

        results[found++] = service->skills[i];
    }

    *skills = results;
    *count = found;
    return 0;
}

int market_service_check_update(market_service_t* service, const char* id, bool* has_update, char** latest_version) {
    if (!service || !id || !has_update || !latest_version || !service->initialized) return -1;

    *has_update = false;

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, id) == 0) {
            *latest_version = strdup(service->agents[i]->version);
            return 0;
        }
    }

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, id) == 0) {
            *latest_version = strdup(service->skills[i]->version);
            return 0;
        }
    }

    *latest_version = NULL;
    return -3;
}

int market_service_reload_config(market_service_t* service, const market_config_t* config) {
    if (!service || !config || !service->initialized) return -1;

    free((void*)service->config.registry_url);
    free((void*)service->config.storage_path);

    memcpy(&service->config, config, sizeof(market_config_t));
    if (config->registry_url) service->config.registry_url = strdup(config->registry_url);
    if (config->storage_path) service->config.storage_path = strdup(config->storage_path);

    return 0;
}

int market_service_sync_registry(market_service_t* service) {
    if (!service || !service->initialized) return -1;

    if (!service->config.enable_remote_registry) {
        return 0;
    }

    return 0;
}
