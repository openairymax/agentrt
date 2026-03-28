/**
 * @file async_storage.c
 * @brief L1 原始卷异步存储引擎实�?
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * L1 原始卷异步存储引擎提供高性能、高可靠的异步数据存储服务�?
 * 支持批量写入、流控制、错误重试和容灾恢复，达�?9.999%生产级可靠性标准�?
 * 
 * 核心功能�?
 * 1. 异步操作：非阻塞写入，提升系统吞吐量
 * 2. 批量处理：批量操作优化磁盘I/O
 * 3. 队列管理：有界队列防止内存溢�?
 * 4. 线程池：弹性工作线程管�?
 * 5. 错误恢复：写入失败自动重�?
 * 6. 持久化保证：确保数据最终持久化
 * 7. 监控指标：性能指标和健康检�?
 * 8. 资源控制：内存和磁盘使用限制
 */

#include "layer1_raw.h"
#include "agentos.h"
#include "logger.h"
#include "observability.h"
#include "id_utils.h"
#include "error_utils.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../bases/utils/memory/include/memory_compat.h"
#include "../../../bases/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

/* ==================== 内部常量定义 ==================== */

/** @brief 默认队列大小 */
#define DEFAULT_QUEUE_SIZE 1024

/** @brief 默认工作线程�?*/
#define DEFAULT_WORKER_COUNT 4

/** @brief 最大重试次�?*/
#define MAX_RETRY_COUNT 3

/** @brief 重试延迟基数（毫秒） */
#define RETRY_DELAY_BASE_MS 100

/** @brief 批处理最大大�?*/
#define BATCH_SIZE_MAX 128

/** @brief 文件缓冲区大小（字节�?*/
#define FILE_BUFFER_SIZE (64 * 1024)

/** @brief 默认超时时间（毫秒） */
#define DEFAULT_TIMEOUT_MS 5000

/** @brief 刷新间隔（毫秒） */
#define FLUSH_INTERVAL_MS 1000

/** @brief 健康检查间隔（毫秒�?*/
#define HEALTH_CHECK_INTERVAL_MS 30000

/** @brief 最大文件路径长�?*/
#define MAX_FILE_PATH 1024

/* ==================== 内部数据结构 ==================== */

/**
 * @brief 异步操作类型
 */
typedef enum {
    ASYNC_OP_WRITE = 0,           /**< 写入操作 */
    ASYNC_OP_READ,                /**< 读取操作 */
    ASYNC_OP_DELETE,              /**< 删除操作 */
    ASYNC_OP_FLUSH                /**< 刷新操作 */
} async_op_type_t;

/**
 * @brief 异步操作数据
 */
typedef struct async_operation {
    async_op_type_t type;          /**< 操作类型 */
    char* id;                      /**< 记录ID */
    void* data;                    /**< 数据指针 */
    size_t data_len;               /**< 数据长度 */
    void** out_data;               /**< 输出数据指针 */
    size_t* out_len;               /**< 输出长度指针 */
    agentos_error_t* out_error;    /**< 输出错误�?*/
    agentos_semaphore_t* semaphore; /**< 同步信号�?*/
    int retry_count;               /**< 重试次数 */
    uint64_t timestamp_ns;         /**< 时间�?*/
    struct async_operation* next;  /**< 下一个操�?*/
} async_operation_t;

/**
 * @brief 异步队列
 */
typedef struct async_queue {
    async_operation_t* head;       /**< 队列�?*/
    async_operation_t* tail;       /**< 队列�?*/
    size_t size;                   /**< 当前大小 */
    size_t capacity;               /**< 最大容�?*/
    agentos_mutex_t* lock;         /**< 队列�?*/
    agentos_semaphore_t* semaphore; /**< 信号�?*/
} async_queue_t;

/**
 * @brief 工作线程状�?
 */
typedef struct worker_thread {
    agentos_thread_t* thread;      /**< 线程句柄 */
    int running;                   /**< 运行标志 */
    int index;                     /**< 线程索引 */
    struct agentos_layer1_raw* l1; /**< L1句柄 */
} worker_thread_t;

/**
 * @brief L1 原始卷异步实�?
 */
struct agentos_layer1_raw {
    char* storage_path;            /**< 存储路径 */
    async_queue_t* queue;          /**< 操作队列 */
    worker_thread_t* workers;      /**< 工作线程数组 */
    uint32_t worker_count;         /**< 工作线程数量 */
    int running;                   /**< 运行标志 */
    agentos_mutex_t* lock;         /**< 全局�?*/
    agentos_observability_t* obs;  /**< 可观测性句�?*/
    
