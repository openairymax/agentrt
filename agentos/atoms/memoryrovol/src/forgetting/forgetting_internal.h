/**
 * @file forgetting_internal.h
 * @brief 遗忘机制内部实现定义（不对外暴露）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_FORGETTING_INTERNAL_H
#define AGENTOS_FORGETTING_INTERNAL_H

#include "forgetting.h"
#include "agentos.h"

typedef struct agentos_layer1_raw agentos_layer1_raw_t;
typedef struct agentos_layer2_feature agentos_layer2_feature_t;

struct agentos_forgetting_engine {
    agentos_forgetting_config_t manager;
    agentos_layer1_raw_t* layer1;
    agentos_layer2_feature_t* layer2;
    agentos_mutex_t* lock;
    int auto_running;
    agentos_thread_t auto_thread;
    void* adaptive;
};

#endif /* AGENTOS_FORGETTING_INTERNAL_H */
