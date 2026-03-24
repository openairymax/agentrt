/**
 * @file alloc.c
 * @brief 物理内存分配器（带追踪的 malloc/free 封装）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 * 
 * @details
 * 本模块实现线程安全的内存分配追踪系统：
 * - 使用原子标志确保初始化线程安全
 * - 使用互斥锁保护统计数据
 * - 支持内存泄漏检测
 * - 跨平台兼容（Windows/Linux/macOS）
 */

#include "mem.h"
#include "task.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

/* ==================== 全局状态 ==================== */

/** @brief 总分配字节数 */
static _Atomic size_t total_allocated = ATOMIC_VAR_INIT(0);

/** @brief 当前使用字节数 */
static _Atomic size_t used_allocated = ATOMIC_VAR_INIT(0);

/** @brief 峰值使用字节数 */
static _Atomic size_t peak_allocated = ATOMIC_VAR_INIT(0);

/** @brief 分配记录链表头 */
static agentos_mem_alloc_info_t* alloc_list = NULL;

/** @brief 统计数据互斥锁 */
static agentos_mutex_t* mem_stats_mutex = NULL;

/** @brief 初始化状态标志（原子操作保护） */
static _Atomic int mem_initialized = ATOMIC_VAR_INIT(0);

/** @brief 初始化锁（用于双重检查锁定） */
static agentos_mutex_t* init_lock = NULL;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 更新峰值内存使用量
 * @note 必须在持有 mem_stats_mutex 时调用
 */
static void update_peak_unlocked(void) {
    size_t current = atomic_load(&used_allocated);
    size_t peak = atomic_load(&peak_allocated);
    if (current > peak) {
        atomic_store(&peak_allocated, current);
    }
}

/**
 * @brief 确保内存子系统已初始化
 * @return 0 成功，-1 失败
 * @note 线程安全的延迟初始化
 */
static int ensure_initialized(void) {
    int expected = 0;
    
    /* 快速路径：已初始化 */
    if (atomic_load(&mem_initialized) == 1) {
        return 0;
    }
    
    /* 慢速路径：需要初始化 */
    if (atomic_compare_exchange_strong(&mem_initialized, &expected, 2)) {
        /* 当前线程获得初始化权 */
        init_lock = agentos_mutex_create();
        if (!init_lock) {
            atomic_store(&mem_initialized, 0);
            return -1;
        }
        
        mem_stats_mutex = agentos_mutex_create();
        if (!mem_stats_mutex) {
            agentos_mutex_destroy(init_lock);
            init_lock = NULL;
            atomic_store(&mem_initialized, 0);
            return -1;
        }
        
        atomic_store(&mem_initialized, 1);
        return 0;
    } else {
        /* 其他线程正在初始化，等待完成 */
        while (atomic_load(&mem_initialized) != 1) {
            /* 自旋等待，让出 CPU */
#ifdef _WIN32
            Sleep(0);
#else
            sched_yield();
#endif
        }
        return 0;
    }
}

/* ==================== 公共接口实现 ==================== */

/**
 * @brief 初始化内存分配器
 * @param heap_size 堆大小（保留参数，暂未使用）
 * @return AGENTOS_SUCCESS 成功，AGENTOS_ENOMEM 内存不足
 * @note 线程安全，可多次调用
 */
agentos_error_t agentos_mem_init(size_t heap_size) {
    (void)heap_size;
    
    if (ensure_initialized() != 0) {
        return AGENTOS_ENOMEM;
    }
    
    return AGENTOS_SUCCESS;
}

/**
 * @brief 清理内存分配器
 * @note 打印泄漏报告，释放所有资源
 */
void agentos_mem_cleanup(void) {
    /* 确保已初始化 */
    if (atomic_load(&mem_initialized) != 1) {
        return;
    }
    
    agentos_mem_check_leaks();

    agentos_mutex_lock(mem_stats_mutex);
    while (alloc_list) {
        agentos_mem_alloc_info_t* info = alloc_list;
        alloc_list = info->next;
        free(info);
    }
    agentos_mutex_unlock(mem_stats_mutex);

    if (mem_stats_mutex) {
        agentos_mutex_destroy(mem_stats_mutex);
        mem_stats_mutex = NULL;
    }
    
    if (init_lock) {
        agentos_mutex_destroy(init_lock);
        init_lock = NULL;
    }
    
    atomic_store(&mem_initialized, 0);
}