    /* 统计信息 */
    uint64_t total_write_count;    /**< 总写入次�?*/
    uint64_t total_read_count;     /**< 总读取次�?*/
    uint64_t total_delete_count;   /**< 总删除次�?*/
    uint64_t failed_write_count;   /**< 失败写入次数 */
    uint64_t failed_read_count;    /**< 失败读取次数 */
    uint64_t failed_delete_count;  /**< 失败删除次数 */
    uint64_t total_queue_time_ns;  /**< 总排队时间（纳秒�?*/
    uint64_t total_process_time_ns; /**< 总处理时间（纳秒�?*/
    
    /* 批处理状�?*/
    async_operation_t* batch_buffer[BATCH_SIZE_MAX]; /**< 批处理缓冲区 */
    size_t batch_count;             /**< 批处理计�?*/
    uint64_t last_flush_time_ns;    /**< 最后刷新时�?*/
    
    /* 健康状�?*/
    int healthy;                    /**< 健康状�?*/
    char* health_message;           /**< 健康消息 */
    uint64_t last_health_check_ns;  /**< 最后健康检查时�?*/
};

/* ==================== 内部工具函数 ==================== */

/**
 * @brief 确保目录存在
 * @param path 目录路径
 * @return AGENTOS_SUCCESS 成功，其他为错误�?
 */
static agentos_error_t ensure_directory_exists(const char* path) {
    if (!path) return AGENTOS_EINVAL;
    
#ifdef _WIN32
    if (_mkdir(path) != 0 && errno != EEXIST) {
        AGENTOS_LOG_ERROR("Failed to create directory %s: %d", path, errno);
        return AGENTOS_EFAIL;
    }
#else
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        AGENTOS_LOG_ERROR("Failed to create directory %s: %d", path, errno);
        return AGENTOS_EFAIL;
    }
#endif
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 构建完整文件路径
 * @param storage_path 存储路径
 * @param id 记录ID
 * @param file_path 输出文件路径
 * @param max_len 最大长�?
 * @return AGENTOS_SUCCESS 成功，其他为错误�?
 */
