/**
 * @file atomic_logging.c
 * @brief 统一分层日志系统原子层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 本文件实现统一分层日志系统的原子层功能，提供：
 * 1. 线程安全的日志记录缓冲和环形队列管理
 * 2. 高性能批量提交机制（mutex保护的多生产者-单消费者）
 * 3. 异步刷盘和背压控制
 * 4. 统计信息收集
 */

#include "atomic_logging.h"
#include <stdlib.h>

#include "include/memory_compat.h"
#include "string_compat.h"
#include <string.h>
#include "platform.h"

static AGENTOS_THREAD_LOCAL thread_local_buffer_t* g_tls_log_buffer = NULL;

/* ==================== 内部常量定义 ==================== */

static const size_t DEFAULT_THREAD_LOCAL_BUFFER_SIZE = 8192;

static const size_t DEFAULT_RING_BUFFER_CAPACITY = 1024;

static const size_t DEFAULT_BATCH_COMMIT_THRESHOLD = 16;

typedef enum {
    NODE_STATE_FREE = 0,
    NODE_STATE_WRITTEN = 1,
    NODE_STATE_PROCESSED = 2
} node_state_t;

/* ==================== 平台无关辅助函数 ==================== */

static void atomic_sleep_ms(uint32_t ms) {
#if AGENTOS_PLATFORM_WINDOWS
    Sleep(ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

/* ==================== 内部数据结构 ==================== */

typedef struct {
    log_record_t* records;
    size_t write_pos;
    size_t capacity;
} thread_local_buffer_t;

typedef struct {
    AtomicLogRecordNode* nodes;
    size_t capacity;
    size_t producer_pos;
    size_t consumer_pos;
    agentos_mutex_t mutex;
    agentos_cond_t not_full;
    uint64_t total_submitted;
    uint64_t total_consumed;
    uint64_t total_dropped;
} ring_buffer_t;

typedef struct {
    atomic_logging_config_t manager;
    bool initialized;
    ring_buffer_t ring_buffer;
    agentos_thread_t flush_thread;
    bool flush_thread_running;
} atomic_logging_state_t;

static atomic_logging_state_t g_atomic_state = {
    .initialized = false,
    .flush_thread_running = false
};

/* ==================== 内部辅助函数 ==================== */

static thread_local_buffer_t* get_thread_local_buffer(void) {
    if (!g_tls_log_buffer) {
        g_tls_log_buffer = (thread_local_buffer_t*)AGENTOS_CALLOC(1, sizeof(thread_local_buffer_t));
        if (g_tls_log_buffer) {
            g_tls_log_buffer->capacity = g_atomic_state.manager.batch_commit_threshold;
            g_tls_log_buffer->records = (log_record_t*)AGENTOS_MALLOC(g_tls_log_buffer->capacity * sizeof(log_record_t));
            g_tls_log_buffer->write_pos = 0;

            if (!g_tls_log_buffer->records) {
                AGENTOS_FREE(g_tls_log_buffer);
                g_tls_log_buffer = NULL;
                return NULL;
            }
        }
    }

    return g_tls_log_buffer;
}

static void free_thread_local_buffer(thread_local_buffer_t* buffer) {
    if (buffer) {
        AGENTOS_FREE(buffer->records);
        AGENTOS_FREE(buffer);
    }
}

static int submit_to_ring_buffer(const log_record_t* record) {
    if (!record || !g_atomic_state.initialized) {
        return -1;
    }

    agentos_mutex_lock(&g_atomic_state.ring_buffer.mutex);

    size_t next_producer_pos = (g_atomic_state.ring_buffer.producer_pos + 1) % g_atomic_state.ring_buffer.capacity;
    if (next_producer_pos == g_atomic_state.ring_buffer.consumer_pos) {
        g_atomic_state.ring_buffer.total_dropped++;
        agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
        return -2;
    }

    AtomicLogRecordNode* node = &g_atomic_state.ring_buffer.nodes[g_atomic_state.ring_buffer.producer_pos];

    memcpy(&node->record, record, sizeof(log_record_t));

    node->state = NODE_STATE_WRITTEN;

    g_atomic_state.ring_buffer.producer_pos = next_producer_pos;
    g_atomic_state.ring_buffer.total_submitted++;

    agentos_cond_signal(&g_atomic_state.ring_buffer.not_full);
    agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);

    return 0;
}

static bool consume_from_ring_buffer(log_record_t* record) {
    if (!record || !g_atomic_state.initialized) {
        return false;
    }

    agentos_mutex_lock(&g_atomic_state.ring_buffer.mutex);

    if (g_atomic_state.ring_buffer.consumer_pos == g_atomic_state.ring_buffer.producer_pos) {
        agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
        return false;
    }

    AtomicLogRecordNode* node = &g_atomic_state.ring_buffer.nodes[g_atomic_state.ring_buffer.consumer_pos];

    if (node->state != NODE_STATE_WRITTEN) {
        agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);
        return false;
    }

    memcpy(record, &node->record, sizeof(log_record_t));

    node->state = NODE_STATE_PROCESSED;

    g_atomic_state.ring_buffer.consumer_pos = (g_atomic_state.ring_buffer.consumer_pos + 1) % g_atomic_state.ring_buffer.capacity;
    g_atomic_state.ring_buffer.total_consumed++;

    agentos_cond_signal(&g_atomic_state.ring_buffer.not_full);
    agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);

    return true;
}

