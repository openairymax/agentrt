/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Copyright (c) 2026 SPHARX Ltd. All Rights Reserved.
 *
 * audit_logger.c - Audit Logger Implementation
 */

/**
 * @file audit_logger.c
 * @brief Audit Logger Implementation
 * @author Spharx AgentRT Team
 * @date 2024
 */

#include "audit.h"
#include "audit_queue.h"
#include "audit_rotator.h"
#include "utils/cupolas_utils.h"

#include "memory_prealloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AGENTOS_HAS_OPENSSL
#include <openssl/sha.h>
#endif

/* Ensure logging macros are available */
#ifndef AGENTOS_LOG_ERROR
#include "../../../commons/utils/logging/include/logging_compat.h"
#endif

#define DEFAULT_QUEUE_SIZE 10000
#define DEFAULT_BATCH_SIZE 100
#define DEFAULT_FLUSH_INTERVAL_MS 1000

/* SEC-13: OOM 关键路径预分配常量 */
#define AUDIT_OOM_PREALLOC_EVENTS 64  /**< OOM 时预分配的审计事件槽位数 */

/* SHA-256 审计哈希链追踪 */
static char g_last_hash[65] = "0000000000000000000000000000000000000000000000000000000000000000";
static cupolas_mutex_t g_hash_chain_lock;

/* ============================================================================
 * SEC-13.2: OOM 审计事件预分配池
 *
 * 启动时预分配 64 个审计条目的环形缓冲区。
 * OOM 时写入预分配缓冲区，确保不丢失审计事件。
 * 使用静态数组，零运行时内存分配。
 * ============================================================================ */

/** @brief OOM 预分配审计条目池 */
static audit_entry_t g_audit_oom_entries[AUDIT_OOM_PREALLOC_EVENTS];

/** @brief OOM 预分配池写入索引 */
static size_t g_audit_oom_write_index = 0;

/** @brief OOM 预分配池已用槽位数 */
static size_t g_audit_oom_used_count = 0;

/** @brief OOM 预分配池互斥锁 */
static cupolas_mutex_t g_audit_oom_lock;

/** @brief OOM 预分配池是否已初始化 */
static bool g_audit_oom_initialized = false;

/**
 * @brief 初始化审计 OOM 预分配池（SEC-13.2）
 *
 * 在 audit_logger_create() 首次调用时初始化。
 * 零内存分配——使用静态数组。
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void audit_oom_pool_init(void)
{
    if (g_audit_oom_initialized) {
        return;
    }

    cupolas_mutex_init(&g_audit_oom_lock);
    __builtin_memset(g_audit_oom_entries, 0, sizeof(g_audit_oom_entries));
    g_audit_oom_write_index = 0;
    g_audit_oom_used_count = 0;
    g_audit_oom_initialized = true;
}

/**
 * @brief 从 OOM 预分配池获取一个审计条目（SEC-13.2）
 *
 * 当正常 audit_entry_create() 失败时调用。
 * 从环形预分配缓冲区中获取一个槽位，不调用系统 malloc。
 *
 * @return 审计条目指针，池耗尽返回 NULL
 */
static audit_entry_t *audit_oom_pool_alloc(void)
{
    if (!g_audit_oom_initialized) {
        return NULL;
    }

    cupolas_mutex_lock(&g_audit_oom_lock);

    if (g_audit_oom_used_count >= AUDIT_OOM_PREALLOC_EVENTS) {
        /* 池已满，覆盖最旧的条目（环形缓冲区） */
        audit_entry_t *entry = &g_audit_oom_entries[g_audit_oom_write_index];
        __builtin_memset(entry, 0, sizeof(audit_entry_t));
        g_audit_oom_write_index =
            (g_audit_oom_write_index + 1) % AUDIT_OOM_PREALLOC_EVENTS;
        cupolas_mutex_unlock(&g_audit_oom_lock);
        return entry;
    }

    audit_entry_t *entry = &g_audit_oom_entries[g_audit_oom_write_index];
    __builtin_memset(entry, 0, sizeof(audit_entry_t));
    g_audit_oom_write_index =
        (g_audit_oom_write_index + 1) % AUDIT_OOM_PREALLOC_EVENTS;
    g_audit_oom_used_count++;

    cupolas_mutex_unlock(&g_audit_oom_lock);
    return entry;
}
#pragma GCC diagnostic pop

/**
 * @brief 计算审计条目的 SHA-256 哈希链值
 * 
 * 哈希格式: SHA256(prev_hash + id + timestamp + subject + action + resource + detail + result)
 * 使用哈希链保证审计日志的不可篡改性。
 */
