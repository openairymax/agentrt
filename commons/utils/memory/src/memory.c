/**
 * @file memory.c
 * @brief 统一内存管理模块 - 核心层实�? * 
 * 实现安全、高效、统一的内存管理功能，支持内存分配、释放、调试和统计�? * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include "../../utils/memory/include/memory_compat.h"
#include "../../utils/string/include/string_compat.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <sys/time.h>
#endif

/**
 * @brief 模块内部状�? */
typedef struct {
    bool initialized;                     /**< 模块是否已初始化 */
    bool debug_enabled;                   /**< 调试功能是否启用 */
    memory_options_t options;             /**< 当前配置选项 */
    
    // 统计信息
    memory_stats_t stats;                 /**< 内存统计信息 */
    
    // 线程同步
#ifdef _WIN32
    CRITICAL_SECTION lock;                /**< Windows临界�?*/
#else
    pthread_mutex_t lock;                 /**< POSIX互斥�?*/
#endif
    
    // 调试信息链表�?    memory_debug_info_t* debug_list_head; /**< 调试信息链表�?*/
    
    // 分配失败回调
    void (*fail_callback)(size_t size, const char* tag, void* user_data);
    void* fail_callback_user_data;
} memory_state_t;

/**
 * @brief 全局模块状态实�? */
static memory_state_t g_state = {
    .initialized = false,
    .debug_enabled = false,
    .options = {
        .alignment = 0,
        .zero_memory = true,
        .tag = NULL,
        .fail_strategy = MEMORY_FAIL_STRATEGY_RETURN_NULL,
        .fail_callback = NULL,
        .fail_callback_user_data = NULL
    },
    .stats = {0},
#ifdef _WIN32
    .lock = {0},
#else
    .lock = PTHREAD_MUTEX_INITIALIZER,
#endif
    .debug_list_head = NULL,
    .fail_callback = NULL,
    .fail_callback_user_data = NULL
};

/**
 * @brief 内部锁初始化
 * 
 * @return 成功返回true，失败返回false
 */
static bool memory_lock_init(void) {
#ifdef _WIN32
    InitializeCriticalSection(&g_state.lock);
    return true;
#else
    return pthread_mutex_init(&g_state.lock, NULL) == 0;
#endif
}

/**
 * @brief 内部锁销�? */
static void memory_lock_destroy(void) {
#ifdef _WIN32
    DeleteCriticalSection(&g_state.lock);
#else
    pthread_mutex_destroy(&g_state.lock);
#endif
}

/**
 * @brief 加锁
 */
static void memory_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_state.lock);
#else
    pthread_mutex_lock(&g_state.lock);
#endif
}

/**
 * @brief 解锁
 */
static void memory_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_state.lock);
#else
    pthread_mutex_unlock(&g_state.lock);
#endif
}

/**
 * @brief 获取当前时间戳（毫秒�? * 
 * @return 时间�? */
