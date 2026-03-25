/**
 * @file session.c
 * @brief 会话管理器实现
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "session.h"
#include "logger.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 会话 ID 长度（UUID v4 格式：xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx） */
#define SESSION_ID_LEN 36

/* 会话结构 */
typedef struct session {
    char            id[SESSION_ID_LEN + 1];  /**< 会话 ID */
    char*           metadata;                 /**< 元数据 JSON */
    uint64_t        created_ns;               /**< 创建时间（纳秒） */
    uint64_t        last_active_ns;           /**< 最后活动时间 */
    struct session* next;                     /**< 哈希表链表指针 */
} session_t;

/* 会话管理器结构 */
struct session_manager {
    session_t**       buckets;          /**< 哈希桶数组 */
    size_t            bucket_count;     /**< 桶数量 */
    size_t            max_sessions;     /**< 最大会话数 */
    uint32_t          timeout_sec;      /**< 超时秒数 */
    
    pthread_mutex_t   lock;             /**< 全局锁 */
    pthread_t         cleaner_thread;   /**< 清理线程 */
    atomic_bool       running;          /**< 运行标志 */
    pthread_cond_t    cleaner_cond;     /**< 清理线程条件变量 */
    
    atomic_size_t     session_count;    /**< 当前会话数 */
    atomic_size_t     memory_usage;     /**< 内存使用量（字节） */
    atomic_size_t     cleanup_count;    /**< 清理计数器 */
    
    /* 性能优化 */
    size_t            avg_session_len;  /**< 平均会话长度 */
    pthread_mutex_t   stats_lock;       /**< 统计信息锁 */
};

/* ========== UUID v4 生成 ========== */

/**
 * @brief 生成 UUID v4 格式的会话 ID
 * @param[out] out 输出缓冲区（至少 37 字节）
 */
static void generate_uuid_v4(char* out) {
    static atomic_uint_fast64_t counter = 0;
    
    uint64_t now_ns = agentos_time_monotonic_ns();
    uint64_t ctr = atomic_fetch_add(&counter, 1);
    
    /* 使用时间戳 + 计数器 + 随机数生成伪 UUID */
    unsigned int seed = (unsigned int)(now_ns ^ (ctr << 16));
    
    /* 生成 16 个随机字节 */
    uint8_t bytes[16];
    for (int i = 0; i < 16; i++) {
        bytes[i] = (uint8_t)(rand_r(&seed) & 0xFF);
    }
    
    /* 设置 UUID v4 版本位 */
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;
    
    /* 格式化为字符串 */
    snprintf(out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0], bytes[1], bytes[2], bytes[3],
        bytes[4], bytes[5],
        bytes[6], bytes[7],
        bytes[8], bytes[9],
        bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
}

/* ========== 哈希函数 ========== */

/**
 * @brief FNV-1a 哈希函数
 */
static size_t hash_session_id(const char* id, size_t bucket_count) {
    size_t hash = 14695981039346656037ULL;  /* FNV offset basis */
    
    while (*id) {
        hash ^= (uint8_t)(*id++);
        hash *= 1099511628211ULL;  /* FNV prime */
    }
    
    return hash % bucket_count;
}

/* ========== 清理线程 ========== */

/**
 * @brief 清理过期会话的线程函数
 */