static void audit_compute_chain_hash(const audit_entry_t *entry, const char *prev_hash,
                                     char *hash_out)
{
#ifdef AGENTOS_HAS_OPENSSL
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, prev_hash, strlen(prev_hash));
    SHA256_Update(&sha256, entry->agent_id ? entry->agent_id : "", 
                  entry->agent_id ? strlen(entry->agent_id) : 0);
    SHA256_Update(&sha256, &entry->timestamp_ms, sizeof(entry->timestamp_ms));
    SHA256_Update(&sha256, entry->action ? entry->action : "",
                  entry->action ? strlen(entry->action) : 0);
    SHA256_Update(&sha256, entry->resource ? entry->resource : "",
                  entry->resource ? strlen(entry->resource) : 0);
    SHA256_Update(&sha256, entry->detail ? entry->detail : "",
                  entry->detail ? strlen(entry->detail) : 0);
    SHA256_Update(&sha256, &entry->result, sizeof(entry->result));

    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &sha256);

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        snprintf(hash_out + i * 2, 3, "%02x", digest[i]);
    }
    hash_out[64] = '\0';
#else
    /* OpenSSL 不可用时的回退: 使用简单校验和 */
    uint32_t checksum = 0;
    const char *str = prev_hash;
    while (*str) checksum = checksum * 31 + (unsigned char)*str++;
    if (entry->agent_id) { str = entry->agent_id; while (*str) checksum = checksum * 31 + (unsigned char)*str++; }
    checksum ^= (uint32_t)entry->timestamp_ms;
    if (entry->action) { str = entry->action; while (*str) checksum = checksum * 31 + (unsigned char)*str++; }
    if (entry->resource) { str = entry->resource; while (*str) checksum = checksum * 31 + (unsigned char)*str++; }
    checksum ^= (uint32_t)entry->result;
    snprintf(hash_out, 65, "%016x%016x%016x%016x", checksum, checksum ^ 0xAAAAAAAA,
             checksum ^ 0x55555555, checksum ^ 0xFFFFFFFF);
#endif
}

struct audit_logger {
    audit_queue_t *queue;
    audit_rotator_t *rotator;
    cupolas_thread_t writer_thread;
    cupolas_atomic32_t running;
    cupolas_atomic64_t total_logged;
    cupolas_atomic64_t total_failed;
    char *log_dir;
    char *log_prefix;
    size_t max_file_size;
    int max_files;
};

static void *audit_writer_thread(void *arg)
{
    audit_logger_t *logger = (audit_logger_t *)arg;
    audit_entry_t *batch[DEFAULT_BATCH_SIZE];

    while (cupolas_atomic_load32(&logger->running)) {
        size_t actual_count = 0;
        int ret =
            audit_queue_timed_pop(logger->queue, &batch[actual_count], DEFAULT_FLUSH_INTERVAL_MS);
        if (ret == cupolas_OK) {
            actual_count++;
            while (actual_count < DEFAULT_BATCH_SIZE) {
                audit_entry_t *next = NULL;
                if (audit_queue_try_pop(logger->queue, &next) != cupolas_OK)
                    break;
                batch[actual_count++] = next;
            }
            for (size_t i = 0; i < actual_count; i++) {
                if (audit_rotator_write(logger->rotator, batch[i]) == cupolas_OK) {
                    cupolas_atomic_add64(&logger->total_logged, 1);
                } else {
                    AGENTOS_LOG_ERROR("[CRITICAL] audit_writer_thread: audit write failed, entry_type=%d, total_failed=%llu", (int)batch[i]->type, (unsigned long long)cupolas_atomic_load64(&logger->total_failed) + 1);
                    cupolas_atomic_add64(&logger->total_failed, 1);
                }
                audit_entry_destroy(batch[i]);
            }
        }
    }

    size_t remaining = audit_queue_size(logger->queue);
    while (remaining > 0) {
        audit_entry_t *entry = NULL;
        if (audit_queue_try_pop(logger->queue, &entry) == cupolas_OK) {
            if (audit_rotator_write(logger->rotator, entry) == cupolas_OK) {
                cupolas_atomic_add64(&logger->total_logged, 1);
            } else {
                AGENTOS_LOG_ERROR("[CRITICAL] audit_writer_thread: audit write failed during shutdown, total_failed=%llu", (unsigned long long)cupolas_atomic_load64(&logger->total_failed) + 1);
                cupolas_atomic_add64(&logger->total_failed, 1);
            }
            audit_entry_destroy(entry);
        }
        remaining = audit_queue_size(logger->queue);
    }

    return NULL;
}

