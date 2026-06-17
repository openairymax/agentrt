/**
 * @file service.c
 * @brief Hook 服务实现骨架
 *
 * @owner team-A
 */

#include "hook_service.h"

#include <string.h>

/* ── Hook 注册表 ── */

#define HOOK_MAX_REGISTRATIONS 64

static hook_registration_t g_hooks[HOOK_MAX_REGISTRATIONS];
static size_t g_hook_count = 0;

/* ── 服务 API 实现 ── */

int hook_service_register(const hook_registration_t *reg) {
    if (!reg || !reg->name || !reg->callback) return -1;
    if (g_hook_count >= HOOK_MAX_REGISTRATIONS) return -1;

    /* 检查重名 */
    for (size_t i = 0; i < g_hook_count; i++) {
        if (strcmp(g_hooks[i].name, reg->name) == 0) return -1;
    }

    g_hooks[g_hook_count] = *reg;
    g_hook_count++;
    return 0;
}

int hook_service_unregister(const char *name) {
    if (!name) return -1;

    for (size_t i = 0; i < g_hook_count; i++) {
        if (strcmp(g_hooks[i].name, name) == 0) {
            /* 移动最后一个到当前位置 */
            g_hooks[i] = g_hooks[g_hook_count - 1];
            g_hook_count--;
            return 0;
        }
    }
    return -1;
}

hook_decision_t hook_service_fire(hook_context_t *ctx) {
    if (!ctx) return HOOK_DECISION_CONTINUE;

    hook_decision_t result = HOOK_DECISION_CONTINUE;

    /* 按优先级排序遍历（简化实现：线性扫描） */
    for (size_t i = 0; i < g_hook_count; i++) {
        if (!g_hooks[i].enabled) continue;
        if (g_hooks[i].type != ctx->type) continue;

        hook_decision_t decision = g_hooks[i].callback(ctx);

        /* 最严格决策优先 */
        if (decision > result) {
            result = decision;
        }

        /* ABORT 立即返回 */
        if (decision == HOOK_DECISION_ABORT) break;
    }

    return result;
}

int hook_service_get_stats(const char *name, hook_stats_t *stats) {
    (void)name;
    if (!stats) return -1;
    memset(stats, 0, sizeof(*stats));
    /* TODO: Phase 2 实现 - 收集实际统计数据 */
    return 0;
}

int hook_service_set_enabled(const char *name, bool enabled) {
    if (!name) return -1;

    for (size_t i = 0; i < g_hook_count; i++) {
        if (strcmp(g_hooks[i].name, name) == 0) {
            g_hooks[i].enabled = enabled;
            return 0;
        }
    }
    return -1;
}