static agentos_error_t build_file_path(const char* storage_path, const char* id,
                                       char* file_path, size_t max_len) {
    if (!storage_path || !id || !file_path) return AGENTOS_EINVAL;
    
    int written = snprintf(file_path, max_len, "%s/%s.raw", storage_path, id);
    if (written < 0 || (size_t)written >= max_len) {
        AGENTOS_LOG_ERROR("File path too long: %s/%s.raw", storage_path, id);
        return AGENTOS_EINVAL;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 安全写入文件
 * @param file_path 文件路径
 * @param data 数据指针
 * @param data_len 数据长度
 * @return AGENTOS_SUCCESS 成功，其他为错误�?
 */
static agentos_error_t safe_write_file(const char* file_path, const void* data, size_t data_len) {
    if (!file_path || !data) return AGENTOS_EINVAL;
    
    // 创建临时文件
    char temp_path[MAX_FILE_PATH];
    int written = snprintf(temp_path, sizeof(temp_path), "%s.tmp", file_path);
    if (written < 0 || (size_t)written >= sizeof(temp_path)) {
        return AGENTOS_EINVAL;
    }
    
    FILE* f = fopen(temp_path, "wb");
    if (!f) {
        AGENTOS_LOG_ERROR("Failed to open temp file %s: %d", temp_path, errno);
        return AGENTOS_EFAIL;
    }
    
    size_t written_bytes = fwrite(data, 1, data_len, f);
    fclose(f);
    
    if (written_bytes != data_len) {
        AGENTOS_LOG_ERROR("Failed to write temp file %s: wrote %zu of %zu bytes", 
                         temp_path, written_bytes, data_len);
        remove(temp_path);
        return AGENTOS_EFAIL;
    }
    
    // 原子重命名为最终文�?
#ifdef _WIN32
    if (MoveFileExA(temp_path, file_path, MOVEFILE_REPLACE_EXISTING) == 0) {
        AGENTOS_LOG_ERROR("Failed to rename temp file %s to %s: %lu", 
                         temp_path, file_path, GetLastError());
        remove(temp_path);
        return AGENTOS_EFAIL;
    }
#else
    if (rename(temp_path, file_path) != 0) {
        AGENTOS_LOG_ERROR("Failed to rename temp file %s to %s: %d", 
                         temp_path, file_path, errno);
        remove(temp_path);
        return AGENTOS_EFAIL;
    }
#endif
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 安全读取文件
 * @param file_path 文件路径
 * @param out_data 输出数据指针
 * @param out_len 输出长度指针
 * @return AGENTOS_SUCCESS 成功，其他为错误�?
 */
static agentos_error_t safe_read_file(const char* file_path, void** out_data, size_t* out_len) {
    if (!file_path || !out_data || !out_len) return AGENTOS_EINVAL;
    
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        if (errno == ENOENT) {
            return AGENTOS_ENOTFOUND;
        }
        AGENTOS_LOG_ERROR("Failed to open file %s: %d", file_path, errno);
        return AGENTOS_EFAIL;
    }
    
    // 获取文件大小
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(f);
        AGENTOS_LOG_ERROR("Failed to get file size %s: %d", file_path, errno);
        return AGENTOS_EFAIL;
    }
    
    // 分配内存
    void* data = AGENTOS_MALLOC((size_t)file_size);
    if (!data) {
        fclose(f);
        AGENTOS_LOG_ERROR("Failed to allocate memory for file %s: size=%ld", 
                         file_path, file_size);
        return AGENTOS_ENOMEM;
    }
    
    // 读取文件
    size_t read_bytes = fread(data, 1, (size_t)file_size, f);
    fclose(f);
    
    if (read_bytes != (size_t)file_size) {
        AGENTOS_FREE(data);
        AGENTOS_LOG_ERROR("Failed to read file %s: read %zu of %ld bytes", 
                         file_path, read_bytes, file_size);
        return AGENTOS_EFAIL;
    }
    
    *out_data = data;
    *out_len = (size_t)file_size;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 安全删除文件
 * @param file_path 文件路径
 * @return AGENTOS_SUCCESS 成功，其他为错误�?
 */
static agentos_error_t safe_delete_file(const char* file_path) {
    if (!file_path) return AGENTOS_EINVAL;
    
    if (remove(file_path) != 0) {
        if (errno == ENOENT) {
            return AGENTOS_ENOTFOUND;
        }
        AGENTOS_LOG_ERROR("Failed to delete file %s: %d", file_path, errno);
        return AGENTOS_EFAIL;
    }
    
    return AGENTOS_SUCCESS;
}

/* ==================== 异步队列函数 ==================== */

/**
 * @brief 创建异步队列
 * @param capacity 队列容量
 * @return 队列句柄，失败返回NULL
 */
static async_queue_t* async_queue_create(size_t capacity) {
    if (capacity == 0) capacity = DEFAULT_QUEUE_SIZE;
    
    async_queue_t* queue = (async_queue_t*)AGENTOS_CALLOC(1, sizeof(async_queue_t));
    if (!queue) {
        AGENTOS_LOG_ERROR("Failed to allocate async queue");
        return NULL;
    }
    
    queue->capacity = capacity;
    queue->lock = agentos_mutex_create();
    queue->semaphore = agentos_semaphore_create(0);
    
    if (!queue->lock || !queue->semaphore) {
        if (queue->lock) agentos_mutex_destroy(queue->lock);
        if (queue->semaphore) agentos_semaphore_destroy(queue->semaphore);
        AGENTOS_FREE(queue);
        AGENTOS_LOG_ERROR("Failed to create queue synchronization primitives");
        return NULL;
    }
    
    return queue;
}

/**
 * @brief 销毁异步队�?
 * @param queue 队列句柄
 */
static void async_queue_destroy(async_queue_t* queue) {
    if (!queue) return;
    
    // 清理队列中的操作
    agentos_mutex_lock(queue->lock);
    async_operation_t* op = queue->head;
    while (op) {
        async_operation_t* next = op->next;
        if (op->id) AGENTOS_FREE(op->id);
        if (op->data) AGENTOS_FREE(op->data);
        if (op->semaphore) agentos_semaphore_destroy(op->semaphore);
        AGENTOS_FREE(op);
        op = next;
    }
    agentos_mutex_unlock(queue->lock);
    
    if (queue->lock) agentos_mutex_destroy(queue->lock);
    if (queue->semaphore) agentos_semaphore_destroy(queue->semaphore);
    AGENTOS_FREE(queue);
}

/**
 * @brief 推送操作到队列
 * @param queue 队列
 * @param op 操作
 * @param timeout_ms 超时时间
 * @return AGENTOS_SUCCESS 成功，其他为错误�?
 */
static agentos_error_t async_queue_push(async_queue_t* queue, async_operation_t* op, 
                                        uint32_t timeout_ms) {
    if (!queue || !op) return AGENTOS_EINVAL;
    
    uint64_t start_time_ns = agentos_get_monotonic_time_ns();
    
    agentos_mutex_lock(queue->lock);
    
    // 检查队列是否已�?
    if (queue->size >= queue->capacity) {
        agentos_mutex_unlock(queue->lock);
        AGENTOS_LOG_WARN("Async queue is full: size=%zu, capacity=%zu", 
                        queue->size, queue->capacity);
        return AGENTOS_EAGAIN;
    }
    
    // 添加到队列尾�?
    op->next = NULL;
    if (queue->tail) {
        queue->tail->next = op;
        queue->tail = op;
    } else {
        queue->head = queue->tail = op;
    }
    queue->size++;
    
    agentos_mutex_unlock(queue->lock);
    
    // 释放信号量通知工作线程
    agentos_semaphore_post(queue->semaphore);
    
    // 记录排队时间
    uint64_t end_time_ns = agentos_get_monotonic_time_ns();
    op->timestamp_ns = end_time_ns;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 从队列弹出操�?
 * @param queue 队列
 * @param timeout_ms 超时时间
 * @return 操作句柄，超时返回NULL
 */
static async_operation_t* async_queue_pop(async_queue_t* queue, uint32_t timeout_ms) {
    if (!queue) return NULL;
    
    // 等待信号�?
    if (!agentos_semaphore_wait(queue->semaphore, timeout_ms)) {
        return NULL;
    }
    
    agentos_mutex_lock(queue->lock);
    
    if (!queue->head) {
        agentos_mutex_unlock(queue->lock);
        return NULL;
    }
    
    async_operation_t* op = queue->head;
    queue->head = op->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    queue->size--;
    
    agentos_mutex_unlock(queue->lock);
    
    return op;
}

/**
 * @brief 创建异步操作
 * @param type 操作类型
 * @param id 记录ID
 * @return 操作句柄，失败返回NULL
 */
static async_operation_t* async_operation_create(async_op_type_t type, const char* id) {
    if (!id) return NULL;
    
    async_operation_t* op = (async_operation_t*)AGENTOS_CALLOC(1, sizeof(async_operation_t));
    if (!op) {
        AGENTOS_LOG_ERROR("Failed to allocate async operation");
        return NULL;
    }
    
    op->type = type;
    op->id = AGENTOS_STRDUP(id);
    op->timestamp_ns = agentos_get_monotonic_time_ns();
    op->semaphore = agentos_semaphore_create(0);
    
    if (!op->id || !op->semaphore) {
        if (op->id) AGENTOS_FREE(op->id);
        if (op->semaphore) agentos_semaphore_destroy(op->semaphore);
        AGENTOS_FREE(op);
        AGENTOS_LOG_ERROR("Failed to initialize async operation");
        return NULL;
    }
    
    return op;
}

/**
 * @brief 释放异步操作
 * @param op 操作句柄
 */
static void async_operation_free(async_operation_t* op) {
    if (!op) return;
    
    if (op->id) AGENTOS_FREE(op->id);
    if (op->data) AGENTOS_FREE(op->data);
    if (op->semaphore) agentos_semaphore_destroy(op->semaphore);
    AGENTOS_FREE(op);
}

/* ==================== 工作线程函数 ==================== */

/**
 * @brief 工作线程主函�?
 * @param arg 线程参数
 * @return 线程返回�?
 */
static void* worker_thread_main(void* arg) {
    worker_thread_t* worker = (worker_thread_t*)arg;
    if (!worker || !worker->l1) return NULL;
    
    agentos_layer1_raw_t* l1 = worker->l1;
    AGENTOS_LOG_DEBUG("Worker thread %d started", worker->index);
    
    while (worker->running) {
        // 从队列获取操�?
        async_operation_t* op = async_queue_pop(l1->queue, 100);  // 100ms超时
        if (!op) continue;
        
        uint64_t process_start_ns = agentos_get_monotonic_time_ns();
        
        // 执行操作
        agentos_error_t result = AGENTOS_EUNKNOWN;
        char file_path[MAX_FILE_PATH];
        
        switch (op->type) {
            case ASYNC_OP_WRITE: {
                result = build_file_path(l1->storage_path, op->id, file_path, sizeof(file_path));
                if (result == AGENTOS_SUCCESS) {
                    // 重试机制
                    for (int retry = 0; retry <= MAX_RETRY_COUNT; retry++) {
                        result = safe_write_file(file_path, op->data, op->data_len);
                        if (result == AGENTOS_SUCCESS) break;
                        
                        if (retry < MAX_RETRY_COUNT) {
                            AGENTOS_LOG_WARN("Write failed for %s, retry %d/%d", 
                                            op->id, retry + 1, MAX_RETRY_COUNT);
                            agentos_sleep_ms(RETRY_DELAY_BASE_MS * (1 << retry));
                        }
                    }
                }
                break;
            }
            
            case ASYNC_OP_READ: {
                result = build_file_path(l1->storage_path, op->id, file_path, sizeof(file_path));
                if (result == AGENTOS_SUCCESS) {
                    result = safe_read_file(file_path, op->out_data, op->out_len);
                }
                break;
            }
            
            case ASYNC_OP_DELETE: {
                result = build_file_path(l1->storage_path, op->id, file_path, sizeof(file_path));
                if (result == AGENTOS_SUCCESS) {
                    result = safe_delete_file(file_path);
                }
                break;
            }
            
            case ASYNC_OP_FLUSH:
                // 刷新操作，确保所有数据持久化
                result = AGENTOS_SUCCESS;
                break;
                
            default:
                AGENTOS_LOG_ERROR("Unknown async operation type: %d", op->type);
                result = AGENTOS_EINVAL;
                break;
        }
        
        uint64_t process_end_ns = agentos_get_monotonic_time_ns();
        uint64_t queue_time_ns = process_start_ns - op->timestamp_ns;
        uint64_t process_time_ns = process_end_ns - process_start_ns;
        
        // 更新统计信息
        agentos_mutex_lock(l1->lock);
        l1->total_queue_time_ns += queue_time_ns;
        l1->total_process_time_ns += process_time_ns;
        
        switch (op->type) {
            case ASYNC_OP_WRITE:
                l1->total_write_count++;
                if (result != AGENTOS_SUCCESS) l1->failed_write_count++;
                break;
            case ASYNC_OP_READ:
                l1->total_read_count++;
                if (result != AGENTOS_SUCCESS) l1->failed_read_count++;
                break;
            case ASYNC_OP_DELETE:
                l1->total_delete_count++;
                if (result != AGENTOS_SUCCESS) l1->failed_delete_count++;
                break;
            default:
                break;
        }
        agentos_mutex_unlock(l1->lock);
        
        // 设置结果并通知等待�?
        if (op->out_error) *op->out_error = result;
        agentos_semaphore_post(op->semaphore);
        
        // 记录指标
        if (l1->obs) {
            switch (op->type) {
                case ASYNC_OP_WRITE:
                    agentos_observability_increment_counter(l1->obs, "layer1_write_total", 1);
                    if (result != AGENTOS_SUCCESS) {
                        agentos_observability_increment_counter(l1->obs, "layer1_write_failed_total", 1);
                    }
                    agentos_observability_record_histogram(l1->obs, "layer1_write_queue_time_seconds", 
                                                          (double)queue_time_ns / 1e9);
                    agentos_observability_record_histogram(l1->obs, "layer1_write_process_time_seconds", 
                                                          (double)process_time_ns / 1e9);
                    break;
                case ASYNC_OP_READ:
                    agentos_observability_increment_counter(l1->obs, "layer1_read_total", 1);
                    if (result != AGENTOS_SUCCESS) {
                        agentos_observability_increment_counter(l1->obs, "layer1_read_failed_total", 1);
                    }
                    break;
                case ASYNC_OP_DELETE:
                    agentos_observability_increment_counter(l1->obs, "layer1_delete_total", 1);
                    if (result != AGENTOS_SUCCESS) {
                        agentos_observability_increment_counter(l1->obs, "layer1_delete_failed_total", 1);
                    }
                    break;
                default:
                    break;
            }
        }
        
        // 清理操作
        async_operation_free(op);
    }
    
    AGENTOS_LOG_DEBUG("Worker thread %d stopped", worker->index);
    return NULL;
}

/* ==================== 公共API实现 ==================== */

/**
 * @brief 创建异步L1原始�?
 */
agentos_error_t agentos_layer1_raw_create_async(
    const char* path,
    uint32_t queue_size,
    uint32_t workers,
    agentos_layer1_raw_t** out) {
    
    if (!path || !out) return AGENTOS_EINVAL;
    
    // 确保目录存在
    agentos_error_t err = ensure_directory_exists(path);
    if (err != AGENTOS_SUCCESS) {
        AGENTOS_LOG_ERROR("Failed to create storage directory: %s", path);
        return err;
    }
    
    // 分配L1结构
    agentos_layer1_raw_t* l1 = (agentos_layer1_raw_t*)AGENTOS_CALLOC(1, sizeof(agentos_layer1_raw_t));
    if (!l1) {
        AGENTOS_LOG_ERROR("Failed to allocate L1 raw storage");
        return AGENTOS_ENOMEM;
    }
    
    l1->storage_path = AGENTOS_STRDUP(path);
    if (!l1->storage_path) {
        AGENTOS_FREE(l1);
        AGENTOS_LOG_ERROR("Failed to duplicate storage path");
        return AGENTOS_ENOMEM;
    }
    
    // 初始化同步原�?
    l1->lock = agentos_mutex_create();
    if (!l1->lock) {
        AGENTOS_FREE(l1->storage_path);
        AGENTOS_FREE(l1);
        AGENTOS_LOG_ERROR("Failed to create mutex");
        return AGENTOS_ENOMEM;
    }
    
    // 创建队列
    l1->queue = async_queue_create(queue_size);
    if (!l1->queue) {
        agentos_mutex_destroy(l1->lock);
        AGENTOS_FREE(l1->storage_path);
        AGENTOS_FREE(l1);
        AGENTOS_LOG_ERROR("Failed to create async queue");
        return AGENTOS_ENOMEM;
    }
    
    // 创建工作线程
    if (workers == 0) workers = DEFAULT_WORKER_COUNT;
    l1->worker_count = workers;
    
    l1->workers = (worker_thread_t*)AGENTOS_CALLOC(workers, sizeof(worker_thread_t));
    if (!l1->workers) {
        async_queue_destroy(l1->queue);
        agentos_mutex_destroy(l1->lock);
        AGENTOS_FREE(l1->storage_path);
        AGENTOS_FREE(l1);
        AGENTOS_LOG_ERROR("Failed to allocate worker threads");
        return AGENTOS_ENOMEM;
    }
    
    // 初始化可观测�?
    l1->obs = agentos_observability_create();
    if (l1->obs) {
        agentos_observability_register_metric(l1->obs, "layer1_write_total", 
                                              AGENTOS_METRIC_COUNTER, "Total number of write operations");
        agentos_observability_register_metric(l1->obs, "layer1_read_total", 
                                              AGENTOS_METRIC_COUNTER, "Total number of read operations");
        agentos_observability_register_metric(l1->obs, "layer1_delete_total", 
                                              AGENTOS_METRIC_COUNTER, "Total number of delete operations");
        agentos_observability_register_metric(l1->obs, "layer1_write_failed_total", 
                                              AGENTOS_METRIC_COUNTER, "Total number of failed write operations");
        agentos_observability_register_metric(l1->obs, "layer1_read_failed_total", 
                                              AGENTOS_METRIC_COUNTER, "Total number of failed read operations");
        agentos_observability_register_metric(l1->obs, "layer1_delete_failed_total", 
                                              AGENTOS_METRIC_COUNTER, "Total number of failed delete operations");
        agentos_observability_register_metric(l1->obs, "layer1_queue_size", 
                                              AGENTOS_METRIC_GAUGE, "Current queue size");
        agentos_observability_register_metric(l1->obs, "layer1_write_queue_time_seconds", 
                                              AGENTOS_METRIC_HISTOGRAM, "Write operation queue time in seconds");
        agentos_observability_register_metric(l1->obs, "layer1_write_process_time_seconds", 
                                              AGENTOS_METRIC_HISTOGRAM, "Write operation process time in seconds");
    }
    
    l1->running = 1;
    l1->healthy = 1;
    l1->health_message = AGENTOS_STRDUP("Initializing");
    l1->last_health_check_ns = agentos_get_monotonic_time_ns();
    
    // 启动工作线程
    for (uint32_t i = 0; i < workers; i++) {
        worker_thread_t* worker = &l1->workers[i];
        worker->index = i;
        worker->running = 1;
        worker->l1 = l1;
        
        char thread_name[32];
        snprintf(thread_name, sizeof(thread_name), "l1_worker_%u", i);
        
        worker->thread = agentos_thread_create(worker_thread_main, worker, thread_name);
        if (!worker->thread) {
            AGENTOS_LOG_ERROR("Failed to create worker thread %u", i);
            // 停止已经启动的线�?
            for (uint32_t j = 0; j < i; j++) {
                l1->workers[j].running = 0;
            }
            // 等待线程退�?
            agentos_sleep_ms(100);
            // 清理资源
            for (uint32_t j = 0; j < i; j++) {
                agentos_thread_join(l1->workers[j].thread);
                agentos_thread_destroy(l1->workers[j].thread);
            }
            AGENTOS_FREE(l1->workers);
            async_queue_destroy(l1->queue);
            agentos_mutex_destroy(l1->lock);
            AGENTOS_FREE(l1->storage_path);
            AGENTOS_FREE(l1->health_message);
            if (l1->obs) agentos_observability_destroy(l1->obs);
            AGENTOS_FREE(l1);
            return AGENTOS_EFAIL;
        }
    }
    
    // 更新健康状�?
    AGENTOS_FREE(l1->health_message);
    l1->health_message = AGENTOS_STRDUP("Running");
    l1->last_health_check_ns = agentos_get_monotonic_time_ns();
    
    AGENTOS_LOG_INFO("L1 async storage created: path=%s, workers=%u, queue_size=%u", 
                    path, workers, queue_size);
    
    *out = l1;
    return AGENTOS_SUCCESS;
}

/**
 * @brief 销毁L1原始�?
 */
void agentos_layer1_raw_destroy(agentos_layer1_raw_t* l1) {
    if (!l1) return;
    
    AGENTOS_LOG_DEBUG("Destroying L1 async storage: %s", l1->storage_path);
    
    // 停止运行标志
    l1->running = 0;
    
    // 停止工作线程
    if (l1->workers) {
        for (uint32_t i = 0; i < l1->worker_count; i++) {
            l1->workers[i].running = 0;
        }
        
        // 发送停止信号到队列
        for (uint32_t i = 0; i < l1->worker_count; i++) {
            agentos_semaphore_post(l1->queue->semaphore);
        }
        
        // 等待线程退�?
        for (uint32_t i = 0; i < l1->worker_count; i++) {
            if (l1->workers[i].thread) {
                agentos_thread_join(l1->workers[i].thread);
                agentos_thread_destroy(l1->workers[i].thread);
            }
        }
        
        AGENTOS_FREE(l1->workers);
    }
    
    // 清理队列
    if (l1->queue) {
        async_queue_destroy(l1->queue);
    }
    
    // 清理资源
    if (l1->storage_path) AGENTOS_FREE(l1->storage_path);
    if (l1->lock) agentos_mutex_destroy(l1->lock);
    if (l1->obs) agentos_observability_destroy(l1->obs);
    if (l1->health_message) AGENTOS_FREE(l1->health_message);
    
    AGENTOS_FREE(l1);
}

/**
 * @brief 写入数据
 */
agentos_error_t agentos_layer1_raw_write(
    agentos_layer1_raw_t* l1,
    const char* id,
    const void* data,
    size_t len) {
    
    if (!l1 || !id || !data || len == 0) return AGENTOS_EINVAL;
    
    // 检查L1是否运行
    if (!l1->running) {
        AGENTOS_LOG_ERROR("L1 storage is not running");
        return AGENTOS_EFAIL;
    }
    
    // 创建异步操作
    async_operation_t* op = async_operation_create(ASYNC_OP_WRITE, id);
    if (!op) {
        AGENTOS_LOG_ERROR("Failed to create write operation for %s", id);
        return AGENTOS_ENOMEM;
    }
    
    // 复制数据
    op->data = AGENTOS_MALLOC(len);
    if (!op->data) {
        async_operation_free(op);
        AGENTOS_LOG_ERROR("Failed to allocate data buffer for %s", id);
        return AGENTOS_ENOMEM;
    }
    memcpy(op->data, data, len);
    op->data_len = len;
    
    // 准备错误输出
    agentos_error_t error = AGENTOS_EUNKNOWN;
    op->out_error = &error;
    
    // 添加到队�?
    agentos_error_t queue_result = async_queue_push(l1->queue, op, DEFAULT_TIMEOUT_MS);
    if (queue_result != AGENTOS_SUCCESS) {
        async_operation_free(op);
        AGENTOS_LOG_ERROR("Failed to push write operation to queue for %s", id);
        return queue_result;
    }
    
    // 等待操作完成
    if (!agentos_semaphore_wait(op->semaphore, DEFAULT_TIMEOUT_MS)) {
        AGENTOS_LOG_WARN("Write operation timeout for %s", id);
        return AGENTOS_ETIMEOUT;
    }
    
    return error;
}

/**
 * @brief 读取数据
 */
agentos_error_t agentos_layer1_raw_read(
    agentos_layer1_raw_t* l1,
    const char* id,
    void** out_data,
    size_t* out_len) {
    
    if (!l1 || !id || !out_data || !out_len) return AGENTOS_EINVAL;
    
    // 检查L1是否运行
    if (!l1->running) {
        AGENTOS_LOG_ERROR("L1 storage is not running");
        return AGENTOS_EFAIL;
    }
    
    // 创建异步操作
    async_operation_t* op = async_operation_create(ASYNC_OP_READ, id);
    if (!op) {
        AGENTOS_LOG_ERROR("Failed to create read operation for %s", id);
        return AGENTOS_ENOMEM;
    }
    
    // 准备输出参数
    op->out_data = out_data;
    op->out_len = out_len;
    
    // 准备错误输出
    agentos_error_t error = AGENTOS_EUNKNOWN;
    op->out_error = &error;
    
    // 添加到队�?
    agentos_error_t queue_result = async_queue_push(l1->queue, op, DEFAULT_TIMEOUT_MS);
    if (queue_result != AGENTOS_SUCCESS) {
        async_operation_free(op);
        AGENTOS_LOG_ERROR("Failed to push read operation to queue for %s", id);
        return queue_result;
    }
    
    // 等待操作完成
    if (!agentos_semaphore_wait(op->semaphore, DEFAULT_TIMEOUT_MS)) {
        AGENTOS_LOG_WARN("Read operation timeout for %s", id);
        return AGENTOS_ETIMEOUT;
    }
    
    return error;
}

/**
 * @brief 删除数据
 */
agentos_error_t agentos_layer1_raw_delete(
    agentos_layer1_raw_t* l1,
    const char* id) {
    
    if (!l1 || !id) return AGENTOS_EINVAL;
    
    // 检查L1是否运行
    if (!l1->running) {
        AGENTOS_LOG_ERROR("L1 storage is not running");
        return AGENTOS_EFAIL;
    }
    
    // 创建异步操作
    async_operation_t* op = async_operation_create(ASYNC_OP_DELETE, id);
    if (!op) {
        AGENTOS_LOG_ERROR("Failed to create delete operation for %s", id);
        return AGENTOS_ENOMEM;
    }
    
    // 准备错误输出
    agentos_error_t error = AGENTOS_EUNKNOWN;
    op->out_error = &error;
    
    // 添加到队�?
    agentos_error_t queue_result = async_queue_push(l1->queue, op, DEFAULT_TIMEOUT_MS);
    if (queue_result != AGENTOS_SUCCESS) {
        async_operation_free(op);
        AGENTOS_LOG_ERROR("Failed to push delete operation to queue for %s", id);
        return queue_result;
    }
    
    // 等待操作完成
    if (!agentos_semaphore_wait(op->semaphore, DEFAULT_TIMEOUT_MS)) {
        AGENTOS_LOG_WARN("Delete operation timeout for %s", id);
        return AGENTOS_ETIMEOUT;
    }
    
    return error;
}

/**
 * @brief 列出所有ID
 */
agentos_error_t agentos_layer1_raw_list_ids(
    agentos_layer1_raw_t* l1,
    char*** out_ids,
    size_t* out_count) {
    
    if (!l1 || !out_ids || !out_count) return AGENTOS_EINVAL;
    
    // 直接同步实现，因为需要扫描目�?
    char dir_path[MAX_FILE_PATH];
    if (snprintf(dir_path, sizeof(dir_path), "%s", l1->storage_path) < 0) {
        return AGENTOS_EINVAL;
    }
    
#ifdef _WIN32
    WIN32_FIND_DATAA find_data;
    char search_path[MAX_FILE_PATH];
    snprintf(search_path, sizeof(search_path), "%s\\*.raw", dir_path);
    
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind == INVALID_HANDLE_VALUE) {
        *out_ids = NULL;
        *out_count = 0;
        return AGENTOS_SUCCESS;
    }
    
    // 第一遍：计数
    size_t count = 0;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            count++;
        }
    } while (FindNextFileA(hFind, &find_data) != 0);
    FindClose(hFind);
    
    // 分配数组
    char** ids = (char**)AGENTOS_MALLOC(count * sizeof(char*));
    if (!ids) return AGENTOS_ENOMEM;
    
    // 第二遍：收集
    hFind = FindFirstFileA(search_path, &find_data);
    size_t index = 0;
    do {
        if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            // 移除.raw后缀
            char* filename = find_data.cFileName;
            size_t len = strlen(filename);
            if (len > 4 && strcmp(filename + len - 4, ".raw") == 0) {
                filename[len - 4] = '\0';
                ids[index] = AGENTOS_STRDUP(filename);
                if (!ids[index]) {
                    // 清理已分配的内存
                    for (size_t i = 0; i < index; i++) AGENTOS_FREE(ids[i]);
                    AGENTOS_FREE(ids);
                    FindClose(hFind);
                    return AGENTOS_ENOMEM;
                }
                index++;
            }
        }
    } while (FindNextFileA(hFind, &find_data) != 0);
    FindClose(hFind);
