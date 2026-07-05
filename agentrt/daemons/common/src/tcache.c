/**
 * @file tcache.c
 * @brief P1.20: per-Thread 缓存层实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * 在每个线程的 TLS 中维护一个小型分配缓存。
 * 小对象（≤ max_cache_size_class）从缓存获取，
 * 缓存未命中时批量从全局分配器获取。
 *
 * 大小类分桶策略：
 *   8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
 *   每个大小类有独立的缓存栈。
 *
 * 批量操作：
 *   - batch_fill: 一次从全局分配器获取 batch_fill_count 个对象
 *   - batch_flush: 缓存满时归还 batch_fill_count 个对象
 */

#include "tcache.h"
#include "logger.h"
#include "memory_compat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 大小类定义 ==================== */

#define NUM_SIZE_CLASSES 10
#define MAX_CACHED_ENTRIES 64
#define BATCH_FILL_COUNT 32
#define MAX_CACHE_SIZE_CLASS 4096
#define MAX_TOTAL_CACHED_BYTES (512 * 1024) /* 512KB */

static const uint32_t SIZE_CLASSES[NUM_SIZE_CLASSES] = {
    8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096
};

/* ==================== 缓存条目 ==================== */

typedef struct {
    void *entries[MAX_CACHED_ENTRIES]; /**< 缓存的对象指针 */
    uint32_t count;                     /**< 当前缓存数量 */
    uint32_t max_count;                 /**< 最大缓存数量 */
    uint32_t size_class;                /**< 大小类 */
} tcache_bin_t;

/* ==================== tcache 结构 ==================== */

struct tcache_s {
    tcache_bin_t bins[NUM_SIZE_CLASSES]; /**< 大小类缓存 */
    tcache_config_t config;
    size_t total_cached_bytes;           /**< 当前缓存总字节数 */

    /* 统计 */
    tcache_stats_t stats;
};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 根据大小找到对应的大小类索引
 */
static int find_size_class(size_t size)
{
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (size <= SIZE_CLASSES[i]) {
            return i;
        }
    }
    return -1; /* 超大对象，不缓存 */
}

/**
 * @brief 从全局分配器批量填充缓存
 */
static int bin_batch_fill(tcache_t *tc, tcache_bin_t *bin)
{
    if (bin->count >= bin->max_count) return 0;

    uint32_t to_fill = bin->max_count - bin->count;
    if (to_fill > tc->config.batch_fill_count) {
        to_fill = tc->config.batch_fill_count;
    }

    AGENTRT_LOG_DEBUG("Tcache: batch_fill size_class=%u (count=%u/%u, "
                      "to_fill=%u, cached_bytes=%zu/%u)",
                      bin->size_class, bin->count, bin->max_count,
                      to_fill, tc->total_cached_bytes,
                      tc->config.max_total_cached_bytes);

    uint32_t filled = 0;
    for (uint32_t i = 0; i < to_fill; i++) {
        void *ptr = AGENTRT_MALLOC(bin->size_class);
        if (!ptr) {
            AGENTRT_LOG_ERROR("Tcache: batch_fill OOM at %u/%u "
                              "(size_class=%u, total_cached=%zu)",
                              i, to_fill, bin->size_class,
                              tc->total_cached_bytes);
            break;
        }

        bin->entries[bin->count + i] = ptr;
        filled++;
        tc->total_cached_bytes += bin->size_class;
    }

    bin->count += filled;
    tc->stats.batch_fill_count++;

    AGENTRT_LOG_DEBUG("Tcache: batch_fill done (size_class=%u, filled=%u/%u, "
                      "cache_count=%u, cached_bytes=%zu)",
                      bin->size_class, filled, to_fill,
                      bin->count, tc->total_cached_bytes);

    return (int)filled;
}

/**
 * @brief 批量归还缓存到全局分配器
 */
static void bin_batch_flush(tcache_t *tc, tcache_bin_t *bin)
{
    uint32_t to_flush = bin->count;
    if (to_flush > tc->config.batch_fill_count) {
        to_flush = tc->config.batch_fill_count;
    }

    AGENTRT_LOG_DEBUG("Tcache: batch_flush size_class=%u (count=%u, "
                      "to_flush=%u, cached_bytes=%zu)",
                      bin->size_class, bin->count, to_flush,
                      tc->total_cached_bytes);

    for (uint32_t i = 0; i < to_flush; i++) {
        AGENTRT_FREE(bin->entries[bin->count - 1 - i]);
        tc->total_cached_bytes -= bin->size_class;
    }

    bin->count -= to_flush;
    tc->stats.batch_flush_count++;

    AGENTRT_LOG_DEBUG("Tcache: batch_flush done (size_class=%u, "
                      "flushed=%u, cache_count=%u, cached_bytes=%zu)",
                      bin->size_class, to_flush, bin->count,
                      tc->total_cached_bytes);
}

