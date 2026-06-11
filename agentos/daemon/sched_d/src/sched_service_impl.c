#include "memory_compat.h"
#include "error.h"
/**
 * @file sched_service_impl.c
 * @brief 调度服务核心实现
 * @details 定义 struct sched_service 并实现 scheduler_service.h 中的所有公共API
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "scheduler_service.h"
#include "svc_logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_AGENTS 128

struct sched_service {
    sched_config_t config;
    agent_info_t *agents[MAX_AGENTS];
    size_t agent_count;
    uint64_t total_tasks_scheduled;
    uint64_t total_success;
    int initialized;
};

int sched_service_create(const sched_config_t *config, sched_service_t **service)
{
    if (!config || !service) {
        SVC_LOG_ERROR("sched_service_create: NULL parameter (config=%p, service=%p)", (const void *)config, (const void *)service);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    sched_service_t *svc = (sched_service_t *)AGENTOS_CALLOC(1, sizeof(sched_service_t));
    if (!svc) {
        SVC_LOG_ERROR("sched_service_create: calloc failed for service");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    __builtin_memcpy(&svc->config, config, sizeof(sched_config_t));
    if (config->ml_model_path)
        svc->config.ml_model_path = AGENTOS_STRDUP(config->ml_model_path);

    svc->initialized = 1;
    *service = svc;
    return 0;
}

int sched_service_destroy(sched_service_t *service)
{
    if (!service) {
        SVC_LOG_ERROR("sched_service_destroy: NULL service parameter");
        return AGENTOS_ERR_INVALID_PARAM;
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]) {
            AGENTOS_FREE(service->agents[i]->agent_id);
            AGENTOS_FREE(service->agents[i]->agent_name);
            AGENTOS_FREE(service->agents[i]);
        }
    }

    AGENTOS_FREE((void *)service->config.ml_model_path);
    AGENTOS_FREE(service);
    return 0;
}

int sched_service_register_agent(sched_service_t *service, const agent_info_t *agent_info)
{
    if (!service || !agent_info || !service->initialized) {
        SVC_LOG_ERROR("sched_service_register_agent: NULL parameter or not initialized (service=%p, agent_info=%p, initialized=%d)", (const void *)service, (const void *)agent_info, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }
    if (service->agent_count >= MAX_AGENTS) {
        SVC_LOG_ERROR("sched_service_register_agent: max agents exceeded (count=%zu, max=%d)", service->agent_count, MAX_AGENTS);
        return AGENTOS_ERR_OVERFLOW;
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_info->agent_id) == 0) {
            service->agents[i]->load_factor = agent_info->load_factor;
            service->agents[i]->success_rate = agent_info->success_rate;
            service->agents[i]->avg_response_time_ms = agent_info->avg_response_time_ms;
            service->agents[i]->is_available = agent_info->is_available;
            service->agents[i]->weight = agent_info->weight;
            return 0;
        }
    }

    agent_info_t *new_agent = (agent_info_t *)AGENTOS_CALLOC(1, sizeof(agent_info_t));
    if (!new_agent) {
        SVC_LOG_ERROR("sched_service_register_agent: calloc failed for new agent");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    if (!agent_info->agent_id) {
        SVC_LOG_ERROR("sched_service_register_agent: agent_info->agent_id is NULL");
        AGENTOS_FREE(new_agent);
        return AGENTOS_ERR_INVALID_PARAM;
    }
    new_agent->agent_id = AGENTOS_STRDUP(agent_info->agent_id);
    new_agent->agent_name =
        agent_info->agent_name ? AGENTOS_STRDUP(agent_info->agent_name) : AGENTOS_STRDUP("");
    if (!new_agent->agent_id || !new_agent->agent_name) {
        SVC_LOG_ERROR("sched_service_register_agent: strdup failed for agent fields (agent_id=%p, agent_name=%p)", (const void *)new_agent->agent_id, (const void *)new_agent->agent_name);
        AGENTOS_FREE(new_agent->agent_id);
        AGENTOS_FREE(new_agent->agent_name);
        AGENTOS_FREE(new_agent);
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }
    new_agent->load_factor = agent_info->load_factor;
    new_agent->success_rate = agent_info->success_rate;
    new_agent->avg_response_time_ms = agent_info->avg_response_time_ms;
    new_agent->is_available = agent_info->is_available;
    new_agent->weight = agent_info->weight;

    service->agents[service->agent_count++] = new_agent;
    return 0;
}

int sched_service_unregister_agent(sched_service_t *service, const char *agent_id)
{
    if (!service || !agent_id || !service->initialized) {
        SVC_LOG_ERROR("sched_service_unregister_agent: NULL parameter or not initialized (service=%p, agent_id=%p, initialized=%d)", (const void *)service, (const void *)agent_id, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_id) == 0) {
            AGENTOS_FREE(service->agents[i]->agent_id);
            AGENTOS_FREE(service->agents[i]->agent_name);
            AGENTOS_FREE(service->agents[i]);

            for (size_t j = i; j < service->agent_count - 1; j++) {
                service->agents[j] = service->agents[j + 1];
            }
            service->agent_count--;
            return 0;
        }
    }
    SVC_LOG_ERROR("sched_service_unregister_agent: agent not found (agent_id=%s)", agent_id ? agent_id : "NULL");
    return AGENTOS_ERR_NOT_FOUND;
}

int sched_service_update_agent_status(sched_service_t *service, const agent_info_t *agent_info)
{
    if (!service || !agent_info || !service->initialized) {
        SVC_LOG_ERROR("sched_service_update_agent_status: NULL parameter or not initialized (service=%p, agent_info=%p, initialized=%d)", (const void *)service, (const void *)agent_info, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    for (size_t i = 0; i < service->agent_count; i++) {
        if (strcmp(service->agents[i]->agent_id, agent_info->agent_id) == 0) {
            service->agents[i]->load_factor = agent_info->load_factor;
            service->agents[i]->success_rate = agent_info->success_rate;
            service->agents[i]->avg_response_time_ms = agent_info->avg_response_time_ms;
            service->agents[i]->is_available = agent_info->is_available;
            service->agents[i]->weight = agent_info->weight;
            return 0;
        }
    }
    SVC_LOG_ERROR("sched_service_update_agent_status: agent not found (agent_id=%s)", agent_info->agent_id ? agent_info->agent_id : "NULL");
    return AGENTOS_ERR_NOT_FOUND;
}

int sched_service_schedule_task(sched_service_t *service, const task_info_t *task_info,
                                sched_result_t **result)
{
    if (!service || !task_info || !result || !service->initialized) {
        SVC_LOG_ERROR("sched_service_schedule_task: NULL parameter or not initialized (service=%p, task_info=%p, result=%p, initialized=%d)", (const void *)service, (const void *)task_info, (const void *)result, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    sched_result_t *res = (sched_result_t *)AGENTOS_CALLOC(1, sizeof(sched_result_t));
    if (!res) {
        SVC_LOG_ERROR("sched_service_schedule_task: calloc failed for result");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    agent_info_t *best_agent = NULL;
    float best_score = -1.0f;

    for (size_t i = 0; i < service->agent_count; i++) {
        if (!service->agents[i]->is_available)
            continue;

        float score = 0.0f;
        switch (service->config.strategy) {
        case SCHED_STRATEGY_WEIGHTED:
            score = service->agents[i]->weight * service->agents[i]->success_rate *
                    (1.0f - service->agents[i]->load_factor);
            break;
        case SCHED_STRATEGY_ROUND_ROBIN:
            if (service->agent_count == 0) {
                score = 0.0f;
            } else {
                score = (float)(service->total_tasks_scheduled % service->agent_count);
                if ((size_t)score == i)
                    score = 100.0f;
                else
                    score = 0.0f;
            }
            break;
        case SCHED_STRATEGY_ML_BASED:
            score = service->agents[i]->success_rate * (1.0f - service->agents[i]->load_factor);
            break;
        default:
            score = service->agents[i]->weight;
            break;
        }

        if (score > best_score) {
            best_score = score;
            best_agent = service->agents[i];
        }
    }

    service->total_tasks_scheduled++;

    if (best_agent) {
        res->selected_agent_id = AGENTOS_STRDUP(best_agent->agent_id);
        res->confidence = best_score > 0 ? (best_score > 1.0f ? 1.0f : best_score) : 0.5f;
        res->estimated_time_ms = best_agent->avg_response_time_ms;
        service->total_success++;
    } else {
        SVC_LOG_ERROR("sched_service_schedule_task: no available agent found (agent_count=%zu, strategy=%d)", service->agent_count, service->config.strategy);
        res->selected_agent_id = NULL;
        res->confidence = 0.0f;
        res->estimated_time_ms = 0;
    }

    *result = res;
    return 0;
}

int sched_service_get_stats(sched_service_t *service, void **stats)
{
    if (!service || !stats || !service->initialized) {
        SVC_LOG_ERROR("sched_service_get_stats: NULL parameter or not initialized (service=%p, stats=%p, initialized=%d)", (const void *)service, (const void *)stats, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    char *json_stats = (char *)AGENTOS_MALLOC(512);
    if (!json_stats) {
        SVC_LOG_ERROR("sched_service_get_stats: malloc failed for stats JSON");
        return AGENTOS_ERR_OUT_OF_MEMORY;
    }

    snprintf(json_stats, 512,
             "{\"agent_count\":%zu,\"total_tasks\":%llu,\"success_rate\":\"%.2f\",\"strategy\":%d}",
             service->agent_count, (unsigned long long)service->total_tasks_scheduled,
             service->total_tasks_scheduled > 0
                 ? (float)service->total_success / (float)service->total_tasks_scheduled
                 : 0.0f,
             service->config.strategy);

    *stats = json_stats;
    return 0;
}

int sched_service_health_check(sched_service_t *service, bool *health_status)
{
    if (!service || !health_status || !service->initialized) {
        SVC_LOG_ERROR("sched_service_health_check: NULL parameter or not initialized (service=%p, health_status=%p, initialized=%d)", (const void *)service, (const void *)health_status, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    bool all_healthy = true;
    for (size_t i = 0; i < service->agent_count; i++) {
        if (service->agents[i]->load_factor > 0.95f) {
            all_healthy = false;
            break;
        }
    }

    *health_status = all_healthy && service->initialized;
    return 0;
}

int sched_service_reload_config(sched_service_t *service, const sched_config_t *config)
{
    if (!service || !config || !service->initialized) {
        SVC_LOG_ERROR("sched_service_reload_config: NULL parameter or not initialized (service=%p, config=%p, initialized=%d)", (const void *)service, (const void *)config, service ? service->initialized : -1);
        return AGENTOS_ERR_INVALID_PARAM;
    }

    AGENTOS_FREE((void *)service->config.ml_model_path);
    service->config.ml_model_path = NULL;

    __builtin_memcpy(&service->config, config, sizeof(sched_config_t));
    if (config->ml_model_path) {
        service->config.ml_model_path = AGENTOS_STRDUP(config->ml_model_path);
        if (!service->config.ml_model_path) {
            SVC_LOG_ERROR("sched_service_reload_config: strdup failed for ml_model_path (path=%s)", config->ml_model_path ? config->ml_model_path : "NULL");
            return AGENTOS_ERR_OUT_OF_MEMORY;
        }
    }

    return 0;
}
