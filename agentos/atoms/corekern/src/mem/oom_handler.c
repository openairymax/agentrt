/**
 * @file oom_handler.c
 * @brief OOM 分级响应框架 - 核心实现
 *
 * 实现五级内存压力框架（SEC-12）：
 *   NORMAL(≤60%) → WARNING(60-75%) → HIGH(75-90%) → CRITICAL(>90%) → FATAL(>95%)
 *
 * 以及优雅降级回调机制（SEC-14）和统计上报（SEC-15）。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "oom_handler.h"
#include "mem.h"
#include "memory_compat.h"
#include "memory_prealloc.h"
#include "string_compat.h"

#include "logging_compat.h"
#include "config_unified.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 内部常量
 * ============================================================================ */

#define OOM_TRACKER_CAPACITY 1024  /**< 分配跟踪环形缓冲区容量 */
#define OOM_DEFAULT_SYSTEM_MEMORY (512ULL * 1024 * 1024)  /**< 默认系统内存 512MB */

/* 压力分级阈值（可配置） */
#define OOM_PRESSURE_THRESHOLD_WARNING   0.70  /**< 70% → WARNING */
#define OOM_PRESSURE_THRESHOLD_DEGRADED  0.80  /**< 80% → DEGRADED */
#define OOM_PRESSURE_THRESHOLD_CRITICAL  0.90  /**< 90% → CRITICAL */
#define OOM_PRESSURE_THRESHOLD_FATAL     0.95  /**< 95% → FATAL */

/* ============================================================================
 * 全局 OOM 处理器单例
 * ============================================================================ */

static oom_handler_t *g_oom_handler = NULL;

/* 用于保护全局状态的原子操作 */
#include "atomic_compat.h"

/* ============================================================================
 * 内部辅助函数
 * ============================================================================ */

/**
 * @brief 获取当前时间戳（毫秒）
 */
