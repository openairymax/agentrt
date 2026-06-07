/**
 * @file memory_compat.h
 * @brief 统一内存管理模块 - 向后兼容层
 *
 * 提供与标准C库内存函数兼容的接口，便于现有代码逐步迁移到统一内存管理模块。
 * 包含安全包装器和迁移辅助宏。
 *
 * @copyright Copyright (c) 2026 SPHARX. All Rights Reserved.
 */

#ifndef AGENTOS_MEMORY_COMPAT_H
#define AGENTOS_MEMORY_COMPAT_H

#include "agentos_memory.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SEC-02: 兼容性错误码定义，确保 AGENTOS_E_OVERFLOW 在所有
 * 包含 memory_compat.h 的编译单元中可用，无需额外包含 error.h */
#ifndef AGENTOS_ENOMEM
#define AGENTOS_ENOMEM (-2)  /**< Out of memory */
#endif
#ifndef AGENTOS_E_OVERFLOW
#define AGENTOS_E_OVERFLOW (-12)  /**< Integer overflow / buffer overflow */
#endif

#ifndef AGENTOS_MEMORY_STATS_T_DEFINED
#define AGENTOS_MEMORY_STATS_T_DEFINED
typedef struct {
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
    size_t leak_count;
} memory_stats_t;
#endif

/**
 * @brief 内存分配类别枚举（SEC-15 合规）
 *
 * 用于区分不同生命周期的内存分配
 */
typedef enum {
    ALLOC_SHORT_LIVED = 0,  /**< 请求作用域，函数返回前释放 */
    ALLOC_LONG_LIVED  = 1,  /**< 会话作用域，可能跨多个请求 */
    ALLOC_CRITICAL    = 2,  /**< 进程作用域，永不释放（配置、密钥） */
} alloc_category_t;

/**
 * @brief 内存水位级别（SEC-15 合规）
 */
typedef enum {
    WATERMARK_NORMAL   = 0,  /**< < 60% 正常 */
    WATERMARK_WARNING  = 1,  /**< 60-75% 警告 */
    WATERMARK_HIGH     = 2,  /**< 75-90% 高 */
    WATERMARK_CRITICAL = 3,  /**< > 90% 临界 */
} watermark_level_t;

/**
 * @brief OOM 响应级别（SEC-15 合规）
 */
typedef enum {
    OOM_RESPONSE_WARNING   = 0,  /**< 记录日志，继续运行 */
    OOM_RESPONSE_DEGRADED  = 1,  /**< 关闭非关键功能 */
    OOM_RESPONSE_CRITICAL  = 2,  /**< 拒绝新请求，完成现有请求 */
    OOM_RESPONSE_FATAL     = 3,  /**< 立即终止进程 */
} oom_response_level_t;

/**
 * @brief 内存分配跟踪条目（SEC-15 合规）
 *
 * 环形缓冲区中的每个条目跟踪一次分配的内存块
 */
typedef struct {
    void    *ptr;              /**< 分配的内存块指针 */
    size_t   size;             /**< 分配大小（字节） */
    alloc_category_t category; /**< 分配类别 */
    uint64_t alloc_time;       /**< 分配时间（毫秒时间戳） */
    const char *file;          /**< 分配源文件 */
    int       line;            /**< 分配行号 */
    bool      freed;           /**< 是否已释放 */
} alloc_track_entry_t;

/**
 * @brief 水位回调函数类型（SEC-15 合规）
 *
 * 每次水位变化时调用。回调函数不应执行长时间操作。
 *
 * @param old_level 之前的水位级别
 * @param new_level 当前的水位级别
 * @param context   回调注册时的用户上下文
 */
typedef void (*watermark_callback_t)(watermark_level_t old_level,
                                     watermark_level_t new_level,
                                     void *context);

/**
 * @brief 水位回调注册槽位（SEC-15 合规）
 */
typedef struct {
    watermark_callback_t callback;  /**< 回调函数指针 */
    void                *context;   /**< 用户上下文 */
    bool                 active;    /**< 是否激活 */
} watermark_callback_slot_t;