static void* session_cleaner_thread(void* arg) {
    session_manager_t* mgr = (session_manager_t*)arg;
    
    AGENTOS_LOG_DEBUG("Session cleaner thread started");
    
    while (atomic_load(&mgr->running)) {
        /* 计算下次清理时间（基于负载动态调整） */
        uint32_t check_interval = mgr->timeout_sec / 4;
        if (check_interval < 10) check_interval = 10;
        if (check_interval > 300) check_interval = 300;
        
        /* 如果会话数量很多，增加清理频率 */
        if (atomic_load(&mgr->session_count) > mgr->max_sessions / 2) {
            check_interval = check_interval / 2;
        }
        
        /* 使用条件变量等待，可被提前唤醒 */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += check_interval;
        
        pthread_mutex_lock(&mgr->lock);
        int ret = pthread_cond_timedwait(&mgr->cleaner_cond, &mgr->lock, &ts);
        (void)ret;  /* 忽略返回值，继续检查 */
        pthread_mutex_unlock(&mgr->lock);
        
        if (!atomic_load(&mgr->running)) {
            break;
        }
        
        /* 清理过期会话 */
        uint64_t now_ns = agentos_time_monotonic_ns();
        uint64_t timeout_ns = (uint64_t)mgr->timeout_sec * 1000000000ULL;
        size_t cleaned = 0;
        size_t memory_freed = 0;
        
        pthread_mutex_lock(&mgr->lock);
        
        for (size_t i = 0; i < mgr->bucket_count; i++) {
            session_t** pp = &mgr->buckets[i];
            
            while (*pp) {
                session_t* s = *pp;
                
                if ((now_ns - s->last_active_ns) > timeout_ns) {
                    /* 从链表中移除 */
                    *pp = s->next;
                    
                    AGENTOS_LOG_DEBUG("Session %s expired, cleaning up", s->id);
                    
                    /* 计算释放的内存 */
                    size_t session_mem = sizeof(session_t) + SESSION_ID_LEN + 1;
                    if (s->metadata) {
                        session_mem += strlen(s->metadata);
                    }
                    
                    /* 释放资源 */
                    free(s->metadata);
                    free(s);
                    
                    atomic_fetch_sub(&mgr->session_count, 1);
                    atomic_fetch_sub(&mgr->memory_usage, session_mem);
                    atomic_fetch_add(&mgr->cleanup_count, 1);
                    cleaned++;
                    memory_freed += session_mem;
                } else {
                    pp = &s->next;
                }
            }
        }
        
        pthread_mutex_unlock(&mgr->lock);
        
        if (cleaned > 0) {
            AGENTOS_LOG_INFO("Cleaned %zu expired sessions, freed %zu bytes", 
                cleaned, memory_freed);
            
            /* 如果清理了大量会话，记录性能指标 */
            if (cleaned > 10) {
                AGENTOS_LOG_WARN("Large cleanup event: %zu sessions removed", cleaned);
            }
        }
    }
    
    AGENTOS_LOG_DEBUG("Session cleaner thread stopped");
    return NULL;
}

/* ========== 公共 API 实现 ========== */

session_manager_t* session_manager_create(
    size_t max_sessions,
    uint32_t timeout_sec) {
    
    /* 参数验证 */
    if (max_sessions == 0 || timeout_sec == 0) {
        return NULL;
    }
    
    /* 分配管理器 */
    session_manager_t* mgr = (session_manager_t*)calloc(1, sizeof(session_manager_t));
    if (!mgr) return NULL;
    
    /* 计算桶数量（约为最大会话数的 1/4，最小 16） */
    mgr->bucket_count = max_sessions / 4;
    if (mgr->bucket_count < 16) mgr->bucket_count = 16;
    if (mgr->bucket_count > 4096) mgr->bucket_count = 4096;
    
    /* 分配桶数组 */
    mgr->buckets = (session_t**)calloc(mgr->bucket_count, sizeof(session_t*));
    if (!mgr->buckets) {
        free(mgr);
        return NULL;
    }
    
    mgr->max_sessions = max_sessions;
    mgr->timeout_sec = timeout_sec;
    
    /* 初始化同步原语 */
    if (pthread_mutex_init(&mgr->lock, NULL) != 0) {
        free(mgr->buckets);
        free(mgr);
        return NULL;
    }
    
    if (pthread_cond_init(&mgr->cleaner_cond, NULL) != 0) {
        pthread_mutex_destroy(&mgr->lock);
        free(mgr->buckets);
        free(mgr);
        return NULL;
    }
    
    if (pthread_mutex_init(&mgr->stats_lock, NULL) != 0) {
        pthread_cond_destroy(&mgr->cleaner_cond);
        pthread_mutex_destroy(&mgr->lock);
        free(mgr->buckets);
        free(mgr);
        return NULL;
    }
    
    atomic_init(&mgr->running, true);
    atomic_init(&mgr->session_count, 0);
    atomic_init(&mgr->memory_usage, 0);
    atomic_init(&mgr->cleanup_count, 0);
    mgr->avg_session_len = 0;
    
    /* 启动清理线程 */
    if (pthread_create(&mgr->cleaner_thread, NULL, session_cleaner_thread, mgr) != 0) {
        pthread_cond_destroy(&mgr->cleaner_cond);
        pthread_mutex_destroy(&mgr->lock);
        free(mgr->buckets);
        free(mgr);
        return NULL;
    }
    
    AGENTOS_LOG_INFO("Session manager created: max=%zu, timeout=%us",
        max_sessions, timeout_sec);
    
    return mgr;
}

