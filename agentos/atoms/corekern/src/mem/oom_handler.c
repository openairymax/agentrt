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
#include "string_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 内部常量
 * ============================================================================ */

#define OOM_TRACKER_CAPACITY 1024  /**< 分配跟踪环形缓冲区容量 */
#define OOM_DEFAULT_SYSTEM_MEMORY (512ULL * 1024 * 1024)  /**< 默认系统内存 512MB */

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
static const char *oom_watermark_name(watermark_level_t level)
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

    g_oom_handler = handler;

    fprintf(stderr, "[OOM] OOM handler initialized: system_memory=%zu bytes, "
            "tracker_capacity=%d\n",
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

    AGENTOS_FREE(handler);
    g_oom_handler = NULL;

    fprintf(stderr, "[OOM] OOM handler destroyed\n");
}

oom_handler_t *agentos_oom_get_handler(void)
{
    return g_oom_handler;
}

int agentos_oom_determine_response(watermark_level_t level)
{
    switch (level) {
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

    fprintf(stderr, "[OOM] OOM event #%llu: requested=%zu, available=%zu, "
            "watermark=%s, response=%s\n",
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
        fprintf(stderr, "[OOM] FATAL: terminating process. "
                "requested=%zu, available=%zu, total_events=%llu\n",
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

    fprintf(stderr, "[OOM] Degradation handler registered: %s (trigger=%s)\n",
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

            fprintf(stderr, "[OOM] Degradation handler unregistered: %s\n",
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
                fprintf(stderr, "[OOM] Degrading: %s (level %s → %s)\n",
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
                fprintf(stderr, "[OOM] Restoring: %s (level %s → %s)\n",
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

        fprintf(stderr, "[OOM] Watermark changed: %s → %s "
                "(allocated=%zu/%zu, %.1f%%)\n",
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