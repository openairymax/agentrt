/**
 * @file alloc.c
 * @brief 物理内存分配器（带追踪的 malloc/free 封装）
 * @copyright (c) 2026 SPHARX. All Rights Reserved.
 *
 * @details
 * 本模块实现线程安全的内存分配追踪系统。
 * - 使用原子标志确保初始化线程安全
 * - 使用互斥锁保护统计数据
 * - 支持内存泄漏检测
 * - 跨平台兼容（Windows/Linux/macOS）
 */

#include "mem.h"
#include "task.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../../commons/utils/memory/include/memory_compat.h"
#include "../../../commons/utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
/* Windows 平台原子操作兼容层 */
#include "../../../commons/utils/include/atomic_compat.h"
#else
#include <stdatomic.h>
#endif

/* ==================== 全局状态 ==================== */

/** @brief 总分配字节数 */
static atomic_size_t total_allocated = 0;

/** @brief 当前使用字节数 */
static atomic_size_t used_allocated = 0;

/** @brief 峰值使用字节数 */
static atomic_size_t peak_allocated = 0;

/** @brief 分配记录链表头 */
static agentos_mem_alloc_info_t* alloc_list = NULL;

/** @brief 统计数据互斥锁 */
static agentos_mutex_t* mem_stats_mutex = NULL;

/** @brief 初始化状态标志（原子操作保护） */
static atomic_int mem_initialized = 0;

/** @brief 初始化锁（用于双重检查锁定） */
static agentos_mutex_t* init_lock = NULL;

/* ==================== 内部辅助函数 ==================== */

/**
 * @brief 更新峰值内存使用量
 * @note 必须在持有 mem_stats_mutex 时调用，使用 relaxed 内存顺序
 */
static void update_peak_unlocked(void) {
    size_t current = atomic_load_explicit(&used_allocated, memory_order_relaxed);
    size_t peak = atomic_load_explicit(&peak_allocated, memory_order_relaxed);
    if (current > peak) {
        atomic_store_explicit(&peak_allocated, current, memory_order_relaxed);
    }
}

/**
 * @brief 确保内存子系统已初始化
 * @return 0 成功，1 失败
 * @note 线程安全的延迟初始化，使用内存屏障保证正确性
 */