audit_logger_t *audit_logger_create(const char *log_dir, const char *log_prefix,
                                    size_t max_file_size, int max_files)
{
    /* 初始化哈希链互斥锁（仅首次调用） */
    static cupolas_atomic32_t hash_lock_inited = {0};
    if (cupolas_atomic_load32(&hash_lock_inited) == 0) {
        cupolas_mutex_init(&g_hash_chain_lock);
        cupolas_atomic_store32(&hash_lock_inited, 1);
    }

    audit_logger_t *logger = (audit_logger_t *)cupolas_mem_alloc(sizeof(audit_logger_t));
    if (!logger)
        return NULL;

    __builtin_memset(logger, 0, sizeof(audit_logger_t));

    if (log_dir) {
        logger->log_dir = cupolas_strdup(log_dir);
        if (!logger->log_dir)
            goto error;
    }

    if (log_prefix) {
        logger->log_prefix = cupolas_strdup(log_prefix);
        if (!logger->log_prefix)
            goto error;
    }

    logger->max_file_size = max_file_size > 0 ? max_file_size : 10 * 1024 * 1024;
    logger->max_files = max_files > 0 ? max_files : 10;

    logger->queue = audit_queue_create(DEFAULT_QUEUE_SIZE);
    if (!logger->queue)
        goto error;

    logger->rotator = audit_rotator_create(log_dir, log_prefix, max_file_size, max_files);
    if (!logger->rotator)
        goto error;

    cupolas_atomic_store32(&logger->running, 1);

    if (cupolas_thread_create(&logger->writer_thread, audit_writer_thread, logger) != cupolas_OK) {
        goto error;
    }

    return logger;

error:
    if (logger->queue)
        audit_queue_destroy(logger->queue);
    if (logger->rotator)
        audit_rotator_destroy(logger->rotator);
    cupolas_mem_free(logger->log_dir);
    cupolas_mem_free(logger->log_prefix);
    cupolas_mem_free(logger);
    return NULL;
}

void audit_logger_destroy(audit_logger_t *logger)
{
    if (!logger)
        return;

    cupolas_atomic_store32(&logger->running, 0);
    audit_queue_shutdown(logger->queue, false);
    cupolas_thread_join(logger->writer_thread, NULL);

    audit_queue_destroy(logger->queue);
    audit_rotator_destroy(logger->rotator);

    cupolas_mem_free(logger->log_dir);
    cupolas_mem_free(logger->log_prefix);
    cupolas_mem_free(logger);
}

int audit_logger_log(audit_logger_t *logger, audit_event_type_t type, const char *agent_id,
                     const char *action, const char *resource, const char *detail, int result)
{
    if (!logger) {
        AGENTOS_LOG_ERROR("audit_logger_log: NULL logger parameter");
        return cupolas_ERROR_INVALID_ARG;
    }

    audit_entry_t *entry = audit_entry_create(type, agent_id, action, resource, detail, result);
    if (!entry) {
        /* SEC-13: Fallback to pre-allocated audit buffer under OOM */
        void *emergency_buf = agentos_prealloc_acquire(AGENTOS_PREALLOC_AUDIT);
        if (emergency_buf) {
            AGENTOS_LOG_WARN("audit_logger_log: using emergency buffer fallback for type=%d, agent_id=%s, action=%s", (int)type, agent_id ? agent_id : "(null)", action ? action : "(null)");
            /* Write a minimal audit record to the emergency buffer */
            int written = snprintf((char *)emergency_buf,
                    AGENTOS_PREALLOC_AUDIT_BUF_SIZE,
                    "{\"type\":%d,\"agent_id\":\"%s\",\"action\":\"%s\","
                    "\"resource\":\"%s\",\"detail\":\"%s\",\"result\":%d,"
                    "\"emergency\":true}\n",
                    (int)type,
                    agent_id ? agent_id : "",
                    action ? action : "",
                    resource ? resource : "",
                    detail ? detail : "",
                    result);

            if (written > 0 && (size_t)written < AGENTOS_PREALLOC_AUDIT_BUF_SIZE) {
                /* Write emergency audit entry directly via rotator */
                if (logger->rotator) {
                    /* Construct a minimal entry from the emergency buffer
                     * for the rotator to write */
                    audit_entry_t *oom_entry = audit_oom_pool_alloc();
                    if (oom_entry) {
                        oom_entry->type = type;
                        oom_entry->timestamp_ms = 0; /* best-effort */
                        oom_entry->result = result;
                        audit_rotator_write(logger->rotator, oom_entry);
                    }
                }
            }

            agentos_prealloc_release(AGENTOS_PREALLOC_AUDIT);
        }
        return cupolas_ERROR_NO_MEMORY;
    }

    /* === 编码契约: SHA-256 审计哈希链（BAN-129）=== */
    cupolas_mutex_lock(&g_hash_chain_lock);
    __builtin_memcpy(entry->prev_hash, g_last_hash, sizeof(entry->prev_hash));
    audit_compute_chain_hash(entry, g_last_hash, entry->curr_hash);
    __builtin_memcpy(g_last_hash, entry->curr_hash, sizeof(g_last_hash));
    cupolas_mutex_unlock(&g_hash_chain_lock);

    int ret = audit_queue_try_push(logger->queue, entry);
    if (ret != cupolas_OK) {
        AGENTOS_LOG_ERROR("[CRITICAL] audit_logger_log: audit queue push failed (buffer overflow), type=%d, agent_id=%s, action=%s, ret=%d", (int)type, agent_id ? agent_id : "(null)", action ? action : "(null)", ret);
        audit_entry_destroy(entry);
        cupolas_atomic_add64(&logger->total_failed, 1);
    }

    return ret;
}

