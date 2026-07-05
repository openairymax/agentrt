/**
 * @file registry.c
 * @brief 执行单元注册表独立实?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "agentrt.h"
#include "atomic_compat.h"
#include "execution.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdlib.h>
#include <string.h>
#include "error.h"
#include "error_compat.h"

#define ATM_RET_ERR(c) \
    do { agentrt_error_push_ex((c), __FILE__, __LINE__, __func__, "%s", agentrt_error_str(c)); return (c); } while(0)


/**
 * @brief 注册表条?
 */
typedef struct registry_entry {
    char *unit_id;                  /**< 单元唯一标识（对?agent_id?*/
    agentrt_execution_unit_t *unit; /**< 执行单元对象 */
    struct registry_entry *next;    /**< 链表下一?*/
} registry_entry_t;

static registry_entry_t *g_registry = NULL;
static agentrt_mutex_t *g_registry_lock = NULL;

static agentrt_error_t ensure_registry_init(void)
{
    agentrt_mutex_t *current =
        (agentrt_mutex_t *)atomic_load_ptr((_Atomic void **)&g_registry_lock, memory_order_acquire);
    if (current)
        return AGENTRT_SUCCESS;

    agentrt_mutex_t *new_lock = agentrt_mutex_create();
    if (!new_lock)
        ATM_RET_ERR(AGENTRT_ENOMEM);

    agentrt_mutex_t *expected = NULL;
    if (!atomic_compare_exchange_strong_ptr((_Atomic void **)&g_registry_lock, (void **)&expected,
                                            (void *)new_lock, memory_order_acq_rel,
                                            memory_order_acquire)) {
        agentrt_mutex_free(new_lock);
    }
    return AGENTRT_SUCCESS;
}

/**
 * @brief 初始化注册表（需在程序启动时调用?
 */
agentrt_error_t agentrt_registry_init(void)
{
    return ensure_registry_init();
}

/**
 * @brief 清理注册?
 */
void agentrt_registry_cleanup(void)
{
    agentrt_mutex_t *lock =
        (agentrt_mutex_t *)atomic_load_ptr((_Atomic void **)&g_registry_lock, memory_order_acquire);
    if (!lock)
        return;
    agentrt_mutex_lock(lock);
    registry_entry_t *entry = g_registry;
    while (entry) {
        registry_entry_t *next = entry->next;
        if (entry->unit_id)
            AGENTRT_FREE(entry->unit_id);
        if (entry->unit) {}
        AGENTRT_FREE(entry);
        entry = next;
    }
    g_registry = NULL;
    agentrt_mutex_unlock(lock);
    agentrt_mutex_free(lock);
    atomic_store_ptr((_Atomic void **)&g_registry_lock, NULL, memory_order_release);
}

/* _take: caller transfers ownership */
agentrt_error_t agentrt_registry_register_unit_take(const char *unit_id, agentrt_execution_unit_t *unit)
{
    if (!unit_id || !unit)
        ATM_RET_ERR(AGENTRT_EINVAL);

    agentrt_error_t err = ensure_registry_init();
    if (err != AGENTRT_SUCCESS)
        return err;

    agentrt_mutex_lock(g_registry_lock);

    // 检查是否已存在同名单元
    registry_entry_t *entry = g_registry;
    while (entry) {
        if (strcmp(entry->unit_id, unit_id) == 0) {
            agentrt_mutex_unlock(g_registry_lock);
            ATM_RET_ERR(AGENTRT_EEXIST);
        }
        entry = entry->next;
    }

    // 创建新条?
    entry = (registry_entry_t *)AGENTRT_MALLOC(sizeof(registry_entry_t));
    if (!entry) {
        agentrt_mutex_unlock(g_registry_lock);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }
    entry->unit_id = AGENTRT_STRDUP(unit_id);
    entry->unit = unit;
    if (!entry->unit_id) {
        AGENTRT_FREE(entry);
        agentrt_mutex_unlock(g_registry_lock);
        ATM_RET_ERR(AGENTRT_ENOMEM);
    }

    entry->next = g_registry;
    g_registry = entry;

    agentrt_mutex_unlock(g_registry_lock);
    return AGENTRT_SUCCESS;
}

void agentrt_registry_unregister_unit(const char *unit_id)
{
    if (!unit_id)
        return;
    if (ensure_registry_init() != AGENTRT_SUCCESS)
        return;
    agentrt_mutex_lock(g_registry_lock);
    registry_entry_t **p = &g_registry;
    while (*p) {
        if (strcmp((*p)->unit_id, unit_id) == 0) {
            registry_entry_t *tmp = *p;
            *p = tmp->next;
            AGENTRT_FREE(tmp->unit_id);
            AGENTRT_FREE(tmp);
            break;
        }
        p = &(*p)->next;
    }
    agentrt_mutex_unlock(g_registry_lock);
}

agentrt_execution_unit_t *agentrt_registry_get_unit(const char *unit_id)
{
    if (!unit_id) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    if (ensure_registry_init() != AGENTRT_SUCCESS) {
        AGENTRT_ERROR_NULL(AGENTRT_ERR_INVALID_PARAM, "null parameter");
        }
    agentrt_mutex_lock(g_registry_lock);
    registry_entry_t *entry = g_registry;
    while (entry) {
        if (strcmp(entry->unit_id, unit_id) == 0) {
            agentrt_execution_unit_t *unit = entry->unit;
            agentrt_mutex_unlock(g_registry_lock);
            return unit;
        }
        entry = entry->next;
    }
    agentrt_mutex_unlock(g_registry_lock);
    AGENTRT_ERROR_NULL(AGENTRT_ERR_UNKNOWN, "operation failed");
}