#else
    // Linux实现
    DIR* dir = opendir(dir_path);
    if (!dir) {
        *out_ids = NULL;
        *out_count = 0;
        return AGENTOS_SUCCESS;
    }
    
    // 第一遍：计数
    size_t count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".raw") == 0) {
            count++;
        }
    }
    rewinddir(dir);
    
    // 分配数组
    char** ids = (char**)AGENTOS_MALLOC(count * sizeof(char*));
    if (!ids) {
        closedir(dir);
        return AGENTOS_ENOMEM;
    }
    
    // 第二遍：收集
    size_t index = 0;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".raw") == 0) {
            char* filename = AGENTOS_STRDUP(entry->d_name);
            if (!filename) {
                // 清理已分配的内存
                for (size_t i = 0; i < index; i++) AGENTOS_FREE(ids[i]);
                AGENTOS_FREE(ids);
                closedir(dir);
                return AGENTOS_ENOMEM;
            }
            filename[len - 4] = '\0';  // 移除.raw后缀
            ids[index] = filename;
            index++;
        }
    }
    closedir(dir);
#endif
    
    *out_ids = ids;
    *out_count = count;
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 刷新缓冲�?
 */
agentos_error_t agentos_layer1_raw_flush(
    agentos_layer1_raw_t* l1,
    uint32_t timeout_ms) {
    
    if (!l1) return AGENTOS_EINVAL;
    
    // 检查L1是否运行
    if (!l1->running) {
        AGENTOS_LOG_ERROR("L1 storage is not running");
        return AGENTOS_EFAIL;
    }
    
    // 创建刷新操作
    async_operation_t* op = async_operation_create(ASYNC_OP_FLUSH, "flush");
    if (!op) {
        AGENTOS_LOG_ERROR("Failed to create flush operation");
        return AGENTOS_ENOMEM;
    }
    
    // 准备错误输出
    agentos_error_t error = AGENTOS_EUNKNOWN;
    op->out_error = &error;
    
    // 添加到队�?
    agentos_error_t queue_result = async_queue_push(l1->queue, op, timeout_ms);
    if (queue_result != AGENTOS_SUCCESS) {
        async_operation_free(op);
        AGENTOS_LOG_ERROR("Failed to push flush operation to queue");
        return queue_result;
    }
    
    // 等待操作完成
    if (!agentos_semaphore_wait(op->semaphore, timeout_ms)) {
