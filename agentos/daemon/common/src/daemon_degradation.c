// SPDX-FileCopyrightText: 2026 SPHARX Ltd.
// SPDX-License-Identifier: Apache-2.0
/**
 * @file daemon_degradation.c
 * @brief Daemon 层优雅降级处理器实现（SEC-14 合规）
 *
 * 实现五类标准化降级处理器:
 *   1. 缓存容量降级  — WARNING: 容量减半 / 恢复时还原
 *   2. 日志级别降级  — WARNING: 提升至 ERROR / 恢复时还原
 *   3. 批处理降级    — HIGH: 批次大小减半 / 恢复时还原
 *   4. 连接拒绝降级  — CRITICAL: 拒绝新连接 / 恢复时接受
 *   5. 自定义降级    — 任意水位触发任意回调
 *
 * @see oom_handler.h  OOM 分级响应框架
 */

#include "daemon_degradation.h"
#include "error.h"
#include "svc_cache.h"
#include "svc_logger.h"

#include <string.h>

/* ============================================================================
 * 常量
 * ============================================================================ */

#define MAX_DEGRADATION_HANDLERS 16  /**< 最大同时注册的降级处理器数 */

/* ============================================================================
 * 全局状态
 * ============================================================================ */

static degradation_handler_t g_degradation_handlers[MAX_DEGRADATION_HANDLERS];
static degrade_cache_ctx_t g_cache_contexts[MAX_DEGRADATION_HANDLERS];
static degrade_log_ctx_t g_log_contexts[MAX_DEGRADATION_HANDLERS];
static degrade_batch_ctx_t g_batch_contexts[MAX_DEGRADATION_HANDLERS];
static degrade_conn_ctx_t g_conn_contexts[MAX_DEGRADATION_HANDLERS];
static int g_handler_count = 0;

/* ============================================================================
 * 降级回调实现
 * ============================================================================ */

/**
 * @brief 缓存降级回调 — 将缓存容量减半
 */
static int on_cache_degrade(degradation_handler_t *handler,
                            watermark_level_t old_level,
                            watermark_level_t new_level)
{
    (void)old_level;
    (void)new_level;

    degrade_cache_ctx_t *ctx = (degrade_cache_ctx_t *)handler->context;
    if (!ctx || !ctx->cache_handle) return AGENTOS_ERR_INVALID_PARAM;

    size_t half_capacity = ctx->original_capacity / 2;
    if (half_capacity < 1) half_capacity = 1;

    svc_cache_set_capacity((svc_cache_t *)ctx->cache_handle, half_capacity);
    ctx->reduced_capacity = half_capacity;

    SVC_LOG_WARN("Cache '%s': capacity reduced %zu → %zu", handler->feature_name, ctx->original_capacity, half_capacity);
    return 0;
}

/**
 * @brief 缓存恢复回调 — 恢复原始缓存容量
 */
static int on_cache_restore(degradation_handler_t *handler,
                            watermark_level_t old_level,
                            watermark_level_t new_level)
{
    (void)old_level;
    (void)new_level;

    degrade_cache_ctx_t *ctx = (degrade_cache_ctx_t *)handler->context;
    if (!ctx || !ctx->cache_handle) return AGENTOS_ERR_INVALID_PARAM;

    svc_cache_set_capacity((svc_cache_t *)ctx->cache_handle, ctx->original_capacity);

    SVC_LOG_WARN("Cache '%s': capacity restored %zu → %zu", handler->feature_name, ctx->reduced_capacity, ctx->original_capacity);
    return 0;
}

/**
 * @brief 日志降级回调 — 将日志级别提升至 ERROR
 */
static int on_log_degrade(degradation_handler_t *handler,
                          watermark_level_t old_level,
                          watermark_level_t new_level)
{
    (void)old_level;
    (void)new_level;

    degrade_log_ctx_t *ctx = (degrade_log_ctx_t *)handler->context;
    if (!ctx) return AGENTOS_ERR_INVALID_PARAM;

    agentos_log_set_level(LOG_LEVEL_ERROR);
    ctx->degraded_log_level = LOG_LEVEL_ERROR;

    SVC_LOG_WARN("Log level: raised to ERROR (was level %d)", ctx->original_log_level);
    return 0;
}

/**
 * @brief 日志恢复回调 — 恢复原始日志级别
 */
