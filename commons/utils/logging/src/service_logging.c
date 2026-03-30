/**
 * @file service_logging.c
 * @brief 统一分层日志系统服务层实�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一分层日志系统的服务层功能，提供：
 * 1. 日志轮转和归�? * 2. 日志过滤和格式化
 * 3. 日志传输和输�? * 4. 监控统计和管理接�? *
 * 注意：这是一个简化实现，提供基本功能�? * 生产环境应使用完整实现以获得所有高级功能�? */

#include "service_logging.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../utils/memory/include/memory_compat.h"
#include "../../utils/string/include/string_compat.h"
#include <string.h>
#include <time.h>
#include <pthread.h>

/* ==================== 内部常量定义 ==================== */

/** 默认工作线程数量 */
static const int DEFAULT_WORKER_THREADS = 2;

/** 默认配置重载间隔（秒�?*/
static const int DEFAULT_CONFIG_RELOAD_INTERVAL = 30;

/** 最大输出器数量 */
static const int MAX_OUTPUTTERS = 16;

/** 最大过滤器数量 */
static const int MAX_FILTERS = 32;

/* ==================== 内部数据结构 ==================== */

/** 输出器基�?*/
typedef struct outputter {
    /** 输出器类�?*/
    int type;
    
    /** 输出器名�?*/
    char name[64];
    
    /** 输出函数 */
    int (*output)(struct outputter* self, const log_record_t* record);
    
    /** 销毁函�?*/
    void (*destroy)(struct outputter* self);
    
    /** 用户数据 */
    void* user_data;
} outputter_t;

/** 过滤器基�?*/
typedef struct filter {
    /** 过滤器类�?*/
    int type;
    
    /** 过滤器名�?*/
    char name[64];
    
    /** 过滤函数 */
    bool (*filter)(struct filter* self, const log_record_t* record);
    
    /** 销毁函�?*/
    void (*destroy)(struct filter* self);
    
    /** 用户数据 */
    void* user_data;
} filter_t;

/** 服务层全局状�?*/
typedef struct {
    /** 当前配置 */
    service_logging_config_t manager;
    
    /** 是否已初始化 */
    bool initialized;
    
    /** 输出器数�?*/
    outputter_t* outputters[MAX_OUTPUTTERS];
    
    /** 输出器数�?*/
    int outputter_count;
    
    /** 过滤器数�?*/
    filter_t* filters[MAX_FILTERS];
    
    /** 过滤器数�?*/
    int filter_count;
    
    /** 工作线程 */
    pthread_t* worker_threads;
    
    /** 工作线程运行标志 */
    bool* worker_running;
    
    /** 互斥锁保护状�?*/
    pthread_mutex_t mutex;
    
    /** 统计信息 */
    service_logging_stats_t stats;
    
    /** 轮转配置 */
    log_rotation_config_t rotation_config;
    
    /** 传输配置 */
    log_transport_config_t transport_config;
} service_logging_state_t;

/* ==================== 全局状态变�?==================== */

