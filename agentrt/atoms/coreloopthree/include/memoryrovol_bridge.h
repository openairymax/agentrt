/**
 * @file memoryrovol_bridge.h
 * @brief C-L12: CoreLoopThree → MemoryRovol 提供商桥接
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 将 MemoryRovol 商业记忆引擎桥接到 AgentRT 的
 * agentrt_memory_provider_t 可拔插架构。
 *
 * 三种构建模式：
 *   AGENTRT_MEMORY_BUILTIN     → 内置免费提供商
 *   AGENTRT_MEMORY_MEMORYROVOL → MemoryRovol 商业提供商
 *   AGENTRT_MEMORY_HYBRID      → 混合模式（内置 + MemoryRovol 同步）
 *
 * 使用方式：
 * @code
 *   // 初始化 MemoryRovol 提供商
 *   memoryrovol_bridge_t *bridge = memoryrovol_bridge_create(&config);
 *
 *   // 获取 agentrt_memory_provider_t 接口
 *   agentrt_memory_provider_t *provider = memoryrovol_bridge_get_provider(bridge);
 *
 *   // 注册为活跃提供商
 *   agentrt_memory_provider_set_active(provider);
 *
 *   // 在 CoreLoopThree 中使用
 *   agentrt_memory_provider_t *active = agentrt_memory_provider_get_active();
 *   active->write_raw(active, data, len, NULL, &record_id);
 *   active->query(active, "query text", 10, &ids, &scores, &count);
 *
 *   // 关闭
 *   memoryrovol_bridge_destroy(bridge);
 * @endcode
 *
 * @see memory_provider.h
 * @see memoryrovol.h
 * @see P1.11 C-L12 连接线
 */

#ifndef AGENTRT_CORELOOPTHREE_MEMORYROVOL_BRIDGE_H
#define AGENTRT_CORELOOPTHREE_MEMORYROVOL_BRIDGE_H

#include "src/memory_provider.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 提供商类型枚举 ==================== */

typedef enum {
    MRB_PROVIDER_BUILTIN = 0,
    MRB_PROVIDER_MEMORYROVOL = 1,
    MRB_PROVIDER_HYBRID = 2,
    MRB_PROVIDER_AUTO = 3
} mrb_provider_type_t;

/* ==================== 内部配置类型 ==================== */

typedef struct {
    const char *memoryrovol_config_path;  /**< MemoryRovol 配置文件路径 */
    const char *storage_path;             /**< 存储路径 */
    mrb_provider_type_t provider_type;    /**< 提供商类型 */
    bool enable_sync;                     /**< 是否启用同步（HYBRID模式） */
    uint32_t sync_interval_ms;            /**< 同步间隔 */
} mrb_config_t;

#define MRB_CONFIG_DEFAULTS { NULL, NULL, MRB_PROVIDER_BUILTIN, false, 5000 }

/* ==================== 桥接器句柄 ==================== */

typedef struct memoryrovol_bridge_s memoryrovol_bridge_t;

/* 内部实现别名，用于 .c 文件中 */
typedef struct memoryrovol_bridge_s mrb_bridge_s;
typedef memoryrovol_bridge_t mrb_bridge_t;

/* ==================== 桥接器配置 ==================== */

typedef struct {
    const char *config_path;          /**< MemoryRovol 配置文件路径 */
    const char *storage_path;         /**< 存储路径 */
    const char *provider_name;        /**< 提供商名称 */
    const char *provider_version;     /**< 提供商版本 */
    bool enable_l1_raw;               /**< 启用 L1 原始存储 */
    bool enable_l2_feature;           /**< 启用 L2 特征提取 */
    bool enable_l3_structure;         /**< 启用 L3 结构绑定 */
    bool enable_l4_pattern;           /**< 启用 L4 模式识别 */
    bool enable_forgetting;           /**< 启用遗忘引擎 */
    bool enable_attractor;            /**< 启用吸引子网络 */
    bool enable_persistence;          /**< 启用持久同调 */
    bool enable_faiss;                /**< 启用 FAISS 加速 */
    bool enable_async_ops;            /**< 启用异步操作 */
    bool enable_llm_integration;      /**< 启用 LLM 集成 */
    uint32_t query_default_limit;     /**< 默认查询结果数 */
    uint32_t sync_interval_ms;        /**< 同步间隔（毫秒） */
} memoryrovol_bridge_config_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief 创建 MemoryRovol 桥接器
 *
 * 初始化 MemoryRovol 引擎并填充 agentrt_memory_provider_t 函数指针表。
 * 根据 config 中的能力标记决定启用哪些 L1-L4 层。
 *
 * @param config 配置（NULL 使用默认）
 * @return 桥接器句柄，失败返回 NULL
 */