/* ==================== 分配记录管理 ==================== */

/**
 * @brief 添加分配记录
 * @param ptr 内存指针
 * @param size 分配大小
 * @param file 源文件名
 * @param line 源文件行号
 * @note 必须在持有 mem_stats_mutex 时调用
 */
static void add_alloc_info_unlocked(void* ptr, size_t size, const char* file, int line) {
    agentos_mem_alloc_info_t* info = (agentos_mem_alloc_info_t*)malloc(sizeof(agentos_mem_alloc_info_t));
    if (info) {
        info->ptr = ptr;
        info->size = size;
        info->file = file;
        info->line = line;
        info->next = alloc_list;
        alloc_list = info;
    }
}

/**
 * @brief 移除分配记录
 * @param ptr 内存指针
 * @note 必须在持有 mem_stats_mutex 时调用
 */
static void remove_alloc_info_unlocked(void* ptr) {
    if (!ptr) return;
    agentos_mem_alloc_info_t* prev = NULL;
    agentos_mem_alloc_info_t* curr = alloc_list;
    while (curr) {
        if (curr->ptr == ptr) {
            if (prev) {
                prev->next = curr->next;
            } else {
                alloc_list = curr->next;
            }
            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
}

/* ==================== 内存分配接口 ==================== */

/**
 * @brief 分配内存（带调试信息）
 * @param size 分配大小
 * @param file 源文件名
 * @param line 源文件行号
 * @return 内存指针，失败返回 NULL
 */
void* agentos_mem_alloc_ex(size_t size, const char* file, int line) {
    void* ptr = malloc(size);
    if (!ptr) {
        return NULL;
    }
    
    /* 确保初始化 */
    if (ensure_initialized() != 0) {
        /* 初始化失败，仍然返回内存，但不追踪 */
        return ptr;
    }
    
    agentos_mutex_lock(mem_stats_mutex);
    atomic_fetch_add(&total_allocated, size);
    atomic_fetch_add(&used_allocated, size);
    update_peak_unlocked();
    add_alloc_info_unlocked(ptr, size, file, line);
    agentos_mutex_unlock(mem_stats_mutex);
    
    return ptr;
}

/**
 * @brief 分配内存
 * @param size 分配大小
 * @return 内存指针，失败返回 NULL
 */
void* agentos_mem_alloc(size_t size) {
    return agentos_mem_alloc_ex(size, __FILE__, __LINE__);
}

/**
 * @brief 分配对齐内存（带调试信息）
 * @param size 分配大小
 * @param alignment 对齐字节数
 * @param file 源文件名
 * @param line 源文件行号
 * @return 内存指针，失败返回 NULL
 */
void* agentos_mem_aligned_alloc_ex(size_t size, size_t alignment, const char* file, int line) {
    void* ptr = NULL;
#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0) ptr = NULL;
#endif
    if (!ptr) {
        return NULL;
    }
    
    if (ensure_initialized() != 0) {
        return ptr;
    }
    
    agentos_mutex_lock(mem_stats_mutex);
    atomic_fetch_add(&total_allocated, size);
    atomic_fetch_add(&used_allocated, size);
    update_peak_unlocked();
    add_alloc_info_unlocked(ptr, size, file, line);
    agentos_mutex_unlock(mem_stats_mutex);
    
    return ptr;
}

/**
 * @brief 分配对齐内存
 * @param size 分配大小
 * @param alignment 对齐字节数
 * @return 内存指针，失败返回 NULL
 */
void* agentos_mem_aligned_alloc(size_t size, size_t alignment) {
    return agentos_mem_aligned_alloc_ex(size, alignment, __FILE__, __LINE__);
}

/**
 * @brief 释放内存
 * @param ptr 内存指针
 */
void agentos_mem_free(void* ptr) {
    if (!ptr) return;

    if (atomic_load(&mem_initialized) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        agentos_mem_alloc_info_t* info = alloc_list;
        while (info) {
            if (info->ptr == ptr) {
                atomic_fetch_sub(&used_allocated, info->size);
                remove_alloc_info_unlocked(ptr);
                break;
            }
            info = info->next;
        }
        agentos_mutex_unlock(mem_stats_mutex);
    }
    free(ptr);
}

/**
 * @brief 释放对齐内存
 * @param ptr 内存指针
 */
void agentos_mem_aligned_free(void* ptr) {
    if (!ptr) return;

    if (atomic_load(&mem_initialized) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        agentos_mem_alloc_info_t* info = alloc_list;
        while (info) {
            if (info->ptr == ptr) {
                atomic_fetch_sub(&used_allocated, info->size);
                remove_alloc_info_unlocked(ptr);
                break;
            }
            info = info->next;
        }
        agentos_mutex_unlock(mem_stats_mutex);
    }
#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

/**
 * @brief 重新分配内存（带调试信息）
 * @param ptr 原内存指针
 * @param new_size 新大小
 * @param file 源文件名
 * @param line 源文件行号
 * @return 新内存指针，失败返回 NULL
 */
void* agentos_mem_realloc_ex(void* ptr, size_t new_size, const char* file, int line) {
    if (!ptr) return agentos_mem_alloc_ex(new_size, file, line);
    if (new_size == 0) {
        agentos_mem_free(ptr);
        return NULL;
    }

    size_t old_size = 0;
    if (atomic_load(&mem_initialized) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        agentos_mem_alloc_info_t* info = alloc_list;
        while (info) {
            if (info->ptr == ptr) {
                old_size = info->size;
                remove_alloc_info_unlocked(ptr);
                break;
            }
            info = info->next;
        }
        agentos_mutex_unlock(mem_stats_mutex);
    }

    void* new_ptr = realloc(ptr, new_size);
    if (new_ptr && atomic_load(&mem_initialized) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        atomic_fetch_sub(&used_allocated, old_size);
        atomic_fetch_add(&used_allocated, new_size);
        atomic_fetch_add(&total_allocated, new_size);
        update_peak_unlocked();
        add_alloc_info_unlocked(new_ptr, new_size, file, line);
        agentos_mutex_unlock(mem_stats_mutex);
    }
    return new_ptr;
}

/**
 * @brief 重新分配内存
 * @param ptr 原内存指针
 * @param new_size 新大小
 * @return 新内存指针，失败返回 NULL
 */
void* agentos_mem_realloc(void* ptr, size_t new_size) {
    return agentos_mem_realloc_ex(ptr, new_size, __FILE__, __LINE__);
}

/* ==================== 统计与诊断 ==================== */

/**
 * @brief 获取内存统计信息
 * @param out_total 总分配字节数输出
 * @param out_used 当前使用字节数输出
 * @param out_peak 峰值使用字节数输出
 */
void agentos_mem_stats(size_t* out_total, size_t* out_used, size_t* out_peak) {
    if (atomic_load(&mem_initialized) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
    }
    if (out_total) *out_total = atomic_load(&total_allocated);
    if (out_used) *out_used = atomic_load(&used_allocated);
    if (out_peak) *out_peak = atomic_load(&peak_allocated);
    if (atomic_load(&mem_initialized) == 1 && mem_stats_mutex) {
        agentos_mutex_unlock(mem_stats_mutex);
    }
}

/**
 * @brief 检查内存泄漏
 * @return 泄漏的分配数量
 */
size_t agentos_mem_check_leaks(void) {
    size_t leak_count = 0;
    size_t leak_size = 0;

    if (atomic_load(&mem_initialized) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
    }

    agentos_mem_alloc_info_t* info = alloc_list;
    while (info) {
        leak_count++;
        leak_size += info->size;
        printf("Memory leak: %p, size: %zu, file: %s, line: %d\n",
               info->ptr, info->size, info->file, info->line);
        info = info->next;
    }

    if (leak_count > 0) {
        printf("Total memory leaks: %zu allocations, %zu bytes\n", leak_count, leak_size);
    } else {
        printf("No memory leaks detected\n");
    }

    if (atomic_load(&mem_initialized) == 1 && mem_stats_mutex) {
        agentos_mutex_unlock(mem_stats_mutex);
    }

    return leak_count;
}
