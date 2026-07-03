/**
 * @file registry.c
 * @brief 执行单元注册表独立实?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentos.h"
#include "atomic_compat.h"
#include "execution.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentos_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentos_error_str(c)); return (c); } while(0)


/**
 * @brief 注册表条?
 */
typedef struct registry_entry {
    char *unit_id;                  /**< 单元唯一标识（对?agent_id?*/
    agentos_execution_unit_t *unit; /**< 执行单元对象 */
    struct registry_entry *next;    /**< 链表下一?*/
} registry_entry_t;

static registry_entry_t *g_registry = NULL;
static agentos_mutex_t *g_registry_lock = NULL;

static agentos_error_t ensure_registry_init(void)
{
    agentos_mutex_t *current =
        (agentos_mutex_t *)atomic_load_ptr((_Atomic void **)&g_registry_lock, memory_order_acquire);
    if (current)
        return AGENTOS_SUCCESS;

    agentos_mutex_t *new_lock = agentos_mutex_create();
    if (!new_lock)
        ATM_RET_ERR(AGENTOS_ENOMEM);

    agentos_mutex_t *expected = NULL;
    if (!atomic_compare_exchange_strong_ptr((_Atomic void **)&g_registry_lock, (void **)&expected,
                                            (void *)new_lock, memory_order_acq_rel,
                                            memory_order_acquire)) {
        agentos_mutex_free(new_lock);
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 初始化注册表（需在程序启动时调用?
 */
agentos_error_t agentos_registry_init(void)
{
    return ensure_registry_init();
}

/**
 * @brief 清理注册?
 */
void agentos_registry_cleanup(void)
{
    agentos_mutex_t *lock =
        (agentos_mutex_t *)atomic_load_ptr((_Atomic void **)&g_registry_lock, memory_order_acquire);
    if (!lock)
        return;
    agentos_mutex_lock(lock);
    registry_entry_t *entry = g_registry;
    while (entry) {
        registry_entry_t *next = entry->next;
        if (entry->unit_id)
            AGENTOS_FREE(entry->unit_id);
        if (entry->unit) {}
        AGENTOS_FREE(entry);
        entry = next;
    }
    g_registry = NULL;
    agentos_mutex_unlock(lock);
    agentos_mutex_free(lock);
    atomic_store_ptr((_Atomic void **)&g_registry_lock, NULL, memory_order_release);
}

/* _take: caller transfers ownership */
agentos_error_t agentos_registry_register_unit_take(const char *unit_id, agentos_execution_unit_t *unit)
{
    if (!unit_id || !unit)
        ATM_RET_ERR(AGENTOS_EINVAL);

    agentos_error_t err = ensure_registry_init();
    if (err != AGENTOS_SUCCESS)
        return err;

    agentos_mutex_lock(g_registry_lock);

    // 检查是否已存在同名单元
    registry_entry_t *entry = g_registry;
    while (entry) {
        if (strcmp(entry->unit_id, unit_id) == 0) {
            agentos_mutex_unlock(g_registry_lock);
            ATM_RET_ERR(AGENTOS_EEXIST);
        }
        entry = entry->next;
    }

    // 创建新条?
    entry = (registry_entry_t *)AGENTOS_MALLOC(sizeof(registry_entry_t));
    if (!entry) {
        agentos_mutex_unlock(g_registry_lock);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }
    entry->unit_id = AGENTOS_STRDUP(unit_id);
    entry->unit = unit;
    if (!entry->unit_id) {
        AGENTOS_FREE(entry);
        agentos_mutex_unlock(g_registry_lock);
        ATM_RET_ERR(AGENTOS_ENOMEM);
    }

    entry->next = g_registry;
    g_registry = entry;

    agentos_mutex_unlock(g_registry_lock);
    return AGENTOS_SUCCESS;
}

void agentos_registry_unregister_unit(const char *unit_id)
{
    if (!unit_id)
        return;
    if (ensure_registry_init() != AGENTOS_SUCCESS)
        return;
    agentos_mutex_lock(g_registry_lock);
    registry_entry_t **p = &g_registry;
    while (*p) {
        if (strcmp((*p)->unit_id, unit_id) == 0) {
            registry_entry_t *tmp = *p;
            *p = tmp->next;
            AGENTOS_FREE(tmp->unit_id);
            AGENTOS_FREE(tmp);
            break;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(g_registry_lock);
}

agentos_execution_unit_t *agentos_registry_get_unit(const char *unit_id)
{
    if (!unit_id) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        }
    if (ensure_registry_init() != AGENTOS_SUCCESS) {
        AGENTOS_ERROR_NULL(AGENTOS_ERR_INVALID_PARAM, "null parameter");
        }
    agentos_mutex_lock(g_registry_lock);
    registry_entry_t *entry = g_registry;
    while (entry) {
        if (strcmp(entry->unit_id, unit_id) == 0) {
            agentos_execution_unit_t *unit = entry->unit;
            agentos_mutex_unlock(g_registry_lock);
            return unit;
        }
        entry = entry->next;
    }
    agentos_mutex_unlock(g_registry_lock);
    AGENTOS_ERROR_NULL(AGENTOS_ERR_UNKNOWN, "operation failed");
}
