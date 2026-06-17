/*
 * Copyright (C) 2026 SPHARX. All Rights Reserved.
 * SPDX-FileCopyrightText: 2026 SPHARX.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file memoryrovol_bridge.h
 * @brief C-L12: CoreLoopThree → MemoryRovol 提供商桥接头文件
 *
 * 实现三种构建模式下的内存提供商选择：
 * - AGENTOS_MEMORY_BUILTIN: 仅内置免费提供商
 * - AGENTOS_MEMORY_MEMORYROVOL: 仅 MemoryRovol 商业提供商
 * - AGENTOS_MEMORY_HYBRID: 根据 agentos.yaml memory.provider 配置切换
 */

#ifndef AGENTOS_MEMORYROVOL_BRIDGE_H
#define AGENTOS_MEMORYROVOL_BRIDGE_H

#include "memory_provider.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 构建模式 ==================== */

/** 构建模式：仅内置提供商 */
#ifndef AGENTOS_MEMORY_BUILTIN
#define AGENTOS_MEMORY_BUILTIN 0
#endif

/** 构建模式：仅 MemoryRovol */
#ifndef AGENTOS_MEMORY_MEMORYROVOL
#define AGENTOS_MEMORY_MEMORYROVOL 0
#endif

/** 构建模式：混合模式（根据配置切换） */
#ifndef AGENTOS_MEMORY_HYBRID
#define AGENTOS_MEMORY_HYBRID 0
#endif

/* ==================== 提供商类型 ==================== */

typedef enum {
    MRB_PROVIDER_BUILTIN = 0,    /**< 内置免费提供商 */
    MRB_PROVIDER_MEMORYROVOL,    /**< MemoryRovol 商业提供商 */
    MRB_PROVIDER_AUTO            /**< 自动选择（根据配置） */
} mrb_provider_type_t;

/* ==================== 桥接配置 ==================== */

typedef struct {
    /** 提供商类型 */
    mrb_provider_type_t provider_type;

    /** 存储路径 */
    const char *storage_path;

    /** MemoryRovol 配置文件路径 */
    const char *memoryrovol_config_path;

    /** 是否启用同步（HYBRID模式下内置⇔Rovol双向同步） */
    bool enable_sync;

    /** 同步间隔（毫秒） */
    uint32_t sync_interval_ms;
} mrb_config_t;

/** 默认桥接配置 */
#define MRB_CONFIG_DEFAULTS                                                    \
    {                                                                          \
        .provider_type = MRB_PROVIDER_AUTO, .storage_path = NULL,             \
        .memoryrovol_config_path = NULL, .enable_sync = false,                \
        .sync_interval_ms = 5000                                              \
    }

/* ==================== 桥接句柄 ==================== */

typedef struct mrb_bridge_s mrb_bridge_t;

/* ==================== 生命周期 ==================== */

/**
 * @brief C-L12: 创建 MemoryRovol 桥接
 *
 * 根据构建模式和配置创建相应的内存提供商。
 *
 * @param config 桥接配置（NULL 使用默认）
 * @return 桥接句柄，失败返回 NULL
 */
mrb_bridge_t *mrb_bridge_create(const mrb_config_t *config);

/**
 * @brief C-L12: 销毁桥接
 * @param bridge 桥接句柄
 */
void mrb_bridge_destroy(mrb_bridge_t *bridge);

/**
 * @brief C-L12: 获取当前活跃的内存提供商
 * @param bridge 桥接句柄
 * @return 内存提供商句柄，失败返回 NULL
 */
agentos_memory_provider_t *mrb_bridge_get_provider(mrb_bridge_t *bridge);

/**
 * @brief C-L12: 获取内置提供商（HYBRID 模式下用于同步）
 * @param bridge 桥接句柄
 * @return 内置提供商句柄，HYBRID 模式外返回 NULL
 */
agentos_memory_provider_t *mrb_bridge_get_builtin(mrb_bridge_t *bridge);

/* ==================== 提供商切换 ==================== */

/**
 * @brief C-L12.2: 切换活跃提供商
 *
 * 在 HYBRID 模式下根据 agentos.yaml 的 memory.provider 配置
 * 在内置提供商和 MemoryRovol 之间切换。
 *
 * @param bridge 桥接句柄
 * @param type 目标提供商类型
 * @return 0 成功，非 0 失败
 */
int mrb_bridge_switch_provider(mrb_bridge_t *bridge, mrb_provider_type_t type);

/**
 * @brief C-L12: 根据 agentos.yaml 配置自动选择提供商
 *
 * 解析 memory.provider 字段值：
 *   "builtin"      → MRB_PROVIDER_BUILTIN
 *   "memoryrovol"  → MRB_PROVIDER_MEMORYROVOL
 *   "auto" / 其他  → MRB_PROVIDER_AUTO（HYBRID模式优先MemoryRovol）
 *
 * @param bridge 桥接句柄
 * @param config_yaml agentos.yaml 中 memory 段的 JSON 表示
 * @return 0 成功，非 0 失败
 */
int mrb_bridge_select_from_config(mrb_bridge_t *bridge, const char *config_yaml);

/* ==================== 状态查询 ==================== */

/**
 * @brief 获取当前活跃的提供商类型
 */
mrb_provider_type_t mrb_bridge_get_active_type(mrb_bridge_t *bridge);

/**
 * @brief 检查 MemoryRovol 是否可用
 */
bool mrb_bridge_is_memoryrovol_available(mrb_bridge_t *bridge);

/**
 * @brief 检查桥接是否健康
 */
bool mrb_bridge_is_healthy(mrb_bridge_t *bridge);

/**
 * @brief 获取提供商统计信息
 */
int mrb_bridge_get_stats(mrb_bridge_t *bridge, agentos_memory_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORYROVOL_BRIDGE_H */