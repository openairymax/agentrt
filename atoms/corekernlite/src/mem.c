/**
 * @file mem.c
 * @brief AgentOS Lite 内存管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * 提供轻量级的内存管理功能：
 * - 使用系统原生内存分配器
 * - 内存统计
 * - 线程安全
 */

#include "../include/mem.h"
#include "../include/task.h"
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <malloc.h>
#else
    #include <stdalign.h>
#endif

typedef struct {
    size_t total_allocated;
    size_t current_used;
    size_t peak_used;
    size_t alloc_count;
    size_t free_count;
    agentos_lite_mutex_t* lock;
    int initialized;
} mem_stats_t;

static mem_stats_t g_mem_stats = {0, 0, 0, 0, 0, NULL, 0};

AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mem_init(size_t heap_size) {
    (void)heap_size;
    
    if (g_mem_stats.initialized) {
        return AGENTOS_LITE_SUCCESS;
    }
    
    memset(&g_mem_stats, 0, sizeof(g_mem_stats));
    g_mem_stats.lock = agentos_lite_mutex_create();
    if (!g_mem_stats.lock) {
        return AGENTOS_LITE_ENOMEM;
    }
    
    g_mem_stats.initialized = 1;
    return AGENTOS_LITE_SUCCESS;
}

AGENTOS_LITE_API void agentos_lite_mem_cleanup(void) {
    if (!g_mem_stats.initialized) {
        return;
    }
    
    if (g_mem_stats.lock) {
        agentos_lite_mutex_destroy(g_mem_stats.lock);
        g_mem_stats.lock = NULL;
    }
    
    memset(&g_mem_stats, 0, sizeof(g_mem_stats));
}

AGENTOS_LITE_API void* agentos_lite_mem_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    void* ptr = malloc(size);
    if (ptr && g_mem_stats.initialized && g_mem_stats.lock) {
        agentos_lite_mutex_lock(g_mem_stats.lock);
        g_mem_stats.current_used += size;
        g_mem_stats.alloc_count++;
        if (g_mem_stats.current_used > g_mem_stats.peak_used) {
            g_mem_stats.peak_used = g_mem_stats.current_used;
        }
        agentos_lite_mutex_unlock(g_mem_stats.lock);
    }
    
    return ptr;
}

AGENTOS_LITE_API void* agentos_lite_mem_aligned_alloc(size_t size, size_t alignment) {
    if (size == 0) {
        return NULL;
    }
    
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL;
    }
    
    void* ptr = NULL;
    
#if defined(_WIN32) || defined(_WIN64)
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0) {
        ptr = NULL;
    }
#endif
    
    if (ptr && g_mem_stats.initialized && g_mem_stats.lock) {
        agentos_lite_mutex_lock(g_mem_stats.lock);
        g_mem_stats.current_used += size;
        g_mem_stats.alloc_count++;
        if (g_mem_stats.current_used > g_mem_stats.peak_used) {
            g_mem_stats.peak_used = g_mem_stats.current_used;
        }
        agentos_lite_mutex_unlock(g_mem_stats.lock);
    }
    
    return ptr;
}

AGENTOS_LITE_API void agentos_lite_mem_free(void* ptr) {
    if (!ptr) {
        return;
    }
    
    free(ptr);
    
    if (g_mem_stats.initialized && g_mem_stats.lock) {
        agentos_lite_mutex_lock(g_mem_stats.lock);
        g_mem_stats.free_count++;
        agentos_lite_mutex_unlock(g_mem_stats.lock);
    }
}

AGENTOS_LITE_API void agentos_lite_mem_aligned_free(void* ptr) {
    if (!ptr) {
        return;
    }
    
#if defined(_WIN32) || defined(_WIN64)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
    
    if (g_mem_stats.initialized && g_mem_stats.lock) {
        agentos_lite_mutex_lock(g_mem_stats.lock);
        g_mem_stats.free_count++;
        agentos_lite_mutex_unlock(g_mem_stats.lock);
    }
}

AGENTOS_LITE_API void* agentos_lite_mem_realloc(void* ptr, size_t new_size) {
    void* new_ptr = realloc(ptr, new_size);
    
    if (new_ptr && g_mem_stats.initialized && g_mem_stats.lock) {
        agentos_lite_mutex_lock(g_mem_stats.lock);
        if (ptr) {
            g_mem_stats.free_count++;
        }
        g_mem_stats.current_used += new_size;
        g_mem_stats.alloc_count++;
        if (g_mem_stats.current_used > g_mem_stats.peak_used) {
            g_mem_stats.peak_used = g_mem_stats.current_used;
        }
        agentos_lite_mutex_unlock(g_mem_stats.lock);
    }
    
    return new_ptr;
}

AGENTOS_LITE_API void agentos_lite_mem_stats(size_t* out_total, size_t* out_used, size_t* out_peak) {
    if (!g_mem_stats.initialized) {
        if (out_total) *out_total = 0;
        if (out_used) *out_used = 0;
        if (out_peak) *out_peak = 0;
        return;
    }
    
    if (g_mem_stats.lock) {
        agentos_lite_mutex_lock(g_mem_stats.lock);
    }
    
    if (out_total) {
        *out_total = g_mem_stats.total_allocated;
    }
    if (out_used) {
        *out_used = g_mem_stats.current_used;
    }
    if (out_peak) {
        *out_peak = g_mem_stats.peak_used;
    }
    
    if (g_mem_stats.lock) {
        agentos_lite_mutex_unlock(g_mem_stats.lock);
    }
}

AGENTOS_LITE_API size_t agentos_lite_mem_check_leaks(void) {
    if (!g_mem_stats.initialized) {
        return 0;
    }
    
    if (g_mem_stats.lock) {
        agentos_lite_mutex_lock(g_mem_stats.lock);
    }
    
    size_t leaks = g_mem_stats.alloc_count - g_mem_stats.free_count;
    
    if (g_mem_stats.lock) {
        agentos_lite_mutex_unlock(g_mem_stats.lock);
    }
    
    return leaks;
}
