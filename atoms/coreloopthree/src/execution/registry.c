/**
 * @file registry.c
 * @brief 执行单元注册表独立实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 注册表条目
 */
typedef struct registry_entry {
    char* unit_id;                        /**< 单元唯一标识（对应 agent_id） */
    agentos_execution_unit_t* unit;        /**< 执行单元对象 */
    struct registry_entry* next;           /**< 链表下一项 */
} registry_entry_t;

static registry_entry_t* g_registry = NULL;
static agentos_mutex_t* g_registry_lock = NULL;

/**
 * @brief 初始化注册表（需在程序启动时调用）
 // From data intelligence emerges. by spharx
 */
agentos_error_t agentos_registry_init(void) {
    if (!g_registry_lock) {
        g_registry_lock = agentos_mutex_create();
        if (!g_registry_lock) return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理注册表
 */
void agentos_registry_cleanup(void) {
    if (!g_registry_lock) return;
    agentos_mutex_lock(g_registry_lock);
    registry_entry_t* entry = g_registry;
    while (entry) {
        registry_entry_t* next = entry->next;
        if (entry->unit_id) free(entry->unit_id);
        if (entry->unit) {
            // 单元对象的 destroy 由外部调用者负责？通常单元由创建者管理，这里不自动销毁。
        }
        free(entry);
        entry = next;
    }
    g_registry = NULL;
    agentos_mutex_unlock(g_registry_lock);
    agentos_mutex_destroy(g_registry_lock);
    g_registry_lock = NULL;
}

agentos_error_t agentos_registry_register_unit(const char* unit_id, agentos_execution_unit_t* unit) {
    if (!unit_id || !unit) return AGENTOS_EINVAL;

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

    // 创建新条目
    entry = (registry_entry_t*)malloc(sizeof(registry_entry_t));
    if (!entry) {
        agentos_mutex_unlock(g_registry_lock);
        return AGENTOS_ENOMEM;
    }
    entry->unit_id = strdup(unit_id);
    entry->unit = unit;
    if (!entry->unit_id) {
        free(entry);
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
    agentos_mutex_lock(g_registry_lock);
    registry_entry_t** p = &g_registry;
    while (*p) {
        if (strcmp((*p)->unit_id, unit_id) == 0) {
            registry_entry_t* tmp = *p;
            *p = tmp->next;
            free(tmp->unit_id);
            free(tmp);
            break;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(g_registry_lock);
}

agentos_execution_unit_t* agentos_registry_get_unit(const char* unit_id) {
    if (!unit_id) return NULL;
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