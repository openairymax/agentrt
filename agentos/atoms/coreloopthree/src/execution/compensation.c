/**
 * @file compensation.c
 * @brief Compensation Transaction Manager Implementation
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include "compensation.h"
#include <stdlib.h>
#include <string.h>

#include <agentos/memory.h>
#include <agentos/string.h>

agentos_error_t agentos_compensation_create(agentos_compensation_t** out_manager) {
    if (!out_manager) return AGENTOS_EINVAL;

    agentos_compensation_t* mgr = (agentos_compensation_t*)AGENTOS_CALLOC(1, sizeof(agentos_compensation_t));
    if (!mgr) return AGENTOS_ENOMEM;

    mgr->lock = agentos_mutex_create();
    if (!mgr->lock) {
        AGENTOS_FREE(mgr);
        return AGENTOS_ENOMEM;
    }

    mgr->human_queue_capacity = 16;
    mgr->human_queue = (char**)AGENTOS_MALLOC(mgr->human_queue_capacity * sizeof(char*));
    if (!mgr->human_queue) {
        agentos_mutex_destroy(mgr->lock);
        AGENTOS_FREE(mgr);
        return AGENTOS_ENOMEM;
    }

    *out_manager = mgr;
    return AGENTOS_SUCCESS;
}

void agentos_compensation_destroy(agentos_compensation_t* manager) {
    if (!manager) return;

    agentos_mutex_lock(manager->lock);

    agentos_compensation_entry_t* entry = manager->entries;
    while (entry) {
        agentos_compensation_entry_t* next = entry->next;
        if (entry->action_id) AGENTOS_FREE(entry->action_id);
        if (entry->compensator_id) AGENTOS_FREE(entry->compensator_id);
        if (entry->input) {
            if (entry->input_free_fn) {
                entry->input_free_fn(entry->input);
            } else {
                AGENTOS_FREE(entry->input);
            }
        }
        AGENTOS_FREE(entry);
        entry = next;
    }
    manager->entries = NULL;
    manager->entry_count = 0;

    for (size_t i = 0; i < manager->human_queue_size; i++) {
        AGENTOS_FREE(manager->human_queue[i]);
    }
    AGENTOS_FREE(manager->human_queue);
    manager->human_queue = NULL;
    manager->human_queue_size = 0;

    agentos_mutex_unlock(manager->lock);

    agentos_mutex_destroy(manager->lock);
    AGENTOS_FREE(manager);
}

agentos_error_t agentos_compensation_register(
    agentos_compensation_t* manager,
    const char* action_id,
    const char* compensator_id,
    const void* input) {

    if (!manager || !action_id || !compensator_id) return AGENTOS_EINVAL;

    agentos_compensation_entry_t* entry = (agentos_compensation_entry_t*)AGENTOS_CALLOC(1, sizeof(agentos_compensation_entry_t));
    if (!entry) return AGENTOS_ENOMEM;

    entry->action_id = AGENTOS_STRDUP(action_id);
    entry->compensator_id = AGENTOS_STRDUP(compensator_id);

    if (input) {
        size_t input_len = strnlen((const char*)input, 65536);
        entry->input = AGENTOS_MALLOC(input_len + 1);
        if (entry->input) {
            memcpy(entry->input, input, input_len + 1);
            entry->input_size = input_len + 1;
        }
    }

    if (!entry->action_id || !entry->compensator_id || (input && !entry->input)) {
        if (entry->action_id) AGENTOS_FREE(entry->action_id);
        if (entry->compensator_id) AGENTOS_FREE(entry->compensator_id);
        if (entry->input) AGENTOS_FREE(entry->input);
        AGENTOS_FREE(entry);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(manager->lock);
    entry->next = manager->entries;
    manager->entries = entry;
    manager->entry_count++;
    agentos_mutex_unlock(manager->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_compensation_compensate(
    agentos_compensation_t* manager,
    const char* action_id) {

    if (!manager || !action_id) return AGENTOS_EINVAL;

    agentos_mutex_lock(manager->lock);

    agentos_compensation_entry_t** p = &manager->entries;
    agentos_compensation_entry_t* entry = NULL;
    while (*p) {
        if (strcmp((*p)->action_id, action_id) == 0) {
            entry = *p;
            *p = entry->next;
            break;
        }
        p = &(*p)->next;
    }

    if (!entry) {
        agentos_mutex_unlock(manager->lock);
        return AGENTOS_ENOENT;
    }

    manager->entry_count--;

    if (manager->human_queue_size >= manager->human_queue_capacity) {
        size_t new_cap = manager->human_queue_capacity * 2;
        char** new_queue = (char**)AGENTOS_REALLOC(manager->human_queue, new_cap * sizeof(char*));
        if (!new_queue) {
            entry->next = manager->entries;
            manager->entries = entry;
            manager->entry_count++;
            agentos_mutex_unlock(manager->lock);
            return AGENTOS_ENOMEM;
        }
        manager->human_queue = new_queue;
        manager->human_queue_capacity = new_cap;
    }

    manager->human_queue[manager->human_queue_size++] = AGENTOS_STRDUP(entry->action_id);
    if (manager->human_queue[manager->human_queue_size - 1] == NULL) {
        entry->next = manager->entries;
        manager->entries = entry;
        manager->entry_count++;
        agentos_mutex_unlock(manager->lock);
        return AGENTOS_ENOMEM;
    }

    if (entry->action_id) AGENTOS_FREE(entry->action_id);
    if (entry->compensator_id) AGENTOS_FREE(entry->compensator_id);
    if (entry->input) {
        if (entry->input_free_fn) {
            entry->input_free_fn(entry->input);
        } else {
            AGENTOS_FREE(entry->input);
        }
    }
    AGENTOS_FREE(entry);

    agentos_mutex_unlock(manager->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_compensation_get_human_queue(
    agentos_compensation_t* manager,
    char*** out_actions,
    size_t* out_count) {

    if (!manager || !out_actions || !out_count) return AGENTOS_EINVAL;

    agentos_mutex_lock(manager->lock);
    *out_count = manager->human_queue_size;
    if (*out_count == 0) {
        *out_actions = NULL;
        agentos_mutex_unlock(manager->lock);
        return AGENTOS_SUCCESS;
    }

    char** actions = (char**)AGENTOS_MALLOC(*out_count * sizeof(char*));
    if (!actions) {
        agentos_mutex_unlock(manager->lock);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < *out_count; i++) {
        actions[i] = AGENTOS_STRDUP(manager->human_queue[i]);
        if (!actions[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(actions[j]);
            AGENTOS_FREE(actions);
            agentos_mutex_unlock(manager->lock);
            return AGENTOS_ENOMEM;
        }
    }

    for (size_t i = 0; i < manager->human_queue_size; i++) {
        AGENTOS_FREE(manager->human_queue[i]);
    }
    manager->human_queue_size = 0;

    agentos_mutex_unlock(manager->lock);

    *out_actions = actions;
    return AGENTOS_SUCCESS;
}

void agentos_compensation_result_free(agentos_compensation_result_t* result) {
    if (!result) return;
    if (result->error_message) {
        AGENTOS_FREE(result->error_message);
    }
}