/* ==================== 生命周期实现 ==================== */

tcache_t *tcache_create(const tcache_config_t *config)
{
    tcache_t *tc = (tcache_t *)AGENTRT_CALLOC(1, sizeof(tcache_t));
    if (!tc) {
        AGENTRT_LOG_ERROR("Tcache: OOM creating tcache");
        return NULL;
    }

    tc->config.max_cache_entries = (config && config->max_cache_entries > 0)
                                       ? config->max_cache_entries
                                       : MAX_CACHED_ENTRIES;
    tc->config.batch_fill_count = (config && config->batch_fill_count > 0)
                                      ? config->batch_fill_count
                                      : BATCH_FILL_COUNT;
    tc->config.max_cache_size_class =
        (config && config->max_cache_size_class > 0)
            ? config->max_cache_size_class : MAX_CACHE_SIZE_CLASS;
    tc->config.max_total_cached_bytes =
        (config && config->max_total_cached_bytes > 0)
            ? config->max_total_cached_bytes : MAX_TOTAL_CACHED_BYTES;
    tc->config.enable_stats = config ? config->enable_stats : false;

    /* 初始化大小类缓存 */
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        tc->bins[i].max_count = tc->config.max_cache_entries;
        tc->bins[i].count = 0;
        tc->bins[i].size_class = SIZE_CLASSES[i];
    }

    tc->total_cached_bytes = 0;
    __builtin_memset(&tc->stats, 0, sizeof(tc->stats));

    AGENTRT_LOG_DEBUG("Tcache: created (max_entries=%zu, batch=%zu, "
                      "max_class=%u, max_bytes=%u)",
                      tc->config.max_cache_entries,
                      tc->config.batch_fill_count,
                      tc->config.max_cache_size_class,
                      tc->config.max_total_cached_bytes);
    return tc;
}

void tcache_destroy(tcache_t *tc)
{
    if (!tc) return;

    size_t cached_bytes = tc->total_cached_bytes;
    size_t cached_count = 0;
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        cached_count += tc->bins[i].count;
    }

    /* 归还所有缓存对象 */
    tcache_flush(tc);

    if (tc->config.enable_stats) {
        uint64_t total = tc->stats.cache_hit_count + tc->stats.cache_miss_count;
        double hit_rate = total > 0
            ? (double)tc->stats.cache_hit_count / (double)total * 100.0
            : 0.0;
        AGENTRT_LOG_INFO("Tcache: destroyed (hits=%llu misses=%llu "
                         "hit_rate=%.2f%%, oversize=%llu, "
                         "cached_at_exit=%zu objects/%zu bytes)",
                         (unsigned long long)tc->stats.cache_hit_count,
                         (unsigned long long)tc->stats.cache_miss_count,
                         hit_rate,
                         (unsigned long long)tc->stats.oversized_alloc,
                         cached_count, cached_bytes);
    } else {
        AGENTRT_LOG_INFO("Tcache: destroyed (cached_at_exit=%zu objects/%zu bytes)",
                         cached_count, cached_bytes);
    }

    AGENTRT_FREE(tc);
}

/* ==================== 分配操作实现 ==================== */

void *tcache_alloc(tcache_t *tc, size_t size)
{
    if (!tc || size == 0) {
        if (!tc) {
            AGENTRT_LOG_ERROR("Tcache: tcache_alloc called with NULL tcache");
        }
        return NULL;
    }

    tc->stats.alloc_count++;

    /* 超大对象直接分配 */
    int bin_idx = find_size_class(size);
    if (bin_idx < 0 || (uint32_t)size > tc->config.max_cache_size_class) {
        tc->stats.oversized_alloc++;
        AGENTRT_LOG_TRACE("Tcache: oversized alloc (size=%zu, "
                          "oversized_count=%llu)",
                          size,
                          (unsigned long long)tc->stats.oversized_alloc);
        return AGENTRT_MALLOC(size);
    }

    tcache_bin_t *bin = &tc->bins[bin_idx];

    /* 缓存为空，批量填充 */
    if (bin->count == 0) {
        tc->stats.cache_miss_count++;
        AGENTRT_LOG_DEBUG("Tcache: cache miss (size=%zu → size_class=%u, "
                          "miss_count=%llu, hit_count=%llu)",
                          size, bin->size_class,
                          (unsigned long long)tc->stats.cache_miss_count,
                          (unsigned long long)tc->stats.cache_hit_count);
        if (bin_batch_fill(tc, bin) <= 0) {
            /* 批量填充失败，降级为直接分配 */
            AGENTRT_LOG_WARN("Tcache: batch_fill failed, fallback to MALLOC "
                             "(size=%zu, size_class=%u)",
                             size, bin->size_class);
            return AGENTRT_MALLOC(size);
        }
    } else {
        tc->stats.cache_hit_count++;
    }

    /* 从缓存取出 */
    bin->count--;
    void *ptr = bin->entries[bin->count];
    tc->total_cached_bytes -= bin->size_class;

    AGENTRT_LOG_TRACE("Tcache: alloc hit (size=%zu → class=%u, ptr=%p, "
                      "cache_remaining=%u/%u, cached_bytes=%zu)",
                      size, bin->size_class, ptr,
                      bin->count, bin->max_count,
                      tc->total_cached_bytes);

    return ptr;
}

