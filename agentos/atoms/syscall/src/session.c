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
#include <stdlib.h>

/* heapstore 集成接口 */
#include "../../../agentos/heapstore/include/heapstore_integration.h"

/* Unified base library compatibility layer */
#include "../../../agentos/commons/utils/memory/include/memory_compat.h"
#include "../../../agentos/commons/utils/string/include/string_compat.h"
#include "../../../agentos/commons/utils/include/check.h"
#include "../../../agentos/commons/utils/observability/include/logger.h"
#include <string.h>
#include <cjson/cJSON.h>

/* 持久化策略配置（内部类型，不导出） */
typedef struct {
    bool enabled;
    int max_retries;
    uint32_t initial_delay_ms;
    uint32_t max_delay_ms;
    uint32_t backoff_multiplier;
    bool fail_fast;
} session_persist_config_t;

typedef struct session {
    char* session_id;
    char* metadata;
    uint64_t created_ns;
    uint64_t last_active_ns;
    session_persist_status_t persist_status;
    agentos_error_t persist_error;
    struct session* next;
} session_t;

static session_persist_config_t g_persist_config = {
    .enabled = true,
    .max_retries = 3,
    .initial_delay_ms = 100,
    .max_delay_ms = 5000,
    .backoff_multiplier = 2,
    .fail_fast = true
};

static session_t* sessions = NULL;
static agentos_mutex_t* session_lock = NULL;

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void load_persist_config(void) {
    static bool loaded = false;
    if (loaded) return;
    
    char* env_val;
    
    env_val = getenv("AGENTOS_SESSION_PERSIST_ENABLED");
    if (env_val) g_persist_config.enabled = (strcmp(env_val, "true") == 0);
    
    env_val = getenv("AGENTOS_SESSION_PERSIST_MAX_RETRIES");
    if (env_val) g_persist_config.max_retries = atoi(env_val);
    
    env_val = getenv("AGENTOS_SESSION_PERSIST_INITIAL_DELAY_MS");
    if (env_val) g_persist_config.initial_delay_ms = (uint32_t)atoi(env_val);
    
    env_val = getenv("AGENTOS_SESSION_PERSIST_MAX_DELAY_MS");
    if (env_val) g_persist_config.max_delay_ms = (uint32_t)atoi(env_val);
    
    env_val = getenv("AGENTOS_SESSION_PERSIST_BACKOFF_MULTIPLIER");
    if (env_val) g_persist_config.backoff_multiplier = (uint32_t)atoi(env_val);
    
    env_val = getenv("AGENTOS_SESSION_PERSIST_FAIL_FAST");
    if (env_val) g_persist_config.fail_fast = (strcmp(env_val, "true") == 0);
    
    AGENTOS_LOG_INFO("Session persist config: enabled=%d, max_retries=%d, fail_fast=%d",
             g_persist_config.enabled, g_persist_config.max_retries, 
             g_persist_config.fail_fast);
    
    loaded = true;
}

static agentos_error_t persist_session_with_retry(
    const char* session_id,
    const char* metadata,
    uint64_t created_ns,
    uint64_t last_active_ns) {
    
    load_persist_config();
    if (!g_persist_config.enabled) {
        return AGENTOS_SUCCESS;
    }
    
    int retry_count = 0;
    agentos_error_t last_err = AGENTOS_SUCCESS;
    uint32_t delay_ms = g_persist_config.initial_delay_ms;
    
    while (retry_count <= g_persist_config.max_retries) {
        last_err = heapstore_syscall_session_save(
            session_id, metadata, created_ns, last_active_ns);
        
        if (last_err == AGENTOS_SUCCESS) {
            if (retry_count > 0) {
                AGENTOS_LOG_INFO("Session persist succeeded after %d retries", retry_count);
            }
            return AGENTOS_SUCCESS;
        }
        
        retry_count++;
        if (retry_count <= g_persist_config.max_retries) {
            AGENTOS_LOG_WARN("Session persist attempt %d failed: %d, retrying in %d ms",
                     retry_count, last_err, delay_ms);
            agentos_time_nanosleep((uint64_t)delay_ms * 1000000ULL);
            delay_ms = MIN(delay_ms * g_persist_config.backoff_multiplier, 
                          g_persist_config.max_delay_ms);
        }
    }
    
    AGENTOS_LOG_ERROR("Session persist failed after %d attempts: %d", 
              retry_count, last_err);
    return last_err;
}

static agentos_error_t persist_delete_with_retry(const char* session_id) {
    load_persist_config();
    if (!g_persist_config.enabled) {
        return AGENTOS_SUCCESS;
    }
    
    int retry_count = 0;
    agentos_error_t last_err = AGENTOS_SUCCESS;
    uint32_t delay_ms = g_persist_config.initial_delay_ms;
    
    while (retry_count <= g_persist_config.max_retries) {
        last_err = heapstore_syscall_session_delete(session_id);
        
        if (last_err == AGENTOS_SUCCESS) {
            if (retry_count > 0) {
                AGENTOS_LOG_INFO("Session delete succeeded after %d retries", retry_count);
            }
            return AGENTOS_SUCCESS;
        }
        
        retry_count++;
        if (retry_count <= g_persist_config.max_retries) {
            AGENTOS_LOG_WARN("Session delete attempt %d failed: %d, retrying in %d ms",
                     retry_count, last_err, delay_ms);
            agentos_time_nanosleep((uint64_t)delay_ms * 1000000ULL);
            delay_ms = MIN(delay_ms * g_persist_config.backoff_multiplier, 
                          g_persist_config.max_delay_ms);
        }
    }
    
    AGENTOS_LOG_ERROR("Session delete failed after %d attempts: %d", 
              retry_count, last_err);
    return last_err;
}

