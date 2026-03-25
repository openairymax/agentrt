/**
 * @file config.c
 * @brief 配置管理器实现
 * 
 * 实现配置文件解析、环境变量覆盖、运行时更新等功能。
 * 支持JSON格式配置文件，支持热重载。
 * 
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "config.h"
#include "logger.h"

#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* ========== 配置管理器内部结构 ========== */

/**
 * @brief 配置管理器内部结构
 */
typedef struct config_manager {
    char* config_path;              /**< 配置文件路径 */
    dynamic_config_t config;        /**< 当前配置 */
    atomic_bool running;            /**< 运行标志 */
    pthread_t watcher_thread;       /**< 配置文件监视线程 */
    pthread_mutex_t lock;           /**< 配置锁 */
    pthread_cond_t change_cond;     /**< 变化条件变量 */
    
    /* 监听器 */
    struct {
        void (*callback)(const char* key, const char* value, void* user_data);
        void* user_data;
    } watcher;
} config_manager_t;

/* ========== 默认配置 ========== */

static const dynamic_config_t default_config = {
    .server = {
        .host = "127.0.0.1",
        .http_port = 8080,
        .ws_port = 8081,
        .metrics_port = 8082,
        .metrics_path = "/metrics",
        .max_request_size = 1024 * 1024,  /* 1MB */
        .max_connections = 1000,
        .request_timeout = 30,
        .session_timeout = 3600,
        .health_check_interval = 60
    },
    .log = {
        .log_file = NULL,
        .log_level = 3,  /* INFO */
        .log_rotation = 10 * 1024 * 1024,  /* 10MB */
        .log_backup_count = 5
    },
    .security = {
        .jwt_secret = NULL,
        .jwt_expire = 3600,
        .max_login_attempts = 5,
        .lockout_duration = 300,
        .allowed_origins = "*",
        .enable_auth = 0
    },
    .performance = {
        .thread_pool_size = 10,
        .connection_pool_size = 100,
        .request_queue_size = 1000,
        .response_buffer_size = 64 * 1024,  /* 64KB */
        .enable_compression = 1
    }
};

/* ========== 配置键映射 ========== */

static const char* config_keys[] = {
    "server.host",
    "server.http_port",
    "server.ws_port", 
    "server.metrics_port",
    "server.metrics_path",
    "server.max_request_size",
    "server.max_connections",
    "server.request_timeout",
    "server.session_timeout",
    "server.health_check_interval",
    "log.log_file",
    "log.log_level",
    "log.log_rotation",
    "log.log_backup_count",
    "security.jwt_secret",
    "security.jwt_expire",
    "security.max_login_attempts",
    "security.lockout_duration",
    "security.allowed_origins",
    "security.enable_auth",
    "performance.thread_pool_size",
    "performance.connection_pool_size",
    "performance.request_queue_size",
    "performance.response_buffer_size",
    "performance.enable_compression"
};

/* ========== 配置解析 ========== */

