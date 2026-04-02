/**
 * @file session.c
 * @brief 会话管理系统调用实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现会话管理系统调用，遵循架构原则：
 * - S-2 层次分解原则：通过 heapstore 进行数据持久化
 * - K-2 接口契约化原则：所有接口有完整契约定义
 * - E-2 可观测性原则：集成可观测性数据采集
 *
 * 集成架构：
 * syscall/session.c ──▶ heapstore（会话数据持久化）
 */

#include "syscalls.h"
#include "agentos.h"
#include "logger.h"
#include <stdlib.h>

/* heapstore 集成接口 */
#include "../../../heapstore/include/heapstore_integration.h"

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <cjson/cJSON.h>

/* heapstore 持久化开关（可通过配置关闭） */
static bool g_use_heapstore_persistence = true;

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
 * @brief 线程安全地确保会话锁已初始化（使�?once 模式�?
 */
static agentos_error_t ensure_lock(void) {
    if (!session_lock) {
        agentos_mutex_t* new_lock = agentos_mutex_create();
        if (!new_lock) return AGENTOS_ENOMEM;
        if (!__sync_bool_compare_and_swap(&session_lock, NULL, new_lock)) {
            agentos_mutex_destroy(new_lock);
        }
    }
    return AGENTOS_SUCCESS;
}

/**
 * @brief 创建新会话
 * 
 * @ownership 调用者负责释放 out_session_id
 * @threadsafe 是
 * @reentrant 否
 */
agentos_error_t agentos_sys_session_create(const char* metadata, char** out_session_id) {
    if (!out_session_id) return AGENTOS_EINVAL;
    agentos_error_t err = ensure_lock();
    if (err != AGENTOS_SUCCESS) return err;

    static uint64_t counter = 0;
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "sess_%llu", (unsigned long long)__sync_fetch_and_add(&counter, 1));
    char* id = AGENTOS_STRDUP(id_buf);
    if (!id) return AGENTOS_ENOMEM;

    session_t* s = (session_t*)AGENTOS_CALLOC(1, sizeof(session_t));
    if (!s) {
        AGENTOS_FREE(id);
        return AGENTOS_ENOMEM;
    }
    s->session_id = id;
    if (metadata) {
        s->metadata = AGENTOS_STRDUP(metadata);
        if (!s->metadata) {
            AGENTOS_FREE(s->session_id);
            AGENTOS_FREE(s);
            AGENTOS_FREE(id);
            return AGENTOS_ENOMEM;
        }
    }
    s->created_ns = agentos_time_monotonic_ns();
    s->last_active_ns = s->created_ns;

    agentos_mutex_lock(session_lock);
    s->next = sessions;
    sessions = s;
    agentos_mutex_unlock(session_lock);

    *out_session_id = AGENTOS_STRDUP(id);
    if (!*out_session_id) {
        agentos_mutex_lock(session_lock);
        session_t** pp = &sessions;
        while (*pp) {
            if (*pp == s) { *pp = s->next; break; }
            pp = &(*pp)->next;
        }
        agentos_mutex_unlock(session_lock);
        AGENTOS_FREE(s->session_id);
        AGENTOS_FREE(s->metadata);
        AGENTOS_FREE(s);
        AGENTOS_FREE(id);
        return AGENTOS_ENOMEM;
    }
    
    /* 持久化到 heapstore（遵循 S-2 层次分解原则） */
    if (g_use_heapstore_persistence) {
        agentos_error_t persist_err = heapstore_syscall_session_save(
            id, metadata, s->created_ns, s->last_active_ns);
        if (persist_err != AGENTOS_SUCCESS) {
            /* 持久化失败不影响内存操作，仅记录日志 */
            LOG_WARN("Session persist to heapstore failed: %d", persist_err);
        }
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 获取会话信息
 */
agentos_error_t agentos_sys_session_get(const char* session_id, char** out_info) {
    if (!session_id || !out_info) return AGENTOS_EINVAL;
    agentos_error_t err = ensure_lock();
    if (err != AGENTOS_SUCCESS) return err;
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
 * 
 * @ownership 内部释放会话资源
 * @threadsafe 是
 * @reentrant 否
 */
agentos_error_t agentos_sys_session_close(const char* session_id) {
    if (!session_id) return AGENTOS_EINVAL;
    agentos_error_t err = ensure_lock();
    if (err != AGENTOS_SUCCESS) return err;
    agentos_mutex_lock(session_lock);
    session_t** p = &sessions;
    while (*p) {
        if (strcmp((*p)->session_id, session_id) == 0) {
            session_t* tmp = *p;
            *p = tmp->next;
            char* saved_id = AGENTOS_STRDUP(tmp->session_id);
            AGENTOS_FREE(tmp->session_id);
            if (tmp->metadata) AGENTOS_FREE(tmp->metadata);
            AGENTOS_FREE(tmp);
            agentos_mutex_unlock(session_lock);
            
            /* 从 heapstore 删除（遵循 S-2 层次分解原则） */
            if (g_use_heapstore_persistence && saved_id) {
                agentos_error_t persist_err = heapstore_syscall_session_delete(saved_id);
                if (persist_err != AGENTOS_SUCCESS) {
                    LOG_WARN("Session delete from heapstore failed: %d", persist_err);
                }
                AGENTOS_FREE(saved_id);
            }
            
            return AGENTOS_SUCCESS;
        }
        p = &(*p)->next;
    }
    agentos_mutex_unlock(session_lock);
    return AGENTOS_ENOENT;
}

/**
 * @brief 列出所有会�?
 */
agentos_error_t agentos_sys_session_list(char*** out_sessions, size_t* out_count) {
    if (!out_sessions || !out_count) return AGENTOS_EINVAL;
    agentos_error_t err = ensure_lock();
    if (err != AGENTOS_SUCCESS) return err;
    agentos_mutex_lock(session_lock);
    size_t count = 0;
    session_t* s = sessions;
    while (s) { count++; s = s->next; }
    char** list = (char**)AGENTOS_CALLOC(count, sizeof(char*));
    if (!list) {
        agentos_mutex_unlock(session_lock);
        return AGENTOS_ENOMEM;
    }
    s = sessions;
    size_t i = 0;
    while (s) {
        list[i] = AGENTOS_STRDUP(s->session_id);
        if (!list[i]) {
            for (size_t j = 0; j < i; j++) AGENTOS_FREE(list[j]);
            AGENTOS_FREE(list);
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