static uint64_t memory_get_timestamp(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ts = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return ts / 10000; // 转换为毫�?#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/**
 * @brief 处理内存分配失败
 * 
 * @param[in] size 请求分配的大�? * @param[in] tag 分配标签
 */
static void memory_handle_fail(size_t size, const char* tag) {
    // 调用用户回调
    if (g_state.fail_callback != NULL) {
        g_state.fail_callback(size, tag, g_state.fail_callback_user_data);
    }
    
    // 根据策略处理
    switch (g_state.options.fail_strategy) {
        case MEMORY_FAIL_STRATEGY_ABORT:
            fprintf(stderr, "内存分配失败：size=%zu, tag=%s\n", size, tag ? tag : "(null)");
            abort();
            break;
            
        case MEMORY_FAIL_STRATEGY_RETRY:
            // 简单重试一�?            // 在实际实现中，这里可以实现更复杂的重试逻辑
            break;
            
        case MEMORY_FAIL_STRATEGY_CALLBACK:
            // 回调已经在上面调用过�?            break;
            
        case MEMORY_FAIL_STRATEGY_RETURN_NULL:
        default:
            // 返回NULL，由调用者处�?            break;
    }
}

/**
 * @brief 添加调试信息记录
 * 
 * @param[in] addr 内存地址
 * @param[in] size 分配大小
 * @param[in] tag 分配标签
 * @param[in] file 源文�? * @param[in] line 行号
 * @param[in] function 函数�? */
static void memory_add_debug_info(void* addr, size_t size, const char* tag,
                                  const char* file, int line, const char* function) {
    if (!g_state.debug_enabled || addr == NULL) {
        return;
    }
    
    memory_debug_info_t* info = AGENTOS_MALLOC(sizeof(memory_debug_info_t));
    if (info == NULL) {
        return;
    }
    
    info->address = addr;
    info->size = size;
    info->tag = tag ? AGENTOS_STRDUP(tag) : NULL;
    info->file = file ? AGENTOS_STRDUP(file) : NULL;
    info->line = line;
    info->function = function ? AGENTOS_STRDUP(function) : NULL;
    info->timestamp = memory_get_timestamp();
    info->next = g_state.debug_list_head;
    g_state.debug_list_head = info;
}

/**
 * @brief 移除调试信息记录
 * 
 * @param[in] addr 内存地址
 */
static void memory_remove_debug_info(void* addr) {
    if (!g_state.debug_enabled || addr == NULL) {
        return;
    }
    
    memory_debug_info_t** prev = &g_state.debug_list_head;
    memory_debug_info_t* current = g_state.debug_list_head;
    
    while (current != NULL) {
        if (current->address == addr) {
            *prev = current->next;
            
            if (current->tag) AGENTOS_FREE((void*)current->tag);
            if (current->file) AGENTOS_FREE((void*)current->file);
            if (current->function) AGENTOS_FREE((void*)current->function);
            AGENTOS_FREE(current);
            
            return;
        }
        
        prev = &current->next;
        current = current->next;
    }
}

/**
 * @brief 查找调试信息记录
 * 
 * @param[in] addr 内存地址
 * @return 调试信息指针，未找到返回NULL
 */
static memory_debug_info_t* memory_find_debug_info(void* addr) {
    if (!g_state.debug_enabled || addr == NULL) {
        return NULL;
    }
    
    memory_debug_info_t* current = g_state.debug_list_head;
    while (current != NULL) {
        if (current->address == addr) {
            return current;
        }
        current = current->next;
    }
    
    return NULL;
}

/**
 * @brief 更新统计信息（分配）
 * 
 * @param[in] size 分配大小
 */
static void memory_update_stats_alloc(size_t size) {
    g_state.stats.total_allocated += size;
    g_state.stats.current_allocated += size;
    g_state.stats.allocation_count++;
    
    if (g_state.stats.current_allocated > g_state.stats.peak_allocated) {
        g_state.stats.peak_allocated = g_state.stats.current_allocated;
    }
}

/**
 * @brief 更新统计信息（释放）
 * 
 * @param[in] size 释放大小
 */
static void memory_update_stats_free(size_t size) {
    g_state.stats.total_freed += size;
    g_state.stats.current_allocated -= size;
    g_state.stats.free_count++;
}

/**
 * @brief 实际内存分配函数（内部使用）
 * 
 * @param[in] size 分配大小
 * @param[in] tag 分配标签
 * @param[in] zero 是否清零
 * @param[in] alignment 对齐要求
 * @return 分配的内存指�? */
static void* memory_allocate_internal(size_t size, const char* tag, bool zero, size_t alignment) {
    if (size == 0) {
        return NULL;
    }
    
    void* ptr = NULL;
    
    if (alignment > 0) {
        // 对齐分配
#ifdef _WIN32
        ptr = _aligned_malloc(size, alignment);
#else
        // POSIX系统使用posix_memalign
        if (posix_memalign(&ptr, alignment, size) != 0) {
            ptr = NULL;
        }
#endif
    } else {
        // 普通分�?        ptr = AGENTOS_MALLOC(size);
    }
    
    if (ptr == NULL) {
        memory_handle_fail(size, tag);
        return NULL;
    }
    
    // 清零内存
    if (zero || g_state.options.zero_memory) {
        memset(ptr, 0, size);
    }
    
    // 更新统计信息
    memory_update_stats_alloc(size);
    
    // 记录调试信息
    if (g_state.debug_enabled) {
        memory_add_debug_info(ptr, size, tag, __FILE__, __LINE__, __func__);
    }
    
    return ptr;
}

bool memory_init(const memory_options_t* options) {
    if (g_state.initialized) {
        return true;
    }
    
    // 初始化锁
    if (!memory_lock_init()) {
        return false;
    }
    
    memory_lock();
    
    // 设置选项
    if (options != NULL) {
        memcpy(&g_state.options, options, sizeof(memory_options_t));
    }
    
    // 初始化统计信�?    memset(&g_state.stats, 0, sizeof(memory_stats_t));
    
    g_state.initialized = true;
    g_state.debug_enabled = false;
    g_state.debug_list_head = NULL;
    g_state.fail_callback = NULL;
    g_state.fail_callback_user_data = NULL;
    
    memory_unlock();
    
    return true;
}

void memory_cleanup(void) {
    if (!g_state.initialized) {
        return;
    }
    
    memory_lock();
    
    // 检查内存泄�?    if (g_state.debug_enabled && g_state.debug_list_head != NULL) {
        fprintf(stderr, "警告：内存清理时发现未释放的内存块\n");
        
        memory_debug_info_t* current = g_state.debug_list_head;
        size_t leak_count = 0;
        size_t leak_size = 0;
        
        while (current != NULL) {
            leak_count++;
            leak_size += current->size;
            fprintf(stderr, "泄漏�?p (%zu字节) - 标签�?s\n", 
                   current->address, current->size, 
                   current->tag ? current->tag : "(null)");
            
            // 释放泄漏的内存（可选）
            // AGENTOS_FREE(current->address);
            
            memory_debug_info_t* next = current->next;
            
            if (current->tag) AGENTOS_FREE((void*)current->tag);
            if (current->file) AGENTOS_FREE((void*)current->file);
            if (current->function) AGENTOS_FREE((void*)current->function);
            AGENTOS_FREE(current);
            
            current = next;
        }
        
        fprintf(stderr, "总计泄漏�?zu个块�?zu字节\n", leak_count, leak_size);
        g_state.debug_list_head = NULL;
    }
    
    g_state.initialized = false;
    
    memory_unlock();
    
    // 销毁锁
    memory_lock_destroy();
}

