/**
 * @file memory_debug.c
 * @brief 统一内存管理模块 - 内存调试功能实现
 * 
 * 实现高级内存调试功能，包括泄漏检测、边界检查、使用分析等�? * 与核心内存管理模块紧密集成，提供详细的调试信息�? * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#include "memory_debug.h"
#include "agentos_memory.h"
#include <stdlib.h>

/* Unified base library compatibility layer */
#include <agentos/memory.h>
#include <agentos/string.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#else
#include <pthread.h>
#include <execinfo.h>
#endif

/**
 * @brief 内存调试块头（红区）
 */
typedef struct {
    size_t magic;                         /**< 魔数，用于验�?*/
    size_t size;                          /**< 原始分配大小 */
    size_t redzone_size;                  /**< 红区大小 */
    const char* tag;                      /**< 分配标签 */
    const char* file;                     /**< 分配位置文件 */
    int line;                             /**< 分配位置行号 */
    const char* function;                 /**< 分配位置函数 */
    uint64_t timestamp;                   /**< 分配时间�?*/
    unsigned char redzone_pattern;        /**< 红区填充模式 */
    bool allocated;                       /**< 是否已分�?*/
    void* stack_trace[16];                /**< 堆栈跟踪（如果启用） */
    size_t stack_depth;                   /**< 堆栈深度 */
    struct memory_debug_block* next;      /**< 下一个调试块 */
    struct memory_debug_block* prev;      /**< 前一个调试块 */
} memory_debug_block_t;

/**
 * @brief 内存调试内部状�? */
typedef struct {
    bool initialized;                     /**< 模块是否已初始化 */
    memory_debug_options_t options;       /**< 当前调试选项 */
    
    // 调试块链表
    memory_debug_block_t* block_list_head; /**< 调试块链表头 */
    size_t block_count;                   /**< 调试块数�?*/
    
    // 统计信息
    size_t total_allocations;             /**< 总分配次�?*/
    size_t total_frees;                   /**< 总释放次�?*/
    size_t error_count;                   /**< 错误数量 */
    
    // 线程同步
#ifdef _WIN32
    CRITICAL_SECTION lock;                /**< Windows临界�?*/
#else
    pthread_mutex_t lock;                 /**< POSIX互斥�?*/
#endif
    
    // 回调函数
    memory_debug_callback_t callback;     /**< 错误回调函数 */
    void* callback_user_data;             /**< 回调用户数据 */
    
    // 堆栈跟踪
    bool stack_trace_enabled;             /**< 是否启用堆栈跟踪 */
    size_t max_stack_depth;               /**< 最大堆栈深�?*/
    
    // 检查点管理
    unsigned int next_checkpoint_id;      /**< 下一个检查点ID */
} memory_debug_state_t;

/**
 * @brief 魔数常量
 */
#define MEMORY_DEBUG_MAGIC 0xDEADBEEFCAFEBABEULL

/**
 * @brief 全局调试状�? */
static memory_debug_state_t g_debug_state = {
    .initialized = false,
    .options = {
        .enable_leak_check = true,
        .enable_boundary_check = true,
        .enable_use_after_free_check = true,
        .enable_double_free_check = true,
        .enable_invalid_free_check = true,
        .track_allocations = true,
        .fill_pattern_on_alloc = true,
        .fill_pattern_on_free = true,
        .alloc_fill_pattern = 0xAA,
        .free_fill_pattern = 0xDD,
        .redzone_size = 16,
        .log_file = NULL,
        .verbosity_level = 1
    },
    .block_list_head = NULL,
    .block_count = 0,
    .total_allocations = 0,
    .total_frees = 0,
    .error_count = 0,
#ifdef _WIN32
    .lock = {0},
#else
    .lock = PTHREAD_MUTEX_INITIALIZER,
#endif
    .callback = NULL,
    .callback_user_data = NULL,
    .stack_trace_enabled = false,
    .max_stack_depth = 16,
    .next_checkpoint_id = 1
};

/**
 * @brief 内部锁初始化
 * 
 * @return 成功返回true，失败返回false
 */