/**
 * @brief 解析服务器配置
 * @param json JSON对象
 * @param config 输出配置
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t parse_server_config(cJSON* json, server_config_t* config) {
    if (!json || !config) return AGENTOS_EINVAL;
    
    cJSON* host = cJSON_GetObjectItem(json, "host");
    if (host && cJSON_IsString(host)) {
        free(config->host);
        config->host = strdup(host->valuestring);
    }
    
    cJSON* http_port = cJSON_GetObjectItem(json, "http_port");
    if (http_port && cJSON_IsNumber(http_port)) {
        config->http_port = (uint16_t)http_port->valueint;
    }
    
    cJSON* ws_port = cJSON_GetObjectItem(json, "ws_port");
    if (ws_port && cJSON_IsNumber(ws_port)) {
        config->ws_port = (uint16_t)ws_port->valueint;
    }
    
    cJSON* metrics_port = cJSON_GetObjectItem(json, "metrics_port");
    if (metrics_port && cJSON_IsNumber(metrics_port)) {
        config->metrics_port = (uint16_t)metrics_port->valueint;
    }
    
    cJSON* metrics_path = cJSON_GetObjectItem(json, "metrics_path");
    if (metrics_path && cJSON_IsString(metrics_path)) {
        free(config->metrics_path);
        config->metrics_path = strdup(metrics_path->valuestring);
    }
    
    cJSON* max_request_size = cJSON_GetObjectItem(json, "max_request_size");
    if (max_request_size && cJSON_IsNumber(max_request_size)) {
        config->max_request_size = (size_t)max_request_size->valueint;
    }
    
    cJSON* max_connections = cJSON_GetObjectItem(json, "max_connections");
    if (max_connections && cJSON_IsNumber(max_connections)) {
        config->max_connections = (size_t)max_connections->valueint;
    }
    
    cJSON* request_timeout = cJSON_GetObjectItem(json, "request_timeout");
    if (request_timeout && cJSON_IsNumber(request_timeout)) {
        config->request_timeout = (uint32_t)request_timeout->valueint;
    }
    
    cJSON* session_timeout = cJSON_GetObjectItem(json, "session_timeout");
    if (session_timeout && cJSON_IsNumber(session_timeout)) {
        config->session_timeout = (uint32_t)session_timeout->valueint;
    }
    
    cJSON* health_check_interval = cJSON_GetObjectItem(json, "health_check_interval");
    if (health_check_interval && cJSON_IsNumber(health_check_interval)) {
        config->health_check_interval = (uint32_t)health_check_interval->valueint;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 解析日志配置
 * @param json JSON对象
 * @param config 输出配置
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t parse_log_config(cJSON* json, log_config_t* config) {
    if (!json || !config) return AGENTOS_EINVAL;
    
    cJSON* log_file = cJSON_GetObjectItem(json, "log_file");
    if (log_file && cJSON_IsString(log_file)) {
        free(config->log_file);
        config->log_file = strdup(log_file->valuestring);
    }
    
    cJSON* log_level = cJSON_GetObjectItem(json, "log_level");
    if (log_level && cJSON_IsNumber(log_level)) {
        config->log_level = log_level->valueint;
    }
    
    cJSON* log_rotation = cJSON_GetObjectItem(json, "log_rotation");
    if (log_rotation && cJSON_IsNumber(log_rotation)) {
        config->log_rotation = (int)log_rotation->valueint;
    }
    
    cJSON* log_backup_count = cJSON_GetObjectItem(json, "log_backup_count");
    if (log_backup_count && cJSON_IsNumber(log_backup_count)) {
        config->log_backup_count = (int)log_backup_count->valueint;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 解析安全配置
 * @param json JSON对象
 * @param config 输出配置
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t parse_security_config(cJSON* json, security_config_t* config) {
    if (!json || !config) return AGENTOS_EINVAL;
    
    cJSON* jwt_secret = cJSON_GetObjectItem(json, "jwt_secret");
    if (jwt_secret && cJSON_IsString(jwt_secret)) {
        free(config->jwt_secret);
        config->jwt_secret = strdup(jwt_secret->valuestring);
    }
    
    cJSON* jwt_expire = cJSON_GetObjectItem(json, "jwt_expire");
    if (jwt_expire && cJSON_IsNumber(jwt_expire)) {
        config->jwt_expire = (uint32_t)jwt_expire->valueint;
    }
    
    cJSON* max_login_attempts = cJSON_GetObjectItem(json, "max_login_attempts");
    if (max_login_attempts && cJSON_IsNumber(max_login_attempts)) {
        config->max_login_attempts = (uint32_t)max_login_attempts->valueint;
    }
    
    cJSON* lockout_duration = cJSON_GetObjectItem(json, "lockout_duration");
    if (lockout_duration && cJSON_IsNumber(lockout_duration)) {
        config->lockout_duration = (uint32_t)lockout_duration->valueint;
    }
    
    cJSON* allowed_origins = cJSON_GetObjectItem(json, "allowed_origins");
    if (allowed_origins && cJSON_IsString(allowed_origins)) {
        free(config->allowed_origins);
        config->allowed_origins = strdup(allowed_origins->valuestring);
    }
    
    cJSON* enable_auth = cJSON_GetObjectItem(json, "enable_auth");
    if (enable_auth && cJSON_IsBool(enable_auth)) {
        config->enable_auth = cJSON_IsTrue(enable_auth) ? 1 : 0;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 解析性能配置
 * @param json JSON对象
 * @param config 输出配置
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t parse_performance_config(cJSON* json, performance_config_t* config) {
    if (!json || !config) return AGENTOS_EINVAL;
    
    cJSON* thread_pool_size = cJSON_GetObjectItem(json, "thread_pool_size");
    if (thread_pool_size && cJSON_IsNumber(thread_pool_size)) {
        config->thread_pool_size = (size_t)thread_pool_size->valueint;
    }
    
    cJSON* connection_pool_size = cJSON_GetObjectItem(json, "connection_pool_size");
    if (connection_pool_size && cJSON_IsNumber(connection_pool_size)) {
        config->connection_pool_size = (size_t)connection_pool_size->valueint;
    }
    
    cJSON* request_queue_size = cJSON_GetObjectItem(json, "request_queue_size");
    if (request_queue_size && cJSON_IsNumber(request_queue_size)) {
        config->request_queue_size = (uint32_t)request_queue_size->valueint;
    }
    
    cJSON* response_buffer_size = cJSON_GetObjectItem(json, "response_buffer_size");
    if (response_buffer_size && cJSON_IsNumber(response_buffer_size)) {
        config->response_buffer_size = (uint32_t)response_buffer_size->valueint;
    }
    
    cJSON* enable_compression = cJSON_GetObjectItem(json, "enable_compression");
    if (enable_compression && cJSON_IsBool(enable_compression)) {
        config->enable_compression = cJSON_IsTrue(enable_compression) ? 1 : 0;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 应用环境变量覆盖
 * @param config 配置
 */
