/**
 * @file memory_debug.h
 * @brief 统一内存管理模块 - 内存调试功能
 * 
 * 提供高级内存调试功能，包括泄漏检测、边界检查、使用分析等。
 * 主要用于开发和测试阶段，帮助发现和修复内存相关错误。
 * 
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEMORY_DEBUG_H
#define AGENTOS_MEMORY_DEBUG_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_debug_api 内存调试API
 * @{
 */

/**
 * @brief 内存调试选项
 */
typedef struct {
    bool enable_leak_check;              /**< 是否启用泄漏检查 */
    bool enable_boundary_check;          /**< 是否启用边界检查 */
    bool enable_use_after_free_check;    /**< 是否启用释放后使用检查 */
    bool enable_double_free_check;       /**< 是否启用双重释放检查 */
    bool enable_invalid_free_check;      /**< 是否启用无效释放检查 */
    bool track_allocations;              /**< 是否跟踪分配信息 */
    bool fill_pattern_on_alloc;          /**< 分配时填充模式 */
    bool fill_pattern_on_free;           /**< 释放时填充模式 */
    unsigned char alloc_fill_pattern;    /**< 分配填充模式值 */
    unsigned char free_fill_pattern;     /**< 释放填充模式值 */
    size_t redzone_size;                 /**< 红区大小（用于边界检查） */
    const char* log_file;                /**< 调试日志文件（NULL表示stderr） */
    int verbosity_level;                 /**< 详细级别（0-3） */
} memory_debug_options_t;

/**
 * @brief 内存泄漏报告
 */
typedef struct {
    size_t leak_count;                   /**< 泄漏数量 */
    size_t total_leaked_bytes;           /**< 总泄漏字节数 */
    struct {
        void* address;                   /**< 泄漏地址 */
        size_t size;                     /**< 泄漏大小 */
        const char* tag;                 /**< 分配标签 */
        const char* file;                /**< 分配位置文件 */
        int line;                        /**< 分配位置行号 */
        const char* function;            /**< 分配位置函数 */
        uint64_t timestamp;              /**< 分配时间戳 */
    } leaks[100];                        /**< 泄漏详细信息（最多100个） */
} memory_leak_report_t;

/**
 * @brief 内存错误类型
 */
typedef enum {
    MEMORY_ERROR_NONE = 0,               /**< 无错误 */
    MEMORY_ERROR_OUT_OF_BOUNDS,          /**< 边界外访问 */
    MEMORY_ERROR_USE_AFTER_FREE,         /**< 释放后使用 */
    MEMORY_ERROR_DOUBLE_FREE,            /**< 双重释放 */
    MEMORY_ERROR_INVALID_FREE,           /**< 无效释放 */
    MEMORY_ERROR_CORRUPTION,             /**< 内存损坏 */
    MEMORY_ERROR_LEAK,                   /**< 内存泄漏 */
    MEMORY_ERROR_ALLOC_FAILED            /**< 分配失败 */
} memory_error_type_t;

/**
 * @brief 内存错误报告
 */
typedef struct {
    memory_error_type_t type;            /**< 错误类型 */
    void* address;                       /**< 相关地址 */
    size_t size;                         /**< 相关大小 */
    const char* description;             /**< 错误描述 */
    const char* file;                    /**< 错误位置文件 */
    int line;                            /**< 错误位置行号 */
    const char* function;                /**< 错误位置函数 */
    uint64_t timestamp;                  /**< 错误时间戳 */
} memory_error_report_t;

/**
 * @brief 内存调试回调函数类型
 */
typedef void (*memory_debug_callback_t)(
    const memory_error_report_t* report,
    void* user_data);

/**
 * @brief 初始化内存调试功能
 * 
 * @param[in] options 调试选项（可为NULL，使用默认选项）
 * @return 成功返回true，失败返回false
 * 
 * @note 需要先启用内存调试（memory_debug_enable）才能使用此功能
 */
bool memory_debug_init(const memory_debug_options_t* options);

/**
 * @brief 启用内存调试
 * 
 * @param[in] enable 是否启用
 * @return 成功返回true，失败返回false
 * 
 * @note 启用调试会增加内存开销和性能开销
 */
bool memory_debug_enable(bool enable);

/**
 * @brief 检查内存调试是否启用
 * 
 * @return 启用返回true，禁用返回false
 */
bool memory_debug_is_enabled(void);

/**
 * @brief 设置内存调试回调函数
 * 
 * @param[in] callback 回调函数
 * @param[in] user_data 用户数据
 */
void memory_debug_set_callback(memory_debug_callback_t callback, void* user_data);

/**
 * @brief 检查内存泄漏
 * 
 * @param[out] report 泄漏报告输出缓冲区（可为NULL）
 * @param[in] dump_to_log 是否将泄漏信息输出到日志
 * @return 泄漏的字节数，0表示无泄漏
 */
size_t memory_debug_check_leaks(memory_leak_report_t* report, bool dump_to_log);

/**
 * @brief 验证内存块完整性
 * 
 * @param[in] ptr 内存指针
 * @param[out] error 错误报告输出缓冲区（可为NULL）
 * @return 内存块完整返回true，损坏返回false
 */
bool memory_debug_validate(void* ptr, memory_error_report_t* error);