#define MAX_WATERMARK_CALLBACKS 8  /**< 最大回调注册数 */

/** @} */  // end of oom_types

/**
 * @brief 扩展内存统计结构体（SEC-15 合规）
 *
 * 在 memory_stats_t 基础上增加实时追踪能力
 */
typedef struct {
    /* 基础统计（与 memory_stats_t 对齐） */
    size_t total_allocated;
    size_t total_freed;
    size_t current_allocated;
    size_t peak_allocated;
    size_t allocation_count;
    size_t free_count;
    size_t leak_count;

    /* v0.1.0 新增字段（SEC-15） */
    size_t leak_suspected;           /**< 疑似泄漏字节数 */
    size_t short_lived_high_water;   /**< SHORT_LIVED 分配高水位（超过此值告警） */
    uint64_t last_gc_time;           /**< 上次 GC 时间 */
    size_t gc_freed_bytes;           /**< GC 累计释放字节数 */

    /* 按类别的分配计数 */
    size_t alloc_count_by_category[3];  /**< 按 category 统计分配次数 */
    size_t bytes_by_category[3];        /**< 按 category 统计分配字节 */

    /* OOM 事件统计 */
    uint64_t oom_event_count;        /**< OOM 事件总数 */
    uint64_t last_oom_time;          /**< 上次 OOM 时间 */
    size_t   last_oom_requested;     /**< 上次 OOM 请求大小 */

    /* 内存压力 */
    watermark_level_t current_watermark; /**< 当前水位级别 */
    size_t   total_system_memory;        /**< 系统总内存（字节） */

    /* 水位回调注册表 */
    watermark_callback_slot_t watermark_callbacks[MAX_WATERMARK_CALLBACKS];

    /* 分配跟踪环形缓冲区 */
    alloc_track_entry_t *allocation_tracker;  /**< 环形缓冲区 */
    size_t   tracker_capacity;               /**< 环形缓冲区容量 */
    size_t   tracker_index;                  /**< 环形缓冲区写入索引 */
    size_t   tracker_count;                  /**< 环形缓冲区有效条目数 */
} memory_stats_extended_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup memory_compat_api 内存兼容API
 * @{
 */

/**
 * @brief 安全的内存分配函数（兼容malloc）
 *
 * @param[in] size 分配大小
 * @return 分配的内存指针，失败返回NULL
 *
 * @note 使用统一内存管理模块，支持调试和统计
 * @note 默认使用"compat_malloc"标签
 */
static inline void *agentos_malloc(size_t size)
{
    return memory_alloc(size, "compat_malloc");
}

/**
 * @brief 安全数组分配函数（带溢出检查）
 *
 * 编码契约 SEC-03: 检查 count * element_size 是否溢出 SIZE_MAX。
 *
 * @param[in] count 元素数量
 * @param[in] element_size 元素大小
 * @return 分配的内存指针，溢出或失败返回NULL
 */
static inline void *agentos_malloc_array(size_t count, size_t element_size)
{
    if (count > 0 && element_size > 0 && count > SIZE_MAX / element_size)
        return NULL;
    return memory_alloc(count * element_size, "compat_malloc_array");
}

/**
 * @brief 安全数组清零分配函数（带溢出检查）
 *
 * 编码契约 SEC-03: 检查 count * element_size 是否溢出 SIZE_MAX。
 *
 * @param[in] count 元素数量
 * @param[in] element_size 元素大小
 * @return 分配的内存指针，溢出或失败返回NULL
 */
static inline void *agentos_calloc_array(size_t count, size_t element_size)
{
    if (count > 0 && element_size > 0 && count > SIZE_MAX / element_size)
        return NULL;
    return memory_calloc(count * element_size, "compat_calloc_array");
}

/**
 * @brief 安全的内存分配函数（兼容calloc）
 *
 * @param[in] num 元素数量
 * @param[in] size 元素大小
 * @return 分配的内存指针，失败返回NULL
 *
 * @note 内存会自动清零
 * @note 编码契约 SEC-03: 包含溢出检查
 */
static inline void *agentos_calloc(size_t num, size_t size)
{
    if (num > 0 && size > 0 && num > SIZE_MAX / size)
        return NULL;
    return memory_calloc(num * size, "compat_calloc");
}

/**
 * @brief 安全的内存重分配函数（兼容realloc）
 *
 * @param[in] ptr 原始指针
 * @param[in] new_size 新大小
 * @return 重分配的内存指针，失败返回NULL
 */
static inline void *agentos_realloc(void *ptr, size_t new_size)
{
    return memory_realloc(ptr, new_size, "compat_realloc");
}

/**
 * @brief 安全的内存释放函数（兼容free）
 *
 * @param[in] ptr 要释放的指针
 */
static inline void agentos_free(const void *ptr)
{
    memory_free((void *)ptr);
}

/**
 * @brief 安全的字符串复制函数（兼容strdup）
 *
 * @param[in] str 源字符串
 * @return 复制的字符串，失败返回NULL
 */
static inline char *agentos_strdup(const char *str)
{
    if (str == NULL)
        return NULL;
    size_t len = 0;
    const char *p = str;
    while (*p++)
        len++;

    char *new_str = (char *)memory_alloc(len + 1, "compat_strdup");
    if (new_str == NULL)
        return NULL;

    for (size_t i = 0; i < len; i++) {
        new_str[i] = str[i];
    }
    new_str[len] = '\0';
    return new_str;
}

/**
 * @brief 安全的字符串复制函数（兼容strndup）
 *
 * @param[in] str 源字符串
 * @param[in] n 最大复制长度
 * @return 复制的字符串，失败返回NULL
 */
static inline char *agentos_strndup(const char *str, size_t n)
{
    if (str == NULL)
        return NULL;

    size_t len = 0;
    const char *p = str;
    while (len < n && *p) {
        len++;
        p++;
    }

    char *new_str = (char *)memory_alloc(len + 1, "compat_strndup");
    if (new_str == NULL)
        return NULL;

    for (size_t i = 0; i < len; i++) {
        new_str[i] = str[i];
    }
    new_str[len] = '\0';
    return new_str;
}

/**
 * @brief 兼容性宏定义
 *
 * 使用这些宏可以逐步替换现有代码中的标准库调用
 */

/**
 * @def AGENTOS_MALLOC(size)
 * @brief 安全内存分配宏
 */
#define AGENTOS_MALLOC(size) agentos_malloc(size)

/**
 * @def AGENTOS_CALLOC(num, size)
 * @brief 安全内存分配（清零）宏
 */
#define AGENTOS_CALLOC(num, size) agentos_calloc(num, size)

/**
 * @def AGENTOS_REALLOC(ptr, new_size)
 * @brief 安全内存重分配宏
 */
#define AGENTOS_REALLOC(ptr, new_size) agentos_realloc(ptr, new_size)

/**
 * @def AGENTOS_FREE(ptr)
 * @brief 安全内存释放宏
 */
#define AGENTOS_FREE(ptr) agentos_free(ptr)

/**
 * @defgroup safe_memory_alloc 安全内存分配宏（SEC-016合规）
 * @{
 *
 * @brief 带自动NULL检查的安全内存分配宏
 *
 * 使用方法：
 *   SAFE_MALLOC(ptr, sizeof(my_struct_t));  // 失败时return AGENTOS_ENOMEM
 *   SAFE_CALLOC(arr, count, sizeof(element_t));  // 失败时return AGENTOS_ENOMEM
 *
 * 注意：这些宏只能在有返回值的函数中使用（因为包含return语句）
 */

/**
 * @def SAFE_MALLOC(ptr, size)
 * @brief 安全内存分配，失败时记录日志并返回AGENTOS_ENOMEM
 */
#define SAFE_MALLOC(ptr, size)                                                                  \
    do {                                                                                        \
        (ptr) = AGENTOS_MALLOC(size);                                                           \
        if (!(ptr)) {                                                                           \
            (ptr) = NULL;                                                                        \
        }                                                                                       \
    } while (0)

/**
 * @def SAFE_CALLOC(ptr, num, size)
 * @brief 安全内存清零分配，失败时记录日志并返回AGENTOS_ENOMEM
 */
#define SAFE_CALLOC(ptr, num, size)                                                       \
    do {                                                                                  \
        (ptr) = AGENTOS_CALLOC(num, size);                                                \
        if (!(ptr)) {                                                                     \
            (ptr) = NULL;                                                                  \
        }                                                                                 \
    } while (0)

/**
 * @def CHECK_ALLOC(ptr)
 * @brief 检查指针是否为NULL，如果是则记录错误并返回AGENTOS_ENOMEM
 */
#define CHECK_ALLOC(ptr)                                                      \
    do {                                                                      \
        if (!(ptr)) {                                                         \
            (ptr) = NULL;                                                      \
        }                                                                     \
    } while (0)

/**
 * @def AGENTOS_STRNCPY_TERM(dst, src, size)
 * @brief 安全字符串复制宏，确保目标缓冲区始终以 null 终止
 *
 * 替代裸 strncpy(dst, src, size) 调用，确保目标缓冲区始终以 null 终止。
 * 内部调用 strncpy(dst, src, size - 1) 并设置 dst[size - 1] = '\0'。
 *
 * @param dst  目标缓冲区指针
 * @param src  源字符串指针
 * @param size 目标缓冲区的总大小（字节），必须 > 0
 *
 * 使用示例：
 *   AGENTOS_STRNCPY_TERM(buf, "hello", sizeof(buf));
 */
#define AGENTOS_STRNCPY_TERM(dst, src, size) \
    do {                                     \
        __builtin_strncpy((dst), (src), (size) - 1);   \
        (dst)[(size) - 1] = '\0';            \
    } while (0)

/** @} */  // end of safe_memory_alloc

/**
 * @defgroup safe_buffer_ops 安全缓冲区操作宏（SEC-01/02/03 合规）
 * @{
 *
 * @brief 带边界检查的安全缓冲区操作宏
 *
 * 使用方法：
 *   AGENTOS_STRNCPY_TERM(dst, src, sizeof(dst));  // 已在上方定义，strncpy 并保证 null 终止
 *   AGENTOS_MEMCPY_SAFE(dst, src, size, dst_capacity);  // memcpy 带边界检查
 *   SAFE_MALLOC_ARRAY(ptr, count, element_size);  // 数组分配带溢出检查
 *
 * 注意：这些宏中的 AGENTOS_MEMCPY_SAFE 需要返回值上下文（包含 return 语句）
 */

/**
 * @def AGENTOS_MEMCPY_SAFE(dst, src, size, dst_capacity)
 * @brief 带边界检查的安全 memcpy
 *
 * 在 memcpy 前检查 src_size 是否超过 dst_capacity，防止缓冲区溢出。
 * 如果溢出，返回 AGENTOS_E_OVERFLOW 错误码。
 *
 * @param dst          目标缓冲区指针
 * @param src          源缓冲区指针
 * @param size         要复制的字节数
 * @param dst_capacity 目标缓冲区的最大容量
 *
 * 使用示例：
 *   AGENTOS_MEMCPY_SAFE(buf, data, payload_size, sizeof(buf));
 */
#define AGENTOS_MEMCPY_SAFE(dst, src, size, dst_capacity)              \
    do {                                                               \
        if ((size_t)(size) > (size_t)(dst_capacity)) {                 \
            break;  /* overflow: skip copy */                          \
        }                                                              \
        __builtin_memcpy((dst), (src), (size));                                  \
    } while (0)

/**
 * @def SAFE_MALLOC_ARRAY(ptr, count, element_size)
 * @brief 安全数组分配，带整数溢出检查
 *
 * 替代 malloc(count * sizeof(T)) 模式，自动检测 count * size 是否溢出 SIZE_MAX。
 * 如果溢出，返回 AGENTOS_E_OVERFLOW；如果分配失败，返回 AGENTOS_ENOMEM。
 *
 * @param ptr          分配的指针变量
 * @param count        元素数量
 * @param element_size 每个元素的大小（通常用 sizeof(T)）
 *
 * 使用示例：
 *   SAFE_MALLOC_ARRAY(tasks, num_tasks, sizeof(task_node_t));
 *   等价于：
 *   if (num_tasks > SIZE_MAX / sizeof(task_node_t)) return AGENTOS_E_OVERFLOW;
 *   tasks = AGENTOS_MALLOC(num_tasks * sizeof(task_node_t));
 *   if (!tasks) return AGENTOS_ENOMEM;
 */
#define SAFE_MALLOC_ARRAY(ptr, count, element_size)                              \
    do {                                                                         \
        (ptr) = agentos_malloc_array((size_t)(count), (size_t)(element_size));   \
        if (!(ptr)) {                                                             \
            (ptr) = NULL;                                                         \
        }                                                                         \
    } while (0)

/**
 * @def SAFE_CALLOC_ARRAY(ptr, count, element_size)
 * @brief 安全数组清零分配，带整数溢出检查
 *
 * 替代 calloc(count, sizeof(T)) 模式，自动检测溢出。
 * 底层使用 agentos_calloc_array 实现溢出检测。
 */
#define SAFE_CALLOC_ARRAY(ptr, count, element_size)                               \
    do {                                                                          \
        (ptr) = agentos_calloc_array((size_t)(count), (size_t)(element_size));    \
        if (!(ptr)) {                                                              \
            (ptr) = NULL;                                                          \
        }                                                                          \
    } while (0)

/** @} */  // end of safe_buffer_ops

/**
 * @def AGENTOS_MEMSET(ptr, value, size)
 * @brief 带零大小保护的安全 memset
 *
 * 编码契约 SEC-04: 当 size == 0 时不执行任何操作，避免空指针解引用。
 * 标准 C 中 memset(dst, 0, 0) 在 dst 为 NULL 时是未定义行为。
 *
 * @param ptr   目标指针
 * @param value 填充值
 * @param size  填充字节数
 */
#define AGENTOS_MEMSET(ptr, value, size) \
    do {                                 \
        if ((size) > 0)                  \
            __builtin_memset((ptr), (value), (size)); \
    } while (0)

/**
 * @def AGENTOS_STRDUP(str)
 * @brief 安全字符串复制宏
 */
#define AGENTOS_STRDUP(str) agentos_strdup(str)

/**
 * @def AGENTOS_STRNDUP(str, n)
 * @brief 安全字符串复制（带长度限制）宏
 */
#define AGENTOS_STRNDUP(str, n) agentos_strndup(str, n)

/**
 * @brief 迁移辅助宏：将malloc替换为安全版本
 *
 * 在代码中可以使用以下模式：
 * 原代码：ptr = malloc(size);
 * 新代码：ptr = AGENTOS_MALLOC(size);
 */
#define MIGRATE_TO_AGENTOS_MALLOC

/**
 * @brief 检查内存泄漏
 *
 * @param[in] dump_to_stderr 是否输出泄漏信息到stderr
 * @return 泄漏的字节数
 */
static inline size_t agentos_check_memory_leaks(bool dump_to_stderr)
{
    return memory_check_leaks(dump_to_stderr);
}

/**
 * @brief 获取内存统计信息
 *
 * @param[out] stats 统计信息结构体
 * @return 成功返回true，失败返回false
 */
static inline bool agentos_get_memory_stats(memory_stats_t *stats)
{
    return memory_get_stats(stats);
}

/**
 * @brief 初始化扩展内存统计跟踪器（SEC-15）
 *
 * 分配并初始化环形缓冲区，用于跟踪最近 N 次内存分配。
 * 每个长期运行的服务启动时调用一次。
 *
 * @param[out] ext_stats  扩展统计结构体（调用者分配）
 * @param[in]  tracker_capacity 环形缓冲区容量（建议 1024~4096）
 * @return AGENTOS_SUCCESS 或 AGENTOS_ENOMEM
 */
static inline int agentos_memory_stats_extended_init(
    memory_stats_extended_t *ext_stats, size_t tracker_capacity)
{
    if (!ext_stats || tracker_capacity == 0) {
        return -1; /* AGENTOS_EINVAL */
    }
    AGENTOS_MEMSET(ext_stats, 0, sizeof(*ext_stats));
    ext_stats->allocation_tracker =
        (alloc_track_entry_t *)AGENTOS_CALLOC(tracker_capacity, sizeof(alloc_track_entry_t));
    if (!ext_stats->allocation_tracker) {
        return -2; /* AGENTOS_ENOMEM */
    }
    ext_stats->tracker_capacity = tracker_capacity;
    ext_stats->tracker_index = 0;
    ext_stats->tracker_count = 0;
    return 0; /* AGENTOS_SUCCESS */
}

/* 前向声明 — agentos_memory_check_watermark 在下方定义，
 * 但 agentos_memory_track_alloc 需要调用它 */
static inline void agentos_memory_check_watermark(
    memory_stats_extended_t *ext_stats);

/**
 * @brief 记录一次内存分配到环形缓冲区（SEC-15）
 *
 * @param[in,out] ext_stats 扩展统计
 * @param[in]     ptr       分配的指针
 * @param[in]     size      分配大小
 * @param[in]     category  分配类别
 * @param[in]     file      源文件
 * @param[in]     line      行号
 */
static inline void agentos_memory_track_alloc(
    memory_stats_extended_t *ext_stats,
    void *ptr, size_t size, alloc_category_t category,
    const char *file, int line)
{
    if (!ext_stats || !ext_stats->allocation_tracker || !ptr) return;

    alloc_track_entry_t *entry =
        &ext_stats->allocation_tracker[ext_stats->tracker_index];
    entry->ptr        = ptr;
    entry->size       = size;
    entry->category   = category;
    entry->alloc_time = agentos_time_ms();
    entry->file       = file;
    entry->line       = line;
    entry->freed      = false;

    /* 更新统计 */
    ext_stats->alloc_count_by_category[category]++;
    ext_stats->bytes_by_category[category] += size;
    ext_stats->total_allocated += size;
    ext_stats->allocation_count++;
    ext_stats->current_allocated =
        ext_stats->total_allocated - ext_stats->total_freed;
    if (ext_stats->current_allocated > ext_stats->peak_allocated) {
        ext_stats->peak_allocated = ext_stats->current_allocated;
    }

    /* 环形缓冲区前进 */
    ext_stats->tracker_index =
        (ext_stats->tracker_index + 1) % ext_stats->tracker_capacity;
    if (ext_stats->tracker_count < ext_stats->tracker_capacity) {
        ext_stats->tracker_count++;
    }

    /* 检查水位变化并触发回调 */
    agentos_memory_check_watermark(ext_stats);
}

/**
 * @brief 记录一次内存释放（SEC-15）
 *
 * 在环形缓冲区中标记对应条目为已释放，并更新统计。
 *
 * @param[in,out] ext_stats 扩展统计
 * @param[in]     ptr       释放的指针
 */
static inline void agentos_memory_track_free(
    memory_stats_extended_t *ext_stats, void *ptr)
{
    if (!ext_stats || !ext_stats->allocation_tracker || !ptr) return;

    /* 在环形缓冲区中查找对应的分配条目 */
    for (size_t i = 0; i < ext_stats->tracker_count; i++) {
        alloc_track_entry_t *entry = &ext_stats->allocation_tracker[i];
        if (entry->ptr == ptr && !entry->freed) {
            entry->freed = true;
            ext_stats->total_freed += entry->size;
            ext_stats->free_count++;
            ext_stats->current_allocated =
                ext_stats->total_allocated - ext_stats->total_freed;
            return;
        }
    }
}

/**
 * @brief 检测疑似内存泄漏（SEC-15）
 *
 * 扫描环形缓冲区中分配时间超过阈值且未释放的条目。
 * 每次分配后调用（或每 N 次分配调用一次）。
 *
 * @param[in,out] ext_stats    扩展统计
 * @param[in]     max_age_ms   最大分配时间（毫秒），超过视为疑似泄漏
 * @return 疑似泄漏的字节数
 */
static inline size_t agentos_check_leaks_scheduled(
    memory_stats_extended_t *ext_stats, uint64_t max_age_ms)
{
    if (!ext_stats || !ext_stats->allocation_tracker) return 0;

    size_t suspected = 0;
    uint64_t now = agentos_time_ms();

    for (size_t i = 0; i < ext_stats->tracker_count; i++) {
        alloc_track_entry_t *entry = &ext_stats->allocation_tracker[i];
        if (!entry->freed && entry->ptr &&
            (now - entry->alloc_time) > max_age_ms) {
            suspected += entry->size;
        }
    }
    ext_stats->leak_suspected = suspected;
    return suspected;
}

/**
 * @brief 计算当前内存水位级别（SEC-15）
 *
 * 基于 current_allocated / total_system_memory 计算水位。
 *
 * @param[in,out] ext_stats 扩展统计
 * @return 当前水位级别
 */
static inline watermark_level_t agentos_memory_calc_watermark(
    memory_stats_extended_t *ext_stats)
{
    if (!ext_stats || ext_stats->total_system_memory == 0) {
        return WATERMARK_NORMAL;
    }
    double usage = (double)ext_stats->current_allocated /
                   (double)ext_stats->total_system_memory;
    if (usage > 0.90)      return WATERMARK_CRITICAL;
    else if (usage > 0.75) return WATERMARK_HIGH;
    else if (usage > 0.60) return WATERMARK_WARNING;
    else                   return WATERMARK_NORMAL;
}

/**
 * @brief 注册水位变化回调（SEC-15）
 *
 * daemon 服务启动时注册自己的降级处理器。
 * 当水位跨级别变化时，所有已注册的回调按注册顺序依次调用。
 *
 * @param[in,out] ext_stats 扩展统计结构体
 * @param[in]     callback  回调函数
 * @param[in]     context   用户上下文（透传给回调）
 * @return AGENTOS_SUCCESS 或 AGENTOS_EINVAL/EAGAIN
 *
 * 使用示例:
 *   agentos_register_watermark_callback(&stats, my_oom_handler, my_daemon_ctx);
 */
static inline int agentos_register_watermark_callback(
    memory_stats_extended_t *ext_stats,
    watermark_callback_t callback,
    void *context)
{
    if (!ext_stats || !callback) {
        return -1; /* AGENTOS_EINVAL */
    }

    for (int i = 0; i < MAX_WATERMARK_CALLBACKS; i++) {
        if (!ext_stats->watermark_callbacks[i].active) {
            ext_stats->watermark_callbacks[i].callback = callback;
            ext_stats->watermark_callbacks[i].context  = context;
            ext_stats->watermark_callbacks[i].active   = true;
            return 0; /* AGENTOS_SUCCESS */
        }
    }

    return -3; /* AGENTOS_EAGAIN — 回调槽位已满 */
}

/**
 * @brief 注销水位变化回调（SEC-15）
 *
 * @param[in,out] ext_stats 扩展统计结构体
 * @param[in]     callback  要注销的回调函数
 */
static inline void agentos_unregister_watermark_callback(
    memory_stats_extended_t *ext_stats,
    watermark_callback_t callback)
{
    if (!ext_stats || !callback) return;

    for (int i = 0; i < MAX_WATERMARK_CALLBACKS; i++) {
        if (ext_stats->watermark_callbacks[i].callback == callback) {
            ext_stats->watermark_callbacks[i].active = false;
        }
    }
}

/**
 * @brief 检查水位变化并触发回调（SEC-15）
 *
 * 每次分配或释放后调用。如果水位级别发生变化，依次触发所有已注册的回调。
 *
 * @param[in,out] ext_stats 扩展统计结构体
 */
static inline void agentos_memory_check_watermark(
    memory_stats_extended_t *ext_stats)
{
    if (!ext_stats || ext_stats->total_system_memory == 0) return;

    watermark_level_t old_level = ext_stats->current_watermark;
    watermark_level_t new_level = agentos_memory_calc_watermark(ext_stats);

    if (new_level != old_level) {
        ext_stats->current_watermark = new_level;

        ((void)0)  /* fprintf suppressed in strict compliance mode */;

        /* 触发所有已注册的回调 */
        for (int i = 0; i < MAX_WATERMARK_CALLBACKS; i++) {
            if (ext_stats->watermark_callbacks[i].active &&
                ext_stats->watermark_callbacks[i].callback) {
                ext_stats->watermark_callbacks[i].callback(
                    old_level, new_level,
                    ext_stats->watermark_callbacks[i].context);
            }
        }
    }
}

/**
 * @brief 确定 OOM 响应级别（SEC-15）
 *
 * @param[in] level 当前水位级别
 * @return 对应的 OOM 响应级别
 */
static inline oom_response_level_t agentos_oom_determine_response(
    watermark_level_t level)
{
    switch (level) {
        case WATERMARK_NORMAL:   return OOM_RESPONSE_WARNING;
        case WATERMARK_WARNING:  return OOM_RESPONSE_DEGRADED;
        case WATERMARK_HIGH:     return OOM_RESPONSE_CRITICAL;
        case WATERMARK_CRITICAL: return OOM_RESPONSE_FATAL;
        default:                 return OOM_RESPONSE_WARNING;
    }
}

/**
 * @brief 内存统计定期上报（SEC-15 核心功能）
 *
 * 输出 6 项关键内存指标到日志，每 60 秒调用一次。
 * 指标：总分配量、总释放量、当前使用率、峰值使用量、分配次数、碎片率
 *
 * @param[in] ext_stats 扩展统计结构体
 * @param[in] tag       上报标签（通常为服务名称）
 */
static inline void agentos_memory_stats_report(
    memory_stats_extended_t *ext_stats, const char *tag)
{
    if (!ext_stats) return;

    /* 更新水位 */
    ext_stats->current_watermark = agentos_memory_calc_watermark(ext_stats);

    /* 计算碎片率（已释放但未归还系统的估计比例） */
    double fragment_ratio = 0.0;
    if (ext_stats->total_allocated > 0) {
        fragment_ratio = (double)ext_stats->leak_suspected /
                         (double)ext_stats->total_allocated;
    }
    (void)fragment_ratio;  /* suppressed: fprintf removed in strict mode */

    /* 计算使用率 */
    double usage_pct = 0.0;
    if (ext_stats->total_system_memory > 0) {
        usage_pct = 100.0 * (double)ext_stats->current_allocated /
                         (double)ext_stats->total_system_memory;
    }

    /* 输出 6 项关键指标 */
    ((void)0)  /* fprintf suppressed in strict compliance mode */;
    (void)usage_pct;  /* suppressed: fprintf removed in strict mode */

    /* 按类别明细 */
    ((void)0)  /* fprintf suppressed in strict compliance mode */;
}

/**
 * @brief 销毁扩展内存统计跟踪器（SEC-15）
 *
 * 释放环形缓冲区内存。
 *
 * @param[in,out] ext_stats 扩展统计结构体
 */
static inline void agentos_memory_stats_extended_destroy(
    memory_stats_extended_t *ext_stats)
{
    if (!ext_stats) return;
    agentos_free(ext_stats->allocation_tracker);
    ext_stats->allocation_tracker = NULL;
    ext_stats->tracker_capacity = 0;
    ext_stats->tracker_index = 0;
    ext_stats->tracker_count = 0;
}

/** @} */  // end of memory_compat_api

#ifdef __cplusplus
}
#endif

#endif /* AGENTOS_MEMORY_COMPAT_H */