static void ensure_lock(void) {
    agentos_mutex_t* current = __atomic_load_n(&session_lock, __ATOMIC_ACQUIRE);
    if (!current) {
        agentos_mutex_t* new_lock = agentos_mutex_create();
        if (!new_lock) return;
        agentos_mutex_t* expected = NULL;
        if (!__atomic_compare_exchange_n(&session_lock, &expected, new_lock,
                                          false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            agentos_mutex_destroy(new_lock);
        }
    }
}

agentos_error_t agentos_sys_session_create(const char* metadata, char** out_session_id) {
    CHECK_NULL(out_session_id);
    ensure_lock();

    static uint64_t counter = 0;
    char id_buf[64];
    snprintf(id_buf, sizeof(id_buf), "sess_%llu", (unsigned long long)__sync_fetch_and_add(&counter, 1));
    
    char* id = NULL;
    session_t* s = NULL;
    char* out_id_copy = NULL;
    agentos_error_t ret = AGENTOS_SUCCESS;
    
    STRDUP_CHECK_ERR(id, id_buf, cleanup, ret, AGENTOS_ENOMEM);
    
    s = (session_t*)AGENTOS_CALLOC(1, sizeof(session_t));
    CHECK_NULL_GOTO_ERR(s, cleanup, ret, AGENTOS_ENOMEM);
    s->session_id = id;
    id = NULL;
    s->persist_status = SESSION_PERSIST_UNKNOWN;
    s->persist_error = AGENTOS_SUCCESS;
    
    if (metadata) {
        s->metadata = AGENTOS_STRDUP(metadata);
        CHECK_NULL_GOTO_ERR(s->metadata, cleanup, ret, AGENTOS_ENOMEM);
    }
    s->created_ns = agentos_time_monotonic_ns();
    s->last_active_ns = s->created_ns;

    agentos_mutex_lock(session_lock);
    s->next = sessions;
    sessions = s;
    agentos_mutex_unlock(session_lock);
    
    session_t* session_for_persist = s;
    
    STRDUP_CHECK_ERR(out_id_copy, s->session_id, cleanup_linked, ret, AGENTOS_ENOMEM);
    s = NULL;
    
    *out_session_id = out_id_copy;
    out_id_copy = NULL;
    
    if (g_persist_config.enabled) {
        session_for_persist->persist_status = SESSION_PERSIST_PENDING;
        agentos_error_t persist_err = persist_session_with_retry(
            session_for_persist->session_id, metadata, 
            session_for_persist->created_ns, session_for_persist->last_active_ns);
        
        if (persist_err == AGENTOS_SUCCESS) {
            session_for_persist->persist_status = SESSION_PERSIST_SUCCESS;
        } else {
            session_for_persist->persist_status = SESSION_PERSIST_FAILED;
            session_for_persist->persist_error = persist_err;
            
            load_persist_config();
            if (!g_persist_config.fail_fast) {
                AGENTOS_LOG_ERROR("Session creation failed due to persistence error: %d", persist_err);
                agentos_mutex_lock(session_lock);
                session_t** pp = &sessions;
                while (*pp) {
                    if (*pp == session_for_persist) {
                        *pp = session_for_persist->next;
                        break;
                    }
                    pp = &(*pp)->next;
                }
                agentos_mutex_unlock(session_lock);
                
                AGENTOS_FREE(session_for_persist->metadata);
                AGENTOS_FREE(session_for_persist->session_id);
                AGENTOS_FREE(session_for_persist);
                AGENTOS_FREE(out_id_copy);
                return persist_err;
            } else {
                AGENTOS_LOG_WARN("Session created but persistence failed: %d (fail_fast mode)", 
                         persist_err);
            }
        }
    } else {
        session_for_persist->persist_status = SESSION_PERSIST_DISABLED;
    }
    
    return AGENTOS_SUCCESS;

cleanup_linked:
    agentos_mutex_lock(session_lock);
    session_t** pp = &sessions;
    while (*pp) {
        if (*pp == s) {
            *pp = s->next;
            break;
        }
        pp = &(*pp)->next;
    }
    agentos_mutex_unlock(session_lock);
    
cleanup:
    if (out_id_copy) AGENTOS_FREE(out_id_copy);
    if (s) {
        AGENTOS_FREE(s->metadata);
        AGENTOS_FREE(s->session_id);
        AGENTOS_FREE(s);
    }
    if (id) AGENTOS_FREE(id);
    return ret;
}

agentos_error_t agentos_sys_session_get(const char* session_id, char** out_info) {
    CHECK_NULL(session_id);
    CHECK_NULL(out_info);
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

agentos_error_t agentos_sys_session_close(const char* session_id) {
    CHECK_NULL(session_id);
    ensure_lock();
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
            
            if (g_persist_config.enabled && saved_id) {
                agentos_error_t persist_err = persist_delete_with_retry(saved_id);
                if (persist_err != AGENTOS_SUCCESS) {
                    AGENTOS_LOG_WARN("Session delete from heapstore failed after retries: %d", persist_err);
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

agentos_error_t agentos_sys_session_get_persist_status(
    const char* session_id,
    session_persist_status_t* out_status,
    agentos_error_t* out_error) {
    
    CHECK_NULL(session_id);
    CHECK_NULL(out_status);
    ensure_lock();
    
    agentos_mutex_lock(session_lock);
    session_t* s = sessions;
    while (s) {
        if (strcmp(s->session_id, session_id) == 0) {
            *out_status = s->persist_status;
            if (out_error) *out_error = s->persist_error;
            agentos_mutex_unlock(session_lock);
            return AGENTOS_SUCCESS;
        }
        s = s->next;
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
