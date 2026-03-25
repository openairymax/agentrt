/**
 * @file session_stats.c
 * @brief 会话统计信息实现
 *
 * 提供会话管理器的统计信息获取功能。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "session.h"
#include <cJSON.h>

/**
 * @brief 获取当前活跃会话数
 *
 * @param mgr 会话管理器句柄
 * @return 活跃会话数，无效参数返回0
 */
size_t session_manager_count(session_manager_t* mgr) {
    if (!mgr) return 0;
    return atomic_load(&mgr->session_count);
}

/**
 * @brief 获取会话管理器统计信息
 *
 * 返回JSON格式的统计信息，包含：
 * - session_count: 当前会话数
 * - memory_usage: 内存使用量（字节）
 * - cleanup_count: 清理计数器
 * - max_sessions: 最大会话数
 * - timeout_sec: 超时秒数
 * - avg_session_len: 平均会话长度
 * - bucket_count: 哈希桶数量
 *
 * @param mgr 会话管理器句柄
 * @param[out] out_json 输出JSON字符串（调用者负责free）
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 * @return AGENTOS_ENOMEM 内存不足
 *
 * @threadsafe 此函数是线程安全的
 */
agentos_error_t session_manager_get_stats(session_manager_t* mgr, char** out_json) {
    if (!mgr || !out_json) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->stats_lock);
    
    cJSON* stats = cJSON_CreateObject();
    if (!stats) {
        pthread_mutex_unlock(&mgr->stats_lock);
        return AGENTOS_ENOMEM;
    }
    
    cJSON_AddNumberToObject(stats, "session_count", (double)atomic_load(&mgr->session_count));
    cJSON_AddNumberToObject(stats, "memory_usage", (double)atomic_load(&mgr->memory_usage));
    cJSON_AddNumberToObject(stats, "cleanup_count", (double)atomic_load(&mgr->cleanup_count));
    cJSON_AddNumberToObject(stats, "max_sessions", (double)mgr->max_sessions);
    cJSON_AddNumberToObject(stats, "timeout_sec", (double)mgr->timeout_sec);
    cJSON_AddNumberToObject(stats, "avg_session_len", (double)mgr->avg_session_len);
    cJSON_AddNumberToObject(stats, "bucket_count", (double)mgr->bucket_count);
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    pthread_mutex_unlock(&mgr->stats_lock);
    
    if (!json_str) return AGENTOS_ENOMEM;
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}