static uint64_t oom_get_time_ms(void)
{
    /* 使用 agentos_time_ms() 如果可用，否则回退到简单实现 */
    /* 使用平台时间函数获取毫秒时间戳 */
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

/**
 * @brief 计算五级水位级别
 *
 * 基于内存使用率计算五级压力级别。
 * 阈值:
 *   ≤60% → NORMAL
 *   60-75% → WARNING
 *   75-90% → HIGH
 *   90-95% → CRITICAL
 *   >95% → FATAL
 */
static watermark_level_t oom_calc_five_level_watermark(
    size_t current_allocated, size_t total_memory)
{
    if (total_memory == 0) {
        return WATERMARK_NORMAL;
    }

    double usage = (double)current_allocated / (double)total_memory;

    if (usage > 0.95)      return WATERMARK_FATAL;
    else if (usage > 0.90) return WATERMARK_CRITICAL;
    else if (usage > 0.75) return WATERMARK_HIGH;
    else if (usage > 0.60) return WATERMARK_WARNING;
    else                   return WATERMARK_NORMAL;
}

/**
 * @brief 获取水位级别名称（用于日志）
 */
static const char *oom_watermark_name(int level)
{
    switch (level) {
    case WATERMARK_NORMAL:   return "NORMAL";
    case WATERMARK_WARNING:  return "WARNING";
    case WATERMARK_HIGH:     return "HIGH";
    case WATERMARK_CRITICAL: return "CRITICAL";
    case WATERMARK_FATAL:    return "FATAL";
    default:                 return "UNKNOWN";
    }
}

/**
 * @brief 获取响应级别名称（用于日志）
 */
static const char *oom_response_name(int response)
{
    switch (response) {
    case OOM_RESPONSE_WARNING:         return "WARNING";
    case OOM_RESPONSE_DEGRADED:        return "DEGRADED";
    case OOM_RESPONSE_CRITICAL:        return "CRITICAL";
    case OOM_RESPONSE_FATAL:           return "FATAL";
    case OOM_RESPONSE_FATAL_TERMINATE: return "FATAL_TERMINATE";
    default:                           return "UNKNOWN";
    }
}

/**
 * @brief 获取压力级别名称（用于日志）
 */
static const char *oom_pressure_name(agentos_mem_pressure_level_t level)
{
    switch (level) {
    case AGENTOS_MEM_PRESSURE_NORMAL:   return "NORMAL";
    case AGENTOS_MEM_PRESSURE_WARNING:  return "WARNING";
    case AGENTOS_MEM_PRESSURE_DEGRADED: return "DEGRADED";
    case AGENTOS_MEM_PRESSURE_CRITICAL: return "CRITICAL";
    case AGENTOS_MEM_PRESSURE_FATAL:    return "FATAL";
    default:                            return "UNKNOWN";
    }
}

/**
 * @brief 基于内存使用率计算压力级别
 */
static agentos_mem_pressure_level_t oom_calc_pressure_level(
    size_t current_allocated, size_t total_memory)
{
    if (total_memory == 0) {
        return AGENTOS_MEM_PRESSURE_NORMAL;
    }

    double usage = (double)current_allocated / (double)total_memory;

    if (usage > OOM_PRESSURE_THRESHOLD_FATAL)      return AGENTOS_MEM_PRESSURE_FATAL;
    else if (usage > OOM_PRESSURE_THRESHOLD_CRITICAL) return AGENTOS_MEM_PRESSURE_CRITICAL;
    else if (usage > OOM_PRESSURE_THRESHOLD_DEGRADED) return AGENTOS_MEM_PRESSURE_DEGRADED;
    else if (usage > OOM_PRESSURE_THRESHOLD_WARNING)  return AGENTOS_MEM_PRESSURE_WARNING;
    else                                               return AGENTOS_MEM_PRESSURE_NORMAL;
}

/**
 * @brief 触发压力级别变化回调
 */
static void oom_fire_pressure_callbacks(
    agentos_mem_pressure_level_t old_level,
    agentos_mem_pressure_level_t new_level)
{
    if (!g_oom_handler) {
        return;
    }

    oom_handler_t *handler = g_oom_handler;

    /* 对每个达到或超过的级别触发回调 */
    int start = (new_level > old_level) ? (int)old_level + 1 : (int)new_level;
    int end   = (int)new_level;

    for (int level = start; level <= end; level++) {
        for (int i = 0; i < AGENTOS_PRESSURE_MAX_CALLBACKS; i++) {
            if (handler->pressure_callbacks[level][i].active &&
                handler->pressure_callbacks[level][i].callback) {
                handler->pressure_callbacks[level][i].callback(
                    (agentos_mem_pressure_level_t)level,
                    handler->pressure_callbacks[level][i].user_data);
            }
        }
    }
}

/* ============================================================================
 * 公共 API 实现
 * ============================================================================ */

agentos_error_t agentos_oom_init(size_t total_system_memory)
{
    /* 防止重复初始化 */
    if (g_oom_handler != NULL) {
        return AGENTOS_SUCCESS;
    }

    oom_handler_t *handler =
        (oom_handler_t *)AGENTOS_CALLOC(1, sizeof(oom_handler_t));
    if (!handler) {
        return AGENTOS_ENOMEM;
    }

    /* 设置系统总内存 */
    if (total_system_memory == 0) {
        handler->total_system_memory = OOM_DEFAULT_SYSTEM_MEMORY;
    } else {
        handler->total_system_memory = total_system_memory;
    }

    /* 初始化扩展统计 */
    handler->ext_stats = (memory_stats_extended_t *)AGENTOS_CALLOC(
        1, sizeof(memory_stats_extended_t));
    if (!handler->ext_stats) {
        AGENTOS_FREE(handler);
        return AGENTOS_ENOMEM;
    }

    /* 初始化分配跟踪环形缓冲区 */
    int ret = agentos_memory_stats_extended_init(
        handler->ext_stats, OOM_TRACKER_CAPACITY);
    if (ret != 0) {
        AGENTOS_FREE(handler->ext_stats);
        AGENTOS_FREE(handler);
        return AGENTOS_ENOMEM;
    }

    handler->ext_stats->total_system_memory = handler->total_system_memory;
    handler->current_watermark = WATERMARK_NORMAL;

    /* 初始化压力分级系统 */
    handler->current_pressure = AGENTOS_MEM_PRESSURE_NORMAL;
    handler->pressure_denied_count = 0;
    handler->total_allocated_bytes = 0;
    agentos_mutex_init(&handler->pressure_mutex);
    AGENTOS_MEMSET(handler->pressure_callbacks, 0,
                   sizeof(handler->pressure_callbacks));

    g_oom_handler = handler;

    /* SEC-13: Initialize pre-allocation pool for critical paths */
    if (agentos_prealloc_init() != 0) {
        AGENTOS_LOG_WARN("[OOM] Pre-allocation pool init failed; "
                "critical path buffers unavailable");
    }

    AGENTOS_LOG_INFO("[OOM] OOM handler initialized: system_memory=%zu bytes, "
            "tracker_capacity=%d",
            handler->total_system_memory, OOM_TRACKER_CAPACITY);

    return AGENTOS_SUCCESS;
}

void agentos_oom_destroy(void)
{
    if (!g_oom_handler) {
        return;
    }

    oom_handler_t *handler = g_oom_handler;

    /* 销毁扩展统计 */
    if (handler->ext_stats) {
        agentos_memory_stats_extended_destroy(handler->ext_stats);
        AGENTOS_FREE(handler->ext_stats);
        handler->ext_stats = NULL;
    }

    /* 降级处理器链表由调用者管理生命周期，此处仅清理引用 */
    handler->degradation_handlers = NULL;

    /* 清理压力分级系统 */
    agentos_mutex_destroy(&handler->pressure_mutex);
    AGENTOS_MEMSET(handler->pressure_callbacks, 0,
                   sizeof(handler->pressure_callbacks));
    handler->current_pressure = AGENTOS_MEM_PRESSURE_NORMAL;

    AGENTOS_FREE(handler);
    g_oom_handler = NULL;

    /* SEC-13: Shutdown pre-allocation pool */
    agentos_prealloc_shutdown();

    AGENTOS_LOG_INFO("[OOM] OOM handler destroyed");
}

oom_handler_t *agentos_oom_get_handler(void)
{
    return g_oom_handler;
}

oom_response_level_t agentos_oom_determine_response(watermark_level_t level)
{
    switch ((int)level) {
    case WATERMARK_NORMAL:
        return OOM_RESPONSE_WARNING;
    case WATERMARK_WARNING:
        return OOM_RESPONSE_DEGRADED;
    case WATERMARK_HIGH:
        return OOM_RESPONSE_CRITICAL;
    case WATERMARK_CRITICAL:
        return OOM_RESPONSE_FATAL;
    case WATERMARK_FATAL:
        return OOM_RESPONSE_FATAL_TERMINATE;
    default:
        return OOM_RESPONSE_WARNING;
    }
}

int agentos_oom_handle(size_t requested, size_t available)
{
    if (!g_oom_handler) {
        /* 未初始化时直接返回 WARNING */
        return OOM_RESPONSE_WARNING;
    }

    oom_handler_t *handler = g_oom_handler;

    /* 记录 OOM 事件 */
    handler->oom_event_count++;
    handler->last_oom_time = oom_get_time_ms();
    handler->last_oom_requested = requested;
    handler->last_oom_available = available;

    /* 更新扩展统计中的 OOM 事件 */
    if (handler->ext_stats) {
        handler->ext_stats->oom_event_count = handler->oom_event_count;
        handler->ext_stats->last_oom_time = handler->last_oom_time;
        handler->ext_stats->last_oom_requested = handler->last_oom_requested;
    }

    /* 刷新水位 */
    watermark_level_t old_level = handler->current_watermark;
    if (handler->ext_stats) {
        handler->current_watermark = oom_calc_five_level_watermark(
            handler->ext_stats->current_allocated,
            handler->total_system_memory);
    }

    int response = agentos_oom_determine_response(handler->current_watermark);

    AGENTOS_LOG_ERROR("[OOM] OOM event #%llu: requested=%zu, available=%zu, "
            "watermark=%s, response=%s",
            (unsigned long long)handler->oom_event_count,
            requested, available,
            oom_watermark_name(handler->current_watermark),
            oom_response_name(response));

    /* 根据响应级别执行对应动作 */
    switch (response) {
    case OOM_RESPONSE_WARNING:
        /* 仅记录日志，继续运行 */
        break;

    case OOM_RESPONSE_DEGRADED:
        /* 触发降级回调 */
        agentos_oom_degrade(old_level, handler->current_watermark);
        break;

    case OOM_RESPONSE_CRITICAL:
        /* 触发降级 + 拒绝新请求 */
        agentos_oom_degrade(old_level, handler->current_watermark);
        /* 返回 CRITICAL，调用者应拒绝新请求 */
        break;

    case OOM_RESPONSE_FATAL:
    case OOM_RESPONSE_FATAL_TERMINATE:
        /* 最后尝试降级，然后终止 */
        agentos_oom_degrade(old_level, handler->current_watermark);
        AGENTOS_LOG_ERROR("[OOM] FATAL: terminating process. "
                "requested=%zu, available=%zu, total_events=%llu",
                requested, available,
                (unsigned long long)handler->oom_event_count);
        abort();
        break;

    default:
        break;
    }

    return response;
}

agentos_error_t agentos_register_degradation(degradation_handler_t *handler)
{
    if (!handler || !handler->feature_name) {
        return AGENTOS_EINVAL;
    }

    if (!g_oom_handler) {
        return AGENTOS_ENOTINIT;
    }

    oom_handler_t *oom = g_oom_handler;

    /* 检查是否已注册 */
    degradation_handler_t *existing = oom->degradation_handlers;
    while (existing) {
        if (existing == handler) {
            /* 已注册，返回成功 */
            return AGENTOS_SUCCESS;
        }
        existing = existing->next;
    }

    /* 添加到链表头部 */
    handler->next = oom->degradation_handlers;
    handler->is_degraded = false;
    oom->degradation_handlers = handler;

    AGENTOS_LOG_INFO("[OOM] Degradation handler registered: %s (trigger=%s)",
            handler->feature_name,
            oom_watermark_name(handler->trigger_level));

    return AGENTOS_SUCCESS;
}

void agentos_unregister_degradation(degradation_handler_t *handler)
{
    if (!handler || !g_oom_handler) {
        return;
    }

    oom_handler_t *oom = g_oom_handler;
    degradation_handler_t **pp = &oom->degradation_handlers;

    while (*pp) {
        if (*pp == handler) {
            *pp = handler->next;
            handler->next = NULL;
            handler->is_degraded = false;

            AGENTOS_LOG_INFO("[OOM] Degradation handler unregistered: %s",
                    handler->feature_name);
            return;
        }
        pp = &(*pp)->next;
    }
}

void agentos_oom_degrade(watermark_level_t old_level,
                         watermark_level_t new_level)
{
    if (!g_oom_handler) {
        return;
    }

    oom_handler_t *oom = g_oom_handler;
    degradation_handler_t *handler = oom->degradation_handlers;

    while (handler) {
        /* 水位上升：检查是否需要触发降级 */
        if (new_level >= handler->trigger_level && !handler->is_degraded) {
            handler->is_degraded = true;
            if (handler->on_degrade) {
                AGENTOS_LOG_WARN("[OOM] Degrading: %s (level %s → %s)",
                        handler->feature_name,
                        oom_watermark_name(old_level),
                        oom_watermark_name(new_level));
                handler->on_degrade(handler, old_level, new_level);
            }
        }
        /* 水位回落：检查是否需要恢复 */
        else if (new_level < handler->trigger_level && handler->is_degraded) {
            handler->is_degraded = false;
            if (handler->on_restore) {
                AGENTOS_LOG_INFO("[OOM] Restoring: %s (level %s → %s)",
                        handler->feature_name,
                        oom_watermark_name(old_level),
                        oom_watermark_name(new_level));
                handler->on_restore(handler, old_level, new_level);
            }
        }
        handler = handler->next;
    }
}

void agentos_oom_update_watermark(size_t current_allocated)
{
    if (!g_oom_handler) {
        return;
    }

    oom_handler_t *oom = g_oom_handler;
    watermark_level_t old_level = oom->current_watermark;
    watermark_level_t new_level = oom_calc_five_level_watermark(
        current_allocated, oom->total_system_memory);

    if (new_level != old_level) {
        oom->current_watermark = new_level;

        AGENTOS_LOG_WARN("[OOM] Watermark changed: %s → %s "
                "(allocated=%zu/%zu, %.1f%%)",
                oom_watermark_name(old_level),
                oom_watermark_name(new_level),
                current_allocated, oom->total_system_memory,
                100.0 * (double)current_allocated /
                         (double)oom->total_system_memory);

        /* 触发降级/恢复回调 */
        agentos_oom_degrade(old_level, new_level);
    }
}

watermark_level_t agentos_oom_get_watermark(void)
{
    if (!g_oom_handler) {
        return WATERMARK_NORMAL;
    }
    return g_oom_handler->current_watermark;
}

bool agentos_oom_is_degraded(void)
{
    if (!g_oom_handler) {
        return false;
    }
    return g_oom_handler->current_watermark >= WATERMARK_WARNING;
}

/* ============================================================================
 * 内存压力分级 API 实现（SEC-12 OOM 分级响应框架）
 * ============================================================================ */

agentos_mem_pressure_level_t agentos_oom_get_pressure(void)
{
    if (!g_oom_handler) {
        return AGENTOS_MEM_PRESSURE_NORMAL;
    }

    oom_handler_t *handler = g_oom_handler;

    agentos_mutex_lock(&handler->pressure_mutex);

    /* 获取当前已分配内存 */
    size_t current_allocated = 0;
    if (handler->ext_stats) {
        current_allocated = handler->ext_stats->current_allocated;
    }

    agentos_mem_pressure_level_t old_level = handler->current_pressure;
    agentos_mem_pressure_level_t new_level = oom_calc_pressure_level(
        current_allocated, handler->total_system_memory);

    if (new_level != old_level) {
        handler->current_pressure = new_level;

        AGENTOS_LOG_WARN("[OOM] Pressure changed: %s → %s "
                "(allocated=%zu/%zu, %.1f%%)",
                oom_pressure_name(old_level),
                oom_pressure_name(new_level),
                current_allocated, handler->total_system_memory,
                100.0 * (double)current_allocated /
                         (double)handler->total_system_memory);

        /* 触发压力变化回调 */
        oom_fire_pressure_callbacks(old_level, new_level);
    }

    agentos_mem_pressure_level_t result = handler->current_pressure;
    agentos_mutex_unlock(&handler->pressure_mutex);

    return result;
}

void agentos_oom_set_pressure(agentos_mem_pressure_level_t level)
{
    if (!g_oom_handler) {
        return;
    }

    oom_handler_t *handler = g_oom_handler;

    agentos_mutex_lock(&handler->pressure_mutex);

    agentos_mem_pressure_level_t old_level = handler->current_pressure;
    handler->current_pressure = level;

    if (level != old_level) {
        AGENTOS_LOG_WARN("[OOM] Pressure manually set: %s → %s",
                oom_pressure_name(old_level),
                oom_pressure_name(level));

        /* 触发压力变化回调 */
        oom_fire_pressure_callbacks(old_level, level);
    }

    agentos_mutex_unlock(&handler->pressure_mutex);
}

int agentos_oom_register_callback(
    agentos_mem_pressure_level_t level,
    void (*callback)(agentos_mem_pressure_level_t, void *),
    void *user_data)
{
    if (!callback || level < 0 || level > AGENTOS_MEM_PRESSURE_FATAL) {
        return AGENTOS_EINVAL;
    }

    if (!g_oom_handler) {
        return AGENTOS_ENOTINIT;
    }

    oom_handler_t *handler = g_oom_handler;

    agentos_mutex_lock(&handler->pressure_mutex);

    /* 查找空闲槽位 */
    for (int i = 0; i < AGENTOS_PRESSURE_MAX_CALLBACKS; i++) {
        if (!handler->pressure_callbacks[level][i].active) {
            handler->pressure_callbacks[level][i].callback = callback;
            handler->pressure_callbacks[level][i].user_data = user_data;
            handler->pressure_callbacks[level][i].active = true;

            agentos_mutex_unlock(&handler->pressure_mutex);

            AGENTOS_LOG_INFO("[OOM] Pressure callback registered: level=%s, slot=%d",
                    oom_pressure_name(level), i);
            return 0;
        }
    }

    agentos_mutex_unlock(&handler->pressure_mutex);

    AGENTOS_LOG_WARN("[OOM] Pressure callback registration failed: "
            "level=%s, no free slots",
            oom_pressure_name(level));
    return AGENTOS_EBUSY;
}

int agentos_oom_check_allocation(size_t requested_size)
{
    if (!g_oom_handler) {
        /* 未初始化时允许分配 */
        return 0;
    }

    oom_handler_t *handler = g_oom_handler;

    agentos_mutex_lock(&handler->pressure_mutex);

    agentos_mem_pressure_level_t pressure = handler->current_pressure;

    int result = 0; /* 0 = 允许 */

    switch (pressure) {
    case AGENTOS_MEM_PRESSURE_NORMAL:
        /* 正常操作，允许所有分配 */
        break;

    case AGENTOS_MEM_PRESSURE_WARNING:
        /* 允许分配，记录警告 */
        AGENTOS_LOG_WARN("[OOM] Allocation under WARNING pressure: "
                "size=%zu, total_allocated=%zu",
                requested_size,
                handler->total_allocated_bytes);
        break;

    case AGENTOS_MEM_PRESSURE_DEGRADED:
        /* 允许分配，但建议减少非必要分配 */
        AGENTOS_LOG_WARN("[OOM] Allocation under DEGRADED pressure: "
                "size=%zu, total_allocated=%zu",
                requested_size,
                handler->total_allocated_bytes);
        break;

    case AGENTOS_MEM_PRESSURE_CRITICAL:
        /* 拒绝非必要分配 */
        handler->pressure_denied_count++;
        result = AGENTOS_ENOMEM;
        AGENTOS_LOG_ERROR("[OOM] Allocation DENIED under CRITICAL pressure: "
                "size=%zu, total_allocated=%zu, denied_count=%zu",
                requested_size,
                handler->total_allocated_bytes,
                handler->pressure_denied_count);
        break;

    case AGENTOS_MEM_PRESSURE_FATAL:
        /* 拒绝所有分配 */
        handler->pressure_denied_count++;
        result = AGENTOS_ENOMEM;
        AGENTOS_LOG_ERROR("[OOM] Allocation DENIED under FATAL pressure: "
                "size=%zu, total_allocated=%zu, denied_count=%zu",
                requested_size,
                handler->total_allocated_bytes,
                handler->pressure_denied_count);
        break;
    }

    /* 跟踪总分配字节数（仅允许时） */
    if (result == 0) {
        handler->total_allocated_bytes += requested_size;
    }

    agentos_mutex_unlock(&handler->pressure_mutex);

    return result;
}

void agentos_oom_report_stats(void)
{
    if (!g_oom_handler) {
        AGENTOS_LOG_INFO("[OOM] OOM handler not initialized");
        return;
    }

    oom_handler_t *handler = g_oom_handler;

    agentos_mutex_lock(&handler->pressure_mutex);

    size_t current_allocated = 0;
    if (handler->ext_stats) {
        current_allocated = handler->ext_stats->current_allocated;
    }

    double usage_pct = 0.0;
    if (handler->total_system_memory > 0) {
        usage_pct = 100.0 * (double)current_allocated /
                         (double)handler->total_system_memory;
    }

    AGENTOS_LOG_INFO("[OOM] Pressure Stats: level=%s, usage=%.1f%%, "
            "allocated=%zu/%zu bytes, total_allocated=%zu, denied=%zu",
            oom_pressure_name(handler->current_pressure),
            usage_pct,
            current_allocated, handler->total_system_memory,
            handler->total_allocated_bytes,
            handler->pressure_denied_count);

    /* 各级别回调统计 */
    for (int level = 0; level <= AGENTOS_MEM_PRESSURE_FATAL; level++) {
        int active_count = 0;
        for (int i = 0; i < AGENTOS_PRESSURE_MAX_CALLBACKS; i++) {
            if (handler->pressure_callbacks[level][i].active) {
                active_count++;
            }
        }
        if (active_count > 0) {
            AGENTOS_LOG_INFO("[OOM]   %s callbacks: %d/%d",
                    oom_pressure_name((agentos_mem_pressure_level_t)level),
                    active_count, AGENTOS_PRESSURE_MAX_CALLBACKS);
        }
    }

    agentos_mutex_unlock(&handler->pressure_mutex);
}

/* ============================================================================
 * OOM YAML 配置支持实现（P0.11）
 * ============================================================================ */

/* 当前活跃配置 */
static agentos_oom_config_t g_oom_config;

void agentos_oom_config_defaults(agentos_oom_config_t *config)
{
    if (!config) return;

    config->warning_threshold     = OOM_PRESSURE_THRESHOLD_WARNING;
    config->degraded_threshold    = OOM_PRESSURE_THRESHOLD_DEGRADED;
    config->critical_threshold    = OOM_PRESSURE_THRESHOLD_CRITICAL;
    config->fatal_threshold       = OOM_PRESSURE_THRESHOLD_FATAL;
    config->check_interval_ms     = 1000;
    config->recovery_cooldown_ms  = 5000;
    config->enable_auto_recovery  = true;
    config->enable_allocation_check = true;
    config->emergency_pool_size   = 1024 * 1024; /* 1MB */
    config->max_heap_size         = 0; /* 自动检测 */
}

int agentos_oom_config_load(const char *config_path,
                             agentos_oom_config_t *config)
{
    if (!config_path || !config) return AGENTOS_EINVAL;

    /* 先填充默认值 */
    agentos_oom_config_defaults(config);

    /* 使用 config_unified 加载 YAML 配置并覆盖默认值 */
    config_context_t *cfg_ctx = config_service_create("oom", NULL, false, false);
    if (cfg_ctx) {
        config_file_source_options_t file_opts = {
            .file_path = config_path,
            .format = "yaml"
        };
        config_source_t *source = config_source_create_file(&file_opts);
        if (source) {
            config_error_t err = config_service_load(cfg_ctx, &source, 1);
            if (err == CONFIG_SUCCESS) {
                /* 读取 agentos.oom.* 配置节，覆盖默认值 */
                const config_value_t *val;

                val = config_context_get(cfg_ctx, "agentos.oom.warning_threshold");
                if (val) config->warning_threshold = config_value_get_double(val, config->warning_threshold);

                val = config_context_get(cfg_ctx, "agentos.oom.degraded_threshold");
                if (val) config->degraded_threshold = config_value_get_double(val, config->degraded_threshold);

                val = config_context_get(cfg_ctx, "agentos.oom.critical_threshold");
                if (val) config->critical_threshold = config_value_get_double(val, config->critical_threshold);

                val = config_context_get(cfg_ctx, "agentos.oom.fatal_threshold");
                if (val) config->fatal_threshold = config_value_get_double(val, config->fatal_threshold);

                val = config_context_get(cfg_ctx, "agentos.oom.check_interval_ms");
                if (val) config->check_interval_ms = (uint32_t)config_value_get_int(val, (int)config->check_interval_ms);

                val = config_context_get(cfg_ctx, "agentos.oom.recovery_cooldown_ms");
                if (val) config->recovery_cooldown_ms = (uint32_t)config_value_get_int(val, (int)config->recovery_cooldown_ms);

                val = config_context_get(cfg_ctx, "agentos.oom.enable_auto_recovery");
                if (val) config->enable_auto_recovery = config_value_get_bool(val, config->enable_auto_recovery);

                val = config_context_get(cfg_ctx, "agentos.oom.enable_allocation_check");
                if (val) config->enable_allocation_check = config_value_get_bool(val, config->enable_allocation_check);

                val = config_context_get(cfg_ctx, "agentos.oom.emergency_pool_size");
                if (val) config->emergency_pool_size = (size_t)config_value_get_int(val, (int)config->emergency_pool_size);

                val = config_context_get(cfg_ctx, "agentos.oom.max_heap_size");
                if (val) config->max_heap_size = (size_t)config_value_get_int(val, (int)config->max_heap_size);

                AGENTOS_LOG_INFO("[OOM] Config loaded from: %s", config_path);
            } else {
                AGENTOS_LOG_WARN("[OOM] Failed to parse YAML config: %s, using defaults",
                        config_path);
            }
            config_source_destroy(source);
        } else {
            AGENTOS_LOG_WARN("[OOM] Cannot open config file: %s, using defaults",
                    config_path);
        }
        config_context_destroy(cfg_ctx);
    } else {
        AGENTOS_LOG_WARN("[OOM] Config system unavailable, using defaults: %s",
                config_path);
    }
    return 0;
}

int agentos_oom_config_apply(const agentos_oom_config_t *config)
{
    if (!config) return AGENTOS_EINVAL;
    if (!g_oom_handler) return AGENTOS_ENOTINIT;

    /* 验证阈值合理性 */
    if (config->warning_threshold >= config->degraded_threshold ||
        config->degraded_threshold >= config->critical_threshold ||
        config->critical_threshold >= config->fatal_threshold) {
        AGENTOS_LOG_ERROR("[OOM] Invalid threshold order in config");
        return AGENTOS_EINVAL;
    }

    /* 保存配置 */
    g_oom_config = *config;

    AGENTOS_LOG_INFO("[OOM] Config applied: warning=%.0f%% degraded=%.0f%% "
            "critical=%.0f%% fatal=%.0f%% auto_recovery=%s",
            config->warning_threshold * 100,
            config->degraded_threshold * 100,
            config->critical_threshold * 100,
            config->fatal_threshold * 100,
            config->enable_auto_recovery ? "true" : "false");

    return 0;
}

/* ============================================================================
 * Per-Daemon OOM 回调实现（P0.11）
 * ============================================================================ */

#define OOM_MAX_DAEMON_CALLBACKS 16

static agentos_daemon_oom_registration_t g_daemon_oom_callbacks[OOM_MAX_DAEMON_CALLBACKS];
static size_t g_daemon_oom_count = 0;

int agentos_oom_register_daemon_callback(
    const agentos_daemon_oom_registration_t *reg)
{
    if (!reg || !reg->daemon_name || !reg->callback) return AGENTOS_EINVAL;
    if (g_daemon_oom_count >= OOM_MAX_DAEMON_CALLBACKS) return AGENTOS_EBUSY;

    /* 检查重名 */
    for (size_t i = 0; i < g_daemon_oom_count; i++) {
        if (strcmp(g_daemon_oom_callbacks[i].daemon_name,
                   reg->daemon_name) == 0) {
            /* 更新已有注册 */
            g_daemon_oom_callbacks[i] = *reg;
            return 0;
        }
    }

    g_daemon_oom_callbacks[g_daemon_oom_count] = *reg;
    g_daemon_oom_count++;

    AGENTOS_LOG_INFO("[OOM] Daemon OOM callback registered: %s (priority=%d)",
            reg->daemon_name, reg->priority);
    return 0;
}

int agentos_oom_unregister_daemon_callback(const char *daemon_name)
{
    if (!daemon_name) return AGENTOS_EINVAL;

    for (size_t i = 0; i < g_daemon_oom_count; i++) {
        if (strcmp(g_daemon_oom_callbacks[i].daemon_name,
                   daemon_name) == 0) {
            g_daemon_oom_callbacks[i] =
                g_daemon_oom_callbacks[g_daemon_oom_count - 1];
            g_daemon_oom_count--;
            return 0;
        }
    }
    return AGENTOS_ENOENT;
}

size_t agentos_oom_fire_daemon_callbacks(agentos_mem_pressure_level_t pressure)
{
    if (!g_oom_handler) return 0;

    size_t total_freed = 0;
    size_t current_allocated = 0;
    if (g_oom_handler->ext_stats) {
        current_allocated = g_oom_handler->ext_stats->current_allocated;
    }

    /* 按优先级降序遍历（简化：线性扫描找最高优先级） */
    for (int pass = 0; pass < 2; pass++) {
        for (size_t i = 0; i < g_daemon_oom_count; i++) {
            agentos_daemon_oom_registration_t *reg =
                &g_daemon_oom_callbacks[i];
            if (!reg->enabled) continue;

            /* pass 0: 高优先级(>50), pass 1: 低优先级 */
            bool is_high = reg->priority > 50;
            if ((pass == 0 && !is_high) || (pass == 1 && is_high)) continue;

            size_t freed = reg->callback(
                reg->daemon_name,
                pressure,
                current_allocated,
                g_oom_handler->total_system_memory,
                reg->user_data);

            if (freed > 0) {
                total_freed += freed;
                AGENTOS_LOG_INFO("[OOM] Daemon %s released %zu bytes "
                        "under %s pressure",
                        reg->daemon_name, freed,
                        oom_pressure_name(pressure));
            }
        }
    }

    return total_freed;
}

/* ============================================================================
 * OOM 恢复策略实现（P0.11）
 * ============================================================================ */

#define OOM_MAX_RECOVERY_CALLBACKS 8

static agentos_oom_recovery_cb_t g_recovery_callbacks[OOM_MAX_RECOVERY_CALLBACKS];
static void *g_recovery_user_data[OOM_MAX_RECOVERY_CALLBACKS];
static size_t g_recovery_count = 0;
static uint64_t g_last_recovery_time_ms = 0;

int agentos_oom_register_recovery_callback(
    agentos_oom_recovery_cb_t callback, void *user_data)
{
    if (!callback) return AGENTOS_EINVAL;
    if (g_recovery_count >= OOM_MAX_RECOVERY_CALLBACKS) return AGENTOS_EBUSY;

    g_recovery_callbacks[g_recovery_count] = callback;
    g_recovery_user_data[g_recovery_count] = user_data;
    g_recovery_count++;
    return 0;
}

bool agentos_oom_should_recover(void)
{
    if (!g_oom_handler) return false;

    /* 仅在压力低于 WARNING 时考虑恢复 */
    if (g_oom_handler->current_pressure >= AGENTOS_MEM_PRESSURE_WARNING) {
        return false;
    }

    /* 检查冷却期 */
    uint64_t now = oom_get_time_ms();
    uint64_t cooldown = g_oom_config.recovery_cooldown_ms;
    if (cooldown == 0) cooldown = 5000;

    if (now - g_last_recovery_time_ms < cooldown) {
        return false;
    }

    return true;
}

int agentos_oom_recover(void)
{
    if (!g_oom_handler) return AGENTOS_ENOTINIT;

    agentos_mem_pressure_level_t old_level = g_oom_handler->current_pressure;
    agentos_mem_pressure_level_t new_level = AGENTOS_MEM_PRESSURE_NORMAL;

    g_last_recovery_time_ms = oom_get_time_ms();

    /* 通知所有恢复回调 */
    for (size_t i = 0; i < g_recovery_count; i++) {
        g_recovery_callbacks[i](OOM_RECOVERY_STARTED,
                                old_level, new_level,
                                g_recovery_user_data[i]);
    }

    /* 执行降级处理器的恢复 */
    agentos_oom_degrade(
        (watermark_level_t)old_level,
        (watermark_level_t)new_level);

    /* 通知恢复完成 */
    for (size_t i = 0; i < g_recovery_count; i++) {
        g_recovery_callbacks[i](OOM_RECOVERY_COMPLETED,
                                old_level, new_level,
                                g_recovery_user_data[i]);
    }

    AGENTOS_LOG_INFO("[OOM] Recovery completed: %s → %s",
            oom_pressure_name(old_level),
            oom_pressure_name(new_level));

    return 0;
}