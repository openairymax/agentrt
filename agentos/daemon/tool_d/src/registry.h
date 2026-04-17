/**
 * @file registry.h
 * @brief 工具注册表接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_REGISTRY_H
#define TOOL_REGISTRY_H

#include "tool_service.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tool_registry tool_registry_t;

tool_registry_t* tool_registry_create(const tool_config_t* cfg);
void tool_registry_destroy(tool_registry_t* reg);

int tool_registry_add(tool_registry_t* reg, const tool_metadata_t* meta);
int tool_registry_remove(tool_registry_t* reg, const char* tool_id);
tool_metadata_t* tool_registry_get(tool_registry_t* reg, const char* tool_id);
char* tool_registry_list_json(tool_registry_t* reg);

#ifdef __cplusplus
}
#endif

#endif /* TOOL_REGISTRY_H */