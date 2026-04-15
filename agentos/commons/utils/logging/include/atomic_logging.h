/**
 * @file atomic_logging.h
 * @brief 统一分层日志系统原子层API
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块提供统一的分层日志系统原子层接口，专注于：
 * - 高性能：无锁队列、零拷贝、批量提交
 * - 线程安全：多生产者单消费者、内存屏障、原子操作
 * - 低延迟：异步刷新、线程本地缓冲、最小化系统调用
 * 
 * 原子层设计原则：
 * 1. **无锁设计**：使用CAS（Compare-And-Swap）操作避免锁竞争
 * 2. **零拷贝**：日志格式化直接写入目标缓冲，避免内存复制
 * 3. **批量提交**：多个日志记录批量提交，减少系统调用开销
 * 4. **分离关注点**：写入线程与刷新线程分离，避免I/O阻塞
 * 
 * 架构角色：
 * - 接收来自核心层的日志记录
 * - 提供线程安全的缓冲和队列管理
 * - 将格式化后的日志传递给服务层输出
 * 
 * 注意：原子层是内部实现细节，大多数用户应该使用核心层API。
 * 只有需要极致性能或特殊线程安全需求的组件才应直接使用原子层。
 */

#ifndef AGENTOS_COMMON_ATOMIC_LOGGING_H
#define AGENTOS_COMMON_ATOMIC_LOGGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "logging.h"
#include <stdbool.h>

/* 跨平台原子操作支持 - 使用统一的 atomic_compat.h */
#include <agentos/atomic_compat.h>
#define HAVE_STDATOMIC 1

/* ==================== 原子层配置 ==================== */

/**
 * @brief 原子层配置结构体
 * 
 * 配置原子层的行为，优化性能和资源使用。
 */
typedef struct {
    /** @brief 是否启用无锁模式，true时使用CAS操作，false时使用互斥锁 */
    bool lock_free_mode;
    
    /** @brief 每个线程的本地缓冲大小（字节） */
    size_t thread_local_buffer_size;
    
    /** @brief 全局环形队列大小（记录数） */
    size_t ring_buffer_capacity;
    
    /** @brief 批量提交阈值，达到此数量时触发批量提交 */
    size_t batch_commit_threshold;
    
    /** @brief 最大批处理大小，单次批量提交的最大记录数 */
    size_t max_batch_size;
    
    /** @brief 异步刷新线程的休眠间隔（毫秒） */
    uint32_t flush_thread_sleep_ms;
    
    /** @brief 是否启用内存池，减少动态内存分配 */
    bool enable_memory_pool;
    
    /** @brief 内存池初始大小（字节） */
    size_t memory_pool_initial_size;
    
    /** @brief 内存池块大小（字节） */
    size_t memory_pool_block_size;
} atomic_logging_config_t;

/* ==================== 原子层数据结构 ==================== */

/**
 * @brief 原子日志记录节点
 * 
 * 环形队列中的节点，包含一条日志记录和必要的元数据。
 * 设计为缓存行对齐（通常64字节），避免伪共享。
 */
typedef struct _AtomicLogRecordNode {
    /** @brief 日志记录数据 */
    log_record_t record;
    
    /** @brief 节点状态：0=空闲，1=已写入待处理，2=已处理可回收 */
    _Atomic uint32_t state;
    
    /** @brief 序列号，用于确保顺序一致性 */
    _Atomic uint64_t sequence;
    
    /** @brief 填充字节，确保缓存行对齐 */
    uint8_t _padding[1];
} AtomicLogRecordNode;

/**
 * @brief 无锁环形缓冲队列
 * 
 * 多生产者单消费者（MPSC）无锁环形队列。
 * 生产者可以并发写入，消费者顺序读取。
 */
typedef struct {
    /** @brief 节点数组 */
    AtomicLogRecordNode* nodes;
    
    /** @brief 队列容量（节点数） */
    size_t capacity;
    
    /** @brief 生产者头指针（写入位置） */
    _Atomic size_t head;
    
    /** @brief 消费者尾指针（读取位置） */
    _Atomic size_t tail;
    
    /** @brief 屏障指针，用于内存顺序控制 */
    _Atomic size_t barrier;
} LockFreeRingBuffer;

