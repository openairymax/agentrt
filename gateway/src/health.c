/**
 * @file health.c
 * @brief 健康检查器实现
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "health.h"
#include "logger.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 最大检查项数量 */
#define MAX_CHECKS 32

/* 检查项结构 */
typedef struct health_check {
    char   name[64];           /**< 检查项名称 */
    int  (*check_fn)(void*);   /**< 检查函数 */
    void*  user_data;          /**< 用户数据 */
    int    last_result;        /**< 最后检查结果 */
    uint64_t last_check_ns;    /**< 最后检查时间 */
} health_check_t;

/* 健康检查器结构 */
struct health_checker {
    health_check_t    checks[MAX_CHECKS];  /**< 检查项数组 */
    size_t            check_count;         /**< 检查项数量 */
    uint32_t          interval_sec;        /**< 检查间隔 */
    
    pthread_mutex_t   lock;                /**< 锁 */
    pthread_t         thread;              /**< 检查线程 */
    atomic_bool       running;             /**< 运行标志 */
    pthread_cond_t    cond;                /**< 条件变量 */
    
    atomic_int        overall_status;      /**< 总体状态 */
    uint64_t          start_time_ns;       /**< 启动时间 */
};

/* ========== 检查线程 ========== */

/**
 * @brief 健康检查线程函数
 */
static void* health_check_thread(void* arg) {
    health_checker_t* checker = (health_checker_t*)arg;
    
    AGENTOS_LOG_DEBUG("Health check thread started");
    
    while (atomic_load(&checker->running)) {
        /* 计算等待时间 */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += checker->interval_sec;
        
        /* 等待或被唤醒 */
        pthread_mutex_lock(&checker->lock);
        int ret = pthread_cond_timedwait(&checker->cond, &checker->lock, &ts);
        (void)ret;
        pthread_mutex_unlock(&checker->lock);
        
        if (!atomic_load(&checker->running)) {
            break;
        }
        
        /* 执行所有检查 */
        int worst_status = 0;  /* 0 = healthy */
        uint64_t now_ns = agentos_time_monotonic_ns();
        
        pthread_mutex_lock(&checker->lock);
        
        for (size_t i = 0; i < checker->check_count; i++) {
            health_check_t* check = &checker->checks[i];
            
            if (check->check_fn) {
                int result = check->check_fn(check->user_data);
                check->last_result = result;
                check->last_check_ns = now_ns;
                
                /* 记录最差状态 */
                if (result > worst_status) {
                    worst_status = result;
                }
                
                if (result != 0) {
                    AGENTOS_LOG_WARN("Health check '%s' failed: %d", 
                        check->name, result);
                }
            }
        }
        
        pthread_mutex_unlock(&checker->lock);
        
        /* 更新总体状态 */
        atomic_store(&checker->overall_status, worst_status);
    }
    
    AGENTOS_LOG_DEBUG("Health check thread stopped");
    return NULL;
}

/* ========== 公共 API 实现 ========== */

health_checker_t* health_checker_create(uint32_t check_interval_sec) {
    if (check_interval_sec == 0) {
        check_interval_sec = 30;  /* 默认 30 秒 */
    }
    
    health_checker_t* checker = (health_checker_t*)calloc(1, sizeof(health_checker_t));
    if (!checker) return NULL;
    
    checker->interval_sec = check_interval_sec;
    checker->start_time_ns = agentos_time_monotonic_ns();
    
    /* 初始化同步原语 */
    if (pthread_mutex_init(&checker->lock, NULL) != 0) {
        free(checker);
        return NULL;
    }
    
    if (pthread_cond_init(&checker->cond, NULL) != 0) {
        pthread_mutex_destroy(&checker->lock);
        free(checker);
        return NULL;
    }
    
    atomic_init(&checker->running, true);
    atomic_init(&checker->overall_status, HEALTH_STATUS_HEALTHY);
    
    /* 启动检查线程 */
    if (pthread_create(&checker->thread, NULL, health_check_thread, checker) != 0) {
        pthread_cond_destroy(&checker->cond);
        pthread_mutex_destroy(&checker->lock);
        free(checker);
        return NULL;
    }
    
    AGENTOS_LOG_INFO("Health checker created: interval=%us", check_interval_sec);
    return checker;
}

void health_checker_destroy(health_checker_t* checker) {
    if (!checker) return;
    
    /* 停止线程 */
    atomic_store(&checker->running, false);
    pthread_cond_broadcast(&checker->cond);
    pthread_join(checker->thread, NULL);
    
    /* 销毁同步原语 */
    pthread_cond_destroy(&checker->cond);
    pthread_mutex_destroy(&checker->lock);
    
    free(checker);
    AGENTOS_LOG_INFO("Health checker destroyed");
}

