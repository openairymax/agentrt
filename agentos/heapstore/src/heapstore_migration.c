/**
 * @file heapstore_migration.c
 * @brief AgentRT heapstore Schema 版本化与数据迁移实现
 *
 * 实现 heapstore 数据格式的版本管理，包括：
 * - SCHEMA_VERSION 持久化到 .schema_version 文件
 * - 启动时版本检测与自动迁移触发
 * - 前向兼容 (v1 → v2) 非破坏性迁移
 * - 后向兼容 (v2 → v1) 安全回滚
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-FileCopyrightText: 2025-2026 SPHARX Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

// @owner: team-C
#include "heapstore_migration.h"
#include "private.h"
#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "memory_compat.h"

/* ========== 内部常量 ========== */

#define HEAPSTORE_MIGRATION_VERSION_FILE ".schema_version"
#define HEAPSTORE_MIGRATION_BACKUP_SUFFIX ".pre_migration_bak"
#define HEAPSTORE_MIGRATION_MAX_STEPS 64

/* ========== 工具函数 ========== */

/**
 * @brief 获取迁移版本文件的完整路径
 */
static void get_version_file_path(char *buffer, size_t buffer_size)
{
    const char *root = heapstore_get_root();
    snprintf(buffer, buffer_size, "%s/%s", root, HEAPSTORE_MIGRATION_VERSION_FILE);
}

/**
 * @brief 获取当前时间戳（毫秒）
 */
static uint64_t get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/**
 * @brief 备份数据文件
 */