static void apply_env_overrides(dynamic_config_t* config) {
    if (!config) return;
    
    /* 服务器配置环境变量 */
    const char* env_host = getenv("AGENTOS_SERVER_HOST");
    if (env_host) {
        free(config->server.host);
        config->server.host = strdup(env_host);
        AGENTOS_LOG_INFO("Override server host from env: %s", env_host);
    }
    
    const char* env_http_port = getenv("AGENTOS_HTTP_PORT");
    if (env_http_port) {
        config->server.http_port = (uint16_t)atoi(env_http_port);
        AGENTOS_LOG_INFO("Override HTTP port from env: %s", env_http_port);
    }
    
    const char* env_ws_port = getenv("AGENTOS_WS_PORT");
    if (env_ws_port) {
        config->server.ws_port = (uint16_t)atoi(env_ws_port);
        AGENTOS_LOG_INFO("Override WebSocket port from env: %s", env_ws_port);
    }
    
    const char* env_metrics_port = getenv("AGENTOS_METRICS_PORT");
    if (env_metrics_port) {
        config->server.metrics_port = (uint16_t)atoi(env_metrics_port);
        AGENTOS_LOG_INFO("Override metrics port from env: %s", env_metrics_port);
    }
    
    /* 日志配置环境变量 */
    const char* env_log_level = getenv("AGENTOS_LOG_LEVEL");
    if (env_log_level) {
        if (strcmp(env_log_level, "DEBUG") == 0) config->log.log_level = LOG_LEVEL_DEBUG;
        else if (strcmp(env_log_level, "INFO") == 0) config->log.log_level = LOG_LEVEL_INFO;
        else if (strcmp(env_log_level, "WARN") == 0) config->log.log_level = LOG_LEVEL_WARN;
        else if (strcmp(env_log_level, "ERROR") == 0) config->log.log_level = LOG_LEVEL_ERROR;
        AGENTOS_LOG_INFO("Override log level from env: %s", env_log_level);
    }
    
    const char* env_log_path = getenv("AGENTOS_LOG_PATH");
    if (env_log_path) {
        free(config->log.log_file);
        config->log.log_file = strdup(env_log_path);
        AGENTOS_LOG_INFO("Override log path from env: %s", env_log_path);
    }
    
    /* 安全配置环境变量 */
    const char* env_jwt_secret = getenv("AGENTOS_JWT_SECRET");
    if (env_jwt_secret) {
        free(config->security.jwt_secret);
        config->security.jwt_secret = strdup(env_jwt_secret);
        AGENTOS_LOG_INFO("Override JWT secret from env");
    }
    
    const char* env_jwt_expire = getenv("AGENTOS_JWT_EXPIRE");
    if (env_jwt_expire) {
        config->security.jwt_expire = (uint32_t)atoi(env_jwt_expire);
        AGENTOS_LOG_INFO("Override JWT expire from env: %s", env_jwt_expire);
    }
    
    const char* env_enable_auth = getenv("AGENTOS_ENABLE_AUTH");
    if (env_enable_auth) {
        config->security.enable_auth = atoi(env_enable_auth) ? 1 : 0;
        AGENTOS_LOG_INFO("Override auth enable from env: %s", env_enable_auth);
    }
    
    /* 性能配置环境变量 */
    const char* env_thread_pool = getenv("AGENTOS_THREAD_POOL_SIZE");
    if (env_thread_pool) {
        config->performance.thread_pool_size = (uint32_t)atoi(env_thread_pool);
        AGENTOS_LOG_INFO("Override thread pool size from env: %s", env_thread_pool);
    }
    
    const char* env_conn_pool = getenv("AGENTOS_CONNECTION_POOL_SIZE");
    if (env_conn_pool) {
        config->performance.connection_pool_size = (uint32_t)atoi(env_conn_pool);
        AGENTOS_LOG_INFO("Override connection pool size from env: %s", env_conn_pool);
    }
}