static bool memory_debug_lock_init(void) {
#ifdef _WIN32
    InitializeCriticalSection(&g_debug_state.lock);
    return true;
#else
    return pthread_mutex_init(&g_debug_state.lock, NULL) == 0;
#endif
}

/**
 * @brief 内部锁销�? */
static void memory_debug_lock_destroy(void) {
#ifdef _WIN32
    DeleteCriticalSection(&g_debug_state.lock);
#else
    pthread_mutex_destroy(&g_debug_state.lock);
#endif
}

/**
 * @brief 加锁
 */
static void memory_debug_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&g_debug_state.lock);
#else
    pthread_mutex_lock(&g_debug_state.lock);
#endif
}

/**
 * @brief 解锁
 */
static void memory_debug_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&g_debug_state.lock);
#else
    pthread_mutex_unlock(&g_debug_state.lock);
#endif
}

/**
 * @brief 获取当前时间戳（毫秒�? * 
 * @return 时间�? */
static uint64_t memory_debug_get_timestamp(void) {
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
 * @brief 记录错误
 * 
 * @param[in] type 错误类型
 * @param[in] addr 相关地址
 * @param[in] size 相关大小
 * @param[in] description 错误描述
 * @param[in] file 文件�? * @param[in] line 行号
 * @param[in] function 函数�? */
static void memory_debug_record_error(memory_error_type_t type,
                                     void* addr, size_t size,
                                     const char* description,
                                     const char* file, int line,
                                     const char* function) {
    if (!g_debug_state.initialized) {
        return;
    }
    
    memory_error_report_t report = {
        .type = type,
        .address = addr,
        .size = size,
        .description = description,
        .file = file,
        .line = line,
        .function = function,
        .timestamp = memory_debug_get_timestamp()
    };
    
    g_debug_state.error_count++;
    
    // 输出到日志
    if (g_debug_state.options.verbosity_level >= 1) {
        FILE* log = stderr;
        if (g_debug_state.options.log_file != NULL) {
            log = fopen(g_debug_state.options.log_file, "a");
            if (log == NULL) {
                log = stderr;
            }
        }
        
        fprintf(log, "[内存错误] 类型�?d, 地址�?p, 大小�?zu\n",
               type, addr, size);
        if (description != NULL) {
            fprintf(log, "描述�?s\n", description);
        }
        if (file != NULL && function != NULL) {
            fprintf(log, "位置�?s:%d (%s)\n", file, line, function);
        }
        
        if (log != stderr) {
            fclose(log);
        }
    }
    
    // 调用回调函数
    if (g_debug_state.callback != NULL) {
        g_debug_state.callback(&report, g_debug_state.callback_user_data);
    }
}

/**
 * @brief 查找调试�? * 
 * @param[in] user_ptr 用户指针（红区后的地址�? * @return 调试块指针，未找到返回NULL
 */
static memory_debug_block_t* memory_debug_find_block(void* user_ptr) {
    if (user_ptr == NULL) {
        return NULL;
    }
    
    uint8_t* block_ptr = (uint8_t*)user_ptr - g_debug_state.options.redzone_size;
    memory_debug_block_t* block = (memory_debug_block_t*)block_ptr;
    
    // 验证魔数
    if (block->magic != MEMORY_DEBUG_MAGIC) {
        return NULL;
    }
    
    return block;
}

/**
 * @brief 添加调试块到链表
 * 
 * @param[in] block 调试�? */
static void memory_debug_add_block(memory_debug_block_t* block) {
    if (block == NULL) {
        return;
    }
    
    block->next = g_debug_state.block_list_head;
    block->prev = NULL;
    
    if (g_debug_state.block_list_head != NULL) {
        g_debug_state.block_list_head->prev = block;
    }
    
    g_debug_state.block_list_head = block;
    g_debug_state.block_count++;
}

/**
 * @brief 从链表移除调试块
 * 
 * @param[in] block 调试�? */
static void memory_debug_remove_block(memory_debug_block_t* block) {
    if (block == NULL) {
        return;
    }
    
    if (block->prev != NULL) {
        block->prev->next = block->next;
    } else {
        g_debug_state.block_list_head = block->next;
    }
    
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }
    
    block->next = NULL;
    block->prev = NULL;
    g_debug_state.block_count--;
}