static int on_log_restore(degradation_handler_t *handler,
                          watermark_level_t old_level,
                          watermark_level_t new_level)
{
    (void)old_level;
    (void)new_level;

    degrade_log_ctx_t *ctx = (degrade_log_ctx_t *)handler->context;
    if (!ctx) return AGENTOS_ERR_INVALID_PARAM;

    agentos_log_set_level((agentos_log_level_t)ctx->original_log_level);

    SVC_LOG_WARN("Log level: restored to level %d", ctx->original_log_level);
    return 0;
}

/**
 * @brief 批处理降级回调 — 将批次大小减半
 */
static int on_batch_degrade(degradation_handler_t *handler,
                            watermark_level_t old_level,
                            watermark_level_t new_level)
{
    (void)old_level;
    (void)new_level;

    degrade_batch_ctx_t *ctx = (degrade_batch_ctx_t *)handler->context;
    if (!ctx || !ctx->batch_size_ptr) return AGENTOS_ERR_INVALID_PARAM;

    size_t half = ctx->original_batch_size / 2;
    if (half < 1) half = 1;

    *ctx->batch_size_ptr = half;
    ctx->reduced_batch_size = half;

    SVC_LOG_WARN("Batch '%s': size reduced %zu → %zu", handler->feature_name, ctx->original_batch_size, half);
    return 0;
}

/**
 * @brief 批处理恢复回调 — 恢复原始批次大小
 */
static int on_batch_restore(degradation_handler_t *handler,
                            watermark_level_t old_level,
                            watermark_level_t new_level)
{
    (void)old_level;
    (void)new_level;

    degrade_batch_ctx_t *ctx = (degrade_batch_ctx_t *)handler->context;
    if (!ctx || !ctx->batch_size_ptr) return AGENTOS_ERR_INVALID_PARAM;

    *ctx->batch_size_ptr = ctx->original_batch_size;

    SVC_LOG_WARN("Batch '%s': size restored %zu → %zu", handler->feature_name, ctx->reduced_batch_size, ctx->original_batch_size);
    return 0;
}

/**
 * @brief 连接拒绝降级回调 — 设置拒绝新连接标志
 */
static int on_conn_degrade(degradation_handler_t *handler,
                           watermark_level_t old_level,
                           watermark_level_t new_level)
{
    (void)old_level;
    (void)new_level;

    degrade_conn_ctx_t *ctx = (degrade_conn_ctx_t *)handler->context;
    if (!ctx || !ctx->reject_new_flag) return AGENTOS_ERR_INVALID_PARAM;

    *ctx->reject_new_flag = true;

    SVC_LOG_WARN("Connection '%s': rejecting new connections", handler->feature_name);
    return 0;
}

/**
 * @brief 连接恢复回调 — 清除拒绝新连接标志
 */
static int on_conn_restore(degradation_handler_t *handler,
                           watermark_level_t old_level,
                           watermark_level_t new_level)
{
    (void)old_level;
    (void)new_level;

    degrade_conn_ctx_t *ctx = (degrade_conn_ctx_t *)handler->context;
    if (!ctx || !ctx->reject_new_flag) return AGENTOS_ERR_INVALID_PARAM;

    *ctx->reject_new_flag = false;

    SVC_LOG_WARN("Connection '%s': accepting new connections", handler->feature_name);
    return 0;
}

/* ============================================================================
 * 公共 API 实现
 * ============================================================================ */

degradation_handler_t *daemon_degradation_register_cache(
    void *cache_handle, size_t original_capacity)
{
    if (!cache_handle || g_handler_count >= MAX_DEGRADATION_HANDLERS) {
        return NULL;
    }

    int idx = g_handler_count++;

    /* 初始化上下文 */
    degrade_cache_ctx_t *ctx = &g_cache_contexts[idx];
    ctx->cache_handle = cache_handle;
    ctx->original_capacity = original_capacity;
    ctx->reduced_capacity = original_capacity;

    /* 初始化处理器 */
    degradation_handler_t *handler = &g_degradation_handlers[idx];
    __builtin_memset(handler, 0, sizeof(degradation_handler_t));
    handler->feature_name = "svc_cache";
    handler->trigger_level = WATERMARK_WARNING;
    handler->action = DEGRADE_REDUCE_CACHE;
    handler->on_degrade = on_cache_degrade;
    handler->on_restore = on_cache_restore;
    handler->context = ctx;
    handler->is_degraded = false;
    handler->next = NULL;

    agentos_register_degradation(handler);
    return handler;
}