void* memory_alloc(size_t size, const char* tag) {
    if (!g_state.initialized) {
        // 如果模块未初始化，使用系统默认分�?        void* ptr = AGENTOS_MALLOC(size);
        if (ptr != NULL && g_state.options.zero_memory) {
            memset(ptr, 0, size);
        }
        return ptr;
    }
    
    memory_lock();
    void* ptr = memory_allocate_internal(size, tag, false, 0);
    memory_unlock();
    
    return ptr;
}

void* memory_calloc(size_t size, const char* tag) {
    if (!g_state.initialized) {
        // 如果模块未初始化，使用系统默认分�?        return AGENTOS_CALLOC(1, size);
    }
    
    memory_lock();
    void* ptr = memory_allocate_internal(size, tag, true, 0);
    memory_unlock();
    
    return ptr;
}

void* memory_aligned_alloc(size_t alignment, size_t size, const char* tag) {
    if (!g_state.initialized) {
        // 如果模块未初始化，使用系统默认分�?#ifdef _WIN32
        return _aligned_malloc(size, alignment);
#else
        void* ptr = NULL;
        if (posix_memalign(&ptr, alignment, size) != 0) {
            return NULL;
        }
        if (ptr != NULL && g_state.options.zero_memory) {
            memset(ptr, 0, size);
        }
        return ptr;
#endif
    }
    
    memory_lock();
    void* ptr = memory_allocate_internal(size, tag, g_state.options.zero_memory, alignment);
    memory_unlock();
    
    return ptr;
}

