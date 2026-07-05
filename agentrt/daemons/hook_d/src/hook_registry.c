/**
 * @file hook_registry.c
 * @brief P2.1.1: Hook 注册表实现 — 按事件分类，优先级排序，线程安全
 *
 * 使用每个 Hook 类型一个排序链表，插入时按优先级降序插入。
 * 全局读写锁保护注册表，支持并发读/串行写。
 *
 * Copyright (C) 2025-2026 SPHARX Ltd. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later OR Apache-2.0
 */

#include "hook_registry.h"
#include "memory_compat.h"
#include "sync_compat.h"

#include <string.h>

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 注册表链表节点
 */
typedef struct hook_node {
    hook_entry_t entry;            /**< Hook 条目 */
    struct hook_node *next;       /**< 下一个节点（单链表） */
} hook_node_t;

/**
 * @brief 全局注册表（单例）
 */
static struct {
    hook_node_t *heads[HOOK_TYPE_COUNT]; /**< 每种 Hook 类型的链表头 */
    size_t count;                        /**< 总注册数 */
    sync_rwlock_t rwlock;               /**< 读写锁 */
    bool initialized;                    /**< 是否已初始化 */
} g_hook_registry;

/* ==================== 生命周期 ==================== */

int hook_registry_init(void)
{
    if (g_hook_registry.initialized)
        return 0;

    AGENTRT_MEMSET(&g_hook_registry, 0, sizeof(g_hook_registry));

    if (AGENTRT_RWLOCK_INIT(&g_hook_registry.rwlock, NULL) != 0) {
        return -1;
    }

    g_hook_registry.initialized = true;
    return 0;
}

void hook_registry_destroy(void)
{
    if (!g_hook_registry.initialized)
        return;

    AGENTRT_RWLOCK_WRLOCK(&g_hook_registry.rwlock);

    for (int i = 0; i < HOOK_TYPE_COUNT; i++) {
        hook_node_t *node = g_hook_registry.heads[i];
        while (node) {
            hook_node_t *next = node->next;
            AGENTRT_FREE(node);
            node = next;
        }
        g_hook_registry.heads[i] = NULL;
    }
    g_hook_registry.count = 0;

    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
    AGENTRT_RWLOCK_DESTROY(&g_hook_registry.rwlock);
    g_hook_registry.initialized = false;
}

/* ==================== 查找辅助函数 ==================== */

/**
 * @brief 在指定类型的链表中按名称查找节点
 * @return 节点指针，未找到返回 NULL（调用者需持有读锁）
 */
static hook_node_t *find_node_by_name(hook_type_t type, const char *name)
{
    if (!name || (unsigned)type >= HOOK_TYPE_COUNT)
        return NULL;

    hook_node_t *node = g_hook_registry.heads[type];
    while (node) {
        if (strcmp(node->entry.name, name) == 0)
            return node;
        node = node->next;
    }
    return NULL;
}

/**
 * @brief 按名称全局查找节点（遍历所有类型）
 * @return 节点指针，未找到返回 NULL（调用者需持有读锁）
 */
static hook_node_t *find_node_global(const char *name)
{
    if (!name) return NULL;
    for (int i = 0; i < HOOK_TYPE_COUNT; i++) {
        hook_node_t *node = find_node_by_name((hook_type_t)i, name);
        if (node) return node;
    }
    return NULL;
}

/* ==================== 注册 / 注销 ==================== */

int hook_registry_register(const hook_entry_t *entry)
{
    if (!entry || !entry->name[0] || (unsigned)entry->type >= HOOK_TYPE_COUNT)
        return -1;

    if (g_hook_registry.count >= HOOK_REGISTRY_MAX)
        return -2;

    /* 检查重名 */
    AGENTRT_RWLOCK_RDLOCK(&g_hook_registry.rwlock);
    hook_node_t *existing = find_node_global(entry->name);
    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);

    if (existing)
        return -3;

    /* 分配新节点 */
    hook_node_t *new_node = (hook_node_t *)AGENTRT_CALLOC(1, sizeof(hook_node_t));
    if (!new_node)
        return -1;

    AGENTRT_MEMCPY(&new_node->entry, entry, sizeof(hook_entry_t));
    AGENTRT_STRNCPY_TERM(new_node->entry.name, entry->name,
                         sizeof(new_node->entry.name));

    /* 按优先级降序插入链表 */
    AGENTRT_RWLOCK_WRLOCK(&g_hook_registry.rwlock);

    hook_node_t **prev = &g_hook_registry.heads[entry->type];
    while (*prev && (*prev)->entry.priority >= entry->priority) {
        prev = &(*prev)->next;
    }
    new_node->next = *prev;
    *prev = new_node;

    g_hook_registry.count++;

    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);

    return 0;
}