/**
 * @brief 线程本地缓冲
 * 
 * 每个线程独立的缓冲，减少全局队列竞争。
 * 当本地缓冲满时，批量提交到全局队列。
 */
typedef struct {
    /** @brief 缓冲数组 */
    log_record_t* buffer;
    
    /** @brief 缓冲容量 */
    size_t capacity;
    
    /** @brief 当前写入位置 */
    size_t position;
    
    /** @brief 线程ID */
    uint64_t thread_id;
} ThreadLocalBuffer;

/* ==================== 原子层API函数 ==================== */

/**
 * @brief 初始化原子层
 * 
 * 初始化原子层内部数据结构，包括环形队列、内存池等。
 * 必须在使用任何原子层函数之前调用。
 * 
 * @param manager 原子层配置，为NULL时使用默认配置
 * @return 0 成功，负值表示错误
 */
int atomic_logging_init(const atomic_logging_config_t* manager);

/**
 * @brief 提交日志记录到原子层（无锁版本）
 * 
 * 将日志记录提交到无锁环形队列，支持多线程并发调用。
 * 如果队列已满，函数将阻塞直到有空间可用（除非配置了非阻塞模式）。
 * 
 * @param record 日志记录
 * @param non_blocking 是否非阻塞，true时队列满则立即返回失败
 * @return 0 成功，负值表示错误
 */
int atomic_logging_submit_lockfree(const log_record_t* record, bool non_blocking);

/**
 * @brief 提交日志记录到原子层（互斥锁版本）
 * 
 * 将日志记录提交到带互斥锁保护的队列。
 * 用于不支持无锁操作的环境或调试目的。
 * 
 * @param record 日志记录
 * @return 0 成功，负值表示错误
 */
int atomic_logging_submit_mutex(const log_record_t* record);

/**
 * @brief 批量提交日志记录
 * 
 * 批量提交多个日志记录到原子层，减少函数调用开销。
 * 
 * @param records 日志记录数组
 * @param count 记录数量
 * @return 成功提交的记录数，负值表示错误
 */
int atomic_logging_submit_batch(const log_record_t* records, size_t count);

/**
 * @brief 从原子层获取日志记录
 * 
 * 从原子层获取下一条可用的日志记录。
 * 通常由服务层的刷新线程调用。
 * 
 * @param record 输出参数，接收日志记录
 * @param timeout_ms 超时时间（毫秒），0表示不阻塞，-1表示无限等待
 * @return 0 成功，1 队列为空，负值表示错误
 */
int atomic_logging_acquire(log_record_t* record, int timeout_ms);

/**
 * @brief 批量获取日志记录
 * 
 * 从原子层批量获取多个日志记录。
 * 
 * @param records 输出数组，接收日志记录
 * @param max_count 最大获取数量
 * @param timeout_ms 超时时间（毫秒）
 * @return 实际获取的记录数，负值表示错误
 */
int atomic_logging_acquire_batch(log_record_t* records, size_t max_count, int timeout_ms);

/**
 * @brief 释放日志记录节点
 * 
 * 释放已处理的日志记录节点，允许重用。
 * 必须在处理完日志记录后调用。
 * 
 * @param record 已处理的日志记录
 * @return 0 成功，负值表示错误
 */
int atomic_logging_release(const log_record_t* record);

/**
 * @brief 获取原子层统计信息
 * 
 * 获取原子层的运行时统计信息，用于监控和调试。
 * 
 * @param out_stats 输出参数，接收统计信息
 * @return 0 成功，负值表示错误
 */
int atomic_logging_get_stats(struct atomic_logging_stats* out_stats);

/**
 * @brief 获取线程本地缓冲
 * 
 * 获取或创建当前线程的本地缓冲。
 * 如果线程第一次调用，将创建新的本地缓冲。
 * 
 * @return 线程本地缓冲指针，失败返回NULL
 */