void tcache_free(tcache_t *tc, void *ptr, size_t size)
{
    if (!tc || !ptr) {
        if (!tc) {
            AGENTRT_LOG_WARN("Tcache: tcache_free called with NULL tcache "
                             "(ptr=%p, size=%zu)", ptr, size);
        }
        return;
    }

    tc->stats.free_count++;

    /* 超大对象直接释放 */
    int bin_idx = find_size_class(size);
    if (bin_idx < 0 || (uint32_t)size > tc->config.max_cache_size_class) {
        AGENTRT_LOG_TRACE("Tcache: oversized free (size=%zu)", size);
        AGENTRT_FREE(ptr);
        return;
    }

    /* 检查缓存总字节数限制 */
    if (tc->config.max_total_cached_bytes > 0 &&
        tc->total_cached_bytes + size > tc->config.max_total_cached_bytes) {
        AGENTRT_LOG_DEBUG("Tcache: cache full, direct free (size=%zu, "
                          "cached=%zu, max=%u)",
                          size, tc->total_cached_bytes,
                          tc->config.max_total_cached_bytes);
        AGENTRT_FREE(ptr);
        return;
    }

    tcache_bin_t *bin = &tc->bins[bin_idx];

    /* 缓存已满，批量归还 */
    if (bin->count >= bin->max_count) {
        AGENTRT_LOG_DEBUG("Tcache: bin full, triggering flush (size_class=%u, "
                          "count=%u/%u)",
                          bin->size_class, bin->count, bin->max_count);
        bin_batch_flush(tc, bin);
    }

    /* 加入缓存 */
    bin->entries[bin->count] = ptr;
    bin->count++;
    tc->total_cached_bytes += bin->size_class;

    AGENTRT_LOG_TRACE("Tcache: free cached (size=%zu → class=%u, ptr=%p, "
                      "cache_count=%u/%u, cached_bytes=%zu)",
                      size, bin->size_class, ptr,
                      bin->count, bin->max_count,
                      tc->total_cached_bytes);
}

/* ==================== 批量操作实现 ==================== */

void tcache_flush(tcache_t *tc)
{
    if (!tc) return;

    size_t flushed_bytes = tc->total_cached_bytes;
    size_t flushed_count = 0;

    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        tcache_bin_t *bin = &tc->bins[i];
        if (bin->count > 0) {
            AGENTRT_LOG_DEBUG("Tcache: flush bin %d (size_class=%u, count=%u)",
                              i, bin->size_class, bin->count);
        }
        flushed_count += bin->count;
        while (bin->count > 0) {
            bin->count--;
            AGENTRT_FREE(bin->entries[bin->count]);
            tc->total_cached_bytes -= bin->size_class;
        }
    }

    tc->stats.batch_flush_count++;

    AGENTRT_LOG_INFO("Tcache: flush complete (flushed=%zu objects, "
                     "%zu bytes)",
                     flushed_count, flushed_bytes);
}

void tcache_purge(tcache_t *tc)
{
    if (!tc) return;

    size_t purged_bytes = tc->total_cached_bytes;
    size_t purged_count = 0;

    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        purged_count += tc->bins[i].count;
        tc->bins[i].count = 0;
    }
    tc->total_cached_bytes = 0;

    AGENTRT_LOG_INFO("Tcache: purge (discarded %zu objects, %zu bytes)",
                     purged_count, purged_bytes);
}

/* ==================== 查询实现 ==================== */

void tcache_get_stats(tcache_t *tc, tcache_stats_t *out_stats)
{
    if (!tc || !out_stats) return;

    *out_stats = tc->stats;

    /* 计算命中率 */
    uint64_t total = tc->stats.cache_hit_count + tc->stats.cache_miss_count;
    if (total > 0) {
        out_stats->hit_rate = (double)tc->stats.cache_hit_count / (double)total;
    }
}

size_t tcache_cached_count(tcache_t *tc)
{
    if (!tc) return 0;

    size_t total = 0;
    for (int i = 0; i < NUM_SIZE_CLASSES; i++) {
        total += tc->bins[i].count;
    }
    return total;
}

size_t tcache_cached_bytes(tcache_t *tc)
{
    return tc ? tc->total_cached_bytes : 0;
}