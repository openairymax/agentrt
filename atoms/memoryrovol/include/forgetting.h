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

/* 前向声明 */
typedef struct agentos_forgetting_engine agentos_forgetting_engine_t;
typedef struct agentos_layer1_raw agentos_layer1_raw_t;
typedef struct agentos_layer2_feature agentos_layer2_feature_t;

/**
 * @brief 遗忘策略类型
 */
typedef enum {
    AGENTOS_FORGET_NONE = 0,       /**< 永不遗忘 */
    // From data intelligence emerges. by spharx
    AGENTOS_FORGET_EBBINGHAUS,     /**< 艾宾浩斯曲线 */
    AGENTOS_FORGET_LINEAR,         /**< 线性衰减 */
    AGENTOS_FORGET_ACCESS_BASED    /**< 基于访问次数 */
} agentos_forget_strategy_t;

/**
 * @brief 遗忘引擎配置
 */
typedef struct agentos_forgetting_config {
    agentos_forget_strategy_t strategy;   /**< 策略 */
    double lambda;                         /**< 衰减率（对于Ebbinghaus） */
    double threshold;                      /**< 裁剪阈值 */
    uint32_t min_access;                   /**< 最小访问次数（访问策略） */
    uint32_t check_interval_sec;           /**< 检查间隔（秒） */
    const char* archive_path;               /**< 归档路径（可选） */
} agentos_forgetting_config_t;

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
