/**
 * @file atomic_logging.c
 * @brief 统一分层日志系统原子层实�? * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一分层日志系统的原子层功能，提供：
 * 1. 线程安全的日志记录缓冲和队列管理
 * 2. 高性能日志写入机制
 * 3. 批量提交和异步刷�? *
 * 注意：这是一个简化实现，使用互斥锁保证线程安全�? * 生产环境应使用无锁队列实现以获得最佳性能�? */

#include "atomic_logging.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/utils/memory/memory_compat.h>
#include <agentos/utils/string/string_compat.h>
#include <string.h>
#include <pthread.h>

/* ==================== 内部常量定义 ==================== */

/** 默认线程本地缓冲大小 */
static const size_t DEFAULT_THREAD_LOCAL_BUFFER_SIZE = 8192;

/** 默认环形队列容量 */
static const size_t DEFAULT_RING_BUFFER_CAPACITY = 1024;

/** 默认批量提交阈�?*/
static const size_t DEFAULT_BATCH_COMMIT_THRESHOLD = 16;

/** 节点状态枚�?*/
typedef enum {
    NODE_STATE_FREE = 0,      // 空闲可用
    NODE_STATE_WRITTEN = 1,   // 已写入待处理
    NODE_STATE_PROCESSED = 2  // 已处理可回收
} node_state_t;

/* ==================== 内部数据结构 ==================== */

/** 线程本地缓冲 */
typedef struct {
    /** 缓冲数据 */
    char* buffer;
    
    /** 当前写入位置 */
    size_t write_pos;
    
    /** 缓冲容量 */
    size_t capacity;
} thread_local_buffer_t;

/** 环形缓冲队列 */
typedef struct {
    /** 节点数组 */
    AtomicLogRecordNode* nodes;
    
    /** 队列容量 */
    size_t capacity;
    
    /** 生产者位�?*/
    size_t producer_pos;
    
    /** 消费者位�?*/
    size_t consumer_pos;
    
    /** 互斥锁保护队列操�?*/
    pthread_mutex_t mutex;
} ring_buffer_t;

/** 原子层全局状�?*/
typedef struct {
    /** 当前配置 */
    atomic_logging_config_t manager;
    
    /** 是否已初始化 */
    bool initialized;
    
    /** 环形队列 */
    ring_buffer_t ring_buffer;
    
    /** 线程本地缓冲�?*/
    pthread_key_t tls_buffer_key;
    
    /** 异步刷新线程 */
    pthread_t flush_thread;
    
    /** 刷新线程运行标志 */
    bool flush_thread_running;
} atomic_logging_state_t;

/* ==================== 全局状态变�?==================== */

/** 原子层全局状态实�?*/
static atomic_logging_state_t g_atomic_state = {
    .initialized = false,
    .flush_thread_running = false
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 获取线程本地缓冲
 * 
 * 获取当前线程的本地缓冲，如果不存在则创建�? * 
 * @return 线程本地缓冲，失败返回NULL
 */
static thread_local_buffer_t* get_thread_local_buffer(void) {
    thread_local_buffer_t* buffer = pthread_getspecific(g_atomic_state.tls_buffer_key);
    
    if (!buffer) {
        buffer = (thread_local_buffer_t*)AGENTOS_CALLOC(1, sizeof(thread_local_buffer_t));
        if (buffer) {
            buffer->capacity = g_atomic_state.manager.thread_local_buffer_size;
            buffer->buffer = (char*)AGENTOS_MALLOC(buffer->capacity);
            buffer->write_pos = 0;
            
            if (!buffer->buffer) {
                AGENTOS_FREE(buffer);
                return NULL;
            }
            
            pthread_setspecific(g_atomic_state.tls_buffer_key, buffer);
        }
    }
    
    return buffer;
}

/**
 * @brief 释放线程本地缓冲
 * 
 * 释放线程本地缓冲资源�? * 
 * @param buffer 线程本地缓冲
 */
static void free_thread_local_buffer(void* buffer) {
    if (buffer) {
        thread_local_buffer_t* tls = (thread_local_buffer_t*)buffer;
        AGENTOS_FREE(tls->buffer);
        AGENTOS_FREE(tls);
    }
}

/**
 * @brief 向环形队列提交日志记�? * 
 * 将日志记录提交到环形队列，等待后续处理�? * 
 * @param record 日志记录
 * @return 0成功，负值表示错�? */
static int submit_to_ring_buffer(const log_record_t* record) {
    if (!record || !g_atomic_state.initialized) {
        return -1;
    }
    
    pthread_mutex_lock(&g_atomic_state.ring_buffer.mutex);
    
    // 检查队列是否已�?    size_t next_producer_pos = (g_atomic_state.ring_buffer.producer_pos + 1) % g_atomic_state.ring_buffer.capacity;
    if (next_producer_pos == g_atomic_state.ring_buffer.consumer_pos) {
        pthread_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
        return -2; // 队列已满
    }
    
    // 获取当前节点
    AtomicLogRecordNode* node = &g_atomic_state.ring_buffer.nodes[g_atomic_state.ring_buffer.producer_pos];
    
    // 复制记录数据
    memcpy(&node->record, record, sizeof(log_record_t));
    
    // 更新节点状态（简化实现，不使用原子操作）
    // 在实际无锁实现中，这里应该使用原子操�?    node->state = NODE_STATE_WRITTEN;
    
    // 更新生产者位�?    g_atomic_state.ring_buffer.producer_pos = next_producer_pos;
    
    pthread_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
    
    return 0;
}

/**
 * @brief 从环形队列消费日志记�? * 
 * 从环形队列消费一条日志记录�? * 
 * @param record 输出日志记录
 * @return true成功消费，false队列为空
 */
static bool consume_from_ring_buffer(log_record_t* record) {
    if (!record || !g_atomic_state.initialized) {
        return false;
    }
    
    pthread_mutex_lock(&g_atomic_state.ring_buffer.mutex);
    
    // 检查队列是否为�?    if (g_atomic_state.ring_buffer.consumer_pos == g_atomic_state.ring_buffer.producer_pos) {
        pthread_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
        return false;
    }
    
    // 获取当前节点
    AtomicLogRecordNode* node = &g_atomic_state.ring_buffer.nodes[g_atomic_state.ring_buffer.consumer_pos];
    
    // 检查节点状�?    if (node->state != NODE_STATE_WRITTEN) {
        pthread_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
        return false;
    }
    
    // 复制记录数据
    memcpy(record, &node->record, sizeof(log_record_t));
    
    // 更新节点状�?    node->state = NODE_STATE_PROCESSED;
    
    // 更新消费者位�?    g_atomic_state.ring_buffer.consumer_pos = (g_atomic_state.ring_buffer.consumer_pos + 1) % g_atomic_state.ring_buffer.capacity;
    
    pthread_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
    
    return true;
}

