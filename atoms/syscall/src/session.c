/**
 * @file session.c
 * @brief 会话管理系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

typedef struct session {
    char* session_id;
    char* metadata;
    uint64_t created_ns;
    uint64_t last_active_ns;
    struct session* next;
} session_t;

static session_t* sessions = NULL;
static agentos_mutex_t* session_lock = NULL;

/**
 * @brief 线程安全地确保会话锁已初始化（使用 once 模式）
 */
static void ensure_lock(void) {
    if (!session_lock) {
        agentos_mutex_t* new_lock = agentos_mutex_create();
        if (!new_lock) return;
        if (!__sync_bool_compare_and_swap(&session_lock, NULL, new_lock)) {
            agentos_mutex_destroy(new_lock);
        }
    }
}

/**
 * @brief 创建新会话
 */
agentos_error_t agentos_sys_session_create(const char* metadata, char** out_session_id) {
    if (!out_session_id) return AGENTOS_EINVAL;
    ensure_lock();

    static uint64_t counter = 0;
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "sess_%llu", (unsigned long long)__sync_fetch_and_add(&counter, 1));
    char* id = strdup(id_buf);
    if (!id) return AGENTOS_ENOMEM;

    session_t* s = (session_t*)calloc(1, sizeof(session_t));
    if (!s) {
        free(id);
        return AGENTOS_ENOMEM;
    }
    s->session_id = id;
    if (metadata) {
        s->metadata = strdup(metadata);
        if (!s->metadata) {
            free(s->session_id);
            free(s);
            free(id);
            return AGENTOS_ENOMEM;
        }
    }
    s->created_ns = agentos_time_monotonic_ns();
    s->last_active_ns = s->created_ns;

    agentos_mutex_lock(session_lock);
    s->next = sessions;
    sessions = s;
    agentos_mutex_unlock(session_lock);

    *out_session_id = strdup(id);
    if (!*out_session_id) {
        agentos_mutex_lock(session_lock);
        session_t** pp = &sessions;
        while (*pp) {
            if (*pp == s) { *pp = s->next; break; }
            pp = &(*pp)->next;
        }
        agentos_mutex_unlock(session_lock);
        free(s->session_id);
        free(s->metadata);
        free(s);
        free(id);
        return AGENTOS_ENOMEM;
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取会话信息
 */
agentos_error_t agentos_sys_session_get(const char* session_id, char** out_info) {
    if (!session_id || !out_info) return AGENTOS_EINVAL;
    ensure_lock();
    agentos_mutex_lock(session_lock);
    session_t* s = sessions;
    while (s) {
        if (strcmp(s->session_id, session_id) == 0) {
            s->last_active_ns = agentos_time_monotonic_ns();
            cJSON* root = cJSON_CreateObject();
            if (!root) {
                agentos_mutex_unlock(session_lock);
                return AGENTOS_ENOMEM;
            }
            cJSON_AddStringToObject(root, "session_id", s->session_id);
            if (s->metadata) cJSON_AddStringToObject(root, "metadata", s->metadata);
            cJSON_AddNumberToObject(root, "created_ns", (double)s->created_ns);
            cJSON_AddNumberToObject(root, "last_active_ns", (double)s->last_active_ns);
            char* json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            agentos_mutex_unlock(session_lock);
            *out_info = json;
            return json ? AGENTOS_SUCCESS : AGENTOS_ENOMEM;
        }
        s = s->next;
    }
    agentos_mutex_unlock(session_lock);
    return AGENTOS_ENOENT;
}

/**
 * @brief 关闭会话
 */
agentos_error_t agentos_sys_session_close(const char* session_id) {
    if (!session_id) return AGENTOS_EINVAL;
    ensure_lock();
    agentos_mutex_lock(session_lock);
    session_t** p = &sessions;
    while (*p) {
        if (strcmp((*p)->session_id, session_id) == 0) {
            session_t* tmp = *p;
            *p = tmp->next;
            free(tmp->session_id);
            if (tmp->metadata) free(tmp->metadata);
            free(tmp);
            agentos_mutex_unlock(session_lock);
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(session_lock);
    return AGENTOS_ENOENT;
}

/**
 * @brief 列出所有会话
 */
agentos_error_t agentos_sys_session_list(char*** out_sessions, size_t* out_count) {
    if (!out_sessions || !out_count) return AGENTOS_EINVAL;
    ensure_lock();
    agentos_mutex_lock(session_lock);
    size_t count = 0;
    session_t* s = sessions;
    while (s) { count++; s = s->next; }
    char** list = (char**)calloc(count, sizeof(char*));
    if (!list) {
        agentos_mutex_unlock(session_lock);
        return AGENTOS_ENOMEM;
    }
    s = sessions;
    size_t i = 0;
    while (s) {
        list[i] = strdup(s->session_id);
        if (!list[i]) {
            for (size_t j = 0; j < i; j++) free(list[j]);
            free(list);
            agentos_mutex_unlock(session_lock);
            return AGENTOS_ENOMEM;
        }
        i++;
        s = s->next;
    }
    agentos_mutex_unlock(session_lock);
    *out_sessions = list;
    *out_count = count;
    return AGENTOS_SUCCESS;
}