void* memory_realloc(void* ptr, size_t new_size, const char* tag) {
    if (ptr == NULL) {
        return memory_alloc(new_size, tag);
    }
    
    if (new_size == 0) {
        memory_free(ptr);
        return NULL;
    }
    
    if (!g_state.initialized) {
        // 如果模块未初始化，使用系统默认重分配
        return AGENTOS_REALLOC(ptr, new_size);
    }
    
    memory_lock();
    
    // 查找原始分配信息
    memory_debug_info_t* debug_info = memory_find_debug_info(ptr);
    size_t old_size = debug_info ? debug_info->size : 0;
    
    // 使用系统realloc
    void* new_ptr = AGENTOS_REALLOC(ptr, new_size);
    if (new_ptr == NULL) {
        memory_handle_fail(new_size, tag);
        memory_unlock();
        return NULL;
    }
    
    // 更新统计信息
    if (new_ptr != ptr) {
        // 释放旧指针，分配新指�?        if (old_size > 0) {
            memory_update_stats_free(old_size);
        }
        memory_update_stats_alloc(new_size);
        
        // 更新调试信息
        if (g_state.debug_enabled) {
            memory_remove_debug_info(ptr);
            memory_add_debug_info(new_ptr, new_size, tag, __FILE__, __LINE__, __func__);
        }
    } else {
        // 同一地址，大小可能改�?        if (new_size > old_size) {
            memory_update_stats_alloc(new_size - old_size);
            
            // 更新调试信息大小
            if (debug_info != NULL) {
                debug_info->size = new_size;
            }
        } else if (new_size < old_size) {
            memory_update_stats_free(old_size - new_size);
            
            // 更新调试信息大小
            if (debug_info != NULL) {
                debug_info->size = new_size;
            }
        }
    }
    
    memory_unlock();
    
    return new_ptr;
}

void memory_free(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    
    if (!g_state.initialized) {
        // 如果模块未初始化，使用系统默认释�?        AGENTOS_FREE(ptr);
        return;
    }
    
    memory_lock();
    
    // 查找分配信息
    memory_debug_info_t* debug_info = memory_find_debug_info(ptr);
    size_t size = debug_info ? debug_info->size : 0;
    
    // 释放内存
    if (debug_info && debug_info->address == ptr) {
        // 对齐分配
#ifdef _WIN32
        _aligned_free(ptr);
#else
        AGENTOS_FREE(ptr);
#endif
    } else {
        // 普通分�?        AGENTOS_FREE(ptr);
    }
    
    // 更新统计信息
    if (size > 0) {
        memory_update_stats_free(size);
    }
    
    // 移除调试信息
    if (g_state.debug_enabled) {
        memory_remove_debug_info(ptr);
    }
    
    memory_unlock();
}

bool memory_get_stats(memory_stats_t* stats) {
    if (stats == NULL) {
        return false;
    }
    
    if (!g_state.initialized) {
        memset(stats, 0, sizeof(memory_stats_t));
        return true;
    }
    
    memory_lock();
    memcpy(stats, &g_state.stats, sizeof(memory_stats_t));
    
    // 计算泄漏次数
    if (g_state.debug_enabled) {
        memory_debug_info_t* current = g_state.debug_list_head;
        size_t leak_count = 0;
        while (current != NULL) {
            leak_count++;
            current = current->next;
        }
        stats->leak_count = leak_count;
    }
    
    memory_unlock();
    
    return true;
}

void memory_reset_stats(void) {
    if (!g_state.initialized) {
        return;
    }
    
    memory_lock();
    memset(&g_state.stats, 0, sizeof(memory_stats_t));
    memory_unlock();
}

bool memory_debug_enable(bool enable) {
    if (!g_state.initialized) {
        return false;
    }
    
    memory_lock();
    
    if (enable == g_state.debug_enabled) {
        memory_unlock();
        return true;
    }
    
    g_state.debug_enabled = enable;
    
    // 如果禁用调试，清理现有调试信�?    if (!enable && g_state.debug_list_head != NULL) {
        memory_debug_info_t* current = g_state.debug_list_head;
        while (current != NULL) {
            memory_debug_info_t* next = current->next;
            
            if (current->tag) AGENTOS_FREE((void*)current->tag);
            if (current->file) AGENTOS_FREE((void*)current->file);
            if (current->function) AGENTOS_FREE((void*)current->function);
            AGENTOS_FREE(current);
            
            current = next;
        }
        g_state.debug_list_head = NULL;
    }
    
    memory_unlock();
    
    return true;
}

