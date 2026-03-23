/**
 * @file compensation.c
 * @brief 补偿事务管理器实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "execution.h"
#include "agentos.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 可补偿动作记录
 */
typedef struct compensable_action {
    char* action_id;
    char* task_id;
    char* compensator_id;      /**< 用于补偿的执行单元ID */
    void* input;                /**< 原始输入（用于补偿） */
    size_t input_len;
    uint64_t timestamp;         /**< 记录时间 */
    struct compensable_action* next;
} compensable_action_t;

/**
 * @brief 补偿管理器内部结构
 */
struct agentos_compensation {
    compensable_action_t* actions;
    agentos_mutex_t* lock;
    char** human_queue;               /**< 待人工介入的操作ID列表 */
    size_t human_queue_size;
    size_t human_queue_capacity;
};

agentos_error_t agentos_compensation_create(agentos_compensation_t** out_manager) {
    if (!out_manager) return AGENTOS_EINVAL;

    agentos_compensation_t* mgr = (agentos_compensation_t*)calloc(1, sizeof(agentos_compensation_t));
    if (!mgr) return AGENTOS_ENOMEM;

    mgr->lock = agentos_mutex_create();
    if (!mgr->lock) {
        free(mgr);
        return AGENTOS_ENOMEM;
    }

    mgr->human_queue_capacity = 16;
    mgr->human_queue = (char**)malloc(mgr->human_queue_capacity * sizeof(char*));
    if (!mgr->human_queue) {
        agentos_mutex_destroy(mgr->lock);
        free(mgr);
        return AGENTOS_ENOMEM;
    }

    *out_manager = mgr;
    return AGENTOS_SUCCESS;
}

void agentos_compensation_destroy(agentos_compensation_t* manager) {
    if (!manager) return;

    agentos_mutex_lock(manager->lock);
    // 释放所有动作
    compensable_action_t* act = manager->actions;
    while (act) {
        compensable_action_t* next = act->next;
        if (act->action_id) free(act->action_id);
        if (act->task_id) free(act->task_id);
        if (act->compensator_id) free(act->compensator_id);
        if (act->input) free(act->input);
        free(act);
        act = next;
    }

    // 释放人工队列
    for (size_t i = 0; i < manager->human_queue_size; i++) {
        free(manager->human_queue[i]);
    }
    free(manager->human_queue);
    agentos_mutex_unlock(manager->lock);

    agentos_mutex_destroy(manager->lock);
    free(manager);
}

agentos_error_t agentos_compensation_register(
    agentos_compensation_t* manager,
    const char* action_id,
    const char* compensator_id,
    const void* input) {

    if (!manager || !action_id || !compensator_id) return AGENTOS_EINVAL;

    compensable_action_t* act = (compensable_action_t*)calloc(1, sizeof(compensable_action_t));
    if (!act) return AGENTOS_ENOMEM;

    act->action_id = strdup(action_id);
    act->compensator_id = strdup(compensator_id);
    act->timestamp = agentos_time_monotonic_ns();

    if (input) {
        // 假设 input 是字符串，需要知道长度
        act->input = strdup((const char*)input);
        act->input_len = act->input ? strlen(act->input) + 1 : 0;
    }

    if (!act->action_id || !act->compensator_id || (input && !act->input)) {
        if (act->action_id) free(act->action_id);
        if (act->compensator_id) free(act->compensator_id);
        if (act->input) free(act->input);
        free(act);
        return AGENTOS_ENOMEM;
    }

    agentos_mutex_lock(manager->lock);
    act->next = manager->actions;
    manager->actions = act;
    agentos_mutex_unlock(manager->lock);

    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_compensation_compensate(
    agentos_compensation_t* manager,
    const char* action_id) {

    if (!manager || !action_id) return AGENTOS_EINVAL;

    agentos_mutex_lock(manager->lock);

    // 查找动作
    compensable_action_t** p = &manager->actions;
    compensable_action_t* act = NULL;
    while (*p) {
        if (strcmp((*p)->action_id, action_id) == 0) {
            act = *p;
            *p = act->next;
            break;
        }
        p = &(*p)->next;
    }

    if (!act) {
        agentos_mutex_unlock(manager->lock);
        return AGENTOS_ENOENT;
    }

    // 此处应通过执行引擎调用补偿单元，但简化：直接加入人工队列
    // 实际实现应使用补偿器ID查找执行单元并调用

    // 加入人工介入队列（模拟无法自动补偿）
    if (manager->human_queue_size >= manager->human_queue_capacity) {
        size_t new_cap = manager->human_queue_capacity * 2;
        char** new_queue = (char**)realloc(manager->human_queue, new_cap * sizeof(char*));
        if (!new_queue) {
            act->next = manager->actions;
            manager->actions = act;
            agentos_mutex_unlock(manager->lock);
            return AGENTOS_ENOMEM;
        }
        manager->human_queue = new_queue;
        manager->human_queue_capacity = new_cap;
    }

    manager->human_queue[manager->human_queue_size++] = strdup(act->action_id);
    if (manager->human_queue[manager->human_queue_size - 1] == NULL) {
        act->next = manager->actions;
        manager->actions = act;
        agentos_mutex_unlock(manager->lock);
        return AGENTOS_ENOMEM;
    }

    // 释放动作记录
    free(act->action_id);
    free(act->compensator_id);
    if (act->input) free(act->input);
    free(act);

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

    char** actions = (char**)malloc(*out_count * sizeof(char*));
    if (!actions) {
        agentos_mutex_unlock(manager->lock);
        return AGENTOS_ENOMEM;
    }

    for (size_t i = 0; i < *out_count; i++) {
        actions[i] = strdup(manager->human_queue[i]);
        if (!actions[i]) {
            for (size_t j = 0; j < i; j++) free(actions[j]);
            free(actions);
            agentos_mutex_unlock(manager->lock);
            return AGENTOS_ENOMEM;
        }
    }

    // 清空队列（调用者获得所有权后，队列应清空）
    for (size_t i = 0; i < manager->human_queue_size; i++) {
        free(manager->human_queue[i]);
    }
    manager->human_queue_size = 0;

    agentos_mutex_unlock(manager->lock);

    *out_actions = actions;
    return AGENTOS_SUCCESS;
}