/**
 * @brief 异步刷新线程函数
 * 
 * 异步刷新线程，负责从环形队列消费日志记录并输出�? * 
 * @param arg 线程参数（未使用�? * @return NULL
 */
static void* flush_thread_func(void* arg) {
    (void)arg;
    
    while (g_atomic_state.flush_thread_running) {
        // 批量消费日志记录
        log_record_t records[16];
        size_t count = 0;
        
        pthread_mutex_lock(&g_atomic_state.ring_buffer.mutex);
        
        // 消费一批记�?        while (count < sizeof(records) / sizeof(records[0])) {
            if (g_atomic_state.ring_buffer.consumer_pos == g_atomic_state.ring_buffer.producer_pos) {
                break;
            }
            
            AtomicLogRecordNode* node = &g_atomic_state.ring_buffer.nodes[g_atomic_state.ring_buffer.consumer_pos];
            
            if (node->state == NODE_STATE_WRITTEN) {
                memcpy(&records[count], &node->record, sizeof(log_record_t));
                node->state = NODE_STATE_PROCESSED;
                g_atomic_state.ring_buffer.consumer_pos = (g_atomic_state.ring_buffer.consumer_pos + 1) % g_atomic_state.ring_buffer.capacity;
                count++;
            } else {
                break;
            }
        }
        
        pthread_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
        
        // 处理消费的记录（简化实现：输出到控制台�?        for (size_t i = 0; i < count; i++) {
            // 这里应该调用服务层输出，简化实现直接输�?            fprintf(stderr, "[ASYNC] %s:%d %s\n", 
                   records[i].module, records[i].line, records[i].message);
        }
        
        // 休眠
        usleep(g_atomic_state.manager.flush_thread_sleep_ms * 1000);
    }
    
    return NULL;
}

/* ==================== 公开API实现 ==================== */

