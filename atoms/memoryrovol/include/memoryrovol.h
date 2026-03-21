/**
 * @file memoryrovol.h
 * @brief MemoryRovol 系统主接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
 */

#ifndef AGENTOS_MEMORYROV_H
#define AGENTOS_MEMORYROV_H

#include "agentos.h"
#include "layer1_raw.h"
#include "layer2_feature.h"
#include "layer3_structure.h"
#include "layer4_pattern.h"
#include "retrieval.h"
#include "forgetting.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MemoryRovol 系统句柄（不透明指针）
 */
typedef struct agentos_memoryrov_handle agentos_memoryrov_handle_t;

/**
 * @brief 初始化 MemoryRovol 系统
 * @param config 配置参数（如果为 NULL 则使用默认配置）
 * @param out_handle 输出系统句柄
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_init(
    const agentos_memoryrov_config_t* config,
    agentos_memoryrov_handle_t** out_handle);

/**
 * @brief 销毁 MemoryRovol 系统，释放所有资源
 * @param handle 系统句柄
 */
void agentos_memoryrov_cleanup(agentos_memoryrov_handle_t* handle);

/**
 * @brief 执行记忆进化（模式挖掘、固化等）
 * @param handle 系统句柄
 * @param force 强制立即执行（忽略周期设置）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_evolve(
    agentos_memoryrov_handle_t* handle,
    int force);

/**
 * @brief 获取系统统计信息（JSON 格式）
 * @param handle 系统句柄
 * @param out_stats 输出 JSON 字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_memoryrov_stats(
    agentos_memoryrov_handle_t* handle,
    char** out_stats);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORYROV_H */