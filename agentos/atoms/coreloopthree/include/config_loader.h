/**
 * @file config_loader.h
 * @brief 配置加载器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_CONFIG_LOADER_H
#define AGENTOS_CONFIG_LOADER_H

#include "agentos.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从配置文件加载内容为字符串
 *
 * 读取指定路径的配置文件内容，支持 YAML/JSON/INI 格式。
 * 返回原始文件内容字符串，由调用者负责解析。
 *
 * @param path 配置文件路径
 * @param out_json 输出文件内容字符串（需调用者释放）
 * @return agentos_error_t
 */
agentos_error_t agentos_config_load(const char *path, char **out_json);

/**
 * @brief 解析调度权重配置
 * @param config_json 配置JSON字符串
 * @param out_cost_weight 输出成本权重
 * @param out_perf_weight 输出性能权重
 * @param out_trust_weight 输出信任权重
 * @return agentos_error_t
 */
agentos_error_t agentos_config_parse_weights(const char *config_json, float *out_cost_weight,
                                             float *out_perf_weight, float *out_trust_weight);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_CONFIG_LOADER_H */