/**
 * @brief 验证所有已分配内存块
 * 
 * @param[out] error_count 错误数量输出
 * @param[in] dump_to_log 是否将错误信息输出到日志
 * @return 发现的错误数量
 */
size_t memory_debug_validate_all(size_t* error_count, bool dump_to_log);

/**
 * @brief 转储内存调试信息
 * 
 * @param[in] file 输出文件名（NULL表示使用初始化时设置的log_file）
 * @param[in] include_stack_trace 是否包含堆栈跟踪
 */
void memory_debug_dump_info(const char* file, bool include_stack_trace);

/**
 * @brief 获取内存块分配信息
 * 
 * @param[in] ptr 内存指针
 * @param[out] file 分配位置文件输出（可为NULL）
 * @param[out] line 分配位置行号输出（可为NULL）
 * @param[out] function 分配位置函数输出（可为NULL）
 * @param[out] tag 分配标签输出（可为NULL）
 * @return 成功找到信息返回true，失败返回false
 */
bool memory_debug_get_allocation_info(void* ptr,
    const char** file, int* line, const char** function, const char** tag);

/**
 * @brief 设置内存块标签
 * 
 * @param[in] ptr 内存指针
 * @param[in] tag 新标签（可为NULL）
 * @return 成功返回true，失败返回false
 */
bool memory_debug_set_tag(void* ptr, const char* tag);

/**
 * @brief 启用或禁用特定调试功能
 * 
 * @param[in] feature 功能名称（"leak_check", "boundary_check"等）
 * @param[in] enable 是否启用
 * @return 成功返回true，失败返回false
 */
bool memory_debug_set_feature(const char* feature, bool enable);

/**
 * @brief 获取内存调试统计信息
 * 
 * @param[out] total_allocations 总分配次数输出
 * @param[out] total_frees 总释放次数输出
 * @param[out] current_allocations 当前分配数输出
 * @param[out] error_count 错误数量输出
 * @return 成功返回true，失败返回false
 */
bool memory_debug_get_stats(size_t* total_allocations, size_t* total_frees,
                           size_t* current_allocations, size_t* error_count);

/**
 * @brief 重置内存调试统计信息
 */
void memory_debug_reset_stats(void);

/**
 * @brief 启用堆栈跟踪
 * 
 * @param[in] enable 是否启用
 * @param[in] max_depth 最大堆栈深度（0表示默认）
 * @return 成功返回true，失败返回false
 * 
 * @note 启用堆栈跟踪会显著增加内存和性能开销
 */
bool memory_debug_enable_stack_trace(bool enable, size_t max_depth);

/**
 * @brief 获取堆栈跟踪
 * 
 * @param[in] ptr 内存指针
 * @param[out] frames 堆栈帧输出缓冲区
 * @param[in] max_frames 最大帧数
 * @return 实际获取的帧数
 */
size_t memory_debug_get_stack_trace(void* ptr, void** frames, size_t max_frames);

/**
 * @brief 设置内存调试日志级别
 * 
 * @param[in] level 日志级别（0-3，0=无，1=错误，2=警告，3=详细）
 */
void memory_debug_set_log_level(int level);

/**
 * @brief 记录内存操作
 * 
 * @param[in] operation 操作类型（"alloc", "free", "realloc"等）
 * @param[in] ptr 内存指针
 * @param[in] size 大小
 * @param[in] file 文件名
 * @param[in] line 行号
 * @param[in] function 函数名
 * 
 * @note 主要用于内部使用，也可用于手动记录自定义内存操作
 */
void memory_debug_log_operation(const char* operation, void* ptr, size_t size,
                               const char* file, int line, const char* function);

/**
 * @brief 内存调试检查点
 * 
 * 创建内存状态检查点，可用于比较内存使用变化。
 * 
 * @param[in] name 检查点名称
 * @return 检查点ID，失败返回0
 */
unsigned int memory_debug_checkpoint(const char* name);

/**
 * @brief 比较检查点
 * 
 * @param[in] checkpoint1 第一个检查点ID
 * @param[in] checkpoint2 第二个检查点ID
 * @param[out] diff_report 差异报告输出缓冲区（可为NULL）
 * @return 差异数量，0表示无差异
 */
size_t memory_debug_compare_checkpoints(unsigned int checkpoint1,
                                       unsigned int checkpoint2,
                                       memory_leak_report_t* diff_report);

/**
 * @brief 分配时内存调试宏
 */
#ifdef MEMORY_DEBUG_ENABLED
#define MEMORY_DEBUG_ALLOC(size, tag) \
    memory_debug_log_operation("alloc", NULL, (size), __FILE__, __LINE__, __func__); \
    memory_alloc((size), (tag))

#define MEMORY_DEBUG_CALLOC(size, tag) \
    memory_debug_log_operation("calloc", NULL, (size), __FILE__, __LINE__, __func__); \
    memory_calloc((size), (tag))

#define MEMORY_DEBUG_FREE(ptr) \
    memory_debug_log_operation("free", (ptr), 0, __FILE__, __LINE__, __func__); \
    memory_free((ptr))
#else
#define MEMORY_DEBUG_ALLOC(size, tag) memory_alloc((size), (tag))
#define MEMORY_DEBUG_CALLOC(size, tag) memory_calloc((size), (tag))
#define MEMORY_DEBUG_FREE(ptr) memory_free((ptr))
#endif

/** @} */ // end of memory_debug_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_DEBUG_H */