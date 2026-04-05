/**
 * @file mount.c
 * @brief 挂载算子：将记忆按上下文窗口限制加载到工作记忆
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "../include/retrieval.h"
#include "../include/layer1_raw.h"
#include "agentos.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include <string.h>

typedef struct mount_entry {
    char* record_id;
    char* content;
    size_t token_count;
    uint64_t last_access;
    struct mount_entry* next;
} mount_entry_t;

struct agentos_mounter {
    agentos_layer1_raw_t* layer1;          /**< 原始层，用于读取内容 */
    agentos_token_counter_t* token_counter; /**< token 计数器 */
    size_t token_limit;                     /**< 最大允许 token 数 */
    mount_entry_t* mounted;                 /**< 已挂载的记忆链表 */
    agentos_mutex_t* lock;
};

agentos_error_t agentos_mounter_create(
    agentos_layer1_raw_t* layer1,
    agentos_token_counter_t* token_counter,
    size_t token_limit,
    agentos_mounter_t** out_mounter) {

    if (!layer1 || !token_counter || token_limit == 0 || !out_mounter) {
        AGENTOS_LOG_ERROR("Invalid parameters to mounter_create");
        return AGENTOS_EINVAL;
    }

    agentos_mounter_t* m = (agentos_mounter_t*)AGENTOS_CALLOC(1, sizeof(agentos_mounter_t));
    if (!m) {
        AGENTOS_LOG_ERROR("Failed to allocate mounter");
        return AGENTOS_ENOMEM;
    }

    m->layer1 = layer1;
    m->token_counter = token_counter;
    m->token_limit = token_limit;
    m->lock = agentos_mutex_create();
    if (!m->lock) {
        AGENTOS_FREE(m);
        return AGENTOS_ENOMEM;
    }

    *out_mounter = m;
    return AGENTOS_SUCCESS;
}

void agentos_mounter_destroy(agentos_mounter_t* mounter) {
    if (!mounter) return;
    agentos_mutex_lock(mounter->lock);
    mount_entry_t* e = mounter->mounted;
    while (e) {
        mount_entry_t* next = e->next;
        AGENTOS_FREE(e->record_id);
        AGENTOS_FREE(e->content);
        AGENTOS_FREE(e);
        e = next;
    }
    agentos_mutex_unlock(mounter->lock);
    agentos_mutex_destroy(mounter->lock);
    AGENTOS_FREE(mounter);
}

agentos_error_t agentos_mounter_mount(
    agentos_mounter_t* mounter,
    const char* record_id,
    const char* context,
    char** out_content) {

    if (!mounter || !record_id || !out_content) {
        AGENTOS_LOG_ERROR("Invalid parameters to mount");
        return AGENTOS_EINVAL;
    }

    // 从原始层读取数据
    void* data = NULL;
    size_t len = 0;
    agentos_error_t err = agentos_layer1_raw_read(mounter->layer1, record_id, &data, &len);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to read raw data for %s", record_id);
        return err;
    }

    // 假设数据是文本，转换为字符串
    char* text = (char*)data;
    // 确保以零结尾（原始数据可能不是字符串，但我们假设�?
    if (text[len - 1] != '\0') {
        char* new_text = (char*)AGENTOS_MALLOC(len + 1);
        if (!new_text) {
            AGENTOS_FREE(data);
            return AGENTOS_ENOMEM;
        }
        memcpy(new_text, data, len);
        new_text[len] = '\0';
        AGENTOS_FREE(data);
        text = new_text;
    }

    // 计算 token �?
    size_t tokens = agentos_token_counter_count(mounter->token_counter, text);

    // 检查是否超过限�?
    size_t current_tokens = 0;
    agentos_mutex_lock(mounter->lock);
    mount_entry_t* e = mounter->mounted;
    while (e) {
        current_tokens += e->token_count;
        e = e->next;
    }

    if (current_tokens + tokens > mounter->token_limit) {
        agentos_mutex_unlock(mounter->lock);
        AGENTOS_FREE(text);
        AGENTOS_LOG_WARN("Mount would exceed token limit, need to unmount some memories");
        return AGENTOS_ERESOURCE;
    }

    // 添加到挂载链�?
    mount_entry_t* entry = (mount_entry_t*)AGENTOS_MALLOC(sizeof(mount_entry_t));
    if (!entry) {
        agentos_mutex_unlock(mounter->lock);
        AGENTOS_FREE(text);
        return AGENTOS_ENOMEM;
    }
    entry->record_id = AGENTOS_STRDUP(record_id);
    entry->content = text; // 转移所有权
    entry->token_count = tokens;
    entry->last_access = agentos_time_monotonic_ns();
    entry->next = mounter->mounted;
    mounter->mounted = entry;
    agentos_mutex_unlock(mounter->lock);

    *out_content = AGENTOS_STRDUP(entry->content);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_mounter_unmount(
    agentos_mounter_t* mounter,
    const char* record_id) {

    if (!mounter || !record_id) return AGENTOS_EINVAL;

    agentos_mutex_lock(mounter->lock);
    mount_entry_t** p = &mounter->mounted;
    while (*p) {
        if (strcmp((*p)->record_id, record_id) == 0) {
            mount_entry_t* tmp = *p;
            *p = tmp->next;
            AGENTOS_FREE(tmp->record_id);
            AGENTOS_FREE(tmp->content);
            AGENTOS_FREE(tmp);
            agentos_mutex_unlock(mounter->lock);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(mounter->lock);
    return AGENTOS_ENOENT;
}

agentos_error_t agentos_mounter_list(
    agentos_mounter_t* mounter,
    char*** out_ids,
    size_t* out_count) {

    if (!mounter || !out_ids || !out_count) return AGENTOS_EINVAL;

    agentos_mutex_lock(mounter->lock);
    size_t count = 0;
    mount_entry_t* e = mounter->mounted;
    while (e) { count++; e = e->next; }

    char** ids = (char**)AGENTOS_MALLOC(count * sizeof(char*));
    if (!ids) {
        agentos_mutex_unlock(mounter->lock);
        return AGENTOS_ENOMEM;
    }

    size_t i = 0;
    e = mounter->mounted;
    while (e) {
        ids[i++] = AGENTOS_STRDUP(e->record_id);
        e = e->next;
    }
    agentos_mutex_unlock(mounter->lock);

    *out_ids = ids;
    *out_count = count;
    return AGENTOS_SUCCESS;
}