/**
 * @brief 验证调试块完整�? * 
 * @param[in] block 调试�? * @param[out] error 错误报告
 * @return 完整返回true，损坏返回false
 */
static bool memory_debug_validate_block(memory_debug_block_t* block,
                                       memory_error_report_t* error) {
    if (block == NULL) {
        if (error != NULL) {
            memory_error_report_t report = {
                .type = MEMORY_ERROR_INVALID_FREE,
                .address = NULL,
                .size = 0,
                .description = "块指针为NULL",
                .file = NULL,
                .line = 0,
                .function = NULL,
                .timestamp = memory_debug_get_timestamp()
            };
            memcpy(error, &report, sizeof(memory_error_report_t));
        }
        return false;
    }
    
    // 检查魔数
    if (block->magic != MEMORY_DEBUG_MAGIC) {
        if (error != NULL) {
            memory_error_report_t report = {
                .type = MEMORY_ERROR_CORRUPTION,
                .address = block,
                .size = 0,
                .description = "魔数不匹配，可能内存损坏",
                .file = NULL,
                .line = 0,
                .function = NULL,
                .timestamp = memory_debug_get_timestamp()
            };
            memcpy(error, &report, sizeof(memory_error_report_t));
        }
        return false;
    }
    
    // 检查红区
    if (g_debug_state.options.enable_boundary_check && 
        g_debug_state.options.redzone_size > 0) {
        uint8_t* redzone_start = (uint8_t*)block + sizeof(memory_debug_block_t);
        size_t redzone_size = g_debug_state.options.redzone_size - sizeof(memory_debug_block_t);
        
        for (size_t i = 0; i < redzone_size; i++) {
            if (redzone_start[i] != block->redzone_pattern) {
                if (error != NULL) {
                    memory_error_report_t report = {
                        .type = MEMORY_ERROR_OUT_OF_BOUNDS,
                        .address = redzone_start + i,
                        .size = 1,
                        .description = "红区损坏，可能边界外访问",
                        .file = block->file,
                        .line = block->line,
                        .function = block->function,
                        .timestamp = memory_debug_get_timestamp()
                    };
                    memcpy(error, &report, sizeof(memory_error_report_t));
                }
                return false;
            }
        }
    }
    
    return true;
}

/**
 * @brief 获取堆栈跟踪
 * 
 * @param[out] frames 堆栈帧输出缓冲区
 * @param[in] max_frames 最大帧�? * @return 实际获取的帧�? */
static size_t memory_debug_capture_stack_trace(void** frames, size_t max_frames) {
    if (!g_debug_state.stack_trace_enabled || max_frames == 0) {
        return 0;
    }
    
#ifdef _WIN32
    return CaptureStackBackTrace(0, (DWORD)max_frames, frames, NULL);
#else
    return backtrace(frames, max_frames);
#endif
}

bool memory_debug_init(const memory_debug_options_t* options) {
    if (g_debug_state.initialized) {
        return true;
    }
    
    // 初始化锁
    if (!memory_debug_lock_init()) {
        return false;
    }
    
    memory_debug_lock();
    
    // 设置选项
    if (options != NULL) {
        memcpy(&g_debug_state.options, options, sizeof(memory_debug_options_t));
    }
    
    // 初始化状态
    g_debug_state.block_list_head = NULL;
    g_debug_state.block_count = 0;
    g_debug_state.total_allocations = 0;
    g_debug_state.total_frees = 0;
    g_debug_state.error_count = 0;
    g_debug_state.callback = NULL;
    g_debug_state.callback_user_data = NULL;
    g_debug_state.stack_trace_enabled = false;
    g_debug_state.max_stack_depth = 16;
    g_debug_state.next_checkpoint_id = 1;
    
    g_debug_state.initialized = true;
    
    memory_debug_unlock();
    
    return true;
}