health_status_t health_checker_get_status(health_checker_t* checker) {
    if (!checker) return HEALTH_STATUS_UNHEALTHY;
    return (health_status_t)atomic_load(&checker->overall_status);
}

agentos_error_t health_checker_get_report(
    health_checker_t* checker,
    char** out_json) {
    
    if (!checker || !out_json) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&checker->lock);
    
    /* 计算运行时间 */
    uint64_t now_ns = agentos_time_monotonic_ns();
    uint64_t uptime_sec = (now_ns - checker->start_time_ns) / 1000000000ULL;
    
    /* 构建检查项 JSON */
    char* checks_json = NULL;
    size_t checks_size = 256 * checker->check_count + 64;
    checks_json = (char*)malloc(checks_size);
    
    if (!checks_json) {
        pthread_mutex_unlock(&checker->lock);
        return AGENTOS_ENOMEM;
    }
    
    char* p = checks_json;
    size_t remaining = checks_size;
    
    p += snprintf(p, remaining, "[");
    remaining = checks_size - (p - checks_json);
    
    for (size_t i = 0; i < checker->check_count; i++) {
        health_check_t* check = &checker->checks[i];
        if (i > 0) {
            int written = snprintf(p, remaining, ",");
            p += written;
            remaining = checks_size - (p - checks_json);
        }
        int written = snprintf(p, remaining,
            "{\"name\":\"%s\",\"status\":%s,\"last_check_ns\":%llu}",
            check->name,
            check->last_result == 0 ? "\"healthy\"" : "\"unhealthy\"",
            (unsigned long long)check->last_check_ns);
        p += written;
        remaining = checks_size - (p - checks_json);
    }
    
    snprintf(p, remaining, "]");
    
    pthread_mutex_unlock(&checker->lock);
    
    /* 构建完整报告 */
    char* json = NULL;
    int len = asprintf(&json,
        "{"
        "\"status\":\"%s\","
        "\"uptime_sec\":%llu,"
        "\"checks\":%s"
        "}",
        atomic_load(&checker->overall_status) == 0 ? "healthy" :
        atomic_load(&checker->overall_status) == 1 ? "degraded" : "unhealthy",
        (unsigned long long)uptime_sec,
        checks_json);
    
    free(checks_json);
    
    if (len < 0 || !json) {
        return AGENTOS_ENOMEM;
    }
    
    *out_json = json;
    return AGENTOS_SUCCESS;
}

agentos_error_t health_checker_register(
    health_checker_t* checker,
    const char* name,
    int (*check_fn)(void* user_data),
    void* user_data) {
    
    if (!checker || !name || !check_fn) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&checker->lock);
    
    /* 检查是否已存在 */
    for (size_t i = 0; i < checker->check_count; i++) {
        if (strcmp(checker->checks[i].name, name) == 0) {
            /* 更新 */
            checker->checks[i].check_fn = check_fn;
            checker->checks[i].user_data = user_data;
            pthread_mutex_unlock(&checker->lock);
            return AGENTOS_SUCCESS;
        }
    }
    
    /* 检查是否已满 */
    if (checker->check_count >= MAX_CHECKS) {
        pthread_mutex_unlock(&checker->lock);
        return AGENTOS_EBUSY;
    }
    
    /* 添加新检查项 */
    health_check_t* check = &checker->checks[checker->check_count++];
    strncpy(check->name, name, sizeof(check->name) - 1);
    check->name[sizeof(check->name) - 1] = '\0';
    check->check_fn = check_fn;
    check->user_data = user_data;
    check->last_result = 0;
    check->last_check_ns = 0;
    
    pthread_mutex_unlock(&checker->lock);
    
    AGENTOS_LOG_DEBUG("Health check registered: %s", name);
    return AGENTOS_SUCCESS;
}

agentos_error_t health_checker_unregister(
    health_checker_t* checker,
    const char* name) {
    
    if (!checker || !name) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&checker->lock);
    
    for (size_t i = 0; i < checker->check_count; i++) {
        if (strcmp(checker->checks[i].name, name) == 0) {
            /* 移动后续元素 */
            for (size_t j = i; j < checker->check_count - 1; j++) {
                checker->checks[j] = checker->checks[j + 1];
            }
            checker->check_count--;
            
            pthread_mutex_unlock(&checker->lock);
            AGENTOS_LOG_DEBUG("Health check unregistered: %s", name);
            return AGENTOS_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&checker->lock);
    return AGENTOS_ENOENT;
}
