/**
 * @file manager_adapter.h
 * @brief C-L01: Manager → CoreLoopThree 连接桥梁
 *
 * 提供 Manager（Python 管理层）与 CoreLoopThree（C 核心引擎）之间的
 * 配置管理接口。Manager 通过此适配器加载/重载 agentrt.yaml 配置，
 * 并控制 CoreLoopThree 的运行时行为。
 *
 * 架构：
 *   Manager (Python) → manager_adapter (C) → config_loader → yaml_loader
 *                                          → loop (CoreLoopThree)
 *
 * 数据流：
 *   1. Manager 生成/修改 agentrt.yaml
 *   2. Manager 调用 manager_adapter_reload() 触发热重载
 *   3. config_loader 检测文件变更 → yaml_loader 解析
 *   4. 回调通知 CoreLoopThree 更新运行时配置
 *
 * @owner team-A
 * @see config_loader.h
 * @see yaml_loader.h
 * @see P1.1 C-L01 连接线
 */

#ifndef AGENTRT_CORELOOPTHREE_MANAGER_ADAPTER_H
#define AGENTRT_CORELOOPTHREE_MANAGER_ADAPTER_H

#include "agentrt.h"
#include "config_loader.h"
#include "loop.h"
#include "yaml_loader.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Manager 适配器句柄
 * ================================================================ */

typedef struct agentrt_manager_adapter_s agentrt_manager_adapter_t;

/* ================================================================
 * 生命周期
 * ================================================================ */

/**
 * @brief 初始化 Manager 适配器
 *
 * 加载 agentrt.yaml 配置，初始化全局配置和热重载系统。
 * 此函数应在 agentrt_init() 之后、agentrt_loop_create() 之前调用。
 *
 * @param yaml_path agentrt.yaml 路径（NULL 使用 ./configs/agentrt.yaml）
 * @param out_adapter 输出适配器句柄
 * @return 0 成功，非0 失败
 *
 * @ownership out_adapter: OWNER（通过 manager_adapter_shutdown 释放）
 */
int manager_adapter_init(const char *yaml_path,
                         agentrt_manager_adapter_t **out_adapter);

/**
 * @brief 关闭 Manager 适配器
 *
 * 停止配置监听，释放资源。
 *
 * @param adapter 适配器句柄
 *
 * @ownership adapter: TRANSFER
 */
void manager_adapter_shutdown(agentrt_manager_adapter_t *adapter);

/* ================================================================
 * 配置管理
 * ================================================================ */

/**
 * @brief 触发配置重载
 *
 * 重新加载 agentrt.yaml 并通知所有注册的回调。
 * 通常由 Manager 在修改 agentrt.yaml 后调用。
 *
 * @param adapter 适配器句柄
 * @return 0 成功，非0 失败
 */
int manager_adapter_reload(agentrt_manager_adapter_t *adapter);

/**
 * @brief 获取当前全局配置（只读）
 *
 * @param adapter 适配器句柄
 * @return 配置指针（BORROW），NULL 表示未加载
 */
const agentrt_yaml_config_t *manager_adapter_get_config(
    agentrt_manager_adapter_t *adapter);

/**
 * @brief 启动配置文件热重载监听
 *
 * 后台线程监控 agentrt.yaml 变更，自动触发重载。
 *
 * @param adapter 适配器句柄
 * @param interval_ms 轮询间隔（毫秒），0 使用默认 1000ms
 * @return 0 成功，非0 失败
 */
int manager_adapter_start_watch(agentrt_manager_adapter_t *adapter,
                                uint32_t interval_ms);

/**
 * @brief 停止配置文件热重载监听
 *
 * @param adapter 适配器句柄
 */
void manager_adapter_stop_watch(agentrt_manager_adapter_t *adapter);

/* ================================================================
 * YAML 配置 → Loop 配置转换
 * ================================================================ */

/**
 * @brief 从 YAML 配置构建 Loop 配置
 *
 * 将 agentrt_yaml_config_t 中的内核参数映射到 agentrt_loop_config_t。
 *
 * @param yaml_config YAML 配置（BORROW）
 * @param out_loop_config 输出 Loop 配置
 * @return 0 成功，非0 失败
 */
int manager_adapter_yaml_to_loop_config(
    const agentrt_yaml_config_t *yaml_config,
    agentrt_loop_config_t *out_loop_config);

/**
 * @brief 从 YAML 配置创建 CoreLoopThree 实例
 *
 * 便捷函数：加载 YAML 配置 → 转换为 Loop 配置 → 创建 Loop。
 *
 * @param adapter 适配器句柄
 * @param out_loop 输出 Loop 句柄
 * @return agentrt_error_t
 *
 * @ownership out_loop: OWNER
 */
agentrt_error_t manager_adapter_create_loop(
    agentrt_manager_adapter_t *adapter,
    agentrt_core_loop_t **out_loop);

/* ================================================================
 * 配置变更回调
 * ================================================================ */

/**
 * @brief Manager 适配器配置变更回调类型
 *
 * 当 agentrt.yaml 热重载时调用，通知上层配置已变更。
 *
 * @param old_config 旧配置（BORROW）
 * @param new_config 新配置（BORROW）
 * @param user_data  用户数据
 */
typedef void (*manager_adapter_config_change_cb_t)(
    const agentrt_yaml_config_t *old_config,
    const agentrt_yaml_config_t *new_config,
    void *user_data);

/**
 * @brief 注册配置变更回调
 *
 * @param adapter 适配器句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 * @return 0 成功，非0 失败
 */
int manager_adapter_on_config_change(
    agentrt_manager_adapter_t *adapter,
    manager_adapter_config_change_cb_t callback,
    void *user_data);

/* ================================================================
 * 状态查询
 * ================================================================ */

/**
 * @brief 获取适配器状态信息
 *
 * @param adapter 适配器句柄
 * @param out_config_loaded 输出配置是否已加载
 * @param out_watch_running 输出监听是否运行中
 * @param out_config_path 输出配置路径（BORROW，不可修改）
 * @return 0 成功
 */
int manager_adapter_get_status(agentrt_manager_adapter_t *adapter,
                               bool *out_config_loaded,
                               bool *out_watch_running,
                               const char **out_config_path);

/**
 * @brief C-L01: 输出 Manager 适配器统计摘要（单行格式，适合周期性日志）
 *
 * @param adapter 适配器句柄
 */
void manager_adapter_dump_stats(agentrt_manager_adapter_t *adapter);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_CORELOOPTHREE_MANAGER_ADAPTER_H */