bool memory_debug_enable(bool enable) {
    // 启用/禁用由memory.c处理，这里只返回状态
    return g_debug_state.initialized;
}

bool memory_debug_is_enabled(void) {
    return g_debug_state.initialized && g_debug_state.options.track_allocations;
}

void memory_debug_set_callback(memory_debug_callback_t callback, void* user_data) {
    memory_debug_lock();
    g_debug_state.callback = callback;
    g_debug_state.callback_user_data = user_data;
    memory_debug_unlock();
}

size_t memory_debug_check_leaks(memory_leak_report_t* report, bool dump_to_log) {
    if (!g_debug_state.initialized) {
        return 0;
    }
    
    memory_debug_lock();
    
    size_t leak_count = 0;
    size_t total_leaked_bytes = 0;
    
    memory_debug_block_t* current = g_debug_state.block_list_head;
    
    // 收集泄漏信息
    while (current != NULL) {
        if (current->allocated) {
            leak_count++;
            total_leaked_bytes += current->size;
            
            // 填充报告
            if (report != NULL && leak_count <= 100) {
                report->leaks[leak_count - 1].address = 
                    (uint8_t*)current + g_debug_state.options.redzone_size;
                report->leaks[leak_count - 1].size = current->size;
                report->leaks[leak_count - 1].tag = current->tag;
                report->leaks[leak_count - 1].file = current->file;
                report->leaks[leak_count - 1].line = current->line;
                report->leaks[leak_count - 1].function = current->function;
                report->leaks[leak_count - 1].timestamp = current->timestamp;
            }
        }
        current = current->next;
    }
    
    // 更新报告
    if (report != NULL) {
        report->leak_count = leak_count;
        report->total_leaked_bytes = total_leaked_bytes;
    }
    
    // 输出到日志
    if (dump_to_log && leak_count > 0) {
        FILE* log = stderr;
        if (g_debug_state.options.log_file != NULL) {
            log = fopen(g_debug_state.options.log_file, "a");
            if (log == NULL) {
                log = stderr;
            }
        }
        
        fprintf(log, "=== 内存泄漏检测报�?===\n");
        fprintf(log, "时间�?llu\n", (unsigned long long)memory_debug_get_timestamp());
        fprintf(log, "泄漏块数�?zu\n", leak_count);
        fprintf(log, "泄漏字节数：%zu\n", total_leaked_bytes);
        
        if (g_debug_state.options.verbosity_level >= 2) {
            fprintf(log, "泄漏详情：\n");
            
            memory_debug_block_t* current = g_debug_state.block_list_head;
            size_t count = 0;
            
            while (current != NULL && count < 20) {
                if (current->allocated) {
                    count++;
                    void* user_ptr = (uint8_t*)current + g_debug_state.options.redzone_size;
                    fprintf(log, "  %p: %zu字节", user_ptr, current->size);
                    
                    if (current->tag != NULL) {
                        fprintf(log, " [%s]", current->tag);
                    }
                    
                    if (current->file != NULL) {
                        fprintf(log, " (%s:%d", current->file, current->line);
                        if (current->function != NULL) {
                            fprintf(log, " in %s", current->function);
                        }
                        fprintf(log, ")");
                    }
                    
                    fprintf(log, "\n");
                }
                current = current->next;
            }
            
            if (count >= 20) {
                fprintf(log, "  ...（更多泄漏，总计%zu个）\n", leak_count);
            }
        }
        
        fprintf(log, "========================\n");
        
        if (log != stderr) {
            fclose(log);
        }
    }
    
    memory_debug_unlock();
    
    return total_leaked_bytes;
}

