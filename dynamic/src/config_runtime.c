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
        free(mgr->config.server.host);
        mgr->config.server.host = strdup(json_value->valuestring);
    } else if (strcmp(key, "server.http_port") == 0 && cJSON_IsNumber(json_value)) {
        mgr->config.server.http_port = (uint16_t)json_value->valueint;
    } else if (strcmp(key, "server.ws_port") == 0 && cJSON_IsNumber(json_value)) {
        mgr->config.server.ws_port = (uint16_t)json_value->valueint;
    } else if (strcmp(key, "log.level") == 0 && cJSON_IsString(json_value)) {
        if (strcmp(json_value->valuestring, "DEBUG") == 0) mgr->config.log.level = LOG_LEVEL_DEBUG;
        else if (strcmp(json_value->valuestring, "INFO") == 0) mgr->config.log.level = LOG_LEVEL_INFO;
        else if (strcmp(json_value->valuestring, "WARN") == 0) mgr->config.log.level = LOG_LEVEL_WARN;
        else if (strcmp(json_value->valuestring, "ERROR") == 0) mgr->config.log.level = LOG_LEVEL_ERROR;
        else err = AGENTOS_EINVAL;
    } else if (strcmp(key, "security.enable_auth") == 0 && cJSON_IsBool(json_value)) {
        mgr->config.security.enable_auth = cJSON_IsTrue(json_value) ? 1 : 0;
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