/** 服务层全局状态实�?*/
static service_logging_state_t g_service_state = {
    .initialized = false,
    .outputter_count = 0,
    .filter_count = 0
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 控制台输出器实现
 * 
 * 将日志记录输出到控制台�? * 
 * @param self 输出器实�? * @param record 日志记录
 * @return 0成功，负值表示错�? */
static int console_outputter_output(outputter_t* self, const log_record_t* record) {
    (void)self;
    
    if (!record) {
        return -1;
    }
    
    // 简单控制台输出
    FILE* stream = (record->level >= LOG_LEVEL_ERROR) ? stderr : stdout;
    fprintf(stream, "[SERVICE] %s:%d %s\n", record->module, record->line, record->message);
    
    return 0;
}

/**
 * @brief 控制台输出器销�? * 
 * 销毁控制台输出器�? * 
 * @param self 输出器实�? */
static void console_outputter_destroy(outputter_t* self) {
    AGENTOS_FREE(self);
}

/**
 * @brief 文件输出器实�? * 
 * 将日志记录输出到文件�? * 
 * @param self 输出器实�? * @param record 日志记录
 * @return 0成功，负值表示错�? */
static int file_outputter_output(outputter_t* self, const log_record_t* record) {
    (void)self;
    (void)record;
    
    // 简化实现：暂时不支持文件输�?    return -1;
}

/**
 * @brief 文件输出器销�? * 
 * 销毁文件输出器�? * 
 * @param self 输出器实�? */
static void file_outputter_destroy(outputter_t* self) {
    AGENTOS_FREE(self);
}

/**
 * @brief 级别过滤器实�? * 
 * 根据日志级别过滤日志记录�? * 
 * @param self 过滤器实�? * @param record 日志记录
 * @return true通过过滤，false被过�? */
static bool level_filter_filter(filter_t* self, const log_record_t* record) {
    if (!self || !record) {
        return false;
    }
    
    // 简化实现：允许所有级�?    return true;
}

/**
 * @brief 级别过滤器销�? * 
 * 销毁级别过滤器�? * 
 * @param self 过滤器实�? */
static void level_filter_destroy(filter_t* self) {
    AGENTOS_FREE(self);
}

/**
 * @brief 工作线程函数
 * 
 * 服务层工作线程，处理日志记录�? * 
 * @param arg 线程参数（线程索引）
 * @return NULL
 */
static void* worker_thread_func(void* arg) {
    int thread_idx = *(int*)arg;
    
    while (g_service_state.worker_running[thread_idx]) {
        // 简化实现：休眠等待
        sleep(1);
    }
    
    return NULL;
}

/**
 * @brief 应用过滤�? * 
 * 对所有过滤器应用过滤规则�? * 
 * @param record 日志记录
 * @return true通过所有过滤器，false被某个过滤器拒绝
 */
static bool apply_filters(const log_record_t* record) {
    if (!record) {
        return false;
    }
    
    for (int i = 0; i < g_service_state.filter_count; i++) {
        filter_t* filter = g_service_state.filters[i];
        if (filter && filter->filter) {
            if (!filter->filter(filter, record)) {
                return false;
            }
        }
    }
    
    return true;
}

/**
 * @brief 应用输出�? * 
 * 对所有输出器应用输出�? * 
 * @param record 日志记录
 * @return 成功输出的数�? */
static int apply_outputters(const log_record_t* record) {
    if (!record) {
        return 0;
    }
    
    int success_count = 0;
    
    for (int i = 0; i < g_service_state.outputter_count; i++) {
        outputter_t* outputter = g_service_state.outputters[i];
        if (outputter && outputter->output) {
            if (outputter->output(outputter, record) == 0) {
                success_count++;
            }
        }
    }
    
    return success_count;
}

/* ==================== 公开API实现 ==================== */

int service_logging_init(const service_logging_config_t* manager) {
    if (g_service_state.initialized) {
        return -1;
    }
    
    // 初始化互斥锁
    if (pthread_mutex_init(&g_service_state.mutex, NULL) != 0) {
        return -2;
    }
    
    // 设置配置
    if (manager) {
        memcpy(&g_service_state.manager, manager, sizeof(service_logging_config_t));
    } else {
        // 使用默认配置
        g_service_state.manager.enable_rotation = false;
        g_service_state.manager.enable_filtering = false;
        g_service_state.manager.enable_transport = false;
        g_service_state.manager.enable_monitoring = false;
        g_service_state.manager.enable_management = false;
        g_service_state.manager.worker_threads = DEFAULT_WORKER_THREADS;
        g_service_state.manager.max_outputters = MAX_OUTPUTTERS;
        g_service_state.manager.max_filters = MAX_FILTERS;
        g_service_state.manager.config_reload_interval = DEFAULT_CONFIG_RELOAD_INTERVAL;
    }
    
    // 初始化统计信�?    memset(&g_service_state.stats, 0, sizeof(service_logging_stats_t));
    
    // 创建默认输出器（控制台）
    outputter_t* console_outputter = (outputter_t*)AGENTOS_CALLOC(1, sizeof(outputter_t));
    if (console_outputter) {
        console_outputter->type = 1;
        snprintf(console_outputter->name, sizeof(console_outputter->name), "%s", "console");
        console_outputter->output = console_outputter_output;
        console_outputter->destroy = console_outputter_destroy;
        
        g_service_state.outputters[g_service_state.outputter_count++] = console_outputter;
    }
    
    // 创建工作线程
    int worker_count = g_service_state.manager.worker_threads;
    if (worker_count > 0) {
        g_service_state.worker_threads = (pthread_t*)AGENTOS_CALLOC(worker_count, sizeof(pthread_t));
        g_service_state.worker_running = (bool*)AGENTOS_CALLOC(worker_count, sizeof(bool));
        
        if (g_service_state.worker_threads && g_service_state.worker_running) {
            for (int i = 0; i < worker_count; i++) {
                g_service_state.worker_running[i] = true;
                
                int* thread_idx = (int*)AGENTOS_MALLOC(sizeof(int));
                *thread_idx = i;
                
                if (pthread_create(&g_service_state.worker_threads[i], NULL, 
                                  worker_thread_func, thread_idx) != 0) {
                    g_service_state.worker_running[i] = false;
                    AGENTOS_FREE(thread_idx);
                }
            }
        }
    }
    
    g_service_state.initialized = true;
    
    return 0;
}

int service_logging_configure_rotation(const log_rotation_config_t* manager) {
    if (!g_service_state.initialized || !manager) {
        return -1;
    }
    
    pthread_mutex_lock(&g_service_state.mutex);
    
    // 保存轮转配置
    memcpy(&g_service_state.rotation_config, manager, sizeof(log_rotation_config_t));
    
    pthread_mutex_unlock(&g_service_state.mutex);
    
    return 0;
}

int service_logging_configure_transport(const log_transport_config_t* manager) {
    if (!g_service_state.initialized || !manager) {
        return -1;
    }
    
    pthread_mutex_lock(&g_service_state.mutex);
    
    // 保存传输配置
    memcpy(&g_service_state.transport_config, manager, sizeof(log_transport_config_t));
    
    pthread_mutex_unlock(&g_service_state.mutex);
    
    return 0;
}

int service_logging_add_outputter(const char* name, int type, void* user_data) {
    if (!g_service_state.initialized || !name || 
        g_service_state.outputter_count >= MAX_OUTPUTTERS) {
        return -1;
    }
    
    pthread_mutex_lock(&g_service_state.mutex);
    
    // 创建输出�?    outputter_t* outputter = (outputter_t*)AGENTOS_CALLOC(1, sizeof(outputter_t));
    if (!outputter) {
        pthread_mutex_unlock(&g_service_state.mutex);
        return -2;
    }
    
    outputter->type = type;
    strncpy(outputter->name, name, sizeof(outputter->name) - 1);
    outputter->user_data = user_data;
    
    // 根据类型设置函数
    switch (type) {
        case 1: // 控制台输出器
            outputter->output = console_outputter_output;
            outputter->destroy = console_outputter_destroy;
            break;
            
        case 2: // 文件输出�?            outputter->output = file_outputter_output;
            outputter->destroy = file_outputter_destroy;
            break;
            
        default:
            AGENTOS_FREE(outputter);
            pthread_mutex_unlock(&g_service_state.mutex);
            return -3;
    }
    
    // 添加到数�?    g_service_state.outputters[g_service_state.outputter_count++] = outputter;
    
    pthread_mutex_unlock(&g_service_state.mutex);
    
    return 0;
}

int service_logging_add_filter(const char* name, int type, void* user_data) {
    if (!g_service_state.initialized || !name || 
        g_service_state.filter_count >= MAX_FILTERS) {
        return -1;
    }
    
    pthread_mutex_lock(&g_service_state.mutex);
    
    // 创建过滤�?    filter_t* filter = (filter_t*)AGENTOS_CALLOC(1, sizeof(filter_t));
    if (!filter) {
        pthread_mutex_unlock(&g_service_state.mutex);
        return -2;
    }
    
    filter->type = type;
    strncpy(filter->name, name, sizeof(filter->name) - 1);
    filter->user_data = user_data;
    
    // 根据类型设置函数
    switch (type) {
        case 1: // 级别过滤�?            filter->filter = level_filter_filter;
            filter->destroy = level_filter_destroy;
            break;
            
        default:
            AGENTOS_FREE(filter);
            pthread_mutex_unlock(&g_service_state.mutex);
            return -3;
    }
    
    // 添加到数�?    g_service_state.filters[g_service_state.filter_count++] = filter;
    
    pthread_mutex_unlock(&g_service_state.mutex);
    
    return 0;
}

int service_logging_process_record(const log_record_t* record) {
    if (!g_service_state.initialized || !record) {
        return -1;
    }
    
    // 应用过滤�?    if (!apply_filters(record)) {
        return 0; // 记录被过滤，返回成功但不输出
    }
    
    // 应用输出�?    int success_count = apply_outputters(record);
    
    // 更新统计信息
    pthread_mutex_lock(&g_service_state.mutex);
    g_service_state.stats.records_processed++;
    g_service_state.stats.records_output += success_count;
    pthread_mutex_unlock(&g_service_state.mutex);
    
    return success_count > 0 ? 0 : -2;
}