bool memory_debug_validate(void* ptr, memory_error_report_t* error) {
    if (!g_debug_state.initialized || ptr == NULL) {
        if (error != NULL) {
            memset(error, 0, sizeof(memory_error_report_t));
        }
        return true;
    }
    
    memory_debug_lock();
    
    memory_debug_block_t* block = memory_debug_find_block(ptr);
    bool valid = memory_debug_validate_block(block, error);
    
    if (!valid && error != NULL && error->type == MEMORY_ERROR_NONE) {
        // 块未找到
        memory_error_report_t report = {
            .type = MEMORY_ERROR_INVALID_FREE,
            .address = ptr,
            .size = 0,
            .description = "未找到调试块，可能不是调试分配的内存",
            .file = NULL,
            .line = 0,
            .function = NULL,
            .timestamp = memory_debug_get_timestamp()
        };
        memcpy(error, &report, sizeof(memory_error_report_t));
    }
    
    memory_debug_unlock();
    
    return valid;
}

size_t memory_debug_validate_all(size_t* error_count, bool dump_to_log) {
    if (!g_debug_state.initialized) {
        if (error_count != NULL) {
            *error_count = 0;
        }
        return 0;
    }
    
    memory_debug_lock();
    
    size_t errors_found = 0;
    memory_debug_block_t* current = g_debug_state.block_list_head;
    
    if (dump_to_log && g_debug_state.options.verbosity_level >= 1) {
        FILE* log = stderr;
        if (g_debug_state.options.log_file != NULL) {
            log = fopen(g_debug_state.options.log_file, "a");
            if (log == NULL) {
                log = stderr;
            }
        }
        
        fprintf(log, "=== 内存完整性验�?===\n");
        
        if (log != stderr) {
            fclose(log);
        }
    }
    
    while (current != NULL) {
        if (!memory_debug_validate_block(current, NULL)) {
            errors_found++;
        }
        current = current->next;
    }
    
    if (dump_to_log && g_debug_state.options.verbosity_level >= 1) {
        FILE* log = stderr;
        if (g_debug_state.options.log_file != NULL) {
            log = fopen(g_debug_state.options.log_file, "a");
            if (log == NULL) {
                log = stderr;
            }
        }
        
        fprintf(log, "验证完成，发现错误：%zu个\n", errors_found);
        fprintf(log, "====================\n");
        
        if (log != stderr) {
            fclose(log);
        }
    }
    
    if (error_count != NULL) {
        *error_count = errors_found;
    }
    
    memory_debug_unlock();
    
    return errors_found;
}

void memory_debug_dump_info(const char* file, bool include_stack_trace) {
    if (!g_debug_state.initialized) {
        return;
    }
    
    memory_debug_lock();
    
    const char* output_file = file ? file : g_debug_state.options.log_file;
    FILE* output = output_file ? fopen(output_file, "w") : stderr;
    
    if (output == NULL) {
        memory_debug_unlock();
        return;
    }
    
    fprintf(output, "=== 内存调试信息转储 ===\n");
    fprintf(output, "时间�?llu\n", (unsigned long long)memory_debug_get_timestamp());
    fprintf(output, "总分配次数：%zu\n", g_debug_state.total_allocations);
    fprintf(output, "总释放次数：%zu\n", g_debug_state.total_frees);
    fprintf(output, "当前分配块数�?zu\n", g_debug_state.block_count);
    fprintf(output, "错误数量�?zu\n", g_debug_state.error_count);
    fprintf(output, "调试选项：\n");
    fprintf(output, "  泄漏检查：%s\n", g_debug_state.options.enable_leak_check ? "启用" : "禁用");
    fprintf(output, "  边界检查：%s\n", g_debug_state.options.enable_boundary_check ? "启用" : "禁用");
    fprintf(output, "  红区大小�?zu字节\n", g_debug_state.options.redzone_size);
    fprintf(output, "堆栈跟踪�?s\n", g_debug_state.stack_trace_enabled ? "启用" : "禁用");
    
    fprintf(output, "当前分配块：\n");
    
    memory_debug_block_t* current = g_debug_state.block_list_head;
    size_t count = 0;
    
    while (current != NULL && count < 50) {
        count++;
        void* user_ptr = (uint8_t*)current + g_debug_state.options.redzone_size;
        
        fprintf(output, "�?#%zu:\n", count);
        fprintf(output, "  用户地址�?p\n", user_ptr);
        fprintf(output, "  块地址�?p\n", current);
        fprintf(output, "  大小�?zu字节\n", current->size);
        fprintf(output, "  已分配：%s\n", current->allocated ? "�? : "�?);
        fprintf(output, "  标签�?s\n", current->tag ? current->tag : "(null)");
        
        if (current->file != NULL) {
            fprintf(output, "  位置�?s:%d", current->file, current->line);
            if (current->function != NULL) {
                fprintf(output, " (%s)", current->function);
            }
            fprintf(output, "\n");
        }
        
        fprintf(output, "  时间戳：%llu\n", (unsigned long long)current->timestamp);
        
        if (include_stack_trace && current->stack_depth > 0) {
            fprintf(output, "  堆栈跟踪�?zu帧）：\n", current->stack_depth);
            for (size_t i = 0; i < current->stack_depth && i < 8; i++) {
                fprintf(output, "    [%zu] %p\n", i, current->stack_trace[i]);
            }
            if (current->stack_depth > 8) {
                fprintf(output, "    ...�?zu更多帧）\n", current->stack_depth - 8);
            }
        }
        
        fprintf(output, "\n");
        
        current = current->next;
    }
    
    if (count >= 50) {
        fprintf(output, "...（更多块，总计%zu个）\n", g_debug_state.block_count);
    }
    
    fprintf(output, "========================\n");
    
    if (output != stderr) {
        fclose(output);
    }
    
    memory_debug_unlock();
}

