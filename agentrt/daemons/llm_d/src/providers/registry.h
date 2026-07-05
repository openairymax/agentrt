/**
 * @file registry.h
 * @brief 提供商注册表接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTRT_LLM_PROVIDER_REGISTRY_H
#define AGENTRT_LLM_PROVIDER_REGISTRY_H

#include "provider.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    const char *api_key;
    const char *api_base;
    const char *organization;
    double timeout_sec;
    int max_retries;
    char **models;
} provider_config_t;

typedef struct service_config {
    size_t llm_cache_capacity;
    uint32_t llm_cache_ttl_sec;
    int max_retries;
    uint32_t timeout_ms;
    const char *token_encoding;
    provider_config_t *providers;
    size_t provider_count;
} service_config_t;

typedef struct provider_registry provider_registry_t;

provider_registry_t *provider_registry_create(const service_config_t *cfg);
provider_registry_t *provider_registry_create_from_config(const service_config_t *cfg,
                                                          const char *config_path);
void provider_registry_destroy(provider_registry_t *reg);
const provider_t *provider_registry_find(provider_registry_t *reg, const char *model);

/**
 * @brief 枚举注册表中所有 (provider, model) 对
 *
 * P3.16 (ACC-DT17): 为 llm_router 端点注册提供遍历接口。
 * 对每个已注册 provider 的每个 model，调用回调 (provider_name, model_name, user_data)。
 *
 * @param reg 注册表
 * @param cb  回调（不可为 NULL）；回调返回非 0 时停止枚举
 * @param user_data 透传给回调的用户数据
 * @return 回调累计返回非 0 的次数（用于短路）；reg/cb 为 NULL 时返回 0
 */
int provider_registry_enumerate(provider_registry_t *reg,
                                int (*cb)(const char *provider_name, const char *model_name,
                                          void *user_data),
                                void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_LLM_PROVIDER_REGISTRY_H */