degradation_handler_t *daemon_degradation_register_log_level(
    int original_log_level)
{
    if (g_handler_count >= MAX_DEGRADATION_HANDLERS) {
        return NULL;
    }

    int idx = g_handler_count++;

    /* 初始化上下文 */
    degrade_log_ctx_t *ctx = &g_log_contexts[idx];
    ctx->original_log_level = original_log_level;
    ctx->degraded_log_level = original_log_level;

    /* 初始化处理器 */
    degradation_handler_t *handler = &g_degradation_handlers[idx];
    __builtin_memset(handler, 0, sizeof(degradation_handler_t));
    handler->feature_name = "svc_log";
    handler->trigger_level = WATERMARK_WARNING;
    handler->action = DEGRADE_REDUCE_LOG_LEVEL;
    handler->on_degrade = on_log_degrade;
    handler->on_restore = on_log_restore;
    handler->context = ctx;
    handler->is_degraded = false;
    handler->next = NULL;

    agentos_register_degradation(handler);
    return handler;
}

degradation_handler_t *daemon_degradation_register_batch(
    size_t *batch_size_ptr, size_t original_batch_size)
{
    if (!batch_size_ptr || g_handler_count >= MAX_DEGRADATION_HANDLERS) {
        return NULL;
    }

    int idx = g_handler_count++;

    /* 初始化上下文 */
    degrade_batch_ctx_t *ctx = &g_batch_contexts[idx];
    ctx->batch_size_ptr = batch_size_ptr;
    ctx->original_batch_size = original_batch_size;
    ctx->reduced_batch_size = original_batch_size;

    /* 初始化处理器 */
    degradation_handler_t *handler = &g_degradation_handlers[idx];
    __builtin_memset(handler, 0, sizeof(degradation_handler_t));
    handler->feature_name = "svc_batch";
    handler->trigger_level = WATERMARK_HIGH;
    handler->action = DEGRADE_REDUCE_BATCH;
    handler->on_degrade = on_batch_degrade;
    handler->on_restore = on_batch_restore;
    handler->context = ctx;
    handler->is_degraded = false;
    handler->next = NULL;

    agentos_register_degradation(handler);
    return handler;
}

degradation_handler_t *daemon_degradation_register_reject_conn(
    bool *reject_flag)
{
    if (!reject_flag || g_handler_count >= MAX_DEGRADATION_HANDLERS) {
        return NULL;
    }

    int idx = g_handler_count++;

    /* 初始化上下文 */
    degrade_conn_ctx_t *ctx = &g_conn_contexts[idx];
    ctx->reject_new_flag = reject_flag;

    /* 初始化处理器 */
    degradation_handler_t *handler = &g_degradation_handlers[idx];
    __builtin_memset(handler, 0, sizeof(degradation_handler_t));
    handler->feature_name = "svc_conn";
    handler->trigger_level = WATERMARK_CRITICAL;
    handler->action = DEGRADE_REJECT_NEW_CONN;
    handler->on_degrade = on_conn_degrade;
    handler->on_restore = on_conn_restore;
    handler->context = ctx;
    handler->is_degraded = false;
    handler->next = NULL;

    agentos_register_degradation(handler);
    return handler;
}

degradation_handler_t *daemon_degradation_register_custom(
    const char *feature_name,
    watermark_level_t trigger_level,
    int (*on_degrade)(degradation_handler_t *, watermark_level_t, watermark_level_t),
    int (*on_restore)(degradation_handler_t *, watermark_level_t, watermark_level_t),
    void *context)
{
    if (!feature_name || !on_degrade || g_handler_count >= MAX_DEGRADATION_HANDLERS) {
        return NULL;
    }

    int idx = g_handler_count++;

    /* 初始化处理器 */
    degradation_handler_t *handler = &g_degradation_handlers[idx];
    __builtin_memset(handler, 0, sizeof(degradation_handler_t));
    handler->feature_name = feature_name;
    handler->trigger_level = trigger_level;
    handler->action = DEGRADE_CUSTOM;
    handler->on_degrade = on_degrade;
    handler->on_restore = on_restore;
    handler->context = context;
    handler->is_degraded = false;
    handler->next = NULL;

    agentos_register_degradation(handler);
    return handler;
}

void daemon_degradation_unregister_all(void)
{
    for (int i = 0; i < g_handler_count; i++) {
        agentos_unregister_degradation(&g_degradation_handlers[i]);
    }
    g_handler_count = 0;

    SVC_LOG_WARN("All degradation handlers unregistered");
}