bool memory_debug_get_allocation_info(void* ptr,
                                     const char** file, int* line,
                                     const char** function, const char** tag) {
    if (!g_debug_state.initialized || ptr == NULL) {
        return false;
    }
    
    memory_debug_lock();
    
    memory_debug_block_t* block = memory_debug_find_block(ptr);
    bool found = (block != NULL);
    
    if (found) {
        if (file != NULL) *file = block->file;
        if (line != NULL) *line = block->line;
        if (function != NULL) *function = block->function;
        if (tag != NULL) *tag = block->tag;
    }
    
    memory_debug_unlock();
    
    return found;
}

bool memory_debug_set_tag(void* ptr, const char* tag) {
    if (!g_debug_state.initialized || ptr == NULL) {
        return false;
    }
    
    memory_debug_lock();
    
    memory_debug_block_t* block = memory_debug_find_block(ptr);
    bool success = (block != NULL);
    
    if (success) {
        // 注意：这里不释放旧的tag，因为它是静态字符串或由memory.c管理
        block->tag = tag;
    }
    
    memory_debug_unlock();
    
    return success;
}

bool memory_debug_set_feature(const char* feature, bool enable) {
    if (!g_debug_state.initialized || feature == NULL) {
        return false;
    }
    
    memory_debug_lock();
    
    bool success = false;
    
    if (strcmp(feature, "leak_check") == 0) {
        g_debug_state.options.enable_leak_check = enable;
        success = true;
    } else if (strcmp(feature, "boundary_check") == 0) {
        g_debug_state.options.enable_boundary_check = enable;
        success = true;
    } else if (strcmp(feature, "use_after_free_check") == 0) {
        g_debug_state.options.enable_use_after_free_check = enable;
        success = true;
    } else if (strcmp(feature, "double_free_check") == 0) {
        g_debug_state.options.enable_double_free_check = enable;
        success = true;
    } else if (strcmp(feature, "invalid_free_check") == 0) {
        g_debug_state.options.enable_invalid_free_check = enable;
        success = true;
    } else if (strcmp(feature, "track_allocations") == 0) {
        g_debug_state.options.track_allocations = enable;
        success = true;
    }
    
    memory_debug_unlock();
    
    return success;
}

bool memory_debug_get_stats(size_t* total_allocations, size_t* total_frees,
                           size_t* current_allocations, size_t* error_count) {
    if (!g_debug_state.initialized) {
        return false;
    }
    
    memory_debug_lock();
    
    if (total_allocations != NULL) {
        *total_allocations = g_debug_state.total_allocations;
    }
    
    if (total_frees != NULL) {
        *total_frees = g_debug_state.total_frees;
    }
    
    if (current_allocations != NULL) {
        *current_allocations = g_debug_state.block_count;
    }
    
    if (error_count != NULL) {
        *error_count = g_debug_state.error_count;
    }
    
    memory_debug_unlock();
    
    return true;
}