memoryrovol_bridge_t *memoryrovol_bridge_create(
    const memoryrovol_bridge_config_t *config);

/**
 * @brief 销毁 MemoryRovol 桥接器
 *
 * 释放 MemoryRovol 引擎和所有资源。
 *
 * @param bridge 桥接器句柄
 */
void memoryrovol_bridge_destroy(memoryrovol_bridge_t *bridge);

/* ==================== 提供商接口 ==================== */

/**
 * @brief C-L12: 获取 MemoryRovol 的 agentrt_memory_provider_t 接口
 *
 * 返回填充了 MemoryRovol 函数指针的 provider 结构体。
 * 调用者不持有所有权，生命周期与 bridge 绑定。
 *
 * @param bridge 桥接器句柄
 * @return provider 指针，失败返回 NULL
 */
agentrt_memory_provider_t *memoryrovol_bridge_get_provider(
    memoryrovol_bridge_t *bridge);

/* ==================== 提供商切换 ==================== */

/**
 * @brief 切换提供商模式
 *
 * 根据 agentrt.yaml 的 memory.provider 配置选择：
 *   - "builtin"   → 内置提供商
 *   - "memoryrovol" → MemoryRovol 提供商
 *   - "hybrid"    → 混合模式
 *
 * @param bridge 桥接器句柄
 * @param mode 模式名称
 * @return 0 成功，非0 失败
 */
int memoryrovol_bridge_switch_mode(memoryrovol_bridge_t *bridge,
                                   const char *mode);

/**
 * @brief 获取当前提供商模式
 *
 * @param bridge 桥接器句柄
 * @return 模式名称（"builtin"/"memoryrovol"/"hybrid"），失败返回 NULL
 */
const char *memoryrovol_bridge_get_mode(memoryrovol_bridge_t *bridge);

/* ==================== 同步控制 ==================== */

/**
 * @brief 启动内置提供商与 MemoryRovol 之间的同步
 *
 * 仅在混合模式下有效。
 *
 * @param bridge 桥接器句柄
 * @return 0 成功，非0 失败
 */
int memoryrovol_bridge_start_sync(memoryrovol_bridge_t *bridge);

/**
 * @brief 停止同步
 *
 * @param bridge 桥接器句柄
 */
void memoryrovol_bridge_stop_sync(memoryrovol_bridge_t *bridge);

/**
 * @brief 检查是否有活跃的同步
 *
 * @param bridge 桥接器句柄
 * @return true 有活跃同步
 */
bool memoryrovol_bridge_has_active_sync(memoryrovol_bridge_t *bridge);

/* ==================== 状态查询 ==================== */

/**
 * @brief 获取桥接器统计信息
 *
 * @param bridge 桥接器句柄
 * @param out_stats 输出统计信息
 * @return 0 成功，非0 失败
 */
int memoryrovol_bridge_get_stats(memoryrovol_bridge_t *bridge,
                                 agentrt_memory_stats_t *out_stats);

/**
 * @brief 健康检查
 *
 * @param bridge 桥接器句柄
 * @param out_json 输出 JSON 格式健康报告
 * @return 0 成功，非0 失败
 */
int memoryrovol_bridge_health_check(memoryrovol_bridge_t *bridge,
                                    char **out_json);

/**
 * @brief 检查桥接器是否就绪
 *
 * @param bridge 桥接器句柄
 * @return true 就绪
 */
bool memoryrovol_bridge_is_ready(memoryrovol_bridge_t *bridge);

/**
 * @brief C-L12: 输出桥接器统计摘要（单行格式，适合周期性日志）
 *
 * 格式: "C-L12: BRIDGE-STATS mode=X reads=N writes=N queries=N "
 *        "errors=N deletes=N evolves=N forgets=N "
 *        "write_latency=avg/max/min us read_latency=avg/max/min us "
 *        "query_latency=avg/max/min us bytes_written=N bytes_read=N "
 *        "query_results=N"
 *
 * @param bridge 桥接器句柄
 */
void memoryrovol_bridge_dump_stats(memoryrovol_bridge_t *bridge);

#ifdef __cplusplus
}
#endif

#endif /* AGENTRT_CORELOOPTHREE_MEMORYROVOL_BRIDGE_H */