/**
 * @file registry.h
 * @brief 提供商注册表接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef LLM_PROVIDER_REGISTRY_H
#define LLM_PROVIDER_REGISTRY_H

#include "provider.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct service_config service_config_t;

typedef struct {
    const char* name;
    const char* api_key;
    const char* api_base;
    const char* organization;
    double timeout_sec;
    int max_retries;
    char** models;
} provider_config_t;

typedef struct provider_registry provider_registry_t;

provider_registry_t* provider_registry_create(const service_config_t* cfg);
void provider_registry_destroy(provider_registry_t* reg);
const provider_t* provider_registry_find(provider_registry_t* reg, const char* model);

#ifdef __cplusplus
}
#endif

#endif /* LLM_PROVIDER_REGISTRY_H */