static void* flush_thread_func(void* arg) {
    (void)arg;

    while (g_atomic_state.flush_thread_running) {
        log_record_t records[16];
        size_t count = 0;

        agentos_mutex_lock(&g_atomic_state.ring_buffer.mutex);

        while (count < sizeof(records) / sizeof(records[0])) {
            if (g_atomic_state.ring_buffer.consumer_pos == g_atomic_state.ring_buffer.producer_pos) {
                break;
            }

            AtomicLogRecordNode* node = &g_atomic_state.ring_buffer.nodes[g_atomic_state.ring_buffer.consumer_pos];

            if (node->state == NODE_STATE_WRITTEN) {
                memcpy(&records[count], &node->record, sizeof(log_record_t));
                node->state = NODE_STATE_PROCESSED;
                g_atomic_state.ring_buffer.consumer_pos = (g_atomic_state.ring_buffer.consumer_pos + 1) % g_atomic_state.ring_buffer.capacity;
                g_atomic_state.ring_buffer.total_consumed++;
                count++;
            } else {
                break;
            }
        }

        agentos_cond_signal(&g_atomic_state.ring_buffer.not_full);
        agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);

        for (size_t i = 0; i < count; i++) {
            service_log_output_record(&records[i]);
        }

        atomic_sleep_ms(g_atomic_state.manager.flush_thread_sleep_ms);
    }

    return NULL;
}

/* ==================== 公开API实现 ==================== */