/**
 * @brief 解析配置文件
 * @param config_path 配置文件路径
 * @param config 输出配置
 * @return AGENTOS_SUCCESS 成功
 */
static agentos_error_t parse_config_file(const char* config_path, dynamic_config_t* config) {
    if (!config_path || !config) return AGENTOS_EINVAL;
    
    /* 读取配置文件 */
    FILE* file = fopen(config_path, "r");
    if (!file) {
        AGENTOS_LOG_ERROR("Failed to open config file: %s", config_path);
        return AGENTOS_EIO;
    }
    
    /* 获取文件大小 */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    /* 读取文件内容 */
    char* file_content = malloc(file_size + 1);
    if (!file_content) {
        fclose(file);
        return AGENTOS_ENOMEM;
    }
    
    size_t bytes_read = fread(file_content, 1, file_size, file);
    file_content[bytes_read] = '\0';
    fclose(file);
    
    /* 解析JSON */
    cJSON* json = cJSON_Parse(file_content);
    free(file_content);
    
    if (!json) {
        AGENTOS_LOG_ERROR("Failed to parse config JSON");
        return AGENTOS_EINVAL;
    }
    
    /* 解析各个配置部分 */
    cJSON* server_json = cJSON_GetObjectItem(json, "server");
    if (server_json) {
        parse_server_config(server_json, &config->server);
    }
    
    cJSON* log_json = cJSON_GetObjectItem(json, "log");
    if (log_json) {
        parse_log_config(log_json, &config->log);
    }
    
    cJSON* security_json = cJSON_GetObjectItem(json, "security");
    if (security_json) {
        parse_security_config(security_json, &config->security);
    }
    
    cJSON* performance_json = cJSON_GetObjectItem(json, "performance");
    if (performance_json) {
        parse_performance_config(performance_json, &config->performance);
    }
    
    cJSON_Delete(json);
    
    /* 应用环境变量覆盖 */
    apply_env_overrides(config);
    
    return AGENTOS_SUCCESS;
}

/* ========== 配置文件监视线程 ========== */

/**
 * @brief 检查文件是否被修改
 * @param file_path 文件路径
 * @param last_mtime 上次修改时间
 * @return 1 被修改，0 未修改
 */
static int file_was_modified(const char* file_path, time_t* last_mtime) {
    struct stat st;
    if (stat(file_path, &st) != 0) {
        return 0;
    }
    
    time_t current_mtime = st.st_mtime;
    if (last_mtime && current_mtime > *last_mtime) {
        *last_mtime = current_mtime;
        return 1;
    }
    
    return 0;
}

/**
 * @brief 配置文件监视线程函数
 * @param arg 配置管理器
 * @return NULL
 */
