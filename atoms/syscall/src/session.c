/**
 * @file session.c
 * @brief 会话管理系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved. "From data intelligence emerges."
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

static void ensure_lock(void) {
    if (!session_lock) {
        session_lock = agentos_mutex_create();
    }
}

agentos_error_t agentos_sys_session_create(const char* metadata, char** out_session_id) {
    if (!out_session_id) return AGENTOS_EINVAL;
    ensure_lock();

    // 生成唯一会话ID
    static uint64_t counter = 0;
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "sess_%llu", (unsigned long long)__sync_fetch_and_add(&counter, 1));
    char* id = strdup(id_buf);
    if (!id) return AGENTOS_ENOMEM;

    session_t* s = (session_t*)malloc(sizeof(session_t));
    if (!s) {
        free(id);
        return AGENTOS_ENOMEM;
    }
    s->session_id = id;
    s->metadata = metadata ? strdup(metadata) : NULL;
    s->created_ns = agentos_time_monotonic_ns();
    s->last_active_ns = s->created_ns;

    agentos_mutex_lock(session_lock);
    s->next = sessions;
    sessions = s;
    agentos_mutex_unlock(session_lock);

    *out_session_id = strdup(id);
    return AGENTOS_SUCCESS;
}

agentos_error_t agentos_sys_session_get(const char* session_id, char** out_info) {
    if (!session_id || !out_info) return AGENTOS_EINVAL;
    ensure_lock();
    agentos_mutex_lock(session_lock);
    session_t* s = sessions;
    while (s) {
        if (strcmp(s->session_id, session_id) == 0) {
            s->last_active_ns = agentos_time_monotonic_ns();
            cJSON* root = cJSON_CreateObject();
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

agentos_error_t agentos_sys_session_list(char*** out_sessions, size_t* out_count) {
    if (!out_sessions || !out_count) return AGENTOS_EINVAL;
    ensure_lock();
    agentos_mutex_lock(session_lock);
    size_t count = 0;
    session_t* s = sessions;
    while (s) { count++; s = s->next; }
    char** list = (char**)malloc(count * sizeof(char*));
    if (!list) {
        agentos_mutex_unlock(session_lock);
        return AGENTOS_ENOMEM;
    }
    s = sessions;
    size_t i = 0;
    while (s) {
        list[i++] = strdup(s->session_id);
        s = s->next;
    }
    agentos_mutex_unlock(session_lock);
    *out_sessions = list;
    *out_count = count;
    return AGENTOS_SUCCESS;
}