ThreadLocalBuffer* atomic_logging_get_thread_local_buffer(void);

/**
 * @brief 提交线程本地缓冲
 * 
 * 将线程本地缓冲中的所有记录批量提交到全局队列。
 * 
 * @param buffer 线程本地缓冲
 * @return 成功提交的记录数，负值表示错误
 */
int atomic_logging_flush_thread_local_buffer(ThreadLocalBuffer* buffer);

/**
 * @brief 刷新原子层
 * 
 * 强制刷新所有缓冲的记录，确保所有已提交的记录都可用。
 * 
 * @return 0 成功，负值表示错误
 */
int atomic_logging_flush(void);

/**
 * @brief 清理原子层
 * 
 * 清理原子层资源，释放所有分配的内存。
 * 必须在程序退出前调用。
 */
void atomic_logging_cleanup(void);

/* ==================== 性能监控结构体 ==================== */

/**
 * @brief 原子层统计信息
 * 
 * 原子层的运行时性能统计信息。
 */
typedef struct atomic_logging_stats {
    /** @brief 总提交记录数 */
    uint64_t total_submitted;
    
    /** @brief 总获取记录数 */
    uint64_t total_acquired;
    
    /** @brief 当前队列中的记录数 */
    size_t current_queue_size;
    
    /** @brief 队列最大使用率（百分比） */
    float queue_max_usage;
    
    /** @brief 提交操作平均延迟（纳秒） */
    uint64_t submit_avg_latency_ns;
    
    /** @brief 获取操作平均延迟（纳秒） */
    uint64_t acquire_avg_latency_ns;
    
    /** @brief 提交冲突次数（无锁模式） */
    uint64_t submit_collisions;
    
    /** @brief 内存池分配次数 */
    uint64_t memory_pool_allocations;
    
    /** @brief 内存池释放次数 */
    uint64_t memory_pool_frees;
    
    /** @brief 线程本地缓冲数量 */
    size_t thread_local_buffers;
    
    /** @brief 批量提交次数 */
    uint64_t batch_submits;
    
    /** @brief 批量获取次数 */
    uint64_t batch_acquires;
} atomic_logging_stats_t;

/* ==================== 内部使用的原子操作包装 ==================== */

/**
 * @brief 内存屏障：写屏障
 * 
 * 确保屏障之前的所有写操作对后续读操作可见。
 * 在写入共享数据后调用。
 */
static inline void atomic_write_barrier(void) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_thread_fence(__ATOMIC_RELEASE);
#elif defined(_MSC_VER)
    _WriteBarrier();
#else
    #error "Unsupported compiler for atomic operations"
#endif
}

/**
 * @brief 内存屏障：读屏障
 * 
 * 确保屏障之后的所有读操作能获取到最新数据。
 * 在读取共享数据前调用。
 */
static inline void atomic_read_barrier(void) {
#if defined(__GNUC__) || defined(__clang__)
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
#elif defined(_MSC_VER)
    _ReadBarrier();
#else
    #error "Unsupported compiler for atomic operations"
#endif
}

/**
 * @brief 原子比较并交换（CAS）操作
 * 
 * 比较内存位置的值与期望值，如果相等则更新为新值。
 * 
 * @param ptr 目标内存位置
 * @param expected 期望值指针（输入/输出）
 * @param desired 新值
 * @return true 操作成功，false 操作失败
 */
static inline bool agentos_atomic_cas_weak(volatile uint64_t* ptr, uint64_t* expected, uint64_t desired) {
#if defined(__GNUC__) || defined(__clang__)
    return __atomic_compare_exchange_n(ptr, expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#elif defined(_MSC_VER)
    return _InterlockedCompareExchange64((volatile __int64*)ptr, desired, *expected) == *expected;
#else
    #error "Unsupported compiler for atomic operations"
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_COMMON_ATOMIC_LOGGING_H */