/**
 * @file forgetting.h
 * @brief 遗忘机制公共接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 公共头文件仅暴露不透明指针和 API 函数声明。
 * 内部结构定义见 forgetting_internal.h（仅实现文件使用）。
 */

#ifndef AGENTOS_FORGETTING_H
#define AGENTOS_FORGETTING_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    AGENTOS_FORGET_NONE = 0,
    AGENTOS_FORGET_EBBINGHAUS,
    AGENTOS_FORGET_LINEAR,
    AGENTOS_FORGET_ACCESS_BASED
} agentos_forget_strategy_t;

typedef struct agentos_forgetting_config {
    agentos_forget_strategy_t strategy;
    double lambda;
    double threshold;
    uint32_t min_access;
    uint32_t check_interval_sec;
    const char* archive_path;
} agentos_forgetting_config_t;

typedef struct agentos_forgetting_engine agentos_forgetting_engine_t;
typedef struct agentos_raw_metadata_db agentos_raw_metadata_db_t;

agentos_error_t agentos_forgetting_create(
    const agentos_forgetting_config_t* manager,
    void* layer1,
    void* layer2,
    agentos_raw_metadata_db_t* meta_db,
    agentos_forgetting_engine_t** out_engine);

void agentos_forgetting_destroy(agentos_forgetting_engine_t* engine);

agentos_error_t agentos_forgetting_prune(
    agentos_forgetting_engine_t* engine,
    uint32_t* out_pruned_count);

agentos_error_t agentos_forgetting_start_auto(agentos_forgetting_engine_t* engine);

void agentos_forgetting_stop_auto(agentos_forgetting_engine_t* engine);

agentos_error_t agentos_forgetting_get_weight(
    agentos_forgetting_engine_t* engine,
    const char* record_id,
    float* out_weight);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_FORGETTING_H */