static int ensure_initialized(void) {
    /* 快速路径：已初始化（使用 acquire 语义） */
    int state = atomic_load_explicit(&mem_initialized, memory_order_acquire);
    if (state == 1) {
        return 0;
    }

    /* 慢速路径：需要初始化 */
    if (state == 0) {
        /* 尝试获取初始化权，使用 0 -> 2 的原子交换 */
        int expected = 0;
        if (atomic_compare_exchange_strong_explicit(&mem_initialized, &expected, 2,
                                                    memory_order_acq_rel,
                                                    memory_order_acquire)) {
            /* 当前线程获得初始化权 */
            init_lock = agentos_mutex_create();
            if (!init_lock) {
                atomic_store_explicit(&mem_initialized, 0, memory_order_release);
                return -1;
            }

            mem_stats_mutex = agentos_mutex_create();
            if (!mem_stats_mutex) {
                agentos_mutex_destroy(init_lock);
                init_lock = NULL;
                atomic_store_explicit(&mem_initialized, 0, memory_order_release);
                return -1;
            }

            /* 初始化完成，发布状态（使用 release 语义） */
            atomic_store_explicit(&mem_initialized, 1, memory_order_release);
            return 0;
        }
    }

    /* 其他线程正在初始化（状态为2），等待完成 */
    while (atomic_load_explicit(&mem_initialized, memory_order_acquire) != 1) {
        /* 自旋等待，让出 CPU */
#ifdef _WIN32
        Sleep(0);
#else
        sched_yield();
#endif
    }
    return 0;
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
    if (atomic_load_explicit(&mem_initialized, memory_order_acquire) != 1) {
        return;
    }

    agentos_mem_check_leaks();

    agentos_mutex_lock(mem_stats_mutex);
    while (alloc_list) {
        agentos_mem_alloc_info_t* info = alloc_list;
        alloc_list = info->next;
        AGENTOS_FREE(info);
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

    atomic_store_explicit(&mem_initialized, 0, memory_order_release);
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
    agentos_mem_alloc_info_t* info = (agentos_mem_alloc_info_t*)AGENTOS_MALLOC(sizeof(agentos_mem_alloc_info_t));
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
            AGENTOS_FREE(curr);
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
    void* ptr = AGENTOS_MALLOC(size);
    if (!ptr) {
        return NULL;
    }

    /* 确保初始化 */
    if (ensure_initialized() != 0) {
        /* 初始化失败，仍然返回内存，但不追踪 */
        return ptr;
    }

    /* 检查 mutex 是否已初始化 */
    if (!mem_stats_mutex) {
        return ptr;
    }

    agentos_mutex_lock(mem_stats_mutex);
    atomic_fetch_add_explicit(&total_allocated, size, memory_order_relaxed);
    atomic_fetch_add_explicit(&used_allocated, size, memory_order_relaxed);
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
    atomic_fetch_add_explicit(&total_allocated, size, memory_order_relaxed);
    atomic_fetch_add_explicit(&used_allocated, size, memory_order_relaxed);
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

    if (atomic_load_explicit(&mem_initialized, memory_order_acquire) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        agentos_mem_alloc_info_t* info = alloc_list;
        while (info) {
            if (info->ptr == ptr) {
                atomic_fetch_sub_explicit(&used_allocated, info->size, memory_order_relaxed);
                remove_alloc_info_unlocked(ptr);
                break;
            }
            info = info->next;
        }
        agentos_mutex_unlock(mem_stats_mutex);
    }
    AGENTOS_FREE(ptr);
}

/**
 * @brief 释放对齐内存
 * @param ptr 内存指针
 */
void agentos_mem_aligned_free(void* ptr) {
    if (!ptr) return;

    if (atomic_load_explicit(&mem_initialized, memory_order_acquire) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        agentos_mem_alloc_info_t* info = alloc_list;
        while (info) {
            if (info->ptr == ptr) {
                atomic_fetch_sub_explicit(&used_allocated, info->size, memory_order_relaxed);
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
    AGENTOS_FREE(ptr);
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
    if (atomic_load_explicit(&mem_initialized, memory_order_acquire) == 1 && mem_stats_mutex) {
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

    void* new_ptr = AGENTOS_REALLOC(ptr, new_size);
    if (new_ptr && atomic_load_explicit(&mem_initialized, memory_order_acquire) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
        atomic_fetch_sub_explicit(&used_allocated, old_size, memory_order_relaxed);
        atomic_fetch_add_explicit(&used_allocated, new_size, memory_order_relaxed);
        atomic_fetch_add_explicit(&total_allocated, new_size, memory_order_relaxed);
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
    if (atomic_load_explicit(&mem_initialized, memory_order_acquire) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
    }
    if (out_total) *out_total = atomic_load_explicit(&total_allocated, memory_order_relaxed);
    if (out_used) *out_used = atomic_load_explicit(&used_allocated, memory_order_relaxed);
    if (out_peak) *out_peak = atomic_load_explicit(&peak_allocated, memory_order_relaxed);
    if (atomic_load_explicit(&mem_initialized, memory_order_acquire) == 1 && mem_stats_mutex) {
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

    if (atomic_load_explicit(&mem_initialized, memory_order_acquire) == 1 && mem_stats_mutex) {
        agentos_mutex_lock(mem_stats_mutex);
    }

    agentos_mem_alloc_info_t* info = alloc_list;
#ifdef AGENTOS_ENABLE_MEMORY_DEBUG
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
#else
    /* 无调试输出，但仍计数 */
    while (info) {
        leak_count++;
        leak_size += info->size;
        info = info->next;
    }
#endif

    if (atomic_load_explicit(&mem_initialized, memory_order_acquire) == 1 && mem_stats_mutex) {
        agentos_mutex_unlock(mem_stats_mutex);
    }

    return leak_count;
}