int atomic_logging_init(const atomic_logging_config_t* manager) {
    if (g_atomic_state.initialized) {
        return -1;
    }

    if (manager) {
        memcpy(&g_atomic_state.manager, manager, sizeof(atomic_logging_config_t));
    } else {
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

    g_atomic_state.ring_buffer.capacity = g_atomic_state.manager.ring_buffer_capacity;
    g_atomic_state.ring_buffer.nodes = (AtomicLogRecordNode*)AGENTOS_CALLOC(
        g_atomic_state.ring_buffer.capacity, sizeof(AtomicLogRecordNode));

    if (!g_atomic_state.ring_buffer.nodes) {
        return -2;
    }

    g_atomic_state.ring_buffer.producer_pos = 0;
    g_atomic_state.ring_buffer.consumer_pos = 0;
    g_atomic_state.ring_buffer.total_submitted = 0;
    g_atomic_state.ring_buffer.total_consumed = 0;
    g_atomic_state.ring_buffer.total_dropped = 0;

    if (agentos_mutex_init(&g_atomic_state.ring_buffer.mutex) != 0) {
        AGENTOS_FREE(g_atomic_state.ring_buffer.nodes);
        return -3;
    }

    if (agentos_cond_init(&g_atomic_state.ring_buffer.not_full) != 0) {
        agentos_mutex_destroy(&g_atomic_state.ring_buffer.mutex);
        AGENTOS_FREE(g_atomic_state.ring_buffer.nodes);
        return -4;
    }

    g_atomic_state.flush_thread_running = true;
    if (agentos_thread_create(&g_atomic_state.flush_thread, flush_thread_func, NULL) != 0) {
        agentos_cond_destroy(&g_atomic_state.ring_buffer.not_full);
        agentos_mutex_destroy(&g_atomic_state.ring_buffer.mutex);
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

    thread_local_buffer_t* buffer = get_thread_local_buffer();
    if (!buffer) {
        return -2;
    }

    int result = submit_to_ring_buffer(record);

    if (result != 0 && !non_blocking) {
        agentos_mutex_lock(&g_atomic_state.ring_buffer.mutex);
        int max_retries = 100;
        while (submit_to_ring_buffer(record) != 0 && g_atomic_state.initialized && max_retries-- > 0) {
            agentos_cond_timedwait(&g_atomic_state.ring_buffer.not_full,
                                   &g_atomic_state.ring_buffer.mutex, 10);
        }
        agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);

        if (max_retries <= 0) {
            return -3;
        }
        return 0;
    }

    return result;
}

int atomic_logging_submit_lockfree(const log_record_t* record, bool non_blocking) {
    return atomic_logging_submit(record, non_blocking);
}

int atomic_logging_flush(void) {
    if (!g_atomic_state.initialized) {
        return -1;
    }

    thread_local_buffer_t* buffer = get_thread_local_buffer();
    if (buffer && buffer->write_pos > 0) {
        for (size_t i = 0; i < buffer->write_pos; i++) {
            submit_to_ring_buffer(&buffer->records[i]);
        }
        buffer->write_pos = 0;
    }

    int max_wait = 100;
    while (max_wait-- > 0) {
        agentos_mutex_lock(&g_atomic_state.ring_buffer.mutex);
        bool empty = (g_atomic_state.ring_buffer.consumer_pos == g_atomic_state.ring_buffer.producer_pos);
        agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);

        if (empty) {
            break;
        }

        atomic_sleep_ms(1);
    }

    return 0;
}

int atomic_logging_get_stats(atomic_logging_stats_t* stats) {
    if (!g_atomic_state.initialized || !stats) {
        return -1;
    }

    memset(stats, 0, sizeof(atomic_logging_stats_t));

    agentos_mutex_lock(&g_atomic_state.ring_buffer.mutex);

    if (g_atomic_state.ring_buffer.producer_pos >= g_atomic_state.ring_buffer.consumer_pos) {
        stats->current_queue_size = g_atomic_state.ring_buffer.producer_pos - g_atomic_state.ring_buffer.consumer_pos;
    } else {
        stats->current_queue_size = g_atomic_state.ring_buffer.capacity - g_atomic_state.ring_buffer.consumer_pos + g_atomic_state.ring_buffer.producer_pos;
    }

    stats->total_submitted = g_atomic_state.ring_buffer.total_submitted;
    stats->total_acquired = g_atomic_state.ring_buffer.total_consumed;
    stats->current_queue_size = g_atomic_state.ring_buffer.capacity;

    agentos_mutex_unlock(&g_atomic_state.ring_buffer.mutex);

    return 0;
}

void atomic_logging_cleanup(void) {
    if (!g_atomic_state.initialized) {
        return;
    }

    g_atomic_state.flush_thread_running = false;
    agentos_thread_join(g_atomic_state.flush_thread, NULL);

    if (g_tls_log_buffer) {
        free_thread_local_buffer(g_tls_log_buffer);
        g_tls_log_buffer = NULL;
    }

    agentos_cond_destroy(&g_atomic_state.ring_buffer.not_full);
    agentos_mutex_destroy(&g_atomic_state.ring_buffer.mutex);
    AGENTOS_FREE(g_atomic_state.ring_buffer.nodes);

    memset(&g_atomic_state, 0, sizeof(g_atomic_state));
}
