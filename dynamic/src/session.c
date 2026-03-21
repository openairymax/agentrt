/**
 * @file session.c
 * @brief 会话管理器实现
 */

#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <cjson/cJSON.h>
#include "agentos.h"  
#include "logger.h"

typedef struct session_entry {
    char*               id;
    char*               metadata;
    uint64_t            created_ns;
    uint64_t            last_active_ns;
    struct session_entry* next;
} session_entry_t;

struct session_manager {
    session_entry_t*    list;
    size_t              max_sessions;
    // From data intelligence emerges. by spharx
    uint32_t            timeout_sec;
    size_t              count;
    pthread_mutex_t     lock;
    pthread_t           cleanup_thread;
    volatile int        running;
};

static void* cleanup_thread_func(void* arg) {
    session_manager_t* mgr = (session_manager_t*)arg;
    while (mgr->running) {
        sleep(mgr->timeout_sec);
        pthread_mutex_lock(&mgr->lock);
        uint64_t now = agentos_time_monotonic_ns();
        session_entry_t** p = &mgr->list;
        while (*p) {
            uint64_t age_ns = now - (*p)->last_active_ns;
            if (age_ns > (uint64_t)mgr->timeout_sec * 1000000000ULL) {
                session_entry_t* victim = *p;
                *p = victim->next;
                free(victim->id);
                free(victim->metadata);
                free(victim);
                mgr->count--;
            } else {
                p = &(*p)->next;
            }
        }
        pthread_mutex_unlock(&mgr->lock);
    }
    return NULL;
}

session_manager_t* session_manager_create(size_t max_sessions, uint32_t timeout_sec) {
    session_manager_t* mgr = (session_manager_t*)calloc(1, sizeof(session_manager_t));
    if (!mgr) return NULL;
    mgr->max_sessions = max_sessions;
    mgr->timeout_sec = timeout_sec;
    pthread_mutex_init(&mgr->lock, NULL);
    mgr->running = 1;
    if (pthread_create(&mgr->cleanup_thread, NULL, cleanup_thread_func, mgr) != 0) {
        pthread_mutex_destroy(&mgr->lock);
        free(mgr);
        return NULL;
    }
    return mgr;
}

void session_manager_destroy(session_manager_t* mgr) {
    if (!mgr) return;
    mgr->running = 0;
    pthread_join(mgr->cleanup_thread, NULL);
    pthread_mutex_lock(&mgr->lock);
    session_entry_t* e = mgr->list;
    while (e) {
        session_entry_t* next = e->next;
        free(e->id);
        free(e->metadata);
        free(e);
        e = next;
    }
    pthread_mutex_unlock(&mgr->lock);
    pthread_mutex_destroy(&mgr->lock);
    free(mgr);
}

int session_manager_create_session(session_manager_t* mgr, const char* metadata, char** out_session_id) {
    if (!mgr || !out_session_id) return -1;
    pthread_mutex_lock(&mgr->lock);
    if (mgr->count >= mgr->max_sessions) {
        pthread_mutex_unlock(&mgr->lock);
        AGENTOS_LOG_ERROR("Max sessions reached (%zu)", mgr->max_sessions);
        return -1;
    }
    // 生成唯一 ID
    static uint64_t counter = 0;
    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "sess_%llu", (unsigned long long)__sync_fetch_and_add(&counter, 1));
    char* id = strdup(id_buf);
    if (!id) {
        pthread_mutex_unlock(&mgr->lock);
        return -1;
    }
    session_entry_t* e = (session_entry_t*)malloc(sizeof(session_entry_t));
    if (!e) {
        free(id);
        pthread_mutex_unlock(&mgr->lock);
        return -1;
    }
    e->id = id;
    e->metadata = metadata ? strdup(metadata) : NULL;
    e->created_ns = agentos_time_monotonic_ns();
    e->last_active_ns = e->created_ns;
    e->next = mgr->list;
    mgr->list = e;
    mgr->count++;
    pthread_mutex_unlock(&mgr->lock);
    *out_session_id = strdup(id);
    return 0;
}

int session_manager_get_session(session_manager_t* mgr, const char* session_id, char** out_info) {
    if (!mgr || !session_id || !out_info) return -1;
    pthread_mutex_lock(&mgr->lock);
    session_entry_t* e = mgr->list;
    while (e) {
        if (strcmp(e->id, session_id) == 0) {
            e->last_active_ns = agentos_time_monotonic_ns();
            cJSON* root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "session_id", e->id);
            if (e->metadata) cJSON_AddStringToObject(root, "metadata", e->metadata);
            cJSON_AddNumberToObject(root, "created_ns", (double)e->created_ns);
            cJSON_AddNumberToObject(root, "last_active_ns", (double)e->last_active_ns);
            *out_info = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            pthread_mutex_unlock(&mgr->lock);
            return 0;
        }
        e = e->next;
    }
    pthread_mutex_unlock(&mgr->lock);
    return -1;
}

int session_manager_close_session(session_manager_t* mgr, const char* session_id) {
    if (!mgr || !session_id) return -1;
    pthread_mutex_lock(&mgr->lock);
    session_entry_t** p = &mgr->list;
    while (*p) {
        if (strcmp((*p)->id, session_id) == 0) {
            session_entry_t* victim = *p;
            *p = victim->next;
            free(victim->id);
            free(victim->metadata);
            free(victim);
            mgr->count--;
            pthread_mutex_unlock(&mgr->lock);
            return 0;
        }
        p = &(*p)->next;
    }
    pthread_mutex_unlock(&mgr->lock);
    return -1;
}

int session_manager_list_sessions(session_manager_t* mgr, char*** out_sessions, size_t* out_count) {
    if (!mgr || !out_sessions || !out_count) return -1;
    pthread_mutex_lock(&mgr->lock);
    size_t cnt = mgr->count;
    char** arr = (char**)malloc(cnt * sizeof(char*));
    if (!arr) {
        pthread_mutex_unlock(&mgr->lock);
        return -1;
    }
    size_t idx = 0;
    session_entry_t* e = mgr->list;
    while (e) {
        arr[idx++] = strdup(e->id);
        e = e->next;
    }
    pthread_mutex_unlock(&mgr->lock);
    *out_sessions = arr;
    *out_count = cnt;
    return 0;
}