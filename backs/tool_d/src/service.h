/**
 * @file service.h
 * @brief 工具服务内部结构声明
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef TOOL_SERVICE_INTERNAL_H
#define TOOL_SERVICE_INTERNAL_H

#include "tool_service.h"
#include "registry.h"
#include "executor.h"
#include "validator.h"
#include "cache.h"
#include "config.h"
#include <pthread.h>

struct tool_service {
    tool_registry_t* registry;
    tool_executor_t* executor;
    tool_validator_t* validator;
    tool_cache_t* cache;
    tool_config_t* config;
    pthread_mutex_t lock;
};

#endif /* TOOL_SERVICE_INTERNAL_H */