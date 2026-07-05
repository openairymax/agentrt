/**
 * @file config_loader.h
 * @brief 配置加载器接口
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * C-L01: Manager → CoreLoopThree 连接线
 * config_loader.c 负责文件监听 → yaml_loader.c 负责解析
 */

#ifndef AGENTRT_CONFIG_LOADER_H
#define AGENTRT_CONFIG_LOADER_H

#include "agentrt.h"
#include "yaml_loader.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
 * @return agentrt_error_t
 */
agentrt_error_t agentrt_config_load(const char *path, char **out_json);

/**
 * @brief 解析调度权重配置
 * @param config_json 配置JSON字符串
 * @param out_cost_weight 输出成本权重
 * @param out_perf_weight 输出性能权重
 * @param out_trust_weight 输出信任权重
 * @return agentrt_error_t
 */
agentrt_error_t agentrt_config_parse_weights(const char *config_json, float *out_cost_weight,
                                             float *out_perf_weight, float *out_trust_weight);

/* ================================================================
 * C-L01: Manager → CoreLoopThree 连接线 API（P1.1）
 * ================================================================ */

/**
 * @brief 从 agentrt.yaml 加载完整配置
 *
 * 整合 config_loader.c 的文件读取 + yaml_loader.c 的解析，
 * 自动应用环境变量覆盖。
 *
 * @param yaml_path agentrt.yaml 文件路径（NULL 使用默认路径 ./configs/agentrt.yaml）
 * @param config 输出配置
 * @return 0 成功，非0失败
 *
 * @ownership config: OWNER（调用者负责生命周期，通过 agentrt_yaml_config_free 释放）
 */
int agentrt_config_load_yaml(const char *yaml_path,
                             agentrt_yaml_config_t *config);

/**
 * @brief 获取全局配置实例
 *
 * CoreLoopThree 启动时加载一次，后续各模块通过此函数获取。
 *
 * @return 全局配置指针（只读，BORROW），NULL 表示未加载
 */
const agentrt_yaml_config_t *agentrt_config_get_global(void);

/**
 * @brief 配置热重载回调类型
 * @param old_config 旧配置（BORROW）
 * @param new_config 新配置（BORROW）
 * @param user_data  用户数据
 */
typedef void (*agentrt_config_reload_cb_t)(
    const agentrt_yaml_config_t *old_config,
    const agentrt_yaml_config_t *new_config,
    void *user_data);

/**
 * @brief 注册配置热重载回调
 * @param callback  回调函数
 * @param user_data 用户数据
 * @return 0 成功，非0失败
 */
int agentrt_config_on_reload(agentrt_config_reload_cb_t callback,
                             void *user_data);

/**
 * @brief 启动配置文件监听（热重载）
 *
 * 后台线程监听 agentrt.yaml 文件变更，
 * 变更时自动重新加载并调用所有注册的回调。
 *
 * @param yaml_path agentrt.yaml 文件路径（NULL 使用默认路径）
 * @param interval_ms 轮询间隔（毫秒），0 使用默认值 1000ms
 * @return 0 成功，非0失败
 */
int agentrt_config_watch_start(const char *yaml_path, uint32_t interval_ms);

/**
 * @brief 停止配置文件监听
 */
void agentrt_config_watch_stop(void);

/**
 * @brief 手动触发配置重载
 * @param yaml_path agentrt.yaml 文件路径（NULL 使用上次路径）
 * @return 0 成功，非0失败
 */
int agentrt_config_reload(const char *yaml_path);

/**
 * @brief CoreLoopThree 配置模块初始化（POSIX init）
 *
 * 在 agentrt_init() 之后调用，加载 agentrt.yaml 到全局配置。
 * 如果文件不存在，使用默认配置。
 *
 * @param yaml_path agentrt.yaml 文件路径（NULL 使用 ./configs/agentrt.yaml）
 * @return 0 成功，非0失败
 */
int agentrt_config_init(const char *yaml_path);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_CONFIG_LOADER_H */