void session_manager_destroy(session_manager_t* mgr) {
    if (!mgr) return;
    
    /* 停止清理线程 */
    atomic_store(&mgr->running, false);
    pthread_cond_broadcast(&mgr->cleaner_cond);
    pthread_join(mgr->cleaner_thread, NULL);
    
    /* 释放所有会话 */
    pthread_mutex_lock(&mgr->lock);
    
    for (size_t i = 0; i < mgr->bucket_count; i++) {
        session_t* s = mgr->buckets[i];
        while (s) {
            session_t* next = s->next;
            free(s->metadata);
            free(s);
            s = next;
        }
    }
    
    pthread_mutex_unlock(&mgr->lock);
    
    /* 销毁同步原语 */
    pthread_cond_destroy(&mgr->cleaner_cond);
    pthread_mutex_destroy(&mgr->lock);
    pthread_mutex_destroy(&mgr->stats_lock);
    
    free(mgr->buckets);
    free(mgr);
    
    AGENTOS_LOG_INFO("Session manager destroyed");
}

agentos_error_t session_manager_create_session(
    session_manager_t* mgr,
    const char* metadata,
    char** out_session_id) {
    
    if (!mgr || !out_session_id) return AGENTOS_EINVAL;
    
    /* 检查会话数限制 */
    if (atomic_load(&mgr->session_count) >= mgr->max_sessions) {
        AGENTOS_LOG_WARN("Session limit reached: %zu", mgr->max_sessions);
        return AGENTOS_EBUSY;
    }
    
    /* 计算内存需求 */
    size_t session_size = sizeof(session_t);
    size_t metadata_size = metadata ? strlen(metadata) : 0;
    size_t total_size = session_size + metadata_size + SESSION_ID_LEN + 1;
    
    /* 检查内存限制 */
    if (atomic_load(&mgr->memory_usage) + total_size > mgr->max_sessions * 1024) {
        AGENTOS_LOG_WARN("Memory limit reached for sessions");
        return AGENTOS_ENOMEM;
    }
    
    /* 创建会话 */
    session_t* s = (session_t*)calloc(1, sizeof(session_t));
    if (!s) return AGENTOS_ENOMEM;
    
    /* 生成 ID */
    generate_uuid_v4(s->id);
    
    /* 设置时间戳 */
    uint64_t now_ns = agentos_time_monotonic_ns();
    s->created_ns = now_ns;
    s->last_active_ns = now_ns;
    
    /* 复制元数据 */
    if (metadata) {
        s->metadata = strdup(metadata);
        if (!s->metadata) {
            free(s);
            return AGENTOS_ENOMEM;
        }
    }
    
    /* 插入哈希表（使用更安全的插入方式） */
    size_t bucket = hash_session_id(s->id, mgr->bucket_count);
    
    pthread_mutex_lock(&mgr->lock);
    s->next = mgr->buckets[bucket];
    mgr->buckets[bucket] = s;
    atomic_fetch_add(&mgr->session_count, 1);
    atomic_fetch_add(&mgr->memory_usage, total_size);
    
    /* 更新统计信息 */
    pthread_mutex_lock(&mgr->stats_lock);
    mgr->avg_session_len = (mgr->avg_session_len + atomic_load(&mgr->session_count)) / 2;
    pthread_mutex_unlock(&mgr->stats_lock);
    
    pthread_mutex_unlock(&mgr->lock);
    
    /* 返回 ID */
    *out_session_id = strdup(s->id);
    if (!*out_session_id) {
        /* 回滚 - 更完善的错误处理 */
        pthread_mutex_lock(&mgr->lock);
        session_t** pp = &mgr->buckets[bucket];
        while (*pp && *pp != s) pp = &(*pp)->next;
        if (*pp == s) {
            *pp = s->next;
            atomic_fetch_sub(&mgr->session_count, 1);
            atomic_fetch_sub(&mgr->memory_usage, total_size);
        }
        pthread_mutex_unlock(&mgr->lock);
        
        free(s->metadata);
        free(s);
        return AGENTOS_ENOMEM;
    }
    
    AGENTOS_LOG_DEBUG("Session created: %s (memory: %zu bytes)", s->id, total_size);
    return AGENTOS_SUCCESS;
}