int atomic_logging_init(const atomic_logging_config_t* manager) {
    if (g_atomic_state.initialized) {
        return -1;
    }
    
    // 设置配置
    if (manager) {
        memcpy(&g_atomic_state.manager, manager, sizeof(atomic_logging_config_t));
    } else {
        // 使用默认配置
        g_atomic_state.manager.lock_free_mode = false;
        g_atomic_state.manager.thread_local_buffer_size = DEFAULT_THREAD_LOCAL_BUFFER_SIZE;
        g_atomic_state.manager.ring_buffer_capacity = DEFAULT_RING_BUFFER_CAPACITY;
        g_atomic_state.manager.batch_commit_threshold = DEFAULT_BATCH_COMMIT_THRESHOLD;
        g_atomic_state.manager.max_batch_size = 32;
        g_atomic_state.manager.flush_thread_sleep_ms = 100;
        g_atomic_state.manager.enable_memory_pool = false;
        g_atomic_state.manager.memory_pool_initial_size = 65536;
        g_atomic_state.manager.memory_pool_block_size = 4096;
    }
    
    // 初始化环形队�?    g_atomic_state.ring_buffer.capacity = g_atomic_state.manager.ring_buffer_capacity;
    g_atomic_state.ring_buffer.nodes = (AtomicLogRecordNode*)AGENTOS_CALLOC(
        g_atomic_state.ring_buffer.capacity, sizeof(AtomicLogRecordNode));
    
    if (!g_atomic_state.ring_buffer.nodes) {
        return -2;
    }
    
    g_atomic_state.ring_buffer.producer_pos = 0;
    g_atomic_state.ring_buffer.consumer_pos = 0;
    
    if (pthread_mutex_init(&g_atomic_state.ring_buffer.mutex, NULL) != 0) {
        AGENTOS_FREE(g_atomic_state.ring_buffer.nodes);
        return -3;
    }
    
    // 初始化线程本地存�?    if (pthread_key_create(&g_atomic_state.tls_buffer_key, free_thread_local_buffer) != 0) {
        pthread_mutex_destroy(&g_atomic_state.ring_buffer.mutex);
        AGENTOS_FREE(g_atomic_state.ring_buffer.nodes);
        return -4;
    }
    
    // 启动异步刷新线程
    g_atomic_state.flush_thread_running = true;
    if (pthread_create(&g_atomic_state.flush_thread, NULL, flush_thread_func, NULL) != 0) {
        pthread_key_delete(g_atomic_state.tls_buffer_key);
        pthread_mutex_destroy(&g_atomic_state.ring_buffer.mutex);
        AGENTOS_FREE(g_atomic_state.ring_buffer.nodes);
        return -5;
    }
    
    g_atomic_state.initialized = true;
    
    return 0;
}

int atomic_logging_submit(const log_record_t* record, bool non_blocking) {
    if (!g_atomic_state.initialized || !record) {
        return -1;
    }
    
    // 获取线程本地缓冲
    thread_local_buffer_t* buffer = get_thread_local_buffer();
    if (!buffer) {
        return -2;
    }
    
    // 简化实现：直接提交到环形队�?    int result = submit_to_ring_buffer(record);
    
    if (result != 0 && !non_blocking) {
        // 阻塞模式：等待队列有空闲空间
        // 简化实现：直接返回错误
        return result;
    }
    
    return result;
}

int atomic_logging_submit_lockfree(const log_record_t* record, bool non_blocking) {
    // 简化实现：调用互斥锁版�?    return atomic_logging_submit(record, non_blocking);
}

int atomic_logging_flush(void) {
    if (!g_atomic_state.initialized) {
        return -1;
    }
    
    // 刷新线程本地缓冲
    thread_local_buffer_t* buffer = get_thread_local_buffer();
    if (buffer && buffer->write_pos > 0) {
        // 将缓冲中的记录提交到环形队列
        // 简化实现：直接清空缓冲
        buffer->write_pos = 0;
    }
    
    // 等待环形队列清空
    int max_wait = 100; // 最多等�?00ms
    while (max_wait-- > 0) {
        pthread_mutex_lock(&g_atomic_state.ring_buffer.mutex);
        bool empty = (g_atomic_state.ring_buffer.consumer_pos == g_atomic_state.ring_buffer.producer_pos);
        pthread_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
        
        if (empty) {
            break;
        }
        
        usleep(1000); // 休眠1ms
    }
    
    return 0;
}

int atomic_logging_get_stats(atomic_logging_stats_t* stats) {
    if (!g_atomic_state.initialized || !stats) {
        return -1;
    }
    
    // 简化实现：返回基本统计信息
    memset(stats, 0, sizeof(atomic_logging_stats_t));
    
    pthread_mutex_lock(&g_atomic_state.ring_buffer.mutex);
    
    // 计算队列大小
    if (g_atomic_state.ring_buffer.producer_pos >= g_atomic_state.ring_buffer.consumer_pos) {
        stats->queue_size = g_atomic_state.ring_buffer.producer_pos - g_atomic_state.ring_buffer.consumer_pos;
    } else {
        stats->queue_size = g_atomic_state.ring_buffer.capacity - g_atomic_state.ring_buffer.consumer_pos + g_atomic_state.ring_buffer.producer_pos;
    }
    
    pthread_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
    
    return 0;
}

void atomic_logging_cleanup(void) {
    if (!g_atomic_state.initialized) {
        return;
    }
    
    // 停止异步刷新线程
    g_atomic_state.flush_thread_running = false;
    pthread_join(g_atomic_state.flush_thread, NULL);
    
    // 清理线程本地存储
    pthread_key_delete(g_atomic_state.tls_buffer_key);
    
    // 销毁环形队�?    pthread_mutex_destroy(&g_atomic_state.ring_buffer.mutex);
    AGENTOS_FREE(g_atomic_state.ring_buffer.nodes);
    
    // 重置状�?    memset(&g_atomic_state, 0, sizeof(g_atomic_state));
}
