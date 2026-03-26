/**
 * @file mem.c
 * @brief 轻量级内存管理实现
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "mem.h"
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/mman.h>
#endif

/* ==================== 内存池配置 ==================== */

#define DEFAULT_POOL_SIZE (4 * 1024 * 1024)  /* 4MB 默认内存池大小 */
#define MAX_ALIGNMENT 4096                   /* 最大对齐要求 */

/* ==================== 内存池结构 ==================== */

typedef struct memory_pool {
    size_t total_size;      /* 总内存大小 */
    size_t used_size;       /* 已使用内存大小 */
    size_t peak_size;       /* 峰值内存使用量 */
    void* base_ptr;         /* 内存池基地址 */
    int initialized;        /* 初始化标志 */
} memory_pool_t;

/* ==================== 全局状态 ==================== */

static memory_pool_t g_memory_pool = {0};

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 初始化内存池（如果需要）
 * @param max_size 最大内存大小
 * @return 错误码
 */
static agentos_lite_error_t init_pool_if_needed(size_t max_size) {
    if (g_memory_pool.initialized) {
        return AGENTOS_LITE_SUCCESS;
    }
    
    size_t pool_size = max_size > 0 ? max_size : DEFAULT_POOL_SIZE;
    
    /* 分配内存池 */
#ifdef _WIN32
    g_memory_pool.base_ptr = VirtualAlloc(NULL, pool_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    g_memory_pool.base_ptr = mmap(NULL, pool_size, PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_memory_pool.base_ptr == MAP_FAILED) {
        g_memory_pool.base_ptr = NULL;
    }
#endif
    
    if (!g_memory_pool.base_ptr) {
        /* 回退到标准 malloc */
        g_memory_pool.base_ptr = malloc(pool_size);
        if (!g_memory_pool.base_ptr) {
            return AGENTOS_LITE_ENOMEM;
        }
    }
    
    g_memory_pool.total_size = pool_size;
    g_memory_pool.used_size = 0;
    g_memory_pool.peak_size = 0;
    g_memory_pool.initialized = 1;
    
    return AGENTOS_LITE_SUCCESS;
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化内存子系统
 */
AGENTOS_LITE_API agentos_lite_error_t agentos_lite_mem_init(size_t max_size) {
    agentos_lite_error_t err = init_pool_if_needed(max_size);
    if (err != AGENTOS_LITE_SUCCESS) {
        return err;
    }
    
    /* 重置统计信息 */
    g_memory_pool.used_size = 0;
    g_memory_pool.peak_size = 0;
    
    return AGENTOS_LITE_SUCCESS;
}

/**
 * @brief 清理内存子系统
 */
AGENTOS_LITE_API void agentos_lite_mem_cleanup(void) {
    if (g_memory_pool.initialized && g_memory_pool.base_ptr) {
#ifdef _WIN32
        VirtualFree(g_memory_pool.base_ptr, 0, MEM_RELEASE);
#else
        /* 检查是否是 mmap 分配的 */
        size_t page_size = sysconf(_SC_PAGESIZE);
        if ((size_t)g_memory_pool.base_ptr % page_size == 0) {
            munmap(g_memory_pool.base_ptr, g_memory_pool.total_size);
        } else {
            free(g_memory_pool.base_ptr);
        }
#endif
        g_memory_pool.base_ptr = NULL;
        g_memory_pool.initialized = 0;
        g_memory_pool.total_size = 0;
        g_memory_pool.used_size = 0;
        g_memory_pool.peak_size = 0;
    }
}

/**
 * @brief 分配内存（简单实现，使用 malloc）
 */
AGENTOS_LITE_API void* agentos_lite_mem_alloc(size_t size) {
    if (size == 0) return NULL;
    
    void* ptr = malloc(size);
    if (ptr && g_memory_pool.initialized) {
        /* 更新统计信息（非原子操作，简化实现） */
        g_memory_pool.used_size += size;
        if (g_memory_pool.used_size > g_memory_pool.peak_size) {
            g_memory_pool.peak_size = g_memory_pool.used_size;
        }
    }
    
    return ptr;
}

/**
 * @brief 释放内存
 */
AGENTOS_LITE_API void agentos_lite_mem_free(void* ptr) {
    if (!ptr) return;
    
    /* 注意：无法获取释放的大小，简化实现不减少 used_size */
    free(ptr);
}

/**
 * @brief 对齐内存分配
 */
AGENTOS_LITE_API void* agentos_lite_mem_aligned_alloc(size_t size, size_t alignment) {
    if (size == 0) return NULL;
    
    /* 确保 alignment 是 2 的幂且不超过最大值 */
    if (alignment == 0 || (alignment & (alignment - 1)) != 0 || alignment > MAX_ALIGNMENT) {
        return NULL;
    }
    
#ifdef _WIN32
    void* ptr = _aligned_malloc(size, alignment);
#else
    void* ptr = aligned_alloc(alignment, size);
#endif
    
    if (ptr && g_memory_pool.initialized) {
        /* 更新统计信息 */
        g_memory_pool.used_size += size;
        if (g_memory_pool.used_size > g_memory_pool.peak_size) {
            g_memory_pool.peak_size = g_memory_pool.used_size;
        }
    }
    
    return ptr;
}

/**
 * @brief 释放对齐内存
 */
AGENTOS_LITE_API void agentos_lite_mem_aligned_free(void* ptr) {
    if (!ptr) return;
    
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

/**
 * @brief 获取内存统计信息
 */
AGENTOS_LITE_API void agentos_lite_mem_stats(size_t* total, size_t* used, size_t* peak) {
    if (total) *total = g_memory_pool.total_size;
    if (used) *used = g_memory_pool.used_size;
    if (peak) *peak = g_memory_pool.peak_size;
}

/**
 * @brief 检查内存泄漏（简化实现，总是返回0）
 */
AGENTOS_LITE_API size_t agentos_lite_mem_check_leaks(void) {
    /* 简化实现：不追踪具体分配，总是返回无泄漏 */
    return 0;
}