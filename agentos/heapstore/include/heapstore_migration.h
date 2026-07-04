/**
 * @file heapstore_migration.h
 * @brief AgentRT heapstore Schema 版本化与数据迁移接口
 *
 * 提供 heapstore 数据格式的版本管理能力：
 * - SCHEMA_VERSION 持久化到 heapstore 数据目录
 * - 启动时自动检测版本差异
 * - 前向兼容 (v1 → v2) 非破坏性迁移
 * - 后向兼容 (v2 → v1) 回滚（保留核心数据）
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

// @owner: team-C
#ifndef AGENTOS_HEAPSTORE_MIGRATION_H
#define AGENTOS_HEAPSTORE_MIGRATION_H

#include "heapstore.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 当前 heapstore 数据格式 Schema 版本
 *
 * 版本号规则：MAJOR * 10000 + MINOR * 100 + PATCH
 * v1.0.0 = 10000, v1.1.0 = 10100, v2.0.0 = 20000
 */
#define HEAPSTORE_SCHEMA_VERSION_CURRENT 10000 /* v1.0.0 */

/**
 * @brief 迁移方向
 */
typedef enum {
    HEAPSTORE_MIGRATE_FORWARD = 0,  /* 前向兼容迁移 (v1 → v2) */
    HEAPSTORE_MIGRATE_BACKWARD      /* 后向兼容回滚 (v2 → v1) */
} heapstore_migration_direction_t;

/**
 * @brief 迁移步骤结果
 */
typedef struct heapstore_migration_step {
    char name[128];             /* 步骤名称 */
    heapstore_error_t result;   /* 执行结果 */
    uint64_t records_affected;  /* 影响的记录数 */
    uint64_t duration_ms;       /* 耗时（毫秒） */
} heapstore_migration_step_t;

/**
 * @brief 迁移报告
 */
typedef struct heapstore_migration_report {
    uint32_t from_version;              /* 源版本 */
    uint32_t to_version;                /* 目标版本 */
    heapstore_migration_direction_t direction;  /* 迁移方向 */
    size_t step_count;                  /* 步骤总数 */
    heapstore_migration_step_t *steps;  /* 步骤详情数组 */
    bool success;                       /* 是否全部成功 */
    uint64_t total_duration_ms;         /* 总耗时 */
    uint64_t total_records_affected;    /* 总影响记录数 */
} heapstore_migration_report_t;

/**
 * @brief 从 heapstore 数据目录读取当前 Schema 版本
 *
 * 版本号存储在 heapstore 根目录下的 .schema_version 文件中。
 * 若文件不存在，视为版本 0（未初始化）。
 *
 * @param version [out] 输出当前 Schema 版本号
 * @return heapstore_error_t 错误码
 *
 * @ownership version: BORROW (caller-owned buffer, function writes to it)
 * @threadsafe 否（应在 heapstore_init 早期调用）
 * @reentrant 否
 *
 * @note 未初始化时返回 heapstore_ERR_NOT_INITIALIZED
 * @since v1.0.0
 */
heapstore_error_t heapstore_migration_get_version(uint32_t *version);

/**
 * @brief 将当前 Schema 版本写入 heapstore 数据目录
 *
 * @param version [in] 要写入的版本号
 * @return heapstore_error_t 错误码
 *
 * @threadsafe 否
 * @reentrant 否
 *
 * @note 通常在迁移完成后调用
 * @since v1.0.0
 */
heapstore_error_t heapstore_migration_set_version(uint32_t version);

/**
 * @brief 检测是否需要迁移
 *
 * 比较堆存储中的版本与编译期当前版本。
 *
 * @param needs_migration [out] 是否需要迁移
 * @param current_version [out] 存储中的当前版本（可为 NULL）
 * @return heapstore_error_t 错误码
 *
 * @ownership needs_migration: BORROW, current_version: BORROW (may be NULL)
 * @threadsafe 否
 * @reentrant 否
 *
 * @since v1.0.0
 */
heapstore_error_t heapstore_migration_check(bool *needs_migration, uint32_t *current_version);

/**
 * @brief 执行前向兼容迁移 (v_from → v_to)
 *
 * 非破坏性操作：新增字段、扩展数据结构。
 * 迁移过程中原始数据保留，仅在迁移成功后才更新版本号。
 *
 * @param target_version [in] 目标版本号（0 表示升级到最新版本）
 * @param report [out] 迁移报告（可为 NULL）
 * @return heapstore_error_t 错误码
 *
 * @ownership report: BORROW (caller-owned buffer, function writes to it, may be NULL)
 * @threadsafe 否
 * @reentrant 否
 *
 * @note 迁移失败时不会修改版本号，保证数据一致性
 * @since v1.0.0
 */
heapstore_error_t heapstore_migration_forward(uint32_t target_version,
                                              heapstore_migration_report_t *report);

/**
 * @brief 执行后向兼容回滚 (v_to → v_from)
 *
 * 丢弃新字段，保留核心数据。
 * 需要显式确认（本 API 不提供 --force，由调用方决定）。
 *
 * @param target_version [in] 目标版本号
 * @param report [out] 回滚报告（可为 NULL）
 * @return heapstore_error_t 错误码
 *
 * @ownership report: BORROW (caller-owned buffer, function writes to it, may be NULL)
 * @threadsafe 否
 * @reentrant 否
 *
 * @warning 回滚会丢弃新版本中新增的字段数据
 * @since v1.0.0
 */
heapstore_error_t heapstore_migration_rollback(uint32_t target_version,
                                               heapstore_migration_report_t *report);

/**
 * @brief 释放迁移报告中的动态分配内存
 *
 * @param report [in] 要释放的迁移报告
 *
 * @ownership report: TRANSFER (function takes ownership of internal allocations)
 * @threadsafe 是
 * @reentrant 是
 *
 * @since v1.0.0
 */
void heapstore_migration_report_free(heapstore_migration_report_t *report);

/**
 * @brief 列出现有数据格式中所有 record 类型的字段
 *
 * 用于迁移脚本了解当前 Schema 结构。
 *
 * @param record_type [in] 记录类型名称（如 "agent", "session", "skill" 等）
 * @param fields [out] 输出字段名数组（以 NULL 结尾），调用方需释放
 * @param field_count [out] 字段数量
 * @return heapstore_error_t 错误码
 *
 * @ownership fields: OWNER (caller must free each string and the array)
 * @threadsafe 是
 * @reentrant 是
 *
 * @since v1.0.0
 */
heapstore_error_t heapstore_migration_list_fields(const char *record_type, char ***fields,
                                                  size_t *field_count);

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_HEAPSTORE_MIGRATION_H */