static void* config_watcher_thread(void* arg) {
    config_manager_t* mgr = (config_manager_t*)arg;
    time_t last_mtime = 0;
    
    if (mgr->config_path) {
        /* 获取初始修改时间 */
        struct stat st;
        if (stat(mgr->config_path, &st) == 0) {
            last_mtime = st.st_mtime;
        }
    }
    
    while (atomic_load(&mgr->running)) {
        /* 检查配置文件变化 */
        if (mgr->config_path && file_was_modified(mgr->config_path, &last_mtime)) {
            AGENTOS_LOG_INFO("Config file changed, reloading...");
            
            /* 重新加载配置 */
            dynamic_config_t new_config;
            memcpy(&new_config, &default_config, sizeof(dynamic_config_t));
            
            agentos_error_t err = parse_config_file(mgr->config_path, &new_config);
            if (err == AGENTOS_SUCCESS) {
                pthread_mutex_lock(&mgr->lock);
                
                /* 通知监听器 */
                if (mgr->watcher.callback) {
                    mgr->watcher.callback("config.reload", "true", mgr->watcher.user_data);
                }
                
                /* 更新配置 */
                memcpy(&mgr->config, &new_config, sizeof(dynamic_config_t));
                
                pthread_mutex_unlock(&mgr->lock);
                
                AGENTOS_LOG_INFO("Configuration reloaded successfully");
            } else {
                AGENTOS_LOG_ERROR("Failed to reload configuration");
            }
        }
        
        /* 等待一段时间 */
        for (int i = 0; i < 5 && atomic_load(&mgr->running); i++) {
            sleep(1);
        }
    }
    
    return NULL;
}

/* ========== 公共API实现 ========== */

config_manager_t* config_manager_create(const char* config_path) {
    /* 分配管理器 */
    config_manager_t* mgr = (config_manager_t*)calloc(1, sizeof(config_manager_t));
    if (!mgr) return NULL;
    
    /* 初始化配置 */
    memcpy(&mgr->config, &default_config, sizeof(dynamic_config_t));
    
    /* 设置配置文件路径 */
    if (config_path) {
        mgr->config_path = strdup(config_path);
        
        /* 加载配置文件 */
        agentos_error_t err = parse_config_file(config_path, &mgr->config);
        if (err != AGENTOS_SUCCESS) {
            AGENTOS_LOG_WARN("Failed to load config file, using defaults");
        }
    }
    
    /* 初始化同步原语 */
    if (pthread_mutex_init(&mgr->lock, NULL) != 0) {
        free(mgr->config_path);
        free(mgr);
        return NULL;
    }
    
    if (pthread_cond_init(&mgr->change_cond, NULL) != 0) {
        pthread_mutex_destroy(&mgr->lock);
        free(mgr->config_path);
        free(mgr);
        return NULL;
    }
    
    atomic_init(&mgr->running, true);
    
    /* 启动监视线程 */
    if (pthread_create(&mgr->watcher_thread, NULL, config_watcher_thread, mgr) != 0) {
        pthread_cond_destroy(&mgr->change_cond);
        pthread_mutex_destroy(&mgr->lock);
        free(mgr->config_path);
        free(mgr);
        return NULL;
    }
    
    AGENTOS_LOG_INFO("Config manager created");
    return mgr;
}

void config_manager_destroy(config_manager_t* mgr) {
    if (!mgr) return;
    
    /* 停止监视线程 */
    atomic_store(&mgr->running, false);
    pthread_join(mgr->watcher_thread, NULL);
    
    /* 销毁同步原语 */
    pthread_cond_destroy(&mgr->change_cond);
    pthread_mutex_destroy(&mgr->lock);
    
    /* 释放配置 */
    free(mgr->config.server.host);
    free(mgr->config.server.metrics_path);
    free(mgr->config.log.log_file);
    free(mgr->config.security.jwt_secret);
    free(mgr->config.security.allowed_origins);
    free(mgr->config_path);
    
    free(mgr);
    
    AGENTOS_LOG_INFO("Config manager destroyed");
}