int hook_registry_unregister(const char *name)
{
    if (!name) return -1;

    AGENTRT_RWLOCK_WRLOCK(&g_hook_registry.rwlock);

    for (int i = 0; i < HOOK_TYPE_COUNT; i++) {
        hook_node_t **prev = &g_hook_registry.heads[i];
        while (*prev) {
            if (strcmp((*prev)->entry.name, name) == 0) {
                hook_node_t *to_free = *prev;
                *prev = to_free->next;
                AGENTRT_FREE(to_free);
                g_hook_registry.count--;
                AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
                return 0;
            }
            prev = &(*prev)->next;
        }
    }

    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
    return -1;
}

/* ==================== 查询 ==================== */

int hook_registry_get_by_type(hook_type_t type,
                               hook_entry_t **out_entries,
                               size_t max_count, size_t *out_count)
{
    if (!out_entries || !out_count || (unsigned)type >= HOOK_TYPE_COUNT)
        return -1;

    *out_count = 0;

    AGENTRT_RWLOCK_RDLOCK(&g_hook_registry.rwlock);

    hook_node_t *node = g_hook_registry.heads[type];
    while (node && *out_count < max_count) {
        if (node->entry.enabled) {
            out_entries[*out_count] = &node->entry;
            (*out_count)++;
        }
        node = node->next;
    }

    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
    return 0;
}

const hook_entry_t *hook_registry_find(const char *name)
{
    if (!name) return NULL;

    AGENTRT_RWLOCK_RDLOCK(&g_hook_registry.rwlock);
    hook_node_t *node = find_node_global(name);
    const hook_entry_t *entry = node ? &node->entry : NULL;
    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);

    return entry;
}

int hook_registry_set_enabled(const char *name, bool enabled)
{
    if (!name) return -1;

    AGENTRT_RWLOCK_WRLOCK(&g_hook_registry.rwlock);
    hook_node_t *node = find_node_global(name);
    if (!node) {
        AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
        return -1;
    }
    node->entry.enabled = enabled;
    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
    return 0;
}

size_t hook_registry_count(void)
{
    return g_hook_registry.count;
}

size_t hook_registry_count_by_type(hook_type_t type)
{
    if ((unsigned)type >= HOOK_TYPE_COUNT) return 0;

    size_t count = 0;
    AGENTRT_RWLOCK_RDLOCK(&g_hook_registry.rwlock);
    hook_node_t *node = g_hook_registry.heads[type];
    while (node) {
        count++;
        node = node->next;
    }
    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
    return count;
}

/* ==================== 统计 ==================== */

int hook_registry_update_stats(const char *name, hook_decision_t decision,
                                uint64_t duration_ns)
{
    if (!name) return -1;

    AGENTRT_RWLOCK_WRLOCK(&g_hook_registry.rwlock);
    hook_node_t *node = find_node_global(name);
    if (!node) {
        AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
        return -1;
    }

    node->entry.invoke_count++;
    node->entry.total_duration_ns += duration_ns;
    if (duration_ns > node->entry.max_duration_ns) {
        node->entry.max_duration_ns = duration_ns;
    }

    switch (decision) {
    case HOOK_DECISION_CONTINUE: break;
    case HOOK_DECISION_SKIP:     node->entry.skip_count++;   break;
    case HOOK_DECISION_RETRY:    node->entry.retry_count++;  break;
    case HOOK_DECISION_ABORT:    node->entry.abort_count++;  break;
    case HOOK_DECISION_MODIFY:   node->entry.modify_count++; break;
    }

    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
    return 0;
}

int hook_registry_get_stats(const char *name, hook_stats_t *stats)
{
    if (!name || !stats) return -1;

    AGENTRT_RWLOCK_RDLOCK(&g_hook_registry.rwlock);
    hook_node_t *node = find_node_global(name);
    if (!node) {
        AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
        return -1;
    }

    stats->invoke_count      = node->entry.invoke_count;
    stats->skip_count        = node->entry.skip_count;
    stats->abort_count       = node->entry.abort_count;
    stats->retry_count       = node->entry.retry_count;
    stats->modify_count      = node->entry.modify_count;
    stats->total_duration_ns = node->entry.total_duration_ns;
    stats->max_duration_ns   = node->entry.max_duration_ns;

    AGENTRT_RWLOCK_UNLOCK(&g_hook_registry.rwlock);
    return 0;
}