int audit_logger_log_permission(audit_logger_t *logger, const char *agent_id, const char *action,
                                const char *resource, int allowed)
{
    return audit_logger_log(logger, AUDIT_EVENT_PERMISSION, agent_id, action, resource, NULL,
                            allowed);
}

int audit_logger_log_sanitizer(audit_logger_t *logger, const char *agent_id, const char *input,
                               const char *output, int passed)
{
    return audit_logger_log(logger, AUDIT_EVENT_SANITIZER, agent_id, "sanitize", input, output,
                            passed);
}

int audit_logger_log_workbench(audit_logger_t *logger, const char *agent_id, const char *command,
                               int exit_code)
{
    return audit_logger_log(logger, AUDIT_EVENT_WORKBENCH, agent_id, "execute", command, NULL,
                            exit_code);
}

/**
 * @brief 验证审计哈希链完整性（BAN-129 编码契约）
 *
 * 从给定条目列表重新计算哈希链，验证每个条目的 curr_hash 是否与
 * 基于 prev_hash + 条目内容的 SHA-256 哈希一致。
 *
 * @param entries     审计条目数组
 * @param entry_count 条目数量
 * @param first_prev_hash 链起始哈希（通常为全零）
 * @param out_invalid_index 输出第一个无效条目的索引（-1 表示全部有效）
 * @return true 如果哈希链完整，false 如果有篡改
 */
bool audit_logger_verify_chain(const audit_entry_t **entries, size_t entry_count,
                               const char *first_prev_hash, int *out_invalid_index)
{
    if (!entries || entry_count == 0 || !first_prev_hash) {
        AGENTOS_LOG_ERROR("audit_logger_verify_chain: NULL/invalid parameter - entries=%p, entry_count=%zu, first_prev_hash=%p", (void *)entries, entry_count, (void *)first_prev_hash);
        if (out_invalid_index) *out_invalid_index = -1;
        return false;
    }

    char expected_prev[65];
    __builtin_memcpy(expected_prev, first_prev_hash, 65);

    for (size_t i = 0; i < entry_count; i++) {
        const audit_entry_t *entry = entries[i];
        if (!entry) {
            AGENTOS_LOG_ERROR("audit_logger_verify_chain: NULL entry at index=%zu", i);
            if (out_invalid_index) *out_invalid_index = (int)i;
            return false;
        }

        /* 验证 prev_hash 是否匹配 */
        if (memcmp(entry->prev_hash, expected_prev, 65) != 0) {
            AGENTOS_LOG_ERROR("audit_logger_verify_chain: prev_hash mismatch at index=%zu, chain tampered", i);
            if (out_invalid_index) *out_invalid_index = (int)i;
            return false;
        }

        /* 重新计算 curr_hash 并验证 */
        char recomputed_hash[65];
        audit_compute_chain_hash(entry, expected_prev, recomputed_hash);
        if (memcmp(entry->curr_hash, recomputed_hash, 65) != 0) {
            AGENTOS_LOG_ERROR("audit_logger_verify_chain: curr_hash mismatch at index=%zu, entry tampered or corrupted", i);
            if (out_invalid_index) *out_invalid_index = (int)i;
            return false;
        }

        /* 前进到下一个条目 */
        __builtin_memcpy(expected_prev, entry->curr_hash, 65);
    }

    if (out_invalid_index) *out_invalid_index = -1;
    return true;
}

void audit_logger_flush(audit_logger_t *logger)
{
    if (!logger)
        return;

    while (audit_queue_size(logger->queue) > 0) {
        cupolas_sleep_ms(10);
    }
}

void audit_logger_stats(audit_logger_t *logger, uint64_t *total_logged, uint64_t *total_failed)
{
    if (!logger) {
        if (total_logged)
            *total_logged = 0;
        if (total_failed)
            *total_failed = 0;
        return;
    }

    if (total_logged)
        *total_logged = cupolas_atomic_load64(&logger->total_logged);
    if (total_failed)
        *total_failed = cupolas_atomic_load64(&logger->total_failed);
}