agentos_error_t config_manager_reload(config_manager_t* mgr, const char* config_path) {
    if (!mgr) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    
    /* 更新配置文件路径 */
    if (config_path) {
        free(mgr->config_path);
        mgr->config_path = strdup(config_path);
    }
    
    /* 重新加载配置 */
    dynamic_config_t new_config;
    memcpy(&new_config, &default_config, sizeof(dynamic_config_t));
    
    agentos_error_t err = parse_config_file(mgr->config_path, &new_config);
    if (err == AGENTOS_SUCCESS) {
        memcpy(&mgr->config, &new_config, sizeof(dynamic_config_t));
        
        /* 通知监听器 */
        if (mgr->watcher.callback) {
            mgr->watcher.callback("config.reload", "true", mgr->watcher.user_data);
        }
        
        pthread_mutex_unlock(&mgr->lock);
        AGENTOS_LOG_INFO("Configuration reloaded successfully");
        return AGENTOS_SUCCESS;
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return err;
}

agentos_error_t config_manager_get_config(config_manager_t* mgr, dynamic_config_t* out_config) {
    if (!mgr || !out_config) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    memcpy(out_config, &mgr->config, sizeof(dynamic_config_t));
    pthread_mutex_unlock(&mgr->lock);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t config_manager_get_server_config(config_manager_t* mgr, server_config_t* out_config) {
    if (!mgr || !out_config) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    memcpy(out_config, &mgr->config.server, sizeof(server_config_t));
    pthread_mutex_unlock(&mgr->lock);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t config_manager_get_log_config(config_manager_t* mgr, log_config_t* out_config) {
    if (!mgr || !out_config) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    memcpy(out_config, &mgr->config.log, sizeof(log_config_t));
    pthread_mutex_unlock(&mgr->lock);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t config_manager_get_security_config(config_manager_t* mgr, security_config_t* out_config) {
    if (!mgr || !out_config) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    memcpy(out_config, &mgr->config.security, sizeof(security_config_t));
    pthread_mutex_unlock(&mgr->lock);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t config_manager_get_performance_config(config_manager_t* mgr, performance_config_t* out_config) {
    if (!mgr || !out_config) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    memcpy(out_config, &mgr->config.performance, sizeof(performance_config_t));
    pthread_mutex_unlock(&mgr->lock);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t config_manager_set_value(config_manager_t* mgr, const char* key, const char* value) {
    if (!mgr || !key || !value) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    
    /* 设置配置值 */
    if (strcmp(key, "server.host") == 0) {
        free(mgr->config.server.host);
        mgr->config.server.host = strdup(value);
    } else if (strcmp(key, "server.http_port") == 0) {
        mgr->config.server.http_port = (uint16_t)atoi(value);
    } else if (strcmp(key, "server.ws_port") == 0) {
        mgr->config.server.ws_port = (uint16_t)atoi(value);
    } else if (strcmp(key, "log.log_level") == 0) {
        mgr->config.log.log_level = atoi(value);
    } else if (strcmp(key, "security.enable_auth") == 0) {
        mgr->config.security.enable_auth = atoi(value);
    } else {
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_ENOTSUP;
    }
    
    /* 通知监听器 */
    if (mgr->watcher.callback) {
        mgr->watcher.callback(key, value, mgr->watcher.user_data);
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return AGENTOS_SUCCESS;
}

agentos_error_t config_manager_watch_changes(config_manager_t* mgr,
                                            void (*callback)(const char* key, const char* value, void* user_data),
                                            void* user_data) {
    if (!mgr) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    mgr->watcher.callback = callback;
    mgr->watcher.user_data = user_data;
    pthread_mutex_unlock(&mgr->lock);
    
    return AGENTOS_SUCCESS;
}

agentos_error_t config_manager_export_json(config_manager_t* mgr, char** out_json) {
    if (!mgr || !out_json) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    
    cJSON* json = cJSON_CreateObject();
    
    /* 服务器配置 */
    cJSON* server_json = cJSON_CreateObject();
    cJSON_AddStringToObject(server_json, "host", mgr->config.server.host);
    cJSON_AddNumberToObject(server_json, "http_port", mgr->config.server.http_port);
    cJSON_AddNumberToObject(server_json, "ws_port", mgr->config.server.ws_port);
    cJSON_AddNumberToObject(server_json, "metrics_port", mgr->config.server.metrics_port);
    cJSON_AddStringToObject(server_json, "metrics_path", mgr->config.server.metrics_path);
    cJSON_AddNumberToObject(server_json, "max_request_size", mgr->config.server.max_request_size);
    cJSON_AddNumberToObject(server_json, "max_connections", mgr->config.server.max_connections);
    cJSON_AddNumberToObject(server_json, "request_timeout", mgr->config.server.request_timeout);
    cJSON_AddNumberToObject(server_json, "session_timeout", mgr->config.server.session_timeout);
    cJSON_AddNumberToObject(server_json, "health_check_interval", mgr->config.server.health_check_interval);
    cJSON_AddItemToObject(json, "server", server_json);
    
    /* 日志配置 */
    cJSON* log_json = cJSON_CreateObject();
    cJSON_AddStringToObject(log_json, "log_file", mgr->config.log.log_file);
    cJSON_AddNumberToObject(log_json, "log_level", mgr->config.log.log_level);
    cJSON_AddNumberToObject(log_json, "log_rotation", mgr->config.log.log_rotation);
    cJSON_AddNumberToObject(log_json, "log_backup_count", mgr->config.log.log_backup_count);
    cJSON_AddItemToObject(json, "log", log_json);
    
    /* 安全配置 */
    cJSON* security_json = cJSON_CreateObject();
    cJSON_AddStringToObject(security_json, "jwt_secret", mgr->config.security.jwt_secret);
    cJSON_AddNumberToObject(security_json, "jwt_expire", mgr->config.security.jwt_expire);
    cJSON_AddNumberToObject(security_json, "max_login_attempts", mgr->config.security.max_login_attempts);
    cJSON_AddNumberToObject(security_json, "lockout_duration", mgr->config.security.lockout_duration);
    cJSON_AddStringToObject(security_json, "allowed_origins", mgr->config.security.allowed_origins);
    cJSON_AddNumberToObject(security_json, "enable_auth", mgr->config.security.enable_auth);
    cJSON_AddItemToObject(json, "security", security_json);
    
    /* 性能配置 */
    cJSON* performance_json = cJSON_CreateObject();
    cJSON_AddNumberToObject(performance_json, "thread_pool_size", mgr->config.performance.thread_pool_size);
    cJSON_AddNumberToObject(performance_json, "connection_pool_size", mgr->config.performance.connection_pool_size);
    cJSON_AddNumberToObject(performance_json, "request_queue_size", mgr->config.performance.request_queue_size);
    cJSON_AddNumberToObject(performance_json, "response_buffer_size", mgr->config.performance.response_buffer_size);
    cJSON_AddNumberToObject(performance_json, "enable_compression", mgr->config.performance.enable_compression);
    cJSON_AddItemToObject(json, "performance", performance_json);
    
    char* json_str = cJSON_Print(json);
    cJSON_Delete(json);
    
    pthread_mutex_unlock(&mgr->lock);
    
    *out_json = json_str;
    return AGENTOS_SUCCESS;
}

agentos_error_t config_manager_validate(config_manager_t* mgr, char** out_error) {
    if (!mgr) return AGENTOS_EINVAL;
    
    pthread_mutex_lock(&mgr->lock);
    
    /* 验证服务器配置 */
    if (!mgr->config.server.host) {
        if (out_error) *out_error = strdup("server.host is required");
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_EINVAL;
    }
    
    if (mgr->config.server.http_port == 0) {
        if (out_error) *out_error = strdup("server.http_port is required");
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_EINVAL;
    }
    
    if (mgr->config.server.max_request_size == 0) {
        if (out_error) *out_error = strdup("server.max_request_size must be greater than 0");
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_EINVAL;
    }
    
    if (mgr->config.server.max_connections == 0) {
        if (out_error) *out_error = strdup("server.max_connections must be greater than 0");
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_EINVAL;
    }
    
    /* 验证安全配置 */
    if (mgr->config.security.enable_auth && !mgr->config.security.jwt_secret) {
        if (out_error) *out_error = strdup("security.jwt_secret is required when authentication is enabled");
        pthread_mutex_unlock(&mgr->lock);
        return AGENTOS_EINVAL;
    }
    
    pthread_mutex_unlock(&mgr->lock);
    return AGENTOS_SUCCESS;
}