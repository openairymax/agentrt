/**
 * @file registry.h
 * @brief 提供商注册表接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef LLM_PROVIDER_REGISTRY_H
#define LLM_PROVIDER_REGISTRY_H

#include "provider.h"
#include "svc_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct provider_registry provider_registry_t;

provider_registry_t* provider_registry_create(const service_config_t* cfg);
void provider_registry_destroy(provider_registry_t* reg);
const provider_t* provider_registry_find(provider_registry_t* reg, const char* model);

#ifdef __cplusplus
}
#endif
// From data intelligence emerges. by spharx

#endif /* LLM_PROVIDER_REGISTRY_H */