void memory_debug_reset_stats(void) {
    if (!g_debug_state.initialized) {
        return;
    }
    
    memory_debug_lock();
    
    g_debug_state.total_allocations = 0;
    g_debug_state.total_frees = 0;
    g_debug_state.error_count = 0;
    
    memory_debug_unlock();
}

bool memory_debug_enable_stack_trace(bool enable, size_t max_depth) {
    if (!g_debug_state.initialized) {
        return false;
    }
    
    memory_debug_lock();
    
    g_debug_state.stack_trace_enabled = enable;
    if (max_depth > 0 && max_depth <= 64) {
        g_debug_state.max_stack_depth = max_depth;
    }
    
    memory_debug_unlock();
    
    return true;
}

size_t memory_debug_get_stack_trace(void* ptr, void** frames, size_t max_frames) {
    if (!g_debug_state.initialized || ptr == NULL || frames == NULL || max_frames == 0) {
        return 0;
    }
    
    memory_debug_lock();
    
    memory_debug_block_t* block = memory_debug_find_block(ptr);
    size_t depth = 0;
    
    if (block != NULL && block->stack_depth > 0) {
        depth = (block->stack_depth < max_frames) ? block->stack_depth : max_frames;
        memcpy(frames, block->stack_trace, depth * sizeof(void*));
    }
    
    memory_debug_unlock();
    
    return depth;
}

void memory_debug_set_log_level(int level) {
    if (level < 0) level = 0;
    if (level > 3) level = 3;
    
    memory_debug_lock();
    g_debug_state.options.verbosity_level = level;
    memory_debug_unlock();
}

void memory_debug_log_operation(const char* operation, void* ptr, size_t size,
                               const char* file, int line, const char* function) {
    if (!g_debug_state.initialized || g_debug_state.options.verbosity_level < 3) {
        return;
    }
    
    FILE* log = stderr;
    if (g_debug_state.options.log_file != NULL) {
        log = fopen(g_debug_state.options.log_file, "a");
        if (log == NULL) {
            log = stderr;
        }
    }
    
    fprintf(log, "[内存操作] %s", operation);
    
    if (ptr != NULL) {
        fprintf(log, " %p", ptr);
    }
    
    if (size > 0) {
        fprintf(log, " (%zu字节)", size);
    }
    
    if (file != NULL && function != NULL) {
        fprintf(log, " at %s:%d (%s)", file, line, function);
    }
    
    fprintf(log, "\n");
    
    if (log != stderr) {
        fclose(log);
    }
}

unsigned int memory_debug_checkpoint(const char* name) {
    // 简化实现：返回一个ID，实际不保存检查点
    if (!g_debug_state.initialized) {
        return 0;
    }
    
    memory_debug_lock();
    unsigned int id = g_debug_state.next_checkpoint_id++;
    
    if (name != NULL && g_debug_state.options.verbosity_level >= 2) {
        FILE* log = stderr;
        if (g_debug_state.options.log_file != NULL) {
            log = fopen(g_debug_state.options.log_file, "a");
            if (log == NULL) {
                log = stderr;
            }
        }
        
        fprintf(log, "[检查点] ID=%u, 名称=%s, 块数=%zu\n", 
               id, name, g_debug_state.block_count);
        
        if (log != stderr) {
            fclose(log);
        }
    }
    
    memory_debug_unlock();
    
    return id;
}

size_t memory_debug_compare_checkpoints(unsigned int checkpoint1,
                                       unsigned int checkpoint2,
                                       memory_leak_report_t* diff_report) {
    // 简化实现：检查点功能未完整实现
    if (diff_report != NULL) {
        memset(diff_report, 0, sizeof(memory_leak_report_t));
    }
    
    return 0;
}