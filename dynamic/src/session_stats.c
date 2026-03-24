size_t session_manager_count(session_manager_t* mgr) {
    if (!mgr) return 0;
    return atomic_load(&mgr->session_count);
}

agentos_error_t session_manager_get_stats(session_manager_t* mgr, char** out_json) {
    if (!mgr || !out_json) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->stats_lock);
    
    cJSON* stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(stats, "session_count", atomic_load(&mgr->session_count));
    cJSON_AddNumberToObject(stats, "memory_usage", atomic_load(&mgr->memory_usage));
    cJSON_AddNumberToObject(stats, "cleanup_count", atomic_load(&mgr->cleanup_count));
    cJSON_AddNumberToObject(stats, "max_sessions", mgr->max_sessions);
    cJSON_AddNumberToObject(stats, "timeout_sec", mgr->timeout_sec);
    cJSON_AddNumberToObject(stats, "avg_session_len", mgr->avg_session_len);
    cJSON_AddNumberToObject(stats, "bucket_count", mgr->bucket_count);
    
    char* json_str = cJSON_Print(stats);
    cJSON_Delete(stats);
    
    pthread_mutex_unlock(&mgr->stats_lock);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}