int service_logging_get_stats(service_logging_stats_t* stats) {
    if (!g_service_state.initialized || !stats) {
        return -1;
    }
    
    pthread_mutex_lock(&g_service_state.mutex);
    memcpy(stats, &g_service_state.stats, sizeof(service_logging_stats_t));
    pthread_mutex_unlock(&g_service_state.mutex);
    
    return 0;
}

int service_logging_reload_config(const char* config_path) {
    (void)config_path;
    
    // 简化实现：暂时不支持配置重�?    return 0;
}

void service_logging_cleanup(void) {
    if (!g_service_state.initialized) {
        return;
    }
    
    pthread_mutex_lock(&g_service_state.mutex);
    
    // 停止工作线程
    if (g_service_state.worker_running) {
        int worker_count = g_service_state.manager.worker_threads;
        for (int i = 0; i < worker_count; i++) {
            g_service_state.worker_running[i] = false;
        }
        
        for (int i = 0; i < worker_count; i++) {
            pthread_join(g_service_state.worker_threads[i], NULL);
        }
        
        AGENTOS_FREE(g_service_state.worker_threads);
        AGENTOS_FREE(g_service_state.worker_running);
    }
    
    // 销毁输出器
    for (int i = 0; i < g_service_state.outputter_count; i++) {
        outputter_t* outputter = g_service_state.outputters[i];
        if (outputter && outputter->destroy) {
            outputter->destroy(outputter);
        }
    }
    
    // 销毁过滤器
    for (int i = 0; i < g_service_state.filter_count; i++) {
        filter_t* filter = g_service_state.filters[i];
        if (filter && filter->destroy) {
            filter->destroy(filter);
        }
    }
    
    pthread_mutex_unlock(&g_service_state.mutex);
    pthread_mutex_destroy(&g_service_state.mutex);
    
    // 重置状�?    memset(&g_service_state, 0, sizeof(g_service_state));
}
