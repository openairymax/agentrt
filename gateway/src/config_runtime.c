/**
 * @file config_runtime.c
 * @brief 运行时配置更新实现
 *
 * 提供运行时配置更新和监听器功能。
 *
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "manager.h"
#include "logger.h"

#include <cJSON.h>
#include <string.h>

/**
 * @brief 运行时更新配置值
 *
 * 支持更新的配置项：
 * - server.host: 服务器主机名
 * - server.http_port: HTTP端口
 * - server.ws_port: WebSocket端口
 * - log.level: 日志级别 (DEBUG/INFO/WARN/ERROR)
 * - security.enable_auth: 是否启用认证
 *
 * @param mgr 配置管理器句柄
 * @param key 配置键（如"server.host"）
 * @param value JSON格式的值
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效或键不存在
 * @return AGENTOS_ENOMEM 内存不足
 *
 * @threadsafe 此函数是线程安全的
 */
agentos_error_t config_manager_update_value(config_manager_t* mgr, const char* key, const char* value) {
    if (!mgr || !key || !value) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    
    /* 解析JSON值 */
    cJSON* json_value = cJSON_Parse(value);
    if (!json_value) {
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_EINVAL;
    }
    
    agentos_error_t err = AGENTOS_SUCCESS;
    
    /* 根据键更新配置 */
    if (strcmp(key, "server.host") == 0 && cJSON_IsString(json_value)) {
        char* new_host = strdup(json_value->valuestring);
        if (!new_host) {
            cJSON_Delete(json_value);
            pthread_mutex_unlock(&mgr->lock);
            return AGENTOS_ENOMEM;
        }
        free(mgr->manager.server.host);
        mgr->manager.server.host = new_host;
    } else if (strcmp(key, "server.http_port") == 0 && cJSON_IsNumber(json_value)) {
        mgr->manager.server.http_port = (uint16_t)json_value->valueint;
    } else if (strcmp(key, "server.ws_port") == 0 && cJSON_IsNumber(json_value)) {
        mgr->manager.server.ws_port = (uint16_t)json_value->valueint;
    } else if (strcmp(key, "log.level") == 0 && cJSON_IsString(json_value)) {
        if (strcmp(json_value->valuestring, "DEBUG") == 0) mgr->manager.log.log_level = LOG_LEVEL_DEBUG;
        else if (strcmp(json_value->valuestring, "INFO") == 0) mgr->manager.log.log_level = LOG_LEVEL_INFO;
        else if (strcmp(json_value->valuestring, "WARN") == 0) mgr->manager.log.log_level = LOG_LEVEL_WARN;
        else if (strcmp(json_value->valuestring, "ERROR") == 0) mgr->manager.log.log_level = LOG_LEVEL_ERROR;
        else err = AGENTOS_EINVAL;
    } else if (strcmp(key, "security.enable_auth") == 0 && cJSON_IsBool(json_value)) {
        mgr->manager.security.enable_auth = cJSON_IsTrue(json_value) ? 1 : 0;
    } else {
        err = AGENTOS_EINVAL;
    }
    
    cJSON_Delete(json_value);
    
    /* 触发回调 */
    if (err == AGENTOS_SUCCESS && mgr->watcher.callback) {
        mgr->watcher.callback(key, value, mgr->watcher.user_data);
    }
    
    pthread_mutex_unlock(&mgr->lock);
    
    if (err == AGENTOS_SUCCESS) {
        AGENTOS_LOG_INFO("Configuration updated: %s = %s", key, value);
    }
    
    return err;
}

/**
 * @brief 设置配置变更监听器
 *
 * 当配置值发生变化时，回调函数将被调用。
 *
 * @param mgr 配置管理器句柄
 * @param callback 回调函数指针
 * @param user_data 用户数据，将传递给回调函数
 * @return AGENTOS_SUCCESS 成功
 * @return AGENTOS_EINVAL 参数无效
 *
 * @threadsafe 此函数是线程安全的
 */
agentos_error_t config_manager_set_watcher(config_manager_t* mgr, 
                                          void (*callback)(const char* key, const char* value, void* user_data),
                                          void* user_data) {
    if (!mgr) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    mgr->watcher.callback = callback;
    mgr->watcher.user_data = user_data;
    pthread_mutex_unlock(&mgr->lock);
    
    return AGENTOS_SUCCESS;
}
