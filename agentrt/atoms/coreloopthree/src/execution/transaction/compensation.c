/**
 * @file compensation.c
 * @brief Compensation Transaction Manager Implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "compensation.h"

#include "agentrt.h"
#include "execution.h"
#include "memory_compat.h"
#include "string_compat.h"

#include <stdlib.h>
#include <string.h>

agentrt_error_t agentrt_compensation_create(agentrt_compensation_t **out_manager)
{
    if (!out_manager)
        return AGENTRT_EINVAL;

    agentrt_compensation_t *mgr =
        (agentrt_compensation_t *)AGENTRT_CALLOC(1, sizeof(agentrt_compensation_t));
    if (!mgr)
        return AGENTRT_ENOMEM;

    mgr->lock = agentrt_mutex_create();
    if (!mgr->lock) {
        AGENTRT_FREE(mgr);
        return AGENTRT_ENOMEM;
    }

    mgr->human_queue_capacity = 16;
    SAFE_MALLOC_ARRAY(mgr->human_queue, mgr->human_queue_capacity, sizeof(char *));
    if (!mgr->human_queue) {
        agentrt_mutex_free(mgr->lock);
        AGENTRT_FREE(mgr);
        return AGENTRT_ENOMEM;
    }

    *out_manager = mgr;
    return AGENTRT_SUCCESS;
}

void agentrt_compensation_destroy(agentrt_compensation_t *manager)
{
    if (!manager)
        return;

    agentrt_mutex_lock(manager->lock);

    agentrt_compensation_entry_t *entry = manager->entries;
    while (entry) {
        agentrt_compensation_entry_t *next = entry->next;
        if (entry->action_id)
            AGENTRT_FREE(entry->action_id);
        if (entry->compensator_id)
            AGENTRT_FREE(entry->compensator_id);
        if (entry->input) {
            if (entry->input_free_fn) {
                entry->input_free_fn(entry->input);
            } else {
                AGENTRT_FREE(entry->input);
            }
        }
        AGENTRT_FREE(entry);
        entry = next;
    }
    manager->entries = NULL;
    manager->entry_count = 0;

    for (size_t i = 0; i < manager->human_queue_size; i++) {
        AGENTRT_FREE(manager->human_queue[i]);
    }
    AGENTRT_FREE(manager->human_queue);
    manager->human_queue = NULL;
    manager->human_queue_size = 0;

    agentrt_mutex_unlock(manager->lock);

    agentrt_mutex_free(manager->lock);
    AGENTRT_FREE(manager);
}

agentrt_error_t agentrt_compensation_register(agentrt_compensation_t *manager,
                                              const char *action_id, const char *compensator_id,
                                              const void *input)
{

    if (!manager || !action_id || !compensator_id)
        return AGENTRT_EINVAL;

    agentrt_compensation_entry_t *entry =
        (agentrt_compensation_entry_t *)AGENTRT_CALLOC(1, sizeof(agentrt_compensation_entry_t));
    if (!entry)
        return AGENTRT_ENOMEM;

    entry->action_id = AGENTRT_STRDUP(action_id);
    entry->compensator_id = AGENTRT_STRDUP(compensator_id);

    if (input) {
        size_t input_len = strlen((const char *)input);
        entry->input = AGENTRT_MALLOC(input_len + 1);
        if (entry->input) {
            __builtin_memcpy(entry->input, input, input_len + 1);
            entry->input_size = input_len + 1;
        }
    }

    if (!entry->action_id || !entry->compensator_id || (input && !entry->input)) {
        if (entry->action_id)
            AGENTRT_FREE(entry->action_id);
        if (entry->compensator_id)
            AGENTRT_FREE(entry->compensator_id);
        if (entry->input)
            AGENTRT_FREE(entry->input);
        AGENTRT_FREE(entry);
        return AGENTRT_ENOMEM;
    }

    agentrt_mutex_lock(manager->lock);
    entry->next = manager->entries;
    manager->entries = entry;
    manager->entry_count++;
    agentrt_mutex_unlock(manager->lock);

    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_compensation_compensate(agentrt_compensation_t *manager,
                                                const char *action_id)
{

    if (!manager || !action_id)
        return AGENTRT_EINVAL;

    agentrt_mutex_lock(manager->lock);

    agentrt_compensation_entry_t **p = &manager->entries;
    agentrt_compensation_entry_t *entry = NULL;
    while (*p) {
        if (strcmp((*p)->action_id, action_id) == 0) {
            entry = *p;
            *p = entry->next;
            break;
        }
        p = &(*p)->next;
    }

    if (!entry) {
        agentrt_mutex_unlock(manager->lock);
        return AGENTRT_ENOENT;
    }

    manager->entry_count--;

    if (manager->human_queue_size >= manager->human_queue_capacity) {
        size_t new_cap = manager->human_queue_capacity * 2;
        char **new_queue = (char **)AGENTRT_REALLOC(manager->human_queue, new_cap * sizeof(char *));
        if (!new_queue) {
            entry->next = manager->entries;
            manager->entries = entry;
            manager->entry_count++;
            agentrt_mutex_unlock(manager->lock);
            return AGENTRT_ENOMEM;
        }
        manager->human_queue = new_queue;
        manager->human_queue_capacity = new_cap;
    }

    manager->human_queue[manager->human_queue_size++] = AGENTRT_STRDUP(entry->action_id);
    if (manager->human_queue[manager->human_queue_size - 1] == NULL) {
        entry->next = manager->entries;
        manager->entries = entry;
        manager->entry_count++;
        agentrt_mutex_unlock(manager->lock);
        return AGENTRT_ENOMEM;
    }

    if (entry->action_id)
        AGENTRT_FREE(entry->action_id);
    if (entry->compensator_id)
        AGENTRT_FREE(entry->compensator_id);
    if (entry->input) {
        if (entry->input_free_fn) {
            entry->input_free_fn(entry->input);
        } else {
            AGENTRT_FREE(entry->input);
        }
    }
    AGENTRT_FREE(entry);

    agentrt_mutex_unlock(manager->lock);
    return AGENTRT_SUCCESS;
}

agentrt_error_t agentrt_compensation_get_human_queue(agentrt_compensation_t *manager,
                                                     char ***out_actions, size_t *out_count)
{

    if (!manager || !out_actions || !out_count)
        return AGENTRT_EINVAL;

    agentrt_mutex_lock(manager->lock);
    *out_count = manager->human_queue_size;
    if (*out_count == 0) {
        *out_actions = NULL;
        agentrt_mutex_unlock(manager->lock);
        return AGENTRT_SUCCESS;
    }

    char **actions;
    SAFE_MALLOC_ARRAY(actions, *out_count, sizeof(char *));
    if (!actions) {
        agentrt_mutex_unlock(manager->lock);
        return AGENTRT_ENOMEM;
    }

    for (size_t i = 0; i < *out_count; i++) {
        actions[i] = AGENTRT_STRDUP(manager->human_queue[i]);
        if (!actions[i]) {
            for (size_t j = 0; j < i; j++)
                AGENTRT_FREE(actions[j]);
            AGENTRT_FREE(actions);
            agentrt_mutex_unlock(manager->lock);
            return AGENTRT_ENOMEM;
        }
    }

    for (size_t i = 0; i < manager->human_queue_size; i++) {
        AGENTRT_FREE(manager->human_queue[i]);
    }
    manager->human_queue_size = 0;

    agentrt_mutex_unlock(manager->lock);

    *out_actions = actions;
    return AGENTRT_SUCCESS;
}

void agentrt_compensation_result_free(agentrt_compensation_result_t *result)
{
    if (!result)
        return;
    if (result->error_message) {
        AGENTRT_FREE(result->error_message);
    }
}