agentos_error_t session_manager_get_session(
    session_manager_t* mgr,
    const char* session_id,
    char** out_info) {
    
    if (!mgr || !session_id || !out_info) return AGENTOS_EINVAL;
    
    size_t bucket = hash_session_id(session_id, mgr->bucket_count);
    
    pthread_mutex_lock(&mgr->lock);
    
    session_t* s = mgr->buckets[bucket];
    while (s && strcmp(s->id, session_id) != 0) {
        s = s->next;
    }
    
    if (!s) {
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_ENOENT;
    }
    
    /* 生成 JSON */
    char* json = NULL;
    int len = asprintf(&json,
        "{\"id\":\"%s\",\"created_ns\":%llu,\"last_active_ns\":%llu,\"metadata\":%s}",
        s->id,
        (unsigned long long)s->created_ns,
        (unsigned long long)s->last_active_ns,
        s->metadata ? s->metadata : "null");
    
    pthread_mutex_unlock(&mgr->lock);
    
    if (len < 0 || !json) {
        return AGENTOS_ENOMEM;
    }
    
    *out_info = json;
    return AGENTOS_SUCCESS;
}

agentos_error_t session_manager_touch_session(
    session_manager_t* mgr,
    const char* session_id) {
    
    if (!mgr || !session_id) return AGENTOS_EINVAL;
    
    size_t bucket = hash_session_id(session_id, mgr->bucket_count);
    
    pthread_mutex_lock(&mgr->lock);
    
    session_t* s = mgr->buckets[bucket];
    while (s && strcmp(s->id, session_id) != 0) {
        s = s->next;
    }
    
    if (!s) {
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_ENOENT;
    }
    
    s->last_active_ns = agentos_time_monotonic_ns();
    
    pthread_mutex_unlock(&mgr->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t session_manager_close_session(
    session_manager_t* mgr,
    const char* session_id) {
    
    if (!mgr || !session_id) return AGENTOS_EINVAL;
    
    size_t bucket = hash_session_id(session_id, mgr->bucket_count);
    
    pthread_mutex_lock(&mgr->lock);
    
    session_t** pp = &mgr->buckets[bucket];
    while (*pp && strcmp((*pp)->id, session_id) != 0) {
        pp = &(*pp)->next;
    }
    
    if (!*pp) {
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_ENOENT;
    }
    
    /* 从链表中移除 */
    session_t* s = *pp;
    *pp = s->next;
    
    atomic_fetch_sub(&mgr->session_count, 1);
    
    pthread_mutex_unlock(&mgr->lock);
    
    AGENTOS_LOG_DEBUG("Session closed: %s", s->id);
    
    free(s->metadata);
    free(s);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t session_manager_list_sessions(
    session_manager_t* mgr,
    char*** out_sessions,
    size_t* out_count) {
    
    if (!mgr || !out_sessions || !out_count) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    
    size_t count = atomic_load(&mgr->session_count);
    
    char** sessions = (char**)malloc(count * sizeof(char*));
    if (!sessions) {
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_ENOMEM;
    }
    
    size_t idx = 0;
    for (size_t i = 0; i < mgr->bucket_count && idx < count; i++) {
        session_t* s = mgr->buckets[i];
        while (s && idx < count) {
            sessions[idx] = strdup(s->id);
            if (!sessions[idx]) {
                /* 回滚 */
                for (size_t j = 0; j < idx; j++) {
                    free(sessions[j]);
                }
                free(sessions);
                pthread_mutex_unlock(&mgr->lock);
                return AGENTOS_ENOMEM;
            }
            idx++;
            s = s->next;
        }
    }
    
    pthread_mutex_unlock(&mgr->lock);
    
    *out_sessions = sessions;
    *out_count = idx;
    return AGENTOS_SUCCESS;
}

size_t session_manager_count(session_manager_t* mgr) {
    if (!mgr) return 0;
    return atomic_load(&mgr->session_count);
}