size_t memory_check_leaks(bool dump_to_stderr) {
    if (!g_state.initialized || !g_state.debug_enabled) {
        return 0;
    }
    
    memory_lock();
    
    size_t leak_size = 0;
    memory_debug_info_t* current = g_state.debug_list_head;
    
    if (dump_to_stderr && current != NULL) {
        fprintf(stderr, "=== 内存泄漏检测报�?===\n");
        fprintf(stderr, "时间�?llu\n", (unsigned long long)memory_get_timestamp());
        fprintf(stderr, "当前分配�?zu字节\n", g_state.stats.current_allocated);
        fprintf(stderr, "泄漏块数：\n");
    }
    
    while (current != NULL) {
        leak_size += current->size;
        
        if (dump_to_stderr) {
            fprintf(stderr, "  %p: %zu字节", current->address, current->size);
            if (current->tag) {
                fprintf(stderr, " [%s]", current->tag);
            }
            if (current->file) {
                fprintf(stderr, " (%s:%d)", current->file, current->line);
            }
            fprintf(stderr, "\n");
        }
        
        current = current->next;
    }
    
    if (dump_to_stderr && leak_size > 0) {
        fprintf(stderr, "总计泄漏�?zu字节\n", leak_size);
        fprintf(stderr, "========================\n");
    }
    
    memory_unlock();
    
    return leak_size;
}

void memory_dump_debug_info(const char* file) {
    if (!g_state.initialized || !g_state.debug_enabled) {
        return;
    }
    
    memory_lock();
    
    FILE* output = file ? fopen(file, "w") : stderr;
    if (output == NULL) {
        memory_unlock();
        return;
    }
    
    fprintf(output, "=== 内存调试信息转储 ===\n");
    fprintf(output, "时间�?llu\n", (unsigned long long)memory_get_timestamp());
    fprintf(output, "当前分配块数：\n");
    
    memory_debug_info_t* current = g_state.debug_list_head;
    size_t count = 0;
    
    while (current != NULL) {
        count++;
        fprintf(output, "�?#%zu:\n", count);
        fprintf(output, "  地址�?p\n", current->address);
        fprintf(output, "  大小�?zu字节\n", current->size);
        fprintf(output, "  标签�?s\n", current->tag ? current->tag : "(null)");
        fprintf(output, "  位置�?s:%d (%s)\n", 
               current->file ? current->file : "(unknown)", 
               current->line,
               current->function ? current->function : "(unknown)");
        fprintf(output, "  时间�?llu\n", (unsigned long long)current->timestamp);
        fprintf(output, "\n");
        
        current = current->next;
    }
    
    fprintf(output, "总计�?zu个内存块\n", count);
    fprintf(output, "=======================\n");
    
    if (file) {
        fclose(output);
    }
    
    memory_unlock();
}

bool memory_validate(void* ptr) {
    if (!g_state.initialized || !g_state.debug_enabled || ptr == NULL) {
        return true;
    }
    
    memory_lock();
    
    bool valid = (memory_find_debug_info(ptr) != NULL);
    
    memory_unlock();
    
    return valid;
}

void memory_set_fail_callback(
    void (*callback)(size_t size, const char* tag, void* user_data),
    void* user_data) {
    
    if (!g_state.initialized) {
        return;
    }
    
    memory_lock();
    
    g_state.fail_callback = callback;
    g_state.fail_callback_user_data = user_data;
    
    memory_unlock();
}

size_t memory_get_current_usage(void) {
    if (!g_state.initialized) {
        return 0;
    }
    
    memory_lock();
    size_t usage = g_state.stats.current_allocated;
    memory_unlock();
    
    return usage;
}

size_t memory_get_peak_usage(void) {
    if (!g_state.initialized) {
        return 0;
    }
    
    memory_lock();
    size_t peak = g_state.stats.peak_allocated;
    memory_unlock();
    
    return peak;
}