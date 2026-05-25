#include "memory_compat.h"
/**
 * @file market_service_impl.c
 * @brief 市场服务核心实现
 * @details 定义 struct market_service 并实现 market_service.h 中的所有公共API
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "market_service.h"
#include "svc_logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <libgen.h>
#include <ftw.h>
#include <unistd.h>

#define MAX_AGENTS 256
#define MAX_SKILLS 256

static int nftw_remove_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)ftwbuf;
    if (typeflag == FTW_DP || typeflag == FTW_D) {
        return rmdir(fpath);
    }
    return remove(fpath);
}

static int recursive_remove(const char *path) {
    return nftw(path, nftw_remove_cb, 64, FTW_DEPTH | FTW_PHYS);
}

static int is_safe_for_shell(const char *str) {
    if (!str) return 0;
    const char *dangerous = ";|&$`'\"\\(){}[]!#~<>\n\r";
    for (size_t i = 0; i < strlen(dangerous); i++) {
        if (strchr(str, dangerous[i])) return 0;
    }
    return 1;
}

static int is_valid_url(const char *url) {
    if (!url || strlen(url) == 0) return 0;
    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return 0;
    return is_safe_for_shell(url);
}

static int is_safe_path_component(const char *str) {
    if (!str || strlen(str) == 0) return 0;
    if (strchr(str, '/') || strchr(str, '\\')) return 0;
    if (strstr(str, "..")) return 0;
    return 1;
}

struct market_service {
    market_config_t config;
    agent_info_t* agents[MAX_AGENTS];
    size_t agent_count;
    skill_info_t* skills[MAX_SKILLS];
    size_t skill_count;
    int initialized;
};

int market_service_create(const market_config_t* config, market_service_t** service) {
    market_config_t default_cfg;
    if (!service) return -1;
    if (!config) {
        memset(&default_cfg, 0, sizeof(default_cfg));
        default_cfg.cache_ttl_ms = 3600000;
        default_cfg.sync_interval_ms = 30000;
        config = &default_cfg;
    }

    market_service_t* svc = (market_service_t*)AGENTOS_CALLOC(1, sizeof(market_service_t));
    if (!svc) return -2;

    memcpy(&svc->config, config, sizeof(market_config_t));
    if (config->registry_url) svc->config.registry_url = AGENTOS_STRDUP(config->registry_url);
    if (config->storage_path) svc->config.storage_path = AGENTOS_STRDUP(config->storage_path);

    svc->initialized = 1;
    *service = svc;
    return 0;
}

int market_service_destroy(market_service_t* service) {
    if (!service) return -1;

    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]) {
            AGENTOS_FREE(service->agents[i]->agent_id);
            AGENTOS_FREE(service->agents[i]->name);
            AGENTOS_FREE(service->agents[i]->version);
            AGENTOS_FREE(service->agents[i]->description);
            AGENTOS_FREE(service->agents[i]->author);
            AGENTOS_FREE(service->agents[i]->repository);
            AGENTOS_FREE(service->agents[i]->dependencies);
            AGENTOS_FREE(service->agents[i]);
        }
    }

    for (size_t i = 0; i < service->skill_count; i++) {
        if (service->skills[i]) {
            AGENTOS_FREE(service->skills[i]->skill_id);
            AGENTOS_FREE(service->skills[i]->name);
            AGENTOS_FREE(service->skills[i]->version);
            AGENTOS_FREE(service->skills[i]->description);
            AGENTOS_FREE(service->skills[i]->author);
            AGENTOS_FREE(service->skills[i]->repository);
            AGENTOS_FREE(service->skills[i]->dependencies);
            AGENTOS_FREE(service->skills[i]);
        }
    }

    AGENTOS_FREE((void*)service->config.registry_url);
    AGENTOS_FREE((void*)service->config.storage_path);
    AGENTOS_FREE(service);
    return 0;
}

int market_service_register_agent(market_service_t* service, const agent_info_t* agent_info) {
    if (!service || !agent_info || !service->initialized) return -1;
    if (service->agent_count >= MAX_AGENTS) return -2;

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_info->agent_id) == 0) {
            AGENTOS_FREE(service->agents[i]->name);
            AGENTOS_FREE(service->agents[i]->version);
            AGENTOS_FREE(service->agents[i]->description);
            AGENTOS_FREE(service->agents[i]->author);
            AGENTOS_FREE(service->agents[i]->repository);
            AGENTOS_FREE(service->agents[i]->dependencies);

            service->agents[i]->name = agent_info->name ? AGENTOS_STRDUP(agent_info->name) : NULL;
            service->agents[i]->version = agent_info->version ? AGENTOS_STRDUP(agent_info->version) : NULL;
            service->agents[i]->description = agent_info->description ? AGENTOS_STRDUP(agent_info->description) : NULL;
            service->agents[i]->type = agent_info->type;
            service->agents[i]->status = agent_info->status;
            service->agents[i]->author = agent_info->author ? AGENTOS_STRDUP(agent_info->author) : NULL;
            service->agents[i]->repository = agent_info->repository ? AGENTOS_STRDUP(agent_info->repository) : NULL;
            service->agents[i]->dependencies = agent_info->dependencies ? AGENTOS_STRDUP(agent_info->dependencies) : NULL;
            service->agents[i]->rating = agent_info->rating;
            service->agents[i]->download_count = agent_info->download_count;
            service->agents[i]->last_updated = (uint64_t)time(NULL);
            return 0;
        }
    }

    agent_info_t* new_agent = (agent_info_t*)AGENTOS_CALLOC(1, sizeof(agent_info_t));
    if (!new_agent) return -3;

    new_agent->agent_id = agent_info->agent_id ? AGENTOS_STRDUP(agent_info->agent_id) : NULL;
    new_agent->name = agent_info->name ? AGENTOS_STRDUP(agent_info->name) : NULL;
    new_agent->version = agent_info->version ? AGENTOS_STRDUP(agent_info->version) : NULL;
    new_agent->description = agent_info->description ? AGENTOS_STRDUP(agent_info->description) : NULL;
    new_agent->type = agent_info->type;
    new_agent->status = agent_info->status;
    new_agent->author = agent_info->author ? AGENTOS_STRDUP(agent_info->author) : NULL;
    new_agent->repository = agent_info->repository ? AGENTOS_STRDUP(agent_info->repository) : NULL;
    new_agent->dependencies = agent_info->dependencies ? AGENTOS_STRDUP(agent_info->dependencies) : NULL;
    if (!new_agent->agent_id || !new_agent->name || !new_agent->version) {
        AGENTOS_FREE(new_agent->agent_id);
        AGENTOS_FREE(new_agent->name);
        AGENTOS_FREE(new_agent->version);
        AGENTOS_FREE(new_agent->description);
        AGENTOS_FREE(new_agent->author);
        AGENTOS_FREE(new_agent->repository);
        AGENTOS_FREE(new_agent->dependencies);
        AGENTOS_FREE(new_agent);
        return -3;
    }
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
            AGENTOS_FREE(service->skills[i]->name);
            AGENTOS_FREE(service->skills[i]->version);
            AGENTOS_FREE(service->skills[i]->description);
            AGENTOS_FREE(service->skills[i]->author);
            AGENTOS_FREE(service->skills[i]->repository);
            AGENTOS_FREE(service->skills[i]->dependencies);

            service->skills[i]->name = skill_info->name ? AGENTOS_STRDUP(skill_info->name) : NULL;
            service->skills[i]->version = skill_info->version ? AGENTOS_STRDUP(skill_info->version) : NULL;
            service->skills[i]->description = skill_info->description ? AGENTOS_STRDUP(skill_info->description) : NULL;
            service->skills[i]->type = skill_info->type;
            service->skills[i]->author = skill_info->author ? AGENTOS_STRDUP(skill_info->author) : NULL;
            service->skills[i]->repository = skill_info->repository ? AGENTOS_STRDUP(skill_info->repository) : NULL;
            service->skills[i]->dependencies = skill_info->dependencies ? AGENTOS_STRDUP(skill_info->dependencies) : NULL;
            service->skills[i]->rating = skill_info->rating;
            service->skills[i]->download_count = skill_info->download_count;
            service->skills[i]->last_updated = (uint64_t)time(NULL);
            return 0;
        }
    }

    skill_info_t* new_skill = (skill_info_t*)AGENTOS_CALLOC(1, sizeof(skill_info_t));
    if (!new_skill) return -3;

    new_skill->skill_id = skill_info->skill_id ? AGENTOS_STRDUP(skill_info->skill_id) : NULL;
    new_skill->name = skill_info->name ? AGENTOS_STRDUP(skill_info->name) : NULL;
    new_skill->version = skill_info->version ? AGENTOS_STRDUP(skill_info->version) : NULL;
    new_skill->description = skill_info->description ? AGENTOS_STRDUP(skill_info->description) : NULL;
    new_skill->type = skill_info->type;
    new_skill->author = skill_info->author ? AGENTOS_STRDUP(skill_info->author) : NULL;
    new_skill->repository = skill_info->repository ? AGENTOS_STRDUP(skill_info->repository) : NULL;
    new_skill->dependencies = skill_info->dependencies ? AGENTOS_STRDUP(skill_info->dependencies) : NULL;
    if (!new_skill->skill_id || !new_skill->name || !new_skill->version) {
        AGENTOS_FREE(new_skill->skill_id);
        AGENTOS_FREE(new_skill->name);
        AGENTOS_FREE(new_skill->version);
        AGENTOS_FREE(new_skill->description);
        AGENTOS_FREE(new_skill->author);
        AGENTOS_FREE(new_skill->repository);
        AGENTOS_FREE(new_skill->dependencies);
        AGENTOS_FREE(new_skill);
        return -3;
    }
    new_skill->rating = skill_info->rating;
    new_skill->download_count = skill_info->download_count;
    new_skill->last_updated = (uint64_t)time(NULL);

    service->skills[service->skill_count++] = new_skill;
    return 0;
}

int market_service_search_agents(market_service_t* service, const search_params_t* params, agent_info_t*** agents, size_t* count) {
    if (!service || !params || !agents || !count || !service->initialized) return -1;

    size_t results_size = 16;
    agent_info_t** results = (agent_info_t**)AGENTOS_MALLOC(sizeof(agent_info_t*) * results_size);
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
            agent_info_t** tmp = (agent_info_t**)AGENTOS_REALLOC(results, sizeof(agent_info_t*) * results_size);
            if (!tmp) { AGENTOS_FREE(results); return -2; }
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
    skill_info_t** results = (skill_info_t**)AGENTOS_MALLOC(sizeof(skill_info_t*) * results_size);
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
            skill_info_t** tmp = (skill_info_t**)AGENTOS_REALLOC(results, sizeof(skill_info_t*) * results_size);
            if (!tmp) { AGENTOS_FREE(results); return -2; }
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
    if (!is_safe_path_component(request->id)) return -1;

    install_result_t* res = (install_result_t*)AGENTOS_CALLOC(1, sizeof(install_result_t));
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
        res->message = AGENTOS_STRDUP("Agent not found");
        res->error_code = -3;
        *result = res;
        return 0;
    }

    const char* base_path = request->install_path ? request->install_path :
                            (service->config.storage_path ? service->config.storage_path : "./agents");
    char install_dir[1024];
    snprintf(install_dir, sizeof(install_dir), "%s/%s", base_path, request->id);

    {
        int mkret = mkdir(install_dir, 0755);
        if (mkret != 0 && errno != EEXIST) {
            res->success = false;
            res->message = AGENTOS_STRDUP("Failed to create install directory");
            res->error_code = -4;
            *result = res;
            return 0;
        }
    }

    char meta_path[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
    snprintf(meta_path, sizeof(meta_path), "%s/agent.json", install_dir);
#pragma GCC diagnostic pop
    FILE* meta_fp = fopen(meta_path, "w");
    if (meta_fp) {
        fprintf(meta_fp, "{\n");
        fprintf(meta_fp, "  \"agent_id\": \"%s\",\n", target->agent_id);
        fprintf(meta_fp, "  \"name\": \"%s\",\n", target->name ? target->name : "");
        fprintf(meta_fp, "  \"version\": \"%s\",\n", request->version ? request->version : (target->version ? target->version : "0.0.1"));
        fprintf(meta_fp, "  \"author\": \"%s\",\n", target->author ? target->author : "");
        fprintf(meta_fp, "  \"status\": \"installed\",\n");
        fprintf(meta_fp, "  \"installed_at\": %lld\n", (long long)time(NULL));
        fprintf(meta_fp, "}\n");
        fclose(meta_fp);
    }

    if (target->repository && strlen(target->repository) > 0 && is_valid_url(target->repository)) {
        char download_path[1024];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(download_path, sizeof(download_path), "%s/package.tar.gz", install_dir);
#pragma GCC diagnostic pop

        pid_t curl_pid = fork();
        if (curl_pid == 0) {
            execlp("curl", "curl", "-sfL", "-o", download_path,
                   target->repository, (char*)NULL);
            _exit(127);
        } else if (curl_pid > 0) {
            int curl_status = 0;
            waitpid(curl_pid, &curl_status, 0);
            int curl_ret = WIFEXITED(curl_status) ? WEXITSTATUS(curl_status) : -1;
            if (curl_ret != 0) {
                SVC_LOG_WARN("Download failed for agent %s from %s (curl_ret=%d), metadata only install",
                            request->id, target->repository, curl_ret);
            } else {
                pid_t tar_pid = fork();
                if (tar_pid == 0) {
                    execlp("tar", "tar", "-xzf", download_path, "-C",
                           install_dir, (char*)NULL);
                    _exit(127);
                } else if (tar_pid > 0) {
                    int tar_status = 0;
                    waitpid(tar_pid, &tar_status, 0);
                }
                remove(download_path);
            }
        } else {
            SVC_LOG_WARN("fork failed for agent %s download: %s",
                        request->id, strerror(errno));
        }
    }

    target->status = AGENT_STATUS_AVAILABLE;
    target->download_count++;

    res->success = true;
    res->message = AGENTOS_STRDUP("Agent installed successfully");
    res->installed_version = AGENTOS_STRDUP(request->version ? request->version : target->version);
    res->install_path = AGENTOS_STRDUP(install_dir);
    res->error_code = 0;

    *result = res;
    return 0;
}

int market_service_install_skill(market_service_t* service, const install_request_t* request, install_result_t** result) {
    if (!service || !request || !result || !service->initialized) return -1;

    install_result_t* res = (install_result_t*)AGENTOS_CALLOC(1, sizeof(install_result_t));
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
        res->message = AGENTOS_STRDUP("Skill not found");
        res->error_code = -3;
        *result = res;
        return 0;
    }

    target->download_count++;

    res->success = true;
    res->message = AGENTOS_STRDUP("Skill installed successfully");
    res->installed_version = AGENTOS_STRDUP(request->version ? request->version : target->version);
    res->install_path = AGENTOS_STRDUP(request->install_path ? request->install_path : "./skills");
    res->error_code = 0;

    *result = res;
    return 0;
}

int market_service_uninstall_agent(market_service_t* service, const char* agent_id) {
    if (!service || !agent_id || !service->initialized) return -1;
    if (!is_safe_path_component(agent_id)) return -1;

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_id) == 0) {
            const char* storage = service->config.storage_path ?
                                  service->config.storage_path : "./agents";
            char install_dir[1024];
            snprintf(install_dir, sizeof(install_dir), "%s/%s", storage, agent_id);

            int rm_ret = recursive_remove(install_dir);
            if (rm_ret != 0) {
                SVC_LOG_WARN("Failed to remove install directory: %s (ret=%d)",
                           install_dir, rm_ret);
            }

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
            AGENTOS_FREE(service->skills[i]->skill_id);
            AGENTOS_FREE(service->skills[i]->name);
            AGENTOS_FREE(service->skills[i]->version);
            AGENTOS_FREE(service->skills[i]->description);
            AGENTOS_FREE(service->skills[i]->author);
            AGENTOS_FREE(service->skills[i]->repository);
            AGENTOS_FREE(service->skills[i]->dependencies);
            AGENTOS_FREE(service->skills[i]);

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
    agent_info_t** results = (agent_info_t**)AGENTOS_MALLOC(sizeof(agent_info_t*) * results_size);
    if (!results) return -2;

    size_t found = 0;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]->status == AGENT_STATUS_AVAILABLE ||
            service->agents[i]->status == AGENT_STATUS_ERROR) {

            if (found >= results_size) {
                results_size *= 2;
                agent_info_t** tmp = (agent_info_t**)AGENTOS_REALLOC(results, sizeof(agent_info_t*) * results_size);
                if (!tmp) { AGENTOS_FREE(results); return -2; }
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
    skill_info_t** results = (skill_info_t**)AGENTOS_MALLOC(sizeof(skill_info_t*) * results_size);
    if (!results) return -2;

    size_t found = 0;
    for (size_t i = 0; i < service->skill_count; i++) {
        if (found >= results_size) {
            results_size *= 2;
            skill_info_t** tmp = (skill_info_t**)AGENTOS_REALLOC(results, sizeof(skill_info_t*) * results_size);
            if (!tmp) { AGENTOS_FREE(results); return -2; }
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
            *latest_version = AGENTOS_STRDUP(service->agents[i]->version);
            return 0;
        }
    }

    for (size_t i = 0; i < service->skill_count; i++) {
        if (strcmp(service->skills[i]->skill_id, id) == 0) {
            *latest_version = AGENTOS_STRDUP(service->skills[i]->version);
            return 0;
        }
    }

    *latest_version = NULL;
    return -3;
}

int market_service_reload_config(market_service_t* service, const market_config_t* config) {
    if (!service || !config || !service->initialized) return -1;

    AGENTOS_FREE((void*)service->config.registry_url);
    AGENTOS_FREE((void*)service->config.storage_path);
    service->config.registry_url = NULL;
    service->config.storage_path = NULL;

    memcpy(&service->config, config, sizeof(market_config_t));
    if (config->registry_url) {
        service->config.registry_url = AGENTOS_STRDUP(config->registry_url);
        if (!service->config.registry_url) return -2;
    }
    if (config->storage_path) {
        service->config.storage_path = AGENTOS_STRDUP(config->storage_path);
        if (!service->config.storage_path) return -2;
    }

    return 0;
}

int market_service_sync_registry(market_service_t* service) {
    if (!service || !service->initialized) return -1;

    if (!service->config.enable_remote_registry) {
        return 0;
    }

    if (!service->config.registry_url || strlen(service->config.registry_url) == 0) {
        SVC_LOG_WARN("Sync registry: no registry_url configured");
        return 0;
    }

    const char* storage = service->config.storage_path ?
                          service->config.storage_path : AGENTOS_CACHE_DIR;

    {
        size_t pos = 0;
        char tmp[1024];
        strncpy(tmp, storage, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
        while (tmp[pos]) {
            if (tmp[pos] == '/' && pos > 0) {
                tmp[pos] = '\0';
                mkdir(tmp, 0755);
                tmp[pos] = '/';
            }
            pos++;
        }
        mkdir(tmp, 0755);
    }

    char index_path[1024];
    snprintf(index_path, sizeof(index_path), "%s/registry_index.json", storage);

    if (!is_safe_for_shell(service->config.registry_url)) {
        SVC_LOG_WARN("Sync registry: invalid registry_url, possible injection detected");
        return 0;
    }

    char url[2048];
    if (strncmp(service->config.registry_url, "http", 4) == 0) {
        snprintf(url, sizeof(url), "%s/index.json", service->config.registry_url);
    } else {
        snprintf(url, sizeof(url), "https://%s/index.json", service->config.registry_url);
    }

    pid_t curl_pid = fork();
    if (curl_pid == 0) {
        execlp("curl", "curl", "-sfL", "-o", index_path, url,
               "--connect-timeout", "10", "--max-time", "60", (char*)NULL);
        _exit(127);
    } else if (curl_pid > 0) {
        int curl_status = 0;
        waitpid(curl_pid, &curl_status, 0);
        int curl_ret = WIFEXITED(curl_status) ? WEXITSTATUS(curl_status) : -1;
        if (curl_ret != 0) {
            SVC_LOG_WARN("Sync registry: download failed from %s (curl_ret=%d)", url, curl_ret);
            return 0;
        }
    } else {
        SVC_LOG_WARN("Sync registry: fork failed: %s", strerror(errno));
        return -2;
    }

    FILE* idx_fp = fopen(index_path, "r");
    if (!idx_fp) {
        SVC_LOG_WARN("Sync registry: cannot open downloaded index %s", index_path);
        return 0;
    }

    fseek(idx_fp, 0, SEEK_END);
    long fsize = ftell(idx_fp);
    fseek(idx_fp, 0, SEEK_SET);

    if (fsize <= 0 || fsize > 10 * 1024 * 1024) {
        fclose(idx_fp);
        SVC_LOG_WARN("Sync registry: invalid index size %ld", fsize);
        return 0;
    }

    char* idx_data = (char*)AGENTOS_MALLOC((size_t)fsize + 1);
    if (!idx_data) {
        fclose(idx_fp);
        return -2;
    }
    size_t nread = fread(idx_data, 1, (size_t)fsize, idx_fp);
    if (nread != (size_t)fsize) { AGENTOS_FREE(idx_data); fclose(idx_fp); return -2; }
    idx_data[nread] = '\0';
    fclose(idx_fp);

    char* entry = strstr(idx_data, "\"agent_id\"");
    int synced = 0;
    while (entry && synced < 256) {
        char* id_start = strchr(entry, ':');
        if (!id_start) break;
        id_start++;
        while (*id_start && (*id_start == ' ' || *id_start == '\t' || *id_start == '"')) id_start++;
        char* id_end = id_start;
        while (*id_end && *id_end != '"' && *id_end != ',' && *id_end != '}') id_end++;

        size_t id_len = (size_t)(id_end - id_start);
        if (id_len > 0 && id_len < 128) {
            char found_id[128];
            memcpy(found_id, id_start, id_len);
            found_id[id_len] = '\0';

            int already_exists = 0;
            for (size_t i = 0; i < service->agent_count; i++) {
                if (strcmp(service->agents[i]->agent_id, found_id) == 0) {
                    already_exists = 1;
                    break;
                }
            }

            if (!already_exists && service->agent_count < MAX_AGENTS) {
                agent_info_t* new_agent = (agent_info_t*)AGENTOS_CALLOC(1, sizeof(agent_info_t));
                if (new_agent) {
                    new_agent->agent_id = AGENTOS_STRDUP(found_id);
                    new_agent->name = AGENTOS_STRDUP(found_id);
                    new_agent->version = AGENTOS_STRDUP("latest");
                    new_agent->status = AGENT_STATUS_AVAILABLE;
                    service->agents[service->agent_count++] = new_agent;
                    synced++;
                }
            }
        }

        entry = strstr(id_end + 1, "\"agent_id\"");
    }

    AGENTOS_FREE(idx_data);
    SVC_LOG_INFO("Sync registry: synced %d new agents from %s", synced, url);
    return 0;
}
