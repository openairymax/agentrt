/**
 * @file forgetting.h
 * @brief 遗忘机制接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_FORGETTING_H
#define AGENTOS_FORGETTING_H

#include "agentos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct agentos_layer1_raw agentos_layer1_raw_t;
typedef struct agentos_layer2_feature agentos_layer2_feature_t;

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

typedef struct agentos_forgetting_engine {
    agentos_forgetting_config_t manager;
    agentos_layer1_raw_t* layer1;
    agentos_layer2_feature_t* layer2;
    agentos_mutex_t* lock;
    int auto_running;
    agentos_thread_t auto_thread;
    void* adaptive;
} agentos_forgetting_engine_t;

/**
 * @brief 创建遗忘引擎
 * @param manager 配置（若为NULL使用默认）
 * @param layer1 L1 原始卷句柄（用于访问元数据）
 * @param layer2 L2 特征层句柄（用于联动删除）
 * @param out_engine 输出引擎句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_forgetting_create(
    const agentos_forgetting_config_t* manager,
    agentos_layer1_raw_t* layer1,
    agentos_layer2_feature_t* layer2,
    agentos_forgetting_engine_t** out_engine);

/**
 * @brief 销毁遗忘引擎
 * @param engine 引擎句柄
 */
void agentos_forgetting_destroy(agentos_forgetting_engine_t* engine);

/**
 * @brief 执行一次遗忘检查（裁剪低权重记忆）
 * @param engine 遗忘引擎
 * @param out_pruned_count 输出裁剪的记录数量
 * @return agentos_error_t
 */
agentos_error_t agentos_forgetting_prune(
    agentos_forgetting_engine_t* engine,
    uint32_t* out_pruned_count);

/**
 * @brief 启动自动遗忘任务（后台线程）
 * @param engine 遗忘引擎
 * @return agentos_error_t
 */
agentos_error_t agentos_forgetting_start_auto(agentos_forgetting_engine_t* engine);

/**
 * @brief 停止自动遗忘
 * @param engine 遗忘引擎
 */
void agentos_forgetting_stop_auto(agentos_forgetting_engine_t* engine);

/**
 * @brief 获取记录的当前遗忘权重
 * @param engine 遗忘引擎
 * @param record_id 记录ID
 * @param out_weight 输出权重（0-1，1表示完全保留）
 * @return agentos_error_t
 */
agentos_error_t agentos_forgetting_get_weight(
    agentos_forgetting_engine_t* engine,
    const char* record_id,
    float* out_weight);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_FORGETTING_H */