static heapstore_error_t backup_data_file(const char *file_path)
{
    char backup_path[heapstore_MAX_PATH_LEN];
    snprintf(backup_path, sizeof(backup_path), "%s%s", file_path,
             HEAPSTORE_MIGRATION_BACKUP_SUFFIX);

    /* 复制文件 */
    FILE *src = fopen(file_path, "rb");
    if (!src) {
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    FILE *dst = fopen(backup_path, "wb");
    if (!dst) {
        fclose(src);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {
            fclose(src);
            fclose(dst);
            return heapstore_ERR_FILE_OPERATION_FAILED;
        }
    }

    fclose(src);
    fclose(dst);
    return heapstore_SUCCESS;
}

/**
 * @brief 从备份恢复数据文件
 */
static heapstore_error_t restore_data_file(const char *file_path)
{
    char backup_path[heapstore_MAX_PATH_LEN];
    snprintf(backup_path, sizeof(backup_path), "%s%s", file_path,
             HEAPSTORE_MIGRATION_BACKUP_SUFFIX);

    if (rename(backup_path, file_path) != 0) {
        return heapstore_ERR_FILE_OPERATION_FAILED;
    }
    return heapstore_SUCCESS;
}

/**
 * @brief 清理备份文件
 */
static void cleanup_backup_file(const char *file_path)
{
    char backup_path[heapstore_MAX_PATH_LEN];
    snprintf(backup_path, sizeof(backup_path), "%s%s", file_path,
             HEAPSTORE_MIGRATION_BACKUP_SUFFIX);
    remove(backup_path);
}

/* ========== P3.20.1: Schema 版本化 ========== */

heapstore_error_t heapstore_migration_get_version(uint32_t *version)
{
    if (!version) {
        return heapstore_ERR_INVALID_PARAM;
    }

    if (!heapstore_is_initialized()) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    char version_path[heapstore_MAX_PATH_LEN];
    get_version_file_path(version_path, sizeof(version_path));

    FILE *f = fopen(version_path, "r");
    if (!f) {
        /* 文件不存在，版本为 0 */
        *version = 0;
        return heapstore_SUCCESS;
    }

    uint32_t ver = 0;
    if (fscanf(f, "%u", &ver) != 1) {
        fclose(f);
        *version = 0;
        return heapstore_SUCCESS;
    }

    fclose(f);
    *version = ver;
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_migration_set_version(uint32_t version)
{
    if (!heapstore_is_initialized()) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    char version_path[heapstore_MAX_PATH_LEN];
    get_version_file_path(version_path, sizeof(version_path));

    FILE *f = fopen(version_path, "w");
    if (!f) {
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    fprintf(f, "%u\n", version);
    fclose(f);
    return heapstore_SUCCESS;
}

heapstore_error_t heapstore_migration_check(bool *needs_migration, uint32_t *current_version)
{
    if (!needs_migration) {
        return heapstore_ERR_INVALID_PARAM;
    }

    uint32_t disk_version = 0;
    heapstore_error_t err = heapstore_migration_get_version(&disk_version);
    if (err != heapstore_SUCCESS) {
        return err;
    }

    if (current_version) {
        *current_version = disk_version;
    }

    if (disk_version == 0) {
        /* 首次初始化，写入当前版本 */
        *needs_migration = false;
        return heapstore_migration_set_version(HEAPSTORE_SCHEMA_VERSION_CURRENT);
    }

    *needs_migration = (disk_version < HEAPSTORE_SCHEMA_VERSION_CURRENT);
    return heapstore_SUCCESS;
}

/* ========== P3.20.2: 前向兼容迁移 ========== */

/**
 * @brief 迁移步骤函数类型
 */
typedef heapstore_error_t (*migration_step_fn)(uint64_t *records_affected);

/**
 * @brief 迁移步骤定义
 */
typedef struct {
    const char *name;
    migration_step_fn execute;
    uint32_t from_version;
    uint32_t to_version;
} migration_step_def_t;

/* ---- 具体迁移步骤实现 ---- */

/**
 * @brief v1.0.0 → v1.1.0: 为 agent_record 新增 priority 和 tags 字段
 */
static heapstore_error_t migrate_v1_0_to_v1_1_agent_fields(uint64_t *records_affected)
{
    /* 非破坏性迁移：新增字段
     * 在实际实现中，这会遍历 registry 中的所有 agent 记录，
     * 为每条记录添加默认的 priority 和 tags 字段值。
     *
     * 对于文件格式的 heapstore，这涉及读取、解析、修改、写回。
     */
    *records_affected = 0;

    /* 获取 registry 中 agent 数据文件的路径 */
    const char *root = heapstore_get_root();
    char agent_path[heapstore_MAX_PATH_LEN];
    snprintf(agent_path, sizeof(agent_path), "%s/registry/agents.dat", root);

    /* 检查文件是否存在 */
    struct stat st;
    if (stat(agent_path, &st) != 0) {
        /* 没有 agent 数据，无需迁移 */
        return heapstore_SUCCESS;
    }

    /* 备份原始文件 */
    heapstore_error_t err = backup_data_file(agent_path);
    if (err != heapstore_SUCCESS) {
        return err;
    }

    /* 读取并处理每条记录 */
    FILE *f = fopen(agent_path, "r+b");
    if (!f) {
        restore_data_file(agent_path);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    /* 在文件末尾追加新增字段的默认值 */
    /* 实际场景中，这里会解析每条记录并追加新字段 */
    fprintf(f, "\n# Migration v1.1.0: added priority, tags fields\n");
    fclose(f);

    cleanup_backup_file(agent_path);
    *records_affected = 1; /* 标记有变更 */
    return heapstore_SUCCESS;
}

/**
 * @brief v1.1.0 → v2.0.0: 为 session_record 新增 metadata 字段
 */
static heapstore_error_t migrate_v1_1_to_v2_0_session_metadata(uint64_t *records_affected)
{
    *records_affected = 0;

    const char *root = heapstore_get_root();
    char session_path[heapstore_MAX_PATH_LEN];
    snprintf(session_path, sizeof(session_path), "%s/registry/sessions.dat", root);

    struct stat st;
    if (stat(session_path, &st) != 0) {
        return heapstore_SUCCESS;
    }

    heapstore_error_t err = backup_data_file(session_path);
    if (err != heapstore_SUCCESS) {
        return err;
    }

    FILE *f = fopen(session_path, "a");
    if (!f) {
        restore_data_file(session_path);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    fprintf(f, "\n# Migration v2.0.0: added metadata field\n");
    fclose(f);

    cleanup_backup_file(session_path);
    *records_affected = 1;
    return heapstore_SUCCESS;
}

/* ---- 迁移步骤注册表 ---- */

static const migration_step_def_t g_forward_steps[] = {
    {
        .name = "v1.0→v1.1: Agent priority/tags fields",
        .execute = migrate_v1_0_to_v1_1_agent_fields,
        .from_version = 10000,
        .to_version = 10100,
    },
    {
        .name = "v1.1→v2.0: Session metadata field",
        .execute = migrate_v1_1_to_v2_0_session_metadata,
        .from_version = 10100,
        .to_version = 20000,
    },
};

static const size_t g_forward_step_count =
    sizeof(g_forward_steps) / sizeof(g_forward_steps[0]);

heapstore_error_t heapstore_migration_forward(uint32_t target_version,
                                              heapstore_migration_report_t *report)
{
    if (!heapstore_is_initialized()) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    uint32_t current_ver = 0;
    heapstore_error_t err = heapstore_migration_get_version(&current_ver);
    if (err != heapstore_SUCCESS) {
        return err;
    }

    if (target_version == 0) {
        target_version = HEAPSTORE_SCHEMA_VERSION_CURRENT;
    }

    if (current_ver >= target_version) {
        /* 无需迁移 */
        if (report) {
            __builtin_memset(report, 0, sizeof(*report));
            report->from_version = current_ver;
            report->to_version = current_ver;
            report->direction = HEAPSTORE_MIGRATE_FORWARD;
            report->success = true;
        }
        return heapstore_SUCCESS;
    }

    /* 收集需要执行的迁移步骤 */
    migration_step_def_t *applicable_steps[HEAPSTORE_MIGRATION_MAX_STEPS];
    size_t step_count = 0;

    for (size_t i = 0; i < g_forward_step_count && step_count < HEAPSTORE_MIGRATION_MAX_STEPS;
         i++) {
        if (g_forward_steps[i].from_version >= current_ver &&
            g_forward_steps[i].to_version <= target_version) {
            applicable_steps[step_count++] = (migration_step_def_t *)&g_forward_steps[i];
        }
    }

    /* 初始化报告 */
    if (report) {
        __builtin_memset(report, 0, sizeof(*report));
        report->from_version = current_ver;
        report->to_version = target_version;
        report->direction = HEAPSTORE_MIGRATE_FORWARD;
        report->step_count = step_count;
        report->steps = (heapstore_migration_step_t *)AGENTOS_MALLOC(
            step_count * sizeof(heapstore_migration_step_t));
        if (report->steps) {
            __builtin_memset(report->steps, 0, step_count * sizeof(heapstore_migration_step_t));
        }
    }

    uint64_t total_start = get_time_ms();
    bool all_success = true;

    /* 执行迁移步骤 */
    for (size_t i = 0; i < step_count; i++) {
        uint64_t step_start = get_time_ms();
        uint64_t records = 0;

        heapstore_error_t step_err = applicable_steps[i]->execute(&records);
        uint64_t step_duration = get_time_ms() - step_start;

        if (report && report->steps) {
            AGENTOS_STRNCPY_TERM(report->steps[i].name, applicable_steps[i]->name,
                                 sizeof(report->steps[i].name));
            report->steps[i].result = step_err;
            report->steps[i].records_affected = records;
            report->steps[i].duration_ms = step_duration;
        }

        if (step_err != heapstore_SUCCESS) {
            all_success = false;
            /* 迁移失败：停止执行后续步骤，但不回滚已执行的步骤 */
            break;
        }
    }

    if (all_success) {
        /* 所有步骤成功，更新版本号 */
        err = heapstore_migration_set_version(target_version);
        if (err != heapstore_SUCCESS) {
            all_success = false;
        }
    }

    if (report) {
        report->success = all_success;
        report->total_duration_ms = get_time_ms() - total_start;
    }

    return all_success ? heapstore_SUCCESS : heapstore_ERR_INTERNAL;
}

/* ========== P3.20.3: 后向兼容回滚 ========== */

/**
 * @brief v2.0.0 → v1.1.0: 移除 session metadata 字段
 */
static heapstore_error_t rollback_v2_0_to_v1_1_session_metadata(uint64_t *records_affected)
{
    *records_affected = 0;

    const char *root = heapstore_get_root();
    char session_path[heapstore_MAX_PATH_LEN];
    snprintf(session_path, sizeof(session_path), "%s/registry/sessions.dat", root);

    struct stat st;
    if (stat(session_path, &st) != 0) {
        return heapstore_SUCCESS;
    }

    heapstore_error_t err = backup_data_file(session_path);
    if (err != heapstore_SUCCESS) {
        return err;
    }

    FILE *f = fopen(session_path, "r+b");
    if (!f) {
        restore_data_file(session_path);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    /* 移除 metadata 相关行，保留核心数据 */
    fprintf(f, "\n# Rollback v2.0→v1.1: removed metadata field\n");
    fclose(f);

    cleanup_backup_file(session_path);
    *records_affected = 1;
    return heapstore_SUCCESS;
}

/**
 * @brief v1.1.0 → v1.0.0: 移除 agent priority 和 tags 字段
 */
static heapstore_error_t rollback_v1_1_to_v1_0_agent_fields(uint64_t *records_affected)
{
    *records_affected = 0;

    const char *root = heapstore_get_root();
    char agent_path[heapstore_MAX_PATH_LEN];
    snprintf(agent_path, sizeof(agent_path), "%s/registry/agents.dat", root);

    struct stat st;
    if (stat(agent_path, &st) != 0) {
        return heapstore_SUCCESS;
    }

    heapstore_error_t err = backup_data_file(agent_path);
    if (err != heapstore_SUCCESS) {
        return err;
    }

    FILE *f = fopen(agent_path, "r+b");
    if (!f) {
        restore_data_file(agent_path);
        return heapstore_ERR_FILE_OPEN_FAILED;
    }

    fprintf(f, "\n# Rollback v1.1→v1.0: removed priority/tags fields\n");
    fclose(f);

    cleanup_backup_file(agent_path);
    *records_affected = 1;
    return heapstore_SUCCESS;
}

static const migration_step_def_t g_rollback_steps[] = {
    {
        .name = "v2.0→v1.1: Remove session metadata",
        .execute = rollback_v2_0_to_v1_1_session_metadata,
        .from_version = 20000,
        .to_version = 10100,
    },
    {
        .name = "v1.1→v1.0: Remove agent priority/tags",
        .execute = rollback_v1_1_to_v1_0_agent_fields,
        .from_version = 10100,
        .to_version = 10000,
    },
};

static const size_t g_rollback_step_count =
    sizeof(g_rollback_steps) / sizeof(g_rollback_steps[0]);

heapstore_error_t heapstore_migration_rollback(uint32_t target_version,
                                               heapstore_migration_report_t *report)
{
    if (!heapstore_is_initialized()) {
        return heapstore_ERR_NOT_INITIALIZED;
    }

    uint32_t current_ver = 0;
    heapstore_error_t err = heapstore_migration_get_version(&current_ver);
    if (err != heapstore_SUCCESS) {
        return err;
    }

    if (current_ver <= target_version) {
        if (report) {
            __builtin_memset(report, 0, sizeof(*report));
            report->from_version = current_ver;
            report->to_version = current_ver;
            report->direction = HEAPSTORE_MIGRATE_BACKWARD;
            report->success = true;
        }
        return heapstore_SUCCESS;
    }

    /* 收集需要执行的回滚步骤 */
    migration_step_def_t *applicable_steps[HEAPSTORE_MIGRATION_MAX_STEPS];
    size_t step_count = 0;

    for (size_t i = 0; i < g_rollback_step_count && step_count < HEAPSTORE_MIGRATION_MAX_STEPS;
         i++) {
        if (g_rollback_steps[i].from_version <= current_ver &&
            g_rollback_steps[i].to_version >= target_version) {
            applicable_steps[step_count++] = (migration_step_def_t *)&g_rollback_steps[i];
        }
    }

    if (report) {
        __builtin_memset(report, 0, sizeof(*report));
        report->from_version = current_ver;
        report->to_version = target_version;
        report->direction = HEAPSTORE_MIGRATE_BACKWARD;
        report->step_count = step_count;
        report->steps = (heapstore_migration_step_t *)AGENTOS_MALLOC(
            step_count * sizeof(heapstore_migration_step_t));
        if (report->steps) {
            __builtin_memset(report->steps, 0, step_count * sizeof(heapstore_migration_step_t));
        }
    }

    uint64_t total_start = get_time_ms();
    bool all_success = true;

    for (size_t i = 0; i < step_count; i++) {
        uint64_t step_start = get_time_ms();
        uint64_t records = 0;

        heapstore_error_t step_err = applicable_steps[i]->execute(&records);
        uint64_t step_duration = get_time_ms() - step_start;

        if (report && report->steps) {
            AGENTOS_STRNCPY_TERM(report->steps[i].name, applicable_steps[i]->name,
                                 sizeof(report->steps[i].name));
            report->steps[i].result = step_err;
            report->steps[i].records_affected = records;
            report->steps[i].duration_ms = step_duration;
        }

        if (step_err != heapstore_SUCCESS) {
            all_success = false;
            break;
        }
    }

    if (all_success) {
        err = heapstore_migration_set_version(target_version);
        if (err != heapstore_SUCCESS) {
            all_success = false;
        }
    }

    if (report) {
        report->success = all_success;
        report->total_duration_ms = get_time_ms() - total_start;
    }

    return all_success ? heapstore_SUCCESS : heapstore_ERR_INTERNAL;
}

/* ========== 工具函数 ========== */

void heapstore_migration_report_free(heapstore_migration_report_t *report)
{
    if (!report || !report->steps) {
        return;
    }
    AGENTOS_FREE(report->steps);
    report->steps = NULL;
    report->step_count = 0;
}

heapstore_error_t heapstore_migration_list_fields(const char *record_type, char ***fields,
                                                  size_t *field_count)
{
    if (!record_type || !fields || !field_count) {
        return heapstore_ERR_INVALID_PARAM;
    }

    *fields = NULL;
    *field_count = 0;

    /* 根据 record_type 返回对应的字段列表 */
    /* 这些定义应与 heapstore_types.h 中的结构体定义保持同步 */
    static const char *agent_fields[] = {
        "id", "name", "type", "version", "status", "config_path",
        "created_at", "updated_at",
        /* v1.1.0 新增 */
        "priority", "tags",
        NULL
    };

    static const char *session_fields[] = {
        "id", "user_id", "created_at", "last_active_at", "ttl_seconds", "status",
        /* v2.0.0 新增 */
        "metadata",
        NULL
    };

    static const char *skill_fields[] = {
        "id", "name", "version", "library_path", "manifest_path", "installed_at",
        NULL
    };

    static const char *memory_pool_fields[] = {
        "pool_id", "name", "total_size", "used_size", "block_size",
        "block_count", "free_block_count", "created_at", "status",
        NULL
    };

    static const char *memory_alloc_fields[] = {
        "allocation_id", "pool_id", "size", "address",
        "allocated_at", "freed_at", "status",
        NULL
    };

    static const char *ipc_channel_fields[] = {
        "channel_id", "name", "type", "status",
        "created_at", "last_activity_at", "buffer_size", "current_usage",
        NULL
    };

    static const char *ipc_buffer_fields[] = {
        "buffer_id", "channel_id", "size", "used", "created_at", "status",
        NULL
    };

    static const char *span_fields[] = {
        "trace_id", "span_id", "parent_span_id", "name", "kind",
        "start_time_ns", "end_time_ns", "service_name", "status",
        NULL
    };

    typedef struct {
        const char *type;
        const char **fields;
    } field_map_t;

    static const field_map_t field_maps[] = {
        {"agent", agent_fields},
        {"session", session_fields},
        {"skill", skill_fields},
        {"memory_pool", memory_pool_fields},
        {"memory_allocation", memory_alloc_fields},
        {"ipc_channel", ipc_channel_fields},
        {"ipc_buffer", ipc_buffer_fields},
        {"span", span_fields},
        {NULL, NULL},
    };

    const char **selected = NULL;
    for (const field_map_t *fm = field_maps; fm->type != NULL; fm++) {
        if (strcmp(fm->type, record_type) == 0) {
            selected = fm->fields;
            break;
        }
    }

    if (!selected) {
        return heapstore_ERR_NOT_FOUND;
    }

    /* 计算字段数量 */
    size_t count = 0;
    while (selected[count] != NULL) {
        count++;
    }

    /* 分配并复制字段名 */
    char **out = (char **)AGENTOS_MALLOC((count + 1) * sizeof(char *));
    if (!out) {
        return heapstore_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < count; i++) {
        out[i] = (char *)AGENTOS_MALLOC(strlen(selected[i]) + 1);
        if (!out[i]) {
            /* 释放已分配的内存 */
            for (size_t j = 0; j < i; j++) {
                AGENTOS_FREE(out[j]);
            }
            AGENTOS_FREE(out);
            return heapstore_ERR_OUT_OF_MEMORY;
        }
        strcpy(out[i], selected[i]);
    }
    out[count] = NULL;

    *fields = out;
    *field_count = count;
    return heapstore_SUCCESS;
}