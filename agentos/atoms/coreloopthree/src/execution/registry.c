/**
 * @file registry.c
 * @brief 执行单元注册表独立实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>

/**
 * @brief 注册表条�?
 */
typedef struct registry_entry {
    char* unit_id;                        /**< 单元唯一标识（对�?agent_id�?*/
    agentos_execution_unit_t* unit;        /**< 执行单元对象 */
    struct registry_entry* next;           /**< 链表下一�?*/
} registry_entry_t;

static registry_entry_t* g_registry = NULL;
static agentos_mutex_t* g_registry_lock = NULL;
static volatile int g_registry_initialized = 0;

/**
 * @brief 确保注册表已初始化（线程安全）
 */
static agentos_error_t ensure_registry_init(void) {
    if (g_registry_initialized) return AGENTOS_SUCCESS;
    if (!g_registry_lock) {
        g_registry_lock = agentos_mutex_create();
        if (!g_registry_lock) return AGENTOS_ENOMEM;
    }
    g_registry_initialized = 1;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 初始化注册表（需在程序启动时调用�?
 */
agentos_error_t agentos_registry_init(void) {
    if (!g_registry_lock) {
        g_registry_lock = agentos_mutex_create();
        if (!g_registry_lock) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理注册�?
 */
void agentos_registry_cleanup(void) {
    if (!g_registry_lock) return;
    agentos_mutex_lock(g_registry_lock);
    registry_entry_t* entry = g_registry;
    while (entry) {
        registry_entry_t* next = entry->next;
        if (entry->unit_id) AGENTOS_FREE(entry->unit_id);
        if (entry->unit) {
            // 单元对象�?destroy 由外部调用者负责？通常单元由创建者管理，这里不自动销毁�?
        }
        AGENTOS_FREE(entry);
        entry = next;
    }
    g_registry = NULL;
    agentos_mutex_unlock(g_registry_lock);
    agentos_mutex_destroy(g_registry_lock);
    g_registry_lock = NULL;
}

agentos_error_t agentos_registry_register_unit(const char* unit_id, agentos_execution_unit_t* unit) {
    if (!unit_id || !unit) return AGENTOS_EINVAL;

    agentos_error_t err = ensure_registry_init();
    if (err != AGENTOS_SUCCESS) return err;

    agentos_mutex_lock(g_registry_lock);

    // 检查是否已存在同名单元
    registry_entry_t* entry = g_registry;
    while (entry) {
        if (strcmp(entry->unit_id, unit_id) == 0) {
            agentos_mutex_unlock(g_registry_lock);
            return AGENTOS_EEXIST;
        }
        entry = entry->next;
    }

    // 创建新条�?
    entry = (registry_entry_t*)AGENTOS_MALLOC(sizeof(registry_entry_t));
    if (!entry) {
        agentos_mutex_unlock(g_registry_lock);
        return AGENTOS_ENOMEM;
    }
    entry->unit_id = AGENTOS_STRDUP(unit_id);
    entry->unit = unit;
    if (!entry->unit_id) {
        AGENTOS_FREE(entry);
        agentos_mutex_unlock(g_registry_lock);
        return AGENTOS_ENOMEM;
    }

    entry->next = g_registry;
    g_registry = entry;

    agentos_mutex_unlock(g_registry_lock);
    return AGENTOS_SUCCESS;
}

void agentos_registry_unregister_unit(const char* unit_id) {
    if (!unit_id) return;
    if (ensure_registry_init() != AGENTOS_SUCCESS) return;
    agentos_mutex_lock(g_registry_lock);
    registry_entry_t** p = &g_registry;
    while (*p) {
        if (strcmp((*p)->unit_id, unit_id) == 0) {
            registry_entry_t* tmp = *p;
            *p = tmp->next;
            AGENTOS_FREE(tmp->unit_id);
            AGENTOS_FREE(tmp);
            break;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(g_registry_lock);
}

agentos_execution_unit_t* agentos_registry_get_unit(const char* unit_id) {
    if (!unit_id) return NULL;
    if (ensure_registry_init() != AGENTOS_SUCCESS) return NULL;
    agentos_mutex_lock(g_registry_lock);
    registry_entry_t* entry = g_registry;
    while (entry) {
        if (strcmp(entry->unit_id, unit_id) == 0) {
            agentos_execution_unit_t* unit = entry->unit;
            agentos_mutex_unlock(g_registry_lock);
            return unit;
        }
        entry = entry->next;
    }
    agentos_mutex_unlock(g_registry_lock);
    return NULL;
}
